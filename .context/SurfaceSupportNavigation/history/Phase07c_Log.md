# Phase 7c 작업 기록

## 2026-10-02 — 단위 0: 실행 계획과 기준선

- 사용자 요청으로 Phase 7c에 착수했다. 각 구현 단위 완료 뒤 계속 진행할지 확인하며, MCP·전체 빌드 선택은 에디터 개폐 상태가 아니라 작업 효율에 따른다.
- README·Current·Decisions·Roadmap과 관련 기준 설계를 읽고 실제 코드와 대조했다. sheet 분할 없음, 쌍당 portal 하나, 전 비지각 Layer 공통 m, Spawn headroom 없음이 문서와 일치했다. 시작 시 작업 트리는 깨끗했다.
- `phases/Phase07c_VolumetricTerrainBaking.md`를 작성했다. 단위 1은 격자에 의존하는 분할과 source별 해상도를 함께 구현하고, 단위 2는 여러 portal과 분할 경계 Nav 연결을 검증한다. 이후 headroom, 회귀 콘텐츠, cooked·2P·성능 Gate로 나눴다.
- Portal 간격·headroom 측정 규약은 해당 단위에서 코드·Config·측정 근거로 확정한다. 천장 교전 clamp는 플레이 검증이 필요성을 보일 때만 별도 제안한다.
- Current·Roadmap을 갱신했다. 문서 참조 경로 확인·`git diff --check` 통과. 코드·에셋 변경과 빌드·자동화·플레이는 없다.
- MCP 도구는 현재 세션 카탈로그에 노출되지 않았다. 라이브 작업에 필요할 때 연결 요청 후 읽기 호출로 검증한다.
- 다음 행동: 사용자 진행 확인 뒤 단위 1 착수.

## 2026-10-02 — 단위 1: source별 해상도·접힌 sheet 분할

- 사용자 진행 확인을 받아 구현했다. 실제 raster와 같은 footprint·watertight 방사 광선으로 sheet의 삼각형별 격자 교차를 수집하고, 겹치는 sheet만 최소 external face 시드·정렬된 이웃 BFS로 분할한다. 격자점 반지름 범위가 HitMergeDistance를 넘으면 현재 part에서 거부한다. 기존 Layer 순서와 external face 표는 유지한다. 지각은 분할하지 않으며 raw folded mesh/overhang 오류를 완화하지 않았다.
- source의 `LNP.Surface.CoarseSupport`를 추출 bool·계약 검증·semantic hash에 반영했다. 지각 N을 기준으로 source별 m=1/기본 m=4를 먼저 정하고, 분할과 최종 raster가 동일한 N·HitMergeDistance를 쓴다. Support 없는 태그는 오류다. codec v2·DataVersion 5를 유지하고 BakerSchemaVersion은 5→6으로 올렸다.
- 베이크 보고서는 분할 전 Sheets, 분할된 원래 sheet 수 split, sub-sheet 사이 공유 용접 모서리의 중복 없는 cutEdges·cutBoundary(cm)를 기록한다. 경계 측정 정의·BFS 규칙을 `design/SurfaceBaking.md`에 반영했다.
- `SupportAtlasFoldedSheet`는 나선 경사로의 자동 분할·각 part raster 성공·모든 face 1회 배정·다층 후보·반복/삼각형 역순의 face 배정 일치를 검사한다. 신규 `SupportAtlasSourceResolution`은 mixed codec·coarse 조회, 신규 `OctantSourceResolution`은 실제 fixture source 태그→hash→두 sheet의 coarse 베이크→payload 감소를 검사한다. 태그는 scope exit에서 원래 값으로 복구하고 LVI를 저장하지 않았다. 계약·hash 기존 검사에도 CoarseSupport 사례를 추가했다.
- 전체 빌드 최초 49.97초, 통합 검사 추가 후 최종 6.45초·exit 0. 최종 빌드 로그는 `Saved/Logs/Phase07c_Unit1_Build.log`다. SupportAtlas 5/5·exit 0(`Phase07c_Unit1_SupportAtlasTests.log`), 재베이크+전체 자동화 82/82·실패/오류 0·exit 0(`Phase07c_Unit1_AllTestsAndRebake.log`)이다. 기존 `Nav.RequestCostReplay`는 CSV 인자 없이 건너뛰었으며 CSV 재생을 새로 실행한 증거는 아니다.
- 세 SurfaceData를 새 schema/settings hash로 재베이크했다. Crust/Regression/Meadow는 Layer 5/7/11, Nav cell 68,500/69,378/67,729, portal 0/2/4이며 기존 콘텐츠의 분할 수는 0이다. 저장본 결정론·exact Layer identity·이음매·A*·scheduler·Pod overlay·이동 회귀를 통과했다.
- 재베이크 전 세 SurfaceData와 자동화 재저장 대상 세 테스트 에셋을 `Saved/Phase07c_Unit1_Backup`에 백업했다. 자동화 종료 뒤 테스트 에셋 세 개를 복구하고 SHA256 일치를 확인했다. 세 SurfaceData의 새 베이크는 유지한다. `git diff --check` 통과.
- 에디터 프로세스가 없는 것을 확인한 뒤 전체 빌드했다. 이번 source 계산·재베이크·자동화는 기존 headless 명령으로 검증했고 라이브 MCP가 필요한 에셋 제작·PIE 조작은 수행하지 않았다. 패키지·2P·성능 측정은 단위 5 Gate, 분할 경계 Nav 연속성은 단위 2의 미완료 항목이다.
- 다음 행동: 사용자 진행 확인 뒤 단위 2(복수 portal·분할 경계 Walk 연결) 착수.

