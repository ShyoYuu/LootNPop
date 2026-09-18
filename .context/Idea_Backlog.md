# LootNPop Idea Backlog

이 문서는 **LootNPop** 프로젝트의 핵심 시스템 구축 이후, 추가 스펙으로 검토할 수 있는 자유로운 아이디어들을 기록합니다.

---

## [AI & 대규모 엔티티 관련]

### 1. 군집 폭주 유도 기믹 (Popcorn Kernel)
- **개요:** 구체 내벽 전역에 주기적으로 스폰되는 거대한 에너지 핵 오브젝트.
- **메커니즘:** 
    - 활성화 시 수백 마리의 `MassEntity`가 스마트 오브젝트(SO) 슬롯을 점유하기 위해 핵으로 쇄도.
    - 적들이 뭉쳐 거대한 덩어리를 형성했을 때 플레이어가 타격하면 일제히 사방으로 사출되는 연출.
- **핵심 기술:** Smart Object(Dynamic Slots), MassEntity, StateTree(Frenzy State).

### 2. 환경 인지 및 우회 시스템 (Hazard Awareness)
- **개요:** 지형 기믹(가스 분출구, 전기 지대)의 상태에 따라 AI가 경로를 동적으로 변경.
- **메커니즘:** 
    - 기믹 오브젝트의 런타임 태그(`State.Danger`)를 감지하여 MassEntity가 해당 구역의 SO 쿼리를 배제함.
- **핵심 기술:** Smart Object Runtime Tags, Mass Processor Query Filtering.

---

## [물리 및 환경 연출 관련]

### 3. 궤도 기반 중력 런처 (Orbital Launcher)
- **개요:** 구형 표면의 곡률을 이용해 캐릭터를 반대편으로 빠르게 사출하는 가속 장치.
- **메커니즘:** 
    - 진입 시 모션 워핑으로 정렬 후, Mover 컴포넌트에 원심력을 고려한 고속 커브 속도 주입.
- **핵심 기술:** Mover Plugin, Motion Warping, Custom Gravity Calculation.


---

## [전투 및 시스템 관련]

### 5. 거대화 및 넉백 체인 (Giant & Chain Reaction)
- **개요:** 특정 버프 획득 시 캐릭터가 거대해지며, 적들을 볼링핀처럼 연쇄적으로 날려버림.
- **메커니즘:** 
    - GAS 어빌리티를 통해 엔티티에 충격 전달 시, 충격을 받은 엔티티가 인접 엔티티에 다시 힘을 전달.
- **핵심 기술:** GAS, MassEntity Neighbor Query.

---

## [UI/HUD 관련]

### 루팅 게이지 HUD (2026-08-17 — 시안 미확정으로 보류)
- **개요:** 루팅 진행률을 보여주는 HUD 게이지. `ALNPLootPod::GetGaugePercent()`는 이미 복제되고 있으나
  ([TechDesign_LootPod.md](TechDesign_LootPod.md) §4.2), 이를 어떻게 보여줄지가 정해지지 않았다.
- **플레이어가 알아야 하는 것 4가지:** ① 얼마나 남았나 ② 지금 차는 중인가 줄어드는 중인가
  ③ 동료가 합류해 빨라졌나 ④ 존 밖으로 날아갔을 때 존이 어느 쪽인가.
- **배치 — 하이브리드:** 존 안에서는 Pod 위 월드 앵커 링, **존 밖으로 밀려나면 화면 고정으로 승격 +
  화면 가장자리에 존 방향 인디케이터**. 넉백으로 Pod가 화면 밖으로 나간 순간이 게이지가 가장 중요한
  순간인데 월드 앵커만 두면 그때 안 보인다 — "존 사수" 재미를 UI가 받쳐 주는 지점.
