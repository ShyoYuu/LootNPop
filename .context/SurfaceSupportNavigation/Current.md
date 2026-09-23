# Surface Support·Navigation 현재 작업 상태

> 상태: 활성
> 현재 Phase: Phase 3 — MassWorldCollision 정확성 기준선 · 착수 준비
> 마지막 갱신: 2026-09-23

## 현재 목표

Phase 2는 완료됐고, 2026-09-23 착수 전 설계 검토 결과를 문서에 반영했다(`history/Phase03_Log.md`). 다음 세션은 Phase 3 실행 문서를 작성하는 것부터 시작한다.

Phase 3 범위(`Roadmap.md` §4):

1. D-025 스레드·쿼리 방침과 Gate 0: Mass worker 동기 scene query를 `-game` 리슨 서버와 비동기 물리 조건에서 검증
2. Placement Marker → 서버 스폰 복제 Actor, 결정론적 움직임의 동적 패널 위 2P Mover 탑승
3. `LNPWorldExact` 기반 MassWorldCollision API
4. 투사체 서버·클라이언트 ghost·탄도 가이드의 exact 전환과 최외곽 반지름 안전망
5. 고정 부하 시나리오의 비용 기준선

Phase 3는 Support Atlas payload를 생성하거나 소비하지 않는다. 실제 Support Atlas 생성은 Phase 4a·4b 범위다.

Phase 3 다음은 exact 전용 부유섬 프로토타입(Phase 3b)과 완전 비행 NPC(Phase 3c)다. Phase 3의 부하 기준선은 3b 한계치 측정과 같은 계측 도구를 쓰도록 만든다.

## 착수 시 필수 문서

- `Roadmap.md`
- `design/RuntimeCollision.md`
- `design/DynamicTerrain.md`
- `design/TerrainContract.md`
- `design/RegressionMap.md`
- `research/ChaosSceneQueries.md`

## Phase 2 인계 기준선

- `FLNPOctantDefinition`과 `ULNPOctantSurfaceData`의 runtime 공개 타입 사용 가능
- header에서 data version, source manifest/hash와 네 payload descriptor 확인 가능
- 기존 `OctantPool`과 새 `OctantDefinitions` 병존, legacy 필드 유지
- 같은 seed와 slot mask에서 definition 전체를 결정론적으로 선택·보존
- source LVI·직접 external package·역할 mesh와 semantic/settings 값을 제한적으로 hash
- 최소 SurfaceData의 editor package 저장·재로드와 Windows cooked runtime 로드 검증 완료
- 최소 fixture의 네 payload는 codec 계약이 아닌 직렬화 검증용 sentinel byte다

## 바로 다음 작업

1. 위 다섯 범위로 `phases/Phase03_*.md` 실행 문서와 완료 조건을 작성한다. 완료 조건에 2P 스모크(D-031)를 넣는다.
2. 부하 시나리오의 적 수·CombatMode 비율·동시 투사체 수를 정한다. 목표 구성은 적의 90% 이상이 PureEntity다.
3. Gate 0 스파이크부터 착수한다. 실패하면 D-025를 재논의한다.

## Phase 1에서 확정된 입력 계약

- 기본 authoring: 비-WP 일반 Static Mesh + Sphere Height Sculpt
- 선택적 authoring: 별도 WP Mesh Terrain + 독립 Static Mesh 재구성
- runtime geometry: authoring graph에 의존하지 않는 독립 `UStaticMesh`
- exact collision: cooked `UBodySetup`과 `CTF_UseComplexAsSimple`
- 의미 metadata: source `UPrimitiveComponent`의 Terrain Contract tag와 collision profile
- slot transform: `(Pitch 0°/180°, Yaw 0°/90°/180°/270°)` 8개
- stale 검출 후보: source LVI·외부 actor/object·역할 mesh·semantic 값·schema version을 정렬한 manifest와 package saved hash

## 이관된 후속 작업

- production Terrain Contract tag·collision profile 마이그레이션은 실제 베이커를 production source에 적용하는 Phase 4 이후에 수행한다.
- production definition의 SurfaceData 연결과 runtime 로드는 Phase 5 소비자 전환에서 수행한다.
- 실제 Support Atlas rasterization과 payload codec은 Phase 4a·4b 범위다.
- `LNPOctantSourceCollector`의 무태그 충돌 컴포넌트 보고와 LVI 내 동적 태그 차단은 Phase 4 착수 전에 수정한다.
- greybox 부유섬 옥탄트 LVI는 Phase 3b에서 만들고, Phase 4 착수 전에 동굴 키트 공동 모듈과 통로를 추가한다(`Roadmap.md` §4).
- Phase 4 착수 전 전제: fixture 재배치·fixture LVI, 동굴 fixture의 키트 방식 교체.

## 알려진 불확실성

- Phase 3 exact query의 호출 빈도와 배치 단위는 정확성 기준선 측정 뒤 결정한다.
- 관찰 거리 축의 근처 반경과 원거리 판정 주기는 Phase 3b 실측으로 정한다.
- `LNPSurfaceSupport`와 `LNPWorldExact` 신규 channel로 기존 소비자를 전환하는 시점은 Phase 3 회귀 결과와 함께 확정한다.
- Mover가 서버 스폰 복제 패널을 movement base로 인식했을 때 2P 예측이 안정적인지는 Phase 3 스파이크로 확인한다.
- Nanite mesh의 complex collision이 원본 mesh와 fallback mesh 중 어디서 만들어지는지는 Phase 4a 착수 시 확인한다.
- Development package의 기존 Lyra Mannequin material은 누락 Material Function 때문에 default material로 대체된다. Surface Navigation 검증과는 분리된 콘텐츠 문제다.

## 블로커

현재 확인된 블로커는 없다.

## 마지막 검증

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
