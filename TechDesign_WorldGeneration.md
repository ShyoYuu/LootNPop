# 월드 생성 시스템 기술 설계 (World Generation Technical Design)

## 1. 개요 (Overview)
**LootNPop**의 월드는 거대한 **구체(Sphere)** 형태의 공간으로 구성됩니다. 현재 설계는 **1/8 조각(Octant) 분할 및 Level Instance 기반 조립** 방식을 채택하고 있으며, 각 조각은 **Geometry Script**와 **PCG**를 결합하여 생성됩니다.

> [!NOTE]
> 단일 거대 PCG 볼륨을 사용한 전역 생성 방식은 스케일링 문제로 인해 기각되었습니다. 상세 이력은 [IterationHistory.md](@IterationHistory.md)를 참조하십시오.

## 2. 핵심 설계 및 구현 상세

### 2.1 분할 정할 방식 (Octant Strategy)
- **구조:** 전체 구체를 $X, Y, Z$ 축을 기준으로 8개의 균일한 조각(Octant)으로 나눕니다.
- **Level Instance:** 각 조각은 개별 레벨 인스턴스로 관리되며, 메인 월드에서 90도/180도 회전 배치를 통해 구체를 완성합니다.
- **장점:** 에디터에서 한 번에 하나의 조각만 작업할 수 있어 성능 부하가 낮고, 필요한 구역만 스트리밍(World Partition)하는 데 유리합니다.

### 2.2 지각 형성 (Geometry Script Base)
- **Geometry Script:** 조각별 기본 지각(Crust)은 지오매트리 스크립트를 사용하여 동적으로 생성합니다.
- **Spherified Octant:** 정육면체의 면을 구체로 투영(Normalize)한 형태의 곡면을 생성하여, 조각 간의 경계선 왜곡과 틈새를 방지합니다.

### 2.3 세부 지형 및 오브젝트 (PCG Layer)
- **PCG Node (`UPCGSphereWorldSettings`):** 지오매트리 스크립트로 생성된 지각 위에서 노이즈 변조 및 타일링 배치를 수행합니다.
- **Baking:** 최종 생성된 PCG 결과물은 HISM(Instanced Static Mesh) 데이터로 구워져(Bake), 런타임에는 복잡한 수학적 계산 없이 렌더링만 수행됩니다.

### 2.2 지형 노이즈 및 경사면 정렬 (Slope-Aware Alignment)
- **3D Perlin Noise:** `FMath::PerlinNoise3D`를 활용하여 반지름($R$) 값을 변조함으로써 자연스러운 지형 굴곡을 형성합니다.
- **경사면 법선(Slope Normal) 계산:** 
    - 현재 위치 주변의 두 지점($P1, P2$)을 샘플링하여 평면의 법선을 외적으로 산출합니다.
    - 이를 통해 타일이 단순히 중심을 바라보는 것이 아니라, 지형의 경사에 맞춰 정렬되어 타일 간의 틈새를 방지합니다.
- **회전 정렬:** `FRotationMatrix::MakeFromZY`를 사용하여 $Z$축을 지형 법선에, $Y$축을 위도(East) 방향에 맞춤으로써 일관된 텍스처 흐름을 유지합니다.

### 2.3 커스텀 중력 및 카메라 제어 (`LNPPawnGravityComponent`)
- **중력 모드:** `Fixed`, `RadialInward`, `RadialOutward` 모드를 지원하여 다양한 행성 환경에 대응합니다.
- **카메라 보정 (Curvature-aware Camera):** 
    - 플레이어가 이동함에 따라 변하는 `UpVector`를 실시간으로 추적합니다.
    - **Surface Curvature Compensation:** 캐릭터가 구면을 따라 움직일 때 발생하는 컨트롤러 회전 오차를 쿼터니언 보간을 통해 보정하여, 플레이어가 멀미 없이 안정적인 시야를 유지하게 합니다.
    - **Pitch Clamping:** 커스텀 방향 기준으로 시야각 상하 한도(약 85도)를 제한하여 짐벌 락 현상을 방지합니다.

## 3. 동기화 및 최적화
- **Deterministic Generation:** 서버의 `ALNPGameState`에서 전달된 `SeedOffset`을 PCG 입력값에 사용하여 모든 클라이언트가 동일한 지형을 생성하도록 보장합니다.
- **HISM & Nanite:** 생성된 모든 포인트는 HISM 인스턴스로 시각화되며, 나나이트를 적용하여 대규모 폴리곤 환경을 구축합니다.
