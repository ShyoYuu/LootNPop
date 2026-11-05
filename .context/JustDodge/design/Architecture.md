# 저스트 회피 기술 설계

> 상태: 기준 설계 (메인 트랙 완료, 2026-11-05). 수치는 Phase 3에서 1차 확정했다.
> 마지막 갱신: 2026-11-05
> 코드 인용은 대부분 2026-10-26 기준이다. 작업 전에 실제 위치를 다시 확인한다.

## 1. 출발점 — 현재 코드 사실

| 사실 | 위치 | 설계에 주는 영향 |
|:---|:---|:---|
| 대시는 GAS 밖에서 돈다. 입력 콜백은 버퍼(0.05초)만 열고, InputCmd(`bWantsToDash`)에 실려 `OnMoverPreSimulationTick`에서 `CanDash()`일 때 `ExecuteDash`를 부른다 | `Character/LNPInputHandlerComponent.cpp:680-697`, `Movement/LNPCharacterMoverComponent.cpp:585-591` | 회피 상태도 같은 경로에서 생겨야 서버·클라이언트가 같은 프레임에 대시를 인식한다 |
| `ExecuteDash`는 `bIsResimulating`이면 연출 직전에 return한다. 그 아래에서만 `OnDashExecuted`와 몽타주를 실행한다 | `LNPCharacterMoverComponent.cpp:281-375` (게이트 `:362-374`) | 프래그먼트 기록은 이 게이트 뒤에 둔다 — 롤백할 때마다 창이 다시 열리면 안 된다 |
| 대시 파라미터: `DashDuration` 0.2, `DashImpulseMagnitude` 2000, `DashCooldown` 1.0 | `LNPCharacterMoverComponent.h:175-182` | 무적 창(0.15)은 대시 길이보다 짧게 둔다 |
| 대시 상태를 나타내는 ASC 태그·Mass 플래그가 없다. 무적 플래그도 없다 | — | 새로 만든다 |
| 플레이어 판정 4곳이 `LNPGuardParry::Judge` 하나를 부른다 | `HitDetection/LNPGuardParryTypes.h:56-70` | 확장 지점이 하나다 |
| 판정은 Worker Thread에서 `FLNPParryStateFragment`만 읽는다. 플레이어가 프래그먼트를 얻는 헬퍼는 `ULNPInputHandlerComponent::GetParryFragment()`다 | `LNPGuardParryTypes.h:17-33`, `LNPInputHandlerComponent.cpp:607-630` | 회피 필드도 같은 구조체에 둔다(`Judge`가 이 구조체 하나만 받는다) |
| 시체는 서버 쿼리에서 `FLNPPlayerDeadTag`로, 클라이언트 예측에서 `IsDead()`로 제외된다 | `LNPProjectileProcessors.cpp:222, 404-406`, `LNPWeaponTraceProcessors.cpp:241, 299-301` | 클라이언트 예측 경로가 따로 있으므로 회피도 예측 경로에서 따로 생략해야 한다 |

## 2. 데이터와 기록 지점

### 2.1 프래그먼트 필드 (`FLNPParryStateFragment`에 추가, 구조체 이름은 유지)

| 필드 | 기본 | 뜻 |
|:---|---:|:---|
| `DodgeIFrameExpiryTime` | -1 | 무적 창 절대 만료 시각(`World->GetTimeSeconds()` 기준). -1이면 없음 |
| `JustDodgeExpiryTime` | -1 | 저스트 창 절대 만료 시각 (Phase 2) |
| `DodgeDirection` | 영벡터 | 실제 대시 이동의 월드 방향. 비리시뮬레이션 분기에서 창과 함께 기록하고 일반·소급 성공 큐가 읽는다 |
| `bJustDodgeConsumed` | false | 이번 대시의 저스트 회피를 이미 썼는지 (J-011, Phase 2) |

구조체 이름을 바꾸지 않는 이유: 이 구조체를 참조하는 프로세서 쿼리·`LNPPoiseTypes.cpp:102`·InputHandler가 모두 그대로 동작한다. 의미가 "방어 상태"로 넓어졌다는 점은 헤더 주석으로 남긴다.

