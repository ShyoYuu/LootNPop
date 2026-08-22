# LootDice 시스템 기술 설계

> **상태: 코드 구현 완료 (2026-07-12) · 에디터 에셋 제작 완료 (`/Game/LootDice/` — BP_LootDice·M_LootDice·PM_LootDice·DA_LootDiceRewardTable, LNP Settings 지정).**
> **PIE 1인 검증 완료:** 디버그 스폰 → 굴림·정지, 60초 정각 소멸, F 획득 → 인벤토리 편입(DA_Pistol) 확인. **잔여 검증:** PIE 2인 복제(정지 윗면 일치·슬립 트래픽), 구형 월드 중력, 드랍/양도, Pod 루팅 완료 → 보상 Pop. 기획은 [GameDesign_LootDice.md](GameDesign_LootDice.md) 참조.
> 구현 파일: `Source/LootNPop/LootDice/LNPLootDice.h/.cpp`, `LNPLootDiceRewardTable.h`, `Source/LootNPop/Interaction/LNPInteractableRegistrySubsystem.h/.cpp` (구 `LNPLootPodSubsystem` 일반화 대체).

## 1. 한눈에 보기

**LootDice**는 서버 권위 물리 시뮬레이션 + Iris 표준 Actor 복제로 구현하는 픽업 Actor. LootPod과 달리 **Mass 엔티티를 쓰지 않는다** — 동시 존재 수가 적고(Pod당 수 개 × 제한 수명), 리지드바디 물리와 페이로드 복제가 필요해 순수 Actor가 적합하다.

```
[ALNPLootPod Popped]   ──┐
[인벤토리 드랍 RPC]    ──┤
[플레이어 사망 전량 드랍] ─┴─▶ [서버: ALNPLootDice 스폰 + Pop 임펄스·고속 회전]
                             ├─ 물리: 서버 시뮬 + ReplicatedMovement (Iris 네이티브, 각속도 포함)
                             ├─ 획득: SmartObject + ULNPInteractionComponent → Server RPC → 인벤토리
                             └─ 소멸: 서버 타이머 → Destroy 복제
```

---

## 2. 설계

### 2.1 왜 Mass가 아니라 순수 Actor인가

Enemy(수백 규모)·발사체(고빈도 생성)와 달리 LootDice는 소수·단명이다. Chaos 리지드바디, SmartObject 등록, 페이로드 프로퍼티 복제 전부 Actor 인프라가 그대로 제공하므로, Mass로 얻을 스케일 이득이 없고 브릿지 비용만 생긴다.

### 2.2 클래스 구성 — `ALNPLootDice`

| 요소 | 내용 |
|:---|:---|
| `UStaticMeshComponent` | 큐브, `SimulatePhysics` — 시뮬 권위는 서버 |
| 아이콘 표시 | **6면 모두 아이콘 텍스처** + 카테고리 색 에미시브 머티리얼(MID) — 정지 후 어느 면이 위든 식별 가능, 빌보드/위젯 불필요 |
| 상호작용 등록 | LootPod과 동일한 레지스트리 체계(BeginPlay/EndPlay 자기 등록) — LootPod 전용 레지스트리를 범용 인터랙터블 레지스트리로 일반화해 재사용. SmartObject는 폐기 (→ [DiscardedApproaches.md](DiscardedApproaches.md) Case 03) |
| 페이로드 (초기 1회 복제, `COND_InitialOnly`) | `ItemDef`(`ULNPItemDefinitionBase` 에셋 참조), `RemainingDuration`(버프 잔여 시간, 신품은 0=풀), `ItemLevel`(무기 강화 레벨, 신품은 1), `SpawnServerTime`(소멸 경고 연출 계산용) |

> **`ItemLevel`(2026-08-20)** — 합성으로 올린 무기 레벨이 드랍→재획득에서 소실되지 않게 한다.
> 획득 처리(`PickupDiceOnServer`)는 서버가 자기 값을 읽어 `AddItemInstance(Def, Level)`로 넘기므로,
> **드랍한 사람이 아니라 줍는 사람이 누구든** 레벨이 그대로 이전된다 (멀티플레이 양도 규칙).
> 버프의 `RemainingDuration`과 달리 월드에 놓인 동안 변하지 않으므로 동결·역산 규칙이 필요 없다.

### 2.3 물리 동기화 — Iris + FRepMovement

**엔진 확인 (UE 5.8):** Iris에 별도의 "물리 동기화 기능"이 있는 것이 아니라, 표준 물리 복제 경로인 `AActor::ReplicatedMovement`(`FRepMovement`)가 Iris 전용 NetSerializer(`Engine/.../RepMovementNetSerializer.h/.cpp`)로 **네이티브 지원**된다. 서버가 물리 시뮬 → `GatherCurrentMovement`가 위치·회전·선속도·**각속도**를 기록 → 클라이언트 물리 복제가 로컬 시뮬에 보정 블렌딩 — 주사위 굴림이 회전까지 포함해 동기화된다.

