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

## 2026-10-18 — 단위 0-2 GASP Mover ABP 블록 목록

### 수행

1. **dirty 원인 확정.** 첫 조회 전 `is_dirty(SandboxCharacter_Mover_ABP)`는 이미 `true`였다. 에디터 로그를 보니 0-1과 같은 에디터 세션이었고, 02:21 자동 저장 때는 깨끗했다가 02:31 ABP 읽기 툴만 돈 직후 02:31:54 자동 저장에 ABP가 들어갔다. 깨끗한 `SandboxCharacter_CMC_ABP`로 툴을 하나씩 돌려 `is_dirty`를 재니 **EventGraph `read_graph_dsl` 직후에만 `true`**가 됐다(목록·노드 정보·프로퍼티·빈 AnimGraph DSL·Control Rig 읽기는 무관). 결과는 `design/GASPReference.md` §1.2.
2. AnimGraph·레이어 5개·블렌드 스택 그래프 2개·Ragdoll 상태 그래프는 `find_nodes`+`get_node_infos` 묶음 스크립트로, K2 함수 약 45개는 `read_graph_dsl`로, 핵심 노드 디테일은 `ObjectTools.get_properties`로 읽었다. `CR_Biped_FootPlacement`은 Up 축 의심 그래프(`Do Scene Query`·`UpdateRootDamper`)만 핀 연결까지 읽었다.
3. State Controller 전이 24개의 규칙 그래프 내용과 전이 노티파이 이름·크로스페이드·우선순위를 읽었다. 어느 상태에서 어느 상태로 가는지는 여전히 안 나온다.
4. Chooser 두 개와 PSD 71개의 의존성을 스크립트로 폴더별 개수만 요약해 소스 에셋 범위를 잡았다. `CHT_PoseSearchDatabases_Relaxed`는 컬럼 4개·8행의 조건 값까지 읽혔다.
5. 엔진 소스로 `AnimNode_Steering`(월드 목표 방향 vs 루트 트랜스폼), `AnimNode_OrientationWarping`(방향을 루트 로컬로 변환), `AnimNode_StrideWarping`(바닥 법선·중력 기본값 `WorldSpaceVector`)의 Up 축 가정을 확인했다.

### 결과

- 블록 13개(B0~B12) 표 초안을 `design/GASPReference.md` §2에 썼다. 채택 9개, 제외 3개(B9 AO·B11 래그돌·B12 상호작용 IK), B1′(SM 경로)는 결정 대기.
- Up 축 ⚠️ 3곳: `Speed2D`·Lean 계산의 월드 XY/Z, `StrideWarping` 기본값, CR `UpdateRootDamper`의 월드 Z 감쇠.
- 0-1의 `E_MovementMode` 매핑 추정이 틀렸다(9=Ragdoll, 10=Flying). §1.2에 정정했다.
- GASP 쪽 쓰기 툴은 쓰지 않았다. C++·LootNPop 에셋 변경 없음. 현재 dirty: Mover ABP·CMC ABP(저장 금지).

### 사용자 자료 요청 (일괄)

필수(MM 경로 기준):

1. `AdditiveLeans`·`Procedural_Feet` 레이어 그래프에서 Ctrl+A → Ctrl+C 텍스트를 `Saved/GASPRef/leans.txt`·`feet.txt`로 저장(이동 모드 Blend 핀↔enum 매핑, Bool 바인딩, Lean BS 입력 바인딩).
2. AnimGraph에서 `Blend Poses by int`·`Offset Root Bone` 두 노드 Ctrl+C → `Saved/GASPRef/animgraph.txt`(핀 바인딩).
3. `CHT_PoseSearchDatabases_Relaxed` Chooser 에디터 스크린샷 1장(8행의 결과 열).

SM 경로를 검토할 때만:

4. `State Controller` 상태 머신 그래프 스크린샷.
5. `CHT_MoverCharacterAnimations_PoseMatch` 스크린샷(중첩 테이블 펼침).
6. Blend Stack(State Machine Blend Stack) 노드 안 그래프 Ctrl+A → `Saved/GASPRef/sm_blendstack.txt`.

Ragdoll 상태 머신 전이는 B11이 범위 밖(AN-005)이라 요청하지 않았다.

### 시행착오·주의

