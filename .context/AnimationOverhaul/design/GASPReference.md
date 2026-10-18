# GASP 참조 조사

> 상태: 초안(§1 단위 0-1, §2 단위 0-2, §2.7 단위 0-3 완료 — 블록별 수치는 Phase 2 착수 시 재확인)
> 마지막 갱신: 2026-10-18
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
| Chooser `CHT_MoverCharacterAnimations_PoseMatch` | `ObjectTools.get_properties`(`columnsStructs`·`contextData`·`resultType`·`outputObjectType`), `AssetTools.get_dependencies` | 컨텍스트(ABP 클래스 ReadWrite, `S_CHT_MoverCharacterAnimations_OUT` Write, `PoseHistoryReference` Read), 컬럼 정의와 **행 조건 값**(1열 `StateName` 이름 컬럼, 10행 = 상태 이름 10개), 결과 타입(`AnimationAsset` 오브젝트). 의존성으로 참조 애니·PSS·enum 전체 목록(약 400개) | **행별 결과**(`ResultsStructs`)와 중첩 Chooser 내용을 못 읽는다. 하위 오브젝트 경로 추측도 실패. 즉 "어느 행이 어떤 애니·하위 테이블·PSS로 가는지"가 빠진다 | 사용자가 Chooser 에디터 스크린샷(행 단위, 중첩 테이블 펼침). 의존성 목록은 스크립트로 폴더별 개수만 요약해 범위 파악에 쓴다 |
| PSS `PSS_SM_Mover_*` | `ObjectTools.list_properties`·`get_properties`, 채널 하위 오브젝트 재귀 | **전부 읽힌다.** 샘플레이트, 스켈레톤(`SK_UEFN_Mannequin`), 전처리, 채널 트리(`Trajectory` 샘플 오프셋·플래그·가중치, `Group` 아래 `Position`·`Velocity`의 본·기준 본·가중치, `Curve`) | 트래젝토리 샘플 `flags`가 비트마스크 정수(예: 48, 176) | 엔진 헤더 `PoseSearchFeatureChannel_Trajectory.h`의 `EPoseSearchTrajectoryFlags`로 해석 |
| PSD (`PSD_SM_CMC_*`로 확인 — `PSD_SM_Mover_*`는 이 폴더에 없음) | `ObjectTools.get_properties`, `AssetTools.get_dependencies` | 스키마 참조, 비용 바이어스, 검색 모드, KDTree·PCA 설정 | **데이터베이스 애니 목록**(`DatabaseAnimationAssets`)과 항목별 설정(미러·샘플 범위) | 의존성 목록으로 들어 있는 애니 집합만 얻는다. 항목별 설정이 필요하면 사용자 스크린샷 |
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
| `Steering` 목표 방향 | 궤적 facing(월드 쿼터니언) | 궤적 facing이 중력 Up 기준인지 적도에서 확인 |
| `ModifyBone`(슬라이드) 이동 월드 공간 | 입력 벡터가 월드 | 슬라이드 제외라 해당 없음 |
| `OrientationWarping`·`OffsetRootBone`·CR 씬 쿼리·`GetSlopeDegreeAngles` | 컴포넌트/캐릭터 Up | 안전(블록 단위에서 적도 확인) |

### 2.5 선택 코어 경로 — MM 노드 방식 (AN-015)

사용자 결정(2026-10-18): **Motion Matching 노드 방식(B1)**으로 간다. GASP Mover 기본값인 State Machine + Chooser + Blend Stack 방식(B1′)은 만들지 않는다.

- 사용자 사유: SM 방식은 아직 검증되지 않았고(GASP에서도 "Experimental" 이름이 붙은 상태 머신·데이터 폴더), 나중에 직접 유지보수하기 어렵다.
- 설계 근거: MM 방식은 선택이 Chooser→PSD로 끝나 ABP에 분기가 없다(AN-003). 현 `ABP_Lyra`가 MM이라 교체 범위가 좁고, PSD·Chooser는 UAF에서 재사용된다(Phase 5).
- 시작·정지·피벗·제자리 회전은 MM의 Starts/Stops/Pivots/TurnInPlace DB와 Steering으로 얻는다(B6).

### 2.6 Phase 2 착수 시 확인할 것

- `CHT_PoseSearchDatabases_Relaxed` 8행 각각의 결과(어느 PSD 묶음인지)는 MCP로 안 읽힌다. 행 조건(이동 모드·자세·이동 상태·보행)과 PSD 이름으로 묶음을 짐작할 수 있으므로, 2-2에서 LootNPop Chooser를 만들 때 Chooser 에디터 스크린샷 1장으로 확인한다.
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
