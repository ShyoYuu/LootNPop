# TechDesign — 인벤토리 아이템 인스턴스 모델

> 2026-07-17 구현 완료(호스트 PIE 검증). Lyra식 하이브리드: **UObject 아이템 인스턴스 + FastArray 델타 복제 + 등록 서브오브젝트**. 스탯은 GameplayTagStack.

## 1. 왜 인스턴스 모델인가

구 모델은 가방이 `TArray<TObjectPtr<ULNPItemDefinitionBase>>` — 공유 DataAsset 포인터라:
- 인스턴스별 상태(무기 레벨·랜덤 스탯·버프 잔여시간)를 담을 수 없고,
- 장착본/보관본을 구분 못 해 `IsEquipped`(정의 포인터 비교)가 오검출했다(기본 무기 DA_Pistol을 픽업 후 드랍 시 무반응이던 버그).

인스턴스 모델은 각 사본에 **`FGuid ItemId` 정체성**을 부여해 이를 근본 해소하고, 확장 스탯의 그릇을 마련한다.

## 2. 타입 구성

| 타입 | 파일 | 역할 |
|:---|:---|:---|
| `FLNPGameplayTagStack` / `FLNPGameplayTagStackContainer` | `Item/LNPGameplayTagStack.*` | Lyra 포팅. `Tag→int32` 스택의 FastArray(델타 복제 + 조회 캐시 TMap). 스탯 저장용 |
| `ULNPInventoryItemInstance` | `Item/LNPInventoryItemInstance.*` | 아이템 런타임 인스턴스(UObject). 복제: `Definition`·`ItemId`·`StatTags`·`bEquipped`(ReplicatedUsing)·`RemainingDuration`. `IsSupportedForNetworking()=true` |
| `FLNPInventoryEntry` / `FLNPInventoryList` | `Item/LNPInventoryList.*` | 인스턴스 참조 FastArray. 복제 콜백에서 소유 컴포넌트 `OnInventoryChanged` 브로드캐스트 |
| `ULNPInventoryComponent` | `Item/LNPInventoryComponent.*` | PlayerState 소유. `BagList`(가방)·`ActiveBuffList`(활성 버프) 두 리스트 + 서버 전용 버프 런타임 |

스탯 태그: `LNPGameplayTags.h`의 `TAG_Item_Level`(`LNP.Item.Level`) — 확장 시 `LNP.Item.*`/`LNP.Stat.*` 패밀리에 추가.

## 3. 복제 방식 (Iris 검증 완료)

- 컴포넌트 생성자: `bReplicateUsingRegisteredSubObjectList = true`. 인스턴스 추가/제거 시 `AddReplicatedSubObject(Instance, COND_OwnerOnly)` / `RemoveReplicatedSubObject`.
- 두 FastArray 리스트 모두 `DOREPLIFETIME_CONDITION(..., COND_OwnerOnly)` — 가방은 소유 클라이언트에만.
- **2인 PIE 실측(2026-07-13):** 서버가 원격 클라 인벤토리에 인스턴스 추가 → 그 클라가 `FLNPInventoryList::PostReplicatedAdd` 수신. **Iris × FastArray × 등록 서브오브젝트 owner-only 복제 성립.** 검증 커맨드: `LNP.Debug.AddBagInstance <ItemDefPath> [PlayerIndex]`(권위 콘솔, 인수 없으면 컨트롤러 인덱스 나열; 호스트 대상은 원본이라 PostReplicatedAdd 안 뜸 — 원격 클라 대상이어야 검증됨).
- FastArray 컨테이너의 `OwnerComponent` 역참조는 비복제 — **양측 생성자에서 `SetOwnerComponent(this)`** 로 설정한다.

### 3.1 ⚠️ Iris는 `PostReplicatedRemove`를 부르지 않는다 (2026-08-20)

**증상:** 게스트가 아이템을 드랍하면 인벤토리 UI에 그 항목이 그대로 남는다(조작은 안 됨). 메뉴를 닫았다
열면 정상. **호스트는 멀쩡하다** — 서버는 `RemoveItemInstance`에서 직접 `OnInventoryChanged`를 쏘기 때문이다.

Iris의 FastArray 디스패치는 이름이 비대칭이다 (`FastArrayReplicationFragmentInternal.h`):

| 이벤트 | Iris가 부르는 콜백 |
|:--|:--|
| 추가 | `PostReplicatedAdd` (333행) |
| 변경 | `PostReplicatedChange` (340행) |
| **제거** | **`PreReplicatedRemove` (320행)** — `PostReplicatedRemove`는 **영영 안 불린다** |

