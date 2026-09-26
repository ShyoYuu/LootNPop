# Surface Support·Navigation 현재 작업 상태

> 상태: 활성
> 현재 Phase: Phase 3c — 완전 비행 NPC 기반 · 구현 단위 1·2 완료, 구현 단위 3 대기
> 마지막 갱신: 2026-09-26

## 현재 목표

Phase 3b(exact 전용 부유섬 프로토타입)가 2026-09-26에 끝났다. 완료 증거는 `phases/Phase03b_ExactFloatingIslandPrototype.md`와 `history/Phase03b_Log.md`에 있다.

현재는 Phase 3c(완전 비행 NPC 기반)다(사용자 결정, 2026-09-26). 실행 문서는 `phases/Phase03c_FlyingNpcFoundation.md`다. 지상 NPC가 섬을 건너지 않아(D-011) 섬이 안전 지대가 되는 공백을 비행 NPC로 메운다. 핵심 완료 조건은 저비용 3D steering과 섬 회피, 섬 위 플레이어 교전이다(`Roadmap.md` §3). 의존성은 Phase 3 exact sweep뿐이다(D-042). Phase 4a(지각 Support Atlas와 옥탄트 이음매)는 3c 뒤에 진행한다.

## 착수 시 필수 문서

- `phases/Phase03c_FlyingNpcFoundation.md`(범위·확정 결정·구현 단위·완료 조건)
- 구현 단위 1·2: `design/MovementIntegration.md` "완전 비행 NPC", `design/RuntimeCollision.md` 쿼리 분류
- 구현 단위 3: `../TechDesign_EnemyNPC_LowLOD.md` §4.4(원거리)·§8(사망)

## Phase 3c 확정 사항(2026-09-26 사용자 결정)

- PureEntity 전용, 같은 Enemy Trait + 비행 태그, 별도 비행 이동 프로세서(D-052)
- 1차 공격은 기존 PureEntity 원거리 사격 재사용 + 발사 전 LoS 게이트(D-052)
- 고도: 교전은 타겟 상대 대역·선회, 비교전은 Home 기준 대역(D-053)
- 외형: 블렌더 MCP로 새 저폴리 static mesh, ISM 표현
- 목표 개체 수·예산: 구현 단위 4 측정 뒤 결정

## Phase 3b 인계 기준선

- 30,000cm `Meadow_00`: 섬 3개(큰 섬·섬 A 경사로 두 방식·섬 B), 간이 동굴, 월드 장치 마커. 8 slot 모두 이 definition
- PureEntity exact 이동(`Enemy/LNPEnemyExactMovement.*`, D-049). CVar `LNP.SurfaceNav.EnemyExactGround`·`EnemyExactLateralSweep`·`EnemyParallelMovement` 모두 기본 1. Phase 6 exact 폴백으로 재사용
- 패널 → Mass PrePhysics 선행 조건(D-050 C안)
- **exact 한계치: 단일 스레드 500마리(최악 조건 기준선), 병렬 750마리.** 접지 개체당 프레임 약 1.46 query·8us. Phase 4 캐시 적중률 목표와 Phase 6 재측정은 병렬 750 기준으로 읽는다
- 부하 harness `-LNPLoadBaseline=N`(slot 4 큰 섬 가장자리 링), 측정 스크립트 `Scripts/Profiling/RunLoadBaselineMatrix.ps1`. 프레임 판정은 패키지 Development 호스트 `-nullrhi`
- 자동화 `LootNPop.SurfaceNavigation` 21개
- 카메라 리그 `CR_ThirdPerson`에 `CollisionPush` 노드를 추가했다(`../TechDesign_CharacterMovement.md` §2.4)

## 바로 다음 작업

1. **구현 단위 3 — 교전 마무리.** LoS 게이트 모듈(`LNPEnemyLineOfSight`, `FLNPEntityAttackConfig::bRequireLineOfSight`, query 분류 `EnemyLineOfSight`)과 막혔을 때 교전 측면 재배치, 비행 넉백 감쇠·조종 약화, 사망 낙하 점검(Phase 문서 §3.2·§3.5)
2. Phase 4a는 3c 뒤에 Phase 4 공통 전제부터 처리한다(fixture 재배치·fixture LVI 30,000cm 좌표 재계산과 `WorldCollision.RegressionMap` 기대값 동시 갱신, 동굴 fixture 키트 교체, greybox 섬 옥탄트에 동굴 키트 공동 모듈과 통로).

## 이관된 후속 작업