- **감쇠 고스트 아크:** 감쇠 속도가 상수(`LNPLootPodGaugeDecayFractionPerSecond`, 초당 MaxGauge의 0.15)라
  "0까지 몇 초"가 계산 가능하다. 링 위에 "N초 뒤 여기까지 줄어듦" 고스트 아크를 겹쳐 그리면 숫자를 읽지
  않아도 절박함이 보인다. **기준 시간 N = 존 경계까지 거리 ÷ 이동 속도** (클램프 [1초, 총 감쇠 시간]) —
  "지금 출발해서 도착했을 때 게이지가 얼마나 남아 있나"가 곧 "돌아갈 가치가 있나"라는 실제 판단과 대응한다.
  멀리 날아갈수록 간격이 저절로 길어져 "너무 멀리 왔다"가 그림으로 보인다. 넉백 중 속도 변화·구면 이동
  때문에 정확도는 낮지만, 정밀 예측이 아니라 위기감의 시각화이므로 오차는 낙관적이지 않은 쪽으로만 잡으면 된다.
- **인원 pip:** 존 안 인원수만큼 링 둘레에 작은 점을 켠다. `×2.4` 같은 배율 텍스트와 달리 **읽지 않고
  세어진다** — 전투 중 주변시로 잡힌다. 파티 규모 6인 이하 전제 (그 이상이면 숫자 표기가 낫다).
- **기여자별 아크 분할은 제외:** 기여자별 데이터 복제 비용이 붙는데(현재 `CurrentGaugePercent` 하나만 복제)
  "누가 얼마나 기여했나"는 협동 게임에서 경쟁심만 자극하고 플레이어의 판단을 바꾸지 않는다.
- **핵심 기술 (검토 시):** 커스텀 Slate 위젯 — 고스트 아크(요소 겹침)와 가변 개수 pip은 머티리얼
  파라미터로 못 한다. 반대로 **단순 원형 게이지 하나면 머티리얼(SDF Arc) + `UImage`가 더 싸다.**
  이 둘이 스펙에서 빠지면 커스텀 위젯을 만들 명분도 함께 빠진다.
  ([Guide_CustomSlateWidget.md](Guide_CustomSlateWidget.md), 선행 사례 `SLNPRadialCooldown`)
- **재검토 시점:** 위 요소들을 포함한 게이지 시안이 확정될 때.

---

## [월드 순환 관련]

### LootPod 리스폰 (2026-07-12 — 정규 스펙에서 제외)
- **개요:** 특정 구역의 Pod가 모두 소모되면 일정 시간 후 새로운 위치에 리스폰. (기존 [GameDesign_LootPod.md](GameDesign_LootPod.md) §5 스펙이었으나 제외 결정)
- **재검토 시점:** 세션 길이·월드 순환 구조가 확정된 뒤 — 리스폰 없이 "유한한 Pod를 다 까면 라운드 종료" 구조가 될 수도 있어, 게임 루프 확정 전에는 설계 불가.
- **핵심 기술 (검토 시):** ULNPMassSpawnSubsystem 결정론적 스폰 재사용, 구역별 소모 카운터, 리스폰 위치 선정(기존 SurfaceCache 투영).

---

### 적 NPC GAS 버프 지원 (2026-07-27 — 이번 범위에서 제외)
- **개요:** 플레이어와 마찬가지로 적 NPC도 GAS 버프/디버프를 받게 한다. 근처 적에게 거는 슬로우·방어 저하 등.
- **현재 상태 (조사 완료):** `ALNPEnemyCharacter`는 **이미 ASC + `ULNPBaseAttributeSet`을 보유**한다.
  그래서 High LOD 적에게 GE를 걸면 지금도 작동한다.
- **막히는 지점:** 적 Actor는 Low↔High LOD 전환마다 **풀에서 스폰/반납**된다. 권위 있는 Health는
  **Mass Fragment**에 있고 High LOD 활성화 때 `AttributeSet->SetHealth()`로 밀어넣는다. 따라서 적 ASC에 건
  GE는 **Actor가 풀로 돌아가면 소멸**하고, 재사용된 ASC가 다른 엔티티에 이전 GE를 흘릴 위험도 있다
  (풀 반납 경로의 `ClearAllAbilities()`는 어빌리티만 지운다).
