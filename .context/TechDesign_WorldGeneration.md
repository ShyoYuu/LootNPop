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

**생성 순서 (`CreateOctantPrimitive`):**

```
AppendBox([0,R]³, Subdivisions)
  → 법선각으로 -X/-Y/-Z 안쪽 3면 선택 → 삭제
  → CompactMesh
  → 구면 투영 (정점 정규화 × Radius)
```

⚠️ **안쪽 3면 삭제는 반드시 구면 투영 *전에* 한다.** 투영을 먼저 하면 X=0 면 위의 모든 점이
같은 평면·같은 반지름으로 정규화되어 **2D 면이 1D 경계 호로 붕괴**한다. 그렇게 퇴화한 삼각형은
법선이 정의되지 않아 `SelectMeshElementsByNormalAngle`이 절반가량을 놓치고, 살아남은 것들이
**이음매 평면 안에 두께 0인 판**으로 남는다(실측: 전체 삼각형의 32%). 평평한 축정렬 박스 면일
때는 법선이 정확히 -X/-Y/-Z라 선택이 확실하다. 삭제가 남기는 정점 인덱스 구멍은 `CompactMesh`로
메운다 — 바로 뒤의 `GetAllVertexPositions`/`SetAllMeshVertexPositions`가 인덱스로 짝을 맞추기 때문이다.

**지각은 두께 없는 단면이다.** 예전에는 `ApplyMeshShell`로 100cm 쉘을 씌웠으나 제거했다
(§6.5). 쉘 두께에 기대던 고속 관통 방어는 `bUseCCD`로 옮겼다 — `ALNPLootDice` 생성자 참조.
`ApplyMeshShell` 노드 자체는 실행 체인에서만 빠진 채 그래프에 남아 있어, 두께가 다시 필요해지면
`FlipNormals`와 `RepairMeshDegenerateGeometry` 사이에 되꽂으면 된다.

### 4.2 세부 지형 (PCG Layer — 에디터 타임)

`ULNPOctantThemeSamplerSettings` 커스텀 PCG 노드가 지각 위에 테마 기반 프랍을 배치한다.

**배치 파이프라인:**

1. **표면 샘플링:** 입력 Spatial Data(지각 메시)를 `PCGVolumeSampler`로 Point Cloud화.
2. **균등 방향 생성:** Octant 사분면(+X,+Y,+Z) 내에서 균등 분포 임의 방향 생성 (cos-weighted 구면 샘플링).
3. **내부→외부 투영:** 구 중심 쪽에서 시작하는 Ray로 `ProjectPoint()`.
4. **표면 정렬:** 메시 Z(Up)를 구 중심 방향으로 정렬(`FRotationMatrix::MakeFromZ`) + 랜덤 Yaw.
   접지는 **선택된 메시의 로컬 Bounds 최저점**(`-GetBoundingBox().Min.Z × Scale.Z`)만큼 Up으로 밀어 맞춘다 — §6.6.
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

### 6.5 이음매 도랑 — 쉘이 경계에서 두께 0으로 오므라들었다

Octant 경계를 따라 **폭 약 2m·깊이 약 1m의 도랑**이 파여 있었다. 적 NPC가 그 위에서 Actor로
승격되면 가슴까지 묻힌 채 멈췄다.

원인은 노이즈가 아니었다. `ApplySeamAwarePerlinNoise`의 변위 마스크 `(X·Y·Z)/R³`는 세 좌표평면
위에서 정확히 0이라 경계를 반지름 R에 정확히 고정한다 — **규약을 정확히 지키고 있었다.**
깨진 곳은 `PostProcessTerrainGeometry`의 `ApplyMeshShell(OffsetDistance=100, bFixedBoundary=true)`
였다. `bFixedBoundary=true`는 경계 정점을 오프셋에서 제외하므로, 쉘이 내부에서는 100cm 두께인데
**이음매에서 두께 0으로 오므라든다.** 플레이 표면(안쪽 면)이 경계에서 원래 패치(= 정확히 R)로
되돌아 나오면서 그 차이가 그대로 도랑이 됐다 — **깊이 = 쉘 두께**.

⚠️ **`bFixedBoundary=false`로 뒤집는 것은 해법이 아니다.** 도랑은 사라지지만 `ApplyMeshShell`의
경계 림 스티칭이 이 메시에서 깨져 있어, 퇴화 삼각형으로 가려져 있던 **거대한 판**이 드러난다
(무작위 20,000방향 최악 오차 +141cm → +1670cm). 쉘 자체를 걷어내는 것이 답이었다.

**측정 근거:** 이음매를 각도로 훑으면 매끈한 대칭 깔때기가 정확히 `SphereRadius`에서 바닥을 쳤고,
등장방형 격자 잔차는 세 좌표평면(적도 row 392 = lat 0.000°, 경도 col 785 = lon 180.000° 등)에서만
기준선의 5~13배로 튀었다. 무효 셀은 0건이고 단순/복합 콜리전이 항상 같은 값을 줘서 베이킹
아티팩트가 아님이 배제됐다. 수정 후 이음매 6곳의 캐시↔실측 오차는 전부 ≈0(최대 −5cm)이 됐다.

### 6.6 PCG 프랍 접지 — 고정 오프셋은 전제가 바뀌면 그대로 오차가 된다

프랍이 지면에서 발목~무릎 높이로 떠 있었고, 뜬 높이가 프랍마다 달랐다. 원인이 둘이다.

1. **고정 `+50cm` 오프셋.** "두께 1m 지각의 내벽에 밀착"시키려고 넣은 상수였다. §6.5로 두께가
   사라지자 **전제가 없어진 보정이 그대로 뜨는 높이**가 됐다. 지금은 선택된 메시의 로컬 Bounds
   최저점으로 대체했다 — 바닥 피벗 메시는 0이 되어 그대로 붙고, 중심 피벗 메시는 반높이만큼
   올라오며, 나무·바위로 교체해도 다시 어긋나지 않는다.
2. **투영 대상이 복셀 점군이다.** `ProjectPoint`가 실제 메시 표면이 아니라 `PCGVolumeSampler`가
   만든 `SamplingVoxelSize`(기본 200cm) 격자 점군에 스냅하므로 착지점이 양자화된다. 프랍마다
   뜬 높이가 다른 편차의 정체이며, 1번을 고쳐도 남는다(§7).

---

## 7. 미구현 / 한계

- **Octant 풀 콘텐츠 부족:** 파이프라인은 완성됐으나 실제 제작된 Octant 테마 에셋 수가 적음. 콘텐츠 확충 필요.
- **런타임 지형 변형 미지원:** HISM Bake + SurfaceCache 사전 베이킹 전제상 게임 중 지형 파괴/변형은 지원하지 않음.
- **PCG 프랍 접지 잔차:** 투영 대상이 200cm 복셀 점군이라 착지점이 양자화된다(§6.6-2). 복셀을 줄이면
  점 개수가 세제곱으로 늘어 현실적이지 않고, 입력 Spatial Data에 직접 투영하도록 바꾸는 편이 맞다.
- **이음매 평탄화:** 변위 마스크 `(X·Y·Z)/R³`는 옥턴트 중심에서도 최대 `1/(3√3) ≈ 0.19`까지만 오른다.
  세 이음매에서 곱으로 감쇠하므로 조각 전체가 눌리고 이음매 근처가 넓게 평탄해진다(`Magnitude`를
  키워 상쇄 중). 가장 가까운 이음매까지의 거리 하나로 `smootherstep`하는 마스크로 바꾸면 조각
  대부분에서 1.0이 되고 블렌드 폭을 직접 통제할 수 있다.
