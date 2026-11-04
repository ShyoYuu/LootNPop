# Phase 4 — 피격 유예 버퍼(J-010 대응) + 문서 마감

> 상태: 진행 중 (2026-11-04) — 단위 1·2 풀 빌드·2인 PIE 로그 검증 통과(사용자 체감 확인 대기), 다음은 단위 3
> 선행: Phase 3 수치 1차 튜닝·지연 측정 (완료)
> 순서: 단위 1 → 2 → 3 → 4 → 5. 각 단위를 끝내고 검증한 뒤 사용자에게 진행 여부를 확인받는다.
> 설계 원본: `design/Architecture.md` §6.2 A. 이 문서는 실행 순서와 이번에 정할 수치·형태를 소유한다.

## 배경

지연이 있으면 서버가 게스트 피격(넉백)을 먼저 처리하고, 그 뒤 도착한 대시 입력을 `CanDash()`의 `IsOnGround()`가 거부한다. RTT +50에서 38번 중 10번, +100에서 19번 중 4번이었다(`design/Architecture.md` §6.1). 게스트는 맞고 대시까지 잃는다.

이 Phase는 원격 클라이언트 피격자의 효과를 짧게 보류한다. 그 사이 대시가 도착하고, 그 대시가 피격보다 먼저 눌렸다고 추정되면 피격을 `Dodged`·`JustDodged`로 바꾼다.

## 착수 전 코드 확인 (2026-11-04)

| 항목 | 위치 | 결론 |
|:---|:---|:---|
| 근접 플레이어 분기 | `HitDetection/LNPWeaponTraceProcessors.cpp:587-641` | Guarded(`:619`) = 경직×가드배율 + `FLNPGuardBlockCommand` + 적 HitStop. Hit(`:628`) = `ReportDodgeHit` + 경직 + `FLNPImpactCueCommand` + 적 HitStop + `FLNPApplyDamageGECommand`. `MarkHit`은 두 분기 모두 앞에서 한다 |
| 투사체 직격 | `HitDetection/LNPProjectileProcessors.cpp:1150-1167` | Guarded·Hit 효과 뒤 `FinishHit`이 임팩트 큐(Ghost 토큰 포함)·소멸·스플래시를 낸다 |
| 스플래시 | `LNPProjectileProcessors.cpp:739-764` | Guarded = 경직 + 가드 큐, Hit = 경직 + 데미지 GE(넉백 있음) |
| 화염 원뿔 | `HitDetection/LNPFlameConeProcessors.cpp:344-372` | Guarded = 경직, Hit = 경직 + 데미지 GE(넉백 0, HitReact 없음). 화상 스택은 가드와 무관하게 확률로 붙는다 |
| 경직 누적 | `GAS/LNPPoiseTypes.h:151-162` | 4경로 모두 Worker Thread에서 프래그먼트에 직접 더한다. 임계 처리는 `ULNPPoiseProcessor`가 다음에 한다 |
| 넉백 | `FLNPApplyDamageGECommand::Run` (`LNPHitDetectionShared.h:740-811`) | 데미지 GE → HitReact 큐 → `ApplyKnockback`. 6.1의 거부 원인이 이 마지막 줄이다 |
| 대시 거부 조건 | `Movement/LNPCharacterMoverComponent.cpp:118-123` | `IsOnGround() && !IsADS() && 쿨다운 없음` |
| 창 기록 | `LNPCharacterMoverComponent.cpp:371-388` | 비리시뮬레이션 분기, 권위·오토노머스. 소급 호출을 붙일 자리 |
| 성공 커맨드 | `LNPHitDetectionShared.h:526-580` | 소비 플래그 확인 → 버프 GE → `.Success` 큐 → 로그. 소급 저스트 회피도 이 처리를 그대로 쓴다 |
| 플레이어 수집 | 근접 `:417-450`, 투사체 `:579-620` | Worker Thread. 투사체 패링 분기가 이미 Worker에서 `PlayerState` 핑을 읽지만(`:1110-1118`), 새 코드는 이 선례를 따르지 않는다(아래 단위 1) |
| 거부 집계 | `Scripts/Profiling/AnalyzeDodgeLog.py:206-220` | `dropped by server` 줄 |