- **지금도 가능한 것:** High LOD 한정·수 초짜리 단기 효과. 단, 풀 반납 시 활성 GE를 지우는 정리 코드는 필요.
- **작업 필요:** 지속 버프는 버프 상태를 **Mass Fragment에 저장**하고 High LOD 활성화 때 ASC에 재적용해야 한다
  — Health가 이미 하는 것과 동일한 패턴이라 범위는 명확하다.
- **재검토 시점:** 적에게 거는 상태이상(슬로우·방어 저하 등)이 기획으로 확정될 때.

---

## [월드 생성 관련]

### Mesh Terrain / Mesh Partition 도입 (2026-09-18 — 조사 완료, 전면 도입 보류)
- **개요:** UE 5.8 실험 기능 `MeshPartition`(내부명 MegaMesh) + `MeshTerrainMode`를 옥탄트 제작에 쓸 수 있는지 검토.
  현재 옥탄트 제작은 `BP_OctantGenerator`(Geometry Script)로 노브(Magnitude·Frequency·Seed)를 돌리는 **간접 생성**이라
  손으로 지형을 다듬을 수단이 없다.
- **맞는 부분:**
    - ⭐ **구면 기준 높이 스컬프트를 엔진이 정식 지원한다.** `EHeightSculptReferenceSurface { Plane, Sphere }`,
      브러시 영역 `CylinderOnSphere`(구 중심에서 브러시 지점을 지나 바깥으로 뻗는 원통), 스탬프 정렬 `ReferenceSphere`
      (법선 = 정규화(브러시위치 − 구중심), 구중심은 기즈모 지정). 즉 **반지름 방향 조각 브러시**가 이미 있다.
    - 베이스 레이어가 하이트필드가 아니라 **임의 `FDynamicMesh3`**(`UMeshProviderModifier::SetMesh`) — 구면 옥탄트를
      그대로 베이스로 넣을 수 있고, Convert Tool이 임의 메시를 MegaMesh로 변환한다.
    - 콜리전 규약이 우리 요구와 일치한다: `CollisionTraceFlag = CTF_UseComplexAsSimple`, `bDoubleSidedGeometry = true`를
      엔진이 기본 보증한다. 두께 없는 지각([TechDesign_WorldGeneration.md](TechDesign_WorldGeneration.md) §6.5)에 맞는다.
    - PCG 양방향 연동(`PCGMeshPartitionQuery`/`Write`/`SculptLayerWrite`, 투영 스포너)과 예제 그래프(침식·스캐터·도로).
- **정면 충돌하는 부분:**
    - ⚠️ **World Partition이 하드 요구사항이다.** 에디터·런타임·빌더 세 곳에서 막는다
      ("Mesh Partition requires a map with World Partition enabled"). 그런데 옥탄트 레벨
      `LVI_Octant_Meadow_00.umap`은 **비-WP 맵**이다(`TestMap03`만 WP). `MeshPartitionLevelInstanceAdapter`는
      방향이 반대다 — 바깥 WP 월드의 MegaMesh에 Level Instance 안의 **모디파이어를 기여**시키는 어댑터지,
      Level Instance 안에 지형을 담는 물건이 아니다.
    - **런타임 회전 배치·인스턴싱 불가.** 컴파일된 섹션은 월드 좌표로 구워진 액터다. 섹션의 StaticMesh조차
      독립 에셋이 아니라 **섹션 액터를 Outer로 갖는 내부 오브젝트**라, LVI 파이프라인에 쓰려면 에셋 복제 단계가 붙는다.
    - **시드 조립과 배타적.** 구체 전체를 하나의 MegaMesh로 만들면 이음매 문제(§6.5·§7)는 소멸하지만
      **매판 랜덤 조합이라는 게임 정체성을 잃는다.**
    - **오목면에서 위아래가 뒤집힌다.** 내벽에서 "위"는 구 중심 쪽인데 도구는 반대로 본다. 언덕은 낮춤(Ctrl)
      방향으로 그려야 하고, **SlopeErode(침식)는 중력이 바깥으로 향한다고 가정하므로 신뢰할 수 없다.**
      채널 UV의 `PlaneProject`도 구면에 못 쓴다(대안 VEUV는 실험 안의 실험).
