# GASP 참조 조사

> 상태: 초안(§1 단위 0-1, §2 단위 0-2, §2.7 단위 0-3, §2.8 단위 2-1, §2.9 단위 2-2, §2.10 단위 2-3 완료 — 블록별 수치는 해당 단위 착수 시 재확인)
> 마지막 갱신: 2026-10-20
> 소유 정보: GASP 5.8 에디터(`unreal-mcp-gasp`, 포트 8001)에서 무엇을 어떻게 읽는지, 그리고 블록별로 LootNPop에서 무엇을 만들지. 원본 덤프가 아니라 요약만 둔다(README §11).

## 1. 읽기 방법

2026-10-18, GASP 에디터(포트 8001)에 대해 읽기 툴만 써서 확인했다. 저장·수정 툴은 쓰지 않았다(AN-009). 수행 기록은 `history/Phase00_Log.md`.

### 1.1 대상별 결과

| 대상 | 툴 | 읽히는 것 | 빠지는 것 | 빠진 것의 대안 |
|:---|:---|:---|:---|:---|
| `SandboxCharacter_Mover_ABP` 그래프 목록 | `BlueprintTools.list_graphs` | AnimGraph, 상태 머신·상태·전이 규칙 그래프, 레이어 함수(`Procedural_*`·`AdditiveLeans`·`AimOffset`), 함수 그래프 전부의 경로와 이름(상태·전이 이름이 경로에 들어 있다) | — | — |
| ABP **AnimGraph 포즈 연결** | `find_nodes`(title `""`) → `get_node_infos` | **읽힌다.** 노드 타입(`type_id`), Pose Link 핀의 입력←출력 연결, 데이터 핀 연결, 핀 기본값, 캐시 포즈 이름(`type_id`에 포함). 레이어 함수 그래프(예: `Procedural_Feet`)도 같은 방식으로 포즈+데이터 핀이 모두 읽힌다 | ① **노드 프로퍼티 바인딩**(핀에 함수·변수를 바인딩한 것). 바인딩된 핀은 "연결 없음 + 기본값"으로 보인다(예: `BlendListByInt.ActiveChildIndex=0`, `BlendStack.AnimationAsset=None`). `AnimGraphNodeBinding_Base.PropertyBindings`는 ObjectTools로도 못 읽는다 ② 연결 정보는 상대 핀의 **인덱스**만 주므로 출력 핀 이름은 상대 노드를 다시 조회해야 안다 | ① 사용자가 노드 선택 → Ctrl+C → 텍스트로 붙여 준다(T3D에 바인딩·링크가 모두 들어 있다, 에셋 변경 없음). 소수 노드면 디테일 패널 스크린샷. 바인딩 함수 이름은 `list_graphs`의 `Get_*` 목록으로 짐작할 수 있다 ② 필요할 때만 상대 노드 `get_node_infos` |
| ABP 노드 디테일(비핀 프로퍼티) | `ObjectTools.list_properties` → `get_properties`(노드 오브젝트, `node` 등) | 노드 구조체 전체 값(예: OffsetRootBone 모드·반감기, MotionMatching 블렌드 프로파일·`database`, BlendStack 블렌드 설정), 노드 함수 바인딩(`initialUpdateFunction`·`becomeRelevantFunction`·`updateFunction`), 노드 `tag`. 상태 결과 노드의 `stateEntryFunction` 등 | 위 ①과 같음 | 위와 같음 |
| ABP **상태 머신 토폴로지** | `find_nodes`·`get_node_infos`·`get_connected_subgraph` | 상태·전이 이름(`list_graphs` 경로에 들어 있다). 상태 노드·전이 노드의 프로퍼티(`ObjectTools`: 크로스페이드 시간·블렌드 모드·우선순위·전이 노티파이 이름 등). 전이 **규칙 그래프 내부**는 `find_nodes`+`get_node_infos`로 읽힌다 | 상태 머신 그래프에서 `find_nodes`가 **빈 목록**, 상태·전이 노드에 `get_node_infos`·`get_connected_subgraph`가 **에러**(`'AnimStateNode' object has no attribute 'get_node_title'`). 즉 **어느 상태에서 어느 상태로 가는 전이인지**가 안 나온다 | 전이 노티파이 이름("Transition: Idle to Locomotion")에서 일부 유추. 정확한 연결은 사용자가 상태 머신 그래프 스크린샷 1장 또는 Ctrl+C 텍스트 |
| ABP 이벤트·함수 그래프 | `read_graph_dsl` | **K2 그래프(EventGraph·함수)는 DSL로 잘 읽힌다**(EventGraph 전체 671자). 흐름 제어·함수 호출 구조 | ① **AnimGraph·레이어·상태·전이 규칙 그래프는 빈 문자열** ② 사용자 정의 enum은 내부 이름(`NewEnumerator4`)으로 나온다 ③ Break 구조체의 어느 출력 핀을 썼는지 DSL에 안 나온다 ④ 접힌 그래프(Composite)는 DSL에서 생략될 수 있다 | ① 포즈 그래프는 `get_node_infos` 경로 ② enum 표시 이름은 GASP 디스크의 `.uasset` 문자열로 목록만 얻는다(매핑 순서는 추정), 확실히 하려면 사용자 확인 ③④ 해당 노드 `get_node_infos` |
| `CR_Biped_FootPlacement` | `ControlRigTools.list_graphs`·`list_variables`·`list_nodes`·`list_pins`·`get_connected_pins`·`get_pin_value`, 노드 클래스는 `ObjectTools.get_class` | **전부 읽힌다.** 그래프 42개(포워드 솔브 + 함수 라이브러리 약 25개), 변수 48개(타입·기본값), 노드·핀 이름, 서브 핀 단위 연결(`RelativeTransform.Translation`), 미연결 핀 기본값. 계층(`get_all_bones` 등)도 툴이 있다 | 노드 클래스가 일반형 `RigVMUnitNode`로만 나온다(RigUnit 구조체 이름은 노드 이름으로 유추) | 노드 이름이 대부분 구조체 이름을 따른다. 애매하면 해당 노드 핀 구성으로 판별 |
| Chooser `CHT_MoverCharacterAnimations_PoseMatch` | `ObjectTools.get_properties`(`columnsStructs`·`contextData`·`resultType`·`outputObjectType`), `AssetTools.get_dependencies` | 컨텍스트(ABP 클래스 ReadWrite, `S_CHT_MoverCharacterAnimations_OUT` Write, `PoseHistoryReference` Read), 컬럼 정의와 **행 조건 값**(1열 `StateName` 이름 컬럼, 10행 = 상태 이름 10개), 결과 타입(`AnimationAsset` 오브젝트). 의존성으로 참조 애니·PSS·enum 전체 목록(약 400개) | **행별 결과**(`ResultsStructs`)와 중첩 Chooser 내용을 못 읽는다. 하위 오브젝트 경로 추측도 실패. 즉 "어느 행이 어떤 애니·하위 테이블·PSS로 가는지"가 빠진다 | **2-2부터 LootNPop 에디터 명령 `LNP.Editor.GASPRef`(아래 §1.4)로 덤프한다.** 중첩 테이블은 `<Chooser>.<표시 이름>` 경로(예: `CHT_PoseSearchDatabases_Relaxed.Stand Walks`)로 MCP에서도 컬럼까지 읽힌다. 컬럼 행 수는 결과 행 수와 같다(비활성 행은 결과가 `Asset=None`) |
| PSS `PSS_SM_Mover_*` | `ObjectTools.list_properties`·`get_properties`, 채널 하위 오브젝트 재귀 | **전부 읽힌다.** 샘플레이트, 스켈레톤(`SK_UEFN_Mannequin`), 전처리, 채널 트리(`Trajectory` 샘플 오프셋·플래그·가중치, `Group` 아래 `Position`·`Velocity`의 본·기준 본·가중치, `Curve`) | 트래젝토리 샘플 `flags`가 비트마스크 정수(예: 48, 176) | 엔진 헤더 `PoseSearchFeatureChannel_Trajectory.h`의 `EPoseSearchTrajectoryFlags`로 해석 |
| PSD (`PSD_SM_CMC_*`로 확인 — `PSD_SM_Mover_*`는 이 폴더에 없음) | `ObjectTools.get_properties`, `AssetTools.get_dependencies` | 스키마 참조, 비용 바이어스, 검색 모드, KDTree·PCA 설정 | **데이터베이스 애니 목록**(`DatabaseAnimationAssets`)과 항목별 설정(미러·샘플 범위) | 의존성 목록으로 들어 있는 애니 집합만 얻는다. 항목별 설정은 `LNP.Editor.GASPRef` 덤프(§1.4) |
| PhysicsControl `PCA_SandboxCharacter` | `ObjectTools.list_properties`·`get_properties` | **전부 읽힌다**(약 12.7KB). 물리 에셋(`PA_UEFN_Mannequin`), 리밋 6개(시작 본·컨트롤 생성 옵션), 기본 월드/부모 공간 컨트롤 값, 추가 컨트롤·모디파이어·세트, 프로필 3개(`kinematic`·`physicalAnimation`·`ragdoll`)의 컨트롤 갱신 값 | — | — |

