# Phase 0 — 참조 환경과 기준 결정

> 상태: 완료(0-1·0-2·0-3 완료 2026-10-18)
> 선행: 사용자가 GASP 에디터에 `Unreal MCP`·`AllToolsets`를 켜고 포트 8001로 띄운다(AN-009)
> 순서: 단위 0-1 → 0-2 → 0-3. 0-3의 스켈레톤 결정은 사용자 몫이다.

## 배경

이후 모든 Phase는 "GASP에서 읽고 LootNPop에서 만든다"를 반복한다. 그래서 먼저 ① MCP로 GASP 구성을 어디까지 읽을 수 있는지, ② GASP Mover ABP가 어떤 블록으로 이뤄졌는지, ③ 어떤 스켈레톤을 기준으로 소스 에셋을 들여올지를 정한다. 이 Phase는 C++ 코드를 바꾸지 않는다.

## 단위 0-1 — MCP 읽기 범위 확인

1. 세션에 `unreal-mcp-gasp`(8001)와 `unreal-mcp`(8000)가 둘 다 보이는지 확인한다. 안 보이면 에디터·포트를 확인하고 세션을 다시 연다.
2. GASP 쪽에서 다음을 읽어 보고, 대상별로 "어떤 툴로 · 무엇이 읽히는지 · 무엇이 빠지는지"를 기록한다.

   | 대상 | 시도할 방법 |
   |:---|:---|
   | `Blueprints/SandboxCharacter_Mover_ABP`의 AnimGraph·이벤트 그래프 | `blueprint.list_graphs` → `get_graph` → `get_node_infos` / `read_graph_dsl` |
   | `Blueprints/ControlRigs/CR_Biped_FootPlacement` | `controlrig.list_graphs`·`get_graph` |
   | 블렌드 스택 Chooser (`Characters/UEFN_Mannequin/Animations/ExperimentalStateMachineData/CHT_MoverCharacterAnimations_PoseMatch`) | 범용 오브젝트·프로퍼티 읽기 |
   | PSD·PSS (`Characters/UEFN_Mannequin/Animations/ExperimentalStateMachineData/PSS_SM_Mover_*` 등) | 범용 오브젝트·프로퍼티 읽기 |
   | `Characters/UEFN_Mannequin/Rigs/PCA_SandboxCharacter`(PhysicsControl 프로필) | 범용 오브젝트·프로퍼티 읽기 |

3. 읽히지 않는 대상은 대안(사용자 스크린샷, 에디터 Python 덤프, 사용자가 노드 구성 설명)을 정한다.
4. GASP 쪽에서 저장·수정 툴을 쓰지 않는다.

완료 조건: `design/GASPReference.md` §1 "읽기 방법" 표 작성. AnimGraph 포즈 연결이 읽히는지 여부가 명시돼 있다.

## 단위 0-2 — GASP Mover ABP 블록 목록

1. `SandboxCharacter_Mover_ABP`를 블록 단위로 나눈다(블렌드 스택·Chooser 선택, Offset Root Bone, Orientation·Stride Warping, Foot Placement, 정지·회전 전환, 경사, 공중, 래그돌 연동 등).
2. 블록마다 기록한다.
   - 입력 변수와 그 출처(GASP BPI·이동 모드) → LootNPop에서 `ULNPAnimInstance`가 공급할 값(AN-002)
   - 쓰는 엔진 노드·플러그인과 **Up 축 가정**(월드 Z인가, 컴포넌트 공간인가)
   - 필요한 소스 에셋(시퀀스·DB)과 스켈레톤
   - LootNPop의 현재 대응물(`ABP_Lyra`의 어느 부분을 대체·확장하는가)
3. `Roadmap.md` Phase 2 잠정 블록 순서를 확정하거나 고친다.

완료 조건: `design/GASPReference.md` §2 블록 표, `Roadmap.md` Phase 2 표 갱신.

## 단위 0-3 — 스켈레톤과 소스 에셋 규칙 (사용자 결정 포함)

1. **스켈레톤 기준(AN-011)** 선택지를 정리해 사용자에게 묻는다.
   - Lyra 유지: 현 `PSD_*_Lyra`·무기 서브 ABP·몽타주를 그대로 쓰고, GASP 신규 클립(경사·전환 등)은 리타깃해 들여온다.
   - UEFN 전환: GASP 클립을 그대로 쓰고(`GASP_Mannequin/MotionMatchingData`에 UEFN PSD가 이미 있다), 기존 Lyra 기반 무기·몽타주 세트를 리타깃한다.
   - 판단 근거로 리타깃 대상 에셋 수(양쪽 방향), `VisualMesh`가 `AnimSourceMesh` 포즈를 받는 현 구조와의 관계를 함께 제시한다.
2. **마이그레이트 규칙(AN-001 세부).**
   - 마이그레이트 대상 폴더와 `.gitignore` 패턴(현 `Content/Characters/GASP_Mannequin/*` 확장 여부).
   - 시퀀스가 끌고 오는 노티파이·모디파이어 BP(`BP_AnimNotify_FoleyEvent*`, `AM_*` 등) 처리: 제거 / 재구현 / 그대로 제외 폴더에.
   - 추적되는 구성 에셋이 제외된 시퀀스를 참조하므로, 새로 클론한 환경에서 소스 에셋을 확보하는 절차(GASP 5.8 Fab 버전, 경로)를 `README.md` §4에 한 단락 남긴다.
3. AN-010(마커리스 소스 영상 정책)을 사용자에게 확인한다.

완료 조건: AN-011 확정, `Decisions.md`에 마이그레이트 규칙 반영, AN-010 상태 갱신.

## 완료 조건 체크리스트

- [x] 0-1: `design/GASPReference.md` §1 읽기 방법 (2026-10-18, 포즈 연결 읽힘 — 바인딩·전이 연결·Chooser 행 결과는 사용자 자료 필요)
- [x] 0-2: `design/GASPReference.md` §2 블록 표, Phase 2 순서 확정 (2026-10-18, 블록 B0~B12, MM 노드 방식 AN-015, Phase 2 6단위)
- [x] 0-3: AN-011 스켈레톤 결정, 마이그레이트 규칙, 소스 에셋 확보 절차, AN-010 확인 (2026-10-18, UEFN 전환 AN-011·폴더/git 규칙 AN-016·노티파이 처리 AN-017·AN-010 확정, `README.md` §4 확보 절차)

## 다음 Phase 인계 항목

- Phase 1A·1B·1C는 이 Phase 뒤에 병렬로 시작할 수 있다. Phase 2는 1C가 끝난 뒤 2-0a(Lyra 세트 UEFN 리타깃)부터 시작한다. 각 갈래의 첫 행동과 동시 실행 제약은 `Current.md`에 있다.
- 0-3에서 정한 노티파이·모디파이어 규칙(AN-017)과 폴더·git 규칙(AN-016)은 1C-1~1C-3에 그대로 쓰인다(`phases/Phase01C_GASPAssetRefresh.md` "반입 목록과 경로 규칙").
