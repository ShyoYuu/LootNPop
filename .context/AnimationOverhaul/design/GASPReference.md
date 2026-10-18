# GASP 참조 조사

> 상태: 초안(§1 완료 — 단위 0-1, §2는 단위 0-2에서 작성)
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
- **GASP ABP가 dirty로 보인다.** 2026-10-18 조회 뒤 `AssetTools.is_dirty`가 `SandboxCharacter_Mover_ABP`만 `true`를 돌려줬다(CR·Chooser·PSS·PSD·PCA는 `false`). 조회 전 상태를 재지 않아 읽기 툴 때문인지, 원래 그랬는지 모른다. **GASP 에디터를 닫을 때는 저장하지 않는다.** 다음 세션은 첫 조회 전에 `is_dirty`를 먼저 재서 원인을 가린다.
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

단위 0-2에서 작성한다(`phases/Phase00_ReferenceSetup.md`).
