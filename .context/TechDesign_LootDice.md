# LootDice 시스템 기술 설계

> **상태: 구현 완료** (코드 2026-07-12, 에디터 에셋 `/Game/LootDice/` — BP_LootDice·M_LootDice·PM_LootDice·DA_LootDiceRewardTable, LNP Settings 지정).
> **검증:** PIE 1인 — 디버그 스폰 → 굴림·정지, 60초 정각 소멸, F 획득 → 인벤토리 편입. PIE 2인(2026-07-17) — 루팅 완료 → 보상 Pop → 획득 → 인벤토리 반영까지 호스트 화면 기준 정상. **잔여:** 정지 윗면의 클라이언트 일치·슬립 후 트래픽 중단 실측(net stat), 동시 획득 선착순, 드랍/양도 2인 확인.
> 기획은 [GameDesign_LootDice.md](GameDesign_LootDice.md). 구현 파일: `LootDice/LNPLootDice.h/.cpp`, `LootDice/LNPLootDiceRewardTable.h`, `Interaction/LNPInteractableRegistrySubsystem.h/.cpp`.

## 1. 한눈에 보기

**LootDice**는 서버 권위 물리 시뮬레이션 + Iris 표준 Actor 복제로 구현하는 픽업 Actor. LootPod과 달리 **Mass 엔티티를 쓰지 않는다** — 동시 존재 수가 적고(Pod당 수 개 × 제한 수명), 리지드바디 물리와 페이로드 복제가 필요해 순수 Actor가 적합하다.

```
[ALNPLootPod Popped]      ──┐
[인벤토리 드랍 RPC]        ──┤
[플레이어 사망 전량 드랍]  ──┴─▶ [서버: ALNPLootDice::SpawnDice — Pop 임펄스·고속 회전]
                                ├─ 물리: 서버 시뮬 + ReplicatedMovement (Iris 네이티브, 각속도 포함)
                                ├─ 획득: 인터랙터블 레지스트리 → ULNPInteractionComponent → Server RPC → 인벤토리
                                └─ 소멸: SetLifeSpan → Destroy 복제
```

---

## 2. 설계

### 2.1 왜 Mass가 아니라 순수 Actor인가

Enemy(수백 규모)·발사체(고빈도 생성)와 달리 LootDice는 소수·단명이다. Chaos 리지드바디, 페이로드 프로퍼티 복제 전부 Actor 인프라가 그대로 제공하므로, Mass로 얻을 스케일 이득이 없고 브릿지 비용만 생긴다.

### 2.2 클래스 구성 — `ALNPLootDice`

| 요소 | 내용 |
|:---|:---|
| 루트 `UStaticMeshComponent` | 큐브, `SimulatePhysics`(서버 권위). ⚠️ **시뮬레이트 중인 PrimitiveComponent가 루트여야** `GatherCurrentMovement`가 선속도·각속도까지 `FRepMovement`에 싣는다 — 별도 SceneComponent 루트 금지 |
| 충돌 | `bUseCCD` — ⚠️ 지각은 두께 없는 단면이라, 스윕으로 도는 폰·발사체와 달리 이산 스텝으로 도는 강체는 한 스텝 변위가 크면 표면을 통과해 월드 밖으로 나간다. Pawn·Camera 채널은 Ignore — 플레이어가 몸으로 밀거나 차지 못하게 해 정지 위치를 보존한다 |
| 아이콘 표시 | **6면 모두 아이콘 텍스처** + 카테고리 색 에미시브 MID(`IconTexture`/`CategoryColor` 파라미터) — 정지 후 어느 면이 위든 식별 가능, 빌보드·위젯 불필요. 머티리얼 미지정이면 조용히 스킵한다 |
| 상호작용 등록 | LootPod과 동일한 레지스트리(BeginPlay/EndPlay 자기 등록) — LootPod 전용 레지스트리를 범용 인터랙터블 레지스트리로 일반화해 재사용. SmartObject는 폐기 (→ [DiscardedApproaches.md](DiscardedApproaches.md) Case 03) |