**결론: AnimGraph 포즈 연결은 MCP로 읽힌다.** 0-2 블록 분해를 MCP만으로 시작할 수 있다. 사용자에게 받아야 하는 것은 세 가지로 좁혀진다.

1. 핀 프로퍼티 바인딩 — 블록마다 핵심 노드 몇 개의 Ctrl+C 텍스트 또는 디테일 패널 스크린샷
2. 상태 머신(`State Controller`·`Ragdoll`) 전이 연결 — 그래프 스크린샷 1~2장
3. Chooser 행별 결과 — `CHT_MoverCharacterAnimations_PoseMatch`와 중첩 테이블 스크린샷

### 1.2 툴 사용 규칙과 함정

- **목록 → 블록 단위 조회 순서를 지킨다.** `list_graphs`로 그래프 경로를 얻고, 블록에 해당하는 그래프만 `find_nodes`/`get_node_infos`로 읽는다. 큰 그래프는 아래 묶음 스크립트로 요약해서 받는다.
- **`ProgrammaticToolset.execute_tool_script`로 묶는다.** 툴 호출 여러 번을 한 번에 돌리고 결과를 "노드·타입·입력←출력" 정도로 줄여 받는다. 쓸 수 있는 모듈은 `json`·`re`·`math`·`copy`·`time`·`datetime`뿐이고 `unreal` 모듈은 없다(**에디터 Python 덤프는 MCP로 불가**). 스크립트 안의 툴 호출이 하나라도 실패하면 `try/except`와 상관없이 스크립트 전체가 에러로 끝난다 — 실패할 수 있는 프로퍼티 이름은 따로 시험한다.
- **덤프 크기 주의.** `describe_toolset`의 `BlueprintTools`·`ControlRigTools`는 7만 자를 넘어 파일로 떨어진다(Python으로 이름·스키마만 뽑아 읽는다). `get_dependencies`는 Chooser 하나에 수백 줄이 나오므로 스크립트에서 개수·폴더만 요약한다.
- **`ObjectTools.list_properties`가 보여 주는 것만 `get_properties`로 읽힌다.** 목록에 없는 내부 UPROPERTY(`PropertyBindings`·`ResultsStructs`·`DatabaseAnimationAssets`)는 이름을 넣어도 실패한다.
- **쓰기 툴 금지 목록**(같은 툴셋 안에 섞여 있다): `write_graph_dsl`, `compile_blueprint`, `create*`/`add_*`/`remove_*`/`delete*`/`connect_pins`/`break_pins`/`set_*`(BlueprintTools·ControlRigTools·ObjectTools), `AssetTools.save_assets`/`delete`/`move`/`duplicate`/`write_file`/`update_metadata_tags`, SlateInspector의 `Click`/`Type`/`PressKey`/`Drag`/`FillForm`/`SelectOption`.
- **`read_graph_dsl`은 BP를 dirty로 만든다(0-2에서 확정).** 깨끗한 `SandboxCharacter_CMC_ABP`로 툴을 하나씩 돌리며 `is_dirty`를 쟀다. `list_graphs`·`find_nodes`·`get_node_infos`·`ObjectTools.list_properties`/`get_properties`, 내용이 빈 AnimGraph에 대한 `read_graph_dsl`은 `false`를 유지했고, **EventGraph(K2)에 `read_graph_dsl`을 건 직후 `true`**가 됐다. 0-1의 Mover ABP dirty도 같은 원인이다(같은 에디터 세션 로그에서 02:21 자동 저장 때는 깨끗했고, 02:31에 ABP 읽기 툴만 돈 뒤 02:31:54 자동 저장에 ABP가 들어갔다). Control Rig 읽기 툴은 dirty를 만들지 않았다. 규칙: K2 그래프는 DSL이 가장 압축적이라 계속 쓰되, **DSL을 쓴 BP는 dirty가 된다고 전제하고 GASP 에디터를 닫을 때 저장하지 않는다.** 자동 저장은 `Saved/Autosaves`에만 쓰므로 원본 `.uasset`은 바뀌지 않는다. 2026-10-18 현재 dirty: `SandboxCharacter_Mover_ABP`, `SandboxCharacter_CMC_ABP`(원인 확인용).
- **`read_graph_dsl`은 데이터 흐름 순환이 있는 그래프에서 실패한다.** 변수를 `VInterpTo`/`RInterpTo`로 자기 갱신하는 그래프는 "Cycle detected in data-flow graph" 에러가 나고, 묶음 스크립트 전체가 죽는다. 먼저 `find_nodes(title="Interp")`로 걸러 그 그래프만 `get_node_infos`로 읽는다(Mover ABP: `Update_EssentialValues`, `OnUpdate_LocomotionTransition`).
- **Blend Poses by Enum의 핀↔enum 항목 매핑은 안 읽힌다.** 핀 이름이 `BlendPose_0..N`으로만 나오고 `VisibleEnumEntries`는 `list_properties`에 없다 → 사용자 Ctrl+C.
- **enum 내부 이름 매핑(0-1 추정 정정).** `E_MovementMode`: `NewEnumerator4`=OnGround, `5`=InAir, `6`=Sliding, `7`=Traversing, `9`=Ragdoll, `10`=Flying — Ragdoll 상태 머신 전이(`==9`), State Controller 전이(`==6` → "to Slide", `==10` → Debug Flying)로 확인했다. `E_MovementState`: `0`=Moving, `4`=Idle(전이 노티파이 이름으로 확인). `E_Stance` `0`=Stand·`1`=Crouch, `E_Gait` `0`=Walk·`1`=Run·`2`=Sprint는 Chooser 행 구성으로 추정.
- **GASP 기본 CVar**(`Config/DefaultEngine.ini`): `DDCvar.LocomotionSetupMover`=1(상태 머신 경로, CMC는 0=모션매칭), `DDCvar.FootPlacementMode`=1("0 off, 1 foot placement node, 2 biped control rig").
- **SlateInspector 스크린샷은 대안으로 쓰기 어렵다.** `Screenshot(ref="")`가 빈 이미지를 돌려줬다(GASP 창이 활성 창이 아닐 때로 추정). 에디터 UI 조작이 필요해 읽기 전용 원칙과도 부딪히므로 사용자 스크린샷을 우선한다.

### 1.3 검증 샘플 — AnimGraph 최상위 포즈 체인

포즈 연결이 읽힌다는 근거로 남긴다. 블록 해석은 0-2가 한다.

```
Output Pose ← PoseHistoryCollector ← [Layer] Procedural_FullBody ← [Layer] Procedural_Feet
  ← OffsetRootBone (tag OffsetRoot, updateFunction OnUpdate_OffsetRootBone)
  ← DeadBlending ← Slot 'DefaultSlot' ← StateMachine 'Ragdoll'

Save Cached 'PreRagdoll' ← [Layer] AimOffset ← [Layer] AdditiveLeans ← [Layer] Procedural_PreLayering
  ← BlendPosesByInt ─ 0: MotionMatching (updateFunction Update_MotionMatching)
                    └ 1: TwoWayBlend (Alpha=1) ─ A: Inertialization ← StateMachine 'State Controller'
                                                └ B: BlendStack (tag "State Machine Blend Stack")
```

- `State Controller`의 상태 그래프(예: `Idle Loop`)는 `StateResult` 하나뿐이고 포즈 입력이 없다. 상태는 `stateEntryFunction`(`OnStateEntry_IdleLoop` 등)으로 블렌드 스택을 구동하는 컨트롤러 역할로 보인다(0-2에서 확인).
- `PSS_SM_Mover_*`를 참조하는 에셋은 Chooser `CHT_MoverCharacterAnimations_PoseMatch`뿐이다(`get_referencers`). Mover 상태 머신 경로는 PSD 없이 Chooser가 PSS로 포즈 매칭을 하는 구성으로 보인다(0-2에서 확인).

### 1.4 C++로 GASP 구성 에셋 읽기 — `LNP.Editor.GASPRef` (2-2)

`ResultsStructs`·`DatabaseAnimationAssets`는 편집 플래그 없는 `UPROPERTY()`라 MCP·Python으로 못 읽지만 C++에서는 접근할 수 있다. LootNPop 에디터 명령 `LNP.Editor.GASPRef <spec.json>`(`Source/LootNPopEditor/Tools/LNPGASPReferenceCommand.cpp`)이 GASP `Content/`를 마운트 포인트 `/GASPRef/`로 올려 GASP 패키지를 불러온다.