`FLNPInventoryList`가 통지를 `PostReplicatedRemove`에만 걸어둬서 제거만 통지가 끊겼다.
`ActiveBuffList`도 같은 타입이라 **게스트에서 버프 만료 시 UI가 안 지워지는 문제**가 함께 있었다.

**두 번째 함정 — 호출 시점.** `PreReplicatedRemove`는 **항목이 아직 배열에 남아 있는 상태**에서 불린다
(실제 제거는 348행 이후). 엔진이 콜백 도중 배열 크기가 바뀌면 에러 로그를 찍을 만큼 이 순서에 엄격하다.
그래서 여기서 바로 브로드캐스트하면 UI가 방금 사라진 항목을 그대로 다시 그려 **증상이 그대로 남는다**.
→ `SetTimerForNextTick`으로 **다음 틱에 통지**한다. 여러 항목이 한 프레임에 빠져 통지가 겹쳐도
목록 재구성은 멱등이라 무해하다.

`PostReplicatedRemove`는 Iris를 끈 클래식 경로용으로 남겨 뒀다(그쪽은 배열이 이미 줄어든 뒤라 즉시 통지).

## 4. 장착/보관 분리 (Option 2)

- 장비 슬롯(`FLNPWeaponInstance`/`FLNPSkillInstance`, `Item/LNPItemInstance.h`)에 `SourceInstance` 참조 추가 — 어느 가방 인스턴스가 장착됐는지.
- 장착(`ULNPEquipmentComponent::EquipWeaponInstance`, **서버 전용**): 슬롯이 인스턴스 참조 + `Instance->SetEquipped(true)` + GAS grant. 해제 시 false + revoke.
- **인스턴스는 BagList에 물리적으로 남는다** — UI가 `!IsEquipped()`만 노출해 분리를 성립시킨다(서브오브젝트 재등록 불필요).
- `bEquipped`는 `ReplicatedUsing=OnRep_InstanceChanged` — 값 변경만으로는 FastArray 콜백이 안 울리므로, 서버는 `SetEquipped`에서·클라는 OnRep에서 `OnInventoryChanged`를 직접 브로드캐스트해 **장착 즉시 UI 재필터**된다. (이 통지가 없으면 "다음 인벤토리 변화 때에야 뒤늦게 사라지는" 버그가 된다 — 실측 후 수정.)
- `IsEquippedInstance(ItemId)`가 사본 정확 판정. 정의 기준 `IsEquipped`는 레거시로 잔존. (2026-08-20 기준 둘 다 호출처 없음 — UI는 인스턴스의 `bEquipped`를 직접 읽는다.)
- **기본 무기:** `DefaultWeapon`도 **가방 인스턴스를 만들어 장착**한다(`ULNPEquipmentComponent::EnsureDefaultWeapon`) — innate 특수 케이스를 없애 루팅 무기와 같은 상태 기계를 타게 했다(2026-08-20). 이미 다른 무기를 들고 있으면 손대지 않으므로 리스폰 시 되돌아가지 않고, 조회를 먼저 하므로 사본이 쌓이지도 않는다. 인벤토리 컴포넌트가 없는 소유자에 한해 정의만으로 장착하는 폴백이 남아 있다(`SourceInstance` null).

> ⚠️ **호출 시점이 중요하다 — `EqComp::BeginPlay`에서 부르면 안 된다** (2026-08-20 2P 실측).
> 게스트의 PlayerState는 BeginPlay 시점에 아직 자기 연결로 복제를 시작하기 전이다. 이때 등록한
> 복제 서브오브젝트를 참조하는 FastArray 엔트리는 **미해결 포인터인 채로 초기 번치에 실린다** —
> 엔트리 자체는 도착해 `PostReplicatedAdd`가 `bag size=1`로 울리지만 `Entry.Instance`가 null이고,
> FastArray는 그 엔트리를 "이미 보냄"으로 간주해 재전송하지 않아 **참조가 영원히 해소되지 않는다.**
> `GetBagInstances()`가 null을 걸러내므로 게스트 가방은 빈 채로 남는다(호스트는 로컬이라 정상).
> 그래서 `ALNPPlayerCharacter::PossessedBy`에서 호출한다 — 폰 스폰 게이트를 통과한 뒤라
> 연결이 완전히 성립해 있고, 런타임 루팅과 같은 검증된 복제 경로를 탄다.
> **교훈: 복제 서브오브젝트를 참조하는 컨테이너는 액터가 실제로 복제를 시작한 뒤에 채운다.**

