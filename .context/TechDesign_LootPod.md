# LootPod 시스템 기술 설계

## 1. 한눈에 보기

**LootPod**은 MassEntity(시뮬레이션) + Actor(비주얼·상호작용·복제)를 결합한 하이브리드 오브젝트. 게이지·근접 판정은 서버 Mass 프로세서가 태그 교체 상태 머신으로 처리하고, Actor는 Niagara 비주얼·상호작용 타겟·근접 클라이언트 복제를 담당한다.

```
[ULNPMassSpawnSubsystem] → 결정론적 스폰 (SurfaceCache 표면 투영)
[ULNPInteractableRegistrySubsystem] → 살아 있는 인터랙터블(Pod·Dice) 레지스트리 (ULNPInteractionComponent가 순회)
[Mass Processors]        → 상태 전환·게이지 (서버 전용, 태그 교체 상태 머신)
[ALNPLootPod Actor]      → 레지스트리 등록 + Niagara VFX + 상태·게이지 복제
```

> 상호작용 탐색에 쓰던 SmartObject 공간 쿼리는 Mass 액터 풀링과의 구조적 충돌로 폐기 (→ [DiscardedApproaches.md](DiscardedApproaches.md) Case 03).

**상태 머신 (태그 교체):**

```
FLNPLootPodIdleTag ──(활성화 요청 Tag 감지 — Interaction Input)──▶ FLNPLootPodLootingTag
        ▲                                                  │ 존 활성: 범위 내 모든 플레이어(프레즌스)의 속도 합산으로 게이지 증가,
        │                                                  │ 전원 이탈 시 감쇠 — 존은 활성 유지, 복귀만으로 재개
        └──(감쇠 끝에 게이지 0 — 완전 취소)◀────────────────┤
                                            (게이지 완료)──▶ Popped → 축하 VFX + LootDice 스폰* → 엔티티 소멸
```

> `*` 표시는 기획 확정·미구현 항목 (§5).

---

## 2. MassEntity 구성 요소

### 2.1 Fragments & Tags

| 타입 | 내용 |
|:---|:---|
| `FLNPLootPodFragment` | `State`(Idle/Looting/Popped), `CurrentGauge`/`MaxGauge`, `LootableDistSquared`(루팅 존 반경² — 트레잇 `LootingZoneRadius`에서 주입, 기본 500cm), `PodID`(보상 조회 키) |
| `FLNPPlayerLootingFragment` | Player 엔티티 상주 (최초 상호작용 시 부착) — `BuffedLootSpeed` (아이템/스킬 변조 배율, 없으면 1.0 취급) |
| Tags | `FLNPLootPodTag`(식별), `FLNPLootPodIdleTag`/`FLNPLootPodLootingTag`(상태별 쿼리 분리), `FLNPPlayerLootingTag`(**1회성 존 활성화 요청** — §4.6) |
| `LNPLootPodGaugeDecayFractionPerSecond` | 게이지 감쇠 속도 constexpr (MaxGauge 대비 초당 비율, 기본 0.15) — 1.0 이상이면 사실상 즉시 초기화 룰 |

> `Interrupted` Enum 값은 **2026-07-09 기획 개정으로 "피격 즉시 취소"가 폐기**되면서 제거됨 (넉백에 의한 존 이탈은 기존 범위 체크가 자동 감지 — [GameDesign_LootPod.md](GameDesign_LootPod.md) §2.2).

`ULNPLootPodTrait`가 엔티티 템플릿을 구성하며, MassReplication Trait(BubbleInfo/Replicator 고정)를 내부 위임한다 (Phase 7). **루팅 존 반경(`LootingZoneRadius`, 기본 500)은 트레잇 프로퍼티** — Pod 종류별 EntityConfig에서 조정하며, Actor(BP)의 `LootingZoneSphere` 반경(존 표시)과 일치시켜야 한다.

**반경 2종 분리:** 상호작용(루팅 State 진입)은 Actor의 `InteractionRadius`(기본 150cm, 단말기 조작 컨셉) + `MaxInteractionAngle`(60°) — 루팅 존(게이지 기여·사수)은 위의 500cm. 존 안에 있어도 State 진입은 단말기 앞에서만 가능하다.

### 2.2 Processors (서버 전용 — `IsClientWorld` 가드)

**`ULNPIdleToLootingProcessor`** — Idle Pod 주변에서 활성화 요청 Tag(`FLNPPlayerLootingTag`) 보유자 감지 → `Looting` 전환 + Pod 태그 교체 (Deferred). 실행 말미에 **요청 Tag를 일괄 소비**한다 (1회성 — §4.6).

