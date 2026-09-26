# Surface Support·Navigation 현재 작업 상태

> 상태: 활성
> 현재 Phase: Phase 4a — 지각 Support Atlas와 옥탄트 이음매 · 구현 단위 0 착수 전
> 마지막 갱신: 2026-09-27

## 현재 목표

Phase 3c(완전 비행 NPC 기반)가 2026-09-27에 끝났다. 완료 증거는 `phases/Phase03c_FlyingNpcFoundation.md`와 `history/Phase03c_Log.md`에 있다.

Phase 4a(지각 Support Atlas와 옥탄트 이음매)를 시작했다. 실행 문서는 `phases/Phase04a_CrustAtlasAndSeams.md`다. 범위는 지각 관련만이고, 섬·동굴 fixture와 동굴 키트, 다층 Atlas는 4b다(사용자 결정). 지각 Layer 식별 규칙은 D-055다.

## 착수 시 필수 문서

- `phases/Phase04a_CrustAtlasAndSeams.md`
- `design/SurfaceBaking.md`(Support Atlas·베이커), `design/DataModel.md`(베이크 에셋·ID), `design/TerrainContract.md`(태그·이음매·int16 캡)

## 인계 기준선(3b·3c)

- 30,000cm `Meadow_00`: 섬 3개(큰 섬·섬 A 경사로 두 방식·섬 B), 간이 동굴, 월드 장치 마커. 8 slot 모두 이 definition
- PureEntity exact 이동(`Enemy/LNPEnemyExactMovement.*`, D-049). CVar `LNP.SurfaceNav.EnemyExactGround`·`EnemyExactLateralSweep`·`EnemyParallelMovement` 모두 기본 1. Phase 6 exact 폴백으로 재사용
- 패널 → Mass PrePhysics 선행 조건(D-050 C안)
- **exact 한계치: 단일 스레드 500마리(최악 조건 기준선), 병렬 약 700마리.** 3b 측정은 750이었으나 3c에서 750이 3회 모두 실패하고 700은 2회 중 1회 통과했다(query 수·호출당 비용은 3b와 같음). 접지 개체당 프레임 약 1.46 query·8us. Phase 4 캐시 적중률 목표와 Phase 6 재측정은 병렬 700 기준으로 읽는다
- 비행 드론(PureEntity, D-052·053): 총수 200, Pod 타입 분리(지상 전용 74, 드론 3기 20·5기 16·6기 10, D-054). 동시 교전은 원거리 슬롯 상한(플레이어당 20)으로 묶인다. 비행 비용은 대부분 엔티티당 공통 비용이라 Support 캐시의 절감 대상이 아니다
- 부하 harness `-LNPLoadBaseline=N`·`-LNPLoadBaselineFlyers=F`(slot 4 큰 섬 가장자리 링), 측정 스크립트 `Scripts/Profiling/RunLoadBaselineMatrix.ps1`. 프레임 판정은 패키지 Development 호스트 `-nullrhi`. 측정 전에 다른 게임·무거운 프로세스를 끈다
- 자동화 `LootNPop.SurfaceNavigation` 26개
- 카메라 리그 `CR_ThirdPerson`에 `CollisionPush` 노드(`../TechDesign_CharacterMovement.md` §2.4)

## 바로 다음 작업

1. **구현 단위 1 — 삼각형 추출과 지각 식별.** Editor에서 컴포넌트의 cooked Chaos trimesh를 옥탄트 로컬 삼각형으로 추출(음수 scale winding 반영), runtime 순수 함수로 D-055 지각 판정과 int16 캡 검사. fixture 슬래브·양면 판의 추출 삼각형이 exact trace hit 위치·법선과 일치하는지 검증
2. **Nanite complex collision 원본 확인**(transient mesh 자동화, 실행 문서 §3.8)

완료(2026-09-27) — 구현 단위 0:
- 수집기 보강(무태그 충돌 일괄 보고·LVI 내 비-Static 차단·tag/profile 응답 일치·Decoration profile, builder brush·transient 액터 제외)
- `Meadow_00` 태그 마이그레이션(지각 `StaticMesh`에 Support+Blocker+Static, `PCG_Octant_BaseProps` spawner descriptor에 Blocker+Static 후 PCG 재생성)
- 지각 fixture LVI `Fixtures/LVI_Octant_Fixture_Crust`(명령 `LNP.SurfaceNav.BuildCrustFixture`)
- 자동화 26/26. 헤드리스 `-ExecCmds`는 쉼표로 명령을 나누고, 에디터 바이너리에서는 `Quit`로 종료되지 않는다(`Automation RunTests`는 종료함)

## 이관된 후속 작업

- production Terrain Contract Component Tag 마이그레이션은 `Meadow_00`만 끝났다(4a 입력). 다른 production 옥탄트를 pool에 넣을 때 같은 방식으로 한다.
- production definition의 SurfaceData 연결과 runtime 로드는 Phase 5 소비자 전환에서 수행한다.
- 실제 Support Atlas rasterization과 payload codec은 Phase 4a·4b 범위다.
- 4b 착수 전으로 미룬 Phase 4 공통 전제: `L_SurfaceRegression` fixture 재배치(30,000cm 좌표·`WorldCollision.RegressionMap` 기대값 동시 갱신), 동굴 fixture 키트 교체, `Meadow_00`에 동굴 키트 공동 모듈과 통로, Conditional Patch 개념 설계(`design/RegressionMap.md` §6).
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

- 3c 측정 중 부하 harness 호스트가 비동기 스폰 배치 직후 한 번 멈췄다(재현 안 됨, `history/Phase03c_Log.md`). 재발하면 작업 스레드 exact probe와 게임 스레드 경합부터 본다.
- 병렬 한계치가 750 → 약 700으로 내려온 원인(비 query 비용 증가로 추정)은 나누어 재지 않았다.
- exact query의 배치 단위, 관찰 거리 축의 근처 반경과 원거리 판정 주기는 아직 정하지 않았다.
- `LNPSurfaceSupport` 소비자 전환 시점은 Phase 5 착수 시 확정한다.
- Nanite mesh의 complex collision이 원본 mesh와 fallback mesh 중 어디서 만들어지는지는 Phase 4a 구현 단위 1에서 확인한다.
- 렌더링 호스트 프레임(300마리 P95 26ms, 1000마리 37ms, RTX 4060 Laptop)은 GPU 비용이며 Surface Navigation 범위 밖이다. 렌더링 트랙에서 다룬다.
- Development package의 기존 Lyra Mannequin material은 누락 Material Function 때문에 default material로 대체된다. Surface Navigation 검증과는 분리된 콘텐츠 문제다.

## 블로커

현재 확인된 블로커는 없다.

## 마지막 검증

2026-09-27 Phase 3c 구현 단위 5(Phase 종료):

- `LootNPopEditor Win64 Development` 성공, 자동화 24/24, 패키지 BuildCookRun 성공
- 에디터 바이너리 `-game` 리슨 2P(새 편성): 게스트에서 드론 교전·Ghost 발사체·HP 바 정상(사용자 확인), 호스트·게스트 ensure·크래시 0
- 사용자 PIE: 새 Pod 편성 체감 이상 없음

2026-09-27 Phase 3c 구현 단위 4(측정·예산):

- 패키지 매트릭스(비행 100/300/500, 지상 500+비행 100, 관통 검출 비행 500, 지상 500·700×2·750×3): 모든 실행 `UnknownHits=0`·`EnvelopeEscapes=0`·ensure 0, 관통 0
