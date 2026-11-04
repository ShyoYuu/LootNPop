# Phase 3 로그 — 수치 1차 튜닝·지연 측정·문서 마감

> 실행서: `phases/Phase03_TuningLatency.md`

## 2026-11-04 — 실행서 확정, 단위 1 계측

사용자가 실행서 초안을 확인하고 단위 1부터 시작했다.

### 변경

- `LNPGuardParry::ReportDodgeHit` — 근접·투사체 직격의 `Hit` 분기에서 `[Dodge] Hit path=... player=... sinceJust=<ms|->`. Worker Thread에서 로그만 남긴다.
- `ULNPGameplayCueNotify_JustDodgeSuccess`(새 `.cpp`, 부모 `ULNPGameplayCueNotify_VFXSound`) — `[Dodge] Success cue arrived player=... local=<0|1> role=...` 후 `Super`. `GCN_LNP_JustDodge_Success`의 부모를 MCP `BlueprintTools.set_parent`로 바꿨다. 레지스트리 `GameplayCueName`은 `GameplayCue.LNP.JustDodge.Success` 그대로다.
- `Scripts/Profiling/AnalyzeDodgeLog.py` — 서버 대시 단위로 success / late / early를 분류하고, 놓침 분포·업링크·연출 지연을 출력한다.
- `design/Architecture.md` §7, `design/Validation.md` §6에 새 문자열 2종.

### 실수와 수정

- `if (...) UE_LOG(...); else UE_LOG(...);`가 `C2181 illegal else`로 실패했다. `UE_LOG`가 블록과 `;`로 펼쳐지기 때문이다. 중괄호로 감쌌다.

### 스크립트를 Phase 2 로그로 시험하며 발견한 것

- **리스폰한 게스트 폰은 서버·게스트 월드에서 이름이 다르다**(서버 `BP_LNPPlayer_C_2`, 게스트 `BP_LNPPlayer_C_10`). 리스폰 전에는 `_C_1`로 같았다. 그래서 Autonomous 줄과 큐 도착 줄은 이름이 아니라 시각으로 서버 폰과 짝짓는다(`map_names`). 시험 로그에서 `_C_10 -> _C_2`로 맞게 짝지어졌다.
- **지연 없는 기준선의 업링크(Autonomous 대시 → 서버 대시):** n=55, min 23 / p50 57 / p90 62 / max 72ms(Phase 2 2인 로그, 2026-11-03 13:51 이후). 입력 버퍼와 프레임 지연이 포함된 값이다. R2·R3는 이 값과 비교한다.

### 검증 (1인 PIE, TestMap03)

빌드: 게임 타깃 통과 → 에디터 풀 빌드 통과.

- 새 문자열 두 종이 문서대로 찍힌다. 호스트 큐는 `local=1 role=ROLE_Authority`이고, 서버 성공과 같은 프레임에 도착한다(server→cue 0ms, 대시→큐 p50 27ms, max 51ms).
- 집계: 대시 60, 성공 7(Projectile 3, Melee 4), 늦은 놓침 7(피격 → 대시 159~296ms), 이른 놓침 1(55ms). 피격 141줄(Projectile 134).
- **다탄 무기는 같은 프레임에 `Hit` 줄이 여러 개 찍힌다**(같은 `sinceJust`로 7줄). 스크립트는 대시당 첫 피격만 쓰므로 집계에는 영향이 없다.
- **이른 놓침 상한을 스크립트에 추가했다.** 처음 집계에서 이른 놓침이 최대 904ms까지 잡혔다. 1초 전 대시는 저스트 회피 시도로 보기 어려우므로 `--early-ms`(기본 300, 늦은 쪽과 대칭)로 자른다. 로그는 1초까지 숫자를 남겨 두어, 상한을 바꿔 다시 집계할 수 있다.
- ensure·AbilitySystem 경고 없음. 에러 줄은 MCP 재접속과 기존 `GameplayCueNotifyPaths` 경고뿐이다.

단위 1 완료. 게스트 쪽 큐 도착(`local=1 role=ROLE_AutonomousProxy`)과 업링크는 단위 2의 2인 회차에서 처음 나온다.