**`ULNPLootingProcessor`** — Looting Pod마다:
- **범위 내 모든 플레이어(`FLNPPlayerTag` 프레즌스 기반)의 `BuffedLootSpeed`를 합산**해 게이지 누적 — 입력 없이 존에 들어오기만 해도 기여하며, 여러 명이 함께 루팅하면 그만큼 빨라진다 (`FLNPPlayerLootingFragment`는 Optional, 없으면 기본 1.0).
- 게이지 진행률을 Actor 복제 프로퍼티에 반영 (게임 스레드 지연 커맨드, 2% 임계값) — 증가·감쇠 공통 경로.
- 완료 → `Popped` 전환 커맨드 + **엔티티 즉시 파괴** (Fragment/Tag 갱신 불필요 — 비주얼·복제 전파는 커맨드의 `UpdateVisuals`가 담당).
- **전원 이탈 → 게이지 감쇠 (존 활성 유지):** `MaxGauge × LNPLootPodGaugeDecayFractionPerSecond`/초 — Looting 태그를 유지하므로 누구든 복귀·합류만으로 재개된다.
- **감쇠 끝에 게이지 0 도달 → 완전 취소:** `Idle` 복귀 + 태그 교체 — 재활성화는 Interaction Input부터.

**`FLNPPodStateTransitionCommand`** (Game Thread) — 상태 전환 통합 후처리: `UpdateVisuals` 호출, 로그, `Popped` 시 `ALNPLootDice::SpawnPodRewards(PodID, Location)` 호출 (Pod Actor는 이미 파괴됐을 수 있어 Entry 데이터만 사용).

---

## 3. 상호작용 흐름

### 3.1 플레이어 측 (ULNPInteractionComponent)

`ULNPInteractableRegistrySubsystem` 레지스트리(Pod·Dice Actor가 BeginPlay/EndPlay에 자기 등록/해제, AActor 기반 — LootDice 도입 시 일반화)를 매 Tick 순회해 `InteractionRadius` 내 후보를 수집, UI에 노출. High LOD Actor는 항상 소수(플레이어 근접 시에만 스폰)라 순회 비용은 무시할 수준이며, 풀에 반납된(Hidden) 액터는 제외한다. SmartObject 공간 쿼리를 쓰지 않는 이유는 [DiscardedApproaches.md](DiscardedApproaches.md) Case 03.

**상호작용 프롬프트:** 후보(거리+각도 통과) 중 **가장 가까운 Idle Pod** 머리 위에 키 아이콘(스크린 스페이스 WidgetComponent, 기본 "F" 라벨)을 표시한다. 판정은 로컬 플레이어 전용(`IsLocallyControlled`) — 복제 무관. Looting 중인 Pod는 프레즌스 기여라 입력이 불필요하므로 프롬프트를 띄우지 않는다. 기본 위젯(`ULNPInteractionPromptWidget`)은 에셋 없이 C++로 구성되며, 아트 적용 시 WidgetComponent의 WidgetClass만 교체하면 된다.

```
상호작용 입력 → 로컬 CanInteract(초근접 InteractionRadius + MaxInteractionAngle) + Pod->StartLooting() (비주얼 예측)
  ├─ 서버/리슨호스트: 즉시 StartLootingOnServer()
  └─ 원격 클라이언트: Server_StartLooting RPC → 서버가 CanInteract 재검증
       → 서버 월드의 플레이어 엔티티에 활성화 요청 Tag 부여 (+ 최초 1회 FLNPPlayerLootingFragment)
       → 다음 프레임 ULNPIdleToLootingProcessor가 감지 후 Tag 소비
```

### 3.2 ALNPLootPod Actor — 비주얼과 복제

- 컴포넌트: `UNiagaraComponent`(빛기둥) / `USphereComponent`(루팅 구역) / `UMassAgentComponent`(Mass 브릿지) / `UWidgetComponent`(상호작용 프롬프트, 기본 숨김) / `SmartObjectComponent`(**상호작용 경로 미사용** — NPC AI 연동 후보로 보류, 불필요 확정 시 제거)
- Niagara `User.Color` 파라미터를 상태별 색상(Idle/Looting/Popped)으로 전환.
- **이중 복제 (Phase 7):** 엔티티 존재·초기 위치는 MassReplication bubble이 전 클라이언트에, `CurrentState`(OnRep → UpdateVisuals)·`CurrentGaugePercent`는 Actor 복제가 근접 클라이언트에 전달. (→ [TechDesign_Networking.md](TechDesign_Networking.md) §3.5)

---

## 4. 어필 포인트 (설계 판단)

