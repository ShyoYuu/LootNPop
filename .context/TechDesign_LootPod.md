# LootPod 시스템 기술 설계

## 1. 한눈에 보기

**LootPod**은 MassEntity(시뮬레이션) + Actor(비주얼·상호작용·복제) 하이브리드다. 게이지·근접 판정은 서버 Mass 프로세서가 태그 교체 상태 머신으로 처리하고, Actor는 비주얼·상호작용 타겟·근접 클라이언트 복제를 담당한다.

```
[ULNPMassSpawnSubsystem]             → 결정론적 스폰 (SurfaceCache 표면 투영) + PodID 발급
[ULNPInteractableRegistrySubsystem]  → 살아 있는 인터랙터블(Pod·Dice) 레지스트리
[Mass Processors]                    → 상태 전환·게이지 (서버 전용, 태그 교체 상태 머신)
[ALNPLootPod Actor]                  → 레지스트리 등록 + Niagara·존 링 비주얼 + 상태·게이지 복제
```

> 상호작용 탐색에 쓰던 SmartObject 공간 쿼리는 Mass 액터 풀링과의 구조적 충돌로 폐기 (→ [DiscardedApproaches.md](DiscardedApproaches.md) Case 03).

**상태 머신 (태그 교체):**

```
FLNPLootPodIdleTag ──(활성화 요청 Tag 감지 — Interaction Input)──▶ FLNPLootPodLootingTag
        ▲                                                  │ 존 활성: 범위 내 모든 플레이어(프레즌스)의 속도 합산으로 게이지 증가,
        │                                                  │ 전원 이탈 시 감쇠 — 존은 활성 유지, 복귀만으로 재개
        └──(감쇠 끝에 게이지 0 — 완전 취소)◀────────────────┤
                                            (게이지 완료)──▶ Popped → 축하 VFX + LootDice 스폰 → 엔티티 소멸
```

---

## 2. MassEntity 구성 요소

### 2.1 Fragments & Tags

| 타입 | 내용 |
|:---|:---|
| `FLNPLootPodFragment` | `State`(Idle/Looting/Popped), `CurrentGauge`/`MaxGauge`, `LootableDistSquared`(루팅 존 반경²), `PodID`(보상 조회 키, 0 = 미발급) |
| `FLNPPlayerLootingFragment` | Player 엔티티 상주 (최초 상호작용 시 부착) — `BuffedLootSpeed` (없으면 1.0 취급) |
| Tags | `FLNPLootPodTag`(식별), `FLNPLootPodIdleTag`/`FLNPLootPodLootingTag`(상태별 쿼리 분리), `FLNPPlayerLootingTag`(**1회성 존 활성화 요청** — §4.6) |
| `LNPLootPodGaugeDecayFractionPerSecond` | 게이지 감쇠 속도 constexpr (MaxGauge 대비 초당 비율, 0.15) — 1.0 이상이면 사실상 즉시 초기화 룰 |

> `Interrupted` Enum 값은 **2026-07-09 기획 개정으로 "피격 즉시 취소"가 폐기**되면서 제거됐다 (넉백 피탈은 기존 범위 체크가 자동 감지 — [GameDesign_LootPod.md](GameDesign_LootPod.md) §2.2).

`ULNPLootPodTrait`가 엔티티 템플릿을 구성하며 MassReplication Trait(BubbleInfo/Replicator 고정)를 내부 위임한다. Pod 종류별로 EntityConfig에서 조정하는 트레잇 프로퍼티는 3개다:

| 프로퍼티 | 기본값 | 비고 |
|:---|---:|:---|
| `LootingZoneRadius` | 500cm | 게이지 기여·사수 판정 반경. Actor(BP) `LootingZoneSphere` 반경과 일치시켜야 한다 (존 링 스케일이 여기서 파생) |
| `MaxGauge` | 10 | 루팅 속도 합산 1.0 기준 채우는 데 걸리는 초. **원본값은 100이고 10은 플레이 테스트 편의로 낮춰 둔 값** — 밸런스 확정 시 100 기준으로 되돌린 뒤 조정 |
| `ReplicationCullDistance` | 60,000cm | 월드 전체(반지름 25,000 × 2)를 덮는다. Pod은 스폰 후 갱신 트래픽이 0이라 거리 컬링 Add/Remove 반복보다 싸다. 같은 EntityConfig의 `VisibleLODDistance[Off]`와 짝을 맞출 것 (§5.4) |

