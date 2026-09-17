# 월드 생성 시스템 기술 설계

## 1. 한눈에 보기

메인 월드는 거대한 구체의 **내부 표면**이 플레이 공간인 Dyson Sphere 구조다. 구체를 8개 Octant(1/8 조각)로 나누고 각각을 Level Instance로 관리한다.

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
| `ALNPGameState::OctantGenSeed` | Runtime | 결정론적 스폰용 복제 시드 |
| `ULNPOctantThemeSamplerSettings` | Runtime (PCG) | 테마 가중치 기반 프랍 배치 커스텀 PCG 노드 |
| `ULNPOctantThemeData` | Runtime | 테마별 프랍(메시·가중치·스케일) Data Asset |
| `ALNPOctantMeshGeneratorBase` | Editor | Geometry Script 지각 메시 생성 + Save Mesh 에디터 확장 |

> 단일 PCG 볼륨 기반 전역 생성은 스케일링 한계로 기각했다(`ULNPSphereWorldSettings`는 소스에 보존). 이력: [DiscardedApproaches.md](DiscardedApproaches.md)

---

## 2. Octant 분할 전략

### 2.1 8분할 구조

기준 Octant 하나를 원본으로, 나머지 7개는 회전 배치로 구체를 완성한다 (`LNPOctantSpawnSubsystem.cpp`).

| 인덱스 | 회전 (Pitch, Yaw, Roll) |
|:---:|:---|
| 0~3 | (0°, 0°/90°/180°/270°, 0°) — 0이 기준 Octant |
| 4~7 | (180°, 0°/90°/180°/270°, 0°) |

### 2.2 Octant Pool — 결정론적 선택

`FRandomStream(OctantGenSeed)`으로 **풀 전체를 Fisher-Yates로 섞은 배치를 8개가 모일 때까지 이어 붙인다.** 풀이 8개 미만이어도 동작하고, 한 바퀴 소진 전까지 중복이 없다.

**결정론 보장:** 서버 GameMode가 시드를 GameState에 쓰고 복제 → 클라이언트 `OnRep_OctantGenSeed`에서 같은 시드로 `StartWorldGeneration()`. (상세: [TechDesign_InitSequence.md](TechDesign_InitSequence.md))

---

## 3. 런타임 스폰 흐름

```
StartWorldGeneration()
├─ GameState->OctantGenSeed 읽기 (0이면 GameState 없는 환경 — 비결정론 랜덤 폴백)
├─ FRandomStream(seed)으로 8개 Octant 선택
├─ SpawnActor<ALevelInstance>() × 8 (AlwaysSpawn) → SetWorldAsset() + LoadLevelInstance()
└─ bIsGenerating = true → Tick()이 매 프레임 8개 모두 IsLoaded() && Level->bIsVisible 확인
        ▼
bGenerationComplete = true → OnWorldGenerationFinished.Broadcast()
```

`bIsVisible`까지 확인하는 이유는 §6.1.

---

## 4. Octant 내부 지형 구성

모든 생성 비용은 **에디터 타임**에 지불하고, 런타임에는 완성된 Level Instance를 로드만 한다.

### 4.1 기본 지각 (Geometry Script — 에디터 타임)

`BP_OctantGenerator`(부모: `ALNPOctantMeshGeneratorBase`)가 Spherified Octant 방식으로 곡면 지각(Crust) 메시를 만든다.

- Radius / Subdivisions / Magnitude / Frequency / Random Seed로 굴곡 제어.
- `FLNPOctantMeshGeneratorCustomization`이 Details 패널에 **Save Mesh** 버튼을 추가해 프리뷰를 Static Mesh 에셋으로 저장한다.

**생성 순서 (`CreateOctantPrimitive`):**

```
AppendBox([0,R]³, Subdivisions)
  → 법선각으로 -X/-Y/-Z 안쪽 3면 선택 → 삭제
  → CompactMesh
  → 구면 투영 (정점 정규화 × Radius)
```

⚠️ **안쪽 3면 삭제는 반드시 구면 투영 *전에* 한다.** 투영을 먼저 하면 X=0 면이 경계 호로 붕괴해 법선이 정의되지 않는다. 그러면 `SelectMeshElementsByNormalAngle`이 절반가량을 놓치고, 남은 삼각형이 이음매 평면에 두께 0인 판으로 남는다(실측: 전체 삼각형의 32%). `CompactMesh`는 삭제로 생긴 정점 인덱스 구멍을 메운다 — 뒤의 `Get/SetAllMeshVertexPositions`가 인덱스로 짝을 맞추기 때문이다.

