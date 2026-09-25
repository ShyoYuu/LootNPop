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

## 2026-09-24 — 부하 시나리오 합의와 Gate 0 스파이크

### 부하 시나리오 (사용자 합의)

적 300·1000 두 단계, PureEntity 90%/ActorPromoted 10%, 동시 투사체 500발(무기 DA 속도 그대로), Development `-game` 리슨 2P, warm-up 10초·capture 30초, 성공 기준 60fps·exact 합계 P95 ≤ 2ms/frame·락 대기 P95 ≤ 0.2ms/frame. 원본은 `../phases/Phase03_MassWorldCollisionBaseline.md` §4다.

### 구현 (`SurfaceNavigation/LNPExactQuerySpike.*`, 비-Shipping 스파이크)

- `ULNPExactQuerySpikeProcessor`: PrePhysics, 게임 스레드 강제 없음, entity 순회 없음(`ShouldAllowQueryBasedPruning=false`). 고정 Fibonacci 방향 3072개를 `k % 3`으로 line·sphere(r50)·capsule(r40 hh90) 동기 `LNPWorldExact` 질의로 순환하고 `ULNPHitIdentitySubsystem::ResolveHit`(snapshot 정적 함수)으로 해석한다. `ParallelFor` 64개 단위.
- 락 대기: 질의 직전 `Solver->GetExternalDataLock_External()`의 `ReadLock/ReadUnlock` 시간을 따로 잰다. 쿼리 내부 대기를 직접 분리할 수 없는 설치형 엔진에서의 근사다.
- `ALNPExactSpikeMover`: 게임 스레드 TG_PrePhysics에서 `LNPDynamicTerrain` kinematic cube(300cm)를 질의 방향 위에서 ±100cm 진동시킨다. 런타임 source로 registry에 등록해 Dynamic 분류 정답으로 쓴다.
- `ULNPExactQuerySpikeSubsystem`: 통계, 결과 표(해시·`Saved/ExactSpike/Table_NetMode*.csv`), 자동 캡처.
- CVar `LNP.SurfaceNav.ExactSpike.{QueriesPerFrame, MovingBodies, Parallel, LockProbe, AutoCapture}`, 명령 `.Report`·`.Reset`.
- `LootNPop.Build.cs`: private `Chaos` 의존성(락 계측).
- 새 파일이 unity 묶음을 바꾸면서 `LNPLootPodCollisionProxy.cpp`와 테스트의 익명 네임스페이스 상수 `PodProxyCenterUp`이 충돌했다. 테스트 쪽을 `TestPodProxyCenterUp`으로 바꿨다.

### 측정 (1024 q/frame, 동적 body 62, Ryzen 7 8845HS 16 논리 코어)

| 조건 | Line P50/P95 | Sphere P50/P95 | Capsule P50/P95 | 프레임 query 합 P50/P95 | 락 probe 쿼리당 P95 | 프레임 락 합 P95 | 프레임 wall P95 |
|:---|:---|:---|:---|:---|:---|:---|:---|
| PIE 리슨 서버(2P, 한 프로세스) | 4.0/7.9us | 8.4/15.7us | 9.0/16.0us | 8.1/10.3ms | 5.8us | 5.7ms | 3.2ms |
| PIE 클라이언트 | 4.0/9.2us | 8.4/18.7us | 9.0/17.7us | 8.4/10.7ms | 10.8us | 5.9ms | 3.2ms |
| 에디터 바이너리 `-game` 리슨 서버(`-corelimit=4`, 1800프레임) | 3.6/7.0us | 7.9/14.3us | 8.5/14.9us | 7.5/9.1ms | 5.8us | 1.65ms | 4.75ms |
| 에디터 바이너리 `-game` 클라이언트(141프레임, 참고용) | 3.8/7.5us | 8.2/14.9us | 8.8/15.9us | 7.8/9.9ms | 0.2us | 0.17ms | 10.2ms |

- 모든 조건에서 processor는 worker에서만 실행됐다(`gamethread=0`). ensure·assert 0, `UnknownHits=0`, `ClassificationErrors=0`. 동적 body hit는 전부 Dynamic으로 해석됐다.
- 결과 표 비교(3072칸): PIE 서버↔클라이언트, `-game` 서버↔클라이언트, PIE↔`-game`의 모든 쌍에서 **양쪽이 모두 옥탄트 slot을 맞힌 칸의 차이는 0**이다. 차이 2~13칸은 모두 slot=-1 런타임 source(LootPod proxy 1cm 차이, 스프링 런처 가장자리를 스치는 sweep)이며, 실행마다 다른 배치이거나 복제 transform 양자화로 설명된다.
- 비용 해석: 질의 1회 평균 약 8~9us(3종 평균)라 합계 P95 ≤ 2ms 기준에서는 프레임당 약 200~220회가 한도다. 에디터 빌드 기준이며 패키지 값으로 다시 확인한다.
- 락 probe가 PIE에서 큰 것은 에디터 락(`RWFIFO_CRITICALSECTION`)이 읽기끼리도 내부 critical section을 거치기 때문으로 보인다. 병렬 worker끼리 경합한다.

### 발견 — `-game`도 에디터 락이다

`CHAOS_SCENE_LOCK_TYPE`은 `WITH_EDITOR`로 갈린다(`Chaos/Public/Framework/Threading.h`). 프로젝트의 `Standalone_*.lnk`는 `UnrealEditor.exe -game`이라 `WITH_EDITOR=1`이고, 락도 PIE와 같은 RWFIFO다. 게임 락(`FRWLOCK`) 조건의 락 대기는 **패키지 빌드에서만** 측정할 수 있다.

### 막힘 — 패키지 실행이 옥탄트 로드에서 멈춤

Win64 Development BuildCookRun(`Saved/SurfaceNavigationPhase3Package`)은 성공했고 `LVI_Octant_Meadow_00`도 cook 에셋 레지스트리에 있다. 그러나 `LootNPop.exe TestMap03?Listen`은 `Spawned 8 LevelInstances. Waiting for load...` 뒤 10분 넘게 로그가 없었다(프로세스는 응답). `ULNPOctantSpawnSubsystem::Tick`의 `IsLoaded()`·`bIsVisible` 조건이 만족되지 않는 것으로 추정한다. 런타임 스폰 `ALevelInstance`의 cooked 로드 경로는 Phase 2에서 검증하지 않았다(Phase 2 packaged 검증은 SurfaceData 자동화 테스트뿐). Gate 0의 게임 락 측정은 이 문제를 해결한 뒤로 미룬다.