## 목표와 범위

범위:

- 피격 유예 서브시스템, 4경로의 효과 묶음화, 경직 누적의 Game Thread 경로, `ExecuteDash`의 소급 호출
- 유예 길이·누름 시각 추정의 계측과 보정
- R1(지연 없음)·R2·R3 재측정
- Phase 3에서 옮겨 온 공통 문서 마감

범위 밖:

- B(넉백 중 대시 허용) — 채택하지 않았다(`design/Architecture.md` §6.2 B).
- 적 피격자와 호스트 피격자. 지금처럼 즉시 처리한다.
- 패링(이미 RTT/2 역보정이 있다)과 화상 지속 틱(J-013).
- "아슬아슬하게 빗나간"(패링 반경 안·피격 반경 밖) 공격의 소급 저스트 회피. `Judge`가 `None`을 낸 공격은 보류할 효과가 없어 기록되지 않는다(결정 3).

## 용어

| 기호 | 뜻 |
|:---|:---|
| `E` | 서버가 그 게스트의 대시를 실행한 시각(`World->GetTimeSeconds()`) |
| `U` | 업링크 추정 — 게스트가 누른 순간부터 서버 실행까지. `PlayerState` RTT/2 + 입력 버퍼 여유 |
| `T_press` | 누름 추정 시각 `E − U` |
| `H` | 유예 길이 `min(U + 지터 여유, 상한)`. 피격 효과를 이만큼 보류한다 |
| `t_h` | 보류된 피격의 서버 판정 시각 |

`U`와 `H`를 나눈다. `Architecture.md` §6.2 초안은 `T_press = E − H`였는데, 그러면 보류 여유만큼 누름을 더 이르게 추정해 "맞은 뒤 누른 대시"까지 소급된다. 누름 추정에는 `U`만 쓰고, `H`는 대시가 늦게 와도 아직 보류 중이게 하는 길이로만 쓴다.

## 사용자 결정 (2026-11-04 확정 — 네 건 모두 권장안)

결과: 1 대칭, 2 지연 0 게스트에도 적용, 3 보류된 피격만 소급, 4 화염 포함.

| # | 질문 | 선택지 | 권장 |
|:---|:---|:---|:---|
| 1 | 게스트 창 길이를 호스트와 같게 할까 | **(a) 대칭:** 원격 게스트의 서버 기록 만료를 `T_press` 기준으로 당긴다(`DodgeIFrameExpiryTime = E − U + IFrameDuration`). 소급 구간 `[T_press, E)` + 실시간 구간 `[E, T_press + IFrame]`을 합쳐 길이가 호스트와 같다. / **(b) 관대:** 서버 창은 지금처럼 `E`부터 연다. 게스트의 실효 무적이 `IFrame + U`(지연 없이도 약 0.21초, RTT +100에서 약 0.29초)로 늘어난다 | (a). (b)는 지연이 클수록 게스트가 유리해진다 |
| 2 | 지연 없는 게스트에도 유예를 적용할까 | 지연이 없어도 업링크는 p50 약 70ms(입력 버퍼)다. 적용하면 "누른 뒤 70ms 안에 맞은" 지연 0 게스트의 늦은 놓침도 회피로 바뀐다. 호스트(누르면 바로 실행)와 같은 조건이 되는 것이다 / 적용하지 않으려면 `U`에서 기준선을 빼야 하는데, 그러면 지연 0의 늦은 놓침이 그대로 남는다 | 적용. 완료 기준의 "지연 없는 회차 불변"을 **"호스트 동작 불변, 지연 0 게스트의 거부 0 유지"**로 바꾼다 |
| 3 | 소급 저스트 회피 범위 | 보류된 피격(`Hit`·`Guarded`이면서 근접·직격)만 / 아슬아슬 빗나감도 표식으로 남겨 소급 | 보류된 피격만. 결정 1(a)와 합치면 RTT가 크고 `U > JustDodgeWindow`(0.12초)일 때 게스트의 "빗나감 저스트 회피"는 실시간 창이 비어 사실상 없어진다. 재측정에서 성공률이 눈에 띄게 떨어지면 그때 표식을 추가한다 |
| 4 | 화염 원뿔도 보류할까 | 포함 / 제외 | 포함. 화염에는 넉백이 없어 대시를 거부하지 않지만, 빼면 지연 게스트만 "누른 직후의 화염 틱"을 맞는다. 4경로 대칭이 판정 한 곳(J-001)의 취지와 맞는다 |

