# Surface Support·Navigation 현재 작업 상태

> 상태: 활성
> 현재 Phase: Phase 3b — exact 전용 부유섬 프로토타입 · 구현 단위 1·2 완료, 구현 단위 3(측정) 착수 전
> 마지막 갱신: 2026-09-25

## 현재 목표

Phase 3(MassWorldCollision 정확성 기준선)가 2026-09-25에 끝났다. 완료 증거는 `phases/Phase03_MassWorldCollisionBaseline.md`와 `history/Phase03_Log.md`에 있다.

Phase 3b는 Support 캐시를 만들기 전에 두 가지를 실측한다(D-032, `Roadmap.md` §4 Phase 3b).

- 300m 내부형 구에서 부유섬이 플레이 공간으로 재미있는가
- PureEntity 접지·낙하·넉백을 worker 동기 exact만으로 처리할 때 몇 마리에서 프레임 예산을 넘는가

완전 비행 NPC(Phase 3c)는 Phase 3 exact query만 의존하는 병렬 분기이며 Phase 4a를 막지 않는다(D-042).

## 착수 시 필수 문서

- `phases/Phase03b_ExactFloatingIslandPrototype.md`
- `Roadmap.md` §4 Phase 3b·Phase 4 공통 전제
- `design/RuntimeCollision.md`
- `design/MovementIntegration.md`
- `design/TerrainContract.md` §7(30,000cm 반지름, int16 복제 캡)
- `.context/Guide_OctantLevelInstance.md`(옥탄트 LVI 제작 절차)
- `history/Phase03_Log.md` 2026-09-25 구현 단위 4 절(부하 harness·측정 규약)

## Phase 3 인계 기준선

- `ULNPMassWorldCollisionSubsystem`: `RaycastWorld`·`SweepSphereWorld`·`SweepCapsuleWorld`·`ProbeSupport`, worker 동기 호출 가능(D-025), POD `FLNPWorldHit`와 hit identity(D-037)
- `ULNPHitIdentitySubsystem`: slot Level 일괄 등록 + 런타임 source 등록, immutable snapshot 게시, 미등록 hit는 `UnknownExactSurface`
- 투사체 서버·Ghost·탄도 가이드는 exact 전용(`LNPProjectileMotion::TraceWorld`·`ClipSegmentToWorld`), world collision envelope 안전망
- Placement Marker → 서버 스폰 복제 Actor(`SpawnPlacedActor`), 결정론 움직이는 패널과 2P Mover 탑승
- 부하 harness `-LNPLoadBaseline=N`. 프레임 판정은 패키지 Development 호스트 `-nullrhi` 서버 CPU 프레임(1000마리 P95 6.05ms). 에디터 바이너리 `-game` 프레임은 오염돼 쓰지 않는다
- 자동화 `LootNPop.SurfaceNavigation` 17개(회귀 맵 exact oracle `WorldCollision.RegressionMap` 포함). 3b 구현 단위 2에서 `ExactMovement.*` 4개 추가(21개)

## Phase 3b 진행 상태

- 구현 단위 1: 30,000cm `Meadow_00`, 섬 3개, 경사로 두 방식, 간이 동굴, 월드 장치 마커
- 구현 단위 2: 패널 → Mass PrePhysics 선행 조건(D-050 C안, 2P 위반 0), exact 접지·공중·배회 재투영(`Enemy/LNPEnemyExactMovement.*`), CVar `LNP.SurfaceNav.EnemyExactGround`(기본 1)·`EnemyExactLateralSweep`(기본 1), 자동화 `ExactMovement.*` 4개. 세부 규약은 Phase 문서 §3.3 "구현 규약"

## 바로 다음 작업

1. **구현 단위 3: 측정 harness 확장.** `LNPLoadBaseline`의 링 배치를 exact probe로 지각·섬 윗면에 직접 놓고(링 일부가 큰 섬 아래를 지나게, 적 10%는 섬 위), 적마다 평균 10초에 한 번 합성 넉백(섬 위 적은 가장자리 밖으로)을 준다. 착지·낙하 이벤트 수와 섬 가장자리 이탈 수 counter를 추가한다. 섬 아래 적이 섬 윗면으로 순간이동하지 않는지도 이 배치로 확인한다(legacy 0 대비).
2. **§3.6 매트릭스.** 패키지 Development 호스트 `-nullrhi` 서버 CPU 프레임으로 N ∈ {300, 1000, 2000} × {legacy, exact} + N=1000 lateral 0/1. 에디터 바이너리 스모크에서 exact 접지 query는 적 300에 프레임당 약 1,000회(평균 3.9us)였다.

## 이관된 후속 작업

