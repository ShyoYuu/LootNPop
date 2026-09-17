# TechDesign — 인벤토리 아이템 인스턴스 모델

> Lyra식 하이브리드: **UObject 아이템 인스턴스 + FastArray 델타 복제 + 등록 서브오브젝트**. 스탯은 GameplayTagStack. 소스는 `Source/LootNPop/Item/`.

## 1. 왜 인스턴스 모델인가

구 모델은 가방이 `TArray<TObjectPtr<ULNPItemDefinitionBase>>`(공유 DataAsset 포인터)였다.
- 인스턴스별 상태(무기 레벨·탄창 잔량·버프 잔여시간)를 담을 수 없다.
- 장착본/보관본을 구분 못 해 정의 포인터 비교가 오검출했다(기본 무기 `DA_Pistol`을 픽업 후 드랍하면 무반응).

각 사본에 **`FGuid ItemId` 정체성**을 부여해 둘 다 해소한다. RPC도 인스턴스 포인터가 아니라 ItemId를 보낸다.

## 2. 타입 구성

| 타입 | 파일 | 역할 |
|:---|:---|:---|
| `FLNPGameplayTagStack` / `FLNPGameplayTagStackContainer` | `LNPGameplayTagStack.*` | Lyra 포팅. `Tag→int32` 스택 FastArray(델타 복제 + 조회용 TMap). 스탯 저장 |
| `ULNPInventoryItemInstance` | `LNPInventoryItemInstance.*` | 아이템 런타임 인스턴스. 복제: `Definition`·`ItemId`·`StatTags`·`bEquipped`·`ChangeCounter`·`RemainingDuration` |
| `FLNPInventoryEntry` / `FLNPInventoryList` | `LNPInventoryList.*` | 인스턴스 참조 FastArray. 복제 콜백에서 소유 컴포넌트 `OnInventoryChanged` 브로드캐스트 |
| `ULNPInventoryComponent` | `LNPInventoryComponent.*` | PlayerState 소유. `BagList`(가방)·`ActiveBuffList`(활성 버프) + 서버 전용 `TMap<FGuid, FLNPBuffRuntime>` |
| `ULNPEquipmentComponent` | `LNPEquipmentComponent.*` | PlayerState 소유. 장비 상태의 단일 원본(§4.1) |
| `FLNPWeaponInstance` / `FLNPSkillInstance` | `LNPItemInstance.h` | 장비 슬롯 내용물(인벤토리 인스턴스가 아니다 — 이름 주의) |

아이템 정의 DataAsset(`ULNPWeaponData`·`ULNPSkillData`·`ULNPBuffData`)은 [TechDesign_Ability.md](TechDesign_Ability.md) §2.3.

인스턴스 스탯 태그(`LNPGameplayTags.h`):
- `TAG_Item_Level`(`LNP.Item.Level`) — 레벨(§7.1)
- `TAG_Item_AmmoSpent`(`LNP.Item.AmmoSpent`) — 탄창 **소모량**(§7.4)

## 3. 복제 방식 (Iris 검증 완료)

- 컴포넌트: `bReplicateUsingRegisteredSubObjectList = true`. 인스턴스 추가/제거 시 `AddReplicatedSubObject(Instance, COND_OwnerOnly)` / `RemoveReplicatedSubObject`.
- 두 리스트 모두 `DOREPLIFETIME_CONDITION(..., COND_OwnerOnly)` — 소유 클라이언트에만.
- FastArray의 `OwnerComponent` 역참조는 비복제 — **양측 생성자에서 `SetOwnerComponent(this)`**.
- 2인 PIE 실측(2026-07-13): 서버가 원격 클라 가방에 추가 → 그 클라에서 `PostReplicatedAdd` 수신. 검증 커맨드는 `LNP.Debug.AddBagInstance`(§7.4). 호스트 대상은 원본이라 콜백이 안 뜬다 — 원격 클라를 지정해야 검증된다.

### 3.1 ⚠️ Iris는 `PostReplicatedRemove`를 부르지 않는다 (2026-08-20)

**증상:** 게스트가 아이템을 드랍하거나 버프가 만료돼도 인벤토리 UI에 항목이 남는다. 호스트는 `RemoveItemInstance`에서 직접 통지하므로 멀쩡하다.