## 이번에 정하는 형태 (권장 기본값, 결정에 따라 조정)

- **보류하는 것:** `Hit`·`Guarded` 결과의 피격자 쪽 효과 전부 — 경직 누적, 데미지 GE(HitReact 큐·넉백 포함), 가드 블록 큐, 근접 임팩트 큐(공격자 Actor HitStop 포함), 순수 엔티티 공격자 HitStop 신호, 화상 스택. 화상 확률은 판정 시점에 굴리고 결과만 보류한다.
- **`[Dodge] Hit` 로그는 판정 시점에 그대로 남긴다**(구현 중 변경, 2026-11-04). 집계의 늦은 놓침 짝짓기가 실제 피격 시각을 써야 하기 때문이다. 소급 회피된 피격은 뒤따르는 `Retro` 줄로 빼고 센다(단위 4).
- **즉시 하는 것:** 근접 `MarkHit`, 투사체 `FinishHit`(임팩트 큐·소멸·주변 스플래시). 투사체 임팩트 큐는 "그 자리에서 터졌다"는 사실이고 사수의 Ghost 정리 토큰을 싣고 있어 보류하지 않는다. 소급 회피된 탄은 관통하지 않는다(J-003의 예외, §6.2와 같음). 근접 `Dodged`가 원래 `MarkHit`을 하지 않는 규칙도 소급에서는 지키지 못한다 — 그 휘두르기는 이미 끝났다.
- **경직 `LastHitTime`:** 보류가 풀려 실제로 쌓이는 시각으로 둔다. 회복 유예가 보이는 피격 시점과 맞는다.
- **사망:** 치명타도 `H`만큼 늦게 적용된다(§6.2 비용).

## 단위 1 — 유예 길이 미러와 서브시스템 골격

판정 프로세서가 Mass 프래그먼트만 읽는다는 규약(README §6)을 지키기 위해, 피격자가 보류 대상인지와 `H`를 **Game Thread가 프래그먼트에 미러링**한다.

- `FLNPParryStateFragment`에 `float HitGraceSeconds = 0`을 추가한다. 0이면 즉시 처리다. 구조체 레이아웃이 바뀌므로 에디터를 닫고 풀 빌드한다.
- `ULNPHitGraceSubsystem`(`UTickableWorldSubsystem`, 서버 전용, `HitDetection/`)을 새로 만든다.
  - 매 틱: 서버 월드의 플레이어 폰마다 `H`를 계산해 프래그먼트에 쓴다. 로컬 조종 폰(호스트)은 0이다.
  - 매 틱: `t_h + H`가 지난 보류 묶음을 적용한다.
  - `ResolveDash(APawn*, double E)`: 단위 3이 채운다.
- `U`·`H` 계산은 `LNPHitGrace::EstimateUplink(const APawn*)` 하나에 둔다. 서브시스템과 `ExecuteDash`가 같은 함수를 쓴다.
- 설정(`ULNPSettings`, `Combat|Just Dodge`): `HitGraceInputAllowance`(초안 0.06초, 입력 버퍼 여유), `HitGraceJitterMargin`(초안 0.04초), `HitGraceMax`(초안 0.25초). 상한을 넘는 고지연에서는 `H = 상한`이고, 그 너머의 거부는 지금처럼 남는다.
- CVar `LNP.HitGrace.Enable [0|1]`(기본 1). 0이면 `HitGraceSeconds`를 0으로 써서 지금 동작으로 돌아간다. 한 PIE 세션 안에서 켜고 끄며 비교하는 용도다.

구현(2026-11-04): `HitDetection/LNPHitGraceSubsystem.h/.cpp`. 서브시스템 틱은 Mass 페이즈가 끝난 뒤(Tickable)라 프래그먼트를 Game Thread에서 써도 판정 프로세서와 겹치지 않는다.