### 4.1 태그 교체 상태 머신 — 쿼리가 곧 상태 필터

상태를 Fragment 값으로만 두면 모든 프로세서가 전체 Pod를 순회하며 분기해야 한다. 상태별 태그(`IdleTag`/`LootingTag`)로 아키타입을 분리하면 각 프로세서의 쿼리가 자기 상태의 엔티티만 받는다 — 상태 검사 비용이 쿼리 필터로 흡수된다. LootPod은 상태 전환 빈도가 낮아 태그 교체(아키타입 마이그레이션) 비용이 문제되지 않는 케이스 (전환이 잦은 Enemy 넉백은 반대 판단 — [TechDesign_EnemyNPC.md §7.3](TechDesign_EnemyNPC.md)).

### 4.2 게이지 복제의 트래픽 제어

매 프레임 변하는 게이지를 그대로 복제하면 Pod 수 × 프레임만큼 트래픽이 나간다. 서버는 **2% 이상 변화(또는 0/1 도달) 시에만** 복제 프로퍼티에 기록 — UI 게이지의 시각적 부드러움은 유지하면서 대역폭을 상수화.

### 4.3 원격 클라이언트 루팅 공백 — 서버 엔티티에 태그를 붙여라

루팅 태그를 로컬 월드의 플레이어 엔티티에만 붙이던 초기 구현은 리슨 호스트에서만 동작했다 — LootPod 프로세서는 서버 전용이라 **서버 월드의 엔티티**에 태그가 있어야 감지한다. `Server_StartLooting` RPC로 해소 (Guard/Parry RPC와 동일 유형의 공백 — 클라이언트 로컬 Mass 상태 변경은 서버에 자동 전파되지 않는다는 일반 원칙).

### 4.4 다인 협동 루팅

범위 내 모든 루터의 속도를 합산하는 설계라, "함께 루팅하면 빨리 깐다"는 협동 인센티브가 프로세서 루프 한 줄로 구현된다. 루팅 속도 버프도 `BuffedLootSpeed` 필드 하나로 어떤 아이템/스킬이든 연동 가능.

### 4.5 취소 조건의 일원화 — 피격 취소를 만들지 않는 이유

"피격 시 즉시 취소"를 별도 구현하면 HitDetection → LootPod 간 브릿지가 필요했다. 기획 개정(존 사수 게임플레이)으로 취소 조건이 **"감쇠 끝에 게이지 0"** 하나로 일원화되면서, 넉백 피탈은 기존 거리 체크가 자동으로 잡는다 — 시스템 간 결합 하나가 통째로 사라진 케이스.

### 4.6 입력=활성화, 기여=프레즌스 — 1회성 요청 태그

초기 구현은 `FLNPPlayerLootingTag`를 "루팅 중" 상태 표시로 쓰면서 제거 경로가 없었다 — 한 번 루팅한 플레이어가 **영구 루터**가 되어 Idle Pod 옆을 지나가기만 해도 재활성화되는 잠재 버그. 기획 확정("완전 취소 후엔 Input부터, 감쇠 중엔 복귀만으로 재개")을 계기로 역할을 분리했다:

| 역할 | 수단 |
|:---|:---|
| 존 활성화 | 요청 Tag — Interaction Input 시 부여, `ULNPIdleToLootingProcessor`가 처리 후 즉시 소비 (1회성) |
| 루팅 기여 | 프레즌스 — 활성 존 범위 안의 모든 `FLNPPlayerTag` 플레이어 (입력 없이 합류·복귀 가능) |

"누구든 존에 들어오기만 하면 기여한다"는 협동 기획이 쿼리 요구 조건 변경만으로 구현되고, 태그 수명 관리 문제도 함께 소멸했다.

---

## 5. 미구현 항목과 작업 계획

### 5.1 게이지 감쇠·완전 취소·프레즌스 기여 — ✅ 구현 완료 (2026-07-10)

구현 내용은 §2.2 `ULNPLootingProcessor`, §4.6 참조. 감쇠 속도는 `LNPLootPodGaugeDecayFractionPerSecond`(`LNPLootPodMassTypes.h`) 하나로 제어.
**잔여 검증:** PIE 2인 — 전원 이탈 시 게이지 감소 표시, 0 도달 후 존 재진입만으로는 미재개(Input 필요), 감쇠 중 복귀 시 잔여분부터 재개.

### 5.2 루팅 속도 스탯·버프 연동 — ✅ 구현 완료 (2026-07-10)

`ULNPBaseAttributeSet`에 `LootSpeed` Attribute 신설 (기본 1.0, 복제, 0.01 하한). Fragment 동기화는 2경로:

| 경로 | 시점 | 커버 |
|:---|:---|:---|
| **Attribute 변경 델리게이트** (`ALNPPlayerCharacter::PushLootSpeedToEntity`, PossessedBy에서 바인딩 — 서버 전용) | 버프 GE 적용/만료 즉시 | 루팅 도중 버프 변동, **상호작용 이력 없는 파티원의 버프**까지 실시간 반영 (Fragment 없으면 생성) |
| **상호작용 시점 캐싱** (`StartLootingOnServer`) | Fragment 최초 부착 시 | 델리게이트 바인딩 전 이력·기본값 |

버프 아이템/GE는 `LootSpeed` Attribute만 변조하면 자동 연동된다.
**검증 완료 (2026-07-27):** `LootSpeed` 변조 GE 에셋(`GE_Buff_LootSpeed`, Infinite·AddBase +1.0)과 버프 아이템(`DA_Buff_LootSpeed` = Loot Booster)이 구현되어 게이지 가속이 동작한다 (→ [GameDesign_Ability.md](GameDesign_Ability.md) §3.3 버프 아이템 목록). 인벤토리 패널의 스탯 리드아웃에서 `LootSpeed` 최종값을 확인할 수 있다.

### 5.3 Popped 후처리 — LootDice 보상 스폰·축하 VFX ✅ 구현·PIE 검증 완료 (2026-07-14)

`FLNPPodStateTransitionCommand::Run()`에서 `ALNPLootDice::SpawnPodRewards(PodID, Location)` 호출:
1. `LNPSettings.LootDiceRewardTable`(`ULNPLootDiceRewardTable`)에서 `PodID` 조회 (미등록 시 `DefaultRewards` 폴백) → **후보 풀**에서 가중 추첨으로 `MinDrops`~`MaxDrops`개(기본 3~4) 선정 (→ [TechDesign_LootDice.md](TechDesign_LootDice.md) §3.1).
2. `ALNPLootDice` N개 스폰 + Pop 임펄스·고속 회전 (→ [TechDesign_LootDice.md](TechDesign_LootDice.md) §2.8).
3. 축하 나이아가라(Confetti)도 같은 Popped 블록에서 **위치 기반·Actor 독립**으로 스폰 — `LNPSettings.LootPodConfettiVFX`(= `NS_LootPodConfetti`)를 `SpawnSystemAtLocation(Entry.Location)`으로 원샷 재생.
   - ⚠️ **초기 구현의 함정(수정 완료):** 처음엔 `ALNPLootPod::UpdateVisuals`(Actor)에서 스폰했으나, Pop 전환이 같은 배치의 `DestroyEntity`로 표현 Actor를 파괴해 `Pod != nullptr` 가드가 실패 → **스폰이 전혀 도달하지 않았다**. 보상 스폰이 위치 기반인 것과 동일한 이유로 Confetti도 프로세서에서 스폰해야 한다. 구 `BP_LNPLootPod.PopConfettiVFX` 경로는 제거됨.
   - 에미터 3종(스프라이트 2·메시 1) 색상 랜덤 HSV(채도 보정 ON·hue 전역) + 크기 5배 튜닝 — PIE에서 알록달록한 색종이 확인.
   - **MP 잔여:** 서버 스폰 Niagara는 원격 클라에 복제되지 않음(리슨 호스트/싱글만 표시) — NetMulticast는 네트워킹 후순위.

### 5.4 빛기둥 Low LOD 표시 — ✅ 구현 완료 (2026-07-12)

빛기둥은 "멀리서도 보인다"가 스펙이므로 Actor(High LOD) 유무와 무관하게 상시 표시되어야 한다. 존재·위치 데이터는 이미 MassReplication bubble이 전 클라이언트에 전달 중 — 시각화만 추가했다.