**원인:** Iris FastArray 디스패치(`FastArrayReplicationFragmentInternal.h`)는 이름이 비대칭이다.

| 이벤트 | Iris가 부르는 콜백 |
|:--|:--|
| 추가 | `PostReplicatedAdd` |
| 변경 | `PostReplicatedChange` |
| **제거** | **`PreReplicatedRemove`** — `PostReplicatedRemove`는 불리지 않는다 |

**두 번째 함정:** `PreReplicatedRemove`는 **항목이 아직 배열에 남아 있을 때** 불린다. 여기서 바로 통지하면 UI가 사라질 항목을 다시 그린다.

**해결:** `PreReplicatedRemove`에서 `SetTimerForNextTick`으로 다음 틱에 통지한다. 통지가 겹쳐도 목록 재구성은 멱등이다. `PostReplicatedRemove`는 Iris를 끈 클래식 경로용으로 남겨 뒀다.

## 4. 장착/보관 분리

- 장비 슬롯(`FLNPWeaponInstance`)의 `SourceInstance`가 어느 가방 인스턴스가 장착됐는지 가리킨다.
- 장착(`ULNPEquipmentComponent::EquipWeaponInstance`, **서버 전용**): 슬롯이 인스턴스 참조 + `SetEquipped(true)` + GAS grant + 탄창 어트리뷰트 복원. 해제(`ClearWeaponSlot`)는 재장전 취소 → 잔량 저장 → `SetEquipped(false)` → revoke. 같은 인스턴스 재장착 요청은 조기 반환.
- **인스턴스는 장착 중에도 `BagList`에 남는다**(서브오브젝트 재등록 불필요). 인벤토리 UI는 장착본도 함께 보여 주고 셀에 장착 배지("E")를 붙인다. 드랍·합성 재료는 `IsEquipped()`로 걸러 낸다.
- `bEquipped`는 `ReplicatedUsing=OnRep_InstanceChanged`. 값 변경은 FastArray 콜백을 울리지 않으므로 서버는 `SetEquipped`에서, 클라는 OnRep에서 `OnInventoryChanged`를 직접 쏜다. 이 통지가 없으면 배지가 다음 인벤토리 변화 때에야 갱신된다.
- `IsEquippedInstance(ItemId)`(사본 정확 판정)·정의 기준 `IsEquipped(Def)`(레거시)는 둘 다 호출처가 없다 — UI·드랍·합성은 인스턴스의 `IsEquipped()`를 직접 읽는다.
- **기본 무기:** `EnsureDefaultWeapon()`이 `DefaultWeapon`(현재 `DA_Pistol`)의 **가방 인스턴스를 만들어 장착**한다 — 루팅 무기와 같은 상태 기계를 탄다. 조회를 먼저 해 사본이 쌓이지 않고, 이미 다른 무기를 들고 있으면 손대지 않는다. 인벤토리 컴포넌트가 없는 소유자만 정의로 장착하는 폴백(`EquipWeapon(Def)`, `SourceInstance` null)을 탄다.

> ⚠️ **`EnsureDefaultWeapon`은 `EqComp::BeginPlay`에서 부르면 안 된다** (2026-08-20 2P 실측).
> - 증상: 게스트 가방이 빈 채로 남는다. `PostReplicatedAdd`는 `bag size=1`로 울리지만 `Entry.Instance`가 null.
> - 원인: 게스트의 PlayerState는 BeginPlay 시점에 아직 자기 연결로 복제를 시작하지 않았다. 이때 등록한 서브오브젝트를 참조하는 엔트리는 미해결 포인터로 초기 번치에 실리고, FastArray는 "이미 보냄"으로 간주해 재전송하지 않는다.
> - 해결: `ALNPPlayerCharacter::PossessedBy`에서 호출한다 — 폰 스폰 게이트를 통과해 연결이 성립한 뒤다. 리스폰도 새 폰의 `PossessedBy`가 다시 부른다.
> - **교훈: 복제 서브오브젝트를 참조하는 컨테이너는 액터가 복제를 시작한 뒤에 채운다.**

### 4.1 장비 상태의 소유권 (2026-08-20 정리)