- **규모가 안 맞는다:** `SphereRadius = 25,000cm` 기준 구 내부 전체 0.79km², 옥탄트 1개 약 0.098km²(≈313m×313m).
  MeshTerrain 예제 하이트맵이 1km·2km다 — **우리 월드 전체가 예제 타일 하나보다 작다.** 그리드 분할·WP 스트리밍·
  HLOD·FarField·플랫폼별 빌드 배리언트는 전부 필요 없고, **남는 가치는 오서링 도구뿐이다.**
- **권장(C안 — 도구만 차용):** 기존 옥탄트 Static Mesh에 Mesh Terrain Mode의 **Height Sculpt(Sphere)** 만 쓰고
  런타임 구조는 손대지 않는다. 근거: `UHeightSculptToolBuilder`는 `UMeshVertexSculptToolBuilder`를 상속할 뿐이고
  툴 내부에서 MegaMesh 타깃은 `Cast<UEditableModifierToolTarget>` **옵셔널 분기**로만 쓴다. 게다가 구면 정렬 로직은
  MeshPartition이 아니라 **범용 `MeshModelingToolset`**(`MeshBrushOpBase.h`, `MeshVertexSculptTool.cpp`)에 있다.
  → **단, 비-WP 레벨에서 평범한 Static Mesh를 대상으로 툴이 실제로 활성화되는지는 미검증이다.**
- **B안(하이브리드) — C안이 실패하면:** 테마별 WP 오서링 맵에서 Convert → 스컬프트 → 섹션 메시를 독립 에셋으로
  구워 기존 LVI에 투입. `BP_OctantGenerator`는 베이스 프리미티브 생성기로 축소된다.
- **검증 스파이크(순서대로 게이트):** ① 비-WP 옥탄트 레벨에서 Height Sculpt(Sphere)가 활성화되는가 →
  ② WP 오서링 맵에서 Convert + 섹션 메시 굽기(`CTF_UseComplexAsSimple`·이음매 R 보존 확인) →
  ③ 산출물로 SurfaceCache 베이킹 + PCG 프랍 배치 회귀(§6.5의 등장방형 격자 잔차 재측정) →
  ④ C안 확정 시 `MeshPartitionWater`·PCG 인터롭 2종은 **끈다**(Water 플러그인까지 딸려 온다).
- **현재 상태:** 관련 5개 플러그인이 `LootNPop.uproject`에 이미 활성화돼 있다. 전부 `IsExperimentalVersion`.
- **재검토 시점:** 옥탄트 풀 콘텐츠를 본격적으로 늘릴 때 — 제작 처리량이 병목이 되는 시점.

---

### 동굴 지형과 볼륨 기반 표면 샘플링 (2026-09-18 — 구상)
- **개요:** 내벽에 동굴/오버행 지형을 도입할 수 있는가. 지형 생성 자체보다 **표면 샘플링 규약**이 쟁점이다.
- **지형 생성 측면 — 유리하다:** 동굴은 Mesh Terrain이 Landscape 대비 가장 크게 이기는 지점이다. 주력은 스컬프트가
  아니라 **Boolean 모디파이어**이고, 불리언은 방향 개념이 없어 **오목면 부호 반전 문제가 적용되지 않는다.**
  Height Sculpt의 `bRequireConnectivity` 주석이 동굴을 예시로 들어 설명할 만큼 엔진이 이를 전제한다.
  단 **Geometry Script로도 불리언은 된다** — 얻는 건 "가능성"이 아니라 **비파괴 반복 속도**다.