**페이로드** — 스폰 후 불변이라 `COND_InitialOnly`로 스폰 번치에만 싣는다 (Deferred 스폰으로 `FinishSpawning` 전 대입을 보장).

| 필드 | 내용 |
|:---|:---|
| `ItemDef` | `ULNPItemDefinitionBase` 에셋 참조 |
| `RemainingDuration` | 버프 잔여 시간 (신품은 0 = 풀 지속) |
| `ItemLevel` | 무기 강화 레벨 (신품은 1) |
| `SpawnServerTime` | 소멸 경고 연출의 클라이언트 로컬 계산용 |
| `AmmoSpent` | 무기 탄창 소모량 (신품은 0). **비복제** — 서버만 읽고 클라이언트에는 보여 줄 곳이 없다 |

> **`ItemLevel`·`AmmoSpent`** — 합성으로 올린 레벨과 쓰던 탄창이 드랍→재획득에서 소실되지 않게 한다. 획득 처리(`PickupDiceOnServer`)는 서버가 자기 값을 읽어 `AddItemInstance(Def, Level, AmmoSpent)`로 넘기므로, **드랍한 사람이 아니라 줍는 사람이 누구든** 그대로 이전된다. 버프의 `RemainingDuration`과 달리 월드에 놓인 동안 변하지 않으므로 동결·역산 규칙이 필요 없다.

### 2.3 물리 동기화 — Iris + FRepMovement

**엔진 확인 (UE 5.8):** Iris에 별도의 "물리 동기화 기능"은 없다. 표준 경로인 `AActor::ReplicatedMovement`(`FRepMovement`)가 Iris 전용 `RepMovementNetSerializer`로 **네이티브 지원**되며, 서버 시뮬 → `GatherCurrentMovement`(위치·회전·선속도·**각속도**) → 클라이언트 보정 블렌딩으로 굴림이 회전까지 동기화된다.

**대역폭 제어 3중 장치:**

1. `NetUpdateFrequency` 20 — 바운스 없는 낮은 반발이라 굴림 활성 구간이 수 초로 짧다. NetCull 10,000cm(분배 논의는 근접 상황).
2. **물리 슬립 후 트래픽 자연 소멸** — 정지하면 `ReplicatedMovement` 델타가 없어 복제가 멈춘다.
3. **제한 수명** — 미획득 시 소멸이 최악 케이스 대역폭의 상한.

Chaos Networked Physics(리심 기반 예측)는 과잉 — 굴림은 게임플레이 판정이 없는 코스메틱이라 서버 권위 스냅샷 + 보정으로 충분하다.

**발사체(Spawn-Only 방송)와 판단이 갈리는 지점:** 발사체는 "빠르고 많다"라서 지속 복제가 부적합했지만([TechDesign_Networking.md](TechDesign_Networking.md) §3.3), LootDice는 "느리고 적고 **정지 위치·윗면이 클라이언트 간 일치해야 한다**" — 파티원이 가리키며 분배를 논의하는 대상이므로 로컬 독립 시뮬은 부적합하다.

### 2.4 구형 중력 — AddForce 방식의 타당성 (엔진 소스 확인)

**"AddForce만으로 자연스러운 중력이 되는가" → 된다.** 엔진 내장 중력도 Chaos `PerParticleGravity`가 매 서브스텝 가속도를 더하는 **상수 가속도일 뿐**이라, `bEnableGravity=false`로 내장 -Z를 끄고 매 Tick `AddForce(GravityDir × GravityAccel, bAccelChange=true)`를 주면 **동일한 수학**으로 적분된다. 방향은 Tick당 1회만 갱신되지만 행성 곡률 대비 한 Tick 이동 거리가 미미해 오차는 무시 가능하다.