- 불러오는 동안만 `redirectPrefixes`(예: `/Game/Characters/UEFN_Mannequin/Animations/MotionMatchingData/`, `/Game/Blueprints/Data/`)를 `/GASPRef/`로 패키지 리다이렉트해, LootNPop에 없는 GASP 구성 에셋 참조가 GASP 쪽으로 풀리게 한다. 시퀀스·스켈레톤은 LootNPop에 같은 경로로 반입돼 있어 리다이렉트하지 않는다(반입 안 된 시퀀스는 `None`으로 나오고 PoseSearch 경고가 난다 — 무해).
- `dump`: 패키지 안 모든 오브젝트의 프로퍼티를 텍스트로 JSON에 쓴다. `clone`: GASP 에셋을 LootNPop 경로에 복제하고 복제본끼리 참조를 잇는다(정규화 세트는 `/Game` 밖 DB를 뺀다). `/GASPRef/` 참조가 남으면 경고한다. 저장하지 않는다.
- 끝나면 리다이렉트·마운트를 내린다. 불러온 GASP 패키지는 에디터를 다시 켤 때까지 메모리에 남는다(dirty가 아니라 저장되지 않는다). GASP 에디터(8001)는 필요 없다 — 디스크의 GASP 파일만 읽는다.
- 같은 파일의 `LNP.Editor.SetChooserResults <Chooser> [Asset ...]`이 Chooser 결과 행을 에셋 결과로 바꾸고 컴파일한다(컬럼은 MCP `set_properties`). MCP로 `columnsStructs`를 쓸 때 기존 원소의 구조체 타입은 바뀌지 않는다 → 빈 배열로 한 번 쓴 뒤 새 컬럼을 넣는다. 배열 크기와 기존 원소를 한 호출에서 같이 바꾸면 거부된다.
- 사용 예: `Saved/AnimOverhaul/GASPRef_2-2_spec.json`(덤프 결과 `GASPRef_2-2.json`, git 제외).

## 2. 블록 표

2026-10-18 단위 0-2. `SandboxCharacter_Mover_ABP`를 블록으로 나누고 "LootNPop에서 무엇을 어떻게 만들지"로 요약했다. 핀 바인딩·enum 매핑은 사용자가 Ctrl+C로 준 `AdditiveLeans`·`Procedural_Feet` 레이어와 AnimGraph 노드 2개의 텍스트로 채웠다.

### 2.1 전체 구조

```
EventGraph (게임 스레드)   Update_CVarDrivenVariables → Update_PropertiesFromCharacter(BPI로 Pawn 구조체 1개 받음)
ThreadSafeUpdate (워커)    Update_Logic = Trajectory → EssentialValues → States → AimOffset → AdditiveLean

AnimGraph
  [선택 코어] BlendPosesByInt(ActiveChildIndex ← 변수 LocomotionSetup) ─ 0: MotionMatching ⟨Chooser CHT_PoseSearchDatabases_Relaxed → PSD 71개⟩
                                              └ 1: TwoWayBlend(α=1) ─ A: Inertialization ← StateMachine 'State Controller'(포즈 기여 없음, 상태 진입 함수만)
                                                                    └ B: BlendStack ⟨상태 진입 시 Chooser CHT_MoverCharacterAnimations_PoseMatch⟩
     (두 경로 모두 블렌드 스택 입력 그래프 안에서 Steering·Orientation/Stride Warping·ResetRoot 적용)
  → [Layer] Procedural_PreLayering(슬라이드 경사 root 보정) → [Layer] AdditiveLeans → [Layer] AimOffset
  → Save Cached 'PreRagdoll' → StateMachine 'Ragdoll' → Slot 'DefaultSlot' → DeadBlending
  → OffsetRootBone → [Layer] Procedural_Feet(CR_Biped_FootPlacement | LegIK) → [Layer] Procedural_FullBody(상호작용 손 IK)
  → PoseHistoryCollector → Output
```

### 2.2 블록 표