- **채택: 방안 (a) ISMC 에미시브 빔 메시** — 코드 변경 없이 EntityConfig 데이터만으로 해결. `MassCrowdVisualizationTrait`의 `StaticMeshInstanceDesc`에 에미시브 실린더 빔(엔진 Cylinder × 스케일 (0.4,0.4,30) = ~40cm 지름 30m 빔, +1500cm Up 오프셋)을 두 번째 ISM 메시로 추가. 머티리얼 `M_LootPillar_LowLOD`(Unlit·Additive·양면, `PillarColor`/`Intensity` 파라미터, **`bUsedWithInstancedStaticMeshes` 필수** — 누락 시 기본 회색 머티리얼로 대체됨).
- **LOD 표현 확장:** `LODRepresentation`을 `[HighResActor, HighResActor, StaticMeshInstance, None]` → 마지막을 `StaticMeshInstance`로 변경해 최원거리(LOD3)에서도 빔이 보이게 함. 빛기둥은 LOD2-3(ISM)에서, 상태별 색 Niagara 기둥은 LOD0-1(Actor)에서 렌더 — **밴드가 겹치지 않아 이중 표시가 구조적으로 방지된다** (원거리 빔은 단일 색, 상태 색 전환은 근접에서만).
- **소멸:** Pod Popped → bubble 엔티티 제거 → ISM 인스턴스 자동 제거.
- **방안 (b) 단일 Niagara + 위치 배열은 폐기** — (a)가 기존 Low LOD ISM 인프라를 그대로 재사용해 코드 0줄로 충족.
- **클라이언트 가시 거리 (2026-08-05 수정):** 초기 구현은 클라이언트에서만 근접해야 빔이 보였다. 원인은 시각화가 아니라 **복제** — `FMassReplicationParameters::LODDistance[Off]` 엔진 기본값 5,000cm가 반지름 25,000cm 월드에 비해 너무 좁아 엔티티 자체가 클라에 도달하지 못했다. 서버는 로컬이라 시각화 거리(60,000)까지 다 보여 증상이 한쪽에만 나타났다. `ULNPLootPodTrait::ReplicationCullDistance`로 `VisibleLODDistance[Off]`와 짝을 맞춘다 — 상세는 `TechDesign_Networking.md` §3.5. 같은 수정으로 클라이언트 빔이 월드 Z 기준으로 누워 있던 문제(적도 부근)도 해결됐다 (`EngineAnalysis_MassReplication.md` §7.9).

**검증:** PIE 1인 — 원거리 pod들에서 청록 빛기둥 다수 확인 (Actor 컬 거리 밖). PIE 2인 (2026-08-05) — 클라이언트에서도 전 영역 빛기둥 가시, 적도 포함 전 구간 기둥 자세가 구 중심을 향함, NPC 이동 서버·클라 일치 확인. **잔여:** Pop 후 빔 소멸 확인.

### 5.5 난이도 스케일링 트리거 — ⏸ 후순위 (2026-07-12)

활성 Pod 수 카운터 → `ULNPTargetingSubsystem` 슬롯 한도/NPC 능력치 조정.
난이도 조절이 의미를 갖는 게임플레이가 완성된 뒤 착수. (Pod 리스폰은 스펙 제외 — [Idea_Backlog.md](Idea_Backlog.md))

### 5.6 Actor (재)스폰 시 엔티티 상태 동기화 — ✅ 구현 완료 (2026-07-10)

Representation이 Actor를 스폰할 때 엔티티 Transform만 동기화되고 `CurrentState`/`CurrentGaugePercent`는 기본값(Idle/0%)으로 시작한다 — 실제 발생한 버그: 루팅 존을 들락거리다 Actor가 LOD 소멸→재스폰되면 Idle로 보이는 Pod에 F 프롬프트가 뜨는데, 엔티티는 Looting이라 존 안 프레즌스 기여로 몰래 차올라 "갑자기 Pop"했다.

**해결 2중:**
1. **자기치유** — `ULNPLootingProcessor`의 게이지 동기화 커맨드(게임 스레드)에서 Actor 상태가 Looting이 아니면 `UpdateVisuals(Looting)`을 밀어 넣는다. 재스폰 Actor가 한 프레임 안에 올바른 상태·게이지로 복원. Idle/Popped 케이스는 기본값·소멸이 각각 자연 일치라 별도 처리 불필요.
2. **High LOD 범위 확장** — LootPod config의 LODParams를 High ~25m(base 2500/visible 3000)로 확장 (기존 ~5-10m는 존 반경 5m와 겹쳐 존 이탈만으로 Actor가 소멸했다). LODMaxCount High도 64로 상향 (멀티 대비).

### 권장 구현 순서

```
1. §5.1 게이지 감쇠 — ✅ 완료
2. §5.2 LootSpeed Attribute 연동 — ✅ 완료
3. LootDice 단독 구현 (TechDesign_LootDice.md §3의 1~5단계) — ✅ 코드 완료 (PIE 검증 잔여)
4. §5.3 Popped 후처리 (보상 테이블 + LootDice 연결) — ✅ 코드 완료 / 축하 VFX 에셋 잔여
5. §5.4 빛기둥 Low LOD — ✅ 완료 (2026-07-12)
6. §5.5 난이도 스케일링 — ⏸ 후순위 (게임플레이 완성 후)
7. 다음 트랙: 인벤토리 UI (드랍/양도 UI 포함)
```
