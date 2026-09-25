# Surface Support·Navigation 현재 작업 상태

> 상태: 활성
> 현재 Phase: Phase 3 — MassWorldCollision 정확성 기준선 · Gate -1·Gate 0 통과, 구현 단위 1~4 완료
> 마지막 갱신: 2026-09-25 (Gate -1 B 잔여 종료: 동적 패널 분류 검증, lifecycle 구조 충족, fixture 항목 Phase 4 이관)

## 현재 목표

Phase 2는 완료됐고, 2026-09-23 착수 전 설계 검토 결과를 문서에 반영했다(`history/Phase03_Log.md`). Phase 3 실행 문서는 `phases/Phase03_MassWorldCollisionBaseline.md`다. Gate -1 A(audit·마이그레이션·8-slot oracle), Gate -1 B 핵심 항목, Gate 0(패키지 게임 락 포함), 구현 단위 1(MassWorldCollision API), 구현 단위 2(투사체 exact 기본·envelope 안전망·legacy 제거), 구현 단위 3(Placement Marker·움직이는 패널·2P Mover 탑승)까지 끝났다.

Phase 3 범위(`Roadmap.md` §4):

1. Gate -1: production exact collision response audit·마이그레이션, hit identity registry 계약, slot→Level Instance lifetime 계약
2. D-025 스레드·쿼리 방침과 Gate 0: Mass worker 동기 scene query를 `-game` 리슨 서버와 비동기 물리 조건에서 검증
3. `LNPWorldExact` 기반 MassWorldCollision API와 worker-safe POD hit identity 변환
4. 투사체 서버·클라이언트 ghost·탄도 가이드의 exact 전환과 최외곽 반지름 안전망
5. Placement Marker → 서버 스폰 복제 Actor, 결정론적 움직임의 동적 패널 위 2P Mover 탑승
6. 고정 부하 시나리오의 비용 기준선

Phase 3는 Support Atlas payload를 생성하거나 소비하지 않는다. 실제 Support Atlas 생성은 Phase 4a·4b 범위다.

Phase 3 다음 핵심 경로는 exact 전용 부유섬 프로토타입(Phase 3b)→Phase 4a다. 완전 비행 NPC(Phase 3c)는 Phase 3 exact query만 의존하는 병렬 분기이며 Phase 4a를 막지 않는다. Phase 3의 부하 기준선은 3b 한계치 측정과 같은 계측 도구를 쓰도록 만든다.

## 착수 시 필수 문서

- `Roadmap.md`
- `design/RuntimeCollision.md`
- `design/DynamicTerrain.md`
- `design/TerrainContract.md`
- `design/RegressionMap.md`
- `research/ChaosSceneQueries.md`
- `phases/Phase03_MassWorldCollisionBaseline.md`

## Phase 2 인계 기준선

- `FLNPOctantDefinition`과 `ULNPOctantSurfaceData`의 runtime 공개 타입 사용 가능
- header에서 data version, source manifest/hash와 네 payload descriptor 확인 가능
- 기존 `OctantPool`과 새 `OctantDefinitions` 병존, legacy 필드 유지
- 같은 seed와 slot mask에서 definition 전체를 결정론적으로 선택·보존
- source LVI·직접 external package·역할 mesh와 semantic/settings 값을 제한적으로 hash
- 최소 SurfaceData의 editor package 저장·재로드와 Windows cooked runtime 로드 검증 완료
- 최소 fixture의 네 payload는 codec 계약이 아닌 직렬화 검증용 sentinel byte다

## 바로 다음 작업

Gate -1 A, Gate 0, 구현 단위 1~4가 끝났다. 부하 기준선은 패키지 Development 호스트 `-nullrhi`의 서버 CPU 프레임으로 판정한다(2026-09-25 사용자 결정). 1000마리 P95 6.05ms, exact·락 기준도 통과했다. 렌더링 호스트의 실패(GPU 대기, RTX 4060 Laptop)는 기준선으로만 남긴다. harness는 `-LNPLoadBaseline=N`, 측정·분석 절차는 `history/Phase03_Log.md` 2026-09-25 분해 절.