`ULNPEquipmentComponent::WeaponSlot`이 **복제되는 단일 원본**이다. 이전에는 Pawn의 무기 데이터만 복제되고 `WeaponSlot`은 비복제라, 시뮬레이티드 프록시에서 두 값이 갈라졌다(롱소드를 휘두르는 캐릭터를 권총 데이터로 판정). 배경은 [TechDesign_Networking.md](TechDesign_Networking.md) §3.9.

| 대상 | 역할 | 복제 |
|:--|:--|:--|
| `ULNPEquipmentComponent::WeaponSlot` | 단일 원본, **서버만 쓴다** | ✅ `ReplicatedUsing=OnRep_WeaponSlot` (조건 없음 — 프록시 포함) |
| `FLNPWeaponInstance::Definition` | 무엇을 들고 있는가 | ✅ 구조체에서 유일하게 복제되는 필드 |
| `FLNPWeaponInstance`의 `SourceInstance`·`GrantedAbilities`·`AppliedEffects` | 서버 전용 | ❌ `NotReplicated` — `SourceInstance`는 OwnerOnly 서브오브젝트라 비소유자에서 영원히 해소되지 않고, 복제하면 Iris가 계속 dirty로 잡는다 |
| `ALNPCharacterBase::CachedWeaponDef` | 비주얼 파생 캐시 | ❌ |
| `ALNPEnemyCharacter::EnemyConfig` | 적 무기의 원본 (EqComp 없음) | ✅ `ReplicatedUsing=OnRep_EnemyConfig` |

- **쓰기 경로는 하나다.** 클라는 `ALNPPlayerCharacter`의 `Server_Equip*` RPC만 보내고 로컬 선반영은 하지 않는다. 뮤테이터는 `EnsureAuthority`(`ensureMsgf`)로 권위를 강제한다. 무기 교체는 메뉴·디버그 키에서만 일어나 RTT 지연이 문제되지 않는다.
- **비주얼 적용 함수도 하나다** — `ALNPCharacterBase::ApplyWeaponVisuals()`. 서버·클라 모두 `OnWeaponSlotApplied`(서버는 슬롯 적용 직후, 클라는 `OnRep_WeaponSlot`)를 거쳐 부른다. 멱등이다.
- **도착 순서**: 슬롯은 PlayerState에, 비주얼은 Pawn에 있고 복제 순서는 보장되지 않는다. 푸시(`PushWeaponToPawn`, Pawn 없으면 no-op)와 풀(Pawn의 `BeginPlay`/`OnRep_PlayerState`/`PossessedBy`에서 `ResolveWeaponDefForVisuals()`)을 양방향으로 걸어 수렴시킨다.
  ⚠️ 풀은 반드시 `InitAbilitySystem()` **뒤에** — 그쪽이 `CurrentAnimSetTag`를 Unarmed로 되돌린다.
- **UI 갱신**: `OnEquipmentChanged`(서버 적용·클라 OnRep 양쪽). `OnInventoryChanged`만으로는 부족하다 — 두 컴포넌트의 OnRep 순서가 보장되지 않고, 인벤토리 없는 폴백 장착은 `bEquipped` 복제 자체가 없다. 스탯 탭은 두 신호를 모두 구독한다.
- **범위 밖**: `ActiveSkillSlots`/`PassiveSkillInstances`는 비복제라 같은 구멍이 있다. 액티브 스킬이 미구현이라 구현 시점에 같은 패턴으로 처리한다.

## 5. 버프 흐름

- `AddBuffItem(BuffData, RemainingDuration)`: 인스턴스 생성 → 만료 타이머 → GE 적용 → 핸들을 **서버 전용 사이드테이블** `FLNPBuffRuntime`에 보관(복제 인스턴스에 핸들을 싣지 않는다) → `ActiveBuffList` 편입.
  GE는 두 갈래다: 정의의 `EffectsToApply`(특수 효과)와 `StatModifiers`(선언형 스탯, `LNPStat::ApplyModifiers`).
