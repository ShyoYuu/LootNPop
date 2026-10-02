# Phase 7c — 입체 지형 베이크 대응

> 상태: 진행 중 — 단위 0·1·2·3 완료, 단위 4 진행 확인 대기
> 착수일: 2026-10-02
> 선행 조건: Phase 7a·7b 완료(700마리 반복 성능 Gate 포함)

## 1. 목표와 범위

한 Support 메시 안의 복층·나선 경사로를 자동 분할하고, 여러 입구·계단을 일반 A*가 선택할 수 있게 한다. 넓은 지하 공동은 source별 해상도로 베이크하고, 비행 편성 Pod는 충분한 headroom이 있는 위치에만 할당한다.

확정 결정은 D-064·065·070~073을 따른다. 지각 overhang은 계속 오류이며, 지상 traversal은 Walk만 허용한다. 수직 통로는 플레이어 동선이고 지상 Nav 연결을 만들지 않는다. 동적 지형은 Phase 8, 실제 품질 콘텐츠는 Phase 9, 활동 대역과 벽 타기는 Phase 13·14 범위다.

각 구현 단위를 검증하고 문서에 결과를 기록한 뒤 사용자에게 다음 단위 진행을 확인한다. 에디터 상태에 따라 구현 방식을 정하지 않는다. 라이브 에셋 조회·제작이 효율적이면 즉시 MCP 연결을 요청하고, 전체 빌드가 필요하면 즉시 에디터 저장·종료를 요청한다. 사용자의 PIE 플레이가 빠른 검증에는 별도 자동화 장치를 만들기 전에 수동 조작을 요청한다.

## 2. 착수 시 확인한 코드 기준선

- `LNPSupportLayers::BuildLayers`는 위치 용접·모서리 연결로 walkable sheet를 만들며 접힘 분할은 없다. source key와 최소 external face 순으로 Layer ID를 정하고 face 표를 만든다.
- `Bake.SupportAtlasFoldedSheet`는 1.3회전 나선 경사로가 Layer 하나가 된 뒤 raster 오류를 내는 것을 기대한다.
- Editor `LNPOctantSurfaceBaker.cpp`는 모든 비지각 Layer에 `Raster.Subdivisions * LayerSubdivisionMultiplier`를 적용한다. source별 해상도 선택은 없다.
- `LNPNavBaking.cpp`의 `BestPortalByComponentPair`는 exact clearance를 통과한 후보 중 component 쌍마다 하나만 저장한다. Traversal codec v1은 이미 portal 목록을 지원한다.
- `LNPSpawnData.h`의 authored anchor·random candidate에는 headroom이 없다. Spawn codec v1이며 순수 `LNPMassSpawnPlanning::BuildPlan` 입력에도 비행 여유 기준이 없다.
- 저장 에셋 기준은 `DataVersion=5`, `BakerSchemaVersion=5`다. 변경된 의미·payload는 재베이크와 stale 차단까지 같은 단위에서 처리한다.
- 시작 시 `git status --short`는 비어 있었다. MCP 도구는 현재 세션 카탈로그에 노출돼 있지 않으며, 라이브 작업 전에 연결 요청과 읽기 호출 검증이 필요하다.

## 3. 구현 단위와 검증

### 단위 0 — 실행 계획과 기준선(이번 단위)

- [x] 기준 설계와 실제 코드의 차이를 확인하고 단위별 의존성·검증을 정한다.
- [x] Current·Roadmap을 7c 착수 상태로 갱신한다.
- 검증: 문서 경로·코드 심볼 확인, `git diff --check`. 코드·에셋 변경과 빌드는 없다.

### 단위 1 — source별 해상도와 접힌 sheet 자동 분할

- [x] `LNP.Surface.CoarseSupport`를 source 수집·검증·hash에 반영한다. Support 없는 태그는 오류이며, 비지각 해당 source는 m=1, 그 외는 기존 기본 m=4를 사용한다(D-072). 같은 source의 모든 sub-sheet는 같은 해상도를 쓴다.
- [x] 분할 전에 지각 N과 source별 Layer 격자를 확정해 `BuildLayers`와 raster가 같은 격자·`HitMergeDistance`를 쓰게 한다.
- [x] D-064 설계대로 최소 external face부터 모서리 인접 삼각형을 확장한다. 동일 격자점에서 합침 거리보다 다른 반지름을 덮는 삼각형은 다음 sub-sheet로 미룬다. 순회와 동률 처리를 명시적으로 정렬하고 face를 누락·중복 배정하지 않는다. 지각은 분할하지 않는다.
- [x] 기존 source key·최소 external face Layer 순서와 face→Layer identity를 유지한다. 분할 수·경계 비용을 보고한다. 경계 길이의 측정 정의도 구현 시 설계에 기록한다.
- [x] 베이커 schema를 올리고 기존 세 SurfaceData를 재베이크한다. Support codec v2의 Layer별 subdivisions를 사용한다.
- 검증: 나선 경사로 분할·각 Layer raster 성공, 같은 방향의 다층 조회·external face 일치, 동일 입력 반복 결정론, 접힘 없는 기존 사례 유지, 지각 overhang 오류 유지, m=1·4 혼합 codec·조회와 태그 validation, 전체 빌드·SurfaceNavigation 자동화·저장본 결정론.
- 이 단위는 Support 분할을 검증한다. 분할 경계의 Nav 경로 연속성은 단위 2의 Gate다.

### 단위 2 — component 쌍당 여러 portal