- **구조적으로 깨지는 소비처 5곳:**

    | 위치 | 깨지는 방식 |
    |:---|:---|
    | `ULNPSurfaceCacheSubsystem` | 자료구조가 **방향 → 지점 1개**. 동굴이면 한 방향에 표면이 N개 |
    | `LNPProjectileMotion::IsUnderSurface` | `Pos.SizeSquared() >= SurfacePoint.SizeSquared()` — **동굴 안은 전부 "지하"**, 발사체가 스폰 즉시 폭발 |
    | `LNPProjectileMotion::PredictArc` | 같은 반지름 비교 → 조준 가이드가 동굴 천장을 지면으로 봄 |
    | `ULNPOctantThemeSamplerSettings` | 중심→바깥 트레이스의 **첫 히트를 플레이 표면으로 규정**(§4.2) → 프랍이 동굴 천장에 붙음 |
    | `ULNPMassSpawnSubsystem` | 스냅샷 기반 스폰 위치 → 같은 이유로 엉뚱한 층 |

- **그대로 살아남는 곳:** 적 공간 격자·적 탐색 질의(**이미 고도를 버린다** — `LNPEnemySpatialGrid.h` "셀은 위치의
  방향 성분으로만 정해진다". 브로드페이즈라 과다포함만 늘고 정합성은 유지) / 구형 중력·Mover(중력 방향은 동굴
  안에서도 −반지름) / 구면 자세 복제(동굴은 반지름이 **작아지는** 쪽이라 int16 캡을 안 건드림).
- **안 1 — 다층 표면 캐시(2.5D). 권장.** `방향 → 지점 1개`를 `방향 → (반지름, 법선) 몇 개`로 바꾼다. 구껍질 안의
  동굴은 본질적으로 **반지름 축을 따라 쌓인 층**이라 이 표현에 정확히 담긴다.
    - 베이킹 변경이 작다: `EAsyncTraceType::Multi`로 바꾸면 **발사 수는 그대로**고 결과만 여러 개다.
      프레임 분할 발사(§6.3)도 그대로 유효하다.
    - **락 프리 모델이 유지된다**(§6.2의 게임 스레드 단독 쓰기 → atomic 게시 → 불변). **재설계가 아니라 확장이다.**
    - 소비처 변경은 국소적이다: `GetSurfacePoint(Dir)` → `GetSurfacePointNear(Dir, CurrentRadius)`.
    - ⚠️ **진짜 난제는 보간이다.** 동굴 입구는 정확히 층이 생기거나 사라지는 경계라, 지금처럼 주변 4셀을 무조건
      바이리니어로 섞으면 §7이 경고한 "지면이 없는 중간 높이"가 재현된다. 순서를 **층 선택 → 그 다음 보간**으로
      뒤집고, 4셀의 층 수가 다를 때의 규약이 필요하다. **§7의 수직 단차 한계는 해결되지 않고 오히려 심해진다.**
- **안 2 — 진짜 볼륨(희소 복셀·3D 내비). 비권장.** NavMesh는 이미 기각돼 있고(Recast 전역 Z-up, §2), 수천 Mass
  엔티티의 워커 O(1) 조회를 3D로 다시 세우는 것은 별도 프로젝트다.
- **훨씬 싼 중간 지점:** **동굴을 "Mass NPC가 들어가지 않는 플레이어 전용 공간"으로 규정**한다. 표면 캐시·PCG·
  Mass 스폰을 전부 그대로 두고, 동굴 영역만 PCG 배치에서 제외하고 프랍은 수작업으로 놓는다. 깨지는 것은
  `IsUnderSurface` 하나뿐이라 "동굴 볼륨 안에서는 지하 판정을 끈다"는 태그 하나로 막힌다.