- **지속 시간 규약**: `> 0` = 기간제, **`-1`(`LNPBuff::PermanentDuration`) = 영구**, `0` = 잘못된 설정(경고 후 영구 취급).
  `AddBuffItem`의 인수 0은 "페이로드 없음"이라 정의값으로 폴백한다. **영구는 반드시 -1로 왕복해야 한다.**
- **만료(서버 권위)**: 만료 시각을 한 번 확정하고 `FTimerManager`에 맡긴다. 타이머가 `ExpireBuffInstance`(GE 해제 + 리스트/서브오브젝트/런타임 제거)를 발화한다.
  - 영구 버프는 타이머를 걸지 않는다 — **타이머 핸들 무효 자체가 "영구" 판별**이다.
  - `ExpireWorldTime`은 양도 시 남은 초를 역산하는 데만 쓴다.
  - 이전(2026-08-17까지)엔 `TickComponent`가 매 프레임 감산했다. 데드라인 방식으로 바꿔 버프별 float 누적 오차와 컴포넌트 틱을 없앴다.
  - `ExpireBuffInstance(FGuid)`는 **값 전달**이어야 한다 — `CreateUObject`가 decay된 페이로드 타입을 요구해 `const FGuid&`는 컴파일되지 않는다. 진입 시 `ClearTimer`로 조기 제거 경로의 타이머 잔류를 막는다.
- **UI 잔여 시간**: 인스턴스의 `RemainingDuration`은 추가 시점의 **복제 스냅샷**이다. 각 머신이 로컬로 센다.
  - 비복제 `DurationStartTime`에 스냅샷이 유효해진 로컬 시각을 찍는다(서버: `SetRemainingDuration`, 소유 클라: `FLNPInventoryList::PostReplicatedAdd`의 `MarkDurationStart`).
  - `GetRemainingDurationLive()` = `스냅샷 - 경과`. 시계 동기화 불필요(오차 ≈ 편도 지연). 기준 시각이 인스턴스에 있어 **메뉴를 닫았다 열어도 이어서 센다**.
  - 표시 위젯: `ULNPBuffChipWidget`(스탯 탭)·`ULNPMenuItemCellWidget`(인벤토리 셀 배지)은 1초 반복 타이머로 갱신하고 재바인딩·`NativeDestruct`에서 정리한다. `ULNPItemDetailPanelWidget`은 `SetItem` 시점에 한 번만 읽는다. 상세는 [TechDesign_InGameMenu.md](TechDesign_InGameMenu.md).
- **드랍/양도**: `RemoveBuffInstance(ItemId)`가 잔여 초(영구는 -1)를 반환 → LootDice 페이로드 → 재획득 시 `AddBuffItem(..., 잔여)`.
  페이로드는 **절대 만료 시각이 아니라 상대 잔여 초**다. 그래서 월드에 놓인 Dice는 버프 시간이 **동결**되고 Dice 수명만 흐른다(기획 의도, [GameDesign_LootDice.md](GameDesign_LootDice.md) §5).

## 6. 획득·드랍·장착 경로

모든 진입점은 `ALNPPlayerCharacter`에 있고 `HasAuthority()`면 직접, 아니면 `Server_*` RPC(ItemId 전달)로 간다. 서버는 소유 인벤토리에서 ItemId를 다시 조회·검증한다.

- **픽업**(`ULNPInteractionComponent::PickupDiceOnServer`): 버프는 `AddBuffItem(Def, 잔여)`, 그 외는 `AddItemInstance(Def, Level, AmmoSpent)`.
- **빈 슬롯 자동 장착**: `AddItemInstance`가 가방 편입 직후 `ULNPEquipmentComponent::TryAutoEquipWeapon`을 부른다. 슬롯이 비어 있고 획득물이 무기일 때만 장착한다(획득이 현재 장비를 갈아치우면 안 된다).
  - 훅을 `AddItemInstance`에 둔 이유: 획득 경로가 여럿(LootDice 픽업, `LNP.Debug.AddBagInstance`, 디버그 키 지급, `EnsureDefaultWeapon`)이고 전부 이 관문을 지난다.
  - 레벨·탄창 소모량은 자동 장착 **전에** 기록해야 한다 — 장착이 이 값으로 GAS 레벨과 탄창을 정한다.