**반경 2종 분리:** 상호작용(루팅 State 진입)은 Actor의 `InteractionRadius`(250cm, 단말기 조작 컨셉) + `MaxInteractionAngle`(60°) — 게이지 기여는 위의 존 반경 500cm. 존 안에 있어도 State 진입은 단말기 앞에서만 가능하다.

### 2.2 Processors (서버 전용 — `IsClientWorld` 가드)

복제로 클라이언트에도 같은 아키타입이 존재하므로, 두 프로세서 모두 `ExecutionFlags = AllNetModes`로 등록하되 `Execute` 첫 줄에서 클라이언트 월드를 걸러낸다.

**`ULNPIdleToLootingProcessor`** (Movement 그룹 이후) — Idle Pod **루팅 존 반경** 안에서 활성화 요청 Tag(`FLNPPlayerLootingTag`) 보유자를 감지 → `Looting` 전환 + Pod 태그 교체 (Deferred). 실행 말미에 **요청 Tag를 일괄 소비**한다 (1회성 — §4.6). 초근접·각도 게이트는 Actor 쪽 `CanInteract`가 이미 통과시킨 뒤라 여기서 다시 보지 않는다.

**`ULNPLootingProcessor`** — Looting Pod마다:
- **범위 내 모든 플레이어(`FLNPPlayerTag` 프레즌스)의 `BuffedLootSpeed`를 합산**해 게이지 누적 — 입력 없이 존에 들어오기만 해도 기여하고, 여럿이면 그만큼 빨라진다 (`FLNPPlayerLootingFragment`는 Optional, 없으면 1.0).
- 게이지 진행률을 Actor 복제 프로퍼티에 반영 (게임 스레드 지연 커맨드, 2% 임계값은 `SetGaugePercent`가 처리) — 증가·감쇠 공통 경로이며 재스폰 자기치유도 여기 실린다 (§5.6).
- 완료 → `Popped` 전환 커맨드 + **엔티티 즉시 파괴** (Fragment/Tag 갱신 불필요 — 전파는 커맨드가 담당).
- **전원 이탈 → 게이지 감쇠 (존 활성 유지):** `MaxGauge × LNPLootPodGaugeDecayFractionPerSecond`/초 — Looting 태그를 유지하므로 누구든 복귀·합류만으로 재개된다.
- **감쇠 끝에 게이지 0 → 완전 취소:** `Idle` 복귀 + 태그 교체 — 재활성화는 Interaction Input부터.

**`FLNPPodStateTransitionCommand`** (Game Thread) — 상태 전환 통합 후처리: `UpdateVisuals` 호출, 로그, `Popped` 시 `ALNPLootDice::SpawnPodRewards(PodID, Location)` + Confetti 스폰 (Pod Actor는 이미 파괴됐을 수 있어 Entry의 PodID·Location만 쓴다).

---

## 3. 상호작용 흐름

### 3.1 플레이어 측 (ULNPInteractionComponent)

`ULNPInteractableRegistrySubsystem` 레지스트리(Pod·Dice Actor가 BeginPlay/EndPlay에 자기 등록/해제)를 매 Tick 순회해 컴포넌트 탐색 반경(500cm) 내 후보를 수집한다. High LOD Actor는 항상 소수(근접 시에만 스폰)라 순회 비용은 무시할 수준이며, 풀에 반납된(Hidden) 액터는 제외한다. **탐색 반경과 판정 반경은 별개다** — 후보에 들어와도 실제 상호작용은 대상의 `CanInteract`(Pod 250cm+60°, Dice 250cm)를 통과해야 한다.