## 2026-10-02 — 단위 2: 복수 portal·분할 경계 Walk 연결

- 사용자의 이어서 진행 요청을 단위 2 착수 확인으로 받아 구현했다. component 쌍별 정밀 검사 통과 후보를 endpoint 거리 제곱·canonical A/B 순으로 정렬하고, 선택한 연결 중점과 200cm 미만인 중복만 억제한다. 200cm는 비지각 Nav 목표 간격 두 칸이며 portal 개수 상한은 두지 않는다. 간격·동률·측정 정의는 `design/GroundNavigation.md`에 기록했다.
- 기존 exact Support polyline·양방향 capsule sweep과 거리·step 필터는 유지했다. `PortalSpacingRejectCount`·`PortalBakeSeconds`를 보고한다. Traversal codec v1·DataVersion 5는 유지하고 schema 6→7·settings hash에 선택 의미를 반영했다.
- 신규 `Nav.MultiplePortals`는 같은 component 쌍의 두 입구 보존·시작/목표 위치별 가까운 입구 A* 선택·벽 구간 연결 거부·반복 payload 결정론·긴 경계 최소 간격을 검사한다. 긴 경계 통과 후보 139개에서 portal 7개를 남겼고 132개를 억제했다(약 0.1ms).
- `SupportAtlasFoldedSheet`에 실제 나선 삼각형의 50cm 간격 지지점·step oracle과 Nav/A* 검사를 더했다. 처음에는 기존 Support 전용 184cm 합성 격자에서 Nav 후보 0개로 실패했다. 생산 콘텐츠와 같은 지각 N=735·비지각 m=4(25cm)로 맞춘 뒤 Layer 3·portal 6·거리/step/지지 경로 후보 1,942/1,404/347, A* 32 node·Layer 전이 1회로 통과했다(16.620ms). 이 순수 oracle은 삼각형 지지 경로를 검증하며 Chaos capsule sweep 증거는 저장 회귀 동굴·production 베이크가 맡는다.
- 첫 빌드는 새 테스트의 람다 `INDEX_NONE` 반환형 추론 오류로 실패했고 명시적 `int32`로 수정했다. 이후 전체 빌드 성공(5.13초), 나선 입력 해상도 수정 후 전체 빌드 성공(5.67초). 집중 자동화 최종 2/2·exit 0(`Saved/Logs/Phase07c_Unit2_FocusedTests_Final.log`)다. 최초 실패 로그는 `Phase07c_Unit2_FocusedTests.log`에 보존했다.
- 재베이크 전 SurfaceData 세 개와 자동화 재저장 대상 세 테스트 에셋을 `Saved/Phase07c_Unit2_Backup`에 백업했다. schema 7 베이크에서 Crust/Regression/Meadow의 Nav cell·component는 기존 68,500/5, 69,378/7, 67,729/91을 유지했다. portal은 0/8/24(이전 0/2/4), Traversal payload는 14,447/14,623/14,978 B(증가 0/120/400 B)다. Support·Nav payload 크기는 기존과 같고 거리/step/정밀 통과 후보도 36/0/0, 3,859/2,448/477, 15,727/11,926/1,029로 같다. portal 처리 시간은 0.011/0.060/0.326초, 간격 억제 수는 0/469/1,005다. 측정은 동일 장면의 1회 베이크이며 반복 성능 Gate가 아니다.
- 저장본 결정론 검사의 기존 `portal 수=2` oracle은 복수 portal 보존과 기대 Layer 쌍 정확히 두 개를 검사하도록 바꿨다. 동굴 room↔corridor↔crust 연결과 예상 밖 연결 차단을 유지한다.
- 최초 재베이크·전체 실행은 82/83 통과이며 실패는 이전 `portal 수=2` oracle 한 건이다(`Saved/Logs/Phase07c_Unit2_AllTestsAndRebake.log`). 수정 후 최종 전체 빌드 6.93초·exit 0(`Phase07c_Unit2_Build.log`), 전체 자동화 83/83·실패/자동화 오류/assert/ensure 0·exit 0(`Phase07c_Unit2_AllTests_Final.log`)이다. 저장본 결정론·face identity·이음매·동굴 A*·scheduler·Pod overlay·이동 회귀를 통과했다. `Nav.RequestCostReplay`는 CSV 인자 없이 건너뛰었으며 CSV 재생을 새로 실행한 증거는 아니다.
- 각 전체 자동화 종료 뒤 재저장 대상 테스트 에셋 세 개를 실행 전 백업으로 복구하고 SHA256 일치를 확인했다. 새 schema 7 SurfaceData 세 개는 유지했다. `git diff --check` 통과다. 이번 단위는 순수 계산·기존 headless 에디터 베이크·자동화가 효율적이어서 라이브 MCP·콘텐츠 제작·PIE를 사용하지 않았다. 패키지·2P·성능은 단위 5 Gate다.
- Current·Roadmap·Decisions·Phase 체크리스트와 기준 설계를 실제 구현 상태로 갱신했다. 다음 행동: 사용자 진행 확인 뒤 단위 3(Spawn headroom·비행 Pod 할당) 착수. 이번 단위 종료 뒤 자동으로 착수하지 않는다.

