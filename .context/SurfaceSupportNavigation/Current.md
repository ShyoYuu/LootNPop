# Surface Support·Navigation 현재 작업 상태

> 상태: 활성
> 현재 Phase: Phase 4b — 부유섬·동굴 키트 다층 베이크 · 구현 단위 0~3 완료, 구현 단위 4 착수 전
> 마지막 갱신: 2026-09-27

## 현재 목표

Phase 4a(지각 Support Atlas와 옥탄트 이음매)가 2026-09-27에 끝났다. 완료 증거는 `phases/Phase04a_CrustAtlasAndSeams.md`와 `history/Phase04a_Log.md`에 있다. 지각 Atlas의 격자·codec·조회·이음매 규약 원본은 `design/SurfaceBaking.md` "지각 Atlas 규약"이다.

지금은 Phase 4b(같은 방향 다층 Support와 동굴 키트 공동 바닥 분리)다. 실행 문서는 `phases/Phase04b_MultiLayerSupport.md`다. 범위 결정: sparse Atlas는 지각과 같은 octahedral 격자 계열에 row span 저장(D-057), ISM Support와 Support proxy는 미루고 베이크 오류로 막는다(D-058). 비지각 Layer 해상도는 m=4(25cm)로 확정했다(구현 단위 3).

## 착수 시 필수 문서

- `phases/Phase04b_MultiLayerSupport.md`
- `design/SurfaceBaking.md`(다층 교차 수집·동굴 키트 베이크·지각 Atlas 규약), `design/TerrainContract.md` §5·§6(키트 에셋 규약)·§7, `design/RegressionMap.md` §3·§4·§7, `design/DynamicTerrain.md` §5(patch Layer가 4b sparse Atlas를 재사용)

## 인계 기준선(3b·3c·4a·4 공통 전제)

- 30,000cm `Meadow_00`: 섬 3개(큰 섬·섬 A 경사로 두 방식·섬 B), 간이 동굴, 월드 장치 마커. 8 slot 모두 이 definition
- PureEntity exact 이동(`Enemy/LNPEnemyExactMovement.*`, D-049). CVar `LNP.SurfaceNav.EnemyExactGround`·`EnemyExactLateralSweep`·`EnemyParallelMovement` 모두 기본 1. Phase 6 exact 폴백으로 재사용
- 패널 → Mass PrePhysics 선행 조건(D-050 C안)
- **exact 한계치: 단일 스레드 500마리(최악 조건 기준선), 병렬 약 700마리.** 접지 개체당 프레임 약 1.46 query·8us. Phase 4 캐시 적중률 목표와 Phase 6 재측정은 병렬 700 기준으로 읽는다
- 비행 드론(PureEntity, D-052·053·054): 총수 200, Pod 타입 분리. 비행 비용은 Support 캐시의 절감 대상이 아니다
- 부하 harness `-LNPLoadBaseline=N`·`-LNPLoadBaselineFlyers=F`, 측정 스크립트 `Scripts/Profiling/RunLoadBaselineMatrix.ps1`. 프레임 판정은 패키지 Development 호스트 `-nullrhi`
- Support Atlas: runtime `LNPSupportAtlas`(격자·row span·rasterize·codec v2·`QueryLayer`·`QueryLayers`), 지각 전용 `LNPCrustAtlas`(이음매 스냅 rasterize·seam 규약), Editor `LNPOctantSurfaceBaker`·`LNP.SurfaceNav.BakeOctant <LevelPath>`. 지각 N=735(100cm), 비지각 Layer m=4(25cm, 확정). `DataVersion` 3. `Meadow_00` payload 2.63MB, 지각 조회 NeedsExact 2.35%, 반지름 P99 1.06cm. 비지각 Layer 반지름 P99 0.12cm, 조회 NeedsExact 20.6%(큰 섬 1.9%). `QueryLayers`는 footprint 가장자리 띠도 후보로 본다
- `DA_OctantSurface_Meadow_00`·`DA_OctantSurface_Fixture_Crust`는 LVI 옆에 저장돼 있다. `Bake.OctantBakeDeterministic`가 저장본과 현재 source의 일치를 검사하므로 LVI나 베이크 설정을 바꾸면 `BakeOctant`로 다시 굽는다
- 회귀 공간(D-056): 정적 사례는 `LVI_Octant_Fixture_Regression`(생성 `LNP.SurfaceNav.BuildRegressionFixture`, 배치 원본 `LNPRegressionFixture.h`)을 8 slot 합성으로 검사하고, 동적 3사례만 `L_SurfaceRegression`(30,000cm)에 있다. `DA_OctantSurface_Fixture_Regression`도 결정론 베이크 검사 대상이다
- 동굴 키트 greybox(`/Game/Maps/CaveKit`, `LNP.SurfaceNav.BuildCaveKit`, 치수 원본 `LNPCaveKit.h`): 직육면체 공동 + 경사 통로, Floor/Shell 분리, 규약 검사 `Bake.CaveKitContract`. `Meadow_00` 동굴은 (위도 15°, 방위 60°)에 있고 `LNP.SurfaceNav.PlaceCaveKit`으로 배치했다(지각 메시 입구 절단 포함)
- 자동화 `LootNPop.SurfaceNavigation` 53개(`Bake.*` 25개). 자동화가 `SurfaceNavigationTests/MeshTerrain`의 `SM_BOptionExtracted`·`SM_COptionSphereSculpt`를 다시 저장하므로 커밋 전에 git으로 되돌린다
- 헤드리스 `-ExecCmds`는 쉼표로 명령을 나누고, 에디터 바이너리에서는 `Quit`로 종료되지 않는다(`Automation RunTests`는 종료함)
- 카메라 리그 `CR_ThirdPerson`에 `CollisionPush` 노드(`../TechDesign_CharacterMovement.md` §2.4)