| 블록 | GASP 구성(요약) | LootNPop에서 만들 것 | Up 축 | 소스 에셋 | `ABP_Lyra` 대응 |
|:---|:---|:---|:---|:---|:---|
| **B0 애니 변수 공급층** | `Update_*` BP 함수들. 입력은 BPI `Get_PropertiesForAnimation`이 주는 `S_CharacterPropertiesForAnimation`(24필드)과 `UMoverTrajectoryPredictor` 궤적 | `ULNPAnimInstance`(C++)에 §2.3 값을 추가. 상태 술어(`IsPivoting`·`ShouldTurnInPlace` 등)도 C++ bool로(AN-003) | ⚠️ `Speed2D`·`IsStarting`이 `VectorLengthXY`(월드 XY), Lean 축이 `RotationFromXVector(Velocity)`(월드 Z 기준) → 접평면 투영·캐릭터 로컬로 계산 | — | 기존 공급값(속도·`GroundSpeed`·`bIsOnGround`·AO·IK)을 확장 |
| **B1 선택 코어(MM 경로)** | `MotionMatching`(블렌드 0.5s HermiteCubic, 블렌드 프로파일 `FastFeet_InstantRoot`, `poseReselectHistory` 0.3, 최대 블렌드 4) + `Update_MotionMatching`이 Chooser로 DB 목록을 골라 `SetDatabasesToSearch`(이동 모드·방향이 바뀌면 `InterruptOnDatabaseChange`). Chooser 8행 = 이동모드×자세×이동상태×보행 | `CHT_PSD_Lyra`(PSD 4개)를 상태별 DB 묶음 Chooser로 교체. 행 입력은 B0의 enum 값. DB 태그(`TurnInPlace` 등)를 PostSelection에서 받아 B2가 쓴다 | 트래젝토리 채널은 루트 기준 XY 플래그 — 루트가 중력 Up에 정렬돼 있으면 안전 | PSD 71개 → 시퀀스 461개(Walk 114·Run 114·Crouch 108·Jump 59·Sprint 21·Slide 20·Traversal 14·Idle 10), PSS 12개, `SK_UEFN_Mannequin` | 현 모션매칭 노드 + `CHT_PSD_Lyra` **대체** |
| ~~B1′ 선택 코어(SM 경로)~~ **미채택(AN-015)** | GASP Mover **기본값**. `State Controller` 상태 10개(Idle Loop·Idle Transition·Idle Break·Locomotion Transition·Locomotion Loop·In Air Transition·In Air Loop·Slide Transition·Slide Loop·Debug Flying Loop)+Conduit, 전이 24개. 상태 진입 함수가 `SetBlendStackAnimFromChooser`로 Chooser(상태 이름 + 포즈 히스토리)를 평가해 `BlendStack`(0.2s QuadraticInOut, 미러 `MDT_UEFN_Mannequin`)에 넣는다. 재생 속도는 `Get_DynamicPlayRate`(곡선 `MoveData_Speed` 대비 `Speed2D`) | 만들지 않는다(§2.5) | — | 시퀀스 약 387개(Crouch 96·Walk 88·Run 86·Jump 54·Sprint 20·Idle 19·Traversal 12·Slide 9), PSS 6개(`PSS_SM_Mover_*` 5 + `PSS_SM_CMC_Idles`), 경사 BS 2개 | 대체(신규 상태 머신) |
| **B2 블렌드 스택 절차 보정** | 블렌드 스택 입력 그래프마다: `Steering`×2(목표 방향 = 궤적 샘플, 시간은 곡선 `SteeringTargetTime`; 두 번째는 제자리 회전 — MM은 DB 태그 `TurnInPlace`, SM은 곡선 `Enable_TurnInPlaceSteering`) → `OrientationWarping`(공간 `RootBoneTransform`, 축 Z, α=곡선 `Enable_Warping`, 방향 = MM `LastNonZeroVelocity`) → (SM 경로에만) `StrideWarping`(α=`Enable_Warping`). MM 경로는 Stride Warping 없이 노드 재생 속도 범위(0.85~1.15)로 보폭 차를 흡수한다 → `ResetRoot` → CopyBone foot→ik_foot | 같은 체인을 MM 노드 블렌드 스택 그래프에 만든다. 입력은 B0(궤적·속도). Stride Warping은 미끄러짐이 보일 때만 추가 | `Steering`은 월드 목표 쿼터니언과 루트 트랜스폼을 비교한다 — 궤적 facing이 중력 Up 기준이어야 한다(2-2에서 적도 확인). `OrientationWarping` 안전(방향을 루트 로컬로 변환). ⚠️ **`StrideWarping`을 넣으면** 기본 `FloorNormalDirection`·`GravityDirection`이 `WorldSpaceVector` ±Z → `ComponentSpaceVector`로 바꾼다 | 시퀀스 곡선 6종: `Enable_Warping`·`SteeringTargetTime`·`Enable_TurnInPlaceSteering`·`MoveData_Speed`·`Max/MinDynamicPlayRate` | 없음(신규) |
| **B3 Offset Root Bone** | 슬롯·DeadBlending 뒤. Graph 모드, 이동 Interpolate·회전 Accumulate, 반감기 0.2/0.1, `MaxTranslationError` 30. 모드 함수: `DefaultSlot` 몽타주 중 Release, OnGround 이동 중 Interpolate·정지 Release, InAir Release. 반감기는 Idle 0.1·Moving 0.3. 핀 바인딩: 모드·반감기 ← `Get_OffsetRoot*` 함수, `bClampToTranslationVelocity` ← `IsMoving`, `MaxTranslationError` ← CVar `DDCvar.OffsetRootBone.TranslationRadius`. `OnUpdate`가 `BasedMovementDelta`(움직이는 base 이동량)를 루트 오프셋에 더한다. EssentialValues의 `RootTransform` = 오프셋 루트(+90° yaw) | 노드 + 모드 함수. Release 조건을 LootNPop 슬롯 3종(`DefaultSlot`·`FullBody`·`UpperBody` 중 전신 슬롯)으로 넓힌다. base 이동량은 Mover base 규칙(자기 Actor 틱)과 맞춘다 | `UpVector`는 디버그에만 등장 — 안전 추정(2-2 확인) | — | 없음(신규) |
| **B4 발 배치** (`Procedural_Feet`) | 이동 모드 Blend: Sliding·Traversing·InAir → `LegIK`(“BasicIK”, 애니의 ik_foot 위치로만 풂), Ragdoll → IK 없음, 그 밖(OnGround·Flying) → Bool(← CVar `a.AnimNode.FootPlacement.Enable`)이 참이면 `CR_Biped_FootPlacement`, 거짓이면 LegIK. `DDCvar.FootPlacementMode`는 Mover ABP에서 쓰지 않는다. CR 입력: `Ground Normal` ← 구조체 `GroundNormal`, `GroundMovementWorldDelta` ← `BasedMovementDelta`(Break 핀 이름으로 추정), `DoRaycast`=true, `Enable Foot Pinning`·`Enable Slope Warping`·`WorldZDamperEnabled`(셋 다 OnGround일 때만), `Has Teleported`, `ForceReset`(OnBecomeRelevant). **5.8 Mover ABP에는 `AnimNode_FootPlacement`가 없다** | `CR_Biped_FootPlacement` 구성을 따른 CR + 레이어(지상만 CR, 공중은 LegIK). 접지 입력은 B0 | CR 씬 쿼리·평면 보정은 리그(컴포넌트) 공간 Z — 메시가 중력에 정렬돼 있으면 안전. ⚠️ **`UpdateRootDamper`가 `To World` 뒤 월드 `.Z`에 스프링 감쇠** → `WorldZDamperEnabled` 끔 또는 중력 Up 축으로 수정 | CR 1개(구성 에셋, 그래프 42개), 본 `foot_*`·`ball_*`·`ik_foot_*`·`pelvis` | 없음(신규) |
| **B5 경사·슬라이드** | 경사: CR 슬로프 워핑(B4) + Chooser가 고르는 `BS_Relaxed_*_Loop_F_Slopes`(블렌드 스택 태그 `BS_Slope`, 입력 `SlopeAngle` ±30°). `SlopeAngle`은 `GetSlopeDegreeAngles(캐릭터 Up)` + `SmoothedGroundNormal`(VInterpTo 5). 슬라이드: `Procedural_PreLayering`이 Sliding일 때 root를 `ModifyBone`(이동 월드 공간·회전 컴포넌트 공간 additive)으로 경사에 맞춘다 | 경사만: 경사 각도·지면 법선 공급(B0) + 경사 BS 행. 슬라이드는 LootNPop에 슬라이드 이동 모드가 없어 제외 | 경사 각도 계산은 캐릭터 Up을 쓴다 — 안전. `SlopeWarping` 노드는 쓰지 않는다(§1, `research` §5의 ⚠️ 회피) | 경사 BS 2개 + 원본 시퀀스 | 없음(신규) |
| **B6 정지·회전 전환** | MM: Chooser가 Starts·Stops·Pivots·Spins·TurnInPlace·Turns·GaitTransitions·Lands DB를 함께 검색. SM: 상태·전이(Pivot·Turn In Place·"Broke" rotation·WrongWay·Started Circling…)와 Chooser 행 | B1 DB 묶음에 포함(MM). 술어(`ShouldTurnInPlace`: \|FutureFacingDelta\|≥50° & 속도<50, `ShouldSpinTransition`: ≥130° & ≥150, `IsPivoting`)는 C++ | B0와 같음 | B1 시퀀스에 포함 | 현 PSD에 없는 세트 **확장** |
| **B7 공중** | MM: Jump·Lands_Light/Heavy DB, 블렌드 시간 `Get_MMBlendTime`(InAir 0.15/0.5, 착지 0.2). SM: In Air 상태 | **AN-007 유지**(Fall_Loop 고정). 착지(Lands_Light/Heavy)만 2-3에서 검토 | Jump 채널에 수직 성분 없음(TechDesign_CombatAnimation §5.5) | Jump 59개 중 착지 일부 | Fall_Loop 고정 경로 유지 |
| **B8 Additive Leans** | 레이어: 이동 모드 Blend(Sliding만 통과, 공중 포함 나머지는 적용) → 이동 상태 Blend(Moving만 적용) → Bool(← `EnableAO`): AO가 켜져 있으면 몸통만, 꺼져 있으면 몸통+머리(`BS_Relaxed_Lean_Head`). 몸통 = `BS_Relaxed_Walk_Leans`↔`BS_Relaxed_Run_Leans` TwoWayBlend(α ← `Speed2D`를 200~320에서 0~1로 매핑). BS 입력 X·Y ← `LeanAmount`. Mesh Space Additive. `LeanAmount` = 가속을 속도 방향 기준으로 Unrotate한 측면 성분 / 속도별 정규화. 곡선 `Disable_AdditiveLeans`로 끔 | 레이어 + C++ `LeanAmount`(캐릭터 로컬 축). 머리 기울기 분기는 LootNPop 조준 상태(FreeAim·락온)로 바꾼다 | ⚠️ `RotationFromXVector(Velocity)`는 월드 Z 기준 회전 → C++에서 접평면 기저로 | BS 3개(Poses 폴더) | 없음(신규). 무기 서브 ABP 오버레이 뒤에 둘지는 2-5에서 |
| B9 Aim Offset | 레이어: `BS_Neutral_AO_Stand_NoSmoothing` Mesh Space Additive, `EnableAO`(회전 모드 Strafe/Aim, 각도 한계 180/110) 아니면 Identity, DeadBlending, 곡선 `Disable_AO`. `AimingRotation`을 Critical Spring으로 평활 | **가져오지 않는다.** 무기별 AO(서브 ABP + C++ 로컬 좌표 `AimYaw`/`AimPitch`)를 유지. 평활(Critical Spring)만 필요하면 C++에 | GASP는 Rotator 델타(월드) — LootNPop 방식이 이미 구면 대응 | — | 무기 서브 ABP AO 유지 |
| B10 포즈 히스토리·DeadBlending | `PoseHistoryCollector`를 **출력 직전**(절차 레이어 뒤)에: 포즈 2개, 수집 본(foot·thigh·spine…), 곡선 `Phase`, 궤적 이력 10·예측 8. `DefaultSlot` 뒤 `DeadBlending`(0.25s, Ragdoll 강도 곡선 필터) | 히스토리 위치·설정을 GASP식으로, 슬롯 뒤 DeadBlending 추가 | — | — | 기존 `PoseHistoryCollector`·슬롯 3종 **확장** |
| B11 래그돌 연동 | `Ragdoll` 상태 머신(NoRagdoll ↔ In Ragdoll ↔ Blend Out Pose, 진입 MovementMode==Ragdoll). In Ragdoll 안의 중첩 상태 머신이 공중(`BS_Ragdoll_Flail`/`Reach`)·지면(부상 상태별 `M_ragdoll_poses_injured`·`BS_Ragdoll_OnGround/Rolling`) 포즈를 내고 `ModifyCurve`로 `Ragdoll_Strength_*` 곡선을 써서 PhysicsControl 강도를 준다. Blend Out은 `PoseSnapshot 'Ragdoll'` | **제외**(AN-005). Phase 3 참고: "애니 곡선으로 PhysicsControl 강도를 준다" 패턴 | — | Ragdoll 폴더 47개(Phase 3에서 필요한 것만) | 없음. 사망 랙돌은 `VisualMesh` 물리 |
| B12 상호작용 손 IK | `Procedural_FullBody`: `CR_UEFN_Mannequin_FullBodyIK`, 입력은 Motion Matching Interaction 결과(`hand_l/r` 목표·α) | **제외**(상호작용 시스템 없음) | — | — | 없음 |

범위 밖으로 확정: Traversal(장애물 넘기), Slide, Debug Flying, MMI. Crouch는 `MoverComponent->IsCrouching()`이 있으나 앉기 입력이 없어 보류(행만 비워 둔다).

### 2.3 `ULNPAnimInstance`가 공급할 값 (AN-002)

GASP는 Pawn이 BPI로 구조체를 넘기고 ABP가 가공한다. LootNPop은 Mover·캐릭터에서 C++가 직접 계산한다. 속도·가속은 캐릭터 Up에 수직인 접평면으로 투영한다.