- 방향은 `ALNPGameState::bIsSphereWorld`(복제)를 보고 산출한다 — 구 내벽 세계는 "바깥쪽 = 아래"(폰 중력과 같은 부호), GameState 미도착 프레임은 평면 중력 폴백. 가속도 `GravityAccel = 2000`은 폰 중력과 같은 값이라 낙하 체감이 통일된다.
- 물리 권위는 서버지만, 클라이언트 로컬 물리 블렌딩 품질을 위해 클라이언트에서도 동일한 힘을 적용한다.

⚠️ **슬립을 깨우지 않는다.** 잠든 바디에 매 Tick 힘을 넣으면 영원히 잠들지 못해 §2.3의 "슬립 후 트래픽 소멸"이 무너진다. `IsAnyRigidBodyAwake()`가 참일 때만 AddForce 한다.

**대안 검토 — Chaos Gravity Group (엔진 확인):** Chaos는 최대 8개 gravity group별 **가속도 벡터**를 지원한다 (`FBodyInstance::SetGravityGroupIndex`). 그룹당 방향이 고정이라 위치에 따라 연속으로 변하는 구면 방사 중력에는 부적합 — 고정 방향으로 근사 가능한 국소 구역이 생기면 재검토 가치가 있다.

### 2.5 Dice 물리 튜닝 — "굴리고, 멈추고, 공개한다"

| 항목 | 값 |
|:---|:---|
| 스폰 각속도 | `SpinSpeedRad` 20 rad/s, 랜덤 축 — 공중에서 아이콘 인지 불가 |
| Pop 임펄스 | `PopImpulseSpeed` 600 cm/s를 표면 Up 기준 `PopConeHalfAngleDeg` 25° 원뿔 안 랜덤 방향으로 (드랍은 ×0.4의 "작은 Pop") |
| Restitution / Friction / Angular Damping | `PM_LootDice` — 통통 튀지 않고 데구루루 굴러 수 초 내 정지 |

공개 타이밍(착지→정지)이 곧 연출이므로, 튜닝 파라미터는 Physical Material + CDO 프로퍼티로 모아 반복 조정할 수 있게 뒀다.

### 2.6 획득 흐름

```
ULNPInteractionComponent가 레지스트리 순회로 후보 탐색 (LootPod과 동일 파이프라인)
→ 프롬프트가 떠 있는 단일 대상에만 Interaction Input → Server_PickupDice RPC
→ 서버 재검증: CanInteract(거리 + bClaimed + 파괴 진행)
→ ItemDef 유형별 편입: 버프 → AddBuffItem(RemainingDuration) / 그 외 → AddItemInstance(Def, ItemLevel, AmmoSpent)
→ SetClaimed() + Destroy (복제로 전 클라이언트 제거)
```

- **후보 게이트가 LootPod과 방향이 반대다.** Pod은 *자기 전방에* 플레이어가 있는지 보지만, Dice는 *플레이어(캐릭터) 전방 140° 원뿔* 안에 Dice가 들어와야 후보가 된다. 뒤쪽 Dice는 더 가까워도 빠져 "지금 보고 있는 것"이 우선된다. 기준은 카메라가 아니라 캐릭터 facing이다.
- **선착순 2중 방어:** 동시 시도의 순서는 서버 RPC 직렬화가 만들고, `Destroy`가 실제로 반영되기까지의 프레임 공백은 `bClaimed` 플래그(비복제, 서버 전용)가 막는다.
- ⏸ **획득 연출(GameplayCue)은 미구현** — 현재 Dice는 소리·이펙트 없이 사라진다.

### 2.7 인벤토리 드랍 흐름 (양도)