## 2026-10-02 — 단위 3: Spawn headroom·비행 Pod 할당

- 사용자 진행 확인을 받아 구현했다. 베이크 전용 physics world에서 authored anchor와 모든 random candidate의 지역 Up 방향으로 반지름 50cm 구를 exact sweep한다. 바닥 법선에 따른 시작 높이 보정, 구 중심을 넘지 않는 종료 높이, 복제 좌표 캡, 시작 겹침 0, 무충돌 유한 상한 규약을 `design/MovementIntegration.md`에 기록했다. Flight Config는 베이크 hash에 포함하지 않는다.
- Spawn codec 1→2로 올리고 authored·random record 끝에 `Headroom` float 4바이트를 더했다. 0~32767cm 유한값만 encode/decode하며 음수·상한 초과·infinity·NaN과 옛 v1을 거부한다. `DataVersion` 5→6·baker schema 7→8을 함께 올렸다. 로더는 기존 version/codec 검증으로 옛 에셋을 게시하지 않고 decoded Spawn struct의 headroom을 같은 snapshot으로 전달한다.
- 실제 `LiftFlyingSpawn`의 몸 구 규칙 `max(CapsuleRadius, CapsuleHalfHeight)`를 따라 필요 여유를 `max(IdleAltitudeMin, IdleAltitudeMax) + 몸 구 + Flight.Clearance + 200cm`로 정했다(기본 2,638cm). 게임 스레드에서 EntityConfig 부모 체인의 첫 EnemyTrait를 조회해 실제 생성 수가 양수인 편성만 계산하고 순수 planner에는 float를 넘긴다. 지상 Config는 0이다.
- 세트별 최대 필요 여유를 authored-for-set·generic·random Pod 경로 모두에 적용했다. 주변 비행 적 시작점에도 종류별 여유 필터를 더해 Pod 옆 낮은 천장 아래에 비행 적이 배치되지 않게 한다. 기존 순서·slot·Layer·반경·간격·총량·shortfall 규칙은 유지한다. 낮은 후보는 소비하지 않으며 Config 변경은 같은 snapshot의 할당만 바꾼다.
- 신규 `Runtime.MassSpawnHeadroom`은 임계값 미만/같음/초과, 모든 Pod 배치 경로, 부족 shortfall·unused authored, 같은 snapshot의 요구값 변경과 authored 우선순위 복귀, 지상·비행 혼합 편성의 주변 적 필터, Config 기본값·변경을 검사한다. 기존 `Runtime.MassSpawnPlanning`은 headroom 0의 지상 편성 동작을 유지한다. `Bake.SpawnCodec`에 새 필드 round trip·버전·비유한값 검사를 추가했다. 저장본 결정론 검사는 실제 sweep으로 구운 open sky와 낮은 동굴 authored headroom도 검사한다.
- 최초 빌드는 테스트에 사용한 `TNumericLimits<float>::Infinity()`가 엔진 API에 없어 실패했다. 표준 `std::numeric_limits`로 수정한 뒤 성공했고, 최종 빌드는 6.30초·exit 0(`Saved/Logs/Phase07c_Unit3_Build.log`)이다. 전체 SurfaceNavigation 84/84·실패/자동화 오류/assert/ensure 0·exit 0(`Phase07c_Unit3_AllTestsAndRebake.log`)를 통과했다. 기존 `Nav.RequestCostReplay`는 CSV 인자 없이 건너뛰었다.
- DataVersion 6·schema 8로 세 SurfaceData를 재베이크했다. Crust/Regression/Meadow Spawn payload는 422,932/424,844/399,612 B로 각 42,292/42,476/39,960 B 증가했다(기록당 4바이트). authored/random 수는 0/10,573, 3/10,616, 0/9,990으로 같고 Support·Nav·Traversal 크기와 portal 0/8/24를 유지했다. random headroom 최소/최대는 797.67/29,999.00, 337.70/29,998.98, 0/31,760.64cm다. Meadow의 0은 시작 겹침 등 비확신 공간을 비행 후보에서 차단하는 값이며 지상 배치에는 영향을 주지 않는다.
- `Saved/Phase07c_Unit3_Backup`에 실행 전 SurfaceData·테스트 에셋 각 세 개를 백업했다. 자동화 종료 뒤 테스트 에셋 세 개를 복구하고 SHA256 일치를 확인했으며 새 SurfaceData는 유지한다. 저장본 결정론·loader·face identity·이음매·Nav·Pod overlay·이동 회귀와 `git diff --check`를 통과했다. 에디터는 닫힌 상태였고 기존 headless 베이크·자동화가 효율적이어서 MCP·PIE·패키지·2P는 실행하지 않았다. 실제 높은 공동·낮은 천장 플레이는 단위 4, cooked·멀티플레이·성능은 단위 5다.
- Current·Phase·Roadmap·D-073 구현 상태와 관련 기준 설계를 갱신했다. 다음 행동: 사용자 진행 확인 뒤 단위 4(입체·지하 공동 회귀 콘텐츠) 착수. 라이브 에셋 제작을 위해 MCP가 효율적이면 즉시 연결을 요청한다.
