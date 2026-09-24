# Surface Support·Navigation 현재 작업 상태

> 상태: 활성
> 현재 Phase: Phase 3 — MassWorldCollision 정확성 기준선 · Gate -1 진행 중
> 마지막 갱신: 2026-09-24 (Gate -1 B 2차 — hit identity registry)

## 현재 목표

Phase 2는 완료됐고, 2026-09-23 착수 전 설계 검토 결과를 문서에 반영했다(`history/Phase03_Log.md`). Phase 3 실행 문서는 `phases/Phase03_MassWorldCollisionBaseline.md`다. Gate -1 핵심 항목과 Gate 0 스파이크의 에디터 조건 측정까지 끝났다.

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

Gate -1 A는 audit·마이그레이션까지 끝났다(8-slot oracle과 비교 CVar는 wrapper 이후). Gate -1 B는 registry 게시와 proxy swap까지 끝났고 lifecycle gate, 동적 패널 분류, fixture 기반 sheet·shell 검증이 남았다. 부하 시나리오 수치는 합의돼 Phase 문서 §4에 고정됐다.

Gate 0 스파이크(`SurfaceNavigation/LNPExactQuerySpike.*`)는 PIE와 에디터 바이너리 `-game` 리슨 2P에서 worker 실행, ensure 0, 미해석·분류 오류 0, 옥탄트 결과 일치를 확인했다. 질의 1회는 평균 약 8~9us(에디터 빌드)이며, 합계 P95 ≤ 2ms 기준이면 프레임당 약 200회가 한도다(`history/Phase03_Log.md` 2026-09-24 Gate 0). 남은 항목은 게임 락 조건의 측정이다.

1. ~~패키지 실행이 옥탄트 로드에서 멈추는 문제~~ — **해결됨(2026-09-24).** 엔진의 `ALevelInstance::SetWorldAsset`은 `WITH_EDITOR` 전용이라 패키지에서는 `ILevelInstanceInterface`의 no-op(`return false`)으로 떨어지고, 월드 에셋이 빈 채로 `RequestLoadLevelInstance`가 조용히 무시돼 `IsLoaded()`가 계속 false였다. `ALNPOctantLevelInstance::SetRuntimeWorldAsset`이 패키지에서 `CookedWorldAsset`을 직접 채운다. 패키지 리슨 서버 `Server init complete`, 클라이언트(`127.0.0.1`) 옥탄트 8개 로드·베이킹·폰 스폰까지 확인했다.
2. 해결 뒤 패키지 리슨 서버·클라이언트를 `-ExecCmds="LNP.SurfaceNav.ExactSpike.QueriesPerFrame 1024, LNP.SurfaceNav.ExactSpike.MovingBodies 64, LNP.SurfaceNav.ExactSpike.AutoCapture 1"`로 띄워 락 대기를 측정하고 Gate 0을 판정한다. 에디터 바이너리 `-game`도 `WITH_EDITOR` 락이라 대체할 수 없다.

Gate 0 판정 뒤에는 구현 단위 1(MassWorldCollision API)로 간다. 스파이크의 질의·해석 코드가 wrapper의 출발점이다.

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

## 알려진 불확실성

- Phase 3 exact query의 호출 빈도와 배치 단위는 정확성 기준선 측정 뒤 결정한다.
- 관찰 거리 축의 근처 반경과 원거리 판정 주기는 Phase 3b 실측으로 정한다.
- `LNPSurfaceSupport` 소비자 전환 시점은 Phase 3 회귀 결과와 함께 확정한다. `LNPWorldExact` 소비자 전환은 production response 마이그레이션과 audit가 통과한 뒤에만 허용한다.
- Mover가 서버 스폰 복제 패널을 movement base로 인식했을 때 2P 예측이 안정적인지는 Phase 3 스파이크로 확인한다.
- Nanite mesh의 complex collision이 원본 mesh와 fallback mesh 중 어디서 만들어지는지는 Phase 4a 착수 시 확인한다.
- Development package의 기존 Lyra Mannequin material은 누락 Material Function 때문에 default material로 대체된다. Surface Navigation 검증과는 분리된 콘텐츠 문제다.

## 블로커

현재 확인된 블로커는 없다.

## 마지막 검증

2026-09-24 Gate 0 스파이크:

- `LootNPopEditor`·`LootNPop Win64 Development` 빌드 성공, 경고 없음. Win64 Development BuildCookRun 성공
- PIE 리슨 2P와 에디터 바이너리 `-game` 리슨 2P에서 1024 q/frame과 동적 body 62개: worker 실행, ensure 0, `UnknownHits=0`, `ClassificationErrors=0`, 옥탄트 slot 결과 차이 0
- 패키지 실행은 옥탄트 로드 대기에서 멈춰 측정하지 못했다 → 같은 날 해결됨(바로 다음 작업 1번). 락 측정은 아직 하지 않았다

2026-09-24 Gate -1 B 2차:

- `LootNPop.SurfaceNavigation` 자동화 12/12 통과(신규 `HitIdentity.LootPodProxySwapRemap` 포함)
- 리슨 서버 2P PIE: 서버·클라이언트 slot source 32개 등록(미분류 0), registry 해석 일치, `UnknownHits=0`

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