**상호작용 프롬프트:** 후보 중 **가장 가까운 Idle Pod 또는 Dice** 머리 위에 키 아이콘(스크린 스페이스 WidgetComponent)을 표시한다. 판정은 로컬 플레이어 전용(`IsLocallyControlled`) — 복제 무관. Looting 중인 Pod는 프레즌스 기여라 입력이 불필요하므로 프롬프트를 띄우지 않는다.

⚠️ **입력은 프롬프트가 떠 있는 단 하나의 대상에만 간다** (`CurrentPromptTarget`). 후보 전체를 순회하던 초기 구현은 F 한 번에 주변 Dice가 몽땅 습득됐다.

```
상호작용 입력 → 로컬 CanInteract + Pod->StartLooting() (비주얼 예측)
  ├─ 서버/리슨호스트: 즉시 StartLootingOnServer()
  └─ 원격 클라이언트: Server_StartLooting RPC → 서버가 CanInteract 재검증
       → 서버 월드의 플레이어 엔티티에 활성화 요청 Tag 부여 (+ 최초 1회 FLNPPlayerLootingFragment)
       → 다음 프레임 ULNPIdleToLootingProcessor가 감지 후 Tag 소비
```

### 3.2 ALNPLootPod Actor — 비주얼과 복제

| 컴포넌트 | 역할 |
|:---|:---|
| `UNiagaraComponent` | 빛기둥. `User.Color` 파라미터를 상태별 색(Idle/Looting/Popped)으로 전환 |
| `UStaticMeshComponent`(ZoneRing) | 루팅 존 홀로그램 링. BeginPlay에서 존 반경에 맞춰 스케일을 잡고 `M_LootZoneRing`의 MID를 만들어 `GaugePercent`를 매 Tick 밀어 넣는다. **Looting 동안만 표시**(감쇠 중 포함) |
| `USphereComponent` | 루팅 존 반경의 단일 출처 (링 스케일이 여기서 파생) |
| `UMassAgentComponent` | Mass 브릿지 |
| `UWidgetComponent` | 상호작용 프롬프트, 기본 숨김 |
| `SmartObjectComponent` | **상호작용 경로 미사용** — NPC AI 연동 후보로 보류 |

**프롬프트 위젯**(`ULNPInteractionPromptWidget`, LootDice와 공용)은 **현재 입력 타입에 실제로 바인딩된 키**를 텍스트 심볼로 그린다 — 키보드 `F`, 게임패드 `□`. `IA_Interaction`의 Enhanced Input 매핑에서 뽑으므로 리매핑을 자동으로 따라간다 (해석기 `LNPInputGlyph` → [TechDesign_InGameMenu.md](TechDesign_InGameMenu.md) §3.4). `EWidgetSpace::Screen`이라 `UWidgetComponent`가 표시/숨김마다 `NativeConstruct`/`NativeDestruct`를 돌려주므로 거기서 글리프를 갱신하고, 표시 중 기기 전환은 `OnInputMethodChangedNative` 구독으로 처리한다.
⚠️ 글리프 해석이 빈 문자열이면 **지우지 말고 직전 값을 유지**한다 — 메뉴가 열린 동안은 폰의 `IMC_Pawn`이 통째로 제거돼 무효 키가 나온다.

**이중 복제:** 엔티티 존재·초기 위치는 MassReplication bubble이 전 클라이언트에, `CurrentState`(OnRep → `UpdateVisuals`)·`CurrentGaugePercent`는 Actor 복제가 근접 클라이언트(NetCull 20,000cm, 갱신 10Hz)에 전달한다. (→ [TechDesign_Networking.md](TechDesign_Networking.md) §3.5)

---

## 4. 어필 포인트 (설계 판단)

### 4.1 태그 교체 상태 머신 — 쿼리가 곧 상태 필터

상태를 Fragment 값으로만 두면 모든 프로세서가 전체 Pod를 순회하며 분기해야 한다. 상태별 태그로 아키타입을 분리하면 각 쿼리가 자기 상태의 엔티티만 받아 상태 검사 비용이 쿼리 필터로 흡수된다. LootPod은 전환 빈도가 낮아 아키타입 마이그레이션 비용이 문제되지 않는 케이스다 (전환이 잦은 Enemy 넉백은 반대 판단 — [TechDesign_EnemyNPC.md §7.3](TechDesign_EnemyNPC.md)).

