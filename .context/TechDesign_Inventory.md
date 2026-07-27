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

## 4. 장착/보관 분리 (Option 2)

- 장비 슬롯(`FLNPWeaponInstance`/`FLNPSkillInstance`, `Item/LNPItemInstance.h`)에 `SourceInstance` 참조 추가 — 어느 가방 인스턴스가 장착됐는지.
- 장착(`ULNPEquipmentComponent::EquipWeaponInstance`): 슬롯이 인스턴스 참조 + 서버가 `Instance->SetEquipped(true)` + GAS grant. 해제 시 false + revoke.
- **인스턴스는 BagList에 물리적으로 남는다** — UI가 `!IsEquipped()`만 노출해 분리를 성립시킨다(서브오브젝트 재등록 불필요).
- `bEquipped`는 `ReplicatedUsing=OnRep_InstanceChanged` — 값 변경만으로는 FastArray 콜백이 안 울리므로, 서버는 `SetEquipped`에서·클라는 OnRep에서 `OnInventoryChanged`를 직접 브로드캐스트해 **장착 즉시 UI 재필터**된다. (이 통지가 없으면 "다음 인벤토리 변화 때에야 뒤늦게 사라지는" 버그가 된다 — 실측 후 수정.)
- `IsEquippedInstance(ItemId)`가 사본 정확 판정. 정의 기준 `IsEquipped`는 레거시로 잔존.
- **기본 무기(innate):** `DefaultWeapon`은 가방 인스턴스 없이 Definition만으로 장착(`SourceInstance` null) — 가방에 없으므로 모호성 없음.

## 5. 버프 흐름

- `AddBuffItem(BuffData, RemainingDuration)`: 인스턴스 생성 → GE 적용 → **GAS 핸들은 서버 전용 사이드테이블** `TMap<FGuid, FLNPBuffRuntime>`(핸들+권위 잔여시간, 복제 인스턴스에 핸들 안 실음) → `ActiveBuffList` 편입.
- 만료: 컴포넌트 tick이 런타임 잔여시간 감소 → 0 도달 시 `ExpireBuffInstance`(GE 해제 + 리스트/서브오브젝트/런타임 제거). `RemainingDuration <= 0` = 무한.
- 인스턴스의 `RemainingDuration`은 **복제 스냅샷**(추가·양도 시점 갱신). 권위 카운트다운은 서버의 `FLNPBuffRuntime`이 하고, **UI 라이브 표시는 각 머신이 로컬로 센다** — 인스턴스의 비복제 `DurationStartTime`에 스냅샷이 유효해진 로컬 월드 시각을 찍고(서버: `SetRemainingDuration`, 소유 클라: `FLNPInventoryList::PostReplicatedAdd`), `GetRemainingDurationLive()`가 `스냅샷 - 경과`를 반환한다. 시계 동기화 불필요(오차 ≈ 편도 지연). 기준 시각이 인스턴스에 남으므로 **인벤토리를 닫았다 열어도 이어서 센다**. `ULNPInventoryEntryWidget`은 버프 엔트리에만 1초 반복 타이머를 걸어 `DetailText`를 갱신(`CeilToInt`, 30s→1s)하고, 항목 재바인딩·`NativeDestruct`에서 타이머를 정리한다. 2026-07-17 PIE 검증 완료.
- 드랍/양도: `RemoveBuffInstance(ItemId)`가 잔여 초를 반환 → LootDice 페이로드 → 재획득 시 `AddBuffItem(..., 잔여)`로 복원 (라운드트립 성립).

## 6. 획득·드랍·장착 경로

- **픽업**(`ULNPInteractionComponent::PickupDiceOnServer`): 버프는 `AddBuffItem`, 그 외는 `AddItemInstance`.
- **드랍**(`ALNPPlayerCharacter::DropItem(FGuid)` → `Server_DropItem(FGuid)`): ItemId로 가방→버프 순 조회, `IsEquipped()` 가드, 제거 성공 후에만 Dice 스폰(복제 방지).
- **장착**(`EquipWeaponInstance(Instance)` → `Server_EquipWeaponInstance(FGuid)`): 클라 예측(슬롯 참조) + 서버 권위(bEquipped·GAS). RPC는 FGuid만 전달.

## 7. 잔여·후속

- **스탯 롤링 미구현**: `StatTags` 그릇은 완성됐으나 굴리는 곳이 없다. 무기 레벨/랜덤 스탯 롤링 + GameplayEffect 배율 연동(SetByCaller 등)은 후속 게임플레이 작업. 그때 LootDice 페이로드에 스탯 스냅샷 라운드트립도 함께 붙인다(빈 배관 선구현 안 함).
- ~~버프 콘텐츠 없음~~ ✅ 해소(2026-07-17): 첫 버프 콘텐츠 제작 — `DA_Buff_LootSpeed`(`/Game/ItemData/Buffs/`, MaxDuration 30s, 아이콘 `T_Icon_LootBooster`) + `GE_Buff_LootSpeed`(`/Game/GAS/Effects/`, BP GE — Infinite 지속·`LootSpeed` Additive +1.0; 수명은 컴포넌트가 핸들로 직접 제거하므로 GE 자체는 무한). `DA_LootDiceRewardTable.DefaultRewards`에 4번째 아이템으로 등록. PIE 실측 완료: 픽업 → 인벤토리 버프 표시(잔여 시간 매초 감소, 재개폐해도 유지) → 루팅 게이지 2배 → 30초 만료 전 구간. `LNP.Debug.AddBagInstance`에도 버프 분기(`Cast<ULNPBuffData>`→`AddBuffItem`) 추가됨. ⚠️ `WBP_BuffEntry`의 `DetailText`도 `bIsVariable`이 꺼져 있어 켰다 — 이 프로젝트의 위젯 BP는 이 플래그가 기본 off인 경우가 잦다.
- 스태킹/수량 없음(사본 = 엔트리 1개), 정렬/필터 UI 없음.