Gate -1 B도 닫혔다. 동적 패널 분류는 자동화와 리슨 2P `ProbePanels`로 확인했고, lifecycle gate는 match 중 reset·unload 경로가 없어 구조로 충족한다(도입 시 gate 필수). disconnected sheet·double-sided shell은 Phase 4 착수 전으로 이관했다(`history/Phase03_Log.md` 2026-09-25 Gate -1 B 절).

1. **Phase 3 완료 조건 정리.** 자동화·회귀 표의 미완 항목(회귀 맵 exact hit, Decoration miss·Pawn 제외, world/entity earliest hit, Editor 자동화)과 `-game` 리슨 2P 스모크(D-031), 두 타깃 빌드를 확인하고 `Roadmap.md`를 갱신한다.

## Phase 1에서 확정된 입력 계약

- 기본 authoring: 비-WP 일반 Static Mesh + Sphere Height Sculpt
- 선택적 authoring: 별도 WP Mesh Terrain + 독립 Static Mesh 재구성
- runtime geometry: authoring graph에 의존하지 않는 독립 `UStaticMesh`
- exact collision: cooked `UBodySetup`과 `CTF_UseComplexAsSimple`
- 의미 metadata: source `UPrimitiveComponent`의 Terrain Contract tag와 collision profile
- slot transform: `(Pitch 0°/180°, Yaw 0°/90°/180°/270°)` 8개
- stale 검출 후보: source LVI·외부 actor/object·역할 mesh·semantic 값·schema version을 정렬한 manifest와 package saved hash

## 이관된 후속 작업

- production Terrain Contract Component Tag 마이그레이션은 실제 베이커를 production source에 적용하는 Phase 4 이후에 수행한다. 단, `LNPWorldExact` collision response/profile 마이그레이션은 D-036에 따라 Phase 3 Gate -1에서 먼저 수행한다.
- production definition의 SurfaceData 연결과 runtime 로드는 Phase 5 소비자 전환에서 수행한다.
- 실제 Support Atlas rasterization과 payload codec은 Phase 4a·4b 범위다.
- `LNPOctantSourceCollector`의 무태그 충돌 컴포넌트 보고와 LVI 내 동적 태그 차단은 Phase 4 착수 전에 수정한다.
- `LNPOctantSourceCollector`의 tag/profile/channel 검증과 marker authoring hash를 Phase 4·8 스키마에 맞춰 보강한다. owned external package를 모두 hash해 decoration 저장도 stale이 되는 현재 보수 정책은 보고서에 명시하고, false stale이 실제 문제가 될 때만 필터링한다.
- C-option 실험 에셋과 테스트의 구형 `LNP.Terrain.*` Component Tag는 Phase 4 입력으로 재사용하기 전에 현재 `LNP.Surface.*` 계약으로 마이그레이션한다.
- 현재 slot 순서 greedy definition 선택은 여러 slot mask가 있는 production pool을 도입하기 전에 최대 고유 제약 할당으로 교체한다(D-043).
- Phase 3b greybox 부유섬 옥탄트부터 기준 반지름 30,000cm로 제작하고 `SphereRadius`를 함께 올린다(D-046). int16 복제 캡은 좌표 성분마다 걸리므로 옥탄트 꼭짓점(좌표축) 부근에서만 여유가 약 2,767cm로 좁다. 동굴은 꼭짓점 부근을 피한다(`design/TerrainContract.md` §7). 기존 `Meadow_00`(25,000cm)는 30,000cm로 새로 만들거나 폐기한다.
- greybox 부유섬 옥탄트 LVI는 Phase 3b에서 만들고, Phase 4 착수 전에 동굴 키트 공동 모듈과 통로를 추가한다(`Roadmap.md` §4).
- Phase 4 착수 전 전제: fixture 재배치·fixture LVI, 동굴 fixture의 키트 방식 교체.
- Phase 3 Gate -1 B에서 이관: 한 component의 disconnected sheet face identity, 내부형 double-sided shell의 hit normal·walkable 판정. `design/RegressionMap.md`의 Phase 4 착수 전 fixture로 검증한다.
- match 중 옥탄트 재생성이나 slot Level 언로드를 도입하면 그 직전에 Mass 처리를 멈추는 gate를 함께 만든다(`design/RuntimeCollision.md`).

## 알려진 불확실성