| 값 | GASP 출처 | LootNPop 공급원 | 쓰는 블록 |
|:---|:---|:---|:---|
| `Velocity`, `Speed2D`, `LastNonZeroVelocity` | 구조체 `Velocity` → `VectorLengthXY` | 기존 `Velocity`·`GroundSpeed`(투영), 0 아닐 때 보관 | B1·B2·B6 |
| `Acceleration`·`AccelerationAmount`(/MaxAccel)·`HasAcceleration`, `VelocityAcceleration`(ΔV/dt)·`RelativeAcceleration`(로컬) | `InputAcceleration`·`CurrentMaxAcceleration` | Mover InputCmd 이동 의도 × 최대 가속, 속도 차분 | B0·B8 |
| `MovementMode`(OnGround/InAir) | 구조체 `MovementMode` | `bIsOnGround` 기반 enum(슬라이드·트래버설 없음) | B1·B3·B4·B7 |
| `MovementState`(Idle/Moving) | 궤적 0.9~1.0s 속도 ≠ 0(허용 10) | Mover 궤적 예측기 | B1·B3·B6 |
| `Gait`(Walk/Run/Sprint), `Stance`, `RotationMode`, `MovementDirection` | 구조체 필드(Pawn 입력 상태) | `bIsSprinting`·입력 크기, `bIsCrouching`, `bShouldStrafe`·조준 모드, 로컬 이동 방향 4분면 | B1 Chooser 행 |
| 각 상태의 `_LastFrame`·`_Recent`·`_Time` | `UpdateStateValues` | C++ 상태 이력(전이·블렌드 시간·DB 인터럽트) | B1·B6 |
| 궤적 파생: `FutureFacingDelta`, `Trj_*Velocity`(과거·근미래·미래), `Trj_TurnAngle`, `Trj_IsCircling`·`CirclingTime`, `Trj_FutureFacing` | `PoseSearchGenerateTrajectory(Predictor)`(0.033s·15·0.1s·15) | 기존 `UMoverTrajectoryPredictor` 궤적에서 C++로 | B2·B6 |
| `IsPivoting`·`ShouldTurnInPlace`·`ShouldSpinTransition`·`JustLanded_Light/Heavy`·`LandVelocity`·`JustTeleported` | ABP 함수 | C++ bool(AN-003). 착지는 Mover 착지, 텔레포트는 위치 점프 임계 | B4·B6·B7 |
| `GroundNormal`(평활)·`SlopeAngle_X/Y`, `GroundLocation` | 구조체 + `GetSlopeDegreeAngles(캐릭터 Up)` | Mover 바닥 결과 | B4·B5 |
| `BasedMovementDelta` | 구조체 | Mover 움직이는 base 이동량 | B3·B4 |
| `LeanAmount` | 측면 가속 / 속도별 정규화 | 캐릭터 로컬 축으로 계산 | B8 |
| AO | `AimingRotation` | 기존 `AimYaw`/`AimPitch` 유지 | (B9 미채택) |

### 2.4 Up 축 점검 결과 요약

| 위치 | 가정 | 조치 |
|:---|:---|:---|
| `Speed2D`·`IsStarting`의 `VectorLengthXY`, Lean의 `RotationFromXVector` | 월드 Z | C++ 접평면 투영(B0) |
| `StrideWarping` `FloorNormalDirection`·`GravityDirection` | `WorldSpaceVector` ±Z(엔진 기본값) | `ComponentSpaceVector` (0,0,±1) |
| `CR_Biped_FootPlacement` `UpdateRootDamper` | 월드 `.Z` 감쇠 | `WorldZDamperEnabled` 끔 또는 중력 Up 축으로 수정 |
| `Steering` 목표 방향 | 궤적 facing(월드 쿼터니언)과 가상 루트의 상대 회전 전체를 루트모션에 더한다 | 2-3 확인: 이동 중 궤적 facing은 중력 Up 기준(기울기 1° 미만). 스폰 첫 프레임 빈 이력만 약 90° → C++ 목표 함수가 Up으로 정렬(§2.10) |
| `ModifyBone`(슬라이드) 이동 월드 공간 | 입력 벡터가 월드 | 슬라이드 제외라 해당 없음 |
| `OffsetRootBone` | ⚠️ 2-3 엔진 확인: `GroundNormal` 기본 월드 (0,0,1) 평면 투영, `bClampToTranslationVelocity`의 `GetSafeNormal2D`, Accumulate·Interpolate가 캡슐 회전 변화를 안 더함(구면 이동 시 메시가 기욺) | 지면 법선 = 컴포넌트 Up(C++ 함수), 속도 클램프 끔, 매 프레임 가상 루트 Up을 컴포넌트 Up으로 스윙 정렬(§2.10) |
| `OrientationWarping`·CR 씬 쿼리·`GetSlopeDegreeAngles` | 컴포넌트/캐릭터 Up | 안전(OrientationWarping은 2-3에서 가상 루트 로컬 변환 확인). 나머지는 블록 단위에서 적도 확인 |

### 2.5 선택 코어 경로 — MM 노드 방식 (AN-015)

사용자 결정(2026-10-18): **Motion Matching 노드 방식(B1)**으로 간다. GASP Mover 기본값인 State Machine + Chooser + Blend Stack 방식(B1′)은 만들지 않는다.

- 사용자 사유: SM 방식은 아직 검증되지 않았고(GASP에서도 "Experimental" 이름이 붙은 상태 머신·데이터 폴더), 나중에 직접 유지보수하기 어렵다.
- 설계 근거: MM 방식은 선택이 Chooser→PSD로 끝나 ABP에 분기가 없다(AN-003). 현 `ABP_Lyra`가 MM이라 교체 범위가 좁고, PSD·Chooser는 UAF에서 재사용된다(Phase 5).
- 시작·정지·피벗·제자리 회전은 MM의 Starts/Stops/Pivots/TurnInPlace DB와 Steering으로 얻는다(B6).

### 2.6 Phase 2 착수 시 확인할 것

- `CHT_PoseSearchDatabases_Relaxed` 8행 각각의 결과 → 2-2에서 C++ 덤프로 확인했다(§1.4·§2.9). 스크린샷은 필요 없었다.
- 행 조건: OnGround·Stand·Idle(보행 무관) / OnGround·Stand·Moving × Walk·Run·Sprint / OnGround·Crouch·Idle / OnGround·Crouch·Moving / Sliding / InAir.
- 노드 수치(블렌드 시간·반감기 등)는 GASP 기본값이다. 블록 단위에서 PIE로 다시 맞춘다.

### 2.7 스켈레톤 기준 — UEFN 전환 (AN-011, 0-3 확정)

결정: 로코모션은 UEFN 스켈레톤과 GASP 5.8 클립을 그대로 쓰고, Lyra 애니(공격·리로드·무기 레이어·몽타주)는 UEFN으로 오프라인 리타깃해 계속 쓴다. 0-3 조사 결과(2026-10-18, 수행 기록은 `history/Phase00_Log.md`):

| 근거 | 내용 |
|:---|:---|
| 현재 메시 구성 | 플레이어·적 모두 `AnimSourceMesh` = `SKM_Manny`(Lyra) + `ABP_Lyra`. `VisualMesh`는 `ABP_Retarget`으로 런타임 리타깃한다 — 플레이어 `SKM_Manny_Simple`(MoverSample, `RTG_Lyra_to_MS`), `ActorPromoted` 적 `SKM_UEFN_Mannequin`(`RTG_Lyra_to_GASP`). `ABP_Retarget`은 `RTG_GASP_to_MS`도 이미 참조한다 |
| UEFN 전환 시 리타깃량 | 추적 에셋 255개에서 의존성을 따라가면 실제로 쓰는 `Lyra_Mannequin/Animations`는 701개 중 121개다. `PSD_*_Lyra` 로코모션 30개는 GASP로 대체되므로 **91개**(서브 ABP 경로 37 · 몽타주 경로 55, 그중 몽타주 21·AO 2). 2-0a 재집계에서 `Weapons/` 아래 캐릭터 몽타주 6개와 `ABP_Lyra`의 `Fall_Loop` 1개가 더해져 실제 리타깃은 **98개**다(`phases/Phase02_GASPMoverABP.md`). 여기에 서브 ABP 5개 재생성, `CHT_Montage`·GA 2개(`GA_Stagger_LongSword`·`GA_ParrySuccess_LongSword`) 참조 재지정 |
| Lyra 유지 시 리타깃량 | MM 경로 시퀀스 461개(앉기·슬라이드·트래버설 빼면 약 319개)·경사 BS 2·Lean BS 3, 곡선 6종·블렌드 프로파일 `FastFeet_InstantRoot`·미러 테이블 `MDT_UEFN_Mannequin`을 Lyra에서 다시 맞춘다. GASP를 업데이트할 때마다 반복한다(AN-013 덮어쓰기 이점 상실) |
| 본 이름 | `root`·`pelvis`·`spine_01~05`·`neck_01/02`·`head`·`ik_foot_root`·`ik_foot_l/r`·`ik_hand_root/gun/l/r`·`weapon_r`는 Lyra(164본)·UEFN(88본)·MoverSample(161본) 모두에 있다. UEFN은 Lyra의 부분집합 + `attach`. UEFN에 없는 것은 보정 본, `interaction`·`center_of_mass`, 가상 본 `VB IK_Hand_*` 3개(2-0a 확인: 메인·서브 ABP 어디에도 쓰지 않는다. 왼손 IK는 `ABP_Sub_LongSword`의 TwoBoneIK `hand_l`) |
| GASP 자체 구성 | GASP 5.8도 UEFN 원본을 `RTG_UEFN_to_UE5_Mannequin` 등으로 다른 메시에 리타깃한다. LootNPop의 `AnimSourceMesh`/`VisualMesh` 분리와 같은 구조다 |