### 2.2 기록 — `ExecuteDash`의 비리시뮬레이션 분기

```
ExecuteDash(...)
  ... 물리 대시·쿨다운 큐잉 (기존)
  if (bIsResimulating) return;          ← 기존 게이트
  [신규] 회피 창 기록
     Now = World->GetTimeSeconds()
     PF->DodgeIFrameExpiryTime = Now + IFrameDuration
     PF->JustDodgeExpiryTime   = Now + JustDodgeWindow   (Phase 2)
     PF->DodgeDirection       = DashDirection
     PF->bJustDodgeConsumed    = false                   (Phase 2)
  OnDashExecuted.Broadcast() / Montage_Play (기존)
```

- 기록하는 머신: **권위(서버)**와 **오토노머스(소유 클라이언트)**. 서버 기록은 판정에 쓰이고, 소유 클라이언트 기록은 예측 연출을 생략하는 데만 쓰인다. 시뮬레이티드 프록시에서는 기록하지 않는다.
- `IFrameDuration`(초안 0.15)과 `JustDodgeWindow`(초안 0.12)는 `ULNPCharacterMoverComponent`의 UPROPERTY다(대시 파라미터와 같은 곳).
- 프래그먼트 접근 수단은 공용 헬퍼 `LNPGuardParry::FindFragment(const AActor*)`다. 소유 Actor의 `UMassAgentComponent`만 쓰므로 서버의 원격 폰에서도 동작한다. InputHandler의 `GetParryFragment()`도 이 헬퍼에 위임한다(Phase 1 단위 0).
- **해제는 만료 시각으로만 한다.** 가드의 bool 미러는 명령형이라 해제 경로가 하나라도 빠지면 가드가 남는다(Parry §2.2). 회피는 시각 비교만 하므로 해제 경로가 없다.
- **사망·리스폰 초기화 지점도 두지 않는다.** 리스폰은 새 폰과 새 엔티티를 만들고, 필드는 기본값 -1로 시작한다. 사망부터 리스폰까지는 `FLNPPlayerDeadTag`가 판정 쿼리에서 빼 준다(Phase 1 단위 0).
- 시계: 서버 판정 프로세서의 `Now`도 같은 `World->GetTimeSeconds()`다(`ParryWindowExpiryTime`과 같은 규약).
- **원격 게스트(서버 기록):** 창을 실행 시각 `E`가 아니라 누름 추정 시각 `E − U`부터 연다. 그리고 같은 자리에서 보류된 피격을 소급 판정한다(§6.2, J-016). 호스트·소유 클라는 `U = 0`이다.

## 3. `Judge` 확장

결과 enum에 두 값을 추가한다: `None, Parried, JustDodged, Dodged, Guarded, Hit`.

```
Judge(PS, FacingDot, Now, bInParryRange, bInHitRange)
  ① Parried     ← 기존 조건 그대로 (패링이 먼저 — 보상이 더 크고 기존 동작 보존)
  ② JustDodged  ← bInParryRange && Now <= PS.JustDodgeExpiryTime && !PS.bJustDodgeConsumed     (Phase 2)
  ③ (bInHitRange가 아니면) None
  ④ Dodged      ← Now <= PS.DodgeIFrameExpiryTime
  ⑤ Guarded / Hit (기존)
```

