# LootPod 시스템 기술 설계

## 1. 아키텍처 개요

**LootPod**은 대규모 배치와 상태 관리 효율성을 위해 MassEntity와 SmartObject를 결합한 하이브리드 방식 사용.

```
[ULNPMassSpawnSubsystem] → 스폰
[SmartObjectSubsystem]   → 상호작용 쿼리 (ULNPInteractionComponent가 검색)
[MassEntity Processors]  → 상태 업데이트 (게이지, 전환)
[ALNPLootPod Actor]      → 비주얼 동기화 (Niagara VFX, 메시 상태)
```

---

## 2. MassEntity 구성 요소 (구현 완료)

### 2.1 Fragments & Tags

**`FLNPLootPodFragment`** — Pod 고유 상태
- `ELNPLootPodState State` — `Idle` / `Looting` / `Interrupted` / `Popped`
- `float CurrentGauge` / `float MaxGauge` — 루팅 진행도
- `float LootableDistSquared` — 유효 루팅 거리 (제곱값, 최적화)
- `int32 PodID` — 보상 데이터 조회 키

**`FLNPPlayerLootingFragment`** — Player 엔티티에 부착
- `float BuffedLootSpeed` — 아이템/스킬에 의해 변조된 루팅 속도 배율

**Tags:**
- `FLNPLootPodTag` — LootPod 엔티티 식별
- `FLNPLootPodIdleTag` / `FLNPLootPodLootingTag` — 상태별 쿼리 분리
- `FLNPPlayerLootingTag` — 루팅 중인 플레이어 식별

### 2.2 Processors

**`ULNPIdleToLootingProcessor`**
- `FLNPLootPodIdleTag` 보유 Pod 주변에서 `FLNPPlayerLootingTag` 플레이어를 감지
- 감지 시 State → `Looting` 전환, 태그 교체

**`ULNPLootingProcessor`**
- `FLNPLootPodLootingTag` 보유 Pod의 게이지를 `DeltaTime × BuffedLootSpeed`로 업데이트
- 플레이어와 거리 체크 (매 프레임, `LootableDistSquared` 기준)
  - 범위 이탈 → State → `Idle` 복귀, 태그 교체
  - 게이지 완료 → State → `Popped`

---

## 3. 상호작용 흐름

### 3.1 플레이어 측 (ULNPInteractionComponent)

플레이어 캐릭터에 부착. `SmartObjectSubsystem`을 통해 주변 `ALNPLootPod`을 검색.
- 상호작용 입력 → 플레이어 엔티티에 `FLNPPlayerLootingTag` 추가 + `FLNPPlayerLootingFragment` 설정
- 이후 `ULNPIdleToLootingProcessor`가 자동으로 감지

### 3.2 Actor → Mass 상태 동기화 (ALNPLootPod)

Mass 프로세서에서 State가 전환되면 `ALNPLootPod` 액터의 Niagara VFX와 메시 상태를 갱신.

**Niagara 연동:**
- 파라미터 이름: `User.Color`
- 상태별 색상으로 루팅 진행 및 완료를 시각적으로 표현

**ALNPLootPod 컴포넌트 구성:**
- `SmartObjectComponent` — 상호작용 등록
- `UNiagaraComponent` (LootPillarVFX) — 빛기둥 이펙트
- `USphereComponent` (LootingZoneSphere) — 루팅 유효 범위 시각화
- `UMassAgentComponent` — Mass 엔티티 브릿지

---

## 4. 미구현 항목 (구현 예정)

### 4.1 Interruption (피격 루팅 취소)

플레이어가 적에게 피격을 받으면 루팅이 즉시 취소되어야 함.

**구현 방향:** HitDetection 시스템(→ [TechDesign_HitDetection.md](TechDesign_HitDetection.md))에서 피격 판정 시 해당 플레이어 엔티티의 `FLNPPlayerLootingTag`를 제거. 태그 제거가 트리거가 되어 `ULNPLootingProcessor`가 다음 프레임에 Idle로 복귀.

```
[HitDetection Processor] → 피격 확인
    → FMassDeferredSetCommand로 FLNPPlayerLootingTag 제거
    → 다음 프레임 ULNPLootingProcessor에서 감지 → Idle 복귀
```

### 4.2 보상 드롭 (Popped 후처리)

`Popped` 상태 전환 시 `PodID`를 기반으로 보상 데이터를 조회하고 아이템을 월드에 스폰. GAS 인프라 구축 후 구현.

### 4.3 난이도 스케일링 트리거

활성 Pod 수를 추적하는 카운터를 통해 `ULNPTargetingSubsystem`의 슬롯 한도 또는 NPC 능력치를 조정.

---

## 5. 확장성 설계 포인트

- **루팅 속도 버프:** `FLNPPlayerLootingFragment::BuffedLootSpeed` 필드 하나만 변경하면 어떤 아이템/스킬이든 루팅 속도에 영향 가능.
- **Pod 타입 다양화:** `PodID`를 통해 보상 테이블을 외부 데이터 에셋으로 관리하면 코드 수정 없이 보상 유형 추가 가능.
- **대규모 대응:** SmartObject로 쿼리, Mass로 시뮬레이션하는 구조를 유지하면 수천 개의 Pod도 성능 저하 없이 운용 가능.