전환 후 구조(2-0b):

- `AnimSourceMesh` = `SKM_UEFN_Mannequin` + UEFN 스켈레톤 메인 ABP. 본이 164개에서 88개로 줄어 포즈 평가 비용이 낮다.
- `VisualMesh`: 플레이어는 UEFN → MoverSample 리타기터(`RTG_GASP_to_MS` 계열), UEFN 메시 적은 같은 스켈레톤이라 런타임 리타깃 없이 포즈를 복사한다. 랙돌·물리 히트리액트가 `VisualMesh` 자신의 물리를 쓰므로 Leader Pose가 아니라 ABP 안 포즈 복사(Copy Pose From Mesh)를 우선 검토한다.
- 새 캐릭터 메시는 UEFN → 그 메시 리타기터 하나만 만들면 된다.
- 무기·몽타주 애니는 리타깃 사본(`Content/Characters/Retargeted/`, git 제외, AN-016)을 쓴다. 그 사이 로코모션은 1C에서 최신화된 `PSD_*_UEFN` 4개를 임시 Chooser로 고르고, 2-2에서 GASP 상태별 PSD로 교체한다. 옛 `CHT_PSD_UEFN`은 1C-2에서 삭제됐으므로 임시 Chooser는 2-0b에서 새로 만든다(권고, `phases/Phase02_GASPMoverABP.md` 결정 A).
- 기각: Lyra 시퀀스를 UEFN에서 "호환 스켈레톤"으로 직접 재생하는 방식. 리타깃 설정이 GASP 소스 에셋인 스켈레톤에 붙어 덮어쓰기 때 사라지고, IK 리타기터보다 비율 보정 품질이 낮다.

GASP 5.8 시퀀스의 외부 의존(MM 경로 Chooser에서 따라간 결과, 처리 규칙 AN-017): `/Game/Misc/SandboxAnimCurveCompressionSettings`(681개 시퀀스), `BP_AnimNotify_FoleyEvent_*` 10종(→ `/Game/Audio/Foley/*`), `BP_NotifyState_EarlyTransition`(22개 시퀀스, → `BPI_SandboxCharacter_ABP`·`SandboxCharacter_CMC_ABP`), `AM_BakePhaseCurveFromFootstepNotifies`(5개 시퀀스, 엔진 의존만). 현재 LootNPop의 옛 GASP 시퀀스 414개에는 GASP 폴더 밖 의존이 없다.

### 2.8 B0 계산식·임계값 (2-1 확인, 2026-10-19)

GASP 8001에서 `SandboxCharacter_Mover_ABP`의 `Update_*`·술어 함수와 폰 `SandboxCharacter_Mover`의 방향 함수를 DSL과 노드 정보로 읽었다(두 BP 모두 dirty, 저장 안 함). 구현은 `ULNPAnimInstance::UpdateLocomotionValues`이고 상수 이름은 cpp 상단에 있다. "LootNPop" 열이 비어 있으면 GASP와 같다.

| 값 | GASP 계산 | LootNPop |
|:---|:---|:---|
| 갱신 순서 | `Update_Logic` = Trajectory → EssentialValues → States → AimOffset → AdditiveLean. 게임 스레드에서 `Update_PropertiesFromCharacter`(BPI 구조체)를 받고, 로직은 스레드 세이프 갱신 | 궤적 생성 → 필수값 → 궤적 파생 → 상태 → Lean, 전부 `NativeUpdateAnimation`. 메인 인스턴스만 |
| 궤적 생성 | `PoseSearchGenerateTrajectory(Predictor)` 이력 0.033s×15, 예측 0.1s×15. 예측기 `MoverTrajectoryPredictor`(샘플 60Hz) | C++가 소유(`Trajectory`). 이전에는 ABP EventGraph가 엔진 기본값(0.04×10, 0.2×8)으로 만들었다 |
| `Speed2D`·`HasVelocity` | `VectorLengthXY(Velocity)`, > 5 | 접평면 속력(`GroundSpeed`) |
| `LastNonZeroVelocity` | HasVelocity일 때 `Velocity` | 접평면 속도 |
| `Acceleration`·`AccelerationAmount`·`HasAcceleration` | 입력 가속 / `CurrentMaxAcceleration`(SafeDivide), > 0 | Mover 이동 의도(접평면, 크기 ≤1) × `CommonLegacyMovementSettings.Acceleration` |
| `VelocityAcceleration` | (V − V_lastFrame) / max(dt, 0.001) | 접평면 속도 차분. 텔레포트 프레임은 0 |
| `RelativeAcceleration` | `Unrotate(VelocityAcceleration, RootTransform)` (오프셋 루트 +90° 또는 캐릭터 트랜스폼) | 캐릭터 로컬 |
| `SmoothedGroundNormal`·`SlopeAngle_X/Y` | `VInterpTo(prev, GroundNormal, dt, 5)`, `GetSlopeDegreeAngles(루트 Right, 평활 법선, 캐릭터 Up)` → X=Roll, Y=Pitch. 바닥 없음 = (0,0,1) | 바닥은 Mover 마지막 바닥 결과(`TryGetFloorCheckHitResult`, 지상일 때만). 없음 = 캐릭터 Up. 평활 결과를 정규화 |
| `BasedMovementDelta` | 폰이 `OnBasedMovementApplied` 마지막 값을 보관 | 애니 갱신 사이 누적, 매 갱신 뒤 비움 |
| 상태 이력 `UpdateStateValues` | LastFrame ← Current, Current ← New, 바뀌면 Time = 0(LastStateTime ← Time) 아니면 Time += dt, Time ≥ 제한이면 Recent ← Current. 제한: MovementMode 0.2s, 나머지 0.1s | 같음. `LastStateTime`은 쓰는 곳이 없어 뺐다 |
| `MovementState` | `IsMoving` = 궤적 0.9~1.0s 속도(외삽) ≠ 0, 성분별 허용 10 | 접평면 속력 > 10 |
| `Gait` | 폰 입력(질주 키 → Sprint, 걷기 토글·아날로그 0.7 미만 → Walk, 나머지 Run). ABP에서 Sprint이고 `Trj_IsCircling`이면 Run | 질주 Modifier → Sprint, Mover 현재 `MaxSpeed` < 450 → Walk(걷기 입력이 없다. `CHT_PSD_UEFN` 경계와 같다), 나머지 Run. Circling 규칙 같음 |
| `RotationMode` | 폰 입력(조준 → Aim, 스트레이프 → Strafe, 기본 OrientToMovement) | `bShouldStrafe`(= 이동 방향 회전 끔) 아니면 OrientToMovement, Mover ADS면 Aim, 아니면 Strafe |
| `MovementDirection` | 폰 입력 단계에서 계산(InputCmd로 전달). OrientToMovement·Sprint·속도 0이면 F. 아니면 지상 속도 방향과 입력 facing의 Yaw 차를 임계로 가른다: 현재 F·B면 ±60/±120, 현재 좌·우면 ±40/±140(히스테리시스, `DDCvar.StrafeStyle` 0). 결과는 F·B·좌(음수 각)·우 넷뿐이다(enum에는 LL·LR·RL·RR이 있으나 판정이 하나씩만 돌려준다) | 같은 임계, 각도는 기존 `Direction`(캐릭터 회전 기준 로컬 계산). enum은 F·B·L·R |
| 방향 강제 전환 | `Update_Trajectory`: \|FutureFacingDelta − 직전\| > 200이면 MovementDirection·Recent를 B로 → 상태 갱신에서 "방향 바뀜"이 되어 MM DB 인터럽트 | 같음 |
| 궤적 속도 | Past −0.3~−0.2s, NearFuture 0.1~0.2s, Future 0.4~0.5s(외삽 없음), PreviousFuture = 직전 Future | 접평면 투영 |
| `Trj_TurnAngle` | `Delta(RotFromX(FutureVelocity), RotFromX(Velocity)).Yaw` | Up 축 부호각(속도 둘 중 하나가 0이면 0) |
| `Trj_FutureFacing` | 1.5s 샘플 facing | 같음 |
| `FutureFacingDelta` | 0·0.25·0.75·1.5s 샘플 facing 누적: 첫 항은 오프셋 루트 대비, 나머지는 앞 샘플 대비 `Delta.Yaw` 합 | 첫 기준은 메시 facing(오프셋 루트 B3 전까지 같다). Up 축 부호각 |
| 각속도·`Trj_IsCircling` | 각속도 Past −0.4~−0.3, Current 0~0.1의 **Z**. 둘 다 < −200 또는 둘 다 > 200, 그리고 \|TurnAngle\| > 50. `CirclingTime`은 그동안 누적 | Up 성분 |
| `IsStarting` | IsMoving & \|FutureVelocity\|XY ≥ Speed2D + 100 | 접평면 |
| `IsPivoting` | MovementState == Moving & (MM 조건) \|Trj_TurnAngle\| ≥ 75(회전 모드 3종 같음) & !Circling. SM 경로 조건은 별도 | MM 조건 |
| `ShouldTurnInPlace`·`ShouldSpinTransition` | \|FutureFacingDelta\| ≥ 50 & Speed2D < 50 / ≥ 130 & ≥ 150 | 같음 |
| `JustLanded_Light/Heavy` | MovementMode OnGround & LastFrame InAir, `Velocity_LastFrame.Z` > −500 / ≤ −500(`HeavyLandSpeedThreshold`) | 직전 속도의 Up 성분 |
| `JustTeleported` | 직전·현재 캐릭터 위치 거리 > 50(`TeleportThreshold`) | **메시** 위치 이동 거리 > 50 + max(\|V\|, \|V_lastFrame\|)·dt. 메시로 재는 이유: 시뮬 프록시는 Actor가 네트워크 값으로 스냅하고 메시만 평활된다. 허용치는 런처·활공(1200cm/s)·고속 낙하 착지(직전 −3000cm/s → 0) 오탐 방지. 2-1 PIE에서 남은 발생: 리스폰·적 승격 스냅(정상), 게스트가 보는 호스트가 1000~2000cm/s일 때 약 80초에 15회(메시가 한 프레임에 실제로 크게 튀는 경우) — 2-5에서 발 배치 리셋에 쓸 때 시뮬 프록시 처리를 정한다 |
| `LeanAmount` | 측면 가속 = `Unrotate(VelocityAcceleration, RotFromX(Velocity)).Y` / `MapRangeClamped(Speed2D, 200~320 → 500~800)`, −1~1. 방향별: F (L,0), B (−L,0), 좌 (0,L), 우 (0,−L) | 접평면, 속도 방향 Right = Up × 속도 |