## 2026-09-24 — Gate 0 패키지 측정·판정과 구현 단위 1

### 패키지 측정 (게임 락 `FRWLOCK`)

패키지 옥탄트 로드 문제(`SetWorldAsset` 에디터 전용)가 해결돼 `LootNPop Win64 Development` BuildCookRun(`Saved/SurfaceNavigationPhase3Package`)으로 다시 측정했다. 조건은 에디터 `-game` 측정과 같다: `TestMap03?Listen`, `-corelimit=4`, `QueriesPerFrame 1024`, `AutoCapture 1`(warm-up 10초·capture 30초), Ryzen 7 8845HS. 판정은 로그로 했다(`Saved/Logs/gate0_*`).

| 조건 | Line P50/P95 | Sphere P50/P95 | Capsule P50/P95 | 프레임 query 합 P50/P95 | 락 probe 쿼리당 P50/P95 | 프레임 락 합 P50/P95 | 프레임 wall P95 | 프레임 수 |
|:---|:---|:---|:---|:---|:---|:---|:---|:---|
| 리슨 서버 + 클라이언트 동시, body 62 — 서버 | 4.3/11.5us | 9.8/22.4us | 10.4/23.2us | 9.8/15.1ms | 0.1/17.5us | 2.27/5.40ms | 9.1ms | 845 |
| 같은 실행 — 클라이언트(참고) | 4.1/11.8us | 9.1/23.0us | 9.7/23.3us | 9.6/14.0ms | 0.1/17.9us | 2.60/5.97ms | 9.0ms | 156 |
| 서버 단독, body 62 | 3.5/8.8us | 8.1/17.2us | 8.7/17.8us | 8.0/10.1ms | 0.1/14.6us | 1.83/3.35ms | 6.0ms | 1710 |
| 서버 단독, body 0 | 3.1/5.7us | 7.2/12.5us | 7.7/13.1us | 6.7/7.8ms | 0.1/0.1us | 0.084/0.105ms | 4.0ms | 1607 |

- 모든 실행에서 `gamethread=0`, ensure 0, `UnknownHits=0`, `ClassificationErrors=0`.
- 결과 표 비교: 패키지 서버↔클라이언트, 패키지↔에디터 `-game` 서버·클라이언트의 모든 쌍에서 양쪽이 옥탄트 slot을 맞힌 칸의 차이 0. 차이 2~7칸은 전부 한쪽이 slot=-1인 런타임 source 칸이다(에디터 측정과 같은 설명).
- **락 대기는 게임 스레드 쓰기 겹침이 만든다.** 쓰기가 없으면 1024 q/frame에서도 프레임 락 합 P95 0.105ms로 부하 기준(0.2ms)을 만족한다. 같은 PrePhysics 구간에 kinematic body 62개를 teleport하면 3.35ms로 약 30배가 되고 query 자체도 20~30% 느려진다. 쿼리당 P50은 0.1us 그대로이고 꼬리(P95 14.6us ≈ query 1회 시간)만 커진다. 쓰기 락이 진행 중인 읽기를 기다리고, 대기 중인 쓰기 뒤에 새 읽기가 서는 패턴으로 해석한다.
- 에디터 `-game` 서버(RWFIFO, body 62)의 1.65ms보다 패키지 쪽 겹침 비용이 크다. 락 구현 차이를 따로 분리하지는 않았다. 결론(겹침을 피하라)은 두 락에서 같다.
- 동시 실행 클라이언트는 30초에 156프레임만 돌았다. 한 머신 두 프로세스의 CPU 경합으로 보이며 참고값으로만 둔다.
- 처리량: 쓰기 없는 조건의 query 1회 평균 약 6.5us(프레임 합 P50 6.7ms/1024)라 합계 2ms 예산에서 약 300회/frame, 쓰기 겹침 조건에서는 약 250회/frame이 한도다.

### Gate 0 판정 — 통과

worker 실행, ensure 부재, 머신·빌드 간 결과 일치, 락 대기 분리 측정을 패키지(게임 락)까지 확인했다. D-025(Mass worker는 동기 query)를 재논의할 실패는 없다. 락 대기가 쓰기 겹침에 비례한다는 결과는 배치 제약으로 남긴다.

- 동적 패널 transform tick은 Mass exact query phase 전에 끝나게 배치한다(구현 단위 3 체크리스트).
- ActorPromoted 적·플레이어 Mover의 이동 쓰기 겹침 비용은 구현 단위 4 부하 기준선(적 100 Actor 포함)에서 측정한다.

### 구현 단위 1 — MassWorldCollision API

- `ULNPMassWorldCollisionSubsystem`(`SurfaceNavigation/LNPMassWorldCollision.*`): `RaycastWorld`·`SweepSphereWorld`·`SweepCapsuleWorld`·`ProbeSupport`. 모든 스레드에서 호출 가능, `TMassExternalSubsystemTraits` `GameThreadOnly=false`.
- 입력 `FLNPWorldQueryParams`: 분류(`ELNPWorldQueryClass` 6종)와 제외 Actor unique ID. 결과 `FLNPWorldHit`: POD + `FLNPExactHitIdentity`(snapshot 정적 `ResolveHit`).
- 분류별 count·hit·총시간·최대시간·락 probe 합을 atomic으로 모으고 필수/선택 합계를 나눠 보고한다. `LNP.SurfaceNav.WorldCollision.{DebugDraw, LockProbe, Report, Reset}`.
- `ProbeSupport`: 호출자 Up 기준 `StepUp→Drop` 구 sweep. walkable 법선이고 registry 역할에 `Support`가 있으며 시작 관통이 아니어야 지지면.
- 자동화 `LootNPop.SurfaceNavigation.WorldCollision.Api`: 식별·miss·제외 Actor·sphere/capsule 정지 위치·바닥/벽 지지 판정·미등록 Unknown counter·ParallelFor worker 256회 결과 일치와 분류 counter 분리. `LootNPop.SurfaceNavigation` 13/13 통과.
- Gate 0 뒤 정정하기로 한 "라인트레이스는 게임 스레드 전용" 서술을 `LNPWorldDeviceSpawnSubsystem.h`와 `../../TechDesign_WorldDevice.md`에서 고쳤다.
- 남은 것: debug draw 화면 확인, Gate 0 스파이크 processor를 wrapper로 옮길지는 투사체 전환(구현 단위 2)에서 실제 소비자가 생길 때 판단한다.