완료 조건: 풀 빌드, 2인 PIE에서 `LNP.Debug.Dodge 1`일 때 게스트 폰의 `H`가 주기적으로 찍힌다(`[Dodge] Grace player=<폰> rtt=<ms> uplink=<ms> hold=<ms>`, 값이 바뀔 때만). 호스트 폰은 0이다.

## 단위 2 — 효과 묶음화와 4경로 연결

경로마다 "즉시 분기"와 "보류 분기"를 따로 쓰면 효과 목록이 두 벌이 된다. 그래서 효과를 **한 묶음 값**으로 모은 뒤, 한 곳이 즉시 실행할지 보류할지를 고른다.

- `FLNPPlayerHitEffects`(값 타입): 경직(원값·배율), 선택적 데미지 GE 엔트리, 임팩트 큐 엔트리, 가드 블록 엔트리, 공격자 엔티티 HitStop 핸들, 화상 엔트리, 로그 정보(경로, `sinceJust`).
- 엔트리 타입은 기존 커맨드의 `FEntry`를 그대로 쓴다. 각 커맨드의 `Run` 루프 본문을 `static void ApplyEntry(...)`로 꺼내 `Run`과 보류 해제가 같은 함수를 부른다. `FLNPApplyPlayerBurnGECommand`의 엔트리도 같은 방식으로 꺼낸다(지금은 `LNPFlameConeProcessors.cpp` 안에 있어 공용 헤더로 옮긴다).
- `LNPHitGrace::Dispatch(Ctx, PS, Poise, Victim, Defender, Path, bJustEligible, Effects, Now)`
  - `PS.HitGraceSeconds <= 0`: 지금과 똑같이 한다 — Worker에서 경직을 쌓고 각 커맨드를 `PushCommand`한다. **호스트·적 경로의 동작과 순서가 바뀌지 않아야 한다.**
  - 그 밖: `FLNPHoldPlayerHitCommand` 하나를 넣는다. Game Thread `Run`이 묶음을 서브시스템에 넘긴다.
- 4경로의 `Guarded`·`Hit` 분기를 묶음 채우기 + `Dispatch`로 바꾼다. `Parried`·`Dodged`·`JustDodged` 분기와 기하 판정은 건드리지 않는다.
- 해제: 서브시스템 틱이 `t_h + H`가 지난 묶음을 적용한다. 경직은 `EntityManager.GetFragmentDataPtr<FLNPPoiseFragment>(Defender)`에 `LNPPoise::Accumulate`. 엔티티가 사라졌으면(사망·리스폰) 버린다.

구현(2026-11-04):

- 효과 묶음 `FLNPPlayerHitEffects`·보류 묶음 `FLNPHeldPlayerHit`·`FLNPHoldPlayerHitCommand`·`LNPHitGrace::Dispatch`/`ApplyHeld`는 `LNPHitGraceSubsystem.h`에 있다.
- 커맨드 5종(가드 블록·임팩트 큐·엔티티 HitStop·데미지 GE·화상)의 `Run` 본문을 `ApplyEntry`(HitStop은 `Apply`)로 꺼냈고, 엔트리를 그대로 받는 `Add(const FEntry&)`를 더했다. 근접 임팩트 엔트리는 `FLNPImpactCueCommand::MakeMelee` 한 곳에서 만든다.
- `FLNPApplyPlayerBurnGECommand`는 `LNPFlameConeProcessors.cpp`에서 `LNPHitDetectionShared.h`로 옮겼다.
- 해제 로그: `[Dodge] Grace release path=<경로> player=<폰> held=<ms>`. 보류 중 리스폰(엔티티 비활성)·사망한 피격자의 묶음은 버린다.

완료 조건: 풀 빌드. `LNP.HitGrace.Enable 0`에서 1인·2인 PIE 동작이 Phase 3과 같다(근접·직격·스플래시·화염 피격, 가드, 경직 그로기). `Enable 1`에서 게스트 피격이 `H`만큼 늦게 적용되고 효과가 빠지지 않는다(넉백·HitReact·HP·경직·화상).