- **먼저 결정할 것:** 기술이 아니라 기획이다 — **동굴이 전투 공간인가.** 아니라면 안 1을 할 이유가 없다.

---

### 랜덤 조합 다양성 확대 — 런타임 생성보다 먼저 볼 것 (2026-09-18 — 구상)
- **개요:** 매판 랜덤 조합은 게임 정체성이고, 옥탄트는 그 **수단**이다. 수단을 런타임 다이내믹 메시 생성으로
  바꾸는 안을 검토했으나, 그 전에 훨씬 싼 선택지가 셋 있다.
- ⭐ **병목은 조합 메커니즘이 아니라 제작 처리량이다.** §7이 이미 진단하고 있다("Octant 풀 콘텐츠 부족 —
  파이프라인은 완성됐으나 테마 에셋 수가 적다"). 현재 옥탄트는 **1개**(`BP_Octant_Meadow_00`).
  현재 알고리즘(풀 셔플 후 8개)으로 서로 다른 월드 수는 풀 8개면 40,320 / **12개면 19,958,400** / 16개면 41억이다.
  **옥탄트 12개면 2천만 가지다.**
- **A. 풀을 늘린다.** 제작 처리량을 올리는 것이 정확히 위 Mesh Terrain 항목 C안이 값을 하는 지점이다.
- **B. 슬롯별 회전 자유도.** 지금은 인덱스별 회전이 고정이다. 옥탄트는 `(1,1,1)` 축 120° 회전에 대해 자기 자신으로
  매핑되고, 경계는 마스크 덕에 항상 정확히 반지름 R이라 이웃과 계속 맞는다. 슬롯마다 이 3가지를 시드로 뽑으면
  같은 에셋으로 **3⁸ = 6,561배**의 변형이 생긴다. → **기하학적 추론이고 미검증이다. 착수 시 경계 정합부터 볼 것.**
- **C. 테마를 슬롯별로 섞는다.** 지각은 그대로 두고 프랍 테마(`ULNPOctantThemeData`)만 교체한다.
- **런타임 생성안의 실제 장애물(조사 결과):**
    - ✅ **인프라는 이미 있다.** SurfaceCache가 지금도 런타임에 123만 발을 7초간 쏘고, 투-게이트 초기화와 로딩
      화면이 그것을 감싼다. "매치 시작 시 월드를 짓고 베이킹하는 단계"는 신규 항목이 아니다.
    - ✅ **PCG 런타임 생성은 엔진 정식 기능이다** — `EPCGComponentGenerationTrigger::GenerateAtRuntime`,
      `PCGRuntimeGenScheduler`, `PCGGenSourcePlayer`. 비-WP 레벨 파티셔닝도 지원된다.
    - ⚠️ **Nanite 빌더는 에디터 전용 모듈이다**(`NaniteBuilder.Build.cs` — "NaniteBuilder module is an editor module").
      다만 런타임에 새로 만드는 것은 **지각 메시 하나**고 프랍은 기존 Nanite 에셋을 인스턴싱하므로
      **손실은 지각 하나뿐**이다.
    - ⚠️ **진짜 위험은 결정론이다.** 서버·클라가 각자 같은 결과를 내야 하는데, Geometry Script 부동소수 일치는
      보증이 없고 **더 위험한 것은 PCG 프랍이다** — SurfaceCache가 `ECC_WorldStatic`을 트레이스하므로 **프랍도
      맞는다.** 프랍 배치가 1cm만 어긋나도 표면 캐시가 갈려 적 접지·발사체 지면 판정이 발산한다. 메시 복제는
      대역폭상 불가능하므로 시드 결정론에 전적으로 의존해야 한다.
    - 런타임 복합 콜리전 쿠킹 비용(수 초)이 SurfaceCache 위에 더 얹힌다.
- **전면 교체가 아니어도 된다:** SurfaceCache·PCG·Mass 입장에서 "런타임에 만든 지각"과 "LVI로 로드한 지각"은
  **똑같은 콜리전 표면**이다. 풀에 소스 종류를 하나 더하는 식으로 접근할 수 있고, 옥탄트 8개 중 하나만 런타임
  생성으로 섞어보는 것이 **결정론 리스크를 재는 가장 싼 방법**이다.
- **재검토 시점:** A·B·C를 다 하고도 다양성이 부족하다고 판단될 때.

---

## [메모 및 낙서장]
- [ ] 소셜 기능용 MVVM 기반 실시간 리더보드 연출.
- [ ] Iris를 활용한 수천 개 파편 데이터 최적화 동기화 실험.
### LootPod 단말기 상호작용 모션 워핑 (2026-07-10)
- **개요:** LootPod 상호작용(F키) 시 "단말기를 직접 조작한다"는 컨셉에 맞춰, 캐릭터를 단말기 앞 정위치·정방향으로 모션 워핑 정렬 후 조작 애니메이션 재생.
- **메커니즘:**
    - 상호작용 반경이 초근접(단말기 조작 거리)이므로 워핑 이동량이 작아 위화감 없음.
    - Pod 전방(현재 CanInteract 각도 조건의 기준 방향)에 워프 타겟 소켓 배치.
- **핵심 기술:** Motion Warping, 구면 중력 Up 벡터 보정 (기존 Orbital Launcher 아이디어와 동일 계열).

---

### 경계 상태 옆걸음 (Strafe) — 슬롯 대기 개체의 위협감 (2026-09-01)
- **개요:** 슬롯 경쟁에서 밀려 `Alert`로 대기 중인 적이 제자리에 서 있는 대신, **플레이어를 계속
  주시하면서 거리를 좁히지 않고 천천히 옆으로 도는** 동작. "포위당하고 있다"는 압박을 만든다.
- **범위:** `ULNPEnemyMovementProcessor`의 **`Alert` 분기 하나로 정의 가능**하다. StateTree·인지·자격
  판정은 손대지 않는다.
- **막히는 지점 (구조 변경 1건):** 현재 이동 방향이 곧 회전 방향이다 — Actor 경로
  (`CapturedMoveInput = OrientationIntent`)와 Entity 경로(`Velocity = OrientationIntent * Speed`)가
  같은 벡터를 쓴다. 옆걸음은 정의상 "보는 방향 ≠ 가는 방향"이라 **`MoveDirection` 지역변수를 분리**해야
  한다. Actor 경로는 `SetAIOrientationIntent`/`SetAIMoveInput`이 이미 별도 호출이라 그대로 지원된다
  (플레이어 ADS·가드 스트레이프와 같은 파이프라인).
- **핵심 기술:** `MoveDirection = Cross(TargetDirOnPlane, UpDir) * StrafeSign`. 접평면에서 타겟 방향과
  수직이므로 **반경 성분이 0 = 거리 보존이 공짜**다. 부호는 엔티티 인덱스 패리티로 뽑으면 새 Fragment
  필드 없이 무리 안에서 좌우가 섞인다 (매 프레임 랜덤이면 제자리에서 떤다).
- **지금이라야 안전한 이유:** 추격 자격이 플레이어의 Pod 거리만 읽게 바뀌어(→
  [TechDesign_EnemyNPC.md](TechDesign_EnemyNPC.md) §7.8) **`Alert`에서 무엇을 하든 상태 판정에
  되먹임이 없다.** 자격이 NPC 자기 위치를 읽던 시기였다면 옆걸음이 곧바로 경계선 진동을 만들었다.
- **알려진 제약:** 2026-09-07에 접평면 **분리력**이 들어와 정지 상태의 겹침은 해소됐지만
  (`ULNPEnemySeparationProcessor`), 예측 회피(CPA)는 없다. 서로 마주 보며 도는 동작에서는
  분리력만으로 부족할 수 있다 — 착수 시 여기부터 볼 것.