## 2026-09-24 — 구현 단위 2: 투사체·Ghost·탄도 가이드 exact 경로

### 구현

- `LNPProjectileMotion::TraceWorld`: `RaycastWorld`(`ProjectileMandatory`)를 감싼 월드 착탄 판정 단일 정의. `UseExactWorldCollision()`은 CVar `LNP.SurfaceNav.ProjectileExact`(0=legacy 기본, 1=exact)를 읽는다.
- `ULNPProjectileHitDetectionProcessor`: 서버·Ghost 두 분기 모두 프레임 선분의 월드 hit을 먼저 구하고 캐릭터 판정 선분을 `PreviousPos → ImpactPoint`로 자른다. 캐릭터에 안 맞았으면 월드 hit 지점에서 폭발·스플래시(중심 `ImpactPoint`, VFX는 법선 방향으로 띄움). exact 경로의 수명 만료는 공중 폭발.
- `PredictArcExact`: 스텝 선분마다 `TraceWorld`, 첫 hit의 `ImpactPoint`가 착탄점. legacy `PredictArc`와 적분·재표집 루프(`SimulateArc`)를 공유한다. `ULNPTrajectoryGuideComponent`가 CVar로 둘 중 하나를 고른다.
- 형상은 line으로 정했다(sphere 아님). 근거와 패링 반사 한계는 `design/RuntimeCollision.md` "투사체 월드 충돌".
- unity 빌드 묶음이 바뀌며 `LNPExactQuerySpike.cpp`와 `LNPMassWorldCollision.cpp`의 익명 네임스페이스 `CyclesToNs`(반환형 다름)가 충돌했다. 후자를 `WorldCollisionCyclesToNs`로 바꿨다.

### 검증

- `LootNPopEditor`·`LootNPop Win64 Development` 빌드 성공, 경고 없음.
- 자동화 `LootNPop.SurfaceNavigation.WorldCollision.ProjectileArc` 통과. 첫 실행은 fixture를 원점 근처에 둬서 실패했다 — 중력이 원점에서 바깥쪽이라 원점 가까이서는 옆으로 크게 휘어 궤적이 바닥을 벗어났다. fixture를 원점에서 1km 떨어뜨려 해결. 기존 13개는 같은 실행에서 통과.
- 에디터 바이너리 `-game` 리슨 2P(`TestMap03?Listen`, `-corelimit=4`), 사용자 플레이 테스트:
  - CVar 1: 호스트·게스트 모두 소총탄이 배경 프랍을 관통하지 않음, 프랍 뒤 적에게 피해 없음, 유탄이 프랍에 막혀 그 자리에서 폭발.
  - CVar 0: 호스트·게스트 모두 기존대로 프랍을 통과(legacy 회귀 없음).
- 호스트 `LNP.SurfaceNav.WorldCollision.Report`(CVar 1 약 4분): `ProjectileMandatory count=40027 hits=1141 avg=2.08us max=306.50us`, 합계 83.3ms(약 167 q/s, 프레임당 0.1ms 미만), `UnknownHits=0`. 두 로그 모두 ensure·assert 0. 게스트 Report는 확인하지 않았다.
- max 306us는 단발 이상치로 보고 부하 기준선(구현 단위 4) P95에서 다시 본다.

### 남은 구현 단위 2 항목

- world collision envelope 최외곽 반지름 안전망.
- 8-slot oracle 통과 뒤 기본값 전환과 `IsUnderSurface`·legacy 분기 제거(D-036).

## 2026-09-24 — 구현 단위 2 완료: envelope 안전망, 8-slot oracle, exact 기본 전환

### world collision envelope 안전망

- `FLNPExactSourceEntry::MaxRadius`를 등록 시점에 계산(`ULNPHitIdentitySubsystem::ComputeSourceMaxRadius`)하고, 게시 때 최댓값을 snapshot `WorldEnvelopeRadius`로 둔다. LootPod proxy는 제외.
- 처음 떠올린 component world bounds 꼭짓점은 지각 한 장이 옥탄트 전체를 덮어 반지름의 약 √3배(25,000cm면 약 43,000cm)가 된다. 탈출탄이 18,000cm를 더 날아야 걸리는 안전망은 의미가 없어 cooked trimesh 정점을 직접 본다.
- `TestMap03` production 8 slot: envelope **26,928cm**(기준 반지름 25,000cm), 수집 6~8ms(1회).
- 투사체(서버·Ghost): 월드 hit 없이 `반지름 > envelope + 500cm`면 VFX·스플래시 없이 DeadTag, `EnvelopeEscapes` counter. `PredictArc`도 같은 스텝에서 끝낸다. `LNP.SurfaceNav.WorldCollision.Report`에 `EnvelopeRadius`·`EnvelopeEscapes` 한 줄 추가.
- 자동화 `WorldCollision.ProjectileArc`에 envelope 값(바닥 아랫면 모서리)과 탈출 궤적이 경계 한 스텝 안에서 끝나는 사례를 추가했다.

### 8-slot oracle

- `LNP.SurfaceNav.ExactOracle`(`SurfaceNavigation/LNPExactOracle.cpp`). 판정 항목은 `design/ValidationAndMigration.md` "production 8-slot exact oracle".
- 첫 실행 `FAIL`: line Miss 827, SlotMismatch 94. 둘 다 결함이 아니었다.
  - Miss는 전부 좌표 평면(이음매) 위에 **정확히** 놓인 방향이었다. Fibonacci·"평면 옆" 표본으로 분류된 것도 좌표를 보면 다른 좌표 평면 위였다(예: `(-0.00017, 0, 1)`은 X=0에서 떨어져 있지만 Y=0 위). 같은 방향 sphere는 모두 r≈25,000에서 맞았다.
  - 평면에서 ±0.0001°(0.04cm)·±0.001°(0.4cm) 표본을 넣자 off-plane miss는 0. 이음매에 실제 틈은 없고, 두 body의 공유 모서리를 정확히 따라가는 ray의 측도 0 누락이다. 이를 정보 항목 EdgeMiss로 분리했다.
  - SlotMismatch는 한 slot만 `slot=-1`(persistent level 런타임 source: 런처·앵커·LootPod proxy)을 먼저 맞은 방향이었다. seed 배치라 대칭이 아니므로 비교에서 제외.