## 단위 3 — 소급 판정

- `ExecuteDash`의 창 기록 직후, 권위이고 원격 조종이며 `HitGraceSeconds > 0`이면 `ULNPHitGraceSubsystem::ResolveDash(Pawn, E)`를 부른다. 리시뮬레이션 게이트 뒤라 롤백마다 반복되지 않는다.
- 결정 1(a)면 같은 자리에서 기록 만료를 `E − U + 길이`로 당긴다(무적·저스트 둘 다, 저스트는 계속 무적 이하로 묶는다).
- 소급 술어는 `LNPGuardParry`에 둔다(J-001 — 판정 순서의 원본과 같은 파일).

```
LNPGuardParry::JudgeRetro(t_h, T_press, IFrame, JustWindow, bJustEligible, bConsumed)
  if t_h < T_press or t_h > T_press + IFrame → 그대로(Hit/Guarded)
  if bJustEligible and !bConsumed and t_h <= T_press + JustWindow → JustDodged
  → Dodged
```

- `ResolveDash`는 그 피격자의 보류 묶음을 시각순으로 돌며 술어를 적용한다.
  - `Dodged`: 묶음을 버린다. `ReportDodgeIgnored` 대신 `[Dodge] Retro Dodged path=<경로> player=<폰> hold=<E − t_h ms>`.
  - `JustDodged`: 묶음을 버리고 `FLNPJustDodgeCommand::ApplyEntry`(소비 플래그 → 버프 → 큐 → 로그)를 바로 부른다. 대시당 1회 규칙은 그 함수가 지킨다. 로그 `[Dodge] Retro JustDodged ...`.
  - 그대로: 남겨 둔다. 만료되면 단위 2의 해제가 적용한다.
- 가드 중 대시도 같은 규칙이다. `Judge` 순서에서 `Dodged`가 `Guarded`보다 앞이기 때문이다(J-006).

완료 조건: 2인 PIE `net.PktLag=25`(두 창)에서 게스트가 근접 공격 직전에 대시하면 `Retro` 줄이 나오고, 넉백·피해가 없으며, 대시가 실행된다. 저스트 회피면 성공 큐가 온다.

## 단위 4 — 계측과 재측정

### 4-1. 누름 추정 보정

단일 프로세스 PIE는 서버·게스트 줄이 같은 시계라 실제 누름 시각(게스트 `IFrame open role=Autonomous`)을 안다. 서버가 추정한 `T_press`를 로그에 남기고 둘을 비교한다.

- `[Dodge] IFrame open` 서버 줄에 `press=<T_press>`를 덧붙인다(원격 게스트만).
- `AnalyzeDodgeLog.py`에 표 두 개를 더한다.
  - **누름 추정 오차:** 추정 `T_press` − 실제 누름(ms) 분포. p50이 0 근처면 `HitGraceInputAllowance`가 맞다.
  - **소급 결과:** `Retro Dodged`·`Retro JustDodged` 수, `hold` 분포, 만료로 그대로 적용된 보류 수. `hold`가 `H`에 붙어 있으면 `H`가 짧다.
- `Retro` 회피는 `dropped`·놓침 집계에서 빼고 성공·회피로 센다.

### 4-2. 재측정 (요인설계, 리슨 2인 PIE)

Phase 3 R1~R3과 같은 편성과 행동이다(게스트 팀 2 `LNP.Team.Set 1 2`, `LNP.Debug.Dodge 1`, `LNP.Debug.ShowDodge 1`). 한 PIE 세션 안에서 `LNP.HitGrace.Enable`을 껐다 켜서 같은 회차에 기준과 비교한다. 구간은 `PktLag` 줄과 CVar 변경 시각으로 나눈다.

| 회차 | 지연(두 창) | 편성 | Enable 0 → 1 |
|:---|:---|:---|:---|
| R1 | 없음 | 기본 `DA_MassSpawnConfig` | 각 절반 |
| R2 | `net.PktLag=25` | `DA_MassSpawnConfig_TestOnePerSpecies` + `LNP.Spawn.ConfigOverride` | 각 절반 |
| R3 | `net.PktLag=50` | R2와 같음 | 각 절반 |