### 4.1 장비 상태의 소유권 (2026-08-20 정리)

`ULNPEquipmentComponent::WeaponSlot`이 **복제되는 단일 원본**이다. 이전에는 Pawn의
`ALNPCharacterBase::EquippedWeaponData`만 복제되고 개념적 원본인 `WeaponSlot`은 복제되지 않아,
시뮬레이티드 프록시에서 두 값이 갈라졌다 (프록시가 `BeginPlay`에서 스스로 장착한 `DefaultWeapon`이
영구히 남아, 롱소드를 휘두르는 캐릭터를 권총 데이터로 판정). 자세한 배경은
[TechDesign_Networking.md](TechDesign_Networking.md)의 장비 상태 복제 항목 참조.

| 대상 | 역할 | 복제 |
|:--|:--|:--|
| `ULNPEquipmentComponent::WeaponSlot` | 단일 원본, **서버만 쓴다** | ✅ `ReplicatedUsing=OnRep_WeaponSlot` (조건 없음 — 프록시 포함) |
| `FLNPWeaponInstance::Definition` | 무엇을 들고 있는가 | ✅ 구조체에서 유일하게 복제되는 필드 |
| `FLNPWeaponInstance`의 나머지 3필드 | 서버 전용 (핸들·가방 인스턴스) | ❌ `NotReplicated` |
| `ALNPCharacterBase::CachedWeaponDef` | 비주얼 파생 캐시 | ❌ |
| `ALNPEnemyCharacter::EnemyConfig` | 적 무기의 원본 (EqComp 없음) | ✅ `ReplicatedUsing=OnRep_EnemyConfig` |

- **쓰기 경로는 하나다.** 클라이언트는 `ALNPPlayerCharacter::RequestEquipWeapon(Instance)`로
  `Server_Equip*` RPC만 보내고, 로컬 선반영(예측)은 하지 않는다. 컴포넌트의 뮤테이터는
  `ensureMsgf`로 권위를 강제한다. 무기 교체는 메뉴·디버그 키에서만 발생하므로 RTT 지연이 문제되지 않는다.
- **비주얼 적용 함수도 하나다** — `ALNPCharacterBase::ApplyWeaponVisuals()`. 서버는 슬롯 적용 직후,
  클라이언트는 `OnRep_WeaponSlot`에서 같은 함수를 부른다. 멱등이라 중복 호출이 안전하다.
- **도착 순서**: 슬롯은 PlayerState의 컴포넌트에, 비주얼은 Pawn에 있고 복제 순서는 보장되지 않는다.
  푸시(`PushWeaponToPawn`, Pawn 없으면 no-op)와 풀(Pawn의 `BeginPlay`/`OnRep_PlayerState`/`PossessedBy`에서
  `ResolveWeaponDefForVisuals()`)을 양방향으로 걸어 어느 쪽이 먼저 와도 수렴시킨다.
  ⚠️ 풀은 반드시 `InitAbilitySystem()` **뒤에** — 그쪽이 `CurrentWeaponTag`를 Unarmed로 되돌린다.
- **UI 갱신**: `OnEquipmentChanged` 델리게이트(서버 적용·클라 OnRep 양쪽에서 브로드캐스트). 인벤토리의
  `OnInventoryChanged`만으로는 부족하다 — `DefaultWeapon`처럼 가방 인스턴스가 없는 장착은 `bEquipped`
  복제가 아예 없고, 있더라도 두 컴포넌트의 OnRep 순서가 보장되지 않는다. 스탯 탭은 두 신호를 모두 구독한다.
- **범위 밖**: `ActiveSkillSlots`/`PassiveSkillInstances`도 같은 구멍이 있으나 액티브 스킬이 미구현이라
  스킬 구현 시점에 같은 패턴으로 처리한다.

## 5. 버프 흐름

- `AddBuffItem(BuffData, RemainingDuration)`: 인스턴스 생성 → GE 적용 → **GAS 핸들은 서버 전용 사이드테이블** `TMap<FGuid, FLNPBuffRuntime>`(핸들+만료 타이머, 복제 인스턴스에 핸들 안 실음) → `ActiveBuffList` 편입.
  적용되는 GE는 아이템 정의의 `EffectsToApply`(특수 효과)와 `StatModifiers`(선언형 스탯 변경, `LNPStat::ApplyModifiers`) 두 갈래다.
