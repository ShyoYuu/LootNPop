# Phase 3 — MassWorldCollision 정확성 기준선 기록

> 상태: 진행 중
> 실행 문서: `../phases/Phase03_MassWorldCollisionBaseline.md`

## 2026-09-23 — 착수 전 설계 검토와 반영

### 검토 방법

Surface Support·Navigation 문서 전체를 프로젝트 소스(SurfaceCache와 소비자, Enemy·투사체 프로세서, 옥탄트 스폰, 소스 수집기, collision 설정)와 UE 5.8 엔진 소스(SceneQuery, Chaos 씬 락, LevelInstance, Mover)에 대조했다.

### 엔진 확인 사항

- Mass worker의 동기 scene query는 씬 읽기 락을 잡으므로 thread-safe다. 비동기 trace는 `check(IsInGameThread())`로 게임 스레드 전용이다.
- 프로젝트는 `bTickPhysicsAsync=True`라 게임 스레드 밖 query는 보간된 GT data를 본다.
- 로컬 스폰된 `ALevelInstance`는 머신마다 무작위 GUID로 내부 레벨 패키지 이름을 만든다. LVI 내부 Actor는 서버와 클라이언트 경로가 달라 복제되지 않는다.
- Mover는 movement base 컴포넌트 참조를 네트워크로 직렬화한다.
- 엔진에 custom spatial acceleration으로 표준 query를 실행하는 경로가 있다.

상세 근거는 `../research/ChaosSceneQueries.md`와 `../design/DynamicTerrain.md` §1에 있다.

### 결정

`../Decisions.md`에 D-025~D-031을 추가했다.

- D-025 지면 조회는 Support snapshot 기본, exact는 세 조건에서만, 스레드별 API 규칙
- D-026 LVI에는 정적 지형과 Placement Marker만, 동적 요소는 서버 스폰 복제 Actor
- D-027 동적 지형은 결정론적 kinematic 움직임
- D-028 조건부 보행면은 Conditional Patch로 사전 베이크
- D-029 stale 검출은 cook·CI, 런타임은 `DataVersion`만
- D-030 옥탄트 이음매 단일 대칭 프로필, 섬·동굴·마커는 경계를 넘지 않음
- D-031 모든 Phase에 2P 스모크

### 로드맵 조정

- Phase 3에 worker query Gate 0, 마커→서버 스폰 패널 스파이크, 투사체·탄도 가이드 exact 전환을 포함했다. 예상 2세션 → 3세션.
- Phase 4를 4a(지각·이음매)와 4b(섬·동굴 다층)로 나누고, 공통 전제로 fixture 재배치·fixture LVI·greybox 옥탄트를 두었다.
- Pod 재귀속을 Phase 6에서 Phase 7로 옮겼다. NavComponent와 path cost가 Phase 7에서 생기기 때문이다.
- legacy SurfaceCache 제거를 Phase 6 완료 조건으로, 슬롯 도달성을 Phase 7로 배정했다.

### 문서 정합성 수정

- `RuntimeCollision.md`의 `LNP_MassWorld`·`LNP_DynamicSupport`를 D-022 channel로 교체했다.
- `SurfaceBaking.md`의 입력 의미 표와 material slot·vertex attribute 서술을 D-021 기준으로 교체했다.
- `DynamicTerrain.md`의 "정적 셀 무효화"를 Conditional Patch 비활성화로 교체했다. `TerrainContract.md`의 정적 payload 제외 규칙과 모순이었다.
- NavComponent 정의 충돌을 StaticNavComponent와 ReachabilityGroup으로 분리해 해소했다.
- 런타임 source hash 검증 단계를 제거했다.

### 코드에 남은 후속 작업

- `LNPOctantSourceCollector`는 collision이 켜진 무태그 컴포넌트를 조용히 건너뛴다. `TerrainContract.md` §3의 보고 규칙과 §8의 차단 오류에 맞춰 수정해야 한다.
- `LNPWorldDeviceSpawnSubsystem.h`와 `../../TechDesign_WorldDevice.md` §6의 "라인트레이스는 게임 스레드 전용" 서술은 Gate 0 검증 뒤 정정한다.

## 2026-09-23 — 설계 자체에 대한 2차 검토 반영

### 결정

