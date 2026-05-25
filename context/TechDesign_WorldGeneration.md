# 월드 생성 시스템 기술 설계

## 1. 개요

LootNPop의 메인 월드는 거대한 구체의 **내부 표면**이 플레이 공간인 Dyson Sphere 구조. 전체 구체를 8개의 Octant(1/8 조각)로 분할하고 각각을 Level Instance로 관리하는 **분할 정복 방식** 채택.

> 단일 PCG 볼륨 기반 전역 생성 방식은 스케일링 한계로 기각. 상세 이력은 [DiscardedApproaches.md](DiscardedApproaches.md) 참조.

---

## 2. Octant 분할 전략 (구현 완료)

### 2.1 8분할 구조

전체 구체를 X, Y, Z 축 기준으로 8개의 균일한 조각으로 분할. 각 조각은 개별 Level Instance로 관리되며, 기준 Octant 하나를 원본으로 삼고 나머지 7개는 90°/180° 회전 배치로 구체를 완성.

**구현된 회전값 (`LNPOctantSpawnSubsystem.cpp`):**

| 인덱스 | 회전 (Pitch, Yaw, Roll) |
|:---:|:---|
| 0 | (0°, 0°, 0°) — 기준 Octant |
| 1 | (0°, 90°, 0°) |
| 2 | (0°, 180°, 0°) |
| 3 | (0°, 270°, 0°) |
| 4 | (180°, 0°, 0°) |
| 5 | (180°, 90°, 0°) |
| 6 | (180°, 180°, 0°) |
| 7 | (180°, 270°, 0°) |

### 2.2 Octant Pool

`ULNPOctantPoolData` 데이터 에셋에 사용 가능한 Octant 레벨 목록을 관리. 스폰 시 `FRandomStream(OctantGenSeed)`으로 풀에서 무작위 선택하여 결정론적으로 8개를 배치.

```
OctantPool → [LevelA, LevelB, LevelC, ...]
                    ↓ FRandomStream (seed 고정)
              8개 무작위 선택 → 각 회전으로 배치
```

---

## 3. 런타임 스폰 흐름 (구현 완료)

`ULNPOctantSpawnSubsystem`이 Level Instance 스폰과 로드 완료 감시를 담당.

```
StartWorldGeneration()
├─ GameState->OctantGenSeed 읽기 (0이면 GameMode가 설정 완료한 상태)
├─ FRandomStream(OctantGenSeed)으로 8개 Octant 선택
├─ World->SpawnActor<ALevelInstance>() × 8 (AlwaysSpawn)
├─ LevelInstance->SetWorldAsset() + LoadLevelInstance()
└─ bIsGenerating = true → Tick() 감시 시작
        │ 매 프레임: 8개 IsLoaded() 체크
        ▼
bIsGenerating = false, bGenerationComplete = true
OnWorldGenerationFinished.Broadcast()
```

**결정론적 보장:** 서버 GameMode가 `OctantGenSeed`를 GameState에 쓰고 복제 → 클라이언트도 동일한 seed로 `StartWorldGeneration()` 호출 → 동일한 8개 배치. (상세: [TechDesign_InitSequence.md](TechDesign_InitSequence.md))

---

## 4. Octant 내부 지형 구성 (설계 완료, 구현 예정)

각 Octant Level 내부는 두 레이어로 구성.

### 4.1 기본 지각 (Geometry Script)

구체 면을 Spherified Octant 방식으로 투영하여 곡면 기반 지각(Crust)을 동적 생성. 조각 간 경계 왜곡과 틈새를 방지하는 핵심 단계.

### 4.2 세부 지형 (PCG Layer)

`UPCGSphereWorldSettings` 커스텀 노드로 지각 위에 노이즈 변조 및 오브젝트 배치 수행.

**3D Perlin Noise 지형 변조:**
```
SurfacePoint = Origin + Dir × (Radius + FMath::PerlinNoise3D(Point × NoiseFreq) × Amplitude)
```

**경사면 정렬:** 주변 두 샘플 지점의 외적으로 지형 법선 계산 → `FRotationMatrix::MakeFromZY`로 타일 정렬. Z축을 지형 법선에, Y축을 위도(East) 방향에 맞춰 텍스처 흐름을 일관되게 유지.

### 4.3 Baking

최종 PCG 결과물을 HISM(Hierarchical Instanced Static Mesh) 데이터로 구워 런타임 계산 제거. Nanite 적용으로 대규모 폴리곤 환경 구축.

---

## 5. 동기화 및 최적화

| 항목 | 방법 |
|:---|:---|
| 결정론적 생성 | `OctantGenSeed` 복제 + `FRandomStream` |
| 에디터 성능 | Level Instance 단위 작업, 필요 조각만 로드 |
| 런타임 스트리밍 | World Partition 그리드 연동 |
| 렌더링 최적화 | HISM Baking + Nanite |

---

## 6. 관련 클래스 인덱스

| 클래스 | 역할 |
|:---|:---|
| `ULNPOctantSpawnSubsystem` | Level Instance 스폰 및 로드 완료 감시 |
| `ULNPOctantPoolData` | 사용 가능한 Octant 레벨 목록 데이터 에셋 |
| `UPCGSphereWorldSettings` | 구형 PCG 포인트 생성 커스텀 노드 |
| `ALNPGameState::OctantGenSeed` | 결정론적 스폰을 위한 복제 시드 |
