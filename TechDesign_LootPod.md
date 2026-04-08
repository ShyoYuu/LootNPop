# LootPod System 기술 설계 (MassEntity + SmartObject)

## 1. 아키텍처 개요
**LootPod**은 `MassEntity`를 통해 대규모 데이터를 관리하고, `SmartObjectSubsystem`에 핸들을 등록하여 상호작용을 지원합니다. 이는 액터 오버헤드 없이 수많은 상호작용 객체를 효율적으로 처리하기 위함입니다.

## 2. MassEntity 구성 요소
### 2.1 Fragments (데이터)
- **`FLNPLootPodFragment`:**
    - `float CurrentGauge`: 현재 루팅 진행도 (0.0 ~ 1.0).
    - `ELNPLootPodState State`: 현재 상태 (Idle, Looting, Interrupted, Popped).
    - `FSmartObjectHandle SOHandle`: 등록된 SmartObject 핸들.
- **`FTransformFragment`:** 엔티티의 위치 및 회전 (Sphere World 정렬 포함).
- **`FMassTag_LootPod`:** LootPod 엔티티를 식별하기 위한 태그.

### 2.2 Processors (로직)
- **`ULNPLootPodProcessor`:**
    - 매 프레임 모든 LootPod의 상태를 업데이트합니다.
    - 루팅 중인 엔티티의 게이지를 증가시키고, 완료 시 보상 스폰 이벤트를 트리거합니다.
- **`ULNPLootPodGravityProcessor`:**
    - `SphereWorld` 중심점을 기준으로 엔티티의 `UpVector`를 계산하여 정렬합니다.

## 3. SmartObject 연동 (MassSmartObject)
- **Registration:** 엔티티가 생성(Spawn)될 때 `SmartObjectSubsystem`에 위치와 `SmartObjectDefinition`을 등록하고 받은 핸들을 `SOHandle`에 저장합니다.
- **Interaction:** 플레이어가 `SmartObject`를 쿼리하면 시스템은 엔티티의 위치를 반환합니다.
- **LOD (Level of Detail):** 
    - 멀리 있는 LootPod은 순수 엔티티로 존재.
    - 가까이 가거나 상호작용이 시작되면 시각적 연출을 위해 **고성능 ISMC(Instanced Static Mesh)** 또는 **Actor Representation**으로 전환될 수 있습니다.

## 4. 수비형 루팅 로직 구현
- **Proximity Check:** `MassEntity`의 `FMassSpatialHashGrid`를 사용하여 주변 플레이어를 빠르게 검색, 루팅 구역 이탈 여부를 체크합니다.
- **Hit Detection:** 플레이어가 피격 시 해당 플레이어와 상호작용 중인 `SmartObjectHandle`을 찾아 엔티티의 상태를 `Interrupted`로 변경합니다.

## 5. 미래 확장성 (Future Considerations)
### 5.1 루팅 속도 버프 (Loot Speed Buff)
- **개요:** 특정 아이템이나 스킬을 통해 게이지 상승 속도를 일시적으로 증가시킴.
- **Mass 구현 계획:** 
    - `FLNPLootPodFragment`에 `BaseLootSpeed` 필드를 활용.
    - `ULNPLootingUpdateProcessor`에서 게이지 계산 시 버프 태그나 추가 프레그먼트의 배율을 적용하도록 설계.

### 5.2 멀티플레이어 협동 루팅
- 여러 플레이어가 동시에 루팅할 경우 속도가 중첩되거나 보너스를 주는 로직 확장 가능.

## 6. 구현 장점 (Learning Point)
- **확장성:** 나중에 LootPod이 수만 개로 늘어나도 성능 저하가 거의 없습니다.
- **데이터 중심 설계:** 모든 로직이 가벼운 구조체(Fragment) 단위로 처리됩니다.
- **최신 스택 활용:** `Mass` + `SmartObject` + `SphereWorld`의 결합을 통해 언리얼 엔진 5.x의 핵심 기술을 모두 학습할 수 있습니다.