```
인벤토리 UI에서 미장착 아이템 드랍 → DropItem(FGuid ItemId) → Server_DropItem(FGuid) RPC
→ ItemId로 인스턴스 조회(가방→버프 순) + IsEquipped() 가드
   ([TechDesign_Inventory.md](TechDesign_Inventory.md) 인스턴스 모델 — 사본을 ItemId로 정확 식별)
→ 제거 전에 무기 레벨·탄창 소모량을 읽어 둔다 (제거하면 인스턴스가 사라진다)
→ RemoveItemInstance() / RemoveBuffInstance() — 버프는 잔여 시간을 반환받아 페이로드에 싣는다
→ 캐릭터 전방에 ALNPLootDice 스폰 (ImpulseScale 0.4 = 작은 Pop)
→ 이후 소멸·획득 규칙은 LootPod 보상과 완전 동일 — 스폰은 공용 SpawnDice로 묶여 있다
```

⚠️ **인벤토리 제거가 성공하기 전에는 스폰하지 않는다** — 아이템 복제(duplication) 방지.

### 2.7.1 사망 전량 드랍 (2026-08-21)

플레이어가 죽으면 **가방 전체(장착 무기 포함) + 활성 버프 전부**가 사망 지점에서 한꺼번에 Pop 한다.
`ALNPPlayerCharacter::DropAllItemsOnDeath()`(서버 전용) → `RemoveAndSpawnDice()`(§2.7과 공유하는 제거·스폰 공용 경로) → `SpawnDice(..., ImpulseScale=1.0)`.

- **장착 해제가 먼저다.** `ULNPEquipmentComponent::UnequipWeapon()`으로 슬롯을 비운다 — 장착본이 `bEquipped`인 채 제거되면 `WeaponSlot.SourceInstance`가 댕글링이 되고 무기 GAS 부여가 회수되지 않는다. (UI 드랍 경로의 `IsEquipped()` 거부 가드는 그대로다 — 규칙이 바뀌는 건 사망뿐이다.)
- **ItemId를 먼저 스냅샷한다.** 제거 루프가 FastArray와 등록 서브오브젝트를 건드리므로 인스턴스 포인터를 들고 순회하면 안 된다.
- 스폰 위치는 사망 지점 한 곳이면 충분하다 — §2.8의 원뿔 랜덤 임펄스가 자연히 흩뿌린다 (`SpawnPodRewards`와 같은 전략).
- 리스폰이 10초, Dice 수명이 60초라 본인·파티원 모두 회수할 시간이 있다.

상세 흐름은 [TechDesign_CharacterMovement.md](TechDesign_CharacterMovement.md) §9 참조.

### 2.8 Pop 임펄스

서버가 스폰 시 표면 Up 기준 원뿔 내 랜덤 방향 임펄스 + 랜덤 축 고속 회전을 1회 부여 — 다중 스폰 시 자연스럽게 흩어진다. 임펄스·회전은 복제할 필요가 없다 (`ReplicatedMovement`가 결과 궤적을 나른다).

### 2.9 소멸

`SetLifeSpan(DiceLifetime)` — 서버에서 60초 후 `Destroy()`, 복제로 전 클라이언트에서 제거된다. 마지막 `BlinkWarnSeconds`(5초) 깜빡임 경고는 클라이언트 로컬 계산이다 — `SpawnServerTime`(초기 복제) + 수명 상수(CDO)만으로 잔여 시간을 구하므로 추가 복제가 필요 없다. 깜빡임 속도는 `BlinkPeriod`(1주기 초, 기본 0.2 — 작을수록 빠름)로 BP CDO에서 조절한다.

---

## 3. 구현 단계 (전부 완료)