- **드랍**(`DropItem(FGuid)` → `Server_DropItem` → `DropItemOnServer`): 가방→버프 순 조회, 장착본 거부. 제거·스폰 꼬리는 `RemoveAndSpawnDice()` 공용 경로다.
  - 페이로드(정의·레벨·탄창 소모량)는 제거 **전에** 읽고, **제거 성공 후에만** Dice를 스폰한다(아이템 복제 방지).
  - 이 규칙이 두 곳에 복제되면 곧 아이템 복제 버그가 되므로 공용 경로로 뺐다.
- **사망 전량 드랍**(`DropAllItemsOnDeath()`, 서버, 2026-08-21): 가방(**장착본 포함**)과 활성 버프를 전부 쏟는다. `UnequipWeapon()`으로 슬롯을 먼저 비우고(GAS 회수·`SourceInstance` 댕글링 방지) ItemId를 스냅샷해 순회한다. → [TechDesign_LootDice.md](TechDesign_LootDice.md) §2.7.1
  - ⚠️ 리스폰 때 `EnsureDefaultWeapon()`이 가방에서 기본 무기를 못 찾아 새 인스턴스를 만든다 — 사망마다 기본 무기 사본이 세상에 하나씩 는다. **의도적 방치**(밸런스 문제가 되면 대응).
- **합성**(`RequestMergeItem(FGuid)` → `Server_MergeItem` → `MergeItemOnServer` → `TryMergeItem`): §7.2.
- **장착**(`RequestEquipWeaponInstance(Instance)` → `Server_EquipWeaponInstance(FGuid)`): 서버 권위 전용, 클라 예측 없음. 결과는 `WeaponSlot` 복제로 돌아온다(§4.1).
- **정의 기반 장착**(`RequestEquipWeapon(Def)` → `Server_EquipWeapon` → `EquipWeaponOnServer`): 디버그 키(`EquipTestWeapon`) 전용.
  1. 가방에서 같은 정의의 인스턴스를 찾는다.
  2. 없으면 **캐릭터의 `TestWeaponList`에 있을 때만** 지급한다 — 없으면 클라가 임의의 `ULNPWeaponData` 에셋을 지목해 장착할 수 있다.
  3. `EquipWeaponInstance`로 넘긴다. 모든 장착이 인스턴스를 거치므로 `bEquipped`·UI·드랍 가드가 경로와 무관하게 같다.
  - `Def == nullptr`은 맨손 전환(`UnequipWeapon`)이다.

## 7. 무기 레벨·합성 (2026-08-20)

### 7.1 레벨은 어디에 있나

인스턴스 `StatTags`의 `TAG_Item_Level` 스택 카운트가 레벨이다 — 이미 복제되는 그릇이라 배관 추가가 없다. 읽기·쓰기는 `GetItemLevel()` / `SetItemLevel(int32)`로 통일한다(레벨 없는 아이템도 1로 읽힌다).

**레벨별 값의 원본은 `ULNPWeaponData::LevelTable`이다**(행 구조 `FLNPWeaponLevelRow`, **행 이름 = 레벨 숫자**, 마지막 연속 행 = 최대 레벨). 레벨마다 절대값을 손으로 넣는 구조다. 장착 시 레벨이 어빌리티 스펙 레벨이자 스탯 행 선택 기준이 된다. 상세는 [TechDesign_Ability.md](TechDesign_Ability.md) §2.3.

> `UDataTable::GetRowMap()`은 TMap이라 저작 순서가 보존되지 않는다 — 인덱스가 아니라 이름으로 조회해야 한다.

### 7.2 합성 — 대상 자신이 결과물이 된다

`ULNPInventoryComponent::CanMergeItem`(판정, 클라도 호출 가능 — Merge 버튼 표시용) / `TryMergeItem`(서버 실행, 판정을 처음부터 다시 한다).

- 대상은 무기만, 최대 레벨 미만일 때만.
- 재료는 **대상을 제외한** 정의·레벨이 같은 **비장착** 가방 인스턴스. `LNPSettings.WeaponMergeMaterialCount`(기본 3) - 1개를 소모하고 **대상의 레벨을 +1**한다.