### 4.2 게이지 복제의 트래픽 제어

매 프레임 변하는 게이지를 그대로 복제하면 Pod 수 × 프레임만큼 트래픽이 나간다. **2% 이상 변화(또는 0/1 도달) 시에만** 복제 프로퍼티에 기록해, 시각적 부드러움은 유지하면서 대역폭을 상수화한다.

### 4.3 원격 클라이언트 루팅 공백 — 서버 엔티티에 태그를 붙여라

루팅 태그를 로컬 월드의 플레이어 엔티티에만 붙이던 초기 구현은 리슨 호스트에서만 동작했다. LootPod 프로세서가 서버 전용이라 **서버 월드의 엔티티**에 태그가 있어야 감지하기 때문이다. `Server_StartLooting` RPC로 해소 — Guard/Parry RPC와 같은 유형의 공백이다(클라이언트 로컬 Mass 상태 변경은 서버에 자동 전파되지 않는다).

### 4.4 다인 협동 루팅

범위 내 모든 루터의 속도를 합산하는 설계라 "함께 루팅하면 빨리 깐다"는 협동 인센티브가 프로세서 루프 한 줄로 구현된다. 루팅 속도 버프도 `BuffedLootSpeed` 필드 하나로 어떤 아이템/스킬이든 연동된다.

### 4.5 취소 조건의 일원화 — 피격 취소를 만들지 않는 이유

"피격 시 즉시 취소"를 구현하려면 HitDetection → LootPod 브릿지가 필요했다. 기획 개정(존 사수 게임플레이)으로 취소 조건이 **"감쇠 끝에 게이지 0"** 하나가 되면서 넉백 피탈은 기존 거리 체크가 자동으로 잡는다 — 시스템 간 결합 하나가 통째로 사라진 케이스.

### 4.6 입력=활성화, 기여=프레즌스 — 1회성 요청 태그

초기 구현은 `FLNPPlayerLootingTag`를 "루팅 중" 상태 표시로 쓰면서 제거 경로가 없었다 — 한 번 루팅한 플레이어가 **영구 루터**가 되어 Idle Pod 옆을 지나가기만 해도 재활성화되는 잠재 버그. 기획 확정을 계기로 역할을 분리했다:

| 역할 | 수단 |
|:---|:---|
| 존 활성화 | 요청 Tag — Interaction Input 시 부여, `ULNPIdleToLootingProcessor`가 처리 후 즉시 소비 (1회성) |
| 루팅 기여 | 프레즌스 — 활성 존 범위 안의 모든 `FLNPPlayerTag` 플레이어 (입력 없이 합류·복귀 가능) |

"누구든 존에 들어오기만 하면 기여한다"는 협동 기획이 쿼리 요구 조건 변경만으로 구현되고, 태그 수명 관리 문제도 함께 소멸했다.

---

## 5. 기능별 구현 현황

### 5.1 게이지 감쇠·완전 취소·프레즌스 기여 — ✅ 완료 (2026-07-10)

구현은 §2.2 `ULNPLootingProcessor`, §4.6 참조. 감쇠 속도는 `LNPLootPodGaugeDecayFractionPerSecond`(`LNPLootPodMassTypes.h`) 하나로 제어한다.

### 5.2 루팅 속도 스탯·버프 연동 — ✅ 완료 (2026-07-10)

`ULNPBaseAttributeSet::LootSpeed` (기본 1.0, 복제, 0.01 하한). Fragment 동기화는 2경로:

| 경로 | 시점 | 커버 |
|:---|:---|:---|
| **Attribute 변경 델리게이트** (`ALNPPlayerCharacter::PushLootSpeedToEntity`, `PossessedBy`에서 바인딩 — 서버 전용) | 버프 GE 적용/만료 즉시 | 루팅 도중 버프 변동, **상호작용 이력 없는 파티원의 버프**까지 (Fragment 없으면 생성) |
| **상호작용 시점 캐싱** (`StartLootingOnServer`) | Fragment 최초 부착 시 | 델리게이트 바인딩 전 이력·기본값 |