- **각도 조건이 없다(J-004).** 회피는 전방향이다.
- **패링 반경이 피격 반경보다 크다.** 그래서 피격 반경 밖이지만 패링 반경 안에 들어온 공격, 즉 "아슬아슬하게 빗나간" 공격도 저스트 회피가 된다. 사용자가 말한 "아슬아슬한 타이밍"의 기하학적 정의다.
- 저스트 창은 무적 창보다 짧으므로 ②가 성립하면 무적 창도 열려 있다. 저스트 창이 무적 창보다 길어지도록 튜닝하면 이 가정이 깨진다. 그래서 기록할 때 `Min(JustDodgeWindow, IFrameDuration)`으로 묶는다.
- `Judge`는 `const` 프래그먼트를 받는다. **소비 플래그(`bJustDodgeConsumed`)는 Worker Thread에서 쓰지 않는다.** 결과가 `JustDodged`이면 Game Thread 커맨드가 소비 플래그를 세운다. 같은 프레임에 들어온 공격 여러 개가 모두 `JustDodged`로 판정될 수 있으므로, 커맨드가 "이미 소비됨"을 다시 확인해 중복 성공을 버린다(J-011).
- 순서(J-006)와 대시당 1회(J-011)는 2026-11-03에 스펙으로 확정됐다. 이 구조에서 나오는 다음 두 동작도 사용자가 의도에 맞는다고 확인했다.
  - **같은 프레임의 근접 공격은 모두 휘두르기가 끝난다.** 근접 경로는 `JustDodged`면 커맨드를 넣기 전에 `MarkHit`을 하므로, 같은 프레임에 들어온 칼날 여럿이 성공 인정 여부와 상관없이 그 대상에게 끝난다. 한 프레임 늦은 칼날은 `Dodged`(`MarkHit` 없음)라 무적이 끝난 뒤에도 겹쳐 있으면 맞는다.
  - **같은 프레임 안에서 어느 공격이 인정되는지는 커맨드 처리 순서에 달려 있다.** 공격 종류와 상관없이 보상이 같아서 지금은 문제가 없다. 공격 종류별로 보상을 다르게 하면 우선순위 규칙이 필요하다.
- 화염·스플래시는 `bInParryRange=false`를 넘기므로 ②가 성립하지 않는다(J-002가 구조적으로 보장된다).

## 4. 경로별 처리

| 경로 (호출 위치) | `Dodged` | `JustDodged` (Phase 2) |
|:---|:---|:---|
| 근접 `JudgePlayerTarget` (`LNPWeaponTraceProcessors.cpp:554`, Judge `:587`) | MarkHit 없음 → 무적이 끝난 뒤에도 칼날이 겹쳐 있으면 맞는다. 경직·피해·큐 없음 | MarkHit(그 휘두르기는 이 대상에게 끝난다) + `FLNPJustDodgeCommand` |
| 투사체 직격 (`LNPProjectileProcessors.cpp:1039-1051`) | `FinishHit` 없음 → 관통해 계속 비행. 투사체 프래그먼트에 회피 대상을 기록하고 이후 그 대상을 후보에서 뺀다(J-003) | 같음 + 커맨드. 패링 반경 계산 조건을 `PS.bIsParrying \|\| 저스트 창 열림`으로 넓힌다 |
| 스플래시 (`LNPProjectileProcessors.cpp:715-724`) | 피해·넉백·경직 없음 | 발생하지 않음 |
| 화염 원뿔 (`LNPFlameConeProcessors.cpp:344`) | 틱 피해·화상 스택 없음 | 발생하지 않음 |
| 클라이언트 투사체 예측 (`LNPProjectileProcessors.cpp:393-484`) | 대상 플레이어의 무적 창이 열려 있으면 예측 임팩트를 생략하고 Ghost의 회피 대상 칸에 기록한다(관통하는 그림 유지, 무적이 끝난 뒤 다시 겹쳐도 맞지 않음) | — |
| 클라이언트 근접 예측 (`LNPWeaponTraceProcessors.cpp:250-368`) | 처리하지 않는다. 로컬 공격자의 HitStop 예측이라 로컬 플레이어가 대상이 아니고, 상대의 무적 창은 그 머신에 없다. PvP에서 예측 HitStop이 한 번 어긋나는 것은 허용한다 | — |
| 플레이어 화상 지속 틱 (`ULNPGameplayEffect_Burn` 주기 GE) | `Judge`를 거치지 않으며 회피하지 않는다. 막는 것은 무적 중에 새로 붙는 스택까지다 | — |