- `read_graph_dsl`은 순환 보간 그래프에서 에러가 나 묶음 스크립트 전체가 죽는다. `find_nodes(title="Interp")`로 먼저 거른다.
- `ObjectTools.get_properties`에 노드 클래스에 없는 이름(예: 전이 노드 `bBidirectional`)을 넣으면 스크립트 전체가 실패한다. 항상 `list_properties`로 받은 키만 넣는다.
- Blend Poses by Enum의 enum 항목 매핑은 프로퍼티로도 안 읽힌다(핀 이름이 `BlendPose_N`).

### 사용자 자료 반영과 결정 (같은 날)

- 사용자가 경로를 이해한 뒤 **MM 노드 방식으로 결정**했다(AN-015). 사유: SM + Chooser + Blend Stack 방식은 아직 검증되지 않았고 직접 유지보수할 자신이 없다. 그래서 요청 4~6(SM 경로 자료)은 받지 않았다.
- 받은 자료: `Saved/GASPRef/leans.txt`·`feet.txt`(레이어 그래프 Ctrl+A), `animgraph.txt`(BlendListByInt·OffsetRootBone). 스크립트로 `PropertyBindings`의 `PathAsText`와 `VisibleEnumEntries`만 추려 해석했다. T3D에서 Blend Poses by Enum의 `BlendPose_0`은 기본값, `BlendPose_1..`은 `VisibleEnumEntries` 순서다. Blend Poses by Bool은 `BlendPose_0`이 참이다.
- 확인된 것: `ActiveChildIndex` ← `LocomotionSetup`, Offset Root 모드·반감기·반경 바인딩, `Procedural_Feet` 모드 매핑(Sliding·Traversing·InAir → LegIK, Ragdoll → IK 없음, 나머지 → CVar `a.AnimNode.FootPlacement.Enable`이면 CR), Lean 분기(Sliding 제외, Moving일 때만, `EnableAO`면 머리 기울기 끔, 걷기↔뛰기 α = Speed2D 200~320).
- Chooser 스크린샷(요청 3)은 오지 않았다. 행 조건은 MCP로 읽혔고 결과 묶음은 PSD 이름으로 짐작되므로 2-2에서 Chooser를 만들 때 확인하기로 했다(`design/GASPReference.md` §2.6).
- `design/GASPReference.md` §2 완성, `Decisions.md` AN-015 추가, `Roadmap.md` Phase 2를 6단위(2-1~2-6)로 확정, Phase 0 체크리스트 0-2 체크.
- 검증: 문서 작업만 했다. GASP 에디터 쓰기 툴 미사용, C++·LootNPop 에셋 변경 없음.

## 2026-10-18 — 단위 0-3 스켈레톤과 소스 에셋 규칙

### 수행

1. **Lyra 실사용 시퀀스 집계.** git 추적 `.uasset` 255개(Maps·외부 액터 제외)를 루트로 LootNPop MCP `get_dependencies`를 따라갔다. 라이선스 폴더(`Lyra_Mannequin`·`GASP_Mannequin`·`MoverSample_Mannequin`·`Weapons`, 추적 예외 폴더는 빼고) 안에서만 더 따라가고, 추적 에셋에 닿으면 그 에셋의 몫으로 넘겼다. 묶음 스크립트 1회로 580번 조회했다. 결과: `Lyra_Mannequin/Animations` 701개 중 121개 사용. `PSD_*_Lyra` 경로 30개, 서브 ABP 경로 37개, 몽타주 경로(`CHT_Montage`·`GA_Stagger_LongSword`·`GA_ParrySuccess_LongSword`) 55개, `ABP_Lyra` 직접 참조 1개(`MM_Unarmed_Jump_Fall_Loop`). 로코모션 PSD를 뺀 91개는 MM 59·AM 21·A 9·AO 2(접두어 기준). 원본 결과는 로컬 `Saved/AnimOverhaul03/reach.txt`(git 제외).
2. **메시 구성.** `BP_LNPPlayer`·`BP_LNPEnemy` CDO 컴포넌트를 `ObjectTools.get_properties`로 읽었다. 두 BP 모두 `AnimSourceMesh` = `SKM_Manny` + `ABP_Lyra`(숨김)다. `VisualMesh`는 `ABP_Retarget`이 구동하며, 플레이어는 `SKM_Manny_Simple`, 적은 `SKM_UEFN_Mannequin`이다. `ABP_Retarget`은 `RTG_GASP_to_MS`·`RTG_Lyra_to_GASP`·`RTG_Lyra_to_MS`를 참조한다.
3. **본 대조.** `SkeletalMeshTools.get_bone_names`로 Lyra 164·UEFN 88·MoverSample 161본을 비교했다. CR·IK·워핑이 쓰는 본은 셋 다 있다. 차이는 `design/GASPReference.md` §2.7.
4. **GASP 5.8 외부 의존.** GASP MCP로 `CHT_PoseSearchDatabases_Mover`·`_Relaxed`에서 `UEFN_Mannequin` 안을 따라가며 경로 밖 의존을 셌다(읽기 툴만 사용). 시퀀스는 커브 압축 설정·Foley 노티파이 10종·EarlyTransition·`AM_BakePhaseCurveFromFootstepNotifies`에 의존한다. 각 의존 체인을 `get_dependencies`로 확인했다(Foley → `/Game/Audio/Foley/*`, EarlyTransition → `BPI_SandboxCharacter_ABP`·CMC ABP, AM·커브 압축 설정 → 엔진만). LootNPop 쪽 옛 GASP 시퀀스 414개와 Lyra 701개에는 폴더 밖 의존이 없다.
5. 디스크: GASP 5.8에는 `RTG_UEFN_to_UE5_Mannequin`·`RTG_UEFN_to_Metahuman_*` 등 UEFN 원본 → 표시 메시 리타기터가 있다.