- 매회 사용자에게 받을 체감: 게스트가 맞는 순간이 늦게 느껴지는가(피해 지연 `H`), "피했는데 맞음"이 줄었는가.
- 수치 조정: `HitGraceInputAllowance`·`HitGraceJitterMargin`은 4-1 표를 보고 회차 사이에 바꾼다(프로젝트 설정, 다음 틱부터 반영).

완료 조건:

- `Enable 1`의 R2·R3에서 `dropped by server`가 0에 가깝다(근접 피격 주변 거부 0이 목표, 상한 초과는 예외).
- 호스트의 피격·회피 동작이 바뀌지 않았다(`Enable 0`과 같은 줄 구성).
- 지연 0 게스트의 거부가 0으로 유지된다(결정 2 — 지연 0 게스트의 늦은 놓침이 회피로 바뀌는 것은 의도된 변화다).
- 사용자가 피해 지연을 거슬리지 않다고 확인한다. 거슬리면 `HitGraceMax`를 낮추고 그 비용(남는 거부)을 기록한다.

## 단위 5 — 문서 마감 (Phase 3에서 옮겨 옴)

- `GameDesign_JustDodge.md`·`TechDesign_JustDodge.md`를 새로 만든다. 작업 폴더 `design/`의 확정 내용을 요약하고, 상세는 링크한다.
- 교차 링크: `TechDesign_ParrySystem.md`(Judge 순서), `TechDesign_HitDetection.md` §7.5(결과 enum·피격 유예), `TechDesign_CharacterMovement.md` §5(대시 무적·소급), `TechDesign_Poise.md`(경직 누적의 Game Thread 경로), `TechDesign_Networking.md`(피격 유예 버퍼).
- 문서 인덱스 3곳(CLAUDE.md, ProjectOverview, Notion)과 `DevelopmentPlan.md`.
- `Decisions.md`: J-010을 `대체됨`으로 바꾸고 새 ID로 피격 유예 버퍼를 확정한다. 결정 1~4의 결과도 행으로 남긴다.
- `design/Architecture.md` §6.2를 구현 결과로 고친다(`U`·`H` 분리 포함).

## 위험

| 위험 | 대응 |
|:---|:---|
| 즉시 경로가 묶음화로 미묘하게 바뀐다(커맨드 순서, 경직 누적 시점) | `Dispatch`의 즉시 분기가 기존 순서 그대로 `PushCommand`하게 한다. 단위 2 완료 조건의 `Enable 0` 비교 |
| 보류 중 엔티티 소멸(사망·리스폰·접속 종료) | 해제와 소급에서 `IsEntityActive`·약참조를 확인하고 버린다 |
| 경직이 보류되는 동안 그로기 판정이 늦어진다 | 의도된 비용(`H`)이다. 해제 시 한 프레임에 여러 건이 쌓여도 `ULNPPoiseProcessor`는 합계만 본다 |
| `PlayerState` 핑이 PIE `PktLag`와 어긋난다 | 4-1 누름 추정 오차 표로 확인한다. 어긋나면 `U`를 Mover 입력 버퍼 실측으로 바꾸는 안을 검토한다 |
| 관통 예외(소급 회피된 탄은 소멸) | 문서에 J-003 예외로 명시한다 |

## 체크리스트

- [x] 사용자 결정 1~4 (2026-11-04, 모두 권장안)
- [x] 단위 1 — 미러·서브시스템 골격·설정·CVar (2026-11-04, 게스트 `H` 110ms·호스트 0)
- [x] 단위 2 — 효과 묶음화, 4경로 연결, `Enable 0` 회귀 없음 (2026-11-04, 로그 기준. 실효 보류는 프레임 양자화로 `H`~`H+1프레임`, `history/Phase04_Log.md`)
- [ ] 단위 3 — 소급 판정, PktLag 25 수동 확인
- [ ] 단위 4 — 누름 추정 보정, R1~R3 재측정, 완료 조건
- [ ] 단위 5 — 공통 문서·교차 링크·인덱스 3곳·결정 원장
