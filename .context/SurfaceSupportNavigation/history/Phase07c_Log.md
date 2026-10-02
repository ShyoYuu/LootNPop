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
