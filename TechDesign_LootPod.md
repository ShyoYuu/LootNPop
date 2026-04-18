# LootPod System 기술 설계 (MassEntity + SmartObject)

## 1. 아키텍처 개요
**LootPod**은 `MassEntity`를 통해 대규모 데이터를 관리하고, `SmartObjectSubsystem`에 핸들을 등록하여 상호작용을 지원합니다. 시각적 연출과 개별 상태 관리를 위해 `ALNPLootPod` 액터와 `MassAgentComponent`를 결합한 하이브리드 방식을 사용합니다.

## 2. MassEntity 구성 요소
### 2.1 Fragments & Tags (데이터)
- **`FLNPLootPodFragment`:**
    - `ELNPLootPodState State`: 현재 상태 (Idle, Looting, Interrupted, Popped).
    - `float CurrentGauge / MaxGauge`: 루팅 진행도 관리.
    - `float LootableDistSquared`: 루팅 유효 거리 (최적화를 위해 제곱값 사용).
    - `int32 PodID`: 보상 데이터 조회를 위한 고유 ID.
- **`FLNPPlayerLootingFragment` (Player Entity):**
    - `float BuffedLootSpeed`: 플레이어별 루팅 속도 배율.
- **Tags:** `FLNPLootPodTag`, `FLNPLootPodIdleTag`, `FLNPLootPodLootingTag`, `FLNPPlayerLootingTag` 등을 사용하여 프로세서의 쿼리 효율 최적화.

### 2.2 Processors (로직)
- **`ULNPIdleToLootingProcessor`:**
    - `Idle` 상태의 Pod 주변에 `PlayerLootingTag`를 가진 엔티티가 있는지 체크하여 상태 전환을 트리거합니다.
- **`ULNPLootingProcessor`:**
    - 루팅 중인 모든 Pod의 게이지를 `DeltaTime`과 플레이어의 `BuffedLootSpeed`를 합산하여 업데이트합니다.
    - 완료 시 `Popped` 상태로 전환하거나 엔티티를 파괴하고 보상을 드랍합니다.

## 3. 상호작용 및 시각화 (Actor Bridge)
### 3.1 `ULNPInteractionComponent`
- 플레이어 캐릭터에 부착되어 `SmartObjectSubsystem`을 통해 주변의 `ALNPLootPod`을 검색합니다.
- 상호작용 시작 시 플레이어 엔티티에 `FLNPPlayerLootingTag`를 추가하여 Mass 프로세서가 인지하도록 합니다.

### 3.2 `FLNPPodStateTransitionCommand`
- Mass 프로세서(C++)에서 상태 변화가 발생했을 때, 이를 실제 월드의 `ALNPLootPod` 액터에게 전달하기 위한 **Batched Command**입니다.
- 이를 통해 Niagara VFX(Pillar Color) 및 메시 상태를 성능 저하 없이 동기화합니다.

## 4. 수비형 루팅 로직 구현
- **Proximity Check:** `ULNPLootingProcessor`에서 매 프레임 플레이어와의 거리를 체크하여, 범위를 벗어나면 즉시 `Idle` 상태로 되돌리고 태그를 교체합니다.
- **Interruption:** (구현 예정) 플레이어 피격 시 플레이어 엔티티의 `LootingTag`를 제거하여 모든 관련 루팅 프로세스를 중단시킵니다.

## 5. 구현 특징 (Key Implementations)
- **효율적인 검색:** `SmartObject`를 사용하여 상호작용 대상을 찾고, 실제 로직 처리는 `Mass`에서 수행하여 수천 개의 Pod 대응 가능.
- **확장성:** `BuffedLootSpeed` 프래그먼트를 통해 아이템이나 스킬에 의한 루팅 속도 변화를 쉽게 적용.
- **시각적 피드백:** Niagara 시스템의 `User.Color` 파라미터를 Mass 상태와 연동하여 직관적인 상태 표시.