2-2에서 쓸 GASP 값(이번 단위에서 구현하지 않음):

- `Get_MMInterruptMode`: MovementMode가 바뀌었거나, (MovementDirection이 바뀌었고 Moving)이면 `InterruptOnDatabaseChange`, 아니면 `DoNotInterrupt`.
- `Get_MMBlendTime`: OnGround에서 직전 OnGround 0.5s, 직전 InAir 0.2s. InAir에서 Velocity.Z > 100이면 0.15s 아니면 0.5s.
- `Get_MMNotifyRecencyTimeOut` 0.2s(보행 무관). `Update_MotionMatching`: Chooser `CHT_PoseSearchDatabases_Relaxed` → `SetDatabasesToSearch(결과, InterruptMode)`. PostSelection에서 선택 DB 태그를 읽는다.
- `Get_DesiredFacing`(Steering 목표, B2): 블렌드 스택 현재 애니 곡선 `SteeringTargetTime`(0~1)을 0.1~1.5s로 매핑한 궤적 샘플 facing.

B10 노드 설정(GASP):

- `PoseHistoryCollector`: 포즈 2개, `samplingInterval` 0, 수집 본 `foot_r·foot_l·thigh_r·thigh_l·spine_05·pelvis·hand_l·hand_r`, 곡선 `Phase`, `rootBoneTranslationRecoveryRatio` 0·회전 1, `bGenerateTrajectory` false(궤적은 핀 바인딩), 이력 10·예측 8(생성 안 해서 무관). LootNPop PSS(`PSS_CoreLocomotion_UEFN`)는 발 2개와 궤적만 쓰므로 이 본 집합으로 충분하다.
- `DeadBlending`(`DefaultSlot` 바로 뒤): 0.25s HermiteCubic, 블렌드 프로필 없음, 외삽 반감기 1(최소 0.05·최대 1), 최대 속도 이동 500·회전 360·스케일 4·곡선 100, 필터 곡선 `Ragdoll_Strength_*` 4개(LootNPop에는 없는 곡선이라 넣지 않는다).

2-1 PIE(리슨 2인, 적도)에서 본 LootNPop 쪽 값의 성질 — 2-2·2-4에서 Chooser 행·술어를 쓸 때 참고:

- 승격 적은 근접 거리에서 AI 이동 의도가 켜졌다 꺼졌다 해서 `MovementState`가 0.1~0.5초 간격으로 Idle↔Moving을 오간다(속도 100~190cm/s에서 Idle로 바뀜 — 궤적이 곧 멈춘다고 예측). GASP 계산 그대로의 결과다. Chooser가 이 값으로 DB 묶음을 바꾸면 DB가 자주 바뀔 수 있어 `_Recent` 사용을 검토한다. 적 `Gait`도 AI가 정한 최대 속력에 따라 Walk↔Run이 같이 바뀐다.
- 800cm/s 질주 중 급반전은 대부분 `bShouldSpinTransition`(예측 facing 누적 146°)으로 잡히고 `bIsPivoting`은 드물다(1회, 286cm/s에서 −161°). LootNPop 이동은 고속에서 크게 호를 그려 0.5초 안의 속도 방향 변화가 75°에 못 미친다.
- 스폰 직후 공중에서 `FutureFacingDelta`가 1초 안쪽으로 90°를 한 번 보인다(`TurnInPlace(facing 90)` 1회). 첫 궤적의 facing 기준이 메시 오프셋과 잠깐 어긋나는 것으로 보이며 공중이라 AN-007 고정 루프가 재생돼 영향이 없다.
- 스트레이프 무기로 좌우를 연타하면 `MovementDirection`이 0.05~0.15초 간격으로 L↔R을 오간다(입력 그대로). `_Recent`는 0.1초 유지 뒤에만 바뀐다.

### 2.9 MM 선택 코어 — GASP Chooser 결과와 LootNPop 구성 (2-2 확인, 2026-10-19)

`CHT_PoseSearchDatabases_Relaxed`를 `LNP.Editor.GASPRef`로 덤프했다(§1.4). 루트 8행의 결과는 전부 **중첩 Chooser**이고, ABP는 멀티 결과로 평가해 나온 DB를 모두 검색한다.

| 루트 행(이동 모드·자세·이동 상태·보행) | 중첩 테이블 | 결과(행 순서, 비활성 제외) |
|:---|:---|:---|
| OnGround·Stand·Idle | `Stand Idles`(8) | `Speed2D` 0~20 → `Stand_Idles` / 20~170 → `Walk_Stops` / 170~400 → `Run_Stops` / 400~ → `Sprint_Stops` / `ShouldTurnInPlace` → `TurnInPlace` / 착지 Light·Heavy → `Idle_Lands_*` / 직전 Sliding → 슬라이드 탈출 |
| OnGround·Stand·Moving·Walk | `Stand Walks`(25) | `IsStarting` × F·B·LL·RL → `Walk_*_Starts` / 직전 OnGround × F·B·LL·RL → `Walk_*_Loops` / `IsPivoting` + 방향·`MovementDirection_Recent` 마스크 → `Walk_F_Turns`(F→F)·`Walk_*_Pivots` / `ShouldSpinTransition` × 방향 → `Walk_*_Spins` / `JustTraversed` → `FromTraversal` / 착지 → `Walk_Lands_*` / 슬라이드 탈출(비활성) |
| …·Run | `Stand Runs`(26) | Walk와 같은 구성 + 직전 보행 Walk·Sprint면 `Run_F_GaitTransitions` |
| …·Sprint | `Stand Sprint`(6) | `IsStarting` → `Sprint_Starts` / 직전 OnGround·!Pivot → `Sprint_Loops` / `IsPivoting` → `Sprint_Turns` / 착지 Light·Heavy / 슬라이드 탈출 |
| Crouch Idle·Crouch Moving·Slide·InAir | 각 2~4행 | 범위 밖(InAir는 `Stand_Jump`·`WalkAndRun_Jump`·`Sprint_Jump`·`Jumps_FromTraversal`) |

