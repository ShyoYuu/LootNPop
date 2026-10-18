# 애니메이션 개선 현재 작업 상태

> 상태: 활성 인계 문서
> 현재 작업: Phase 0 참조 환경과 기준 결정 — 착수 전
> 마지막 갱신: 2026-10-16

## 현재 결정

**작업 계획 수립(2026-10-16).** 코드·에셋 변경은 아직 없다. 사용자 결정:

- GASP는 구성을 보며 직접 다시 만들고, 소스 에셋만 마이그레이트·git 제외(AN-001)
- Locomotor 사족 드론은 최후순위 별도 트랙(AN-008)
- GASP 에디터는 MCP 포트 8001로 동시에 띄워 읽기 전용 참조(AN-009)
- 옛 GASP 유래 소스 에셋은 중복이어도 5.8 최신본으로 교체 — Phase 1C 추가(AN-012, 2026-10-18)

결정 대기: 기준 스켈레톤(AN-011), 마커리스 소스 영상 정책(AN-010) — Phase 0 단위 0-3에서 묻는다. 최신화 방식(AN-013 제안)과 LootNPop 산출물 추적(AN-014)은 Phase 1C 단위 1C-1에서 묻는다.

⚠️ `GASP_Mannequin/Animations/Enemy/*`(LootNPop이 만든 리타깃 적 애니 14개)는 `.gitignore`에 걸려 저장소에 없다. Phase 1C 전에 로컬 파일을 지우거나 덮어쓰지 않는다(`research/UE58_AnimationFeatures.md` §6).

출발점(상세는 [TechDesign_CombatAnimation.md](../TechDesign_CombatAnimation.md)):

- 메인 ABP는 `ABP_Lyra`(모션매칭 + Mover 궤적 예측 + 무기별 Linked Layer + 슬롯 3종). `ActorPromoted` 적이 같은 ABP를 공유한다.
- 공중은 `Fall_Loop` 고정(§5.5), 히트리액트는 GameplayCue로 재생하는 코스메틱 몽타주(§6.4).
- 랙돌·포즈 표시는 `VisualMesh`, ABP 구동은 `AnimSourceMesh`.
- `.mcp.json`에 `unreal-mcp-gasp`(8001) 항목이 있다(로컬 파일, git 제외). 2026-10-18 두 에디터 동시 연결 확인 — 8000은 LootNPop, 8001은 GASP.

## 바로 다음 행동

- 사용자가 GASP 에디터를 포트 8001로 띄운 뒤 **새 세션**에서 [Phase 0](phases/Phase00_ReferenceSetup.md) 단위 0-1(MCP 읽기 범위 확인)부터 시작한다. `history/Phase00_Log.md`를 만들고 거기에 기록한다.

## 인계 근거

- 엔진·GASP 사실 조사: `research/UE58_AnimationFeatures.md`
- GASP 로컬 경로: `D:/UnrealProjects/GameAnimationSample`