- 최종(에디터 바이너리 `-game`, `TestMap03`, 방향 63,008개 × 3형상 = 189,024 query, 90~140ms):

| 실행 | Miss L/S/C | Unknown | StartPen | ShapeOrder | ExactDeeper | SlotMismatch(제외) | EdgeMiss | 지형 \|exact−legacy\| P50/P95/max |
|:---|:---|:---|:---|:---|:---|:---|:---|:---|
| 단독 | 0/0/0 | 0 | 0 | 0 | 0 | 0 (94) | 835 | 0.1/2.7/104.4cm |
| 리슨 서버(NetMode 2) | 0/0/0 | 0 | 0 | 0 | 0 | 0 (150) | 835 | 0.1/2.7/104.4cm |
| 클라이언트(NetMode 3) | 0/0/0 | 0 | 0 | 0 | 0 | 0 (130) | 834 | 0.1/2.7/114.6cm |

- 판정: **PASS**. Gate -1 A의 oracle 항목 완료.

### exact 기본 전환과 legacy 제거(D-036)

- audit(MISSING=0)와 oracle 통과로 조건 충족. CVar `LNP.SurfaceNav.ProjectileExact`, `UseExactWorldCollision`, `IsUnderSurface`, SurfaceCache 기반 `PredictArc`, 판정 프로세서의 legacy 분기(`IsTerminated`·`ImpactVfxPos`)와 SurfaceCache 의존을 제거했다. exact 판 `PredictArcExact`가 `PredictArc`가 됐다.
- 탄도 가이드는 SurfaceCache 베이크 완료를 옥탄트 로드 신호로만 계속 쓴다(Phase 5에서 옥탄트 생성 완료로 교체).
- `ULNPOctantSpawnSubsystem::OctantRotations`를 public으로 옮겼다(oracle이 slot 회전을 쓴다).

### 검증

- `LootNPopEditor`·`LootNPop Win64 Development` 빌드 성공, 경고 없음.
- `LootNPop.SurfaceNavigation` 자동화 14/14 통과.
- 위 oracle 3회 PASS(legacy 제거 전 빌드). 제거 뒤 빌드로 헤드리스 리슨 2P를 다시 돌려 서버·클라이언트 PASS(EdgeMiss 837). 제거 뒤 2P 플레이 스모크는 아직이다.

## 2026-09-24 — exact 기본 2P 플레이 스모크와 관전 Ghost 외삽 구간

### 사용자 플레이(에디터 바이너리 `-game` 리슨 2P, `TestMap03`)

- 호스트·게스트 모두 프랍·지면에서 투사체가 막힘, ADS 가이드 착탄점 문제 없음.
- Report: 호스트 `ProjectileMandatory count=25913 hits=980 avg=2.29us max=138.90us`, `UnknownHits=0`, `EnvelopeEscapes=0`. 게스트 `count=40480 hits=1781 avg=1.87us max=226.70us`, `UnknownHits=0`, **`EnvelopeEscapes=1`**.
- 게스트에서 가끔 투사체가 공중에서 사라짐. 사용자 가설은 대역폭 때문에 발사 신호가 유실됐다는 것이다.

### 분석

- 발사 방송 `Multicast_SpawnSpectatorGhosts`는 Reliable이라 대역폭 때문에 유실되지 않는다. 유실됐다면 Ghost가 사라지는 게 아니라 처음부터 안 보인다. 가설은 기각했다.
- 결함: 관전 Ghost는 Dead Reckoning으로 `SpawnPos`에서 최대 200ms 외삽한 위치에 스폰되고 `PreviousPos`도 그 위치다. 외삽 구간이 월드 판정을 한 번도 받지 않아, 그 사이의 지면·프랍 너머에서 Ghost가 생긴다. 지면 너머면 지각 아래를 날다가 envelope 밖에서 조용히 사라지고(게스트 `EnvelopeEscapes=1`과 부합), 프랍 너머면 계속 날다가 서버 확정 큐에 지워진다(공중에서 사라지고 VFX는 프랍 쪽). legacy에서는 `IsUnderSurface`가 지면 아래 스폰을 즉시 폭발시켜 드러나지 않았다.
- VFX 없이 Ghost가 사라지는 다른 경로는 서버의 예측 키 거부(`DestroyAllGhostsForKey`)와 envelope 이탈뿐이다. 서버 확정 큐가 살아 있는 Ghost를 지울 때는 서버 위치에 VFX가 뜬다.

### 조치

- `SpawnSpectatorGhosts`: 외삽 구간 `SpawnPos → ExtrapolatedPos`를 `TraceWorld`로 검사하고, 월드에 맞으면 Ghost를 만들지 않는다. 엔티티 예약은 검사 뒤로 옮겼다.
- `DestroyAllGhostsForKey`(서버 거부 때만 호출)에 로그 한 줄: `Ghost: prediction key %d rejected by server`.
- `LootNPopEditor`·`LootNPop Win64 Development` 빌드 성공.

### 재확인(사용자 2P 플레이)

- 게스트 화면에서 호스트 탄이 지면·프랍을 통과하거나 공중에서 사라지는 현상 없음. 게스트 자신의 탄 소실도 없음.
- 게스트 Report: `ProjectileMandatory count=813 hits=55 avg=13.42us max=57.10us`, `UnknownHits=0`, `EnvelopeEscapes=0`. 양쪽 로그에 `prediction key ... rejected`·ensure 없음.
- 짧은 세션이라 표본이 적고, 평균 13us는 1차 세션(1.87us)보다 높다. 비용은 구현 단위 4 부하 기준선에서 P95로 다시 본다.


## 2026-09-24 — 구현 단위 3: Placement Marker와 움직이는 패널

### 구현 (`Source/LootNPop/DynamicTerrain/`)

