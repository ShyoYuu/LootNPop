# 저스트 회피 (Just Dodge) 기술 설계

> 기획: [GameDesign_JustDodge.md](GameDesign_JustDodge.md)
> 상세 설계 원본: [JustDodge/design/Architecture.md](JustDodge/design/Architecture.md) — 이 문서는 프로젝트 공통 요약이다. 결정 원장은 [JustDodge/Decisions.md](JustDodge/Decisions.md)(J-001~J-019)다.
> 구현 상태: 메인 트랙 완료(2026-11-05, Phase 1~4). 비주얼 트랙 V는 계획 단계다.

## 1. 한눈에 보기

대시는 GAS 밖에서 돈다(InputCmd → Mover 시뮬레이션, [TechDesign_CharacterMovement.md](TechDesign_CharacterMovement.md) §5). 그래서 회피 상태도 같은 시뮬레이션 경로에서 만들고, 판정은 패링과 같은 Mass 미러 패턴을 탄다.

| 역할 | 위치 | 근거 |
|:---|:---|:---|
| 회피 창 기록 | `ULNPCharacterMoverComponent::ExecuteDash`의 비리시뮬레이션 분기 | 서버·소유 클라가 같은 프레임에 대시를 인식한다. 롤백마다 창이 다시 열리지 않는다 |
| 상태 보관 | `FLNPParryStateFragment`(만료 시각 필드) | Worker Thread에서 락 없이 읽는다 |
| 판정 | `LNPGuardParry::Judge` (4개 판정 프로세서 공용) | 판정 한 곳(§3) |
| 성공 처리 | `FLNPJustDodgeCommand` (Game Thread) | ASC(버프 GE·큐)가 필요하다 |
| 지연 보정 | `ULNPHitGraceSubsystem` (서버, Game Thread) | 원격 게스트 피격 효과 보류 + 소급 회피(§6) |
| 연출 경계 | `GameplayCue.LNP.JustDodge.Success` / `.Buff` | 메인 트랙은 비주얼 없이 완결한다 |

### 파일 구성

| 파일 | 내용 |
|:---|:---|
| `HitDetection/LNPGuardParryTypes.h/.cpp` | 프래그먼트 필드, `Judge`·`JudgeRetro`, `Report*` 디버그 |
| `HitDetection/LNPHitDetectionShared.h` | `FLNPJustDodgeCommand`, 효과 커맨드들의 `ApplyEntry` |
| `HitDetection/LNPHitGraceSubsystem.h/.cpp` | 피격 유예: 효과 묶음, `Dispatch`, 보류·해제·소급 |
| `Movement/LNPCharacterMoverComponent.cpp` | `ExecuteDash` 창 기록, `IFrameDuration`·`JustDodgeWindow` |
| `GAS/Effects/LNPGameplayEffect_JustDodgeBuff` | 버프 GE |
| `GAS/GameplayCues/LNPGameplayCueNotify_JustDodgeSuccess` | 성공 큐(V1 순보가 확장할 자리) |

## 2. 데이터 — 만료 시각 미러

`FLNPParryStateFragment`(이름은 패링 시절 그대로)에 필드를 더했다.

| 필드 | 뜻 |
|:---|:---|
| `DodgeIFrameExpiryTime` | 무적 창 절대 만료 시각(`World->GetTimeSeconds()`), −1 = 없음 |
| `JustDodgeExpiryTime` | 저스트 창 만료 시각. 길이는 무적 창 이하로 묶는다 |
| `bJustDodgeConsumed` | 이번 대시의 저스트 회피를 썼는가(대시당 1회) |
| `HitGraceSeconds` | 서버 전용 피격 유예 보류 길이(§6). 0이면 즉시 처리 |

- **해제 경로가 없다.** 가드의 bool 미러는 명령형이라 해제가 하나만 빠져도 가드가 남는다([TechDesign_ParrySystem.md](TechDesign_ParrySystem.md) §2.2). 회피는 시각 비교만 하므로 그런 결함이 생길 수 없다.
- 사망·리스폰 초기화도 없다. 리스폰은 새 엔티티를 만들고, 그 사이는 `FLNPPlayerDeadTag`가 판정 쿼리에서 뺀다.
- 기록하는 머신은 권위(판정용)와 오토노머스(클라 투사체 예측 임팩트 생략용)다. 시뮬레이티드 프록시는 기록하지 않는다.

## 3. `Judge` 확장 — 판정은 한 곳에서

결과 enum에 `JustDodged`·`Dodged`를 더했다. 순서는 다음과 같다(J-006).