- **투사체 회피 대상 칸:** `FLNPProjectileFragment`에 `FMassEntityHandle DodgedEntity` 1칸을 둔다. 탄 하나가 무적 플레이어 둘을 연달아 관통하는 경우는 드물어서 1칸으로 시작하고, 필요하면 늘린다. **패링 반사 때 초기화한다.** 반사탄은 원래 공격자를 노리고, 반사한 사람은 새 `Instigator`라 어차피 제외된다. 클라이언트 Ghost도 같은 칸을 쓴다.
- **근접 Dodged에 MarkHit을 하지 않는 이유:** 너무 일찍 대시해서 무적이 끝났는데도 아직 칼날 안에 있다면 맞는 것이 정직하다. 저스트 회피는 타이밍이 맞았다는 뜻이므로 그 휘두르기를 끝낸다.

## 5. 성공 처리와 버프 (Phase 2)

### 5.1 `FLNPJustDodgeCommand` (Game Thread, 서버)

1. 소비 플래그를 확인하고 이미 소비됐으면 버린다. 아니면 세운다.
2. 버프 GE `ULNPGameplayEffect_JustDodgeBuff`를 방어자 ASC에 적용한다.
3. `GameplayCue.LNP.JustDodge.Success`를 실행한다. 큐의 `Location`은 현재 방어자 위치, `Normal`은 프래그먼트에 기록된 `DodgeDirection`이다. `ExecuteDash`는 소급 판정의 `ResolveDash`를 부르기 전에 이 방향을 기록한다. Actor의 현재 속도는 새 대시 적용 전의 값일 수 있으므로 사용하지 않는다(V1a 방향 버그 수정).
4. 서버 로그 `[Dodge] JustDodge success player=<id> attack=<Melee|Projectile>`를 남긴다.

- 공격자에게는 아무 효과도 주지 않는다(감속·경직 없음 — 사용자 결정).
- 큐는 ASC를 통해 모든 머신에 전파된다. 소유 플레이어에게만 보여야 하는 연출(화면 효과)은 큐 안에서 `IsLocallyControlled()`로 가른다.

### 5.2 버프 GE

| 항목 | 값 |
|:---|:---|
| 지속 | `HasDuration` 2.5초 (SetByCaller 또는 UPROPERTY) |
| 스택 | `AggregateByTarget`, `StackLimitCount = 1`, `StackDurationRefreshPolicy = RefreshOnSuccessfulApplication` → 중첩 없이 시간만 갱신 |
| Modifier | `AttackSpeed`·`MoveSpeed` 각각 `MultiplyAdditive` +0.2 (`TAG_GE_Data_Stat_*` SetByCaller) — 곱연산 채널 (`GAS/LNPStatModifier.h:26-34`). ⚠️ SetByCaller에는 배율 자체 `1 + 0.2`를 넣는다. `MultiplyAdditive`의 항등원은 1이라 0.2를 그대로 넣으면 ×0.2로 느려진다(2026-11-03 실측). 설정값(`ULNPSettings::JustDodgeBuff*`)은 +0.2 표기다 |
| GameplayCue | `GameplayCue.LNP.JustDodge.Buff` (지속 큐 — GE가 갱신돼도 유지, 만료 시 제거) |

- 스택 설정은 `GAS/Effects/LNPGameplayEffect_Burn.cpp:24-34`의 C++ 패턴을 따른다. `SetStackingType`이 에디터 전용이라 deprecated 필드에 직접 쓰고 `PRAGMA_DISABLE_DEPRECATION_WARNINGS`로 감싼다.
- 버프 아이템용 GE는 Infinite GE이고 만료를 인벤토리 타이머가 처리한다. 저스트 회피 버프는 아이템이 아니므로 GE 기간으로 끝낸다. 버프 아이템 경로(`LNPInventoryComponent`)는 거치지 않는다.
- 소비처는 기존 그대로다. AttackSpeed는 몽타주 시작 시 PlayRate·발사 쿨다운·재장전에 반영되고, MoveSpeed는 `FLNPMoveSpeedModifier`가 매 틱 반영한다. 휘두르는 도중에 버프가 켜지면 다음 몽타주부터 빨라진다.
- ⚠️ MoveSpeed는 ASC에서 직접 읽으므로 버프가 켜지고 꺼지는 순간 서버와 클라이언트의 틱이 어긋나 짧은 보정이 생길 수 있다([CharacterMovement §4.1](../../TechDesign_CharacterMovement.md)). 연속 저스트 회피로 이런 경계가 자주 생기면 체감될 수 있으므로 Phase 2 검증 항목에 넣는다.