- `FLNPPlacementId`(`(slot, MarkerId)`), `ILNPPlacedElement::InitializeFromMarker`(deferred 스폰 중 FinishSpawning 전에 호출 → 초기 스폰 번치에 실림).
- `ALNPPlacementMarker`: 루트가 `USplineComponent`(경로 = 마커 로컬), NoCollision·비복제. `MarkerId`는 `PostActorCreated`에서 발급, 에디터 복제(`PostDuplicate` Normal)·붙여넣기(`PostEditImport`)는 새로 발급, PIE 복제는 유지.
- `ALNPMovingPanel`: 복제 Actor, `ReplicatedMovement` 끔, `bAlwaysRelevant`. 초기 복제 `FLNPPanelPath`(마커 원점 위치·회전, 마커 스플라인 위치 곡선 `FInterpCurveVector`, 속도, 끝 정지 시간, `Revision`, 시작 server time). 양쪽이 같은 곡선에서 등속 거리 표(구간당 16샘플)를 만들어 `Pose(serverTime)`을 계산한다. 열린 경로는 왕복, 닫힌 루프는 순환. 회전하지 않고 평행 이동만 한다. 루트 `LNPDynamicTerrain` 400×400×30cm.
- `ULNPDynamicTerrainSubsystem`: 공통 서버 스폰 `SpawnPlacedActor`(월드 장치 앵커·런처도 이 함수로 옮김), `SpawnFromMarkers`(slot 0~7 Level + persistent level, 중복 MarkerId 경고), DynamicSupport POD snapshot 게시.
- registry: `RegisterRuntimeSource(Component, Placement, MaxRadiusOverride)`. 패널은 `(slot, MarkerId)`와 경로 swept 반지름을 넘긴다. `FLNPExactHitIdentity::MarkerId` 추가.
- `ALNPGameMode::OnSurfaceBakingComplete`에서 `SpawnDevices` 다음에 `SpawnFromMarkers`.
- 콘텐츠: `LVI_Octant_Meadow_00`에 마커 1개(지면 →7.85m 상승 → 15m 수평, 300cm/s, 끝 정지 2초). 8 slot 모두 패널이 생긴다.
- 진단: `LNP.DynamicTerrain.LogRiders N`(패널 근처 Mover 폰의 base·패널 로컬 위치·속도), `LNP.DynamicTerrain.LogDepartures 1|2`(이탈 관성, 2는 서버 플레이어 자동 점프), `LNP.DynamicTerrain.PlaceRider [i]`(Mover 텔레포트로 패널 위에 올림), `LNP.DynamicTerrain.RiderJump [i]`.

### 1차 2P 플레이 — 실패

- late join 자세 일치, 호스트·게스트 탄이 패널을 관통하지 않음: 통과.
- 탑승: 상승·하강 중 떨림, 수평 이동 중 미끄러짐(입력으로 따라가야 함), 이탈 관성 없음.

### 원인 1 — 틱 배치(절반만 추종)

- `LogRiders` 측정: base는 패널로 잡히는데, 패널이 0.3초에 90cm 가는 동안 폰은 45cm만 따라가 가장자리에서 떨어졌다.
- 처음 배치는 `OnWorldPreActorTick`(Mass 페이즈와 겹치지 않게)이었다. NPP 시뮬레이션도 같은 델리게이트에서 돌아 순서가 정해지지 않고, 패널 Actor에 틱이 없어 Mover의 base 추종 틱(`UpdateBasedMovementScheduling` → TG_PrePhysics, `AddTickDependency`는 base의 컴포넌트/Actor 틱이 있을 때만 prerequisite를 건다)이 패널 이동 뒤라는 보장도 없었다.
- Mover의 전제: NPP 시뮬레이션 뒤, base가 자기 틱에서 움직이고, base 추종 틱이 그 틱을 prerequisite로 잡아 이동량을 캡슐과 pending·presentation sync state에 옮긴다(`UBasedMovementUtils::UpdateSimpleBasedMovement`).
- 조치: 패널이 자기 Actor 틱(TG_PrePhysics)에서 자세를 갱신한다. snapshot 게시는 모든 패널 Actor 틱을 prerequisite로 갖는 서브시스템 틱 함수(TG_PrePhysics)로 옮겼다. 현재 worker exact query는 StartPhysics 페이즈(투사체 착탄)에만 있으므로 Gate 0의 쓰기 겹침 제약은 유지된다.
- 재측정: 수평 294cm/s·정지·수직 ±292cm/s 한 주기 동안 패널 로컬 좌표 변화 7초에 0.7cm. 함께 올라탄 ActorPromoted 적도 실려 다닌다.

### 원인 2 — 물리 속도 0(관성 없음)

- 걸어 나가기(`UAsyncWalkingMode::CaptureFinalState`)와 점프(`FJumpImpulseEffect`)는 모두 `GetMovementBaseVelocityAtPoint` → 물리 body 속도를 관성으로 더한다. teleport로 옮긴 kinematic body의 물리 속도는 0이다. `ComponentVelocity`는 이 경로에서 읽히지 않는다.
- 조치: 매 갱신 뒤 `SetPhysicsLinearVelocity(패널 속도)`. 프레임 선두(Mover 시뮬레이션 시점)에서 `bodyVel == panelVel` 확인.

### 2차 검증

- 사용자 2P(에디터 바이너리 `-game` 리슨): 호스트·게스트 모두 상승·하강·수평 이동에서 떨림 없음, 입력 없이 패널 위 고정 자리 유지.
- 관성(PIE 리슨 서버, `LogDepartures 2` 자동 점프 5회, 양방향): 이탈 프레임 패널 접평면 299.4~299.8cm/s, 폰 296.3~296.7cm/s(전달률 0.99). +0.3초 237~241cm/s(0.80)는 `UAsyncFallingMode::FallingDeceleration` 기본 200cm/s²과 정확히 일치(300−200×0.3=240)하는 평지 점프와 같은 공중 감속이다. 5회 모두 패널 위에 다시 착지. 걸어 나가기 경로는 같은 속도원을 쓰지만 수치로 따로 재지는 않았다.
- `LootNPopEditor`·`LootNPop Win64 Development` 빌드 성공(경고 없음), `LootNPop.SurfaceNavigation` 자동화 14/14.
- PIE 서버: 패널 8개 스폰·활성화 4.3ms, 경로 길이 2285cm, swept 반지름 25,435cm(envelope 26,928cm 안).