| 단계 | 내용 |
|:---|:---|
| 1 | `ALNPLootDice` 뼈대: 큐브 메시+물리+페이로드+수명. 디버그 스폰 `LNP.Debug.SpawnLootDice [개수] [ItemDef 경로]` — 경로를 생략하면 `DefaultRewards`에서 **Dice마다 개별 추첨**한다(페이로드 없는 더미는 획득해도 인벤토리에 안 들어가므로) |
| 2 | 구형 중력 + 슬립 가드 (§2.4) |
| 3 | 복제: `bReplicates` + `SetReplicatingMovement(true)` + `COND_InitialOnly` 페이로드 (Deferred 스폰) |
| 4 | 레지스트리 일반화(`ULNPInteractableRegistrySubsystem`) + `Server_PickupDice` + 인벤토리 편입 |
| 5 | 드랍: `DropItem(FGuid)`/`Server_DropItem` — ItemId 조회 + `IsEquipped()` 가드 (2026-07-17 인스턴스 모델로 전환) |
| 6 | LootPod Popped 연결: `ULNPLootDiceRewardTable` + `SpawnPodRewards` (→ [TechDesign_LootPod.md](TechDesign_LootPod.md) §5.3) |
| 7 | 6면 아이콘 MID 훅·카테고리 색·소멸 깜빡임 |
| 8 | 보상 후보 풀 가중 추첨 (`FLNPLootDiceRewardEntry` + `MinDrops`/`MaxDrops`, 2026-07-27) |

**에디터 에셋:** BP_LootDice(큐브 메시 ~30cm), M_LootDice(`IconTexture` Texture Param + `CategoryColor` Vector Param 에미시브), PM_LootDice(낮은 Restitution·높은 Friction·Angular Damping), DA_LootDiceRewardTable, LNP Settings 지정(`LootDiceClass`/`LootDiceRewardTable`), ItemDef 에셋 `Icon` 지정 — 모두 완료. BP CDO는 C++ 기본값을 하나도 덮지 않는다(§2.5·§2.9 수치가 곧 실효값).

### 3.1 보상 테이블 구조 (2026-07-27)

`ULNPLootDiceRewardTable`은 **후보 풀**이지 드랍 목록이 아니다. 드랍 가능한 모든 보상을 등록해 두고 Pop 시점에 일부만 뽑는다 (→ [GameDesign_LootDice.md](GameDesign_LootDice.md) §2.5).

| 필드 | 역할 |
|:---|:---|
| `FLNPLootDiceRewardEntry.Item` | 후보 `ULNPItemDefinitionBase` |
| `FLNPLootDiceRewardEntry.Weight` | 가중 추첨 비중. **0 이하면 후보에서 제외** — 코드 수정 없이 임시로 막는 스위치 |
| `FLNPLootDiceRewardSet.Entries` | 후보 전체 목록 |
| `FLNPLootDiceRewardSet.MinDrops` / `MaxDrops` | 1회 Pop당 스폰 개수 범위 (기본 3~4). `MaxDrops < MinDrops`면 `MinDrops`로 올림 |

**PodID 발급:** `ULNPLootPodTrait`는 **템플릿**을 만들어 모든 Pod가 공유하므로 트레잇에서 개별 ID를 줄 수 없다. 그래서 `ULNPMassSpawnSubsystem::SetupSpawnedEntities`가 스폰된 엔티티에 `FLNPLootPodFragment`가 있으면 `NextPodID`(1부터)를 하나씩 발급한다. **0은 미발급**을 뜻한다. 보상 조회가 서버 전용이라 복제하지 않는다. `RewardsByPodID`를 비워 두면 모든 Pod가 `DefaultRewards`로 폴백하므로, Pod별 차등 보상이 필요할 때만 채우면 된다.

`SpawnPodRewards`는 유효 후보(`Item != nullptr && Weight > 0`)의 가중치 합을 구한 뒤 `MinDrops`~`MaxDrops`회 반복하며 `FRandRange(0, TotalWeight)` 누적 감산으로 하나씩 뽑는다. **복원 추출(중복 허용)** — 매 뽑기가 독립이며, 마지막 후보는 부동소수 오차로 잔량이 남아도 `Picked` 폴백으로 반드시 선택된다. 추첨은 서버에서만 수행되므로(Dice는 스폰 번치로 복제) 클라이언트와 시드를 맞출 필요가 없다.

---

## 4. 미확정 항목

- 소멸 제한 시간(60초)·물리 튜닝값(각속도·Restitution·Damping)의 체감 밸런스 — 값 자체는 코드/에셋에 확정돼 있고 조정만 남았다.
- 획득 연출(GameplayCue) 미구현 (§2.6).
