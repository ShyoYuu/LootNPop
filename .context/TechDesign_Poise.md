# 경직(Poise) 시스템 기술 설계

## 1. 한눈에 보기

피해와 나란히 흐르는 **두 번째 판정 축**이다. 누적·감쇠·임계 판정은 Mass Fragment와 Processor가 맡고, 행동 차단은 GAS 어빌리티가, 연출은 GameplayCue가 각각 소유한다.

| 역할 | 수행 위치 | 근거 |
|:---|:---|:---|
| 상태 보관 | `FLNPPoiseFragment` (Mass) | Worker Thread 판정에서 락 없이 접근 |
| 누적 | 히트 판정 Processor 서버 구역 (Worker Thread) | 캡슐 수집 데이터·피격 분기를 그대로 재사용 |
| 자연회복 · 상태 전이 판정 | `ULNPPoiseProcessor` (PostPhysics, 서버 전용) | 매 틱 도는 단일 지점 |
| 발동 (GA·큐·공격 취소) | `FLNPStaggerCommand` (Game Thread) | ASC 접근 필요 |
| 행동 차단 | `ULNPAbility_Stagger`의 `ActivationOwnedTags` | 어빌리티 수명 = 차단 구간, 소유 클라에 복제됨 |
| 연출 | `GameplayCue.LNP.Character.Stagger` | 적 ASC가 `Minimal` 복제라 어빌리티가 시뮬 프록시에 안 간다 |

**구현 상태:** 누적·자연회복·그로기·다운·가드 브레이크·패링 연계 모두 완료. PIE 1인 + 2인 검증 완료.
**Standalone `-game` 2프로세스 로그 실측** (PIE보다 강한 구성): 서버 발신 13회에 대해 호스트 13 / 게스트 13 수신,
밸류 태그 3종(Light 4 / Heavy 2 / **Parried 7**)이 양쪽 로그에서 완전히 일치했고 `tags=1`이 26건 전부.
호스트·게스트 어느 쪽이 패링해도 동일하게 재현된다 — `Minimal` ASC를 GameplayCue로 우회한 §7 설계가 성립한다.
그로기 루프 포즈 에셋만 잔여.

기획 의도는 [GameDesign_Poise.md](GameDesign_Poise.md) 참조.

---

## 2. 설계 원칙 — 자연회복이 유일한 감소 수단

임계값을 넘었다고 게이지를 리셋하거나 차감하지 않는다. 자연회복 속도를 상회하는 화력을 몰아쳐야만 게이지가 유지·상승하고, 손을 놓으면 저절로 풀린다.

이 불변식의 유일한 예외가 **다운**이다 — 게이지 0 리셋 + 면역. 시스템 전체에서 스턴락을 끊는 단 하나의 탈출구이자, 유일한 방어 장치다.

```
  ~T1        평시. 감쇠만 진행
  T1 이상    그로기 — 공격·이동 불가. 누적은 계속된다 (리셋·면역·차감 없음)
             → 자연회복으로 T1 아래로 내려가면 해제
             → T2 도달 시 다운
  다운       고정 시간 정지 + 게이지 0 + 면역 PoiseDownImmunitySeconds
```

> 그로기 구간에서 누적을 막지 않는 것은 버그가 아니라 설계다 — 그게 딜 구간이 성립하는 이유다.

---

## 3. 데이터 구조

### 3.1 FLNPPoiseFragment

```cpp
// GAS/LNPPoiseTypes.h
float  Current;                 // 현재 경직도. 다운으로만 리셋된다
float  Resistance;              // 경직저항력 미러 (워커 스레드가 ASC를 못 보므로)
float  StaggerThreshold;        // T1 — 폰별 시드
float  DownThreshold;           // T2 — 폰별 시드
double LastHitTime;             // 감쇠 유예 판정용
float  ImmunityTimeRemaining;   // 다운 직후 면역. 누적을 막는 유일한 조건
float  GroggyElapsed;           // 그로기 유지 시간 — 유입 보너스 계산에 쓴다
uint8  bIsGroggy : 1;           // 진입·이탈 에지 감지용
uint8  bParryBreakPending : 1;  // 패링 유발 여부 — 연출만 갈라 쓰는 1회성 플래그
```