### 함정

- 옥탄트 LVI는 One File Per Actor다. MCP `save_assets([맵])`은 새 액터 패키지를 저장하지 않고, `save_actor`는 새 패키지에 대해 "Asset does not exist"로 실패한다. `save_assets([])`(dirty 전체)로 저장된다.
- UE 5.8 `USplineComponent`는 `SplineCurves`가 원본이다(`SplineComponent.UseSplineCurves` 기본 true). MCP로 점 배열을 바꿀 때 크기 변경과 값 변경을 한 호출에 할 수 없다.
- `SetActorLocation`으로 Mover 폰을 옮기면 다음 시뮬레이션이 sync state로 되돌린다. 테스트 배치는 `FTeleportEffect`로 한다.

## 2026-09-25 — 구현 단위 4: 부하 기준선 harness

### 구현 (`Source/LootNPop/SurfaceNavigation/LNPLoadBaseline.*`)

- 실행 인자 `-LNPLoadBaseline=N`로 켠다. 없으면 서브시스템 자체가 생성되지 않는다. 보조 인자: `-LNPLoadBaselineSeed`(기본 1), `-LNPLoadBaselinePlayers`(기본 2), `-LNPLoadBaselineProjectiles`(기본 500, 요인 분리 대조용), `-LNPLoadBaselineQuit`(보고 뒤 종료, 리슨 서버는 20초 유예).
- 적: `DA_MassSpawnConfig`의 적 수를 0으로 두고 Pod만 유지하며, Pod 배치 seed도 고정한다. -Z 극(PlayerStart 영역) 링(안쪽 1500cm, 1마리당 150,000cm²)에 정확히 N마리를 둔다. 10마리 중 1마리가 ActorPromoted이고 나머지는 PureEntity 근접·원거리 반반이다. 세력권 중심은 링 중심이며 Pod 엔티티 없이 위치만 준다(`SetupSpawnedEntities` 조건 완화).
- 투사체: PureEntity 원거리 적도 실제로 발사체를 쏜다(`LNPEntityAttackProcessor`). 그래서 전체 발사체 수를 query로 세고 부족분만 서버에 주입한다. 무기 값은 `DA_Enemy_PureEntity_Ranged01`의 `WeaponData`·`EntityAttackConfig`를 쓰므로 자연 발사체와 같은 공유 프래그먼트가 된다. 링 위 120cm에서 링 중심 ±1500cm를 향해 쏜다(pitch -2~+6°). Multicast를 하지 않는다.
- 플레이어는 harness가 켜져 있는 동안 피해를 받지 않는다(`ULNPBaseAttributeSet`). 경직·넉백은 그대로다.
- 계측: 준비(옥탄트 생성·스폰 완료·플레이어 2명 폰, 게스트는 자기 폰)가 끝나면 warm-up 10초, capture 30초. 프레임마다 `ULNPMassWorldCollisionSubsystem::GetTotals` 누적값 차분으로 exact 합계·query 수·락 probe를 표본화한다. 프레임 시간은 틱 사이 벽시계 간격이다. `FApp::GetDeltaTime()`은 0.1초로 잘려 과부하 구간이 정확히 100.00ms로 찍혔다. 락 probe CVar는 harness가 켠다.
- 적 Actor 수는 `MaxPromotedSlotsPerPlayer = 2`가 상한이다. 2P에서는 ActorPromoted 배치 비율(10%)과 무관하게 동시 Actor가 4~6개이고(사망 연출 중인 Actor 포함), 나머지는 슬롯을 기다린다. 제품 규칙이므로 harness는 바꾸지 않는다.

### 측정 (AMD Ryzen 7 8845HS, 에디터 바이너리 `-game` 리슨 2P 같은 머신, `-corelimit=4`, `t.MaxFPS 0`)

게스트 창까지 렌더하면 대조군(적 1·발사체 0)도 호스트 P50 20.7ms라 프레임 판정이 환경에 묻힌다. 아래는 게스트 `-nullrhi` 기준이다.

| 조건 | 호스트 프레임 P50/P95 | 호스트 exact/frame P95 | 호스트 락/frame P95 | 호스트 query/frame P50 | 게스트 프레임 P95 | 게스트 exact P95 |
|:---|:---|:---|:---|:---|:---|:---|
| 적 1·발사체 0 | 9.35/11.04ms | 0 | 0 | 0 | 7.26ms | 0 |
| 적 300·발사체 500 | 22.42/28.06ms | 1.249ms | 0.095ms | 500 | 12.52ms | 0.353ms |
| 적 1000·발사체 500 | 37.42/43.40ms | 1.665ms | 0.128ms | 507 | 16.15ms | 0.102ms |
| 적 1000·발사체 0(자연 발사 평균 64발) | 34.13/41.32ms | 0.269ms | 0.026ms | 64 | 16.20ms | 0.114ms |

- 모든 조건에서 `UnknownHits=0`, `EnvelopeEscapes=0`, 발사체 평균 502발(최소 466).
- **exact 합계와 락 대기 기준은 1000마리까지 통과했다.** 발사체 1발 = 프레임당 line query 1개이며 평균 1.7~2.1us다.
- **프레임 기준(P95 ≤ 16.6ms)은 호스트에서 300마리부터 실패한다.** 원인은 exact가 아니다. 발사체 500발을 더해도 호스트 프레임은 3ms 늘 뿐이고(34.1→37.4), 그중 exact 증가분은 약 1.4ms다. 적 수가 대조군 대비 +13ms(300)와 +25ms(1000)를 만든다.
- 호스트도 `-nullrhi`로 돌리면 1000·500이 P50 49.7ms로 오히려 느려졌다. 렌더링이 아니라 서버 CPU 시뮬레이션(적 Mass 처리·복제 등)의 비용이다. 어느 프로세서인지는 아직 Insights로 나누지 않았다.
- 게스트는 1000마리에서도 P95 약 16ms로 경계선이다. 게스트는 exact query를 거의 하지 않는다(자연 발사체 Ghost만).
- 첫 실행(게스트 렌더 포함, `FApp` delta 측정)의 호스트 300·500: exact P95 1.081ms, 락 P95 0.092ms로 위와 같은 수준이었다.