## 6. 네트워크와 지연

| 구간 | 내용 |
|:---|:---|
| 대시 실행 | 서버는 게스트의 InputCmd를 받은 프레임에 대시를 실행한다(입력 버퍼 지연 포함). 판정 무적 창은 이 서버 시각부터 열린다 |
| 적의 공격 | 서버 권위다. 게스트는 적을 RTT/2와 보간 지연만큼 늦게 본다 |
| 결과 | 게스트가 "보고 피한" 타이밍은 서버 기준으로 약 RTT만큼 늦다. 패링이 RTT/2 역보정으로 다루는 것과 같은 성격의 문제다 |
| 연출 | 서버가 확정한 큐는 소유 화면에 다시 RTT/2 뒤에 도착한다. RTT가 100ms 수준이면 대시(0.2초)가 거의 끝난 뒤 순보 연출이 시작될 수 있다 |

- **J-010(대체됨 → J-015, 2026-11-05):** 무적 창에 RTT 역보정을 하지 않는다는 결정이었다. 시작점을 과거로 당겨도 이미 적용된 피해는 되돌릴 수 없어서다. Phase 3 측정에서 비용이 드러났고(§6.1), 피해를 늦추는 피격 유예 버퍼(§6.2)로 대체했다. 피해가 보류되므로 이제는 창을 `E − U`로 당겨도 의미가 있다.
- **J-007(확정, 2026-11-04):** 연출은 서버 확정 뒤에만 낸다. 게스트 누름 → 소유 화면 성공 큐가 RTT +50·+100에서 p50 약 150ms로 대시(0.2초) 안이었고, 사용자도 늦다고 느끼지 않았다. 순보 비주얼이 붙은 뒤 V-05에서 다시 본다.
- **기각된 대응 후보 — 소유 클라이언트 연출 예측:** 오토노머스 프래그먼트와 클라이언트 예측 경로로 성공을 미리 그린다. J-007 확정으로 채택하지 않는다.

### 6.1 측정된 비용 — 넉백이 늦게 도착한 대시를 거부한다 (Phase 3, 2026-11-04)

- 지연을 주면 게스트가 누른 대시의 일부를 서버가 끝내 실행하지 않는다. RTT +50에서 38번 중 10번, +100에서 19번 중 4번이 거부됐다. 지연이 없으면 0번이다. 거부된 14건 모두 누른 시점 ±300ms 안에 서버 피격이 있었고, 대부분은 누른 뒤 대시 입력이 서버에 도착하기 전(+29~+76ms)이었다.
- 메커니즘: `ApplyKnockback`이 이동 모드를 공중으로 강제하고, `CanDash()`는 `IsOnGround()`를 요구한다. 그래서 서버가 피격을 먼저 처리하면 게스트는 **맞고, 대시까지 잃는다.** 게스트는 넉백을 아직 보지 못한 채 대시를 예측 실행했다가 롤백된다(보정 캡슐이 많이 보인 원인).
- 사용자는 군중 속에서 이것을 "피했는데 맞았다"로 체감하지 못했다. 그래도 사용자 결정으로 대응을 설계한다.
- 측정 근거: `history/Phase03_Log.md` 2026-11-04 R2·R3. 재측정은 `AnalyzeDodgeLog.py`의 `dropped by server` 줄로 한다.

### 6.2 피격 유예 버퍼 (J-015, 구현 완료 2026-11-05)

> 구현 실행서와 측정: `phases/Phase04_HitGraceBuffer.md`, `history/Phase04_Log.md`. 아래 A는 설계 당시의 표이고, 구현에서 바뀐 점은 표 아래 "구현 결과"에 정리했다.