**부착 지점**

| 대상 | 방법 |
|:---|:---|
| Enemy | `ULNPEnemyTrait::BuildTemplate`에서 `AddFragment_GetRef` + 즉시 시드 |
| Player | `DA_PlayerEntityConfig`의 `MassAssortedFragmentsTrait` (에디터, `FLNPParryStateFragment`와 같은 자리) |

⚠️ Player 쪽은 에디터 에셋 설정이라 빠뜨리기 쉽다. 누락 시 `ALNPPlayerCharacter::PushPoiseResistanceToEntity`가 경고 로그를 남기고 플레이어는 **경직도가 아예 쌓이지 않는다.**

### 3.2 스텟 — 경직력 · 경직저항력

| 축 | 위치 | 비고 |
|:---|:---|:---|
| 경직력 | `ULNPAbility_BasicAttack::PoiseDamage`, 근접은 `ComboPoiseDamages` | 넉백(`KnockbackStrength`)과 같은 자리·같은 패턴 |
| 경직저항력 | `ULNPBaseAttributeSet::PoiseResistance` → 프래그먼트로 미러 | 정식 스텟 — 버프·스탯 탭 자동 연동 |

**무기 레벨 스케일을 걸지 않는다.** 피해처럼 제곱으로 커지면 고레벨 무기 하나로 영구 경직락이 성립한다. 넉백도 같은 이유로 레벨과 무관하다.

⚠️ 산탄(`ULNPAbility_RangedSpreadAttack`)은 19발까지 나가고 발마다 누적되므로 **발당 값**으로 잡아야 한다.

감쇠 공식은 방어력과 동일한 형태다 (`GAS/LNPDamageFormula.h`):

```cpp
LNPPoise::ApplyResistance(Raw, Resist) = Raw * 100 / (100 + Resist)
```

**저항 미러 갱신**

| 대상 | 시점 |
|:---|:---|
| Player | 어트리뷰트 변경 델리게이트 + `PossessedBy`에서 기초값 1회. 엔티티 핸들이 늦게 준비되면 `Tick`이 성립할 때까지 재시도 |
| Enemy | `BuildTemplate`에서 1회 (적은 지속 버프를 받지 않는다) |

> 기초값을 명시적으로 밀어 넣는 이유: 변경 델리게이트는 **값이 바뀔 때만** 울린다. 버프를 한 번도 안 받은 플레이어는 저항 0으로 남아 잡몹에게 굳어 버린다.

### 3.3 임계값이 전역 상수면 안 되는 이유

T1~T2 구간이 곧 딜 구간이다. 적은 넓게, 플레이어는 좁게 잡고 싶은데 **저항이 유입량을 나누기 때문에 전역 상수로 두면 정반대가 된다.**

밴드 통과 타수 = `(T2 − T1) ÷ (경직력 × 저항계수)`

| 대상 | 저항계수 | 전역 밴드 70을 지나는 데 |
|:---|:---|:---|
| 적 (저항 20) | ×0.833 | 2.1타 ← 딜 구간이 짧다 |
| 플레이어 (저항 150) | ×0.400 | 4.4타 ← 무력 구간이 길다 |

저항이 높을수록 밴드도 느리게 지나가므로, **임계값을 폰별로** 들고 있어야 의도가 성립한다 (플레이어는 `ULNPSettings`, 적은 `ULNPEnemyConfig`에서 시드).

### 3.4 튜닝값 — `ULNPSettings` `Combat|Poise`

| 값 | 뜻 |
|:---|:---|
| `PoiseDecayPerSecond` | 자연회복 속도. 스텟이 아니라 전역 상수 |
| `PoiseDecayDelaySeconds` | 마지막 피격 후 감쇠 재개까지의 유예 — 연타가 실제로 쌓이게 하는 장치 |
| `PoiseStaggerThreshold` / `PoiseDownThreshold` | **플레이어 기준** T1·T2 |
| `PoiseDownLockSeconds` | 다운 지속 시간 (GA 수명) |
| `PoiseDownImmunitySeconds` | 다운 직후 면역. 다운 지속보다 길게 잡아 일어선 직후 유예를 준다 |
| `PoiseGroggyBonusPerSecond` | 그로기 유지 시간 비례 유입 보너스 (§5) |
| `PoiseGuardMultiplier` | 가드로 막았을 때의 누적 비율 |
| `PoiseParryBreakRatio` | 근접 패링이 쏟아붓는 양 — 공격자 T1에 대한 배율 |