- **지속 시간 규약**: `Duration > 0` = 기간제, **`-1` = 영구**(만료 없음, UI 시간 표시 없음), `0` = 잘못된 설정(경고 로그 후 영구 취급).
  `AddBuffItem`의 인수 0은 "페이로드 없음"을 뜻해 아이템 정의값으로 폴백하므로, **영구는 반드시 -1로 왕복해야 한다**.
- **만료(서버 권위)**: `AddBuffItem`에서 만료 시각을 한 번 확정하고 `FTimerManager`에 맡긴다 — `SetTimer(FLNPBuffRuntime::ExpireTimer, ...)`가 `ExpireBuffInstance`(GE 해제 + 리스트/서브오브젝트/런타임 제거)를 발화시킨다. 영구 버프(-1)는 타이머를 걸지 않으며 **핸들 무효 자체가 "영구" 판별**이라 별도 센티널이 없다. `ExpireWorldTime`은 양도 시 남은 초를 역산하는 용도로만 쓴다.
  - **2026-08-17 전환** — 이전엔 `TickComponent`가 매 프레임 `RemainingDuration -= DeltaTime`으로 감산했다. 데드라인 방식으로 바꿔 ① 버프별 float 누적 오차 제거(전역 시계 1개만 누적), ② 버프 N개 순회 → 엔진 힙 top 비교, ③ `bCanEverTick` 제거(서버·클라 양쪽에서 매 프레임 호출 소멸). 복제 대역폭은 전환 전에도 스냅샷 1회뿐이어서 변화 없다.
  - `ExpireBuffInstance(FGuid)`는 **값 전달**이어야 한다 — `CreateUObject`이 페이로드를 `std::decay_t`한 멤버 함수 포인터 타입을 요구하므로 `const FGuid&`로는 바인딩이 컴파일되지 않는다. 진입 시 `ClearTimer`로 조기 제거(양도) 경로의 타이머 잔류를 막는다.
- 인스턴스의 `RemainingDuration`은 **복제 스냅샷**(추가·양도 시점 갱신). 권위 만료 판정은 서버의 `FLNPBuffRuntime` 타이머가 하고, **UI 라이브 표시는 각 머신이 로컬로 센다** — 인스턴스의 비복제 `DurationStartTime`에 스냅샷이 유효해진 로컬 월드 시각을 찍고(서버: `SetRemainingDuration`, 소유 클라: `FLNPInventoryList::PostReplicatedAdd`), `GetRemainingDurationLive()`가 `스냅샷 - 경과`를 반환한다. 시계 동기화 불필요(오차 ≈ 편도 지연). 기준 시각이 인스턴스에 남으므로 **인벤토리를 닫았다 열어도 이어서 센다**. 표시는 인게임 메뉴가 담당한다 — `ULNPBuffChipWidget`(스탯 탭 버프 칩)과 `ULNPMenuItemCellWidget`(인벤토리 셀 배지)이 각각 1초 반복 타이머로 잔여 초를 갱신(`CeilToInt`)하고, 항목 재바인딩·`NativeDestruct`에서 타이머를 정리한다. 2026-07-17 PIE 검증 완료(당시 위젯은 `ULNPInventoryEntryWidget`, 2026-08-07 인게임 메뉴로 이관되며 삭제).
- 드랍/양도: `RemoveBuffInstance(ItemId)`가 잔여 초를 반환 → LootDice 페이로드 → 재획득 시 `AddBuffItem(..., 잔여)`로 복원 (라운드트립 성립).
  페이로드는 **절대 만료 시각이 아니라 상대 잔여 초**다 — 그래서 월드에 놓인 Dice는 버프 지속시간이 **동결**되고 60초 수명 제한만 흐른다(기획 의도, [GameDesign_LootDice.md](GameDesign_LootDice.md) §양도). 데드라인을 그대로 실으면 이 동결이 깨지므로 `ExpireWorldTime - Now`로 되돌려 싣고, 픽업 시 `Now + 잔여`로 재계산한다.

## 6. 획득·드랍·장착 경로