### 사용자 결정

- **AN-011 UEFN 전환.** 사용자 요지: UEFN 마네킹·UEFN 애니가 모든 로코모션의 기본이다. 공격·리로드·무기별 애님 레이어에는 Lyra 애니를 계속 쓸 수 있어야 한다. Lyra 시퀀스를 UEFN으로 리타깃해 쓰고, `AnimSourceMesh`는 UEFN, `VisualMesh`는 캐릭터별 메시 + 리타기터로 한다. "전부"는 실사용 91개와 이후 필요분으로 기록했다(AN-001 필요할 때 반입). 실행 단위 2-0a·2-0b를 `Roadmap.md` Phase 2에 추가했다.
- **AN-016 폴더·git:** 원본 경로 유지안. 이 선택으로 AN-013(경로 정렬)을 확정, AN-014(리타깃 산출물은 이동하되 git 제외)를 해소로 표기했다.
- **AN-017 노티파이:** 독립 반입 + 리다이렉트안. BP 생성 클래스 → 네이티브 클래스 `CoreRedirects`가 노티파이 인스턴스에서 경고 없이 풀리는지는 검증하지 않았다 → 1C-1 실험 항목.
- **AN-010:** 제안대로 확정.

### 결과

- `Decisions.md` AN-010·011·013·014 갱신, AN-016·017 추가. `README.md` §4 소스 에셋 확보 절차, §5 메시 구성 갱신. `design/GASPReference.md` §2.7을 결정 근거로 교체. `phases/Phase01C_GASPAssetRefresh.md`에 "반입 목록과 경로 규칙" 절 추가, 1C-1~1C-3 단계 갱신. `Roadmap.md` Phase 0 완료, Phase 2에 2-0a·2-0b 추가. Phase 0 체크리스트 완료.
- 검증: 문서 작업만 했다. GASP 쪽 쓰기 툴과 `read_graph_dsl`은 쓰지 않았다. LootNPop 쪽 쓰기는 `Saved/AnimOverhaul03/`에 조사 결과 텍스트를 쓴 것뿐이다(`AssetTools.write_file`). C++·에셋 변경은 없다.

### 시행착오·주의

- `PA_UEFN_Mannequin`·`IK_UEFN_Mannequin`이 존재하지 않는 경로 `/Game/Characters/GASP_Mannequin/GASP_Mannequin/Meshes/SKM_UEFN_Mannequin`(프리뷰 메시로 추정)을 참조한다. `get_dependencies`를 이 경로에 걸면 스크립트 전체가 죽는다 → 의존성 순회 전에 `AssetTools.exists`로 거른다. 1C 덮어쓰기 때 사라질 것으로 본다.
- 0-1 기록과 같이, `get_dependencies`의 "Asset does not exist" 에러는 `try/except`로 감싸도 스크립트 전체를 끝냈다. 실패할 수 있는 호출은 사전 조건(`exists`)을 먼저 확인한다.
- `ObjectTools.get_properties`의 인자 이름은 `instance`·`properties`다. BP 컴포넌트 기본값은 `<BP>.Default__<BP>_C:<컴포넌트>`로 읽힌다.
- MM 경로 Chooser에서 따라간 GASP 애니는 759개로, 0-2의 461개보다 많다. 시퀀스가 다른 시퀀스·BS를 참조하는 것까지 셌기 때문으로 보인다. 리타깃량 비교에는 0-2 수치를 썼다.