---

## 4. 판정 흐름

```
공격 어빌리티 PoiseDamage
  → FLNPWeaponTraceFragment / FLNPProjectileSharedFragment ::PoiseDamage   (KnockbackStrength와 같은 자리)
      → 판정 Processor 서버 구역: LNPPoise::Accumulate(피격자 프래그먼트, ...)
            Current += ApplyResistance(PoiseDamage, Resistance)
                       × (가드면 PoiseGuardMultiplier)
                       × (그로기면 1 + 보너스율 × GroggyElapsed)
      → ULNPPoiseProcessor (PostPhysics, 서버 전용)
            자연회복 → 그로기 진입·이탈 / 다운 판정
              → FLNPStaggerCommand (Game Thread)
                    Groggy : 공격 취소 · 몽타주 정지 · 가드 해제 · GA 발동 · 큐
                    None   : 실행 중인 경직 GA 취소
                    Down   : 위 + 그로기 GA를 먼저 취소한 뒤 재발동
```

**Actor 승격 여부와 무관하게 같은 눈금으로 쌓인다.** Low LOD 적(Actor 없음)도 게이지가 차고 줄며, 그로기·다운 중에는 `ULNPEnemyMovementProcessor`가 `bIsGroggy`·`ImmunityTimeRemaining`을 보고 속도를 0으로 만든다 — GA도 몽타주도 없으므로 이 검사가 유일한 정지 경로다.

**클라이언트 예측 경로에서는 절대 누적하지 않는다.** GE를 적용하지 않는 것과 같은 이유다 (§7).

---

## 5. 그로기는 상태이지 시간이 아니다

`ULNPAbility_Stagger`가 `WaitDelay`로 스스로 끝나지 않는다. 게이지가 T1 아래로 회복하면 `ULNPPoiseProcessor`가 **이탈 에지**를 잡아 `FLNPStaggerCommand(None)`으로 취소한다. 다운만 고정 시간(`PoiseDownLockSeconds`)을 쓴다.

| 항목 | 값 |
|:---|:---|
| 트리거 | `Stagger.Light`(그로기) / `Stagger.Heavy`(다운) — 경직도 상태 전이가 유일한 진입점 |
| `ActivationOwnedTags` | `State.Staggered`, `Block.AttackInput`, `Block.MovementInput` |
| `ActivationBlockedTags` | `State.Staggered` — 그로기 중 재진입 방지. **다운은 이 태그에 자기가 막히므로** 커맨드가 그로기 GA를 먼저 취소한다 |

### 무한 그로기 방지 — 시계가 아니라 타격으로

게이지를 T1 바로 위에 걸쳐 두는 화력이면 T2에 영영 닿지 않아 무력 상태가 무한정 유지된다. 이를 **유지 시간 비례 유입 보너스**로 막는다:

```
유입 = 기본 × (1 + PoiseGroggyBonusPerSecond × GroggyElapsed)
```

유입이 시간에 비례해 무한히 커지고 자연회복은 상수이므로, **계속 때리는 한 T2 도달이 보장된다.** 때리기를 멈추면 자연회복으로 풀리는 것이 정상 동작이다.

> **타임아웃(N초 넘으면 강제 다운)을 쓰지 않는 이유:** 타임아웃은 아무도 때리지 않는 순간에 픽 쓰러질 수 있어 인과가 화면에 안 보인다. 보너스는 **다운이 항상 타격 위에서** 일어나고, "자연회복을 상회하는 화력" 원칙 바깥에 안전장치를 덧대는 대신 원칙 안에서 같은 구멍을 막는다. 몰아치는 정상 전투에서는 보너스가 붙기 전에 이미 T2에 닿으므로 거의 개입하지 않는다.

---

## 6. Command별 책임 — `FLNPStaggerCommand::Run`