- D-006 대체: 지하 공간은 재사용 공동 모듈과 통로 조각의 동굴 키트로 만든다(D-035). 내부형 구에서 땅속은 바깥쪽이고 지각은 두께 없는 단면이라, 지각 한 장으로는 동굴 천장과 그 위 지면을 동시에 표현할 수 없다.
- D-025 보강: 관찰 거리 축을 추가했다. 원거리 개체는 risk 구간에서도 거친 지지면을 쓰고 정확성 필수 전환만 exact로 처리한다. PureEntity도 worker 동기 exact를 사용한다.
- D-032: Support 캐시 전에 exact 전용 부유섬 프로토타입으로 재미와 exact 한계치를 실측하고, 캐시 도입 뒤 같은 시나리오로 재측정한다.
- D-033: 대규모 추격 경로는 목표별 flow field와 계층형 A*를 모두 구현해 실측 비교한다.
- D-034: 베이커 핵심 계산은 runtime 모듈의 순수 함수로 둔다.

### 로드맵 조정

- Phase 3 뒤에 Phase 3b(exact 전용 부유섬 프로토타입)와 Phase 3c(완전 비행 NPC, 기존 Phase 11)를 넣었다. 11번은 결번이다.
- Phase 6 완료 조건에 3b 시나리오 재측정을 추가했다.
- Phase 10을 "대규모 추격 경로: flow field와 계층형 A*"로 바꿨다.

### 근거로 확인한 사실

- 현재 런타임 베이크 약 7초는 트레이스 발사를 프레임당 3000개로 제한해 약 412프레임에 나눈 시간이다. 에디터 베이크를 택한 근거는 로드 시간보다 제작 시점 오류 피드백과 무거운 분석이다.
- 목표 구성에서 적의 90% 이상은 PureEntity이고 ActorPromoted는 소수 엘리트에만 적용한다. 게임 스레드가 움직이는 물리 body가 적어, 락 경합보다 query 자체의 CPU 비용이 한계를 정할 가능성이 높다.
- 단순 octahedral 사상은 옥탄트 중심 셀이 꼭짓점보다 면적 약 5.2배, 선 길이 약 2.3배 크다.
- 폭 20m 평면 공동 바닥은 가장자리가 약 20cm 높고 경사 약 2.3°다. 구면 캡 바닥을 제작 가이드로 두었다.

### 1차 반영분 정정

- 최외곽 반지름 안전망의 기준을 지각 반지름에서 옥탄트 geometry 최대 반지름으로 바꿨다. 동굴은 지각보다 바깥쪽으로 파고들기 때문이다.

## 2026-09-23 — 전체 문서 정밀 검토 반영

### 결정

`../Decisions.md`에 D-036~D-044를 추가하고 D-033을 D-044로 대체했다.

- production `LNPWorldExact` response 마이그레이션을 Phase 3 Gate -1로 앞당김
- face/instance identity를 포함하는 worker-safe exact hit registry
- 원거리 risk·edge의 보수적 경계 처리
- Support proxy exact association과 Destructible profile 역할 분리
- A* chord heuristic과 request/cache versioning
- Conditional Patch의 base lattice 활성화 데이터 규약
- 완전 비행 NPC Phase 3c를 Phase 4a 선행 조건에서 분리
- 옥탄트 선택을 greedy에서 최대 고유 결정론적 제약 할당으로 교체
- flow field·계층형 A*는 최소 프로토타입 비교 뒤 채택 방식만 production 통합

### 문서 보강

- Phase 3 실행 문서를 `../phases/Phase03_MassWorldCollisionBaseline.md`로 작성하고 Gate -1·Gate 0을 독립 완료 조건으로 만들었다.
- component 단독 Layer 역매핑을 제거하고 collision face·ISM instance 기반 identity 계약을 추가했다.
- 동적 패널의 tick prerequisite, path revision, server epoch와 late join 검증을 추가했다.
- Conditional Patch 겹침 합성, source hash, base tile 병합 규약을 구체화했다.
- A* 다중 프레임 request가 snapshot/connectivity/tile revision 변경을 검출하도록 했다.
- Phase 7을 7a 데이터 기반과 7b 경로 실행 내부 게이트로 나눴다.
- Phase 10은 두 방식을 제품 수준으로 완성하기 전에 동일 harness의 최소 프로토타입으로 비교하도록 바꿨다.

### 현재 코드와의 명시적 인계