- Phase 3 exact query의 호출 빈도와 배치 단위는 정확성 기준선 측정 뒤 결정한다.
- 관찰 거리 축의 근처 반경과 원거리 판정 주기는 Phase 3b 실측으로 정한다.
- `LNPSurfaceSupport` 소비자 전환 시점은 Phase 3 회귀 결과와 함께 확정한다. `LNPWorldExact` 소비자 전환은 production response 마이그레이션과 audit가 통과한 뒤에만 허용한다.
- Nanite mesh의 complex collision이 원본 mesh와 fallback mesh 중 어디서 만들어지는지는 Phase 4a 착수 시 확인한다.
- Development package의 기존 Lyra Mannequin material은 누락 Material Function 때문에 default material로 대체된다. Surface Navigation 검증과는 분리된 콘텐츠 문제다.

## 블로커

현재 확인된 블로커는 없다.

## 마지막 검증

2026-09-25 Gate -1 B 잔여:

- `LootNPopEditor` 빌드 성공, 경고 없음. `LootNPop.SurfaceNavigation` 자동화 15/15(신규 `WorldCollision.DynamicMarkerHit`)
- 에디터 바이너리 `-game` 리슨 2P 무인(`-LNPLoadBaseline=1`, 발사체 0): 호스트·게스트 모두 `ProbePanels panels=8 failures=0 PASS`
- `LootNPop Win64 Development` 게임 타깃 빌드 성공, 경고 없음

2026-09-25 구현 단위 4(호스트 프레임 분해):

- harness capture 구간 trace region 추가, `LootNPopEditor` 빌드·Win64 Development BuildCookRun 성공
- 에디터 바이너리 `-game` 오염 두 가지 확인: `LNP.Debug.DrawEnemyAction` 기본 1(렌더 스레드 +16ms), `-nullrhi`의 수명 0 디버그 라인 누적(`LineBatchComponent` 약 29ms/프레임)
- 패키지 호스트 `-nullrhi` 1000마리·발사체 0: P95 6.05ms. 렌더링 호스트 P95: 1000·0 37.7ms, 1000·500 37.0ms, 300·500 26.0ms(GPU 오클루전 쿼리 대기 평균 12ms/프레임)
- 패키지 exact P95 0.94ms(발사체 500), 락 P95 0.046ms, 모든 조건 `UnknownHits=0`·`EnvelopeEscapes=0`

2026-09-25 구현 단위 4(부하 기준선):

- `LootNPopEditor`·`LootNPop Win64 Development` 빌드 성공, 경고 없음
- 에디터 바이너리 `-game` 리슨 2P 무인 실행(게스트 `-nullrhi`, Ryzen 7 8845HS). 모든 조건에서 `UnknownHits=0`, `EnvelopeEscapes=0`, 발사체 평균 502발
- 호스트 exact/frame P95: 300마리 1.249ms, 1000마리 1.665ms. 락 P95: 0.095ms, 0.128ms. 둘 다 통과
- 호스트 프레임 P95: 대조군(적 1) 11.0ms, 300마리 28.1ms, 1000마리 43.4ms로 실패. 발사체 0발로 돌려도 1000마리는 41.3ms
- 게스트 프레임 P95는 1000마리에서 약 16ms

2026-09-24 구현 단위 3(Placement Marker·움직이는 패널):

- `LootNPopEditor`·`LootNPop Win64 Development` 빌드 성공, 경고 없음. `LootNPop.SurfaceNavigation` 자동화 14/14
- PIE: 마커 1개 → 패널 8개 스폰 4.3ms, swept 반지름 25,435cm(envelope 26,928cm 안)
- 사용자 2P(에디터 바이너리 `-game` 리슨): late join 자세 일치, 양쪽 탄이 패널에서 막힘, 호스트·게스트 탑승 중 떨림·미끄러짐 없음
- 계측: 탑승 한 주기 패널 로컬 좌표 변화 7초 0.7cm. 점프 이탈 관성 전달률 0.99(5회), +0.3초 0.80은 `FallingDeceleration` 200cm/s²과 일치
- 1차 실패(절반 추종·관성 0)의 원인과 조치는 `history/Phase03_Log.md` 같은 날 구현 단위 3 절