- production Terrain Contract Component Tag 마이그레이션은 실제 베이커를 production source에 적용하는 Phase 4 이후에 수행한다.
- production definition의 SurfaceData 연결과 runtime 로드는 Phase 5 소비자 전환에서 수행한다.
- 실제 Support Atlas rasterization과 payload codec은 Phase 4a·4b 범위다.
- `LNPOctantSourceCollector`의 무태그 충돌 컴포넌트 보고와 LVI 내 동적 태그 차단은 Phase 4 착수 전에 수정한다.
- `LNPOctantSourceCollector`의 tag/profile/channel 검증과 marker authoring hash를 Phase 4·8 스키마에 맞춰 보강한다. owned external package를 모두 hash해 decoration 저장도 stale이 되는 현재 보수 정책은 보고서에 명시하고, false stale이 실제 문제가 될 때만 필터링한다.
- C-option 실험 에셋과 테스트의 구형 `LNP.Terrain.*` Component Tag는 Phase 4 입력으로 재사용하기 전에 현재 `LNP.Surface.*` 계약으로 마이그레이션한다.
- 현재 slot 순서 greedy definition 선택은 여러 slot mask가 있는 production pool을 도입하기 전에 최대 고유 제약 할당으로 교체한다(D-043).
- int16 복제 캡은 좌표 성분마다 걸리므로 30,000cm 옥탄트의 꼭짓점(좌표축) 부근 여유가 약 2,767cm다. 동굴은 꼭짓점 부근을 피한다(`design/TerrainContract.md` §7).
- Phase 4 착수 전 전제: fixture 재배치·fixture LVI(30,000cm 좌표 재계산), 동굴 fixture의 키트 방식 교체, greybox 섬 옥탄트에 동굴 키트 공동 모듈과 통로 추가.
- Phase 3 Gate -1 B에서 이관: 한 component의 disconnected sheet face identity, 내부형 double-sided shell의 hit normal·walkable 판정. `design/RegressionMap.md`의 Phase 4 착수 전 fixture로 검증한다.
- match 중 옥탄트 재생성이나 slot Level 언로드를 도입하면 그 직전에 Mass 처리를 멈추는 gate를 함께 만든다(`design/RuntimeCollision.md`).
- 회귀 맵을 30,000cm로 옮기면 `WorldCollision.RegressionMap` 자동화의 좌표 기대값도 같은 커밋에서 갱신한다.

- 스프링 런처는 개체별 발사 속도·각도 입력이 없다(`DA_WorldDeviceConfig` 전역값, 정점 약 2,530cm). 섬별 튜닝은 이 입력을 만든 뒤에 한다. 지금은 런처→섬 A, 앵커→섬 B·큰 섬으로 역할을 나눴다.

## 알려진 불확실성

- exact query의 호출 빈도와 배치 단위, 관찰 거리 축의 근처 반경과 원거리 판정 주기는 Phase 3b 실측으로 정한다.
- `LNPSurfaceSupport` 소비자 전환 시점은 Phase 3b 결과와 함께 확정한다.
- Nanite mesh의 complex collision이 원본 mesh와 fallback mesh 중 어디서 만들어지는지는 Phase 4a 착수 시 확인한다.
- 렌더링 호스트 프레임(300마리 P95 26ms, 1000마리 37ms, RTX 4060 Laptop)은 GPU 비용이며 Surface Navigation 범위 밖이다. 렌더링 트랙에서 다룬다.
- Development package의 기존 Lyra Mannequin material은 누락 Material Function 때문에 default material로 대체된다. Surface Navigation 검증과는 분리된 콘텐츠 문제다.

## 블로커

현재 확인된 블로커는 없다.

## 마지막 검증

2026-09-25 Phase 3b 구현 단위 2(exact 접지 경로):

- `LootNPopEditor`·`LootNPop Win64 Development` 빌드 성공, 경고 없음, 자동화 21/21
- 에디터 바이너리 `-game` 리슨 2P 무인(적 300·발사체 500, exact 1): `MassPrePhysicsOrder` 위반 호스트·게스트 0, 호스트 `UnknownHits=0`·`EnvelopeEscapes=0`·락 P95 0.153ms, 크래시·ensure 0. 게스트 워밍업 구간 Unknown 1건은 원인 미확인(투사체 Ghost 쪽)

2026-09-25 Phase 3b 구현 단위 1(30,000cm Meadow_00):

- `LootNPopEditor` 빌드 성공, 자동화 17/17
- 8 slot 생성, 마커 요소 48개 스폰(0 skipped), audit `MISSING=0`, `UnknownHits=0`, envelope 32,636cm
- `ExactOracle`: ExactDeeper 20건(정보 항목, legacy 캐시 결함) 외 0