## 바로 다음 작업

1. 구현 단위 4: 자동화 `WorldCollision.LayerIdentity`를 만든다. fixture 회귀 LVI를 8 slot 테스트 월드에 놓고, exact hit의 (source key, `FaceIndex`)를 face 표로 해석한 Layer가 같은 지점 `QueryLayers` Layer와 같은지 본다. 사례는 섬 둘 3층, 동굴 바닥, 섬 가장자리 바깥이다(Phase 문서 §4 구현 단위 4)
2. 같은 단위에서 `Meadow_00` 섬 B 계단을 검사한다. 칸 위에서는 그 칸 Layer로 Supported이거나 NeedsExact여야 하고 다른 Layer를 고르면 안 된다. NeedsExact 지점은 exact face→Layer가 그 칸이어야 한다. 이어서 에디터 빌드, 자동화 전체, `-game` 리슨 2P 스모크, 문서 갱신으로 Phase 4b를 닫는다

## 이관된 후속 작업

- production Terrain Contract Component Tag 마이그레이션은 `Meadow_00`만 끝났다(4a 입력). 다른 production 옥탄트를 pool에 넣을 때 같은 방식으로 한다.
- production definition의 SurfaceData 연결과 runtime 로드는 Phase 5 소비자 전환에서 수행한다.
- Phase 5 로더는 SurfaceData 게시 전에 seam hash 호환성을 검사한다(D-043). 대응표는 `LNPCrustAtlas::ComputeSeamPairs`, 규약은 `design/SurfaceBaking.md` "지각 Atlas 규약".
- `Meadow_00` 지각 이음매 경계 정점에 `|d| < 5e-7cm` 부동소수점 잡음이 있다. 베이커가 스냅하므로 Atlas에는 영향이 없다. 출처(mesh 생성기·빌드)는 추적하지 않았다.
- `LNPOctantSourceCollector`의 tag/profile/channel 검증과 marker authoring hash를 Phase 4·8 스키마에 맞춰 보강한다. owned external package를 모두 hash해 decoration 저장도 stale이 되는 현재 보수 정책은 보고서에 명시하고, false stale이 실제 문제가 될 때만 필터링한다.
- C-option 실험 에셋과 테스트의 구형 `LNP.Terrain.*` Component Tag는 Phase 4 입력으로 재사용하기 전에 현재 `LNP.Surface.*` 계약으로 마이그레이션한다.
- 현재 slot 순서 greedy definition 선택은 여러 slot mask가 있는 production pool을 도입하기 전에 최대 고유 제약 할당으로 교체한다(D-043).
- int16 복제 캡은 좌표 성분마다 걸리므로 30,000cm 옥탄트의 꼭짓점(좌표축) 부근 여유가 약 2,767cm다. 동굴은 꼭짓점 부근을 피한다(`design/TerrainContract.md` §7).
- **PCG 제외 구역(옥탄트 양산 전 필수, 사용자 결정 2026-09-27):** PCG 프랍은 지각에만 광선을 쏘므로 동굴 입구 구멍에는 생기지 않지만 지붕 덮인 입구 옆·경사로 위에는 생길 수 있다. `Meadow_00`은 입구 주변 4개가 통행을 막지 않아 문제없지만 양산 옥탄트에서는 충분히 생길 수 있으므로 제외 구역을 만든다(`design/TerrainContract.md` §5 경사로와 같은 과제).
- `WorldCollision.Api`의 "Some raycasts ran off the game thread"는 `ParallelFor`가 워커를 못 받으면 간헐 실패한다(2026-09-27 1회, 재실행 통과). 반복되면 워커 강제 실행으로 테스트를 고친다.
- match 중 옥탄트 재생성이나 slot Level 언로드를 도입하면 그 직전에 Mass 처리를 멈추는 gate를 함께 만든다(`design/RuntimeCollision.md`).
- Mass 스폰·Pod 배치는 여전히 SurfaceCache 첫 hit를 쓰므로 Pod가 섬 윗면에 생길 수 있다. Phase 5 Spawn stream에서 해결한다.
- 스프링 런처는 개체별 발사 속도·각도 입력이 없다(`DA_WorldDeviceConfig` 전역값, 정점 약 2,530cm). 섬별 튜닝은 이 입력을 만든 뒤에 한다. 지금은 런처→섬 A, 앵커→섬 B·큰 섬으로 역할을 나눴다. 런처→섬 A 경로는 3b 기능 점검에서 육안 미확인이다.
- LootPod collision proxy(`LNPStaticBlocker`)가 Camera 채널을 Block해 카메라가 Pod 뒤에서 당겨진다. 거슬리면 proxy만 Camera Ignore로 바꾼다.