## 2026-09-25 — 구현 단위 4: 호스트 프레임 실패 분해(Insights)

### 계측 준비

- harness capture 30초 구간에 trace region `LNPLoadBaselineCapture`를 남긴다(`TRACE_BEGIN_REGION`/`TRACE_END_REGION`). `-trace` 인자가 없으면 비용이 없다.
- 호스트만 `-trace=cpu,frame,bookmark,region,log -tracefile=...`로 잡고, 통계는 헤드리스 Insights로 뽑는다: `UnrealInsights.exe -OpenTraceFile=<utrace> -AutoQuit -NoUI -ExecOnAnalysisCompleteCmd="@=<rsp>"`, rsp 한 줄에 `TimingInsights.ExportTimerStatistics "<csv>" -threads="GameThread" -region="LNPLoadBaselineCapture" -sortBy=TotalInclusiveTime`. 산출물은 `Saved/Profiling/Phase03/`(gitignore).
- trace 오버헤드는 무시할 수준이다. 에디터 `-game` 1000·P0 재현이 P95 41.9ms(기존 41.3ms)였다.

### 에디터 바이너리 `-game` 측정의 오염 두 가지

1. **`LNP.Debug.DrawEnemyAction` 기본값 1.** 에디터 전용(`WITH_EDITOR`) 프로세서가 로컬 폰 5,000cm 안의 적마다 `DrawSolidBox`를 그린다. 링 외곽이 7,071cm라 1000마리 대부분이 들어온다. 호스트 렌더 스레드 `FinishDynamicMeshElements`가 평균 16ms/프레임이었고 게임 스레드는 `Sync_RenderingThread`에서 평균 20ms를 기다렸다. 이것만 끄면 호스트 P50 35.1→20.3ms, 게스트 P95 16.2→10.4ms.
2. **`-nullrhi`에서 수명 0 디버그 라인이 영원히 쌓인다.** 비지속 라인은 `UGameViewportClient::Draw`(`GameViewportClient.cpp:2107` `FlushLineBatchers`)에서만 비워지는데 `-nullrhi`는 이 경로를 타지 않는다. Mass 디버그 드로우(플레이어 구·근접 적 캡슐·패링 라인)가 매 프레임 추가되고 `ULineBatchComponent::TickComponent`가 전체 배열을 훑어 프레임당 약 29ms가 됐다. 2026-09-25 첫 측정의 "호스트도 `-nullrhi`면 더 느리다"는 이 때문이다.

그래서 프레임 판정은 에디터 전용 프로세서가 없는 **패키지 Development 빌드**로 다시 했다.

### 패키지 Development 측정 (`Saved/SurfaceNavigationPhase3Package`, 같은 머신·`-corelimit=4`·게스트 `-nullrhi`)

| 조건 | 호스트 프레임 P50/P95 | 호스트 exact P95 | 호스트 락 P95 | 게스트 프레임 P95 |
|:---|:---|:---|:---|:---|
| 적 1000·발사체 0 | 26.48/37.67ms | 0.237ms | 0.011ms | 3.47ms |
| 적 1000·발사체 500 | 28.71/37.02ms | 0.941ms | 0.046ms | 3.51ms |
| 적 300·발사체 500 | 14.24/26.02ms | 0.916ms | 0.046ms | 3.43ms |
| 적 1000·발사체 0, **호스트 `-nullrhi`** | **5.18/6.05ms** | 0.126ms | 0.005ms | 2.62ms |

- 모든 조건에서 `UnknownHits=0`, `EnvelopeEscapes=0`.
- **서버 CPU 시뮬레이션은 1000마리에서 P95 6.05ms로 기준(16.6ms) 안이다.** 호스트 `-nullrhi`는 적 Mass 처리·복제·exact를 모두 수행하고 렌더만 뺀 조건이다.
- **호스트 프레임 실패는 GPU 대기다.** 패키지 1000·P0 trace에서 worker의 `GPUBound_WaitingForGPUForOcclusionQueries`가 평균 12.0ms/프레임, 렌더 스레드 `WaitForVisibilityTasks`가 평균 13.6ms, 게임 스레드 `Sync_RenderingThread`가 평균 12.8ms다. 게임 스레드 자체 `Tick_Engine`은 평균 9.3ms(병렬 틱 대기 포함)다. 측정 GPU는 외장 NVIDIA GeForce RTX 4060 Laptop이다(`Chosen D3D12 Adapter Id = 0`, 디스플레이 출력도 이 어댑터). CPU 이름의 Radeon 780M 내장 GPU는 쓰지 않았다. 해상도 1280×720, 엔진 기본 렌더링(Lumen·VSM)이다. 300마리도 P95 26ms로 실패해 적 수(인스턴싱 스킨드 메시·VFX)에 따라 GPU 비용이 커진다.
- exact query는 발사체 500발에서 P95 0.94ms로 에디터 바이너리(1.67ms)보다 가볍다.
- 에디터 `-game` 수치와 패키지 수치는 섞어 비교하지 않는다. 에디터 쪽 게스트 16ms도 액션 마커 드로우의 몫이었다.

### 결정(사용자, 2026-09-25)

- Phase 3 부하 기준선의 프레임 기준은 서버 CPU 프레임(패키지 호스트 `-nullrhi`) P95 ≤ 16.6ms로 재정의한다. 1000마리 P95 6.05ms로 통과.
- 렌더링 호스트 프레임(300마리 P95 26ms, 1000마리 37ms, RTX 4060 Laptop·엔진 기본 렌더링)은 기준선으로만 기록한다. 적 수에 비례하는 GPU 비용은 Phase 3 범위 밖이며 렌더링 트랙에서 다룬다.
- 측정 build 규약을 패키지 Development로 바꿨다(`phases/Phase03_MassWorldCollisionBaseline.md` §4, `design/RuntimeCollision.md` Gate 0 계측 계약).

## 2026-09-25 — Gate -1 B 잔여: 동적 패널 분류와 registry lifecycle

### 구현