2026-09-24 구현 단위 2 완료(envelope·oracle·legacy 제거):

- `LootNPopEditor`·`LootNPop Win64 Development` 빌드 성공, 경고 없음. `LootNPop.SurfaceNavigation` 자동화 14/14 통과(`WorldCollision.ProjectileArc`에 envelope 사례 추가)
- production envelope 26,928cm(기준 반지름 25,000cm), 수집 약 7ms
- `LNP.SurfaceNav.ExactOracle`: 에디터 바이너리 `-game` 단독·헤드리스 리슨 서버·클라이언트 모두 PASS(189,024 query, 실패 0, 정보 항목 EdgeMiss 약 835). legacy 제거 뒤 빌드에서도 2P PASS
- 2P 플레이 스모크(사용자): 프랍·지면 착탄과 ADS 가이드 정상. 1차에서 찾은 관전 Ghost 외삽 구간 누락을 고친 뒤 게스트 `UnknownHits=0`·`EnvelopeEscapes=0`, 탄 소실 없음

2026-09-24 구현 단위 2(투사체 exact 경로):

- `LootNPopEditor`·`LootNPop Win64 Development` 빌드 성공, 경고 없음
- 자동화 신규 `WorldCollision.ProjectileArc` 통과(바닥·벽 착탄, 무충돌 수명 끝, 분류 counter). 기존 13개는 같은 날 전체 실행에서 통과, 신규는 fixture 수정 뒤 `WorldCollision` 그룹 재실행에서 통과
- 에디터 바이너리 `-game` 리슨 2P(`TestMap03`), CVar 1: 호스트·게스트 모두 프랍 관통 없음, 프랍 뒤 적 피해 없음, 유탄이 프랍에서 폭발. CVar 0: 양쪽 모두 기존대로 프랍 통과
- 호스트 Report: `ProjectileMandatory count=40027 hits=1141 avg=2.08us max=306.50us`, `UnknownHits=0`, ensure 0

2026-09-24 Gate 0 패키지·구현 단위 1:

- `LootNPopEditor` 빌드 성공(경고 없음), Win64 Development BuildCookRun 성공
- 패키지 리슨 서버·클라이언트 2P와 서버 단독 2회(body 62/0): worker 실행, ensure 0, `UnknownHits=0`, `ClassificationErrors=0`, 옥탄트 slot 칸 차이 0(패키지↔에디터 포함)
- `LootNPop.SurfaceNavigation` 자동화 13/13 통과(신규 `WorldCollision.Api`)
- `LootNPop Win64 Development`(게임 타깃) 빌드 성공, 경고 없음. 새 wrapper는 아직 패키지 런타임에서 호출되지 않는다(소비자 없음)

Phase 2 인계 시점:

- `LootNPopEditor Win64 Development`: 성공, Editor 종료 상태의 전체 빌드
- `LootNPop Win64 Development`: 성공
- Phase 2 schema 자동화: 직렬화·선택·dependency·hash·package save/reload 5/5 통과, 오류·경고 없음
- 실제 `/Game/SurfaceNavigationTests/Schema/DA_MinimalOctantSurfaceData` 저장·package reload·object path 재로드 성공
- Windows 단일-package cook: 성공, cooked `.uasset`·`.uexp` 생성
- Win64 Development BuildCookRun: 성공, 953 package cook과 archive 생성
- packaged runtime `MinimalSurfaceDataCookedLoad`: 1/1 성공, 오류·경고 없음
- cooked header, manifest, Support·Navigation·Traversal·Spawn payload와 descriptor 보존 확인
- Editor 재시작 후 `OctantPoolData`: definition 1개와 legacy Level 1개 보존 확인
- PIE world generation: 8개 Level Instance spawn·visible load와 완료 이벤트 확인

## Phase 2 산출물

- 데이터 모델: `design/DataModel.md`
- 실행 기록: `history/Phase02_Log.md`
- 완료 조건: `phases/Phase02_OctantDataSchema.md`
- 최소 cooked output: `Saved/SurfaceNavigationPhase2Cook/`
- packaged 검증 archive: `Saved/SurfaceNavigationPhase2Package/Windows`
- packaged 자동화 보고서: `Saved/SurfaceNavigationPhase2Reports/index.json`