- **픽업**(`ULNPInteractionComponent::PickupDiceOnServer`): 버프는 `AddBuffItem`, 그 외는 `AddItemInstance`.
- **빈 슬롯 자동 장착**: `AddItemInstance`가 가방 편입 직후 `ULNPEquipmentComponent::TryAutoEquipWeapon`을 호출한다 — 무기 슬롯이 비어 있고 획득물이 무기일 때만 장착하며, 이미 장착 중이면 손대지 않는다(획득이 현재 장비를 갈아치우면 안 된다). 기획상 맨손 상태는 존재하지 않아야 하므로(`EqComp::BeginPlay`가 `DefaultWeapon` 장착) **안전망 성격**이다. 훅을 픽업 경로가 아니라 `AddItemInstance`에 둔 이유는 획득 경로가 여럿(LootDice 픽업, `LNP.Debug.AddBagInstance`, 디버그 키 지급)이고 전부 이 관문을 지나기 때문이다. 판단 자체는 장비 상태를 소유한 컴포넌트가 한다.
- **드랍**(`ALNPPlayerCharacter::DropItem(FGuid)` → `Server_DropItem(FGuid)`): ItemId로 가방→버프 순 조회, `IsEquipped()` 가드, 제거 성공 후에만 Dice 스폰(복제 방지). 무기 레벨은 제거 **전에** 읽어 페이로드에 싣는다 (§7.4).
- **합성**(`RequestMergeItem(FGuid)` → `Server_MergeItem` → `TryMergeItem`): 재료 n-1개 소모 + 대상 레벨 +1. → §7.2
- **장착**(`RequestEquipWeaponInstance(Instance)` → `Server_EquipWeaponInstance(FGuid)`): **서버 권위 전용**(bEquipped·GAS·비주얼). 클라 예측 없음, 결과는 `WeaponSlot` 복제로 되돌아온다. RPC는 FGuid만 전달하고 서버가 소유 인벤토리에서 조회·검증한다. §4.1 참조.
- **정의 기반 장착**(`RequestEquipWeapon(Def)` → `Server_EquipWeapon` → `EquipWeaponOnServer`): 디버그 키(`EquipTestWeapon`) 전용 경로. 서버가 ① 가방에서 같은 정의의 인스턴스를 찾고 ② 없으면 **`TestWeaponList`에 있을 때만** 지급한 뒤 ③ `EquipWeaponInstance`로 넘긴다. 즉 **모든 장착이 결국 인스턴스를 거치므로** bEquipped·인벤토리 UI·드랍 가드가 경로에 무관하게 동일하게 동작한다. ②의 허용 목록 검증이 없으면 클라이언트가 임의의 `ULNPWeaponData` 에셋을 지목해 장착할 수 있다. `Def == nullptr`은 맨손 전환 요청이다.

## 7. 무기 레벨·합성 (2026-08-20)

### 7.1 레벨은 어디에 있나

인스턴스 `StatTags`의 `TAG_Item_Level` 스택 카운트가 레벨이다 — 이미 복제되는 그릇이라 배관 추가가 없다.
읽기·쓰기는 얇은 래퍼 `ULNPInventoryItemInstance::GetItemLevel()` / `SetItemLevel(int32)`로 통일한다
(레벨 없는 아이템도 1로 읽힌다 — 0레벨은 의미가 없다).

**레벨별 값의 원본은 무기 DA가 아니라 `ULNPWeaponData::LevelTable`이다** (행 구조 `FLNPWeaponLevelRow`,
**행 이름 = 레벨 숫자**). 공식 배율이 아니라 레벨마다 절대값을 손으로 넣는 구조이며,
**마지막 연속 행이 곧 최대 레벨**이다. 상세는 [TechDesign_Ability.md](TechDesign_Ability.md) §2.3.

> `UDataTable::GetRowMap()`은 TMap이라 저작 순서가 보존되지 않는다 — 인덱스가 아니라 이름으로 조회해야 한다.

### 7.2 합성 — 대상 자신이 결과물이 된다

`ULNPInventoryComponent::CanMergeItem`(판정, 클라도 호출 가능) / `TryMergeItem`(서버 실행).

재료는 **대상을 제외한**, 정의·레벨이 같은 **비장착** 가방 인스턴스이고, 재료 n-1개를 소모한 뒤
**대상의 레벨을 +1**한다. 이 한 경로가 기획의 두 규칙을 동시에 만족시킨다:

| 대상 | 소모 | 결과 |
|:--|:--|:--|
| 비장착 무기 | 재료 n-1개 (+대상 자신) | n개 투입 → 1개 산출 |
| 장착 중인 무기 | 재료 n-1개 | 장착 무기 +1레벨 |

