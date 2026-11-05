# 엘리트 NPC 확정 결정

> 상태: 확정 결정 원장
> 마지막 갱신: 2026-11-05
> 규칙: 결론만 기록하고, 상세 근거와 구현은 링크된 문서가 소유한다. ID 접두어 `EL-`는 다른 작업 폴더의 `D-`·`J-`·`AN-`와 구분하기 위한 것이다.

## 결정 목록

| ID | 결정 | 상태 | 상세 근거 |
|:---|:---|:---|:---|
| EL-001 | 두뇌를 둘로 나눈다. 엘리트 전용 Mass StateTree(`ST_EnemyUnbound`)가 매크로(대기·배회·귀환·교전 여부)를, Actor의 StateTreeComponent(`CombatBrain`)가 전투 패턴(선택·연계·페이즈)을 맡는다. BehaviorTree는 쓰지 않는다 | 확정(2026-11-05, 사용자) | `design/Architecture.md` §1, §7 |
| EL-002 | LowLOD §2 등급 어휘 규약을 유지한다. 코드 축은 "귀속 방식" `ELNPEnemyHomeMode { Pod, Arena, Roam }`이다. "엘리트"는 기획 문서와 에셋 표시명에서만 쓴다 | 확정(2026-11-05, 사용자) | [LowLOD §2](../TechDesign_EnemyNPC_LowLOD.md), `design/Architecture.md` §2 |
| EL-003 | Pod 편성(`AssociatedEnemies`)의 적은 PureEntity여야 한다. ActorPromoted가 들어가면 `IsDataValid` 오류다 | 확정(2026-11-05, 사용자 요청의 규칙화) | `design/Architecture.md` §2.2 |
| EL-004 | 이동 의도 작성자는 한 명이다. `FLNPEnemyActorBrainTag`가 붙어 있는 동안 `ULNPEnemyMovementProcessor`는 Actor 의도를 쓰지 않고 전투 두뇌만 쓴다 | 기준 설계 | `design/Architecture.md` §6.3 |
| EL-005 | 비귀속 배치는 Pod 배치 뒤 별도 시드 스트림(`WorldSeed ^ 새 salt`)으로 한다. 엘리트 엔트리를 추가해도 Pod·Pod 적 배치는 같은 시드에서 동일하다 | 기준 설계 | `design/Architecture.md` §4 |
| EL-006 | 아레나 이탈 시 귀환 + 완전 리셋(HP·경직·페이즈). 엘리트는 세션당 1회만 등장하고 재등장하지 않는다 | 확정(2026-11-05, 사용자) | `design/GameRules.md` §2, §3 |
| EL-007 | 엘리트 상태의 원본은 엔티티다. 페이즈는 HP 비율에서 파생한다. Actor 재생성 때 패턴 쿨다운이 초기화되는 것은 허용한다 | 기준 설계 | `design/Architecture.md` §7.5 |
| EL-008 | 아레나 엘리트의 추격 자격은 "플레이어가 아레나 안에 있는가"(플레이어↔HomeCenter ≤ LeashRadius)다. Roam은 앵커 없이 시야·인지·인내 규칙만 쓴다 | 기준 설계 | `design/Architecture.md` §5, `design/GameRules.md` §2 |
| EL-009 | 사망 보상은 `ULNPEnemyConfig::DeathRewards`(`FLNPLootDiceRewardSet`, 비어 있으면 드랍 없음)에 둔다. 등급 어휘가 없는 범용 필드다. 추첨·스폰은 Pod 보상과 같은 `ALNPLootDice::SpawnRewardSet`을 쓴다 | 기준 설계 | `design/Architecture.md` §8 |
| EL-010 | 아레나 위치는 에디터 전용 마커 `ALNPArenaSpawnPoint`를 Spawn 스트림에 베이크해서 얻는다. 런타임은 레벨 Actor를 스캔하지 않는다(D-059 원칙 유지). Spawn codec 3→4 | 기준 설계 | `design/Architecture.md` §3 |

## BT를 쓰지 않는 이유 (EL-001 근거 요약)

- BT의 이동·탐색 노드(MoveTo, EQS)는 NavMesh 기반이다. 구면 맵에서는 NavMesh를 기각했다([DiscardedApproaches.md](../DiscardedApproaches.md) Case 02). 결국 이동 Task를 직접 만들어야 한다.
- BT를 쓰려면 AIController 빙의, Blackboard, Mass 타겟팅 → Blackboard 브리지가 필요하다. 지금 적 Pawn에는 컨트롤러가 없고 AI 입력은 `ULNPInputHandlerComponent`의 AI Intent로 들어간다.
- UE 5.8 StateTree는 패턴 AI에 필요한 기능(Utility Selection·Consideration, Linked Asset, 이벤트 전이, 전역 Task/Evaluator, 디버거)을 갖고 있다. 프로젝트의 Mass StateTree 지식도 이어진다.
- Mass StateTree 하나로 패턴까지 넣지 않는 이유: Task Tick이 신호 구동이라 매 프레임 신호를 보내야 하고, GAS 어빌리티 종료 이벤트와 연동하기가 번거롭다([TechDesign_EnemyNPC_StateTree.md](../TechDesign_EnemyNPC_StateTree.md) §1).

## 변경 규칙

- 확정 결정을 바꿀 때 기존 행을 조용히 덮어쓰지 않는다.
- 기존 행의 상태를 `대체됨`으로 바꾸고 새 ID를 추가한다.
- 변경 이유와 영향은 관련 설계 문서와 현재 Phase 로그에 기록한다.
- 단순한 구현 선택이나 임시 디버그 값은 이 원장에 넣지 않는다.
