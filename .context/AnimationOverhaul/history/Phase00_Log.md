# Phase 0 작업 로그

> 소유 정보: Phase 0(참조 환경과 기준 결정)의 날짜별 수행 내용·검증 결과·시행착오. 현재 유효한 결론은 `design/GASPReference.md`가 소유한다.

## 2026-10-18 — 단위 0-1 MCP 읽기 범위 확인

### 수행

1. 세션에서 `unreal-mcp`(8000)와 `unreal-mcp-gasp`(8001)가 둘 다 보이는지 `list_toolsets`로 확인했다. 두 서버 모두 툴셋 목록을 돌려줬고, 8001 쪽에서 `/Game/Blueprints/SandboxCharacter_Mover_ABP` 등 GASP 에셋이 조회돼 GASP 에디터임을 확인했다.
2. GASP 쪽에서 5개 대상을 읽기 툴로만 조회했다. 결과 표는 `design/GASPReference.md` §1.1.
   - ABP: `list_graphs` → AnimGraph `find_nodes`(18노드) → `get_node_infos`. 묶음 스크립트로 최상위 포즈 체인과 `Procedural_Feet` 레이어(포즈+데이터 핀)를 요약했다. 노드 디테일은 `ObjectTools.get_properties`. `read_graph_dsl`은 EventGraph·함수에서만 내용이 나왔다.
   - 상태 머신: 상태·전이 노드 `get_node_infos`/`get_connected_subgraph`가 에러, 전이 규칙 그래프 내부는 `find_nodes`+`get_node_infos`로 읽혔다.
   - Control Rig: 그래프 42개·변수 48개·그래프별 노드 수를 얻고, `Calculate Limb Length`(7노드)로 핀 연결·기본값이 읽히는지 확인했다.
   - Chooser: 컬럼·컨텍스트·행 조건 값은 읽혔고 행 결과는 실패. `get_dependencies`로 참조 에셋 전체를 얻었다.
   - PSS: `PSS_SM_Mover_Loops`·`PSS_SM_Mover_Transitions`의 채널 트리를 재귀로 읽었다. PSD는 `PSD_SM_Mover_*`가 없어 `PSD_SM_CMC_Loops`로 확인했다.
   - PhysicsControl: `PCA_SandboxCharacter` 프로퍼티 전체(약 12.7KB)를 읽었다.
3. 빠지는 항목마다 대안을 정했다(§1.1 마지막 열). 에디터 Python 덤프는 MCP에 `unreal` 모듈이 없어 불가로 판정했다.

### 결과

- **AnimGraph 포즈 연결은 읽힌다.** 빠지는 것은 핀 프로퍼티 바인딩, 상태 머신 전이 연결, Chooser 행 결과, PSD 애니 목록 네 가지다. 앞의 세 가지는 0-2에서 사용자 스크린샷이나 Ctrl+C 텍스트로 받는다. PSD 애니 목록은 의존성 조회로 대체한다.
- GASP 쪽 쓰기 툴은 쓰지 않았다. C++·LootNPop 에셋 변경은 없다.

### 시행착오·주의

- 조회를 마친 뒤 `is_dirty`가 `SandboxCharacter_Mover_ABP`만 `true`를 돌려줬다. 조회 전 값을 재지 않아 원인을 모른다. 저장하지 않았고, GASP 에디터를 닫을 때도 저장하지 않아야 한다. 0-2 첫 조회 전에 `is_dirty`를 먼저 잰다.
- `get_dependencies`를 직접 호출하면 Chooser 하나에 시퀀스 경로 약 400줄이 그대로 컨텍스트에 들어온다. 다음부터는 스크립트에서 요약해서 받는다.
- 묶음 스크립트에서 실패하는 `get_properties`(없는 프로퍼티 이름)가 하나라도 있으면 `try/except`로 감싸도 스크립트 전체가 에러로 끝났다.
- SlateInspector `Screenshot(ref="")`는 빈 이미지를 돌려줬다. 대안으로 채택하지 않았다.
- E_MovementMode 표시 이름은 GASP `.uasset` 문자열에서 OnGround·InAir·Sliding·Traversing·Flying·Ragdoll 순으로 나왔다. `NewEnumerator4/5/6/7/9/10`과 이 순서로 대응한다고 추정하지만 검증하지 않았다.