- C-option 테스트 에셋은 구형 `LNP.Terrain.*` tag를 사용하므로 Phase 4 입력 전에 마이그레이션해야 한다.
- `LNPOctantSourceCollector`는 무태그 충돌 component, LVI 내부 동적 역할, tag/profile 응답 검증을 아직 Terrain Contract 수준으로 차단하지 않는다.
- `LNPOctantSpawnSubsystem`은 완료 시 Level Instance 배열을 비우므로 slot→Loaded Level 참조 보존이 필요하다.
- 현재 SurfaceData payload는 `TArray<uint8>`라 header와 stream의 선택적 I/O가 아니라 전체 asset 로드다.

## 2026-09-23~24 — Gate -1 A: production exact response audit·마이그레이션

### 도구

- `LNP.SurfaceNav.AuditExactResponse [all]` (`Source/LootNPop/SurfaceNavigation/LNPExactResponseAudit.cpp`, 비-Shipping): 현재 월드의 query 충돌 component를 Pawn 응답과 `LNPWorldExact` 응답으로 분류한다(MISSING·ExactOnly·Ok·NonBlocking). shape가 없는 component는 `NoBody`로 따로 센다. ISM·HISM은 instance body를 쓰므로 별도로 판정한다.
- `LNPCollisionChannels.h`: `SurfaceSupport`·`WorldExact` channel 상수.

### 최초 audit (`TestMap03` PIE, 8 slot)

Ok=0. 모든 world geometry가 `BlockAll`/`BlockAllDynamic`이라 custom channel 기본값(Ignore)을 따랐다. exact 경로로 전환하면 전부 관통하는 상태였다.

| 대상 | 이전 | 이후 |
|:---|:---|:---|
| 지각 `SM_Octant_00` (`BP_Octant_Meadow_00` SCS) | BlockAll | `LNPStaticTerrain` |
| 프랍 HISM Cone 1106·Cube 580·Cylinder 278 (`PCG_Octant_BaseProps` Mesh Selector `TemplateDescriptor`) | BlockAll | `LNPStaticBlocker` |
| `ALNPSpringLauncher::MeshComponent` (C++ 생성자) | BlockAllDynamic | `LNPStaticTerrain` (D-045) |
| `BP_LNPLootPod` 메시 | BlockAll | 미해결 — Gate -1 B의 collision proxy로 처리(D-047) |
| `BP_LNPLootPod.PillarBeam` (30m 빔 실린더) | BlockAllDynamic | `NoCollision` |

- `DefaultEngine.ini`에 빠져 있던 `LNPDestructibleSupport`·`LNPDestructibleBlocker` profile을 추가했다(D-039).
- 재 audit: 지각 8개, 스프링 런처 40개 Ok. 프랍 HISM은 저장된 LVI component의 BodyInstance에서 `LNPStaticBlocker`와 `LNPWorldExact` Block을 직접 확인했다.

### 함정

- `LVI_Octant_Meadow_00`은 One File Per Actor다. actor 데이터는 `__ExternalActors__/.../E/D5/LH4LVIE62RWPVNDJUSEM7X.uasset`에 있다. `SceneTools.save_actor`는 이 경로를 찾지 못해 실패했고, 레벨 저장 버튼으로 저장했다.
- PCG 그래프의 Mesh Selector를 바꿔도 LVI의 PCG component는 재생성되지 않았다. component `Seed`를 바꿨다 되돌리는 방식(43→42)으로 재생성을 일으켰다.
- `ObjectTools.set_properties`로 `BodyInstance.collisionProfileName`만 바꾸면 `collisionEnabled`는 기존 값이 남는다. profile을 바꿀 때 `collisionEnabled`도 함께 지정한다.
- 작업 전 working tree에 출처를 알 수 없는 `LVI_Octant_Meadow_00.umap` 변경이 있어서 저장소 상태로 롤백한 뒤 작업했다.

### 결정

- D-045 서버 스폰 정적 장치, D-046 기준 반지름 30,000cm, D-047 Mass 기반 상호작용 오브젝트 collision proxy
- int16 위치 복제 캡은 좌표 성분마다 걸린다. 제약은 옥탄트 꼭짓점(좌표축) 부근에만 걸리므로, 인코딩을 바꾸지 않고 제작 규칙과 베이커 검사로 처리한다(`../design/TerrainContract.md` §7).

### 남은 Gate -1 A 항목

- 8-slot line/sphere/capsule oracle, exact/legacy 비교 CVar: MassWorldCollision wrapper 이후
- audit의 ISM 판정 수정은 다음 에디터 재시작 빌드부터 적용된다(정적 콘솔 커맨드는 Live Coding으로 반영되지 않음). 적용 뒤 PIE audit에서 HISM instance 수 1106/580/278 유지를 확인한다.