- 방향 enum `E_MovementDirection` 표시 이름 = F·B·LR·LL·RL·RR(값 0~5). Chooser는 좌·우를 **LL·RL**로만 고른다. `LR`·`RR` 루프 DB는 어느 행도 쓰지 않는다. 각 행에 방향 "Any" 행이 하나씩 비활성으로 남아 있다.
- 루프 DB 항목은 모두 `bDisableReselection=True`, 미러·샘플 범위 기본값. 비용 바이어스는 DB마다 다르다(예: Run LL·RL 진행 포즈 −0.2, Sprint −0.5, Idles −0.01).
- 스키마: `PSS_Relaxed_Loops` = 궤적(−0.125·0·0.33·0.8·1.35s, 위치·속도 XY) + `Phase` 곡선(0, −0.033s) + 골반 Heading X(0.3), **포즈 채널 없음**. `PSS_Relaxed_Idle` = 포즈(발 위치·속도, 골반·spine_05 속도) + 궤적(**가중치 0**). 정규화는 `PSN_Relaxed_All` 하나(`NormalizeWithCommonSchema`).
- GASP MM 노드: 0.5s HermiteCubic, 블렌드 프로필 `FastFeet_InstantRoot`, 재생 속도 0.85~1.15, `poseReselectHistory` 0.3, 최대 블렌드 4, `bShouldFilterNotifies`.

LootNPop 구성(2-2):

- 평면 멀티 결과 Chooser `CHT_PoseSearchDatabases_UEFN` 11행 — 지상·Idle → `PSD_Stand_Idles_UEFN`, 지상·Moving × Walk·Run × F·B·L·R → `PSD_Stand_{Walk,Run}_{F,B,LL,RL}_Loops_UEFN`, Sprint → `PSD_Stand_Sprint_Loops_UEFN`, 공중 → `PSD_Jump_UEFN`. 2-4에서 Starts·Stops·Pivots·Spins·Turns·TurnInPlace·Lands 행을 더한다(술어·`_Recent`·`_LastFrame`은 2-1에서 이미 공급). 루프 행에 GASP의 "직전 OnGround" 조건은 넣지 않았다 — Lands DB가 없어 착지 프레임에 빈 결과가 되기 때문이다(2-4에서 같이 넣는다).
- PSD·PSS·PSN은 GASP 것을 `LNP.Editor.GASPRef clone`으로 `Characters/MotionMatchingData/`에 복제했다(항목·바이어스·스키마 값이 GASP와 같다). MM 노드 설정은 GASP 값으로 맞췄다.
- 인터럽트: GASP 식 + **출발 프레임(Idle→Moving)**. Idle 스키마가 궤적을 보지 않아 진행 중인 Idle 포즈가 움직여도 이기기 때문이다(GASP는 Starts DB가 이 구간을 받는다). 2-4에서 Starts DB가 들어오면 이 예외를 다시 본다.
- 승격 적 `MovementState` 깜빡임: Chooser는 현재 값을 쓴다. 사용자 PIE(적도·리슨 2인·승격 적 전투)에서 떨림이 보이지 않았다.

### 2.10 B2 블렌드 스택·B3 Offset Root — GASP 노드 값과 LootNPop 구성 (2-3 확인, 2026-10-20)

GASP `SandboxCharacter_Mover_ABP`를 MCP(`find_nodes`·`get_node_infos`·`get_properties`)로 읽고, 바인딩은 사용자 Ctrl+C 텍스트로 확인했다(DSL 미사용, 저장 없음).

**MM 노드 블렌드 스택 그래프(블렌드 샘플마다 따로 돈다)**

| 순서 | 노드 | GASP 값 | LootNPop |
|:---|:---|:---|:---|
| 1 | LocalToComponent | — | 같음 |
| 2 | CopyBone `foot_l→ik_foot_l`, `foot_r→ik_foot_r` | 컴포넌트 공간, **이동·회전·스케일 복사가 모두 꺼짐**(바인딩 없음) → 사실상 무동작 | 같은 값으로 두었다. ik_foot 정렬이 필요하면 2-5(발 배치)에서 켠다 |
| 3 | Steering | 활성 = `EnableSteering`(현재 애니 활성·`IsMoving`, 또는 공중·슬라이드), 목표 = `Get_DesiredFacing`(곡선 `SteeringTargetTime` 0~1 → 궤적 0.1~1.5s facing), 절차 목표 시간 = 같은 곡선 → 0.1~0.3s, `AnimatedTargetTime` **10**(핀 값, 노드 구조체 2는 낡은 값), `DisableSteeringBelowSpeed` 10, 알파 블렌드 0.1s | C++ `ShouldEnableSteering`·`GetSteeringTargetFacing`(+ 컴포넌트 Up 정렬)·`GetSteeringProceduralTargetTime` |
| 4 | Steering(제자리 회전) | 활성 = `CurrentDatabaseTags`에 `TurnInPlace`, 목표 같음, 절차 목표 시간 0.2 고정, `DisableSteeringBelowSpeed` −1 | C++ `IsTurnInPlaceDatabaseActive`(MM 검색 결과 DB 태그, 1프레임 늦음). 2-4에서 TurnInPlace DB가 생기면 동작 |
| 5 | OrientationWarping | Graph 모드, α = 현재 애니 곡선 `Enable_Warping`, 방향 = `LastNonZeroVelocity`, 공간 `RootBoneTransform`(바인딩 `Get_OrientationWarpingWarpingSpace` = Offset Root가 켜져 있으면 이 값), 척추 `spine_01~05`, IK 발 `ik_foot_l/r`·`ik_foot_root`, 회전 보간 속도 8, `LocomotionAngleDeltaThreshold` 135 | α = C++ `GetOrientationWarpingAlpha`, 공간 상수 |
| 6 | ResetRoot → ComponentToLocal | α 1 | 같음 |

- 함수 입력은 태그 `MMBlendStackInput`(입력 노드) 노드 참조다. 엔진 `GetCurrentBlendStackAnimAsset/Time`이 그 샘플의 애니·시간을 준다.
- MCP `create_node`는 블렌드 스택 그래프 같은 중첩 그래프에서 `Cannot cast ... to 'Blueprint'`로 실패한다(`connect_pins`·`break_pins`·`set_properties`는 된다). GASP Ctrl+C 텍스트의 함수 참조를 C++ 함수로 바꿔 붙여넣었다(변환 스크립트는 세션 스크래치, 결과 `Saved/AnimOverhaul/lnp_2-3_blendstack_paste.txt`).

**Offset Root Bone(AnimGraph, `DeadBlending` 뒤 · 무기 후처리 레이어 앞)**

| 항목 | GASP | LootNPop |
|:---|:---|:---|
| 이동 모드 | `DefaultSlot` 몽타주 → Release, 지상 `IsMoving` → Interpolate·아니면 Release, 공중 → Release | 전신 슬롯 `DefaultSlot`·`FullBody` 몽타주 → Release, 나머지 같음(`GetOffsetRootTranslationMode`) |
| 회전 모드 | `DefaultSlot` 몽타주 → Release, 아니면 Accumulate(Ragdoll·Flying → Release) | 전신 슬롯 → Release, 아니면 Accumulate |
| 이동 반감기 | Idle 0.1, Moving 0.3 | 같음 |
| 회전 반감기 | 0.1 | 같음 |
| `MaxTranslationError` | 바인딩 `Get_OffsetRootTranslationRadius` = CVar `DDCvar.OffsetRootBone.TranslationRadius`(ini 기본 0, 런타임 값 미확인), 노드 기본 30 | 30 상수 |
| `MaxRotationError` | −1(제한 없음) | **90°** — 제자리 회전 DB가 없는 동안 정지 중 허리가 180°까지 꺾이는 것을 막는다. 2-4 뒤에도 안전망 |
| `bClampToTranslationVelocity` | 바인딩 `IsMoving` | 끔(엔진이 `GetSafeNormal2D` 사용) |
| 지면 법선 | 기본 월드 (0,0,1) | `GetOffsetRootGroundNormal` = 컴포넌트 Up |
| base 이동량 | `OnUpdate`에서 `ApplyDeltaToOffsetRootBone(BasedMovementDelta)` | `NativeThreadSafeUpdateAnimation`(그래프 갱신 직전)이 태그로 노드를 찾아 더하고 Up 정렬까지 한다. ABP 노드 함수 없음 |

- GASP는 AO·`RootTransform` 계산을 오프셋 루트 기준으로 한다. LootNPop도 AO(`AimYaw`/`AimPitch`)와 `FutureFacingDelta` 첫 기준을 가상 루트로 바꿨다(서브 ABP는 메인 인스턴스 값을 읽는다).
- 디버그: `LNP.Anim.DebugLocomotion` 표시 마지막 줄과 `[AnimB3]`(2, 1초마다) — 모드, 거리, Yaw, 기울기, 궤적 facing 기울기. 거리는 직전 평가 가상 루트와 이번 프레임 메시를 비교해 한 프레임 이동량이 더해진다. 엔진 토글 `a.AnimNode.OffsetRootBone.Enable`·`a.AnimNode.Steering.Enabled`·`a.AnimNode.OrientationWarping.Enable`, 시각화 `a.AnimNode.OffsetRootBone.Debug`.