**대역폭 제어 3중 장치:**

1. `NetUpdateFrequency` 20 안팎 — 바운스 없는 낮은 반발이라 굴림 활성 구간이 수 초로 짧다.
2. **물리 슬립 후 트래픽 자연 소멸** — 정지하면 `ReplicatedMovement` 델타가 없어 복제가 멈춘다.
3. **제한 수명** — 미획득 시 소멸이 최악 케이스 대역폭의 상한.

Chaos Networked Physics(리심 기반 예측)는 과잉 — 굴림은 게임플레이 판정이 없는 코스메틱이라 서버 권위 스냅샷 + 보정으로 충분하다.

**발사체(Spawn-Only 방송)와 판단이 갈리는 지점:** 발사체는 "빠르고 많다"라서 지속 복제가 부적합했지만([TechDesign_Networking.md](TechDesign_Networking.md) §3.3), LootDice는 "느리고 적고 **정지 위치·윗면이 클라이언트 간 일치해야 한다**" — 파티원이 가리키며 분배 논의하는 대상이므로 로컬 독립 시뮬은 부적합, 서버 권위 복제가 옳다.

### 2.4 구형 중력 — AddForce 방식의 타당성 (엔진 소스 확인)

**"AddForce만으로 자연스러운 중력이 되는가" → 된다.** 엔진 내장 중력 자체가 Chaos 솔버의 `PerParticleGravity`가 매 서브스텝 `Particle.Acceleration() += 중력벡터` 를 더하는 **상수 가속도일 뿐**이다 (`Chaos/PerParticleGravity.h` 확인). `bEnableGravity=false`로 내장 -Z를 끄고 매 Tick `AddForce(GravityDir × 중력가속도, bAccelChange=true)`를 주면 내장 중력과 **동일한 수학**으로 적분된다 — "자연스러움"의 차이가 원리적으로 없다. 중력 방향은 게임 스레드에서 Tick당 1회 갱신되지만, 행성 곡률 대비 한 Tick의 이동 거리가 미미해 오차는 무시 가능.

- 방향 산출은 기존 캐릭터 구형 중력의 산출 로직 재사용.
- 물리 권위는 서버지만, 클라이언트 로컬 물리 블렌딩 품질을 위해 클라이언트에서도 동일한 힘을 적용.

**주의 — 슬립을 깨우지 않기:** 잠든 바디에 매 Tick 힘을 넣으면 영원히 잠들지 못해 §2.3의 "슬립 후 트래픽 소멸"이 무너진다. `IsAnyRigidBodyAwake()` 체크 후에만 AddForce (AddForce의 wake 동작은 구현 시 확인).

**대안 검토 — Chaos Gravity Group (엔진 확인):** Chaos는 최대 8개 gravity group별 **가속도 벡터**를 지원한다 (`FBodyInstance::SetGravityGroupIndex`, `PerParticleGravity::MAccelerations[8]`). 그룹당 방향이 고정이라 위치에 따라 연속으로 변하는 구면 방사 중력에는 부적합 — 다만 고정 방향으로 근사 가능한 국소 구역이 생기면 재검토 가치 있음.

### 2.5 Dice 물리 튜닝 — "굴리고, 멈추고, 공개한다"

| 항목 | 값/방향 |
|:---|:---|
| 스폰 각속도 | **높게** (`SetPhysicsAngularVelocityInRadians`) — 공중에서 아이콘 인지 불가 |
| Restitution | **낮게** (Physical Material) — 통통 튀지 않고 데구루루 구름 |
| Friction / Angular Damping | 착지 후 수 초 내 정지하도록 튜닝 — 회전이 잦아들며 보상 공개 |

공개 타이밍(착지→정지)이 곧 연출이므로, 튜닝 파라미터는 Physical Material + 스폰 상수로 모아 반복 조정 가능하게 한다.

### 2.6 획득 흐름

```
ULNPInteractionComponent가 인터랙터블 레지스트리 순회로 후보 탐색 (LootPod과 동일 파이프라인)
→ Interaction Input → Server_PickupDice RPC
→ 서버 검증: Dice 유효(선착순 레이스는 서버 직렬화가 자연 해결) + 거리 재검증
→ ItemDef 유형별 인벤토리 편입: AddToStorage() / AddBuffItem(RemainingDuration)
→ Dice Destroy (복제로 전 클라이언트 제거) + 획득 연출 (GameplayCue)
```

### 2.7 인벤토리 드랍 흐름 (양도)

