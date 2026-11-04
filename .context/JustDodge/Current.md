# 저스트 회피 현재 작업 상태

> 상태: 활성 인계 문서
> 현재 작업: 메인 트랙 Phase 4 피격 유예 버퍼(J-010 대응) + 문서 마감 — 단위 1 진행 중
> 마지막 갱신: 2026-11-04

## 현재 결정

사용자가 정한 것:

- 저스트 회피는 근접 공격과 투사체 직격에만 발동한다(J-002).
- 무적 중인 플레이어를 지나는 투사체는 관통한다(J-003).
- 반경은 기존 패링 반경을 쓴다(J-004).
- 버프는 AttackSpeed·MoveSpeed 곱연산 +20%, 2.5초, 시간만 갱신한다(J-005). 사용자가 천천히 상향을 검토한다.
- 연출은 서버 확정 뒤에만 낸다(J-007 확정, 2026-11-04).
- 화상 지속 틱은 무적으로 막지 않는다(J-013). PvP 근접 예측 HitStop 어긋남은 허용한다(J-014).
- 비주얼은 마지막 독립 트랙으로 둔다(J-012).

Phase 1~3은 끝났다(Phase 3은 2026-11-04). 결과는 `history/Phase0N_Log.md`에 있다. Phase 4가 올라설 토대는 다음과 같다.

- 판정: `ExecuteDash`가 무적·저스트 창을 `FLNPParryStateFragment`에 기록한다. `Judge` 순서는 Parried → JustDodged → (반경 밖 None) → Dodged → Guarded → Hit이다. 성공은 `FLNPJustDodgeCommand`가 버프 GE와 `.Success` 큐로 처리한다.
- 수치(1차 확정, 현행 유지): Mover 컴포넌트의 `IFrameDuration` 0.15·`JustDodgeWindow` 0.12, 프로젝트 설정 `Combat|Just Dodge`(2.5초, 0.2, 0.2).
- 계측: `LNP.Debug.Dodge 1`이면 `[Dodge] Hit ... sinceJust=`(놓친 회피)와 `[Dodge] Success cue arrived ... local=`(연출 지연)가 찍힌다. 성공 큐 에셋의 부모는 `ULNPGameplayCueNotify_JustDodgeSuccess`다(V1이 확장한다).
- 집계: `Scripts/Profiling/AnalyzeDodgeLog.py <로그> --since HH:MM:SS [--until HH:MM:SS]`. 한 PIE 세션 안의 지연 구간은 로그의 `LogNet: PktLag set to N` 줄로 나눈다.

## Phase 4가 고칠 문제

지연이 있으면 게스트가 맞기 직전에 누른 대시를 서버가 거부한다. RTT +50에서 38번 중 10번, +100에서 19번 중 4번이었다. 서버가 피격 넉백(이동 모드를 공중으로 강제)을 먼저 처리하면, `CanDash()`의 `IsOnGround()` 조건 때문에 뒤늦게 도착한 대시 입력이 실행되지 않는다. 게스트는 맞고 대시까지 잃는다(`design/Architecture.md` §6.1).

대응은 §6.2 A 피격 유예 버퍼다. 원격 클라이언트 피격자의 효과를 짧게 보류하고, 그 사이 도착한 대시가 먼저 눌렸다고 추정되면 `Dodged`·`JustDodged`로 소급한다.

## 바로 다음 행동

- 실행서 `phases/Phase04_HitGraceBuffer.md` 초안을 썼다(2026-11-04, 코드 위치 재확인 포함). 사용자 결정 1~4는 모두 권장안으로 확정됐다(창 대칭·지연 0 게스트에도 적용·보류된 피격만 소급·화염 포함). 단위 1부터 진행한다.
- 단위 1·2 완료·커밋(7aafd4a). 게스트 `H` 110ms(실효 p50 141ms, 프레임 양자화), 호스트 0, `Enable 0` 회귀 없음(`history/Phase04_Log.md`).
- 단위 3 완료(2026-11-04): PktLag 25에서 소급 89건, Phase 3 유형 거부 0. 남은 거부 6건은 모두 "서버 피격 뒤에 누른" 경우다(호스트도 잃는다). 누름 추정은 +13~+25ms 이르다(`HitGraceInputAllowance` 0.06 과대). **다음: 단위 4 — 집계 스크립트 확장(누름 추정 오차·Retro·거부 유형 분리), allowance를 0.045 안팎으로 낮춘 뒤 R1~R3 재측정.**
- 초안의 핵심 변경: 누름 추정 `U`와 보류 길이 `H`를 나눴다(§6.2 초안의 `T_press = E − H`는 보류 여유만큼 과하게 소급한다). `H`는 Game Thread가 `FLNPParryStateFragment::HitGraceSeconds`로 미러링해 판정 프로세서가 UObject를 읽지 않는다.
- 완료 기준: Phase 3 R2·R3 조건 재측정에서 "누름 전 서버 피격" 유형의 거부가 0에 가깝고, 호스트 동작이 바뀌지 않으며, 지연 0 게스트의 거부가 0으로 유지된다.
- Phase 4 끝에 Phase 3에서 옮겨 온 문서 마감을 한다(`Roadmap.md` §4 Phase 4).

## 인계 근거

- 검증 절차와 시나리오: `design/Validation.md`. PvP 경로는 `LNP.Team.Set 1 2`를 빼먹지 않는다(§6).
- 지연 에뮬레이션: `net.PktLag=N`은 입력한 창의 송신에만 걸린다. 호스트·게스트 두 창 모두에 넣어야 RTT가 약 +2N이 된다.
- 단일 프로세스 PIE 로그에서는 폰 이름이 월드마다 다르거나 겹친다(리스폰 뒤 서버 `_C_2`·게스트 `_C_10`, 게스트 월드의 자기 폰 `_C_0`이 서버의 호스트 폰과 같은 이름). 집계 스크립트는 시각과 역할로 짝짓는다.
- 빌드·PIE 검증 방법: 프로젝트 공통 워크플로(Build.bat, Live Coding, Unreal MCP)를 따른다. 구조체 레이아웃·새 `.cpp`·새 CVar가 있으면 에디터를 닫고 풀 빌드한다. 에디터가 떠 있으면 게임 타깃 빌드로 컴파일만 확인할 수 있다. 커밋한 뒤에는 깨끗한 트리에서 Build.bat을 다시 돌려 확인한다.