버프 아이템/GE는 `LootSpeed`만 변조하면 자동 연동된다. 검증 완료 (2026-07-27) — `GE_Buff_LootSpeed`(Infinite·AddBase +1.0) + `DA_Buff_LootSpeed`로 게이지 가속 확인 (→ [GameDesign_Ability.md](GameDesign_Ability.md) §3.3).

### 5.3 Popped 후처리 — 보상 스폰·축하 VFX — ✅ 완료 (2026-07-14)

`FLNPPodStateTransitionCommand::Run()`의 `Popped` 블록에서:
1. `LNPSettings.LootDiceRewardTable`에서 `PodID` 보상 조회(미등록 시 `DefaultRewards`) → 후보 풀 가중 추첨으로 `MinDrops`~`MaxDrops`개(3~4) 선정 (→ [TechDesign_LootDice.md](TechDesign_LootDice.md) §3.1).
2. `ALNPLootDice` N개 스폰 + Pop 임펄스·고속 회전 (→ [TechDesign_LootDice.md](TechDesign_LootDice.md) §2.8).
3. `LNPSettings.LootPodConfettiVFX`(`NS_LootPodConfetti`)를 `SpawnSystemAtLocation(Entry.Location)`으로 원샷 재생.

⚠️ **연출도 보상처럼 위치 기반이어야 한다.** 처음엔 Confetti를 `ALNPLootPod::UpdateVisuals`(Actor)에서 스폰했으나, Pop 전환이 같은 배치의 `DestroyEntity`로 표현 Actor를 파괴해 `Pod != nullptr` 가드가 실패 → **스폰이 전혀 도달하지 않았다.** 구 `BP_LNPLootPod.PopConfettiVFX` 경로는 제거됐다.

⚠️ **MP 잔여:** 서버 스폰 Niagara는 원격 클라이언트에 복제되지 않아 리슨 호스트/싱글에서만 보인다 — NetMulticast는 네트워킹 후순위.

### 5.4 빛기둥 Low LOD 표시 — ✅ 완료 (2026-07-12)

빛기둥은 "멀리서도 보인다"가 스펙이라 Actor(High LOD) 유무와 무관하게 상시 표시돼야 한다. 존재·위치는 이미 MassReplication bubble이 전 클라이언트에 전달 중이므로 시각화만 추가했다. 대안이던 "단일 Niagara + 위치 배열"은 폐기 — ISM 안이 기존 Low LOD 인프라를 재사용해 **코드 0줄**로 충족한다.

- **ISMC 에미시브 빔 메시** — `MassCrowdVisualizationTrait`의 `StaticMeshInstanceDesc`에 에미시브 실린더(스케일 (0.4,0.4,30) ≈ 지름 40cm·길이 30m, +1500cm Up 오프셋)를 두 번째 ISM 메시로 추가. 머티리얼은 `M_LootPillar_LowLOD`(Unlit·Additive·양면) — ⚠️ **`bUsedWithInstancedStaticMeshes` 필수**, 누락 시 기본 회색으로 대체된다.
- **LOD 표현:** `LODRepresentation` 마지막 항목을 `None` → `StaticMeshInstance`로 바꿔 최원거리(LOD3)에서도 빔이 보이게 했다. 빔은 LOD2-3(ISM), LOD0-1(Actor)은 BP 컴포넌트 `PillarBeam`(같은 실린더·스케일·오프셋·머티리얼)이 그린다. **밴드가 겹치지 않아 이중 표시가 구조적으로 방지된다.** `LootPillarVFX`(Niagara)는 상태 색 파라미터 경로만 남아 있고 BP의 Asset은 비어 있다.
- ⚠️ **`PillarBeam`은 `NoCollision`이어야 한다.** 엔진 기본값(BlockAllDynamic)이 남아 있어 승격 Actor에 보이지 않는 30m 기둥 충돌이 있었다(2026-09-24 수정). Pod 충돌은 collision proxy가 소유한다([SurfaceSupportNavigation/design/TerrainContract.md](SurfaceSupportNavigation/design/TerrainContract.md) §2, D-047).
- **소멸:** Pod Popped → bubble 엔티티 제거 → ISM 인스턴스 자동 제거.