## 2026-09-24 — Gate -1 B: slot 참조·hit identity 추출·LootPod collision proxy

### 구현

- `ULNPOctantSpawnSubsystem`: 완료 시 slot 순서의 `ALevelInstance`·`ULevel` weak ref를 보존한다. `GetSlotLevel(Slot)`, `FindSlotForLevel(Level)`.
- `LootPod/LNPLootPodCollisionProxy.*`(D-047): `ULNPLootPodCollisionProxySubsystem`이 hidden ISM(`/Engine/BasicShapes/Sphere` ×2.56 = r128, `LNPStaticBlocker`, `SetRemoveSwap`)을 소유한다. `FLNPLootPodCollisionProxyTag`가 없는 Pod를 게임 스레드 프로세서가 추가하고, `FLNPLootPodTag` Remove observer가 제거한다. index→`FMassEntityHandle` 표와 `Generation`.
- `BP_LNPLootPod`·`ALNPLootPod::MeshComponent`: `NoCollision`.
- `LNP.SurfaceNav.ProbeHitIdentity [Distance]`(비-Shipping): 시점 방향 `LNPWorldExact` line trace, `bReturnFaceIndex`. component·profile·source level·slot·`Item`·`FaceIndex`·proxy 엔티티·PodID·generation을 로그로 남긴다.
- 지난 세션 audit ISM 판정의 const 컴파일 오류 수정(`UStaticMesh::GetBodySetup` 사용).

### 설계 정정

- ISM 기본 제거는 `RemoveAt`(뒤 index 전부 이동)다. swap 전제는 `SetRemoveSwap()`이 있어야 성립한다(`InstancedStaticMesh.cpp` `RemoveInstanceInternal`, physics body도 같이 swap).
- `PodID`는 서버 전용·비복제라 클라이언트 proxy는 PodID로 매핑할 수 없다. 사용자 결정으로 index→엔티티 핸들(머신 로컬)로 바꾸고 식별자 복제는 추가하지 않았다(`../design/TerrainContract.md` §2).
- 서버는 엔티티 생성 뒤에 transform을 채운다(`SetupSpawnedEntities`). 그래서 추가는 생성 observer가 아니라 태그 조회 프로세서로 한다.

### 검증 (`LootNPopEditor Win64 Development` 빌드 성공, TestMap03)

- audit(1P PIE): `MISSING=0 ExactOnly=0`, Ok=76. HISM 1106/580/278이 8 slot 모두 유지되고, proxy ISM 117 instance가 Ok다. Current의 직전 다음 작업 1번 완료.
- probe, 리슨 서버 2P PIE(사용자 조준):

| 대상 | 결과 |
|:---|:---|
| 지각 | `Slot=6`, `FaceIndex=72819`, `Item=-1`, UpDot=1.000 |
| HISM Cube | `Slot=4`, `Item=568`, `FaceIndex=-1` (단순 충돌이라 face 없음) |
| 스프링 런처 | Persistent, `Slot=-1`, `LNPStaticTerrain` |
| Pod A(서버) | `Item=8` → entity 17, PodID 9, Generation 118 |
| Pod B(서버) | `Item=75` → entity 151, PodID 76 |
| Pod(클라이언트) | `Item=72` → entity 78, `PodID=0`, Generation 119, instance 117 |
| Popped 자리(서버·클라이언트) | proxy를 관통해 뒤쪽 Cone HISM에 hit |

- 캐릭터(Mover)가 proxy에 막힌다(사용자 확인).
- 투사체는 Pod를 관통한다. 현재 투사체는 world query 없이 SurfaceCache 반지름 판정(`IsUnderSurface`)만 쓰므로 프랍도 관통한다. 회귀가 아니며 Phase 3 구현 단위 2(투사체 exact 전환)의 범위다.
- FaceIndex는 trimesh(complex-as-simple) 지각에서만 나오고, 단순 충돌 shape(HISM 프랍·proxy 구)는 `-1`이다. 이 shape의 identity는 `Item`이 담당한다.

### 남은 Gate -1 B 항목

- swap으로 옮겨진 인스턴스의 재해석 검증: 마지막 index의 Pod가 제거된 index로 옮겨진 뒤 같은 엔티티로 해석되는지. 수동 조준보다 subsystem 자동화 테스트가 확실하다.
- immutable registry 구축·게시, worker POD 결과, static/dynamic/미등록 분류, lifecycle gate
- disconnected sheet face identity와 double-sided shell normal: 회귀 fixture가 필요하다.
- `-game` 리슨 서버 2P 스모크(D-031)