**A. 피격 유예 버퍼 (근본 대응)**

서버가 원격 클라이언트 플레이어에게 가는 피격 효과를 짧게 보류한다. 그 사이에 도착한 대시가 "피격보다 먼저 눌렸다"고 추정되면 회피로 바꾼다. 패링이 RTT/2 역보정으로 "방어를 미래로 연장"하는 것과 같은 계열이다. 다만 창을 늘리는 대신 **피해를 늦춘다.**

| 항목 | 설계 |
|:---|:---|
| 대상 | 서버에서 원격 클라이언트가 조종하는 플레이어 피격자. 호스트와 적은 지금처럼 즉시 처리한다 |
| 유예 길이 `H` | `clamp(피격자 RTT/2 + 여유, 0, 상한)`. RTT/2는 패링과 같은 `PlayerState` 핑에서 얻는다. 지연 없는 업링크(입력 버퍼 포함 p50 약 70ms)가 RTT/2보다 길어서 여유값이 필요하다. 여유와 상한은 재측정으로 정한다 |
| 보류하는 것 | 결과가 `Hit`·`Guarded`인 피격의 모든 게임플레이 효과 — 데미지 GE(넉백·히트리액트 포함), 경직 누적, 가드 블록 커맨드, 화상 스택, 임팩트 큐·적 HitStop. 4개 경로 모두 같은 묶음을 보류 큐에 넣는다 |
| 즉시 하는 것 | 판정의 기하 소비 — 근접 `MarkHit`, 투사체 `FinishHit`(소멸). 소급 회피가 되어도 그 탄은 관통하지 않는다(J-003의 예외로 받아들인다) |
| 소급 판정 | 서버가 그 플레이어의 대시를 실행하면 누른 시각을 `T_press = 실행 시각 − H`로 추정한다. 보류 중인 피격 중 `T_press ≤ 피격 시각 ≤ T_press + IFrameDuration`이면 `Dodged`로 바꾼다. 근접·직격이고 피격 당시 패링 반경 안이었으며 `피격 시각 ≤ T_press + JustDodgeWindow`이면 `JustDodged`로 바꾼다(대시당 1회 규칙 유지) |
| 만료 | 소급되지 않은 피격은 `피격 시각 + H`에 그대로 적용한다 |
| 판정 위치 | 소급 판정은 Game Thread의 보류 큐에서 한다. 원래 판정은 `Judge` 그대로라 J-001(판정 한 곳)을 깨지 않는다. 소급 규칙은 `LNPGuardParry`에 술어 하나로 둔다 |

- 효과: 넉백이 보류되므로 늦게 도착한 대시도 `CanDash()`를 통과하고, 그 피격은 소급 회피된다. 6.1의 "맞고 대시까지 잃는" 경우가 없어진다.
- 비용: 게스트가 받는 모든 피해가 `H`만큼 늦게 보인다(게스트 화면 기준 총 지연 ≈ 다운링크 + `H`). 사망도 `H`만큼 늦어진다. 경직 누적이 Worker Thread에서 Game Thread 보류 큐로 옮겨 가는 경로가 생긴다.
- 크기: 보류 큐(서브시스템 1개), 4개 경로의 효과 커맨드 묶음화, 경직 누적 이전, `ExecuteDash` 권위 분기의 소급 호출, 재측정. **1~2세션 규모라 별도 단계로 둔다.**

**B. 넉백 중 대시 허용 (부분 대응)**

`CanDash()`가 넉백으로 강제된 공중 상태에서는 짧은 유예 동안 대시를 허용한다(대시가 넉백을 끊는다).

- 효과: 늦게 도착한 대시가 거부되지 않아 게스트가 대시를 잃지 않고 보정 롤백도 사라진다. **피격 자체는 그대로 맞는다.**
- 비용: 호스트를 포함한 모두의 게임플레이가 바뀐다(대시로 넉백 캔슬). 공중 대시 제한([CharacterMovement §5.3](../../TechDesign_CharacterMovement.md))에 예외가 생긴다.
- 크기: Mover 상태 하나와 `CanDash()` 조건 하나라 작다.