**클라이언트 가시 거리 문제 — 원인 2건 (둘 다 해결):**

| 증상 | 원인 | 수정 |
|:---|:---|:---|
| 클라이언트에서 근접해야 빔이 보임 (2026-08-05) | 시각화가 아니라 **복제** — `LODDistance[Off]` 엔진 기본값 5,000cm가 반지름 25,000cm 월드엔 너무 좁아 엔티티 자체가 클라에 도달하지 못했다. 서버는 로컬이라 시각화 거리까지 다 보여 증상이 한쪽에만 나타났다 | `ULNPLootPodTrait::ReplicationCullDistance`로 `VisibleLODDistance[Off]`와 짝을 맞춤. 적도 부근에서 빔이 월드 Z로 누워 있던 문제도 같이 해소 |
| `-game` 리슨 서버에서만 재발 (2026-08-19) | **템플릿 빌드 시점** — `-game` 리슨 서버는 월드 서브시스템 `Initialize()` 시점의 `GetNetMode()`가 `NM_Standalone`이라 `UMassReplicationTrait::BuildTemplate`이 조기 반환하고, 복제 프래그먼트가 빠진 템플릿이 월드 수명 내내 캐시됐다 | `ULNPMassSpawnSubsystem`의 템플릿 warm-up을 `OnWorldBeginPlay()`로 이동 |

⚠️ PIE는 두 번째 함정을 구조적으로 재현하지 못한다 — **Mass 가시성 검증은 반드시 `-game` 2프로세스로도** 할 것 ([TechDesign_Networking.md](TechDesign_Networking.md) §3.5, [EngineAnalysis_MassReplication.md](EngineAnalysis_MassReplication.md) §7.9).

### 5.5 난이도 스케일링 트리거 — ⏸ 후순위 (2026-07-12)

활성 Pod 수 카운터 → `ULNPTargetingSubsystem` 슬롯 한도/NPC 능력치 조정. 난이도 조절이 의미를 갖는 게임플레이가 완성된 뒤 착수한다. (Pod 리스폰은 스펙 제외 — [Idea_Backlog.md](Idea_Backlog.md))

### 5.6 Actor (재)스폰 시 엔티티 상태 동기화 — ✅ 완료 (2026-07-10)

Representation이 Actor를 스폰할 때 엔티티 Transform만 동기화되고 `CurrentState`/`CurrentGaugePercent`는 기본값(Idle/0%)으로 시작한다. 실제 버그: 존을 들락거리다 Actor가 LOD 소멸→재스폰되면 Idle로 보이는 Pod에 F 프롬프트가 뜨는데 엔티티는 Looting이라, 프레즌스 기여로 몰래 차올라 "갑자기 Pop"했다. 해결은 2중이다.

1. **자기치유** — `ULNPLootingProcessor`의 게이지 동기화 커맨드(게임 스레드)에서 Actor 상태가 Looting이 아니면 `UpdateVisuals(Looting)`을 밀어 넣어 한 프레임 안에 복원한다. Idle/Popped는 기본값·소멸이 자연 일치라 별도 처리가 없다.
2. **High LOD 범위 확장** — LODParams를 High ~25m(base 2500/visible 3000)로 확장. 기존 ~5-10m는 존 반경 5m와 겹쳐 존 이탈만으로 Actor가 소멸했다. `LODMaxCount` High도 64로 상향.

### 5.7 잔여 확인

- 2인 PIE 풀 플로우(적 처치 → 상호작용 → 게이지 완료 → Pop → Dice 획득)는 2026-07-17 검증했다. 다만 그때는 호스트 화면만 봤고, 게스트 쪽 Pop 후 엔티티·빔 소멸은 리슨 서버 파괴 옵저버 복구(2026-08-20, [EngineAnalysis_MassReplication.md](EngineAnalysis_MassReplication.md) §7.10) 이후 재확인이 필요하다.
- 감쇠 세부 규칙(0 도달 후 존 재진입만으로는 미재개, 감쇠 중 복귀 시 잔여분 재개)의 2인 실측.
