# 애니메이션 개선 현재 작업 상태

> 상태: 활성 인계 문서
> 현재 작업: Phase 0 단위 0-2(GASP Mover ABP 블록 목록) — 착수 전
> 마지막 갱신: 2026-10-18

## 현재 결정

코드·LootNPop 에셋 변경은 아직 없다. 사용자 결정:

- GASP는 구성을 보며 직접 다시 만들고, 소스 에셋만 마이그레이트·git 제외(AN-001)
- Locomotor 사족 드론은 최후순위 별도 트랙(AN-008)
- GASP 에디터는 MCP 포트 8001로 동시에 띄워 읽기 전용 참조(AN-009)
- 옛 GASP 유래 소스 에셋은 중복이어도 5.8 최신본으로 교체 — Phase 1C 추가(AN-012)

결정 대기: 기준 스켈레톤(AN-011), 마커리스 소스 영상 정책(AN-010) — 단위 0-3에서 묻는다. 최신화 방식(AN-013 제안)과 LootNPop 산출물 추적(AN-014)은 Phase 1C 단위 1C-1에서 묻는다.

⚠️ `GASP_Mannequin/Animations/Enemy/*`(LootNPop이 만든 리타깃 적 애니 14개)는 `.gitignore`에 걸려 저장소에 없다. Phase 1C 전에 로컬 파일을 지우거나 덮어쓰지 않는다(`research/UE58_AnimationFeatures.md` §6).

**MCP 읽기 범위(0-1 결과, 상세 `design/GASPReference.md` §1).** AnimGraph·레이어 그래프의 포즈 연결, 노드 디테일, 함수·이벤트 그래프(DSL), Control Rig, PSS, PhysicsControl 프로필은 MCP로 읽힌다. 다음 세 가지는 MCP로 안 읽혀 0-2에서 사용자 자료가 필요하다.

1. 핀 프로퍼티 바인딩(바인딩된 핀은 "연결 없음 + 기본값"으로 보인다) — 핵심 노드 Ctrl+C 텍스트 또는 디테일 패널 스크린샷
2. 상태 머신 `State Controller`·`Ragdoll`의 전이 연결 — 그래프 스크린샷
3. Chooser `CHT_MoverCharacterAnimations_PoseMatch`의 행별 결과 — Chooser 에디터 스크린샷

⚠️ GASP의 `SandboxCharacter_Mover_ABP`가 dirty로 보인다(원인 미확인). GASP 에디터를 닫을 때 저장하지 않는다.

출발점(상세는 [TechDesign_CombatAnimation.md](../TechDesign_CombatAnimation.md)):

- 메인 ABP는 `ABP_Lyra`(모션매칭 + Mover 궤적 예측 + 무기별 Linked Layer + 슬롯 3종). `ActorPromoted` 적이 같은 ABP를 공유한다.
- 공중은 `Fall_Loop` 고정(§5.5), 히트리액트는 GameplayCue로 재생하는 코스메틱 몽타주(§6.4).
- 랙돌·포즈 표시는 `VisualMesh`, ABP 구동은 `AnimSourceMesh`.
- `.mcp.json`에 `unreal-mcp-gasp`(8001) 항목이 있다(로컬 파일, git 제외). 8000은 LootNPop, 8001은 GASP.

## 바로 다음 행동

- **새 세션**에서 [Phase 0](phases/Phase00_ReferenceSetup.md) 단위 0-2를 시작한다. 첫 조회 전에 GASP `SandboxCharacter_Mover_ABP`의 `AssetTools.is_dirty`를 먼저 잰다. 그다음 `design/GASPReference.md` §1.3 포즈 체인을 출발점으로 블록을 나누고, 블록마다 레이어·함수 그래프를 `get_node_infos`/`read_graph_dsl`로 좁혀 읽는다.
- 위 1~3의 사용자 자료는 블록 목록 초안이 나온 뒤 필요한 노드만 골라 한 번에 요청한다(바인딩은 블록별 핵심 노드, 상태 머신 2개, Chooser 1개 + 중첩 테이블).

## 인계 근거

- MCP 읽기 방법·함정: `design/GASPReference.md` §1
- 0-1 수행 기록: `history/Phase00_Log.md`
- 엔진·GASP 사실 조사: `research/UE58_AnimationFeatures.md`
- GASP 로컬 경로: `D:/UnrealProjects/GameAnimationSample`