- 대상이 결과물이라 **ItemId가 보존**된다 → 합성 후에도 UI 선택이 유지된다.
- 장착본은 재료 후보에서 걸러지므로 "장착 중인 무기를 재료로 못 쓴다"가 자연히 성립한다.
- 대상이 장착본이면 `ULNPEquipmentComponent::RefreshWeaponSlotGrants()`로 GAS를 새 레벨로 재부여한다.
  `ClearWeaponSlot`→`Equip` 경로를 쓰면 안 된다 — `bEquipped`가 내려갔다 올라가 UI가 깜빡이고,
  `EquipWeaponInstance`의 "같은 인스턴스면 조기 반환"에 걸려 **아무 일도 일어나지 않는다**.
- RPC: `RequestMergeItem(FGuid)` → `Server_MergeItem` → `MergeItemOnServer`. 클라가 보낸 판정은
  서버가 `TryMergeItem`에서 처음부터 다시 한다.

### 7.3 클라이언트 통지 — `ChangeCounter`가 필요한 이유

레벨은 **인스턴스 자신의** `StatTags`에 있어 소유 컴포넌트의 `BagList` FastArray 콜백을 울리지 않고,
`StatTags` 쪽 FastArray는 소유 컴포넌트를 모른다. 그 결과 원격 클라에서 "재료가 사라지는 통지"와
"레벨이 오르는 복제"가 서로 다른 경로로 도착해 **순서가 보장되지 않는다**.
그래서 `SetItemLevel`이 `ChangeCounter`(ReplicatedUsing=`OnRep_InstanceChanged`)를 함께 올려,
`bEquipped`와 같은 통지 경로를 타게 했다.

같은 함정의 UI 판이 둘 더 있다 — 둘 다 "목록은 그대로인데 항목 내부만 바뀐" 경우다:
- 셀 배지: `ItemGrid->RegenerateAllEntries()` (기존)
- 디테일 패널: `RefreshGrid()`가 선택 델리게이트에 기대지 않고 **항상** `DetailPanel->SetItem()`을 부른다.
  `SetSelectedItem`은 선택 대상이 그대로면 `OnItemSelectionChanged`를 다시 쏘지 않는다.

### 7.4 드랍→재획득 왕복

LootDice 페이로드에 `ItemLevel`(COND_InitialOnly)이 실린다 →
[TechDesign_LootDice.md](TechDesign_LootDice.md) §2.2. **다른 플레이어가 주워도 레벨이 보존된다.**

디버그: `LNP.Debug.AddBagInstance <ItemDefPath> [PlayerIndex] [Level]` — 3번째 인수가 레벨.
합성 테스트는 같은 레벨 사본을 n개 만들어야 하므로 이 인수가 필요하다.

---

## 8. 잔여·후속

- **랜덤 스탯 롤링 미구현**: 무기 레벨은 §7로 해소됐으나 인스턴스별 **랜덤** 스탯은 아직 없다.
  `StatTags` 그릇은 그대로 쓸 수 있고, 붙일 때 LootDice 페이로드에 스탯 스냅샷 왕복도 함께 넣어야 한다
  (레벨은 이미 왕복하므로 같은 자리에 얹으면 된다).
- ~~버프 콘텐츠 없음~~ ✅ 해소(2026-07-17): 첫 버프 콘텐츠 제작 — `DA_Buff_LootSpeed`(`/Game/ItemData/Buffs/`, MaxDuration 30s, 아이콘 `T_Icon_LootBooster`) + `GE_Buff_LootSpeed`(`/Game/GAS/Effects/`, BP GE — Infinite 지속·`LootSpeed` Additive +1.0; 수명은 컴포넌트가 핸들로 직접 제거하므로 GE 자체는 무한). `DA_LootDiceRewardTable.DefaultRewards`에 4번째 아이템으로 등록. PIE 실측 완료: 픽업 → 인벤토리 버프 표시(잔여 시간 매초 감소, 재개폐해도 유지) → 루팅 게이지 2배 → 30초 만료 전 구간. `LNP.Debug.AddBagInstance`에도 버프 분기(`Cast<ULNPBuffData>`→`AddBuffItem`) 추가됨. ⚠️ 당시 `WBP_BuffEntry`(현재는 삭제)의 `DetailText`도 `bIsVariable`이 꺼져 있어 켰다 — 이 프로젝트의 위젯 BP는 이 플래그가 기본 off인 경우가 잦다.
- 스태킹/수량 없음(사본 = 엔트리 1개), 정렬/필터 UI 없음.