```
인벤토리 UI에서 미장착 아이템 드랍 → DropItem(FGuid ItemId) → Server_DropItem(FGuid) RPC
→ ItemId로 인스턴스 조회(가방→버프 순) + IsEquipped() 가드
   ([TechDesign_Inventory.md](TechDesign_Inventory.md) 인스턴스 모델 — 사본을 ItemId로 정확 식별)
→ 제거 전에 무기 레벨을 읽어 둠 (제거하면 인스턴스가 사라진다)
→ RemoveItemInstance() / RemoveBuffInstance() — 버프는 잔여 시간을 반환받아 페이로드에 실음
→ 캐릭터 전방에 ALNPLootDice 스폰 + 작은 Pop 임펄스·회전
→ 이후 소멸·획득 규칙은 LootPod 보상과 완전 동일 — 스폰 경로를 공용 함수로 묶는다
```

### 2.7.1 사망 전량 드랍 (2026-08-21)

플레이어가 죽으면 **가방 전체(장착 무기 포함) + 활성 버프 전부**가 사망 지점에서 한꺼번에 Pop 한다.
`ALNPPlayerCharacter::DropAllItemsOnDeath()` (서버 전용) →
`RemoveAndSpawnDice()` (§2.7의 드랍과 공유하는 제거·스폰 공용 경로) → `SpawnDice(..., ImpulseScale=1.0)`.

- **장착 해제가 먼저다.** `ULNPEquipmentComponent::UnequipWeapon()`을 한 번 불러 슬롯을 비운다 —
  장착본이 `bEquipped`인 채로 제거되면 `WeaponSlot.SourceInstance`가 댕글링이 되고 무기 GAS 부여가 회수되지 않는다.
  (UI 드랍 경로의 `IsEquipped()` 거부 가드는 그대로 유지된다 — 규칙이 바뀌는 건 사망뿐이다.)
- **ItemId를 먼저 스냅샷한다.** 제거 루프가 FastArray와 등록 서브오브젝트를 건드리므로 인스턴스 포인터를
  들고 순회하면 안 된다.
- 스폰 위치는 사망 지점 한 곳이면 충분하다 — §2.8의 원뿔 랜덤 임펄스가 자연히 흩뿌린다
  (`SpawnPodRewards`와 같은 전략).
- 리스폰이 10초, Dice 수명이 60초라 본인·파티원 모두 회수할 시간이 있다.

상세 흐름은 [TechDesign_CharacterMovement.md](TechDesign_CharacterMovement.md) §9 참조.

### 2.8 Pop 임펄스

서버가 스폰 시 표면 Up 기준 원뿔 내 랜덤 방향 임펄스 + 랜덤 축 고속 회전을 1회 부여 — 다중 스폰 시 자연스럽게 흩어진다. 임펄스·회전은 복제할 필요 없음 (`ReplicatedMovement`가 결과 궤적을 나른다).

### 2.9 소멸

서버 타이머(가칭 60초) → `Destroy()`. 마지막 N초 깜빡임 경고는 클라이언트 로컬 처리 (`SpawnServerTime` + 수명으로 계산 — 추가 복제 불필요). 깜빡임 속도는 `BlinkPeriod` UPROPERTY(1주기 초, 기본 0.2 — 작을수록 빠름)로 BP CDO에서 조절.

---

## 3. 작업 계획 — 코드 전 단계 구현 완료 (2026-07-12), 검증 열은 잔여 확인 항목