## 알려진 불확실성

- 3c 측정 중 부하 harness 호스트가 비동기 스폰 배치 직후 한 번 멈췄다(재현 안 됨, `history/Phase03c_Log.md`). 재발하면 작업 스레드 exact probe와 게임 스레드 경합부터 본다.
- 병렬 한계치가 750 → 약 700으로 내려온 원인(비 query 비용 증가로 추정)은 나누어 재지 않았다.
- exact query의 배치 단위, 관찰 거리 축의 근처 반경과 원거리 판정 주기는 아직 정하지 않았다.
- `LNPSurfaceSupport` 소비자 전환 시점은 Phase 5 착수 시 확정한다.
- 렌더링 호스트 프레임(300마리 P95 26ms, 1000마리 37ms, RTX 4060 Laptop)은 GPU 비용이며 Surface Navigation 범위 밖이다. 렌더링 트랙에서 다룬다.
- Development package의 기존 Lyra Mannequin material은 누락 Material Function 때문에 default material로 대체된다. Surface Navigation 검증과는 분리된 콘텐츠 문제다.

- `-game` 2P 스모크 게스트의 Mover SimulatedProxy 시작 위치 경고 중 월드 반지름을 한참 벗어난 위치(최대 약 1.5km)가 많다. 3c 스모크에도 있었으므로 Surface Navigation 회귀는 아니고 원인은 조사하지 않았다(`history/Phase04a_Log.md`).

## 블로커

현재 확인된 블로커는 없다.

## 마지막 검증

2026-09-27 Phase 4b 구현 단위 3: 에디터 빌드 성공, 세 옥탄트 m=4 `BakeOctant` 재저장, 자동화 `LootNPop.SurfaceNavigation` 53/53(로그 `Saved/Logs/Auto4b_U3.log`). 지각 Atlas 지표는 무회귀다.

2026-09-27 Phase 4b 구현 단위 2: 에디터 빌드 성공, 자동화 52/52.

2026-09-27 Phase 4b 구현 단위 0: Development 패키지 리슨 2P(`-LNPLoadBaseline=50`, 로그 `Saved/Logs/FaceIndex4b_*`)에서 호스트·게스트 모두 `ProbeFaceIndex` 2,000/2,000 PASS, `ProbePanels` PASS.
