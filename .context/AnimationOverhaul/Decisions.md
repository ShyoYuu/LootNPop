# 애니메이션 개선 확정 결정

> 상태: 확정 결정 원장
> 마지막 갱신: 2026-10-16
> 규칙: 결론만 기록하고 상세 근거와 구현은 링크된 문서가 소유한다. ID 접두어 `AN-`은 다른 작업 폴더(`D-`·`L-`)와 구분하기 위한 것이다.

## 결정 목록

| ID | 결정 | 상태 | 상세 근거 |
|:---|:---|:---|:---|
| AN-001 | GASP 시스템은 에셋을 통째로 마이그레이트하지 않고 **구성을 보며 핵심 블록부터 LootNPop에서 직접 다시 만든다.** 애니메이션 시퀀스·스켈레탈 메시 같은 소스 에셋만 마이그레이트하고 git에서 제외한다(기존 `Content/Characters/GASP_Mannequin/*` 패턴). ABP·Chooser·PSD·PSS·Control Rig·PhysicsControl 프로필 같은 구성 에셋은 직접 만들고 추적한다. | 확정(2026-10-16, 사용자 결정) | `Roadmap.md` §1 |
| AN-002 | GASP의 이동 모드 BP(`BP_MovementMode_*`)와 BPI 인터페이스(`BPI_SandboxCharacter_*`)는 가져오지 않는다. 이동은 기존 C++ Mover 모드가 소유하고, 애니 변수는 `ULNPAnimInstance`(C++)가 접평면 투영 값으로 공급한다. | 확정 | [TechDesign_CombatAnimation.md](../TechDesign_CombatAnimation.md) §7.4 |
| AN-003 | 상태별 애니메이션 선택 로직은 ABP 그래프가 아니라 **Chooser 테이블과 C++**에 둔다. PSD·Chooser는 UAF(`UAFChooser`·`UAFPoseSearch`)에서도 재사용되므로 이후 UAF 이전 비용을 그래프 배선 수준으로 줄인다. | 확정 | `research/UE58_AnimationFeatures.md` §1 |
| AN-004 | 히트리액트는 지금처럼 **코스메틱**이다(GameplayCue 수신 시 머신별 로컬 재생, 비복제, 데디 서버 생략). 물리 히트리액트는 `VisualMesh`의 상체 바디만 구동하고 캡슐·Mover·판정은 건드리지 않는다. 행동을 끊는 경직(GA_Stagger)은 몽타주 경로를 유지한다. | 확정 | [TechDesign_CombatAnimation.md](../TechDesign_CombatAnimation.md) §6.4·§6.5 |
| AN-005 | GASP식 파워드 래그돌 넘어짐·기상(캡슐이 물리를 따라가는 연출)은 이 작업 범위 밖이다. 서버 권위 위치·리컨실리에이션 문제를 낳으므로 필요해지면 별도 결정으로 다룬다. | 확정 | `Roadmap.md` §5 |
| AN-006 | UAF는 **판정 게이트**다. Phase 5 스파이크를 통과하기 전에는 마이그레이션에 착수하지 않는다. 불합격이면 사유를 [DiscardedApproaches.md](../DiscardedApproaches.md)에 남기고 다음 엔진 버전에서 다시 본다. | 확정 | `Roadmap.md` Phase 5 |
| AN-007 | 공중 `Fall_Loop` 고정([TechDesign_CombatAnimation.md](../TechDesign_CombatAnimation.md) §5.5)은 유지한다. 수직 채널을 가진 공중 스키마를 따로 만들기 전에는 GASP 점프·낙하 처리로 바꾸지 않는다. | 확정 | 같은 문서 §5.5 |
| AN-008 | Locomotor 사족 드론은 **최후순위 별도 트랙**이다. 이동 외 전투 모션이 GASP에 없어 전투 참여 방안이 서기 전에는 이식하지 않는다. 검증·이식 모두 본 트랙과 독립적으로 진행한다. | 확정(2026-10-16, 사용자 결정) | `Roadmap.md` 트랙 L |
| AN-009 | GASP 에디터는 MCP `unreal-mcp-gasp`(포트 8001)로 LootNPop 에디터와 동시에 띄워 **읽기 전용**으로 참조한다. GASP 쪽에서는 저장·수정 툴을 쓰지 않는다. | 확정(2026-10-16, 사용자 결정) | `README.md` §4 |
| AN-010 | 마커리스 모캡에 쓰는 다른 게임 영상은 프로토타입·레퍼런스로 한정하고, 최종 출시 에셋은 직접 촬영한 영상으로 같은 파이프라인을 돌린다(타사 모션 작업물 권리 문제). | 제안(사용자 확인 필요) | `research/UE58_AnimationFeatures.md` §4 |
| AN-011 | 로코모션 기준 스켈레톤(현행 Lyra PSD 유지 vs GASP UEFN 스켈레톤 전환)은 Phase 0에서 사용자가 고른다. 정해지기 전에는 Phase 2에 착수하지 않는다. | 결정 대기 | `phases/Phase00_ReferenceSetup.md` 단위 0-3 |

## 변경 규칙

- 확정 결정을 바꿀 때 기존 행을 조용히 덮어쓰지 않는다. 상태를 `대체됨`으로 바꾸고 새 ID를 추가한다.
- 변경 이유와 영향을 관련 설계 문서와 현재 Phase 로그에 기록한다.
- 단순 구현 선택이나 임시 디버그 값은 이 원장에 넣지 않는다.