**지각은 두께 없는 단면이다.** 예전의 100cm `ApplyMeshShell`은 제거했다(§6.5). 고속 관통 방어는 `bUseCCD`로 옮겼다(`ALNPLootDice` 생성자). 쉘 노드는 그래프에 남아 있어 필요하면 `FlipNormals`와 `RepairMeshDegenerateGeometry` 사이에 되꽂으면 된다.

⚠️ **구워낸 메시는 `CollisionTraceFlag = CTF_UseComplexAsSimple`이어야 한다.** SurfaceCache(NPC 접지)와 PCG 프랍 배치(§4.2)가 둘 다 단순 트레이스(`bTraceComplex=false`)를 쓴다. 컨벡스 헐이면 둘이 **같은 방향으로 함께 틀려** 프랍과 NPC 높이가 서로 맞으므로 원인 추적이 어렵다. 이 플래그는 블루프린트(`ReceiveSaveMesh()`)가 정해 C++이 보증하지 않는다 — 새 메시를 구울 때마다 확인한다.

### 4.2 세부 지형 (PCG Layer — 에디터 타임)

`ULNPOctantThemeSamplerSettings`가 지각 위에 테마 프랍을 배치한다.

0. **샘플 수:** 입력 Bounds의 최대 변을 반지름 R로 보고 `ceil(0.1 × π × R[m]² × SamplingDensity)`, `[1, 20000]` 클램프.
1. **방향 생성:** Octant 사분면(+X,+Y,+Z)에서 면적 균등 분포(φ ∈ [0, π/2], cosθ ∈ [0, 1] 각각 균등).
2. **내부→외부 라인트레이스:** 입력이 감싼 **지각 프리미티브 컴포넌트**에 구 중심에서 바깥으로 `LineTraceComponent`. 첫 히트가 플레이 표면이다. SurfaceCache와 같은 `bTraceComplex=false`를 쓴다(§6.6).
3. **표면 정렬:** 메시 Z를 구 중심 방향으로 정렬(`FRotationMatrix::MakeFromZ`) + 랜덤 Yaw. 접지는 **선택된 메시의 로컬 Bounds 최저점**(`-GetBoundingBox().Min.Z × Scale.Z`)만큼 Up으로 민다(§6.6).
4. **가중치 선택:** `PropEntries`의 Weight 비례로 메시를 고르고 `MeshPath` Metadata로 Static Mesh Spawner에 넘긴다. 스케일은 Min/MaxScale 랜덤 보간.

### 4.3 Baking

PCG 결과를 HISM으로 구워 런타임 계산을 없애고, Nanite로 대규모 폴리곤을 처리한다.

> 제작 절차: [Guide_OctantLevelInstance.md](Guide_OctantLevelInstance.md)

---

## 5. 동기화 및 최적화 요약

| 항목 | 방법 |
|:---|:---|
| 결정론적 생성 | `OctantGenSeed` 복제 + `FRandomStream` + Fisher-Yates 배치 셔플 |
| 에디터 성능 | Level Instance 단위 작업, 필요 조각만 로드 |
| 런타임 성능 | 에디터 타임 HISM Baking + Nanite — 런타임 절차 생성 없음 |

---

## 6. 어필 포인트 (트러블슈팅 & 엔진 분석)

### 6.1 `IsLoaded()` ≠ 플레이 가능

`ALevelInstance::IsLoaded()`는 **패키지** 로드만 보장한다. `AddToWorld`(컴포넌트·물리 씬 등록)는 프레임 예산에 따라 여러 프레임에 나눠 실행된다. 그래서 중간 참여 클라이언트처럼 로드가 늦으면 **콜리전이 없는 상태로** 표면 베이킹이 시작돼 트레이스가 전부 빗나갔다.

**해결:** `ULevelInstanceSubsystem::GetLevelInstanceLevel()`로 `ULevel`을 얻어 `bIsVisible`(AddToWorld 완료 신호)까지 확인한다.

### 6.2 PCG 파티션 Bounds 상속으로 인한 NumZ Overflow (지난 이력)

파티션 액터의 거대한 Z Bounds를 그대로 받아 `ToPointData()`에서 복셀 개수가 오버플로우했고, 당시에는 반지름으로 클램프한 SafeBounds로 `SampleVolume()`을 직접 호출해 우회했다. §6.6에서 복셀 샘플링을 걷어내며 이 우회도 사라졌다 — 지금 Bounds는 반지름 추정에만 쓰인다.

### 6.3 CDO에서도 동작하는 에디터 Detail 버튼