| 티어 | 하는 일 |
|:---|:---|
| `Groggy` | 공격 GA 취소 → 몽타주 정지 → 가드 강제 해제 → GA 발동 → 큐 |
| `Down` | 위와 같되, GA 발동 **직전에 그로기 GA를 취소**한다 (차단 태그 해제용) |
| `None` | 실행 중인 경직 GA만 취소 |

- **공격 취소:** `CancelCurrentAttackAbility()`. 몽타주만 덮어써서는 GAS 상태가 남아 콤보·쿨다운이 어긋난다.
- **몽타주 정지:** `Montage_Stop`을 **명시적으로** 부른다. 근접은 `PlayMontageAndWait`이 어빌리티 취소에 딸려 멈추지만, 원거리(`ULNPAbility_RangedAttack`)는 몽타주를 `Montage_Play`로 흘려보내고 어빌리티가 즉시 끝나므로 취소할 대상 자체가 없다.
- **가드 해제:** 서버 프래그먼트를 내리고 `ULNPInputHandlerComponent::Client_ForceReleaseGuard()`로 소유 클라의 눌림 상태까지 턴다.
- **랙돌 제외:** 사망 연출·적 시체에는 걸지 않는다 (해제는 랙돌 여부와 무관하게 처리해 태그가 남지 않게).

---

## 7. 네트워크 — 게이지는 안 보내고 결과만 보낸다

**경직도는 복제하지 않는다.** 클라이언트는 자신에게 들어오는 모든 히트를 알 수 없어(타 플레이어·NPC 공격은 전부 서버 판정) 예측이 반드시 어긋나고, **경직을 오예측하면 맞지도 않은 공격에 입력이 잠긴다** — 늦게 걸리는 것보다 훨씬 나쁘다. 넉백이 같은 이유로 미예측인 것과 일관된다.

전파되는 것은 결과 두 가지뿐이고, 서로 다른 채널을 탄다:

| 무엇 | 채널 | 왜 |
|:---|:---|:---|
| 입력 차단 | GA 활성화 복제 (`ActivationOwnedTags`) | 권위 상태라 코스메틱 큐에 실을 수 없다. 플레이어 ASC는 `Mixed`라 소유 클라에 도달한다 |
| 몽타주 | `GameplayCue.LNP.Character.Stagger` | **적 ASC는 `Minimal`이라 어빌리티 활성화가 시뮬 프록시에 안 간다** — GA가 몽타주를 들면 게스트 화면에서 적 경직이 안 보인다 |
| AI 행동 정지 | 전파 없음 | AI는 서버 전용 |

⚠️ 둘 다 몽타주를 재생하면 소유 클라에서 이중 재생이 난다. **몽타주는 큐만, 차단은 GA만.**

추가 대역폭은 0이다. HUD 게이지가 필요해지면 그때 `COND_OwnerOnly` float 하나를 붙인다.

> **밸류 태그도 함께 도착한다.** `FGameplayCueParameters::NetSerialize`는 `AggregatedSourceTags`를
> **RepBits 마스크 바깥에서 무조건** 직렬화한다 — RepFlag 목록에 없는 이유는 "복제 안 함"이 아니라
> *빈 컨테이너가 1비트라 마스크 비트를 둘 이유가 없어서*다(엔진 주석 명시). 2인 실측에서
> Light/Heavy 구분이 게스트까지 정확히 전달되는 것을 확인했다.

---

## 8. 연계 — 가드 브레이크 · 근접 패링

**가드 브레이크:** 가드가 성립한 분기에서도 `PoiseGuardMultiplier`를 곱해 누적한다. 임계 돌파 시 커맨드가 가드를 강제 해제한다. `ReleaseGuardState()`가 `bIsGuardPressed`를 내리므로, Enhanced Input의 `Started`는 전이에서만 발화하는 특성상 **키를 계속 누르고 있어도 떼었다 다시 눌러야** 재가드된다.

**근접 패링:** `LNPPoise::ApplyParryBreak`가 공격자 T1의 `PoiseParryBreakRatio`배를 한 번에 넣어 즉시 그로기에 빠뜨린다. 경직저항력을 적용하지 않는다 — 패링은 스텟 대결이 아니라 타이밍 판정이다. 초과분이 남아 그로기가 오래 가고, 이어서 때리면 다운까지 이어진다.

