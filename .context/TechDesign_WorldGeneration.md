# 월드 생성 시스템 기술 설계

## 1. 한눈에 보기

LootNPop의 메인 월드는 거대한 구체의 **내부 표면**이 플레이 공간인 Dyson Sphere 구조. 전체 구체를 8개의 Octant(1/8 조각)로 분할하고 각각을 Level Instance로 관리하는 **분할 정복 방식**을 채택했다.

```
[에디터 타임 — Octant 에셋 제작]
BP_OctantGenerator (Geometry Script) ──→ Octant 지각 Static Mesh
        + PCG (Octant Theme Sampler) ──→ 프랍 배치 → HISM Bake
        → Level Instance로 저장 → OctantPoolData에 등록

[런타임 — 결정론적 조립]
ALNPGameMode: OctantGenSeed 확정 → GameState 복제
ULNPOctantSpawnSubsystem: FRandomStream(seed)으로 풀에서 8개 선택
        → ALevelInstance × 8 스폰 (90°/180° 회전 배치) → 로드 감시 → 완료 브로드캐스트
```

### 핵심 구성 요소

| 클래스 | 모듈 | 역할 |
|:---|:---|:---|
| `ULNPOctantSpawnSubsystem` | Runtime | Level Instance 8개 스폰 + 로드 완료 감시 |
| `ULNPOctantPoolData` | Runtime | 사용 가능한 Octant 레벨 목록 Data Asset |
| `ALNPGameState::OctantGenSeed` | Runtime | 결정론적 스폰을 위한 복제 시드 |
| `ULNPOctantThemeSamplerSettings` | Runtime (PCG) | 테마 가중치 기반 프랍 배치 커스텀 PCG 노드 |
| `ULNPOctantThemeData` | Runtime | 테마별 프랍(메시·가중치·스케일) Data Asset |
| `ALNPOctantMeshGeneratorBase` | Editor | Geometry Script 기반 지각 메시 생성 + Save Mesh 에디터 확장 |

> 단일 PCG 볼륨 기반 전역 생성 방식은 스케일링 한계로 기각 (`ULNPSphereWorldSettings` 노드는 폐기 상태로 소스에 히스토리와 함께 보존). 상세 이력은 [DiscardedApproaches.md](DiscardedApproaches.md) 참조.

---

## 2. Octant 분할 전략

### 2.1 8분할 구조

전체 구체를 X, Y, Z 축 기준으로 8개의 균일한 조각으로 분할. 기준 Octant 하나를 원본으로 삼고 나머지 7개는 90°/180° 회전 배치로 구체를 완성한다.

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

### 2.2 Octant Pool — 결정론적 선택

`ULNPOctantPoolData`에 등록된 레벨 목록에서 `FRandomStream(OctantGenSeed)`으로 8개를 선택. **Fisher-Yates로 섞은 풀 전체를 한 배치로 삼아 8개가 모일 때까지 반복**하므로, 풀이 8개 미만이어도 동작하고 풀을 한 바퀴 소진하기 전까지는 같은 Octant가 중복되지 않는다.

```
OctantPool → [LevelA, LevelB, LevelC, ...]
                    ↓ FRandomStream (seed 고정) + Fisher-Yates 배치 셔플
              8개 선택 → 인덱스별 회전으로 배치
```

**결정론 보장 경로:** 서버 GameMode가 `OctantGenSeed`를 GameState에 쓰고 복제 → 클라이언트 `OnRep_OctantGenSeed`에서 동일 seed로 `StartWorldGeneration()` → 서버·클라 동일 배치. (상세: [TechDesign_InitSequence.md](TechDesign_InitSequence.md))

---

## 3. 런타임 스폰 흐름

`ULNPOctantSpawnSubsystem`이 Level Instance 스폰과 로드 완료 감시를 담당.

```
StartWorldGeneration()
├─ GameState->OctantGenSeed 읽기 (0이면 GameState 없는 환경 — 비결정론 랜덤 폴백)
├─ FRandomStream(seed)으로 8개 Octant 선택
├─ World->SpawnActor<ALevelInstance>() × 8 (AlwaysSpawn)
├─ LevelInstance->SetWorldAsset() + LoadLevelInstance()
└─ bIsGenerating = true → Tick() 감시 시작
        │ 매 프레임: 8개 모두 IsLoaded() && Level->bIsVisible 체크
        ▼
bIsGenerating = false, bGenerationComplete = true
OnWorldGenerationFinished.Broadcast()
```

> 로드 판정이 `IsLoaded()`만이 아니라 `bIsVisible`까지 확인하는 이유는 §6.1 참조.

---

## 4. Octant 내부 지형 구성

각 Octant Level 내부는 두 레이어로 구성된다. 모든 생성 비용은 **에디터 타임**에 지불하고, 런타임에는 완성된 Level Instance를 로드만 한다.

### 4.1 기본 지각 (Geometry Script — 에디터 타임)

`BP_OctantGenerator`(부모: `ALNPOctantMeshGeneratorBase`)가 구체 면을 Spherified Octant 방식으로 투영해 곡면 지각(Crust) 메시를 동적 생성. 조각 간 경계 왜곡과 틈새를 방지하는 핵심 단계다.