```
Parried → JustDodged(패링 반경 안 && 저스트 창 && 미소비) → (피격 반경 밖이면 None) → Dodged(무적 창) → Guarded → Hit
```

- 근접·투사체 직격·스플래시·화염 4경로가 이 함수 하나를 부른다. 경로마다 회피 분기를 복제하지 않는다 — PvP 근접에만 패링이 빠졌던 사고의 교훈이다([TechDesign_HitDetection.md](TechDesign_HitDetection.md) §7.5).
- 저스트 판정이 피격 반경 검사보다 앞이다. 패링 반경 안까지 들어온 "아슬아슬한 빗나감"도 저스트 회피다.
- 화염·스플래시는 `bInParryRange=false`를 넘기므로 저스트 회피가 구조적으로 성립하지 않는다.
- **소비 플래그는 Worker Thread에서 쓰지 않는다.** 같은 프레임의 공격 여럿이 모두 `JustDodged`로 나올 수 있다. 그래서 Game Thread 커맨드가 플래그를 확인하고 첫 건만 인정한다.

## 4. 경로별 처리

| 경로 | `Dodged` | `JustDodged` |
|:---|:---|:---|
| 근접 | `MarkHit` 없음 — 무적이 끝난 뒤 아직 겹쳐 있으면 맞는다 | `MarkHit` + 성공 커맨드 |
| 투사체 직격 | 관통. 투사체 프래그먼트의 회피 대상 칸(`DodgedEntity`)으로 같은 대상 재피격을 막는다. 패링 반사 때 비운다 | 같음 + 성공 커맨드 |
| 스플래시·화염 | 피해·넉백·경직·새 화상 스택 없음 | 발생하지 않음 |
| 클라 투사체 예측 | 대상의 무적 창이 열려 있으면 예측 임팩트를 생략한다 | — |
| 화상 지속 틱 | 막지 않는다(J-013) | — |

## 5. 성공 처리와 버프

`FLNPJustDodgeCommand::ApplyEntry`의 처리 순서는 다음과 같다.

1. 소비 플래그를 확인하고 세운다.
2. 버프 GE를 적용한다.
3. `.Success` 큐를 실행한다(위치와 대시 방향을 싣는다).
4. 로그를 남긴다.

- 버프 GE: `HasDuration` 2.5초, `AggregateByTarget`·스택 상한 1·적용 시 지속 갱신 → 중첩 없이 시간만 갱신한다. 스택 설정은 Burn GE의 C++ 패턴을 따른다.
- ⚠️ `MultiplyAdditive`의 항등원은 1이다. SetByCaller에 `1 + 0.2`를 넣어야 한다. 0.2를 그대로 넣으면 ×0.2로 느려진다(2026-11-03 실측).
- 연출은 서버 확정 뒤에만 낸다(J-007). 게스트 누름 → 소유 화면 큐가 RTT +100에서도 p50 약 150ms로 대시 길이 안이었다.

## 6. 네트워크 — 피격 유예 버퍼 (J-015)

**문제:** 대시는 InputCmd 경로라 서버가 입력이 도착한 프레임에 실행한다. 지연이 있으면 서버가 게스트의 피격 넉백(이동 모드를 공중으로 강제)을 먼저 처리한다. 그러면 뒤늦게 도착한 대시를 `CanDash()`의 `IsOnGround()`가 거부한다. 게스트는 **맞고 대시까지 잃는다.** RTT +50에서 38번 중 10번이 이렇게 거부됐다.

**해법:** 피해를 늦춘다. 패링이 RTT/2 역보정으로 방어 창을 미래로 늘리는 것과 같은 계열이지만, 창을 늘리는 대신 피격 효과를 보류한다.

```
판정 프로세서 (Worker)       Hit·Guarded → FLNPPlayerHitEffects 묶음 → LNPHitGrace::Dispatch
                               ├ PS.HitGraceSeconds == 0 (호스트·적·꺼짐) → 기존 커맨드 즉시
                               └ > 0 (원격 게스트)                      → FLNPHoldPlayerHitCommand
ULNPHitGraceSubsystem (Game)   매 틱: 플레이어별 H를 프래그먼트에 미러 / t_h + H가 지난 묶음 적용
ExecuteDash (권위, 원격 게스트) 창을 E − U부터 열고 → ResolveDash: 보류 묶음을 JudgeRetro로 소급
```

| 기호 | 정의 |
|:---|:---|
| `U` 업링크 추정 | `RTT/2 + HitGraceInputAllowance(0.045)` — 누름 추정 `T_press = E − U`에만 쓴다 |
| `H` 보류 길이 | `min(U + HitGraceJitterMargin(0.04), HitGraceMax(0.25))` — 늦게 온 대시를 기다리는 데만 쓴다 |