> **전용 스태거 GA를 병행하지 않는 이유:** 고정 시간 GA와 게이지 기반 그로기를 동시에 돌리면 GA가 먼저 끝나면서 게이지는 아직 T1 위인데 행동이 풀려 **그로기가 조용히 깨진다** (프로세서는 `bIsGroggy`가 이미 참이라 재진입 명령을 내지 않는다). 그래서 패링 전용 이벤트·지속시간을 제거하고 경직 시스템 하나로 통합했다.

연출은 갈라 쓴다. `bParryBreakPending`(1회성 비트)을 세우면 프로세서가 그로기 진입 에지에서 소비해 밸류 태그를 `Value.Stagger.Parried`로 바꿔 보낸다. **행동은 일반 그로기와 완전히 동일하고 몽타주만 다르다.** 플래그는 진입·이탈·다운 어느 쪽에서든 소비되므로 나중의 무관한 그로기로 새지 않는다.

---

## 9. 어필 포인트 (트러블슈팅 & 설계 판단)

### 9.1 낮은 임계값이 자동 발동하면 높은 임계값에는 도달할 수 없다

초기 구현은 T1을 넘는 순간 고정 시간 경직을 걸고 게이지를 0으로 리셋했다. 그 결과 **2단계가 구조적으로 도달 불가능**했다 — 게이지가 60에서 잘려 130 근처에 갈 기회가 없고, 한 프레임에 130을 한꺼번에 넘기는 다발 히트(19펠릿 산탄)만 2단계를 냈다.

"리셋 대신 임계값만 빼면 되지 않나"도 안 된다. 83−60=23에서 두 대 더 치면 106이 되고 106도 60을 넘으니 또 1단계가 발동한다. 130 아래에서 영원히 진동한다.

해결은 임계값 조정이 아니라 **그로기를 시간이 아닌 상태로 재정의**하는 것이었다.

### 9.2 콤보 창이 입력 차단을 건너뛰던 구멍

`ALNPCharacterBase::TryActivateAttack`은 `State.ComboWindow`를 `Block.AttackInput`보다 먼저 검사하고, 그 분기는 차단을 보지 않고 곧바로 재발동한다. 그로기 진입 시점에 콤보 창 태그가 한 프레임이라도 남아 있으면 차단을 통째로 건너뛴다 — NPC의 `FLNPEnemyAttackTask`는 매 프레임 이 함수를 두드리므로 그 한 프레임을 정확히 집어낸다.

두 태그의 순서를 바꾸는 것은 위험하다(두 ANS 구간이 겹치도록 몽타주가 짜여 있으면 정상 콤보가 깨진다). 대신 `State.Staggered`를 **최상단 절대 게이트**로 넣었다 — 경직 중엔 콤보 창이든 뭐든 공격이 불가능한 것이 명백하므로 부작용이 없다.

### 9.3 상태 전이를 에지로 잡아 GA를 켜고 끈다

그로기는 지속 시간이 없으므로 GA가 스스로 끝날 수 없다. 프로세서가 `bIsGroggy`의 진입·이탈 에지를 감지해 GA를 켜고 끈다. 다운은 그로기 위에 덮어쓰는 전이라 `ActivationBlockedTags`에 자기가 막히는데, 커맨드가 그로기 GA를 먼저 취소해 해결한다 — 취소 → `EndAbility` → 태그 해제가 동기 처리되므로 같은 호출 안에서 재발동이 성립한다.

### 9.4 부수 발견 — Low LOD 적이 15% 더 아팠다

조사 중 `FLNPEnemyFragment::Defense`가 선언만 되어 있고 **어디서도 값이 채워지지 않는** 것을 발견했다. Low LOD 피격은 이 값으로 감쇠하고 High LOD는 ASC의 `DefensePower`(기초 10 + 무기 5)를 쓰므로, 같은 공격이 Low LOD 적에게만 15% 더 들어가고 있었다. LOD는 거리·상태로 바뀌므로 "HP 차감 속도가 일정하지 않은" 체감의 원인이었다. `BuildTemplate`에서 MaxHealth와 같은 방식(`LNPStat::ResolveStatValue`)으로 시드하도록 수정.