- Radius / Subdivisions / Magnitude / Frequency / Random Seed 파라미터로 지형 굴곡 제어.
- `FLNPOctantMeshGeneratorCustomization`(`IDetailCustomization`)이 Details 패널에 **Save Mesh** 버튼을 추가 — 프리뷰 결과를 Static Mesh 에셋으로 저장한다.

### 4.2 세부 지형 (PCG Layer — 에디터 타임)

`ULNPOctantThemeSamplerSettings` 커스텀 PCG 노드가 지각 위에 테마 기반 프랍을 배치한다.

**배치 파이프라인:**

1. **표면 샘플링:** 입력 Spatial Data(지각 메시)를 `PCGVolumeSampler`로 Point Cloud화.
2. **균등 방향 생성:** Octant 사분면(+X,+Y,+Z) 내에서 균등 분포 임의 방향 생성 (cos-weighted 구면 샘플링).
3. **내부→외부 투영:** 구 중심 쪽에서 시작하는 Ray로 `ProjectPoint()` — 두께 1m 지각의 **내벽**에 먼저 닿도록 보장.
4. **표면 정렬:** 메시 Z(Up)를 구 중심 방향으로 정렬(`FRotationMatrix::MakeFromZ`) + 랜덤 Yaw. Pivot은 중심 방향 50cm 오프셋으로 내벽 표면에 밀착.
5. **가중치 선택:** `ULNPOctantThemeData::PropEntries`의 Weight 비례 확률로 메시 선택, `MeshPath` Metadata로 후속 Static Mesh Spawner에 전달. 스케일은 Min/MaxScale 랜덤 보간.

### 4.3 Baking

최종 PCG 결과물을 HISM(Hierarchical Instanced Static Mesh)으로 구워 런타임 계산 제거. Nanite 적용으로 대규모 폴리곤 환경 구축.

> Octant 에셋 신규 제작 절차: [Guide_OctantLevelInstance.md](Guide_OctantLevelInstance.md)

---

## 5. 동기화 및 최적화 요약

| 항목 | 방법 |
|:---|:---|
| 결정론적 생성 | `OctantGenSeed` 복제 + `FRandomStream` + Fisher-Yates 배치 셔플 |
| 에디터 성능 | Level Instance 단위 작업, 필요 조각만 로드 |
| 런타임 성능 | 에디터 타임 HISM Baking + Nanite — 런타임 절차 생성 없음 |

---

## 6. 어필 포인트 (트러블슈팅 & 엔진 분석)

### 6.1 `IsLoaded()` ≠ 플레이 가능 — Level Instance 로드 판정의 함정

`ALevelInstance::IsLoaded()`는 레벨 **패키지** 로드만 보장할 뿐, `AddToWorld`(컴포넌트 등록·물리 씬 등록)의 완료는 보장하지 않는다. `AddToWorld`는 프레임 예산에 따라 여러 프레임에 걸쳐 분할 실행되므로, 중간 참여 클라이언트처럼 로드가 지연되는 상황에서는 **콜리전이 아직 없는 상태**로 후속 단계(표면 베이킹)가 시작되어 라인트레이스가 전부 빗나가는 버그가 발생했다.

**해결:** `ULevelInstanceSubsystem::GetLevelInstanceLevel()`로 실제 `ULevel`을 얻어 `bIsVisible`(AddToWorld 완료 신호)까지 확인한 뒤에만 완료로 판정.

### 6.2 PCG 파티션 Bounds 상속으로 인한 NumZ Overflow

Theme Sampler가 PCG 파티션 액터의 Bounds를 그대로 상속받으면 거대한 Z 범위 때문에 `ToPointData()` 내부에서 복셀 개수(NumZ) 오버플로우가 발생했다. **반지름 기반으로 클램프한 SafeBounds**를 만들어 `PCGVolumeSampler::SampleVolume()`을 직접 호출하는 방식으로 우회.

### 6.3 CDO에서도 동작하는 에디터 Detail 버튼

Blueprint 에디터에서 Details 패널을 열면 커스터마이제이션 대상이 인스턴스가 아닌 **CDO**인 경우가 있다. `FLNPOctantMeshGeneratorCustomization`은 대상이 CDO이면 `TObjectIterator`로 같은 클래스의 라이브 인스턴스를 찾아 `ReceiveSaveMesh()`를 호출하도록 처리 — 레벨 뷰포트/Blueprint 에디터 어느 쪽에서 눌러도 동작한다. Blueprint 이벤트 호출은 `FEditorScriptExecutionGuard`로 감싸 에디터 타임 실행을 허용.

### 6.4 시드 하나로 끝내는 월드 동기화

월드 전체를 복제하는 대신 **int32 시드 하나만 복제**하고 양쪽에서 동일한 결정론적 알고리즘(FRandomStream + 배치 셔플)을 실행. 대역폭 비용이 사실상 0이며, 조립 결과는 서버·클라이언트가 항상 일치한다.

---

## 7. 미구현 / 한계

- **Octant 풀 콘텐츠 부족:** 파이프라인은 완성됐으나 실제 제작된 Octant 테마 에셋 수가 적음. 콘텐츠 확충 필요.
- **런타임 지형 변형 미지원:** HISM Bake + SurfaceCache 사전 베이킹 전제상 게임 중 지형 파괴/변형은 지원하지 않음.