**권장:** A를 별도 단계로 진행한다. A가 넉백을 보류하므로 게스트에게는 B의 효과까지 포함된다. B는 게임플레이 변경이라 지연 대응과 섞지 않는다. → A를 채택했다(사용자, 2026-11-04). B는 채택하지 않았다.

**구현 결과 (Phase 4)**

| 항목 | 결과 |
|:---|:---|
| `U`와 `H` 분리 | 누름 추정에는 업링크 추정 `U = RTT/2 + HitGraceInputAllowance(0.045)`만 쓴다. 보류 길이는 `H = min(U + HitGraceJitterMargin(0.04), HitGraceMax(0.25))`다. 초안의 `T_press = E − H`는 지터 여유만큼 과하게 소급한다. 재측정한 누름 추정 오차는 p50 +4~+8ms다 |
| 창 대칭 (J-016) | 원격 게스트의 서버 창을 `E − U`부터 연다. 소급 구간과 실시간 구간을 합친 길이가 호스트와 같다 |
| 대상 (J-017·J-019) | 서버에서 로컬 조종이 아닌 플레이어 피격자 전부다. 지연이 없어도 업링크(입력 버퍼, p50 약 55~66ms)가 있으므로 적용한다. 4경로(근접·직격·스플래시·화염) 모두 같다 |
| 미러 | `ULNPHitGraceSubsystem`(서버 Tickable)이 매 틱 `FLNPParryStateFragment::HitGraceSeconds`에 `H`를 쓴다. 판정 프로세서는 프래그먼트만 읽는다 |
| 묶음 | 4경로의 `Hit`·`Guarded` 효과를 `FLNPPlayerHitEffects`에 모아 `LNPHitGrace::Dispatch`에 넘긴다. `H = 0`이면 예전과 똑같이 즉시 커맨드를 넣는다. 커맨드 5종은 본문을 `ApplyEntry`로 꺼내 즉시·해제가 같은 함수를 쓴다 |
| 보류/즉시 | 보류하는 것: 경직, 데미지 GE(HitReact·넉백), 가드 블록 큐, 근접 임팩트 큐(공격자 HitStop 포함), 엔티티 HitStop 신호, 화상 스택(확률은 판정 시점에 굴림). 즉시 하는 것: 근접 `MarkHit`, 투사체 `FinishHit`(임팩트 큐·소멸·주변 스플래시), `[Dodge] Hit` 로그 |
| 소급 (J-018) | `LNPGuardParry::JudgeRetro`: `T_press ≤ t_h ≤ T_press + IFrame`이면 `Dodged`, 근접·직격이고 미소비이며 `t_h ≤ T_press + Just`이면 `JustDodged`. 보류된 피격만 대상이다(빗나감은 기록이 없다). 성공 처리는 `FLNPJustDodgeCommand::ApplyEntry`를 공유한다 |
| 해제 | 서브시스템 틱이 `t_h + H`가 지난 묶음을 적용한다. 프레임 단위라 실효 보류는 `H`~`H + 1프레임`이다. 리스폰·사망한 피격자의 묶음은 버린다 |
| 비용 | 게스트가 받는 피해·사망이 실효 보류만큼 늦게 보인다(PktLag 50에서 화면상 약 0.25초). 사용자 체감은 문제없음이었다. 소급 회피된 탄은 관통하지 않는다(J-003 예외). 결정 1 때문에 지연이 크면 실시간 저스트 창이 `0.12 − U`로 줄어, 게스트의 "아슬아슬 빗나감" 저스트 회피는 거의 없어진다 |
| 측정 | Phase 3 유형 거부(누른 뒤 서버 피격): `Enable 0`에서 8건 → `Enable 1`에서 0건(R1~R3). 지연 구간 게스트 성공률(거부 포함): R2 38%→65%, R3 30%→60%. 남은 거부는 "이미 맞은 뒤 누름"이며, 호스트도 같은 상황이면 대시를 잃는다 |

## 7. 디버그

