# 저스트 회피 기술 설계

> 상태: 기준 설계 (메인 트랙). 수치는 초안이다.
> 마지막 갱신: 2026-10-26
> 코드 인용은 2026-10-26 기준이다. 작업 전에 실제 위치를 다시 확인한다.

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
     PF->bJustDodgeConsumed    = false                   (Phase 2)
  OnDashExecuted.Broadcast() / Montage_Play (기존)
```

- 기록하는 머신: **권위(서버)**와 **오토노머스(소유 클라이언트)**. 서버 기록은 판정에 쓰이고, 소유 클라이언트 기록은 예측 연출을 생략하는 데만 쓰인다. 시뮬레이티드 프록시에서는 기록하지 않는다.
- `IFrameDuration`(초안 0.15)과 `JustDodgeWindow`(초안 0.12)는 `ULNPCharacterMoverComponent`의 UPROPERTY다(대시 파라미터와 같은 곳).
- 프래그먼트 접근 수단은 단위 0에서 정한다. 이동 컴포넌트가 InputHandler의 `GetParryFragment()`를 빌리는 방법과 헬퍼를 공용으로 옮기는 방법 중에서 고른다. 서버의 원격 폰에서도 그 경로가 동작하는지 확인해야 한다.
- **해제는 만료 시각으로만 한다.** 가드의 bool 미러는 명령형이라 해제 경로가 하나라도 빠지면 가드가 남는다(Parry §2.2). 회피는 시각 비교만 하므로 해제 경로가 없다. 사망·리스폰 때 -1로 초기화하는 지점만 둔다.
- 시계: 서버 판정 프로세서의 `Now`도 같은 `World->GetTimeSeconds()`다(`ParryWindowExpiryTime`과 같은 규약).

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
- 저스트 창은 무적 창보다 짧으므로 ②가 성립하면 무적 창도 열려 있다. 저스트 창이 무적 창보다 길어지도록 튜닝하면 이 가정이 깨진다. 튜닝 범위를 `JustDodgeWindow <= IFrameDuration`으로 제한한다.
- `Judge`는 `const` 프래그먼트를 받는다. **소비 플래그(`bJustDodgeConsumed`)는 Worker Thread에서 쓰지 않는다.** 결과가 `JustDodged`이면 Game Thread 커맨드가 소비 플래그를 세운다. 같은 프레임에 들어온 공격 여러 개가 모두 `JustDodged`로 판정될 수 있으므로, 커맨드가 "이미 소비됨"을 다시 확인해 중복 성공을 버린다(J-011).
- 화염·스플래시는 `bInParryRange=false`를 넘기므로 ②가 성립하지 않는다(J-002가 구조적으로 보장된다).

## 4. 경로별 처리

| 경로 (호출 위치) | `Dodged` | `JustDodged` (Phase 2) |
|:---|:---|:---|
| 근접 `JudgePlayerTarget` (`LNPWeaponTraceProcessors.cpp:554`, Judge `:587`) | MarkHit 없음 → 무적이 끝난 뒤에도 칼날이 겹쳐 있으면 맞는다. 경직·피해·큐 없음 | MarkHit(그 휘두르기는 이 대상에게 끝난다) + `FLNPJustDodgeCommand` |
| 투사체 직격 (`LNPProjectileProcessors.cpp:1039-1051`) | `FinishHit` 없음 → 관통해 계속 비행. 투사체 프래그먼트에 회피 대상을 기록하고 이후 그 대상을 후보에서 뺀다(J-003) | 같음 + 커맨드. 패링 반경 계산 조건을 `PS.bIsParrying \|\| 저스트 창 열림`으로 넓힌다 |
| 스플래시 (`LNPProjectileProcessors.cpp:715-724`) | 피해·넉백·경직 없음 | 발생하지 않음 |
| 화염 원뿔 (`LNPFlameConeProcessors.cpp:344`) | 틱 피해·화상 스택 없음 | 발생하지 않음 |
| 클라이언트 예측 (`LNPProjectileProcessors.cpp:404-474`, `LNPWeaponTraceProcessors.cpp:299`) | 로컬 플레이어가 무적이면 예측 임팩트를 생략한다(관통하는 그림 유지) | — |

- **투사체 회피 대상 칸:** 투사체 프래그먼트(`LNPProjectileMassTypes.h:72~`)에는 지금 "이미 맞힌 대상"을 담는 칸이 없다. `FMassEntityHandle` 1칸을 추가한다. 탄 하나가 무적 플레이어 둘을 연달아 관통하는 경우는 드물어서 1칸으로 시작하고, 필요하면 늘린다. 반사(패링) 때 이 칸을 초기화할지는 Phase 1 단위 0에서 정한다.
- **근접 Dodged에 MarkHit을 하지 않는 이유:** 너무 일찍 대시해서 무적이 끝났는데도 아직 칼날 안에 있다면 맞는 것이 정직하다. 저스트 회피는 타이밍이 맞았다는 뜻이므로 그 휘두르기를 끝낸다.

## 5. 성공 처리와 버프 (Phase 2)

### 5.1 `FLNPJustDodgeCommand` (Game Thread, 서버)

1. 소비 플래그를 확인하고 이미 소비됐으면 버린다. 아니면 세운다.
2. 버프 GE `ULNPGameplayEffect_JustDodgeBuff`를 방어자 ASC에 적용한다.
3. `GameplayCue.LNP.JustDodge.Success`를 실행한다. 큐에 실을 정보는 위치와 대시 방향이다(V1 연출에 쓴다).
4. 서버 로그 `[Dodge] JustDodge success player=<id> attack=<Melee|Projectile>`를 남긴다.

- 공격자에게는 아무 효과도 주지 않는다(감속·경직 없음 — 사용자 결정).
- 큐는 ASC를 통해 모든 머신에 전파된다. 소유 플레이어에게만 보여야 하는 연출(화면 효과)은 큐 안에서 `IsLocallyControlled()`로 가른다.

### 5.2 버프 GE

| 항목 | 값 |
|:---|:---|
| 지속 | `HasDuration` 2.5초 (SetByCaller 또는 UPROPERTY) |
| 스택 | `AggregateByTarget`, `StackLimitCount = 1`, `StackDurationRefreshPolicy = RefreshOnSuccessfulApplication` → 중첩 없이 시간만 갱신 |
| Modifier | `AttackSpeed`·`MoveSpeed` 각각 `MultiplyAdditive` +0.2 (`TAG_GE_Data_Stat_*` SetByCaller) — 곱연산 채널 (`GAS/LNPStatModifier.h:26-34`) |
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

- **J-010(초안):** 무적 창에 RTT 역보정을 하지 않는다. 시작점을 과거로 당겨도 이미 적용된 피해는 되돌릴 수 없고, 만료만 앞당겨진다. 패링의 역보정 상한(창의 절반)도 같은 이유로 창을 늘리지 못한다.
- **J-007(초안):** 연출은 서버 확정 뒤에만 낸다.
- **Phase 3에서 잰다:** 네트워크 에뮬레이션 50/100ms에서 게스트의 저스트 회피 성공률, 큐 도착 시각과 대시 종료 시각의 차이.
- **측정 결과가 나쁠 때의 대응 후보(미채택):**
  1. **소유 클라이언트 연출 예측:** 오토노머스 프래그먼트와 클라이언트 예측 경로로 성공을 미리 그린다. 오예측(서버가 실패로 판정)이면 버프가 오지 않을 뿐이라 해가 작지만, 연출과 결과가 엇갈린다.
  2. **피격 유예 버퍼:** 서버가 게스트에게 가는 피해를 최대 RTT/2만큼 보류하고, 그 사이에 대시 입력이 도착하면 회피로 바꾼다. 효과는 크지만 모든 피격 경로에 지연을 넣는다.

## 7. 디버그

- `LNP.Debug.Dodge [0|1]`
  - 서버 로그: `[Dodge] IFrame ignored path=<Melee|Projectile|Splash|Flame>`, `[Dodge] JustDodge success ...`
  - 화면: 로컬 폰의 무적 창과 저스트 창 남은 시간, 성공 시 텍스트 `JUST DODGE`(비주얼이 없는 메인 트랙에서 테스터 피드백용)
- 보조 도구는 기존 것을 쓴다: `LNP.Debug.ShowSpeed 1`(버프 배율 확인), `mover.debug.ShowCorrections 1`, `LNP.Spawn.EnemyDensity 0`(보정 진단 시 적 제거).
