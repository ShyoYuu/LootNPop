# Enemy NPC System - Technical Design Document

## 1. 시스템 아키텍처
시스템은 **MassEntity (Data/Sim)** -> **Targeting Subsystem (Logic/Balancing)** -> **Actor & GAS (Visual/Combat)**의 3단계 구조로 동작한다.

## 2. 데이터 구조
### 2.1. Mass Fragments & Tags
- `FLNPEnemyFragment`: `float Health`, `float MaxHealth`, `float Defense`. (Actor 없이도 Mass에서 데미지 계산 가능)
- `FLNPEnemySharedFragment`: `ULNPEnemyConfig` 포인터 보유. 동일 타입의 적들이 공유하는 데이터.
- `FLNPEnemyTargetingFragment`: 
    - `FMassEntityHandle TargetPlayer`: 현재 추적 중인 플레이어.
    - `float CurrentScore`: 우선순위 점수.
    - `ELNPTargetingState State`: 슬롯 상태 (None, Requesting, Confirmed, Releasing).
- `FLNPEnemyTag` / `FLNPPlayerTag`: 각각 적과 플레이어 엔티티를 식별하기 위한 마커 태그.

### 2.2. Data Assets
- `ULNPEnemyConfig` (`UPrimaryDataAsset`): 
    - `EnemyTypeTag`: 적 정체성 Gameplay Tag.
    - `EnemyActorClass`: 스폰할 Actor 클래스 (Generic Shell).
    - `SkeletalMesh` / `AnimBlueprint`: 비주얼 설정.
    - `DefaultAbilities`: 초기 부여할 GAS 어빌리티 리스트.
    - `TargetingConfig`: 점수 가중치 설정.

## 3. 핵심 컴포넌트
### 3.1. ULNPTargetingSubsystem (World Subsystem)
- 플레이어별 슬롯(Melee 10, Ranged 20) 현황 관리.
- **주요 함수**:
    - `RegisterEnemyInterest()`: 적이 자신의 점수를 등록.
    - `RebalanceSlots()`: 매 프레임 점수 기반으로 슬롯 재할당.
    - `IsSlotConfirmed()`: 특정 적의 슬롯 점유 여부 확인.

### 3.2. Mass Processors
- `ULNPEnemyScoringProcessor`: 타겟과의 거리를 기반으로 `CurrentScore` 업데이트.
- `ULNPEnemyTargetingProcessor`: 서브시스템과 통신하여 슬롯 상태 동기화.
- `ULNPHealthProcessor`: 체력 모니터링 및 사망(Entity Destroy) 처리.
- `ULNPEnemyRepresentationProcessor`: Actor 스폰 시 초기화 및 데이터 동기화(`SyncFromEntity`) 호출.

## 4. Actor 및 GAS (Gameplay Ability System) 연동
- **Generic Shell Actor (`ALNPEnemyCharacter`)**: 
    - `InitializeFromConfig()`: 데이터 에셋을 기반으로 메쉬 및 어빌리티 초기화.
    - `SyncFromEntity(Health, State)`: Mass -> Actor 데이터 주입.
    - `SyncToEntity(out Health)`: Actor -> Mass 데이터 추출.
- **Player Registration**: `ALNPCharacterBase`는 `BeginPlay`에서 `MassAgentComponent`를 통해 자신을 `FLNPPlayerTag`로 등록하여 적들의 타겟팅 대상이 됨.

## 5. 멀티플레이어 및 성능 고려사항
- **Thread Safety**: `LNPTargetingSubsystem`은 `FCriticalSection`을 사용하여 병렬 처리되는 Mass Processor들 사이의 데이터 안전성 보장.
- **Memory Efficiency**: 동일 타입의 적들은 `FLNPEnemySharedFragment`를 통해 데이터 에셋 포인터를 공유하여 메모리 사용량 최소화.
- **Authority**: 서버의 서브시스템이 슬롯 할당의 최종 권한을 가짐.

## 6. 중력 및 지면 정렬 (Gravity & Grounding)
- **제어권 분리 (Control Ownership)**:
    - **Actor 상태 (High LOD)**: `LNPPawnGravityComponent`가 물리 엔진 및 레이캐스트를 기반으로 정밀한 접지와 회전 제어.
    - **Mass 상태 (Medium/Low LOD)**: `ULNPEnemyGravityProcessor`가 수학적 연산을 통해 최소 비용으로 지면 밀착 및 방향 정렬 처리.
- **수학적 접지 (Sphere World Optimization)**:
    - 구체 행성 환경임을 고려하여, Mass 상태의 엔티티는 비싼 레이캐스트 대신 `행성 중심으로부터의 거리 = 반지름` 공식을 이용해 위치를 강제 보정(Grounding).
- **데이터 동기화**:
    - `MassRepresentationProcessor`의 기능을 활용하여 Actor의 실시간 위치를 Fragment로 역동기화.
    - 중력 방향(`UpVector`)은 프래그먼트에 저장하지 않고, 엔티티의 현재 위치를 기반으로 필요 시마다 실시간 계산하여 연산 효율성 극대화.