- 로그와 화면 문구는 CVar를 나눈다. 둘 다 기본은 꺼짐이다(사용자 요청, 2026-11-03). 구현은 `LNPGuardParry::ReportDodgeIgnored`·`ReportDodgeWindowOpened`·`ReportJustDodgeSuccess`·`ReportGhostImpactSkipped`(`HitDetection/LNPGuardParryTypes.cpp`)에 있다.
- `LNP.Debug.Dodge [0|1]` — 로그
  - `[Dodge] IFrame open <폰> role=<역할> expiry=<시각> just=<저스트 창 길이>`: 권위·오토노머스에서 창을 연 시점
  - `[Dodge] IFrame ignored path=<Melee|Projectile|Splash|Flame> player=<폰>`: 서버
  - `[Dodge] JustDodge success player=<폰> attack=<Melee|Projectile>`: 서버 성공 커맨드가 확정한 시점(대시당 최대 1줄)
  - `[Dodge] Ghost impact skipped player=<폰>`: 클라 Ghost가 무적 때문에 예측 임팩트를 생략한 시점(탄 하나당 1줄). 게스트 화면의 D-07 판정용
  - `[Dodge] Hit path=<Melee|Projectile> player=<폰> sinceJust=<ms|->`: 서버. 저스트 회피 대상 공격이 그대로 맞은 시점(`ReportDodgeHit`). `sinceJust`는 마지막 저스트 창이 닫힌 뒤 경과 ms이고, 1초가 넘었거나 대시가 없었으면 `-`다. 놓친 저스트 회피 계측용(Phase 3)
  - `[Dodge] Success cue arrived player=<폰> local=<0|1> role=<역할>`: `.Success` 큐가 그 머신에 도착한 시점(`ULNPGameplayCueNotify_JustDodgeSuccess`). `local=1`이 소유 화면이다. 연출 지연 계측용(Phase 3)
  - 집계: `Scripts/Profiling/AnalyzeDodgeLog.py <로그> [--since HH:MM:SS]`. 리스폰한 폰은 서버·게스트 월드에서 이름이 달라서 게스트 쪽 줄은 시각으로 짝짓는다
- `LNP.Debug.ShowDodge [0|1]` — 화면 문구(보라 계열, 사용자 요청으로 두 색을 구분, 2026-11-03)
  - `IFRAME  0.15s  JUST 0.12s  (<폰>)` — **연한 보라**: 로컬 폰이 창을 연 순간 무적 창 길이만큼 떠 있다. 문구가 보이는 동안이 곧 무적 창이다
  - `IFRAME BLOCK  <경로>  (<폰>)` — **짙은 보라**: 서버가 무적으로 피격을 무시할 때마다 1초. 경로마다 한 줄을 덮어쓴다. **이 문구가 떴다면 무적이 없었을 때 맞았을 공격이다.** 이동만으로 빗나간 공격과 플레이 중에 구분하는 수단이다(사용자 피드백, 2026-11-03)
  - 서버 판정 시점에 띄우는 `GEngine` 문구라 단일 프로세스 PIE에서는 모든 창에 보인다. 별도 프로세스 게스트에게는 보이지 않는다
  - `JUST DODGE  <Melee|Projectile>  (<폰>)` — **밝은 자홍**: 서버가 저스트 회피를 확정할 때 1.5초. 비주얼이 없는 메인 트랙에서 테스터 피드백용이다. `IFRAME BLOCK`과 같은 이유로 단일 프로세스 PIE에서는 모든 창에 보인다
- `LNP.HitGrace.Enable [0|1]`(기본 1) — 피격 유예 A/B. `[Dodge] Grace ...`·`Grace release ...`·`Retro ...` 줄과 `IFrame open`의 `press=`·`uplink=`는 `design/Validation.md` §6.
- 보조 도구는 기존 것을 쓴다: `LNP.Debug.ShowSpeed 1`(버프 배율 확인), `Mover.LocalPlayer.ShowCorrections`, `LNP.Spawn.EnemyDensity 0`(보정 진단 시 적 제거).