| 대상 | 소모 | 결과 |
|:--|:--|:--|
| 비장착 무기 | 재료 n-1개 (+대상 자신) | n개 투입 → 1개 산출 |
| 장착 중인 무기 | 재료 n-1개 | 장착 무기 +1레벨 |

- 대상이 결과물이라 **ItemId가 보존**된다 → 합성 후에도 UI 선택이 유지된다.
- 장착본은 재료 후보에서 걸러지므로 "장착 중인 무기를 재료로 못 쓴다"가 자연히 성립한다.
- 대상이 장착본이면 `RefreshWeaponSlotGrants()`로 GAS만 새 레벨로 재부여한다.
  ⚠️ `ClearWeaponSlot`→`Equip` 경로를 쓰면 안 된다 — `bEquipped`가 내려갔다 올라가 UI가 깜빡이고, `EquipWeaponInstance`의 "같은 인스턴스면 조기 반환"에 걸려 아무 일도 안 일어난다.

### 7.3 클라이언트 통지 — `ChangeCounter`가 필요한 이유

레벨은 **인스턴스 자신의** `StatTags`에 있어 `BagList` FastArray 콜백을 울리지 않고, `StatTags` 쪽 FastArray는 소유 컴포넌트를 모른다. 원격 클라에서 "재료 제거 통지"와 "레벨 복제"가 다른 경로로 와 순서가 보장되지 않는다.
→ `SetItemLevel`이 `ChangeCounter`(ReplicatedUsing=`OnRep_InstanceChanged`)를 함께 올려 `bEquipped`와 같은 통지 경로를 태운다.

같은 함정("목록은 그대로인데 항목 내부만 바뀜")의 UI 판이 둘 더 있다:
- 셀 배지: `ItemGrid->RegenerateAllEntries()` — 목록이 같으면 기존 행을 재사용해 `NativeOnListItemObjectSet`이 다시 안 불린다.
- 디테일 패널: `RefreshGrid()`가 **항상** `DetailPanel->SetItem()`을 부른다 — 선택 대상이 그대로면 `OnItemSelectionChanged`가 다시 안 온다.

### 7.4 드랍→재획득 왕복

LootDice 페이로드(`COND_InitialOnly`)에 `ItemLevel`이 실린다 → [TechDesign_LootDice.md](TechDesign_LootDice.md) §2.2. 픽업은 서버가 자기 값을 읽으므로 **다른 플레이어가 주워도 레벨이 보존된다.**

**탄창 잔량도 같은 길로 왕복한다 (2026-09-14).**
- 인스턴스 `StatTags`의 `TAG_Item_AmmoSpent`에 **남은 수가 아니라 소모량**을 저장한다 — 태그 스택은 0이면 엔트리가 사라지므로, 부재 = 0 = 가득이 되는 소모량이라야 신품이 저절로 가득 찬다.
- 장착 해제(`ClearWeaponSlot`) 시 저장 → Dice의 **비복제** `AmmoSpent` → `AddItemInstance(Def, Level, AmmoSpent)` → 장착 시 `ApplyMagazineAttributes`가 복원. 상세는 [TechDesign_Ability.md](TechDesign_Ability.md) §5.5.

디버그: `LNP.Debug.AddBagInstance <ItemDefPath> [PlayerIndex] [Level]`(권위 전용, 인수 없으면 컨트롤러 인덱스 나열, 버프 정의면 `AddBuffItem`으로 분기). 합성 테스트는 같은 레벨 사본이 n개 필요해 레벨 인수를 둔다.

---

## 8. 잔여·후속

- **랜덤 스탯 롤링 미구현**: 인스턴스별 랜덤 스탯은 아직 없다. `StatTags` 그릇을 그대로 쓰고, LootDice 페이로드에 스탯 스냅샷 왕복을 레벨 옆에 함께 넣어야 한다.
- 버프 콘텐츠: 스탯 6종 × 합/곱 12종(→ [GameDesign_Ability.md](GameDesign_Ability.md) §3.3). ⚠️ 이 프로젝트의 위젯 BP는 `bIsVariable`이 기본 off인 경우가 잦다 — 첫 버프 UI 제작 때 텍스트 블록에서 걸렸다.
- 스태킹/수량 없음(사본 = 엔트리 1개), 정렬/필터 UI 없음.