- production Terrain Contract Component Tag 마이그레이션은 실제 베이커를 production source에 적용하는 Phase 4 이후에 수행한다.
- production definition의 SurfaceData 연결과 runtime 로드는 Phase 5 소비자 전환에서 수행한다.
- 실제 Support Atlas rasterization과 payload codec은 Phase 4a·4b 범위다.
- `LNPOctantSourceCollector`의 무태그 충돌 컴포넌트 보고와 LVI 내 동적 태그 차단은 Phase 4 착수 전에 수정한다.
- `LNPOctantSourceCollector`의 tag/profile/channel 검증과 marker authoring hash를 Phase 4·8 스키마에 맞춰 보강한다. owned external package를 모두 hash해 decoration 저장도 stale이 되는 현재 보수 정책은 보고서에 명시하고, false stale이 실제 문제가 될 때만 필터링한다.
- C-option 실험 에셋과 테스트의 구형 `LNP.Terrain.*` Component Tag는 Phase 4 입력으로 재사용하기 전에 현재 `LNP.Surface.*` 계약으로 마이그레이션한다.
- 현재 slot 순서 greedy definition 선택은 여러 slot mask가 있는 production pool을 도입하기 전에 최대 고유 제약 할당으로 교체한다(D-043).
- int16 복제 캡은 좌표 성분마다 걸리므로 30,000cm 옥탄트의 꼭짓점(좌표축) 부근 여유가 약 2,767cm다. 동굴은 꼭짓점 부근을 피한다(`design/TerrainContract.md` §7).
- Phase 3 Gate -1 B에서 이관: 한 component의 disconnected sheet face identity, 내부형 double-sided shell의 hit normal·walkable 판정. `design/RegressionMap.md`의 Phase 4 착수 전 fixture로 검증한다.
- match 중 옥탄트 재생성이나 slot Level 언로드를 도입하면 그 직전에 Mass 처리를 멈추는 gate를 함께 만든다(`design/RuntimeCollision.md`).
- Mass 스폰·Pod 배치는 여전히 SurfaceCache 첫 hit를 쓰므로 Pod가 섬 윗면에 생길 수 있다. Phase 5 Spawn stream에서 해결한다.
- 스프링 런처는 개체별 발사 속도·각도 입력이 없다(`DA_WorldDeviceConfig` 전역값, 정점 약 2,530cm). 섬별 튜닝은 이 입력을 만든 뒤에 한다. 지금은 런처→섬 A, 앵커→섬 B·큰 섬으로 역할을 나눴다. 런처→섬 A 경로는 3b 기능 점검에서 육안 미확인이다.
- LootPod collision proxy(`LNPStaticBlocker`)가 Camera 채널을 Block해 카메라가 Pod 뒤에서 당겨진다. 거슬리면 proxy만 Camera Ignore로 바꾼다.

## 알려진 불확실성

- exact query의 배치 단위, 관찰 거리 축의 근처 반경과 원거리 판정 주기는 아직 정하지 않았다.
- `LNPSurfaceSupport` 소비자 전환 시점은 Phase 5 착수 시 확정한다.
- Nanite mesh의 complex collision이 원본 mesh와 fallback mesh 중 어디서 만들어지는지는 Phase 4a 착수 시 확인한다.
- 렌더링 호스트 프레임(300마리 P95 26ms, 1000마리 37ms, RTX 4060 Laptop)은 GPU 비용이며 Surface Navigation 범위 밖이다. 렌더링 트랙에서 다룬다.
- Development package의 기존 Lyra Mannequin material은 누락 Material Function 때문에 default material로 대체된다. Surface Navigation 검증과는 분리된 콘텐츠 문제다.

## 블로커

현재 확인된 블로커는 없다.

## 마지막 검증

2026-09-26 Phase 3c 구현 단위 2:

- `LootNPopEditor Win64 Development` 성공, 자동화 23/23(신규 `FlightSteering.DetourAndStuck`)
- PIE `TestMap03` 비행 240: `FlightSteering` 평균 3.11us, `UnknownHits=0`, `EnvelopeEscapes=0`, ensure 0
- 사용자 PIE(Pod당 드론 10): 동굴형 경사로 근처 우회·드론 간 비겹침 확인

2026-09-26 Phase 3c 구현 단위 1:

- `LootNPopEditor Win64 Development` 성공, 자동화 22/22(신규 `FlightSteering.LookaheadStop`)
- PIE `TestMap03`, 비행 240: `FlightSteering` 평균 3.71us·비행 1마리당 프레임 약 0.3회, `UnknownHits=0`, `EnvelopeEscapes=0`, ensure 0

2026-09-26 Phase 3b 구현 단위 4(기능 점검·Phase 종료):

- 사용자 PIE: 카메라 지형 비관통(CollisionPush 추가 후), 탄도 가이드 섬 밑면·측벽 끊김, 앵커로 큰 섬 오르기, 넉백 낙하 뒤 지각 착지 통과. 런처→섬 A 미확인
- `LootNPop Win64 Development` 성공, `LootNPopEditor` up to date, 자동화 21/21
- 에디터 바이너리 `-game` 리슨 2P(적 300·기본 CVar): 호스트·게스트 `UnknownHits=0`·`EnvelopeEscapes=0`·`MassPrePhysicsOrder` 위반 0, LayerJumps 0, ensure 0