## 2026-09-24 — Gate -1 B: hit identity registry·proxy swap 검증

### 구현

- `SurfaceNavigation/LNPHitIdentityRegistry.*`: `ULNPHitIdentitySubsystem`(tickable world subsystem, `TMassExternalSubsystemTraits` GameThreadOnly=false). slot Level source는 profile 분류로 일괄 등록하고, 런타임 source는 소유자가 직접 등록하며, LootPod proxy 표는 게시마다 복사한다. 불변 snapshot 게시, `FLNPExactHitIdentity` POD 결과, `Unknown` counter. 계약은 `../design/RuntimeCollision.md` hit identity registry 절에 있다.
- `ALNPSpringLauncher`: BeginPlay·EndPlay에서 `MeshComponent`를 런타임 source로 등록·해제한다.
- `LNP.SurfaceNav.ProbeHitIdentity`: registry 해석 결과 한 줄을 추가로 남긴다.
- 자동화 `LootNPop.SurfaceNavigation.HitIdentity.LootPodProxySwapRemap`: 임시 Game 월드에서 Pod 4개를 넣고 중간 index를 제거한 뒤, 옮겨진 Pod가 ISM transform·물리 trace `Item`·registry 해석 모두에서 같은 엔티티인지 확인한다. 게시 전 불변, generation, 중복 추가, 게시 전 취소, 미등록·범위 밖 Item의 `Unknown`과 counter도 확인한다.

### 설계 정정 — proxy 반영 시점

지난 구현은 `RemoveInstance`를 요청 즉시 호출했다. ISM 제거는 physics body index를 바로 swap하는데 snapshot 표는 다음 게시에 바뀌므로, 그 사이 worker query가 옮겨진 Pod의 `Item`을 제거된 Pod 엔티티로 해석한다. 이제 `AddPod`·`RemovePod`는 큐에만 쌓고, `ULNPHitIdentitySubsystem::Tick`이 게시 직전에 `ApplyPendingChanges`를 호출해 반영한다. proxy 충돌 생성·소멸은 최대 1프레임 늦어진다(`../design/TerrainContract.md` §2).

### 검증

- `LootNPopEditor Win64 Development` 빌드 성공, 경고 없음. 새 헤더는 `Mass/EntityHandle.h`·`Mass/ExternalSubsystemTraits.h`(MassCore)를 쓴다.
- `LootNPop.SurfaceNavigation` 자동화 12/12 통과(신규 1, 기존 11).
- 리슨 서버 2P PIE(TestMap03, 사용자 조준):

| 확인 | 결과 |
|:---|:---|
| 등록(서버·클라이언트) | `32 slot sources from 8 slot levels (0 unclassified)` — 지각 8 + HISM 3종×8 |
| 지각(서버) | `Static SB Slot=7 Face=57706` |
| Cylinder HISM(서버) | `Static -B Slot=7 Instance=64` |
| 스프링 런처(서버) | `Static SB Slot=-1` |
| Pod(서버) | `Static -B Instance=39 Entity=79`, proxy 표 해석과 일치 |
| Pop 뒤 서버 | instance 119→118, 다른 Pod `Instance=57 Entity=115` 일치 |
| Pop 뒤 클라이언트 | instance 117→116, `Instance=76·88`이 엔티티로 해석 |
| `UnknownHits` | 전 probe 0 |
| 캐릭터(사용자 확인) | Pod에는 막히고, Pop으로 Pod가 사라진 자리는 통과한다. 큐 지연 반영 뒤에도 proxy 수명이 유지된다 |

- 클라이언트 registry generation(16~19)이 서버(7~8)보다 높다. 스프링 런처 복제 BeginPlay가 여러 프레임에 걸쳐 등록돼 게시가 더 잦기 때문이며, 해석 결과에는 영향이 없다.
- 클라이언트에서 지각·HISM·런처 probe는 하지 않았다. slot 등록 수가 서버와 같다.

### 남은 Gate -1 B 항목

- match reset·stream unload lifecycle gate: slot Level 목록 변경 시 재수집은 구현했지만 실제 reset·unload 시나리오로 검증하지 않았다.
- 동적 패널 분류: 구현 단위 3 이후.
- disconnected sheet face identity와 double-sided shell normal: 회귀 fixture가 필요하다.
- `-game` 리슨 서버 2P 스모크(D-031).