---

## 10. 디버그 시각화

`LNP.Debug.DrawPoise 1` — `ULNPPoiseDebugDrawProcessor`(에디터 전용, 서버 전용, CVar 기본 0).
`LNP.Debug.DrawPoiseDistance`(cm, 기본 5000)로 표시 거리를 런타임에 조절한다.

> 적 디버그 드로우(`ULNPEnemyDebugDrawProcessor`)의 `DebugDrawProximityDistSq`(500cm)를 공유하지 않는다 —
> 패링 넉백처럼 대상이 멀리 튕겨 나가는 상황을 쫓아가야 해서 훨씬 넓어야 하고, 테스트 중 조절할 수 있어야 한다.

머리 위에 게이지 바를 그린다. 흰 눈금이 T1, 오른쪽 끝이 T2.

| 색 | 상태 | 라벨 |
|:---|:---|:---|
| 초록→노랑→빨강 | 평시 (비율) | `48 / 200   R 20` |
| 시안 | 그로기 | `GROGGY 1.4s   72 / 200` |
| 자주 | 다운 (바는 면역 잔여) | `DOWN  immune 2.1s` |

현재 Chooser 배정: Light `AM_SW_Damage_Fast`, Parried `AM_SW_Damage_Backward`, Heavy `AM_MM_HitReact_Front_Hvy_01`.

> **수치 밸런스는 아직 플레이로 조정하지 않았다.** 검증용으로 눌러 두었던 공격력·경직력은 전부 원복했고,
> 임계값 초안은 C++ 기본값에 반영돼 있다 — 플레이어 60 / 95(밴드 35), 적 60 / 200(밴드 140).
> `DefaultGame.ini` 오버라이드는 없으므로 이 기본값이 그대로 시작점이다.
>
> ⚠️ 다만 원복된 공격력 기준으로는 근접 3~4타에 서로 죽는다(적 HP 120 · 플레이어 100).
> 플레이어는 T1 60까지 약 4타가 필요해 경직이 거의 발동하지 않는다 — **경직이 의미를 가지려면
> 전투 지속 시간이 늘어나야 한다**는 뜻이고, 이는 경직 임계값이 아니라 공격력·HP 쪽 밸런스 문제다.

프로세서로 둔 이유: `FLNPPoiseFragment`를 가진 모든 엔티티가 한 경로로 덮인다 — 플레이어·High LOD 적·**Actor 없는 Low LOD 적**까지. 위젯으로는 마지막 항목이 불가능하다.

⚠️ `DrawDebugString`은 `TestBaseActor`가 null이면 모든 텍스트를 `WorldSettings` 하나에 몰아넣고 append한다. `r.DebugSafeZone.MaxDebugTextStringsPerActor`(기본 128)에 걸리면 그 프레임의 나머지 엔티티는 **숫자만 조용히 사라진다**(바는 정상).

---

## 11. 미구현 / 제약사항

- **그로기 루프 포즈:** 그로기 몽타주는 원샷이다. 그로기가 몽타주보다 길면 남은 시간은 idle 포즈로 굳어 있는 그림이 된다 (입력 차단은 정상). 루프 가능한 그로기 포즈 에셋이 필요하다.
- **다운 전용 몽타주:** 현재 `AM_MM_HitReact_Front_Hvy_01`(Hvy 히트리액트)로 대체 중. 넉다운·기상 애니메이션을 확보하면 Chooser 행의 에셋만 교체하면 된다.
- **원거리 적의 경직력:** `DA_NPC_Pistol`이 `ULNPAbility_RangedSpreadAttack` **네이티브 CDO**를 직접 부여받아 19펠릿을 쏜다. 펠릿 수·경직력이 C++ 기본값이라 에셋으로 조절할 수 있는 축이 무기 스텟뿐이다 — 한 볼리로 플레이어를 즉시 다운시킨다.
- **적 패링 미지원:** 패링 자체가 플레이어 전용이므로 `ApplyParryBreak`도 공격자(적) 방향으로만 흐른다.