- [x] exact Support polyline·양방향 capsule sweep 검증을 유지하면서 쌍당 하나인 선택 정책을 최소 간격을 둔 복수 portal로 바꾼다(D-065).
- [x] 간격 측정 위치·동률 순서·중복 억제를 설계에 명시한다. 입구 두 개를 보존하는 간격과 후보/검사 수·베이크 시간을 먼저 측정한다. 근거 없이 portal 상한으로 입구를 잘라내지 않는다.
- [x] 분할된 나선 경사로 경계, 긴 맞닿은 경계, 입구가 두 개인 Layer 쌍을 순수 회귀 입력으로 검사한다. 기존 Traversal codec을 유지하고 schema·재베이크를 반영한다.
- 검증: 두 연결이 모두 존재하고 시작·목표 위치에 따라 A*가 가까운 연결을 선택, 분할 경계 연속 Walk·불가능한 벽 관통 차단, portal·face 표 결정론, 기존 이음매·동굴·Pod overlay 회귀와 전체 자동화.

### 단위 3 — Spawn headroom과 비행 Pod 할당

- [x] authored anchor와 random Pod 후보 모두 bake-only exact 상향 sweep으로 여유 거리를 기록한다(D-073). sweep 형상·출발점·최대 측정 거리·무충돌 표현은 비행 Config와 복제 좌표 캡을 확인해 설계에 정한다.
- [x] Spawn codec과 DataVersion을 갱신하고 loader validation·snapshot 전파·전체 재베이크를 함께 처리한다. 형식 변경 전 에셋을 새 형식으로 조용히 해석하지 않는다.
- [x] 게임 스레드에서 비행 편성 Config의 `IdleAltitude` 상한·clearance·여유로 필요한 headroom을 계산해 순수 spawn plan에 넘긴다. 베이크는 고도 Config를 hash에 묶지 않는다.
- [x] authored-for-set → generic → random의 기존 우선순위와 총량·shortfall 규약을 유지하며 모든 배치 경로에 headroom 필터를 적용한다.
- 검증: 임계값 미만·같음·초과, 비행 없는 편성, 비행 혼합 편성, 수동 앵커와 random 필터, 후보 부족 shortfall, Config 변경 시 재베이크 없이 할당 변경, codec 잘못된 값·버전·결정론, 전체 빌드·자동화.

### 단위 4 — 입체·지하 공동 회귀 콘텐츠

- [ ] 기존 정적 fixture LVI에 복층/나선 경사로, 두 입구·두 계단, 경사로 충돌 proxy와 Decoration 계단, 높은 공동·낮은 천장, 분기·긴 경사로·Nav 미연결 수직 통로 사례를 추가한다. 고정 좌표와 기대 결과는 `design/RegressionMap.md`가 소유한다.
- [ ] 옥탄트 중심 근방 배치·Floor/Shell 분리·좌표 성분 int16 캡을 지킨다. 지각 입구 절단을 여러 입구·수직 통로로 확장한다. 문 폭 200cm는 콘텐츠 규칙이며 validation 오류로 추가하지 않는다(D-070).
- [ ] 큰 공동 바닥 m=1과 그 위 구조물 m=4를 함께 검사한다. 에셋 변경 전 백업·저장, 이후 재베이크·결정론을 확인한다.
- 검증: exact face identity·다층 query·유령 지면 없음·Layer 경계 이동·8-slot 도달성·양 입구 경로·수직 통로 NoPath·headroom 할당. Editor/MCP 관찰 뒤 사용자 PIE로 추격·낙하·높은 공동의 비행 교전을 확인한다.
- 천장 근처 비행 교전이 어색한 경우에만 상향 probe 고도 clamp를 별도 구현 단위로 제안한다. 현재는 검증 항목이다.

### 단위 5 — cooked·멀티플레이·성능 Gate

- [ ] 전체 빌드·전체 SurfaceNavigation 자동화·Development BuildCookRun을 통과한다. 자동화가 재저장하는 테스트 에셋 세 개는 실행 전 백업으로 복구하고 해시를 확인한다.
- [ ] 패키지 1P와 `-game` 리슨 2P host/guest에서 같은 버전·Support/Nav snapshot·hit identity를 확인하고 probe·assert·ensure·crash·Layer jump·Unknown hit·게시 순서 위반을 검사한다.
- [ ] 긴 경사로·복층 추격의 확장 수·scheduler tick·완료 지연과 portal 수·베이크 시간·payload·decoded resident를 기록한다. 공동 m=1의 exact 오차는 기존 지각 합격 기준과 비교한다.
- [ ] 7b와 같은 trace 없는 Development package·`-nullrhi -corelimit=4`·투사체 500 조건에서 자연/합성 700마리 추격을 각각 두 번 측정한다. 프레임 P95 ≤16.67ms·경로 tick P95 ≤1.5ms를 유지하고 실패하면 7c를 완료하지 않는다.
- [ ] 입체/지하 시나리오의 cache 적중률·exact CPU·락 대기·최대 수용량을 재측정한다. 기존 Meadow와 콘텐츠가 다르면 결과를 구분하고 동일 시나리오 exact-only/cache-first를 비교한다.
- [ ] 완료 증거를 로그에 기록하고 Current·Roadmap·기준 설계의 예정 표기를 실제 구현 상태로 갱신한다.

## 4. 다음 단위의 착수 조건

단위 3은 2026-10-02에 전체 빌드·전체 자동화 84/84·DataVersion 6·schema 8 세 SurfaceData 재베이크를 통과했다. 단위 4는 사용자 진행 확인 뒤 착수한다. 입체·지하 공동 회귀 콘텐츠를 추가하며, 라이브 제작·관찰에 MCP가 효율적이면 즉시 연결을, 전체 빌드가 필요하면 즉시 에디터 저장·종료를 요청한다. 사용자 PIE 플레이로 추격·낙하·높은 공동의 비행 교전을 확인한다. 상세 증거는 `../history/Phase07c_Log.md`를 따른다.
