# LootPod 시스템 기술 설계

## 1. 한눈에 보기

**LootPod**은 MassEntity(시뮬레이션) + SmartObject(상호작용 쿼리) + Actor(비주얼·복제)를 결합한 하이브리드 오브젝트. 게이지·근접 판정은 서버 Mass 프로세서가 태그 교체 상태 머신으로 처리하고, Actor는 Niagara 비주얼과 근접 클라이언트 복제만 담당한다.

```
[ULNPMassSpawnSubsystem] → 결정론적 스폰 (SurfaceCache 표면 투영)
[SmartObjectSubsystem]   → 상호작용 후보 쿼리 (ULNPInteractionComponent가 검색)
[Mass Processors]        → 상태 전환·게이지 (서버 전용, 태그 교체 상태 머신)
[ALNPLootPod Actor]      → SmartObject 등록 + Niagara VFX + 상태·게이지 복제
```

**상태 머신 (태그 교체):**

```
FLNPLootPodIdleTag ──(범위 내 FLNPPlayerLootingTag 감지)──▶ FLNPLootPodLootingTag
        ▲                                                        │
        └──(범위 내 루터 없음 — Idle 복귀)◀───────────────────────┤
                                              (게이지 완료)──▶ Popped → 엔티티 즉시 소멸
```

---

## 2. MassEntity 구성 요소

### 2.1 Fragments & Tags

| 타입 | 내용 |
|:---|:---|
| `FLNPLootPodFragment` | `State`(Idle/Looting/Interrupted/Popped), `CurrentGauge`/`MaxGauge`, `LootableDistSquared`, `PodID`(보상 조회 키) |
| `FLNPPlayerLootingFragment` | Player 엔티티 부착 — `BuffedLootSpeed` (아이템/스킬 변조 배율) |
| Tags | `FLNPLootPodTag`(식별), `FLNPLootPodIdleTag`/`FLNPLootPodLootingTag`(상태별 쿼리 분리), `FLNPPlayerLootingTag`(루팅 중 플레이어) |

> `Interrupted`는 Enum에만 정의되어 있고 프로세서 로직 미사용 — 범위 이탈은 `Looting → Idle` 직행. 피격 취소(§5.2) 구현 시 활용 재검토.

`ULNPLootPodTrait`가 엔티티 템플릿을 구성하며, MassReplication Trait(BubbleInfo/Replicator 고정)를 내부 위임한다 (Phase 7).

### 2.2 Processors (서버 전용 — `IsClientWorld` 가드)

**`ULNPIdleToLootingProcessor`** — Idle Pod 주변에서 `FLNPPlayerLootingTag` 플레이어 감지 → `Looting` 전환 + 태그 교체 (Deferred).

**`ULNPLootingProcessor`** — Looting Pod마다:
- **범위 내 루터들의 `BuffedLootSpeed`를 합산**해 게이지 누적 — 여러 명이 함께 루팅하면 그만큼 빨라진다.
- 게이지 진행률을 Actor 복제 프로퍼티에 반영 (게임 스레드 지연 커맨드, 2% 임계값).
- 완료 → `Popped` 전환 커맨드 + **엔티티 즉시 파괴** (Fragment/Tag 갱신 불필요 — 비주얼·복제 전파는 커맨드의 `UpdateVisuals`가 담당).
- 범위 내 루터 없음 → `Idle` 복귀 + 태그 교체.

**`FLNPPodStateTransitionCommand`** (Game Thread) — 상태 전환 통합 후처리: `UpdateVisuals` 호출, 로그, `Popped` 보상 스폰 지점(TODO 스텁).

---

## 3. 상호작용 흐름

### 3.1 플레이어 측 (ULNPInteractionComponent)

`SmartObjectSubsystem`으로 주변 `ALNPLootPod`을 지속 탐색(`InteractionRadius`, Tick), 후보 목록을 UI에 노출.

```
상호작용 입력 → 로컬 CanInteract(거리+MaxInteractionAngle) + Pod->StartLooting() (비주얼 예측)
  ├─ 서버/리슨호스트: 즉시 StartLootingOnServer()
  └─ 원격 클라이언트: Server_StartLooting RPC → 서버가 CanInteract 재검증
       → 서버 월드의 플레이어 엔티티에 FLNPPlayerLootingTag + FLNPPlayerLootingFragment 부여
       → 다음 프레임 ULNPIdleToLootingProcessor가 자동 감지
```

### 3.2 ALNPLootPod Actor — 비주얼과 복제

- 컴포넌트: `SmartObjectComponent` / `UNiagaraComponent`(빛기둥) / `USphereComponent`(루팅 구역) / `UMassAgentComponent`(Mass 브릿지)
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

---

## 5. 미구현 항목

### 5.1 게이지 초기화 (범위 이탈 시)
기획상 이탈 시 게이지가 초기화되어야 하나 현재 유지됨. `ULNPLootingProcessor` 이탈 분기에 `CurrentGauge = 0` 한 줄 추가로 해결 가능 — 기획 확정 대기.

### 5.2 Interruption (피격 루팅 취소)
피격 시 즉시 취소. HitDetection에서 피격 플레이어의 `FLNPPlayerLootingTag`를 제거하면 다음 프레임 프로세서가 Idle 복귀 — 연결 지점만 남음 (→ [TechDesign_HitDetection.md §8](TechDesign_HitDetection.md)).

### 5.3 보상 드롭 (Popped 후처리)
`FLNPPodStateTransitionCommand::Run()`에 TODO 스텁. `PodID` 기반 보상 테이블 조회 → 아이템 월드 스폰 → 인벤토리 연동 (→ [TechDesign_Ability.md](TechDesign_Ability.md) §6).

### 5.4 난이도 스케일링 트리거
활성 Pod 수 카운터 → `ULNPTargetingSubsystem` 슬롯 한도/NPC 능력치 조정.

### 5.5 루팅 속도 버프 연동
`BuffedLootSpeed`는 현재 기본값 1.0 고정 — 플레이어 스탯/버프에서 읽어오는 연결 필요 (프로세서에 TODO).