- **`U`와 `H`를 나누는 이유:** 누름을 `E − H`로 잡으면 지터 여유만큼 이르게 추정해 "맞은 뒤 누른 대시"까지 소급된다.
- **창 대칭(J-016):** 원격 게스트의 서버 창은 `E − U`부터 연다. 소급 구간과 실시간 구간을 합친 길이가 호스트와 같다.
- **소급 술어 `JudgeRetro`:** `T_press ≤ t_h ≤ T_press + IFrame`이면 `Dodged`다. 근접·직격이고 미소비이며 `t_h ≤ T_press + Just`이면 `JustDodged`다. `Judge`와 같은 파일에 둔다.
- **보류하는 것:** 경직 누적, 데미지 GE(HitReact·넉백), 가드 블록·근접 임팩트 큐, 공격자 HitStop, 화상 스택.
- **즉시 하는 것:** 근접 `MarkHit`, 투사체 소멸·주변 스플래시. 그래서 소급 회피된 탄은 관통하지 않는다(J-019).
- **경직 누적의 두 번째 경로:** 보류된 경직은 해제 시 Game Thread에서 `EntityManager`로 피격자 프래그먼트를 찾아 쌓는다([TechDesign_Poise.md](TechDesign_Poise.md) §4).
- **측정(2026-11-05, R1~R3):** 피격 유예가 없애려던 거부("누른 뒤 서버 피격" 유형)는 `Enable 0`에서 8건, `Enable 1`에서 0건이었다. 지연 구간 게스트 성공률(거부 포함)은 R2 38%→65%, R3 30%→60%다. 누름 추정 오차는 p50 +4~+8ms다.
- **비용:** 게스트가 받는 피해·사망이 `H`~`H + 1프레임`만큼 늦게 보인다. 실시간 저스트 창이 `0.12 − U`로 줄어, 고지연 게스트의 빗나감 저스트 회피는 거의 없다(J-018).

## 7. 디버그

| 도구 | 용도 |
|:---|:---|
| `LNP.Debug.Dodge 1` | 로그: `[Dodge] IFrame open/ignored`, `JustDodge success`, `Hit ... sinceJust`, `Success cue arrived`, `Grace`, `Grace release`, `Retro` |
| `LNP.Debug.ShowDodge 1` | 화면 문구(보라 계열): `IFRAME`, `IFRAME BLOCK [RETRO]`, `JUST DODGE` |
| `LNP.HitGrace.Enable 0\|1` | 피격 유예 A/B(기본 1) |
| `Scripts/Profiling/AnalyzeDodgeLog.py` | 성공률·놓침·업링크·연출 지연·소급·거부 유형·누름 추정 오차 집계 |

문자열 규약과 판정 시나리오는 [JustDodge/design/Validation.md](JustDodge/design/Validation.md)에 있다.

## 8. 어필 포인트

### 8.1 판정 한 곳 확장
패링 누락 버그 이후 4경로가 `Judge` 하나를 쓰도록 정리해 두었다. 그래서 회피 두 단계(무적·저스트)를 더하는 일이 판정 함수 한 곳과 경로별 결과 처리로 끝났다. 지연 대응도 같은 원칙을 지켰다. 소급 술어는 `Judge` 옆에 두고, 4경로의 효과 처리는 `Dispatch` 하나로 모았다.

### 8.2 해제 경로가 없는 상태
회피 상태를 bool이 아니라 만료 시각으로 미러링했다. 그래서 "어느 경로에서 끄는 것을 잊었다"는 종류의 버그가 원천적으로 없다.

### 8.3 측정이 결정을 뒤집었다
"InputCmd 경로라 역보정이 필요 없다"(J-010)는 이론상 맞았다. 그런데 "놓친 회피"와 "서버가 거부한 누름"을 세는 계측을 붙이자 넉백이 대시를 막는 경로가 드러났다. 대응은 창을 늘리는 대신 피해를 늦추는 방식이고, 같은 계측으로 효과를 확인했다(거부 8→0).

## 9. 미구현 / 한계

- 비주얼 트랙 V(순보·잔상·오라·카메라)가 남았다. 일부는 그래픽스 작업으로 넘겼다.
- 피격 유예 상한 0.25초를 넘는 지연에서는 그 너머의 거부가 남는다.
- 공격 종류별로 저스트 회피 보상을 다르게 하면 같은 프레임 다중 성공의 우선순위 규칙이 필요하다.
- PvP 근접에서 공격자 화면의 예측 HitStop이 상대의 무적과 한 번 어긋날 수 있다(J-014, 허용).