- `ULNPMassWorldCollisionSubsystem`에 수명주기별 hit counter를 추가했다. `GetLifetimeHitCount`, Dynamic hit 중 MarkerId가 있는 수(`GetDynamicMarkerHitCount`)와 없는 수(`GetDynamicWithoutMarkerCount`)를 센다. 동적 요소는 마커 스폰뿐이므로(D-026) 없는 쪽은 분류 오류다. `Report`에 `HitLifetime` 줄이 추가됐다.
- 콘솔 `LNP.SurfaceNav.ProbePanels`(`LNPHitIdentityProbe.cpp`): 활성 패널마다 패널 법선을 따라 중심을 관통하는 raycast를 `MassWorldCollision`으로 쏘고, hit가 `Dynamic`·패널의 `(slot, MarkerId)`로 해석되는지 검사한다. 부하 발사체는 -Z 극 링을 겨냥해 패널에 맞지 않으므로, harness 보고 끝에서 이 명령을 호출해 호스트·게스트가 각자 검사한다.
- 자동화 `LootNPop.SurfaceNavigation.WorldCollision.DynamicMarkerHit`:
  - 마커 패널 hit는 Dynamic, slot, MarkerId, Support+Blocker로 해석된다.
  - 패널을 옮겨도 identity는 자세와 무관하다. 게임 스레드가 옮긴 자세를 query가 바로 본다.
  - 마커 없는 Dynamic hit는 `noMarker` counter로만 센다.
  - 해제하면 generation이 올라간 snapshot이 게시되고, 옛 snapshot은 소유 Actor를 파괴한 뒤에도 index·serial 비교만으로 읽힌다.

### 검증

- `LootNPopEditor`·`LootNPop Win64 Development` 빌드 성공, 경고 없음. `LootNPop.SurfaceNavigation` 자동화 15/15 통과.
- 에디터 바이너리 `-game` 리슨 2P 무인 실행(양쪽 `-nullrhi`, `-LNPLoadBaseline=1 -LNPLoadBaselineProjectiles=0`): 호스트(NetMode 2)와 게스트(NetMode 3) 모두 `[ProbePanels] panels=8 failures=0 -> PASS`. 패널은 이동 중이었다.

### registry lifecycle — 구조로 충족(사용자 결정)

- 현재 코드에는 match 중 reset·stream unload 경로가 없다. `StartWorldGeneration`은 월드마다 서버 GameMode와 클라이언트 GameState에서 한 번씩만 호출되고, match 종료는 월드 해제다. worker query는 월드 틱의 Mass phase 안에서만 돌므로 월드 해제와 겹치지 않는다.
- worker가 쥔 snapshot은 공유 참조라 게시 교체나 `Deinitialize` 뒤에도 살아 있고, 키 비교가 UObject를 역참조하지 않는다(위 자동화).
- 따라서 Phase 3에서는 별도 Mass gate를 만들지 않는다. **match 중 옥탄트 재생성이나 slot Level 언로드를 도입할 때는 그 직전에 Mass 처리를 멈추는 gate가 필수다**(`design/RuntimeCollision.md`). 참고로 지금 `StartWorldGeneration`을 다시 부르면 옛 Level Instance를 파괴하지 않고 목록만 비운다.

### 이관(사용자 결정)

- 한 component의 disconnected sheet face identity와 내부형 double-sided shell normal 검증은 `design/RegressionMap.md` 계획대로 Phase 4 착수 전 fixture 추가와 함께 수행한다. Phase 3에는 face→Layer 소비자가 없다.

## 2026-09-25 — 자동화·회귀 잔여와 Phase 3 종료

### 구현

- `LNPProjectileMotion::ClipSegmentToWorld`: 투사체 처리 프로세서 안의 람다였던 world/entity earliest hit 판정(월드 hit로 캐릭터 판정 선분을 자른다)을 함수로 옮겼다. 서버 Pass와 클라이언트 Ghost가 같은 함수를 부른다. 동작 변화 없음.
- 자동화 `WorldCollision.EarliestHit`: 벽 앞 적은 맞고 벽 뒤 적은 잘린 선분 밖이라 맞지 않는다. 월드 판정이 없으면 벽 뒤 적도 맞는 대조군 포함.
- 자동화 `WorldCollision.RegressionMap`(`WITH_EDITOR`): `L_SurfaceRegression`을 로드해 `LNP.Regression.Fixture` 37개의 mesh·transform·profile·mobility를 테스트 월드로 복제하고 Decoration을 뺀 25개를 런타임 source로 등록한다. 맵은 일반 레벨이라 slot 등록 경로를 타지 않기 때문이다. 기대값은 생성 스크립트 치수에서 나온다.
  - 기본 지각, 부유섬 1·2층 거리순, 섬 가장자리 안쪽 support·바깥 miss·측벽 sweep, 동굴 floor(Support)·ceiling·양벽(Blocker만), 나무·바위(Blocker만), 상태형 기둥(Dynamic Blocker), 움직이는 패널(Dynamic Support)과 아래 gap miss, 파괴 바닥(Destructible), seam 양쪽 hit와 경계를 가로지르는 sweep 무충돌.
  - Decoration을 지나는 ray와 등록하지 않은 `Pawn` profile 판을 지나는 ray가 모두 지면에 맞고 `UnknownHits=0`.
  - 바위는 비균등 scale 구라 표면 좌표 대신 바위 영역 안 hit만 본다.

### 검증

- `LootNPopEditor`·`LootNPop Win64 Development` 빌드 성공, 경고 없음.
- `UnrealEditor-Cmd -NullRHI` `Automation RunTests LootNPop.SurfaceNavigation` 17/17 통과, 새 경고 없음.
- 에디터 바이너리 `-game` 리슨 2P 무인(양쪽 `-nullrhi`, `-corelimit=4`, `-LNPLoadBaseline=300`, 발사체 500): 호스트·게스트 모두 `UnknownHits=0`, `EnvelopeEscapes=0`, `ProbePanels panels=8 failures=0 PASS`, exact·락 PASS, ensure·crash 0. 호스트 프레임 FAIL은 에디터 바이너리 오염(구현 단위 4 분해 절)이라 판정하지 않는다.

### Phase 3 종료

- 완료 조건을 모두 충족했다. Gate -1 B의 disconnected sheet face identity와 double-sided shell normal은 Phase 4 착수 전 fixture와 함께 검증한다.
- 다음 핵심 경로는 Phase 3b(exact 전용 부유섬 프로토타입)다. 실행 문서는 착수 직전에 만든다.