| 단계 | 내용 | 검증 (잔여) |
|:---|:---|:---|
| 1 ✅ | `ALNPLootDice` 뼈대: 큐브 메시+물리+페이로드+수명, 디버그 스폰 커맨드 `LNP.Debug.SpawnLootDice [개수] [ItemDef 경로]` | 로컬에서 회전하며 날아가 구르다 정지, 제한 시간 후 소멸 |
| 2 ✅ | 구형 중력 (§2.4) + 슬립 가드 (`IsAnyRigidBodyAwake` 체크 후에만 AddForce) | 내벽 표면에서 낙하 방향 정상, 정지 후 슬립 진입 (AddForce wake 동작 실측) |
| 3 ✅ | 복제: `bReplicates` + `SetReplicatingMovement(true)` + `COND_InitialOnly` 페이로드 (Deferred 스폰으로 스폰 번치 동봉 보장) | PIE 2인 — 클라이언트 정지 위치·윗면이 서버와 일치, 슬립 후 트래픽 중단 (net stat) |
| 4 ✅ | 레지스트리 일반화(`ULNPInteractableRegistrySubsystem`) + `Server_PickupDice` + 인벤토리 편입 (Buff→`AddBuffItem(잔여)`, 그 외→`AddToStorage`) | 원격 클라이언트 획득 → 인벤토리 반영, 동시 획득 시 1명만 성공, **LootPod 프롬프트·루팅 회귀 없음** |
| 5 ✅ | 드랍: `ALNPPlayerCharacter::DropItem(FGuid)`/`Server_DropItem` — 인스턴스 ItemId 조회 + `IsEquipped()` 가드 (제거 성공 전 스폰 금지; 2026-07-17 인스턴스 모델로 전환) | 버프 잔여 시간이 유지된 채 양도됨, 장착 아이템 드랍 거부 |
| 6 ✅ | LootPod Popped 연결: `ULNPLootDiceRewardTable` + `SpawnPodRewards` (축하 VFX는 에셋 잔여) | 루팅 완료 → Dice N개 Pop! (→ [TechDesign_LootPod.md](TechDesign_LootPod.md) §5.3) |
| 7 ✅ | 6면 아이콘 MID 훅(`IconTexture`/`CategoryColor` 파라미터)·카테고리 색·소멸 깜빡임 (머티리얼·아이콘 에셋 잔여) | 정지 후 카테고리 식별 가능, 공중 회전 중엔 비식별, 마지막 5초 깜빡임 |
| 8 ✅ | 보상 후보 풀 가중 추첨 (`FLNPLootDiceRewardEntry` + `MinDrops`/`MaxDrops`, 2026-07-27) | Pop마다 다른 3~4종 조합, 가중치 0 항목 미출현, 중복 출현 허용 |

**에디터 에셋:** BP_LootDice(큐브 메시 ~30cm), M_LootDice(`IconTexture` Texture Param + `CategoryColor` Vector Param 에미시브), PM_LootDice(Restitution ~0.05·높은 Friction·Angular Damping), DA_LootDiceRewardTable, LNP Settings 지정(`LootDiceClass`/`LootDiceRewardTable`), ItemDef 에셋 `Icon` 지정 — 모두 완료.

### 3.1 보상 테이블 구조 (2026-07-27)

`ULNPLootDiceRewardTable`은 **후보 풀**이지 드랍 목록이 아니다. 드랍 가능한 모든 보상을 등록해 두고 Pop 시점에 일부만 뽑는다
(→ [GameDesign_LootDice.md](GameDesign_LootDice.md) §2.5).

| 필드 | 역할 |
|:---|:---|
| `FLNPLootDiceRewardEntry.Item` | 후보 `ULNPItemDefinitionBase` |
| `FLNPLootDiceRewardEntry.Weight` | 가중 추첨 비중. **0 이하면 후보에서 제외** — 코드 수정 없이 임시로 막는 스위치 |
| `FLNPLootDiceRewardSet.Entries` | 후보 전체 목록 |
| `FLNPLootDiceRewardSet.MinDrops` / `MaxDrops` | 1회 Pop당 스폰 개수 범위 (기본 3~4). `MaxDrops < MinDrops`면 `MinDrops`로 올림 처리 |

**PodID 발급 (2026-07-27):** `ULNPLootPodTrait`는 **템플릿**을 만들어 모든 Pod가 공유하므로 트레잇에서 개별 ID를 줄 수 없다.
그래서 `ULNPMassSpawnSubsystem::SetupSpawnedEntities`가 스폰된 엔티티에 `FLNPLootPodFragment`가 있으면
`NextPodID`(1부터)를 하나씩 발급한다. **0은 미발급을 뜻한다.**
보상 조회가 서버 전용(`SpawnPodRewards` → `SpawnDice`는 클라에서 early-return)이라 **복제하지 않는다**.
`RewardsByPodID`를 비워 두면 모든 Pod가 `DefaultRewards`로 폴백하므로, Pod별 차등 보상이 필요할 때만 채우면 된다.

`SpawnPodRewards`는 유효 후보(`Item != nullptr && Weight > 0`)의 가중치 합을 구한 뒤,
`MinDrops`~`MaxDrops`회 반복하며 `FRandRange(0, TotalWeight)` 누적 감산으로 하나씩 뽑는다.
**복원 추출(중복 허용)** — 매 뽑기가 독립이며, 마지막 후보는 부동소수 오차로 잔량이 남아도
`Picked` 폴백으로 반드시 선택된다.

추첨은 **서버에서만** 수행된다 (`SpawnPodRewards`는 서버 전용, Dice는 스폰 번치로 복제) — 클라이언트와 시드를 맞출 필요가 없다.

---

## 4. 미확정 항목

- 소멸 제한 시간 밸런스 (가칭 60초).
- Dice 물리 튜닝값 (스폰 각속도·Restitution·Damping) — 공개 타이밍 체감 조정.
- 구형 중력 AddForce의 슬립/wake 동작 — 구현 시 확인 (§2.4 주의 사항).
