# Phase 3 — MassWorldCollision 정확성 기준선 기록

> 상태: 진행 전
> 실행 문서: `../phases/Phase03_*.md` (작성 예정)

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