Blueprint 에디터의 Details 패널은 커스터마이제이션 대상이 **CDO**일 수 있다. 대상이 CDO면 `TObjectIterator`로 같은 클래스의 라이브 인스턴스를 찾아 `ReceiveSaveMesh()`를 호출하고, 호출은 `FEditorScriptExecutionGuard`로 감싼다.

### 6.4 시드 하나로 끝내는 월드 동기화

월드 대신 **int32 시드 하나만 복제**하고 양쪽이 같은 결정론적 알고리즘을 돌린다. 대역폭 비용이 사실상 0이다.

### 6.5 이음매 도랑 — 쉘이 경계에서 두께 0으로 오므라들었다

**증상:** Octant 경계를 따라 폭 약 2m·깊이 약 1m의 도랑. 그 위에서 Actor로 승격된 적 NPC가 가슴까지 묻혔다.

**원인:** 노이즈가 아니었다. 변위 마스크 `(X·Y·Z)/R³`는 좌표평면에서 0이라 경계를 정확히 R에 고정한다. 문제는 `ApplyMeshShell(OffsetDistance=100, bFixedBoundary=true)`였다. 경계 정점을 오프셋에서 제외하므로 쉘이 이음매에서 두께 0으로 오므라들고, **깊이 = 쉘 두께**인 도랑이 됐다.

⚠️ **`bFixedBoundary=false`는 해법이 아니다.** 경계 림 스티칭이 깨져 있어 퇴화 삼각형에 가려졌던 거대한 판이 드러난다(최악 오차 +141cm → +1670cm). 쉘 자체를 걷어냈다.

**근거:** 등장방형 격자 잔차가 세 좌표평면(적도 row 392, 경도 col 785 등)에서만 기준선의 5~13배로 튀었고, 단순/복합 콜리전이 같은 값을 줘 베이킹 아티팩트는 배제됐다. 수정 후 이음매 6곳 오차는 전부 ≈0(최대 −5cm).

### 6.6 PCG 프랍 접지 — 고정 오프셋은 전제가 바뀌면 오차가 된다

프랍이 발목~무릎 높이로, 프랍마다 다르게 떠 있었다. 원인은 둘이다.

1. **고정 `+50cm` 오프셋.** 두께 1m 지각을 전제한 상수라 §6.5로 두께가 사라지자 그대로 뜨는 높이가 됐다. 메시 로컬 Bounds 최저점으로 대체해 피벗 위치와 무관하게 붙는다.
2. **복셀 점군 투영.** 착지점이 `PCGVolumeSampler`의 200cm 격자에 얹혀 반지름 방향 최대 ±100cm의 결정론적 편차가 남았다.
   - 샘플 점은 **월드 원점 기준 축정렬 격자**에 고정돼 구면과 무관하다.
   - ⚠️ `UPCGBasePointData::ProjectPoint`는 최근접 탐색이 아니라 AABB에 걸린 점들의 **겹침 볼륨 가중 평균**이다.
   - ⚠️ 복셀 크기 축소는 답이 아니다 — `SampleVolume`은 박스 전체를 채워 50cm면 1.25억 개다.
   - ⚠️ `SpatialData->ProjectPoint()` 직접 투영도 안 된다 — 기반 구현이 `SamplePoint` 폴백이라 위치를 옮기지 않는다(2026-09-10 시도 → 프랍이 광선 시작점에 뭉침).

**해결:** PCG의 Actor 입력은 메시가 아니라 **콜리전 바디**(`UPrimitiveComponent` → `UPCGPrimitiveData`)다. 이미 가진 콜리전을 복셀로 열화시켜 쓰던 것이므로, 같은 컴포넌트에 `LineTraceComponent`를 쏴 양자화를 0으로 만들었다. 컴포넌트 한정이라 이미 배치된 프랍 HISM을 맞히지 않고, `FBodyInstance` 직행이라 메인 스레드 강제도 필요 없다.

---

## 7. 미구현 / 한계

- **Octant 풀 콘텐츠 부족:** 파이프라인은 완성됐으나 테마 에셋 수가 적다.
- **런타임 지형 변형 미지원:** HISM Bake + SurfaceCache 사전 베이킹 전제상 지원하지 않는다.
- **이음매 평탄화:** 마스크 `(X·Y·Z)/R³`는 옥턴트 중심에서도 최대 `1/(3√3) ≈ 0.19`라 조각 전체가 눌리고 이음매 근처가 넓게 평탄해진다(`Magnitude`를 키워 상쇄 중). 가장 가까운 이음매까지 거리로 `smootherstep`하는 마스크로 바꾸면 대부분에서 1.0이 되고 블렌드 폭을 직접 통제할 수 있다.
