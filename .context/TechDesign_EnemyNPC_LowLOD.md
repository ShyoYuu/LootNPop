# Enemy NPC — Low LOD(순수 엔티티) 전투 기술 설계

## 1. 한눈에 보기

원래 모든 Enemy는 전투 진입(`Confirmed`) 시 예외 없이 High LOD Actor로 승격되어 GAS·몽타주로 싸웠다
(→ [TechDesign_EnemyNPC.md](TechDesign_EnemyNPC.md) §7.2). 이 문서는 **승격을 Config 옵션으로 바꾸고**,
승격하지 않는 개체가 순수 MassEntity 상태 그대로 기본 공격까지 수행하게 만든 설계다.

```
[기존]  Confirmed ──▶ 무조건 Actor 승격 ──▶ GAS 어빌리티 + 몽타주 + ANS 히트 윈도우
[변경]  Confirmed ──▶ CombatMode 분기
          ├ ActorPromoted : 기존 경로 그대로 (이번 작업에서 한 줄도 바뀌지 않는다)
          └ PureEntity    : Mass 프로세서가 공격 위상을 직접 구동
                             ├ 판정 : 기존 HitDetection 파이프라인 100% 재사용
                             ├ 연출 : 행동 상태 1바이트 복제 → ISKM 인스턴싱 애니메이션
                             └ 사망 : 랙돌 없음, 사망 시퀀스 재생 후 소멸
```

**세 개의 독립 트랙으로 구성된다.**

| 트랙 | 내용 | 의존 |
|:---|:---|:---|
| **A. 순수 엔티티 공격** | Mass 전용 근접·원거리 기본 공격 | 없음 (서버 판정만으로 성립) |
| **B. 행동 상태 채널** | 서버 행동 상태를 1바이트로 복제 | 없음 |
| **C. ISM↔ISKM 하이브리드** | LOD 구간별 인스턴싱 애니메이션 | B (상태가 입력) |

A는 B·C 없이도 "보이지 않지만 실제로 때리는 적"으로 완성되고, B는 C의 입력이 된다 — 이 분리가 곧
단계별 검증 경로다 (§10).

**진행 상황 (2026-09-13):** A·B·C 전부 완료 — **Stage 5c(무기 스킨드 메시)까지 완료**, 칼날 아크를 무기 궤적에 맞췄다.
순수 엔티티는 검·샷건을 손에 들고 Idle·Move·Attack·Stagger·Parried·Dying을 전부 재생한다. 남은 것은 수치 튜닝뿐이다(§9).

---

## 2. 용어 규약 — "등급"은 시스템 용어가 아니다

⚠️ **코드·데이터 애셋에는 엘리트/잡몹/티어 같은 등급 어휘를 넣지 않는다.**
시스템이 아는 것은 단 하나, "전투 시 Actor로 승격하는가"뿐이다.

```cpp
UENUM(BlueprintType)
enum class ELNPEnemyCombatMode : uint8
{
    ActorPromoted,  // 전투 진입 시 High LOD Actor 승격 — GAS·몽타주·랙돌
    PureEntity,     // 승격하지 않음 — Mass 프로세서 전용 공격
};
```

등급·티어는 **기획 문서와 밸런싱에서만** 쓴다 (→ [GameDesign_EnemyNPC.md](GameDesign_EnemyNPC.md)).
근거는 셋이다.

- **등급은 늘어나고 축은 늘어나지 않는다.** 2단계가 5단계가 되어도 시스템이 가르는 것은 영원히 두 값이다.
- **조합이 어긋난다.** "엘리트인데 PureEntity"(대규모 정예 웨이브)나 "잡몹인데 ActorPromoted"(튜토리얼 1마리)는
  기획상 충분히 있을 수 있다. 이름이 모드를 단정하는 순간 코드가 거짓말을 시작한다.
- **판정이 한 값으로 끝난다.** 슬롯·비주얼·공격 경로가 전부 이 옵션 하나에서 갈린다.

`bCanPromoteToActor` + `bHasEntityAttack`처럼 bool 둘로 쪼개지 않는 이유도 같다 — bool 조합은
정의되지 않은 상태(둘 다 false / 둘 다 true)를 만들고, 그 동작을 문서로 방어해야 한다.

---

## 3. 이미 준비되어 있는 것 (재사용 자산)

이 설계가 성립하는 이유는 기존 코드가 이미 Actor 비의존이기 때문이다.
**새로 만드는 것보다 그대로 쓰는 것이 훨씬 많다.**

| 자산 | 왜 그대로 쓰이는가 |
|:---|:---|
| `FLNPWeaponTraceFragment` | 칼날을 **좌표 4점 + 반경**으로만 표현한다. 본·소켓·몽타주 참조가 하나도 없다 |
| `ULNPWeaponTraceHitDetectionProcessor` | `AttackerQuery`가 요구하는 것은 `FLNPWeaponTraceFragment` **하나뿐**이다 |
| `JudgePlayerTarget` 람다 | 패링→가드→피격 2단계 판정. 근접 PvP와 Enemy→Player가 이미 같은 코드를 탄다 |
| 발사체 스폰 | `ULNPAbility_RangedAttack::SpawnProjectile`은 껍데기만 어빌리티고 내용은 전부 `PushCommand<FMassCommandBuildEntityWithSharedFragments<…>>`다 |
| `FLNPApplyDamageGECommand` | **피격자 ASC만** 필요하다. 공격자 Actor는 없어도 된다. 연출은 나중에 `FLNPImpactCueCommand`로 떼어냈다(§4.7) |
| `AimPitchMin/MaxDeg` | 이미 Actor가 아니라 Config에 있다 — *"Mass 프로세서는 Actor가 없는 Low LOD에서도 돌아야 한다"* 는 이유로 |
| `FLNPPoiseFragment` | 경직은 원래 엔티티 단위 값이다. 순수 엔티티도 그대로 굳는다 |
| `FLNPEnemyMovementConfig::AttackInterval` | 당시 **선언만 있고 소비처가 0이었다.** 엔티티 공격 쿨다운이 이 필드의 유일한 소비처다 |

---

## 4. 트랙 A — 순수 엔티티 공격

### 4.1 데이터

무기 상수(발사체 속도·수명·폭발 반경·피해 GE 클래스)는 **이미 `ULNPEnemyConfig::WeaponData`에 있다.**
새로 정의할 것은 지금까지 *어빌리티 인스턴스가* 공급하던 값뿐이다 — 즉 기존 `FLNPProjectileSharedFragment`를
채우던 두 출처(WeaponData / Ability) 중 **Ability 쪽만 대체**한다.

`FLNPEntityAttackConfig`(`Enemy/LNPEnemyConfig.h`). 괄호 안은 C++ 기본값이고, 실제 값은 에셋에 있다(§6.5).

| 필드 | 규약 |
|:---|:---|
| `Damage` (10) · `PoiseDamage` (10) · `KnockbackStrength` (0) | **원거리는 펠릿 하나당**이다 — 산탄은 명중 수만큼 곱해져 들어간다 |
| `ParryRadius` (40) | ⚠️ 무기 `HitRadius`보다 크게 — 2단계 판정(패링 먼저)의 규약 |
| `WindupTime` (0.35) · `ActiveTime` (0.20) · `RecoveryTime` (0.45) | 선딜(플레이어가 읽고 반응할 구간) / 근접 칼날 생존 구간, **원거리는 미사용** / 후딜 |
| `ParriedRecoveryTime` (1.05) | 패링당해 자세가 무너진 채 남는 시간. ⚠️ 위상과 달리 **파생시킬 원본이 없어** 재생할 모션 길이와 손으로 맞춘다 — 서버는 어떤 시퀀스가 도는지 모르고, 알게 하면 애니가 게임플레이를 정하게 된다 |
| `PivotForward` (20) · `PivotUp` (30) · `BladeInner` (30) · `BladeOuter` (140) · `HitRadius` (12) | 가상 칼날 치수, 전부 **캡슐 중심 기준 로컬**. ⚠️ `BladeOuter`는 시각 무기 길이와 반드시 일치시킬 것(§6.3) |
| `ArcStartDeg` (-70) · `ArcEndDeg` (70) · `ArcPitchStartDeg` (-15) · `ArcPitchEndDeg` (-15) | 스윙 아크. **Yaw와 Pitch 둘 다** Active 구간에서 보간된다(§6.5). Pitch는 접평면 기준 **양수가 위** |
| `MuzzleLocalOffset` ((40,0,10)) | 총구(캡슐 중심 로컬 X=전방·Y=우측·Z=위) |
| `AimTargetUpOffset` (0) | 조준점 상하 보정. 캡슐 중심이 **골반** 높이라 가슴께를 겨누려면 양수(§4.4) |
| `HexRingCount` (0) · `HexStepDegrees` (5) | 산탄 배치 — 0=단발, 1=7발, 2=19발(§4.4) |

공격 간격은 새 필드를 만들지 않고 **`FLNPEnemyMovementConfig::AttackInterval`을 쓴다.**
당시 아무도 읽지 않던 필드이고, 의미가 정확히 일치한다.

### 4.2 상태 기계 — 판단은 Task가, 진행은 Processor가

```cpp
USTRUCT()
struct FLNPEntityAttackFragment : public FMassFragment
{
    ELNPEntityAttackPhase Phase = None;   // None / Windup / Active / Recovery
    float             PhaseElapsed      = 0.f;
    float             CooldownRemaining = 0.f;   // AttackInterval에서 채워진다
    FMassEntityHandle SwingEntity;        // 근접: 살아 있는 칼날 엔티티
    uint8             bAttackRequested : 1 = 0;  // Task가 세우고 프로세서가 소비하는 1회성 요청
    float             ParriedTimeRemaining = 0.f; // 서버 전용 장부 — 소비처는 ULNPEnemyActionProcessor
};
```

⚠️ **`ParriedTimeRemaining`은 공격 위상을 끊지 않는다.** 패링 분기가 이미 그 플레이어를 피격 목록에
올려 두므로 칼날이 남은 Active를 살아도 다시 맞히지 못한다 — 연출만 덮어쓰면 되고, 위상을 건드리면
쿨다운·스윙 파괴 경로까지 함께 흔들린다.

⚠️ **이 프래그먼트는 `ActorPromoted` 개체에도 붙인다.** 모드로 아키타입을 가르지 않는다
(`FLNPEnemyActionFragment`·`FMassRepresentationAnimationFragment`도 같다 →
[TechDesign_EnemyNPC.md](TechDesign_EnemyNPC.md) §2.1).

- StateTree 외부 데이터 핸들이 Optional이 되면 Task 코드에 null 분기가 생긴다.
- 아키타입이 갈리면 같은 쿼리를 두 벌 유지해야 한다.
- 대가는 개체당 ~24바이트다. 위 두 비용보다 압도적으로 싸다.

```
FLNPEnemyAttackTask::Tick  (신호 구동)
  └ CombatMode == ActorPromoted → Enemy->TryActivateAttack()       (기존 경로)
    CombatMode == PureEntity    → AttackFrag.bAttackRequested = 1   (요청만)

ULNPEntityAttackProcessor  (매 프레임, 서버 전용, Behavior 그룹 이후)
  └ 쿨다운 감소 → 요청 소비 → 위상 진행 → 근접이면 칼날 4점 갱신 → 종료·중단 처리
```

**왜 위상 진행을 Task에 두지 않는가.** Mass StateTree의 Task Tick은 `StateTreeActivate` 신호가 있어야만
돈다. 신호가 끊기면 스윙이 중간에 멈춘 채 칼날 엔티티만 살아남는다.
이는 배회 교착에서 이미 겪은 함정이고(→ TechDesign_EnemyNPC.md §5.1), 해법도 같다 —
**시간 측정과 진행은 매 프레임 도는 프로세서가, 판단은 Task가 단독으로.**

### 4.3 근접 — "가상 칼날"

`UANS_LNPMeleeHitWindow`가 하는 일은 **본 위치를 읽어 4점을 채우는 것**뿐이다.
그러므로 그 4점을 절차적으로 계산하면 판정 코드는 한 줄도 바뀌지 않는다.

```
기저 (구면 규약 — TechDesign_EnemyNPC.md §5.1)
  Center = FTransformFragment 위치 (= 캡슐 중심, 발밑이 아니다)
  Up     = (GravityOrigin - Center).GetSafeNormal()
  Fwd    = 엔티티 전방을 접평면에 투영 후 정규화
  Right  = Cross(Up, Fwd)

매 프레임 (Active 구간, t = PhaseElapsed / ActiveTime)
  Pivot = Center + Fwd*PivotForward + Up*PivotUp
  θ     = Lerp(ArcStartDeg, ArcEndDeg, t)          Yaw
  φ     = Lerp(ArcPitchStartDeg, ArcPitchEndDeg, t) Pitch — Yaw와 같은 t로 함께 보간한다
  Dir   = (Fwd*cos θ + Right*sin θ)*cos φ + Up*sin φ
  Prev ← Curr
  SwordRootCurr = Pivot + Dir * BladeInner
  SwordTipCurr  = Pivot + Dir * BladeOuter
```

- 첫 프레임 `Prev == Curr` 축퇴는 판정 프로세서가 이미 선분-선분 폴백으로 처리한다.
- 스윙 엔티티는 `NotifyBegin` 대신 **Active 진입 시 생성**, Active 종료 시 파괴.
  `TimeToLive` 안전장치는 그대로 살린다 — 프로세서가 종료를 놓쳤을 때의 그물이 된다.
- 채워야 하는 필드: `InstigatorEntity`(적 엔티티), `InstigatorTeam = Enemy`,
  `InstigatorActor = nullptr`, `bIsLocalInstigator = false`.
- 기울임은 **고정 Right축 회전이 아니라** 접평면 성분을 Up으로 들어 올려 만든다(위 식).
  고정 축으로 돌리면 Yaw가 ±90°에 가까울 때 축과 방향이 겹쳐 회전이 사라진다.
  기저와 이 식은 `Enemy/LNPEntityAttackShared.h`에 있고 **게스트 Ghost 경로가 같은 함수를 부른다.**

**칼날 갱신은 2패스다.** 칼날은 적과 **다른 엔티티**라 서로의 프래그먼트에 임의 접근할 수 없다 —
Pass 1(적 쿼리)이 위상을 진행하며 4점을 계산해 모으고, Pass 2(칼날 쿼리)가 핸들로 매칭해 기록한다.

⚠️ **칼날 마커는 Tag가 아니라 Fragment(`FLNPEntitySwingFragment`)다.** 칼날은
`FMassCommandBuildEntity` 한 번으로 만들어야 하는데, `BuildEntity`와 `AddTag`를 같은 배치에 디퍼드하면
아키타입 전환 타이밍 때문에 쿼리가 그 엔티티를 못 찾는다(`UANS_LNPMeleeHitWindow`도 같은 이유로 Tag를 안 쓴다).

**이 접근을 택하는 이유는 패링이다.** 순간 원뿔·구 판정으로 때우면 `JudgePlayerTarget`의 패링→가드→피격
2단계를 새로 짜야 하고, 그것은 [TechDesign_HitDetection.md](TechDesign_HitDetection.md) §7.5가
"PvP 쪽에만 패링 체크가 누락됐던" 사고로 못 박아 둔 **분기 복제 함정의 재생산**이다.

⚠️ **서버에서만 스윙 엔티티를 만든다.** ANS는 서버·클라 양쪽에서 만들지만 그것은 로컬 공격자
예측용이다. 엔티티 NPC는 예측 대상이 아니다.

### 4.4 원거리

`SpawnProjectile`의 Actor 의존 4곳을 대체한다.

| Actor 의존 | 순수 엔티티 대체 |
|:---|:---|
| `Muzzle` 소켓 위치 | 캡슐 중심 + `MuzzleLocalOffset` (조준선 기준점이 이미 캡슐 중심이다) |
| `GetBaseAimRotation()` | 접평면 전방 + `AimPitchMin/MaxDeg` 클램프된 Pitch |
| `ComputeDamage()` (ASC) | `FLNPEntityAttackConfig::Damage` |
| `Multicast_SpawnGhostProjectiles` | **트랙 B의 상태 채널로 대체** (§5.4) |

발사 시점은 **Windup 종료 시 1회**다. `ActiveTime`은 근접 전용이고 원거리에서는 후딜 시작점일 뿐이다.

⚠️ **`MuzzleLocalOffset.X`는 캡슐 반경보다 커야 한다.** 캡슐 안에서 스폰하면 발사체가 자기 몸에 닿아
즉시 파괴된다(적 팀 발사체는 적에게 피해를 주지 않고 소멸만 한다).

**조준점은 타겟의 캡슐 중심 + `AimTargetUpOffset`이다.** 캡슐 중심은 반높이(96cm) 지점이라 서 있는
캐릭터에서는 **골반**이고, 그대로 겨누면 "하반신을 노리는" 그림이 된다. 좌표 규약이 캡슐 중심이라는
사실과 "사람이 겨누는 곳"이 다르다는 점을 데이터로 흡수하는 자리다.

**산탄은 어빌리티와 공식을 공유한다.** 배치는 `LNPSpread::BuildHexRingDirections`
(`HitDetection/LNPSpreadPattern.h`)를 부른다 — `ULNPAbility_RangedSpreadAttack`이 쓰던 코드를 그대로
꺼낸 것이다. 복제했다면 중력 Up 기준 직교 기저와 짐벌 수렴 방지(→
[TechDesign_Ability.md](TechDesign_Ability.md) §3.2)가 한쪽에만 남는 사고를 재생산했을 것이다.

발사 식별자(SalvoID)의 규약은 게스트 Ghost와 짝이므로 §5.4에 모아 두었다.

예측 사격(리드샷)은 하지 않는다 — 파라미터가 하나 늘고, 순수 엔티티의 담당 영역은 정의상
"단순한 기본 공격"이다. 필요해지면 `ActorPromoted` 쪽 어빌리티의 일이 된다.

### 4.5 중단 규칙

| 사유 | 처리 |
|:---|:---|
| 경직·다운 진입 | 위상 즉시 `None`, 칼날 엔티티 파괴, 쿨다운 유지 |
| 사망(`FLNPEnemyDyingTag`) | 쿼리에서 통째로 빠진다 — 칼날은 `TimeToLive`가 회수한다(아래) |
| 타겟 상실 | Windup 중이면 취소, Active 이후면 끝까지 재생 (헛스윙이 자연스럽다) |

⚠️ **경직 판별은 `bIsGroggy || ImmunityTimeRemaining > 0`이다.** 다운은 게이지와 플래그를 0으로
내리므로 `bIsGroggy`만 보면 **다운 중인 적이 그대로 공격한다.** `ULNPEnemyActionProcessor`도 같은 규약을
쓴다 (→ [TechDesign_Poise.md](TechDesign_Poise.md) §5).

⚠️ **사망은 `DestroySwing`을 타지 않는다.** `AttackQuery`가 `FLNPEnemyDyingTag`를 `None`으로 걸고 있어
죽는 순간 그 엔티티가 쿼리에서 빠지기 때문이다. 남은 칼날은 좌표가 갱신되지 않은 채
`TimeToLive`(= `ActiveTime + 0.2초`)까지 제자리에 살아 있다가 회수된다 — 여기서 `TimeToLive`는
그물이 아니라 **1차 방어선이다.**

⚠️ **경직 취소 경로를 반드시 새로 넣어야 한다.** Actor 경로에서는 `FLNPStaggerCommand::Run`이
`CancelCurrentAttackAbility()`로 끊지만, 그 경로는 *"Actor가 없는 Low LOD 적은 연출도 어빌리티도 없다"* 며
피격자 Actor 캐스트에서 조기 반환한다(`LNPPoiseTypes.cpp`). **끊어 줄 주체가 아무도 없다.**
`ULNPEntityAttackProcessor`가 매 프레임 `FLNPPoiseFragment`를 읽어 스스로 끊는 것이 유일한 경로다.

### 4.6 감수하는 한계

| 항목 | 판단 근거 |
|:---|:---|
| 동적 수치 — 버프·디버프로 공격력·경직력이 변하지 않는다 | 적 GAS 버프는 원래 백로그 항목이다(Actor 풀 반납 시 GE 소멸). 여기서 새로 생기는 제약이 아니다 |
| 콤보 — 콤보 인덱스별 경직력·넉백 없음 | 단타 고정 |
| Lag Compensation — `RewindSeconds = 0` | **이미 그렇다** — 공격자에 PlayerController가 없으면 되감기가 0이다 |
| 클라이언트 예측 없음 | 예측은 로컬 공격자 전용 개념 |
| 공격자 HitStop 없음 | 순수 엔티티가 *때릴* 때 자기 재생을 멈추는 것은 없다. 반대로 **플레이어가 순수 엔티티를 때릴 때의 HitStop은 정상 동작한다**(§4.7) |
| 그로기와 다운의 연출 구분 없음 | 둘 다 `Action::Stagger` 하나로 나간다(`ActorPromoted`는 Light/Heavy 몽타주가 갈린다). 행동 상태 값을 하나 더 쓸 만큼 그림 차이가 크지 않다 — 필요해지면 3비트에 두 자리가 남아 있다 |
| 랙돌 없음 | 사망 시퀀스 + **사망 팝**으로 대체 (§8) |
| 상하 조준 **자세**가 복제되지 않는다 | **발사 방향은 클램프된 Pitch를 쓴다**(§4.4) — 없는 것은 게스트가 보는 *자세*뿐이다 (§5.5) |

**해소된 칸 셋** — 셋 다 한동안 "감수하는 한계"로 잘못 남아 있었다.

- **적이 피격자인 방향** ✅ 2026-09-07. 이전 판은 *"임팩트 VFX와 넉백이 피격자 Actor/ASC를 경유하는데
  순수 엔티티에는 둘 다 없다"* 며 미해결로 두었다 → §4.7.
- **피격 리액션** ✅ 2026-09-07. Stagger 시퀀스로 재활용한다. ⚠️ 그 전까지 Stagger 액션은
  그로기에서만 나갔고 **평타 피격에는 아무 반응이 없었다** → §4.7.
- **월드 HP Bar** ✅ 2026-09-09. 월드 스페이스가 아니라 **스크린 스페이스 커스텀 Slate 마커**로 붙었고,
  승격 Actor의 옛 `UWidgetComponent` 경로는 함께 삭제했다 (→ [TechDesign_HUD.md](TechDesign_HUD.md) §11).

### 4.7 적이 피격자인 방향 (2026-09-07 해소)

§4.6이 오래 비워 두었던 칸이다. 위 항목들은 전부 *"순수 엔티티가 공격자"* 인 방향이고,
**반대 방향에는 별도의 구멍이 다섯 개** 있었다 — 전부 같은 뿌리에서 나왔다:
**반응이 피격자 Actor/ASC를 경유하는데 순수 엔티티에는 둘 다 없다.**

| 빠져 있던 것 | 어디서 사라졌나 | 해결 |
|:---|:---|:---|
| 원거리 착탄 이펙트 | 커맨드가 피격자 ASC를 못 찾아 `continue` | 전송 ASC를 피격자 → 공격자로 폴백 (→ [TechDesign_HitDetection.md](TechDesign_HitDetection.md) §6.2) |
| 근접 임팩트 이펙트 | 데미지 커맨드 안에 묶여 있어 GE를 안 타면 함께 사라짐 | 연출을 데미지에서 분리 (같은 문서 §6.1) |
| 공격자 HitStop(근접) | 위와 같음 + 클라 예측이 Actor 없는 적을 후보에서 제외 | 위와 같음 + 예측 후보 필터 완화 |
| 피격 넉백 | `FLNPEnemyVelocityFragment`가 *"넉백용"* 으로 선언만 되고 **쓰는 코드가 없었다** | `LNPHitDetection::ApplyEntityKnockback` 단일 헬퍼 |
| 폭발 스플래시 피해 | `ApplySplash`가 Actor 없는 적을 통째로 건너뜀 | 직격 분기와 같은 형태로 |

⚠️ **넉백에는 Up 성분이 반드시 섞여야 한다.** 이동 프로세서의 공중 분기는 새 위치가 접지 반지름을
넘는 순간 표면에 스냅하고 속도를 0으로 만든다 — 순수 접평면 속도는 **한 프레임 만에 흡수돼
아무것도 보이지 않는다.** 근접 패링 넉백이 쓰던 가중(수평 0.7 : 위 0.3)을 그대로 재사용한다.

#### 피격 플린치 — 값을 늘리지 않고 경직 연출을 재활용한다

`FLNPEnemyFragment::FlinchTimeRemaining`(`ULNPEnemyConfig::PureEntityFlinchTime`, 기본 0.35초)이
0보다 크면 행동 상태가 `Stagger`로 나간다. **`ELNPEnemyAction`에 값을 늘리지 않는다** —
재활용하기로 한 모션이 이미 그 값에 매핑돼 있어 새 값이 그림을 하나도 늘리지 않고,
3비트에 두 자리밖에 남지 않았다 (§5.2).

우선순위는 `Dying > Parried > 그로기 > **Attack** > 플린치 > Move/Idle`이다
(`ULNPEnemyActionProcessor`). 순서 자체가 두 군데서 규약이다.

⚠️ **플린치를 공격보다 아래에 둔다** — 칼날이 살아 있는데 몸만 움찔하면 *"칼이 안 닿았는데 맞는다"* 가 된다.
경직이 공격 위에 있는 것과는 이유가 반대다(그쪽은 실제로 공격을 끊는다).

⚠️ **`Parried`를 그로기보다 먼저 본다.** 패링은 `ApplyParryBreak`으로 경직도 함께 밀어 넣으므로
(→ [TechDesign_Poise.md](TechDesign_Poise.md) §8), 순서를 바꾸면 `Parried`가 **한 번도 나가지 못하고**
전부 `Stagger`로 덮인다.

⚠️ **이미 플린치 중이면 타이머만 갱신하고 상태는 건드리지 않는다.** 매 탄마다 전이를 새로 만들면
① 모션이 끊겨 보이고 ② `Stagger`는 `IsOneShot`이라 복제 갱신 주기 게이트를 우회하므로(§5.6)
**연사가 그대로 대역폭이 된다.** 연사 한 묶음이 진입 1 + 이탈 1로 끝나는 것이 의도다.

#### 공격 중 이동·회전 잠금

`ULNPEnemyMovementProcessor`가 `FLNPEntityAttackFragment::Phase`를 읽어
**Windup부터 이동 정지, Active부터 회전 고정**한다. Actor 경로에서는 공격 몽타주의
`ANS_LNPBlockMovementInput`이 같은 일을 하지만, 몽타주가 없는 순수 엔티티에는 이것이 유일한 경로다.

회전을 Windup이 아니라 **Active부터** 잠그는 이유는 §4.1이 선딜을 *"플레이어가 읽고 반응할 구간"* 으로
정의했기 때문이다 — 선딜에는 조준이 따라와야 하고, 칼날이 살아 있는 동안 몸이 돌면
가상 칼날이 플레이어를 따라와 **회피 자체가 무효가 된다.**

⚠️ `ActorPromoted`는 `ULNPEntityAttackProcessor`가 청크 단위로 조기 반환해 `Phase`가 영원히 `None`이다 —
**모드 분기 없이 순수 엔티티에만 적용되는 이유가 이것이다.**

---

## 5. 트랙 B — 행동 상태 채널

### 5.1 왜 이벤트가 아니라 상태인가

순수 엔티티의 공격은 **서버 전용 Mass 로직**이고, 트랙 B 이전의 복제 페이로드
`FLNPReplicatedAgent`에는 **위치 + 접평면 Yaw**뿐이었다 — 클라이언트는 이 개체가 공격 중인지 알 방법이 없다.

발사 이벤트를 Multicast RPC로 쏘는 방법도 있지만 택하지 않는다 — **RPC 수 = 발사 수 × 개체 수**가 되어
"순수 엔티티는 다수"라는 이 설계의 전제와 충돌한다. **다수를 전제하는 순간 연출은 개별 이벤트가 아니라
상태 복제로 흘러야 한다.**

⚠️ 페이로드에 타입 식별자를 따로 넣지 않는다 — 베이스 `FReplicatedAgentBase`의 `TemplateID`가 이미 하고
있고, 그것으로 스폰된 클라이언트 엔티티는 **서버와 같은 템플릿**이라 `FLNPEnemySharedFragment`
(→ `ULNPEnemyConfig`)를 그대로 갖는다. 게스트는 Config를 **직접 읽는다**(§5.4가 그 위에 선다).

### 5.2 인코딩 — 1바이트

```cpp
// 3비트 = 상한 8값. 현재 6값이라 **두 자리 남았다.** ⚠️ 새 값은 반드시 끝에 붙인다 —
// 중간에 끼우면 저장된 ULNPEnemyConfig::ActionSequences(enum 키 맵)가 조용히 다른 행동을 가리킨다.
UENUM() enum class ELNPEnemyAction : uint8 { Idle, Move, Attack, Stagger, Dying, Parried };

// FLNPReplicatedAgent에 추가된 필드 — 셋 다 PositionYaw의 **형제 멤버**다
uint8 ActionAndSeq;   // 상태 3비트 + 전이 카운터 5비트
int8  AimPitch;       // 발사 순간의 상하 조준각 (∓90도를 int8 전 범위에, 약 0.7도) — §5.4
uint8 HealthPct;      // HP 비율 0~255 — 나중에 들어왔다 (§5.5)
```

⚠️ **형제 멤버로 두는 것이 인코딩의 절반이다.** `FStructNetSerializer::SerializeDelta`가
구조체 멤버마다 "같음" 1비트를 쓰므로(→ [Guide_NetBandwidth.md](Guide_NetBandwidth.md) §2.3),
형제로 두면 값이 안 바뀐 갱신에서 **1비트로 접힌다.** `PositionYaw` 안에 중첩하면 위치가 바뀔
때마다 함께 실려 나간다.

- **전이 카운터가 반드시 필요하다.** 상태 값만 보내면 연속 공격(Attack → Attack)의 두 번째 시작을
  놓친다 — 값이 바뀌지 않기 때문이다. 상태가 전이할 때마다 카운터를 1 올리고, 클라는 **카운터가 바뀌면**
  해당 시퀀스를 처음부터 재생한다. 5비트(0~31) wrap으로 충분하다.
- **재생 시각(타임스탬프)은 보내지 않는다.** 복제 갱신 주기가 0.1~0.3초라 위상 동기는 어차피 근사이고,
  4바이트가 추가된다. 클라는 "수신 시점부터 재생"으로 근사한다 — 판정은 이미 서버 권위이므로
  위상이 어긋나도 게임플레이에 영향이 없다.
  (엔진의 같은 계열 구조체 `FReplicatedAgentPathData`는 `ActionServerStartTime`을 double로 싣는다.
  거기서 갈라지는 지점이다.)

**실측 결과는 §10.2에 모아 두었다.** 요지 하나만 여기 남긴다 —
⚠️ **이 문서가 처음 적었던 "약 8% 증가"는 갱신 1회당 페이로드 기준이고 델타 압축을 빼고 센 값이다.**
실제 영향은 그보다 두 자릿수 배 작았다. **비용은 바이트 수가 아니라 갱신 횟수로 세야 한다.**

### 5.3 서버·클라 단일 소비 경로

```
[서버]  프로세서가 FLNPEnemyActionFragment에 기록 ─┬─▶ (리슨 호스트) 애니 프로세서가 읽음
                                                    └─▶ 복제 페이로드에 실림
[클라]  버블 핸들러가 수신값을 같은 Fragment에 기록 ──▶ 애니 프로세서가 읽음
```

애니 프로세서에는 **넷 모드별 값 분기가 없다.** 호스트용·게스트용 경로를 따로 두지 않는다는 뜻이고,
그것이 의도다 — "양쪽이 같은 입력을 보고 같은 그림을 그린다"가 이 채널의 존재 이유다.
다만 *실행 여부*는 가른다: `ExecutionFlags = Client | Standalone`이라 그리지 않는 데디 서버에서는
아예 돌지 않는다(엔진의 소비 프로세서도 같은 플래그다). 서버 전용 판정 프로세서들이
`LNPMass::IsClientWorld()` 가드로 시작하는 것과 정확히 반대 방향이다.

⚠️ **`AddEntityCallback`(스폰 시드)과 `ModifyEntityCallback`(갱신) 양쪽 모두 채워야 한다.**
시드를 빠뜨리면 버블에 새로 들어온 적이 실제 상태와 무관하게 `Idle`로 시작해 다음 전이까지 굳어 보인다.
(`FLNPMassFastArrayItem`의 규약 — *"멤버가 바뀌면 반드시 Dirty 표시할 것"*.)

⚠️ **리플리케이터 쿼리에 넣을 때 이 프래그먼트는 반드시 Optional이다.**
`ULNPMassReplicator::AddRequirements`는 세 리플리케이터가 공유하므로, 하드 요구로 넣으면
이 프래그먼트가 없는 Player·LootPod 청크가 복제 쿼리에서 통째로 빠진다.

### 5.4 발사체 관전 가시성

클라이언트 전용 프로세서가 `ULNPGhostProjectileSubsystem::SpawnSpectatorGhosts`를 **스스로 호출한다.**
필요한 인자(무기 상수·속도·수명)는 전부 자기 엔티티의 `FLNPEnemySharedFragment` Config에서 나오고,
발사 위치·방향은 복제된 위치·Yaw·조준각에서 파생된다. **새 RPC가 필요 없다.**

⚠️ **버블 핸들러 안에서 직접 스폰하지 않는다.** 수신 콜백은 Mass 실행 컨텍스트 밖이고,
§5.3의 "같은 프래그먼트를 읽는 단일 소비 경로" 규약과도 어긋난다.

#### 한 번의 공격은 전이를 **두 번** 만든다

```
Move -> Attack   (Seq+1)   선딜 시작. 게스트는 자세만 바꾸고 발사하지 않는다.
Attack -> Attack (Seq+1)   발사. 서버가 이 순간 조준각을 확정해 함께 싣는다.
```

⚠️ **공격 상태 진입에서 고스트를 만들면 두 번 틀린다.** 그 시점은 선딜 시작이라
① 고스트가 선딜 길이(기본 0.35초)만큼 앞질러 날아가고 ② 서버가 **아직 조준각을 계산하지도 않았다.**
그래서 발사를 별도의 전이로 만들고, 게스트는 **직전에 소비한 행동이 이미 `Attack`이었을 때만**
발사로 읽는다. 두 전이가 한 갱신에 뭉쳐 도착하면 그 발사의 고스트를 건너뛴다 —
**없는 탄이 생기는 것보다 안 보이는 편이 안전하다**(§5.6이 감수하기로 한 스킵과 같은 성질).

#### ⚠️ Ghost 키는 결정론적으로 유도한다

`ULNPGhostProjectileSubsystem`은 `FLNPGhostKey{PlayerID, KeyOrSalvo, SpawnIndex}` **정확 일치**로만
고스트를 파괴한다. 서버가 쓰던 `IssueServerSalvoID()`는 전역 카운터라 복제되지 않으므로, 게스트가
임의 키로 만들면 **서버 임팩트 큐가 그 고스트를 못 찾아 관통해 날아간다.**
→ 양쪽이 `f(FMassNetworkID, 전이 카운터)`로 같은 키를 스스로 유도한다(`LNPEntityAttackShared.h`). **추가 대역폭 0.**

- **키는 한 번의 발사에 하나다.** 펠릿마다 새로 발급하면 같은 발사의 펠릿이 서로 다른 발사로 잡힌다 —
  펠릿 구분은 `SpawnIndex`가 맡는다.
- 키 공간이 겹치면 안 된다 — 예측 키는 0~65535, `IssueServerSalvoID`는 65536부터이므로 **음수 영역**을 쓴다.
- `FMassNetworkIDFragment`는 복제 트레이트가 붙이므로 **Standalone에는 없다.** 그때만 전역 카운터로 돌아간다.

**감수하는 오차 (전부 코스메틱 — 임팩트 지점은 서버 큐가 확정한다):** 발사 지점과 수평 방향이
게스트 보간값이라 미세하게 어긋나고(`Multicast_SpawnGhostProjectiles`가 이미 감수하는 것과 같은 종류),
발사 시각을 싣지 않아 Dead Reckoning 업스트림 지연이 0이다(수신자 RTT/2만 적용).

- ⚠️ **총구는 조준각과 무관한 고정 오프셋이다**(캡슐 중심 기준). 캡슐 중심이 골반 높이라 총구는
  허리쯤이고, 조준 자세가 없는 ISM 상태에서는 **위로 쏘면 하반신에서, 아래로 쏘면 상반신에서**
  나오는 것처럼 읽힌다(2026-09-05 관찰). 스폰 지점이 실제로 움직이는 것은 아니다 —
  총구 높이는 `MuzzleLocalOffset.Z`로, 나머지는 트랙 C의 조준 자세로 해소된다.

### 5.5 무엇을 넣지 않는가 (지금은)

피격 방향 — **뺀다.** 플레이 테스트에서 "이게 없어서 못 읽겠다"가
확인된 것만 골라서 넣는다. 상태 채널은 한 번 넓히면 좁히기 어렵고, 대역폭은 개체 수에 곱해진다.

**HP 비율은 그 절차를 거쳐 들어왔다 (2026-09-09).** 처음엔 빼고 내보냈고, 순수 엔티티의 HP를
클라이언트가 아는 경로가 **아예 없다**는 것이 적 HP 바를 붙일 때 하드 블로커로 드러나 추가했다
(`uint8 HealthPct`, → [TechDesign_HUD.md](TechDesign_HUD.md) §11.3). Aim Pitch와 같은 성질이다 —
**피격할 때만 바뀌므로** 나머지 갱신에서는 델타 압축이 1비트로 접는다.

**Aim Pitch는 그 절차를 거쳐 들어왔다 (2026-09-05).** 처음엔 빼고 내보냈고, 2P 고저차 교전에서
*"게스트 화면에선 충분히 피했는데 서버에선 맞고, 피격 모션과 HP 차감까지 실행된다"* 가 확인되어 추가했다.
값이 **발사 순간에만 바뀌므로** 나머지 갱신에서는 델타 압축이 1비트로 접는다 — 넓히더라도
"매 갱신 변하지 않는 축"을 고르면 비용이 거의 붙지 않는다는 것이 이 사례의 교훈이다.

### 5.6 함정 — 복제 주기 vs 공격 길이

복제 LOD가 Low(0.3초)인 거리에서 짧은 공격(총 1.0초)은 시작과 끝이 두 갱신 사이에 들어가
**통째로 스킵될 수 있다.** 전이 카운터가 "전이가 있었다"는 사실은 알려주지만 이미 늦은 시점이다.

**해법 — 일회성 전이만 게이트를 우회한다 (채택, 2026-09-05).**

`Attack`·`Stagger`·`Parried`·`Dying` **진입**은 `UpdateInterval` 게이트를 건너뛰고 즉시 Dirty를 건다.
`Idle <-> Move`는 게이트를 그대로 탄다. 판별 원본은 `FLNPEnemyActionFragment::IsOneShot()` 하나이며,
ISKM의 Loop/Clamp도 같은 함수에서 파생한다(§6.4).

⚠️ **전이 전부를 우회시키면 안 된다.** 루프 상태는 늦게 도착해도 그림이 같은 반면,
멈췄다 걷기를 반복하는 배회 개체는 `Idle <-> Move` 전이를 초당 여러 번 만들어 갱신 수를
통제 없이 밀어올린다. **일회성/루프 구분 하나가 스킵과 플랩을 동시에 막는다.**

같은 이유로 `Idle <-> Move` 판별에는 **실제 변위 기준 히스테리시스**(진입 40 / 이탈 15 cm/s)를 둔다.
이 데드밴드는 연출 장치이자 대역폭 장치다 — 실측에서 개체당 전이가 **중앙값 0.26회/s
(최대 0.31, 연속 전이 간격 중앙값 3.03초)** 로 억제됐다.

감수하기로 했던 스킵은 남지만(두 전이가 한 갱신에 뭉치면 그 발사의 고스트를 건너뛴다)
체감 문제로 보고되지 않았다. ⚠️ **타임스탬프를 추가하는 방향으로는 가지 않는다** — 대역폭이
늘고 스킵 문제는 그대로 남는다.

---

## 6. 트랙 C — ISM ↔ ISKM 하이브리드

### 6.1 엔진 기반 (UE 5.8)

ISM만으로는 스켈레탈 애니메이션이 불가능하다. 그러나 **5.8의 MassRepresentation은
`SkinnedMeshInstance`를 정식 표현 타입으로 갖고 있다.**

| 요소 | 클래스 / 구조체 |
|:---|:---|
| 표현 타입 | `EMassRepresentationType::SkinnedMeshInstance` |
| Trait 필드 | `UMassVisualizationTrait::SkinnedMeshInstanceDesc` — 현재 쓰는 `UMassCrowdVisualizationTrait`이 이미 상속한다 |
| 필드 타입 | `FSkinnedMeshInstanceVisualizationDesc` (**`Meshes` 배열** + `bUseTransformOffset`/`TransformOffset` + `CustomDataFloats`) |
| 배열 원소 | `FMassSkinnedMeshInstanceVisualizationMeshDesc` (Asset / TransformProvider / Min·MaxLODSignificance / MaterialOverrides / bCastShadows) |
| 애니 데이터 | `FMassRepresentationAnimationFragment::AnimData` |
| 애니 주입 | `FMassInstancedSkinnedMeshInfo::AddBatchedAnimationData(FAnimSequenceTrackAutoPlayData)` |
| 소비 프로세서 | `UMassConsumeInstancedSkinnedMeshAnimationProcessor` (PrePhysics · `Representation` 그룹 · `Client\|Standalone`) |
| 백엔드 | `UInstancedSkinnedMeshComponent` + `UAnimSequenceTransformProviderData` (GPU 전용) |

프로세서가 채워 넣는 값은 `FAnimSequenceTrackAutoPlayData`의 다섯 필드뿐이다 —
`SequenceIndex` · `Position` · `PlayRate` · `BlendTime` · `LoopMode`.
`BlendTime`이 있으므로 Idle ↔ Move ↔ Attack 전환에 블렌드까지 걸린다.
우리가 만든 것은 `ULNPEnemyAnimationProcessor` 하나 — **행동 상태 → `SequenceIndex` 매핑**이다.

⚠️ **`FMassRepresentationAnimationFragment`는 엔진 트레이트가 붙여 주지 않는다.**
`UMassVisualizationTrait::BuildTemplate`은 `FMassRepresentationLODFragment`까지만 넣는데, 소비 프로세서는
이 프래그먼트를 **필수 요구**로 건다. 안 붙이면 그 쿼리가 **아무 엔티티도 매칭하지 않아** 경고 하나 없이
그냥 안 움직인다(엔진의 유일한 선례 `MetaHumanMassCrowdVisualizationTrait`도 직접 붙인다).
그래서 `ULNPEnemyTrait::BuildTemplate`이 `CombatMode`와 무관하게 **전원에게** 붙인다.

⚠️ **엔진은 `SequenceIndex`가 바뀔 때만 트랙을 다시 앵커링한다**(`MassVisualizationComponent`).
매 프레임 같은 값을 써도 안전하다는 뜻이면서, **중간 상태 없이 같은 행동을 반복하는 채널을 나중에
추가하면 그 반복이 안 보인다**는 뜻이기도 하다. 지금은 공격이 반드시 다른 상태를 경유해 재진입하므로
(§5.3의 전이 규약) 걸리지 않는다.

### 6.2 유의도 구간 분배

| 구간 | 표현 | 근거 |
|:---|:---|:---|
| High (근거리) | ISKM | 공격 모션이 읽혀야 하는 거리 |
| Medium | ISKM (그림자 off) | 실루엣과 동작만 |
| Low | ISM | 점처럼 보이는 거리에서 스키닝 비용을 낼 이유가 없다 |
| Off | 없음 | `ULNPEnemyTrait::ReplicationCullDistance`와 값을 맞출 것 (기존 규칙) |

**이 분배는 코드가 아니라 데이터로 조정되고, 손잡이가 둘이다** — 무엇으로 그릴지는
`Params.LODRepresentation[EMassLOD]`가, 한 표현 안에서 어떤 메시 항목을 쓸지는 항목별
`Min/MaxLODSignificance`가 정한다.

실제 구성(2026-09-06, `DA_EnemyEntityConfig_PureEntity_*`): `LODRepresentation = [ISKM, ISKM, ISM, None]`.
Medium을 별도 항목으로 가르지 않았다 — ISM 쪽이 이미 `bCastShadows = false`라 ISKM도 같게 두었고,
지금은 유의도 범위 하나(0~Max)로 전 구간을 덮는 메시 항목 하나뿐이다. 그림자를 켜면 그때 둘로 가른다.

⚠️ **Low를 ISM으로 남긴 것은 대조군을 겸한다.** ISKM이 안 보일 때 "멀어지면 보인다"가 곧
"렌더 경로 문제이지 엔티티 문제가 아니다"의 증거가 된다 — 5a에서 실제로 그렇게 썼다.

⚠️ **`PureEntity`용 EntityConfig는 `LODRepresentation`에서 Actor 단계를 없애고 템플릿 Actor를 비워야 한다.**
`ULNPEnemyLODOverrideProcessor`가 High 강제를 건너뛰어도, 가까우면 거리 기반 LOD가 자연히 High가 되고
표현 매핑에 Actor가 있으면 그대로 스폰된다 — **"승격 안 하기로 한 개체가 가까이 가니까 Actor가 된다"** 를
실제로 밟았다(2026-09-05). 단일 진실은 enum이고 EntityConfig는 그 모드에서 쓸 비주얼만 정의한다.
어긋남은 `ULNPEnemyTrait::ValidateTemplate`이 **양방향으로** 경고한다(PureEntity인데 Actor가 남았다 /
ActorPromoted인데 Actor가 없다). `false`는 돌려주지 않는다 — 어긋남은 고쳐야 할 설정이지 스폰을 막을
사유가 아니다. (트레이트는 `GetTypedOuter<UMassEntityConfigAsset>()`로 부모 체인의
`UMassVisualizationTrait::Params`를 읽는다. `BuildContext`의 템플릿 데이터가 protected이기 때문이다.)

⚠️ **차단을 코드로 강제하려던 접근은 폐기했다.** LOD 값을 "Actor를 쓰지 않는 첫 단계"까지 눌러쓰는
방식을 먼저 시도했는데, **`FMassRepresentationLODFragment::LOD`는 표현뿐 아니라 유의도·틱 레이트까지
정하는 값**이라 표현 하나를 막으려고 나머지까지 끌어내리게 된다. 코드는 "전투로 LOD를 끌어올리지
않는다"까지만 하고, 무엇으로 그릴지는 이미 그것을 데이터로 갖고 있는 트레이트에게 맡긴다.

### 6.3 무기 — 손 본 웨이팅 스킨드 메시

ISKM에는 소켓 본 어태치가 없다. 대신 `Desc.Meshes`가 배열이고, 항목마다 ISKM 컴포넌트가 하나씩 생성되어
**동일 트랜스폼·동일 애니 재생 상태**를 받는다 (`MassVisualizationComponent`).

그러므로 무기를 **같은 스켈레톤의 손 본에 100% 웨이팅한 스킨드 에셋**으로 만들어 두 번째 항목으로 등록하면
GPU 스키닝이 무기를 손 위치로 옮겨 준다 — 엔진의 모듈러 캐릭터 방식과 같은 원리다.

- ⚠️ 무기 종류가 늘면 무기마다 스킨드 에셋을 구워야 한다. 다만 적은 **무기를 교체하지 않으므로**
  (`ULNPEnemyConfig::WeaponData` 고정 1개) 조합 폭발이 없다 — 이 제약이 여기서 이득이 된다.
  몸통과 병합한 메시를 따로 굽는 방식보다 낫다 — 캐릭터 메시와 LOD를 복제하지 않는다.
- ⚠️ **`BladeInner` / `BladeOuter`는 이 메시의 실제 길이와 손으로 맞춰야 한다.** 애니메이션에서 뽑을 수
  없으므로 Config 상수이고, 어긋나면 "칼이 안 닿았는데 맞는다"가 된다.
  `ULNPWeaponTraceDebugDrawProcessor`로 눈으로 맞출 것 — **cvar 게이트가 없어** 에디터 빌드에서
  플레이어 반경 5m(`ULNPSettings::DebugDrawProximityDistSq`) 안의 칼날이 자동으로 그려진다.
  대조는 **`slomo 0.1`** 로 한다(§6.5 끝).

#### 적용 현황 (2026-09-13, Stage 5c 완료)

무기는 **애니 세트** 단위로 묶는다. 세트 하나가 시퀀스 리스트(ASL) 1개와 ASTP 2개(몸통·무기)를 갖는다.

| 세트 | 적 | ASL | 몸통 ASTP (`Meshes[0]`) | 무기 ASTP (`Meshes[1]`) | 무기 메시 |
|:---|:---|:---|:---|:---|:---|
| 검 | Melee01 | `ASL_Enemy_UEFN_Sword` | `ASTP_Enemy_UEFN_Sword_Body` | `ASTP_Enemy_UEFN_Sword_Weapon` | `SKM_LongSword_UEFN` |
| 샷건 | Ranged01 | `ASL_Enemy_UEFN_Shotgun` | `ASTP_Enemy_UEFN_Shotgun_Body` | `ASTP_Enemy_UEFN_Shotgun_Weapon` | `SKM_Shotgun_UEFN` |

- 이름 규칙: `ASL_Enemy_UEFN_{세트}`, `ASTP_Enemy_UEFN_{세트}_{Body|Weapon}` (에셋은 `/Game/Enemy`).
- 세트는 **적 타입이 아니라 애니 세트**다. 같은 무기 자세를 쓰는 적끼리는 ASL·ASTP를 공유한다.
- 세트를 나누는 대가는 ISKM 컴포넌트 수다 — 프로바이더가 Desc 해시에 들어가므로 몸통이 같은 메시여도
  세트마다 컴포넌트가 따로 생긴다.

두 무기 모두 스켈레톤은 `SK_UEFN_Mannequin`(88본, 마네킹과 **본 배열이 인덱스까지 동일**),
웨이팅은 **`weapon_r` 100%**(소켓이 아니라 **본**), 빌드 세팅은 `bOptimizeForInstancing`,
소스 FBX는 `Art/Meshes/SKM_*_UEFN.fbx`(에셋의 `AssetImportData`가 가리켜 에디터 Reimport가 동작한다).

| 항목 | 롱소드 | 샷건 |
|:---|:---|:---|
| 메시 | `/Game/Weapons/LongSword/Mesh/SKM_LongSword_UEFN` (68정점) | `/Game/Weapons/Shotgun/Mesh/SKM_Shotgun_UEFN` (16,099정점) |
| 정점 배치 | `VS_LongSword`의 실제 어태치 값 — `weapon_r` 로컬 `(-5, 4, 2)` / `FRotator(P18, Y92, R-17)` | `VS_Shotgun` — `(0, 0, 0)` / `FRotator(P0, Y90, R-4)` |
| 등록 | `DA_EnemyEntityConfig_PureEntity_Melee01`의 `Meshes[1]` | `..._Ranged01`의 `Meshes[1]` |
| 비고 | 원본 `SKM_LongSword`는 실린더를 늘린 임시 에셋(전 면 스무스가 정상). 그립→칼끝 **118.4cm**, 폼멜 쪽 31cm | 머티리얼 `MI_Weapon_Shotgun` — 부모 `M_Weapon`에 `bUsedWithInstancedSkinnedMesh` 필수(§6.7) |

- ⚠️ **부품 본은 전부 `weapon_r`로 합쳤다 — 부품 움직임은 포기한다.** ISKM은 **마네킹 본의 구운 트랜스폼만**
  재생하므로 무기 자체의 본을 구동할 방법이 없다. 부품이 필요한 거리는 Actor 승격 구간이 담당한다.
- 컴포넌트 로컬 정점은 `inv(EMPTY.world) @ mesh.world @ co`로 뽑는다(두 무기가 같은 기준이라 결과가 일관된다).
- **비대칭 무기로 좌표 변환이 검증됐다.** 롱소드는 원통이라 축 회전이 틀려도 드러나지 않았는데, 샷건은 PIE에서
  방향과 왼손 위치(펌프 부근)가 맞았다 — 본 로컬 `diag(1,-1,1)` 규약이 롤까지 옳다는 뜻이다.
- `hand_r`에 웨이팅해도 된다 — 스킨드 메시는 **정점 위치 자체가 고정 오프셋**이라 칼이 손목으로 올라가지
  않는다(그건 소켓 어태치의 증상이다). 플레이어 칼과 같은 기준이라 그립 비교가 쉬워서 `weapon_r`을 쓴다.

##### ⚠️ 프로바이더는 메시마다 하나씩, 시퀀스 배열은 몸통과 똑같이

애니 재생 상태(시퀀스 인덱스·시간)는 항목들이 공유하지만, `TransformProvider`는 항목별로 설정되고
**자기 메시에 바인딩되어 있어야** 한다 — `UAnimSequenceTransformProviderData::IsValidFor()`가
`SkinnedAsset` 불일치를 등록 거부로 처리하고, 거부당한 메시는 **레퍼런스 포즈로 굳는다**
(로그는 `doesn't match SkinnedAsset on InstancedSkinnedMesh` 한 줄뿐이다).

무기용 ASTP는 **몸통 ASTP를 복제해 `SkinnedAsset`만 바꾼다.** 새로 만들면 §6.4의 인덱스를 손으로 맞춰야 하고,
빈 `Sequence` 항목을 한 순간이라도 두면 에디터가 죽는다(§6.7).

✅ **시퀀스 배열 동기화는 `UAnimSequenceTransformProviderSequenceList`(ASL)가 한다** (2026-09-13).
배열을 무기 세트별 ASL 하나에 두고 몸통·무기 ASTP가 `SequenceList`로 참조한다 — ASL을 편집하면
`PostEditChangeProperty`가 참조하는 모든 프로바이더에 배열을 밀어 넣고, 프로바이더 `PostLoad`도 ASL에서
다시 복사한다. ASL은 EditorOnly라 런타임 비용이 없다.

⚠️ **ASL을 도입한 이유:** 그전에는 몸통 ASTP의 시퀀스를 바꾸면 무기 ASTP도 손으로 맞춰야 했고,
어긋나도 에러·경고가 **전혀 없이** 몸과 칼이 서로 다른 클립을 재생했다. 2026-09-13에 Idle/Run을 교체한 뒤
"Attack에서는 맞는데 Idle·Run에서만 칼이 손에서 떨어지는" 증상으로 실제로 밟았다 — **`weapon_r` 본을
의심하기 전에 두 프로바이더의 시퀀스 배열부터 대조할 것.**

##### ⚠️ 한때 "ISKM에서 렌더되지 않는다"고 잘못 결론 냈다 (2026-09-12)

**증상:** 무기 메시가 ISM 구간과 Actor 구간에서는 보이는데 ISKM 구간에서만 투명했다.
**원인:** ISKM이 아니라 **메시의 바인드 포즈**였다(아래 규약의 ①·② 위반) — 애니메이션이 걸리는 순간
스키닝 행렬이 틀어져 메시가 터진다. ISM은 스키닝을 안 하고 Actor는 원본 마네킹 메시를 그리니 그쪽에서만
보였고, 레퍼런스 포즈로 그리는 썸네일도 멀쩡해 ISKM 경로를 열 가지 넘게 배제하며 하루를 버렸다.

> **판별은 메시 에디터에서 `Preview Animation` 한 번이면 끝난다.** 애니를 고르는 순간 메시가 사라지면
> 바인드 포즈 문제다. **ISKM에 등록하기 전에 반드시 이것부터 확인할 것.**

##### DCC 왕복 규약 — 좌표·스케일·바인드 포즈 (2026-09-12~13 실측)

추측으로 유도하려 들면 하루가 사라진다. **여기 적힌 값과 절차를 그대로 쓸 것.**

**좌표.** 블렌더 FBX 임포터는 기본 축에서 **본 보정 행렬을 만들지 않는다**(`bone_correction_matrix = None`).
흔히 기대하는 "FBX X축 → 블렌더 Y축" 재정렬이 없으므로, 남는 차이는 UE FBX 익스포터의 축 반전 하나뿐이다.

| 대상 | 변환 |
|:---|:---|
| 월드 좌표 | UE `+X`(전방) → 블렌더 `-Y` · UE `+Y`(오른쪽) → 블렌더 `-X` · `+Z` 보존 |
| **본 로컬 좌표** | **`diag(1, -1, 1)`** — Y만 반전 (`FFbxDataConverter::ConvertPos`의 규약) |

반전 축을 가르는 검증은 에셋 안에 있다. `palm_r_Socket`은 `hand_r` 로컬 **(-7.5, +2, 0)** 인데 블렌더에서 잰
`weapon_r`의 같은 본 로컬 Y는 **-3.41** 이다 — 손바닥과 그립점이 반대쪽일 수는 없다. (손끝 방향이 본 `-X`인
것도 이 소켓으로 안다. UE 마네킹의 손 본은 X축이 자식 방향이 **아니다.**)
회전은 `FRotationMatrix` 순서로 조립한 뒤 `T · L · T⁻¹`(T = 위 본 로컬 반전)로 켤레를 취한다.

**절차.**

1. 마네킹 FBX를 `automatic_bone_orientation=False`로 임포트한다. (MCP 실행 환경에서 활성 오브젝트가 없으면
   임포터가 `Context missing active object`로 죽는다 — 프리미티브를 하나 추가해 두고 임포트한 뒤 지운다.)
2. **아마추어 오브젝트 이름을 정확히 `root`로 둔다.** 아마추어 오브젝트 이름이 **곧 루트 본 이름**이다.
   같은 파일을 두 번 임포트해 붙는 `.001` 접미사가 그대로 `root_001` 본이 된다.
3. 아마추어의 부모(EMPTY, 스케일 `0.01`)를 **월드 트랜스폼을 유지한 채 해제만** 한다 → 아마추어가 `0.01`을
   떠안는다. ⚠️ **`transform_apply(scale=True)`는 금지(①).** 본 좌표를 미터로 구우면 바인드 행렬이
   "FBX SDK 검사는 통과하지만 실제 본 배치와는 어긋난" 상태가 되어 UE가 고칠 기회를 잃는다.
   씬을 cm로 맞추는 것도 금지 — 익스포터가 미터→cm를 한 번 더 적용해 정확히 100배가 된다.
4. 무기를 배치하고 전 정점을 본 하나에 100% 웨이팅, `Armature` 모디파이어를 `root`에 건다.
5. 익스포트는 `object_types={'ARMATURE','MESH'}` — ⚠️ **EMPTY 제외(②). FBX에서 EMPTY는 본으로 변환된다.**
   나머지는 `add_leaf_bones=False`, `apply_scale_options='FBX_SCALE_NONE'`, `global_scale=1.0`,
   `axis_forward='-Z'`, `axis_up='Y'`, `mesh_smooth_type='FACE'`.
6. `SkeletalMeshTools.import_file`에 `skeleton=SK_UEFN_Mannequin`을 준 뒤 **즉시 두 가지를 확인한다** —
   `get_bone_names`가 `root`부터 88개로 마네킹과 같은지, 그리고 아래 `LogFbx` 로그.

| 임포트 로그 | 의미 |
|:---|:---|
| `Not valid bind pose … Recreating bind pose succeeded` | **정상.** UE가 바인드 포즈를 재생성했다 |
| `Valid bind pose for Pose (…)` | **위험 신호.** `transform_apply`를 거친 메시에서 나온다 — 애니메이션에서 터진다 |
| `The Skeleton … is missing bones … They will be added now` | **스켈레톤 오염.** 본 이름이 틀렸다(②). **저장하지 말고 에디터를 재시작**할 것 — 원본 스켈레톤을 쓰는 메시·애니 전부가 dirty가 된다(실제로 겪었다) |

⚠️ `EditorAppToolset.CaptureAssetImage`의 썸네일은 **레퍼런스 포즈**라 메시가 깨졌는지는 잡지만 바인드 포즈
문제는 못 잡는다. 에셋을 바꿀 때마다 썸네일 → 애니 프리뷰 순으로 확인한다.

### 6.4 시퀀스 인덱스 규약

인덱스는 `UAnimSequenceTransformProviderData::Sequences`에 구워진 **배열 순서**다.
**애니 프로세서에 하드코딩하지 않고** `ULNPEnemyConfig::ActionSequences`(`TMap<ELNPEnemyAction, 인덱스 배열>`)가
매핑을 갖는다 — 적 타입마다 시퀀스 수와 순서가 다르다. 루프 여부는 데이터로 두지 않고
`FLNPEnemyActionFragment::IsOneShot()`에서 파생한다(§5.6과 같은 원본).

검 세트 (2026-09-13) — 시퀀스는 **`ASL_Enemy_UEFN_Sword`에서 편집**하고 참조하는 ASTP 둘이 따라간다(§6.3):

| Index | 시퀀스 | 길이 | 매핑 |
|:---:|:---|---:|:---|
| 0 | `A_SW_Idle_UEFN` | 11.33s | Idle (Loop) — 검 든 자세. 맨손 `M_Neutral_Stand_Idle_Loop`에서 교체 |
| 1 | `A_SW_Run_UEFN` | 0.73s | Move (Loop) — 검 든 자세. 맨손 `M_Relaxed_Run_Loop_F_Troy`에서 교체 |
| 2 | `A_SW_Attack_01_UEFN` | 2.47s | Attack (Clamp) — 사선 내려베기 |
| 3 | `A_SW_Attack_02_UEFN` | 2.00s | *미사용* — 올려베기라 아크와 방향이 반대다 |
| 4 | `A_SW_Damage_Backward_UEFN` | 1.07s | Parried (Clamp) |
| 5 | `MM_Death_Front_01_UEFN` | 1.10s | Dying (Clamp) |
| 6 | `A_SW_Damage_Fast_UEFN` | 0.57s | Stagger (Clamp) |
| 7 · 8 | `A_SW_Damage_Left/Right_UEFN` | 1.07s | *미사용* — §6.7의 제자리성 위반 |

⚠️ **인덱스는 위치 의존이다. 항목을 지우면 뒤가 전부 밀린다.** 2026-09-06에 중간 항목 하나를 지웠다가
경직·사망·패링 셋이 동시에 엉뚱한 모션을 가리켰다. **끝에만 붙이고, 지웠다면 매핑을 전수 재확인한다.**
미사용 항목을 지우지 않고 남겨 두는 이유가 이것이다.

샷건 세트 (2026-09-13) — `ASL_Enemy_UEFN_Shotgun`. **칸 배치를 검 세트와 같게** 두어 Ranged01의
`ActionSequences`가 같은 인덱스를 쓴다:

| Index | 시퀀스 | 길이 | 매핑 |
|:---:|:---|---:|:---|
| 0 | `MM_Shotgun_Idle_ADS_UEFN` | 3.40s | Idle (Loop) |
| 1 | `MM_Rifle_Jog_Fwd_UEFN` | 1.70s | Move (Loop) — `MM_Rifle_Walk_Fwd_UEFN`과 비교 중 |
| 2 | `MM_Shotgun_Fire_UEFN` | 0.67s | Attack (Clamp) — 원래 additive, `None`으로 전환(§6.7) |
| 5 · 6 | `MM_Death_Front_01_UEFN` · `A_SW_Damage_Fast_UEFN` | — | Dying · Stagger (검 세트와 같은 클립) |
| 3 · 4 · 7 · 8 | 검 세트와 같은 클립 | — | *미사용* — 칸만 유지 |

⚠️ **새 ASL은 빈 칸 없이 채운 뒤에 ASTP에 연결한다.** 연결된 ASL을 편집하면 참조하는 ASTP가 전부 다시 빌드되므로,
§6.7의 "빈 `Sequence` 항목이 있으면 에디터가 죽는다" 조건이 모든 프로바이더에 한꺼번에 걸린다.

#### 변형은 연출 다양성이지 재생 보장이 아니다

배열에 인덱스를 둘 이상 두면 `FLNPEnemyActionFragment::Seq`에서 유도해 번갈아 쓴다.
공격은 반드시 다른 상태를 경유해 재진입하므로(§5.3) **변형이 하나여도 재생은 보장된다.**

⚠️ **`Seq`를 그대로 나머지 연산하면 안 된다.** 공격 사이의 전이 수가 대체로 짝수(`Move → Attack → Move → Attack`)라
`Seq % 2`가 한쪽 패리티에 **고정된다** — 2026-09-06 실측에서 "1번만 반복하다 간격이 홀수인 순간 2번으로
넘어가 다시 고정"으로 나타났다. 곱셈 후 상위 비트를 내려(`(Seq * 2654435761u) >> 13`) 패리티 상관을 끊는다.
값이 여전히 `Seq`만의 함수라 **서버와 게스트가 같은 변형을 고른다.**

⚠️ **변형끼리 스윙 방향이 다르면 판정과 동시에 맞출 수 없다** — 아크 상수가 `FLNPEntityAttackConfig`에
**한 벌뿐**이다. 아크를 변형 항목으로 옮기는 선택지는 **폐기했다**: 그러면 "표현이 고른 변형"이 "판정 기하"를
정하게 되어 §6.5가 세운 의존 방향이 뒤집힌다. 같은 방향으로 베는 변형만 묶거나 변형을 하나로 둔다(현재는 후자).

### 6.5 애니 타이밍과 판정 타이밍의 단일 정의

⚠️ **AnimNotify가 없으므로 애니메이션에서 히트 타이밍을 뽑을 수 없다.**
Windup/Active/Recovery는 `FLNPEntityAttackConfig` 상수가 정의하고, 애니는 그 상수에 **맞춰 재생된다.**

```
PlayRate = SequenceLength / (WindupTime + ActiveTime + RecoveryTime)
```

- 이 관계는 데이터로만 묶여 있어 어긋나도 컴파일도 실행도 실패하지 않는다 — **조용히 어긋난다.**
- 그래서 `PlayRate`를 **프로세서가 계산**한다. 애니 속도를 상수에서 파생시키는 방향이지 그 반대가 아니다.
  Config를 고치면 그림이 따라온다.
- `SequenceLength`도 손으로 적지 않는다 — `SkinnedMeshDescHandle` → `FMassInstancedSkinnedMeshInfo` →
  `Desc.Meshes[n].TransformProvider` → `UAnimSequenceTransformProviderData::GetSequencePlayLength()`로
  **엔진이 가진 데이터에서 읽는다.** 애니를 갈아 끼워도 위상 합만 맞으면 속도가 따라온다.
- `FLNPEnemyMovementConfig::ComputeStopDistance()`가 정지 거리에 대해 하는 일과 같은 계열의 규약이다.

#### ⭐ PlayRate를 1.0에 맞추면 튜닝이 눈대중을 벗어난다

위상 합을 클립 길이와 같게 두면 `PlayRate = 1.0`이 되고, 그 순간 **애니메이션의 프레임 번호가 그대로 초가 된다.**
접촉 구간을 프레임으로 읽어 `프레임 / FPS`로 넣으면 끝이라, "대충 절반쯤"이 사라진다.

근접 실측 예 (`A_SW_Attack_01_UEFN`, 74프레임 @ 30fps = 2.4667초, 칼날 적합 구간 22~30프레임):

```
WindupTime   = 22/30 = 0.7333      (0 ~ 22프레임)
ActiveTime   =  8/30 = 0.2667      (22 ~ 30프레임 — 칼날 생존)
RecoveryTime = 44/30 = 1.4667      (30 ~ 74프레임 — 마무리)
합 = 2.4667 → PlayRate 1.000
```

- **클립 뒤쪽의 "안 쓰는" 프레임은 잘라낼 게 아니라 `RecoveryTime`에 배정한다.** 후딜이 원래 그 용도다.
  에셋 편집이 0이고 되돌리기도 쉽다.
- ⚠️ 위상은 **연출값이 아니라 게임플레이 값**이다. `WindupTime`은 플레이어가 읽고 반응하는 창이고,
  **원거리는 이 값이 끝나는 순간 발사한다.** 위상 합을 늘렸으면 `FLNPEnemyMovementConfig::AttackInterval`을
  줄여 공격 주기를 유지할지 함께 정할 것(쿨다운은 Recovery가 끝나는 순간 시작된다).

#### 아크는 Yaw와 Pitch를 함께 보간한다

기울기를 상수 하나로 고정하면 **일정 기울기의 수평 훑기**밖에 안 되어, 수직에 가까운 사선 베기 모션과는
궤적이 맞지 않는다. `ArcPitchStartDeg`/`ArcPitchEndDeg`로 쪼개 Yaw와 같은 `T`로 보간한다
(둘을 같게 두면 예전 동작 그대로다). 부호 규약은 접평면 기준 **양수가 위**(머리 쪽)다.

⚠️ **애니와 판정을 묶어 주는 것은 위상 합뿐이다.** 접촉 순간·아크 방향·피벗 높이는 파생시킬 수 없으므로
디버그 드로우를 보며 손으로 맞춘다(→ §9 #3). AnimNotify가 없다는 제약의 실제 대가가 여기다.

**롱소드 확정값 (2026-09-13, `DA_Enemy_PureEntity_Melee01`)** — 무기 메시가 붙은 뒤 칼 궤적에 맞췄다:

```
Yaw     ArcStartDeg  130  →  ArcEndDeg   -170     (범위 300°)
Pitch   ArcPitchStart 78  →  ArcPitchEnd  -78     (가파른 사선 내려베기)
Pivot   Forward 20 / Up 10          Blade  Inner 30 / Outer 140
위상    22f / 8f / 44f  (PlayRate 1.0)
```

손 궤적만 보고 잡은 1차값(Yaw ±70, Pitch ±55, 위상 20/10/44)은 **실제 칼보다 훨씬 작고 느렸다.**
시작을 크게 뒤로 젖히고(범위 140° → 300°), 판정 시작을 2프레임 늦춰 Active를 짧게 해 따라잡게 했다.

- **대조는 `slomo 0.1`로 한다.** 월드 시간 확장이라 애니·칼날 판정·디버그 드로우가 **같은 비율로** 느려져
  상대 오차가 왜곡되지 않는다. ⚠️ ASTP의 `playRate`를 낮추면 안 된다 — 프로세서가 덮어쓰고,
  먹더라도 애니만 느려져 오차가 실제보다 커 보인다.
- ⚠️ **Pitch는 ±90°를 넘기지 않는다.** 접평면 기준이라 90°를 넘으면 위를 지나 반대편으로 넘어간다.
- 판정 시작을 늦출 때는 **Active 종료 시점(Windup+Active)과 위상 합을 유지**하면 끝 타이밍과 PlayRate가
  그대로다(20/10 → 22/8). Active를 6프레임 아래로 줄이면 빠르게 지나가는 대상을 놓치기 시작한다.

**원거리 샷건 확정값 (2026-09-13, `DA_Enemy_PureEntity_Ranged01`)** — 사격 클립 0프레임에 발사를 맞췄다:

```
위상    Windup 0 / Active 0 / Recovery 0.667 (20f, PlayRate 1.0)
주기    AttackInterval 1.5 → 1.833      (Windup + Recovery + Interval = 2.5초 유지)
```

- 원거리는 **Windup이 끝나는 순간 1회 발사하고 Active를 건너뛴다**(`ULNPEntityAttackProcessor`).
  ⚠️ 그런데 PlayRate는 원거리여도 `Windup + Active + Recovery`로 계산하므로(`ULNPEnemyAnimationProcessor`),
  **원거리 `ActiveTime`이 0이 아니면 클립이 그만큼 늘어나 끝이 잘린다.** 원거리는 `ActiveTime = 0`으로 둔다.
- ⚠️ 쿨다운은 Recovery가 끝날 때 시작하므로 원거리 주기는 `Windup + Recovery + AttackInterval`이다 — **Active는 들어가지
  않는다.** 위상을 바꾸면 이 합이 유지되도록 `AttackInterval`을 보정한다.
- Lyra 사격 클립은 **발사와 동시에 재생되는 전제의 모션**이라 0프레임 발사가 가장 자연스럽다. 총기 모션은 작아
  예고 동작 구실을 못 하므로, 원거리 공격은 **발사체를 보고 반응**하게 한다(적 발사체 속도를 낮춰 둔 이유) —
  롱소드와 달리 Windup이 예고 창이 아니다. Windup이 0이면 공격을 시작한 틱의 조준 방향으로 쏜다.
- 발사 위치는 `MuzzleLocalOffset`이고 **손 본을 따라가지 않는다.**

### 6.6 포즈 연속성은 포기한다

`ActorPromoted` 개체의 ISKM ↔ Actor 전환에서 포즈가 튄다. 지금도 ISM ↔ Actor 팝이 있으므로
새 문제는 아니지만, 애니가 붙으면 더 눈에 띈다. **수용한다.**

- 승격은 전투 진입 순간(거리 무관)에 일어나므로 화면 밖이거나 카메라 주목 대상이 아닌 경우가 많다.
- 연속성을 맞추려면 Actor 쪽 ABP의 포즈를 ISKM 시퀀스 위상과 동기화해야 하는데, 그것은
  Motion Matching 로코모션(→ [TechDesign_CombatAnimation.md](TechDesign_CombatAnimation.md))과
  근본적으로 맞지 않는다. 비용이 이득을 압도한다.

### 6.7 에셋 요건 — 전부 실측으로 나온 것들

#### 렌더 요건은 **Nanite가 아니다**

| # | 증상 | 실제 요건 |
|:--:|:---|:---|
| 1 | `AssetCheck: Error … requires 'Optimize for Instancing'` | 스켈레탈 메시 **빌드 세팅**의 `bOptimizeForInstancing`을 **전 LOD**에 켤 것 |
| 2 | `Material … missing usage flag InstancedSkinnedMesh!` (기본 머티리얼로 대체됨) | 머티리얼의 `bUsedWithInstancedSkinnedMesh` |
| 3 | `NaNs found on Bounds for … InstancedSkinnedMeshComponent` | 1·2의 파생 — 프로바이더 빌드 실패로 애니 바운드가 무효. 고치면 재발 없음 |

Nanite는 **꺼진 상태로도 동작한다.** `FInstancedSkinnedMeshSceneProxyDesc::CreateMeshObject`가
Nanite → Static → **GPUSkin** 순으로 떨어지고 `r.GPUSkin.UseSceneExtension`이 기본 `true`이기 때문이다.
실제로 `SKM_UEFN_Mannequin`은 `NaniteSettings.bEnabled = false` 그대로 두고 GPUSkin 경로로 붙였다.

> `bOptimizeForInstancing`은 본 맵을 통합하는 빌드 옵션이라 일반 스켈레탈 렌더링과 호환된다.
> 이 메시의 참조자는 적(`BP_LNPEnemy` + 적 EntityConfig)뿐이라 **플레이어에는 영향이 없다.**

#### 시퀀스 선정 기준 둘

⚠️ **추가(additive) 애니메이션은 못 쓴다.** ASTP 컴파일러는 절대 포즈로 굽기 때문에 `AAT_LocalSpaceBase`
클립을 넣으면 뼈가 원점으로 모여 **메시가 통째로 사라진다.** Lyra의 `MM_HitReact_*`가 전부 추가 클립이라
경직 모션에서 실제로 밟았다. 고르기 전에 `AdditiveAnimType`이 `AAT_None`인지 확인할 것.

예외가 있다 — **기준 포즈가 자기 자신의 프레임인 additive 클립**(`RefPoseType = ABPT_AnimFrame`, `RefPoseSeq` = 자기 자신)은
원시 데이터가 전신 포즈이므로 `AdditiveAnimType`을 `AAT_None`으로 바꾸면 그대로 쓸 수 있다.
`MM_Shotgun_Fire_UEFN`(`AAT_RotationOffsetMeshSpace`)이 그렇게 들어갔다. 바꾼 뒤 썸네일에 전신 포즈가 나오는지 볼 것.

⚠️ **제자리 클립이어야 한다.** 실제 재생 경로(렌더러 `Skinning/AnimSequenceTransformProvider`)가
**메시 본 인덱스 0(`root`)의 이동·회전을 레퍼런스 포즈로 고정**하므로 `root`에 실린 루트 모션은 자동으로 제거된다.
(컴파일러의 같은 분기는 바운드 계산용이다.)
덕분에 인플레이스 클립을 따로 구울 필요가 없다는 이점이 있지만, **반대로 변위가 큰 클립은 그 변위를 잃고
캡슐 위치와 어긋난다.** 좌우로 크게 밀리는 피격 모션을 경직에 썼다가 되돌렸다.

⚠️ **`root`가 아닌 본에 실린 변위는 남는다 — 리타겟한 로코모션에서 밟는다** (2026-09-13).
`MM_Rifle_Jog_Fwd_UEFN`이 앞으로 달려갔다가 루프마다 제자리로 튀었다. 원본 Lyra 클립은 전진 이동이
`root`에 있지만(`bForceRootLock`) 원본 pelvis의 **월드 위치**가 앞으로 가고, 리타게터의 **Pelvis Motion 옵**이
그 이동을 대상 pelvis에 옮긴다. 렌더러는 `root`만 버리므로 pelvis 이동이 남는다.

- **해결: Pelvis Motion 옵의 `Scale Horizontal` = 0으로 리타겟.** 모션매칭용 `RTG_Lyra_to_GASP`를 건드리지 않도록
  복제한 `RTG_Lyra_to_GASP_noRootMotion`에 설정했다. 대가로 좌우 골반 흔들림도 사라지지만 멀리서는 티가 안 난다.
- ⚠️ 클립의 `Enable Root Motion`도, 리타게터의 **Root Motion 옵도 무관하다** — `root`는 어차피 버려진다.
  `Blend To Source Translation Weights`도 **효과가 없다**(기본 0인 값에만 곱해지는 가중치다).
- 판별: 애니 에디터에서 pelvis가 **`root` 위를 따라다니며 위아래로만** 움직이면 제자리다. 프리뷰와 ISKM 결과가 같다.

#### ASTP 편집 시 에디터가 죽는 조건

⚠️ **`Sequences` 배열에 `Sequence`가 비어 있는 항목을 한 순간이라도 두면 에디터가 죽는다.**
`PostEditChangeProperty`가 즉시 비동기 DDC 빌드를 띄우고, 그 항목의 본 배열(크기 0)을 인덱싱하다 assert한다.

MCP 규약상 배열은 "크기 변경"과 "값 변경"을 한 번에 못 해 **빈 칸으로 늘렸다가 채우는** 2단계를 쓰기 쉬운데,
이 에셋에서는 그 중간 상태가 치명적이다. **늘릴 때는 기존 유효 항목의 복사본을 붙이고** 값을 덮어쓴다.

---

## 7. 슬롯 시스템 변경

`ULNPTargetingSubsystem`에 세 번째 풀을 추가한다.

| 풀 | 기본값 | 대상 |
|:---|:---:|:---|
| `MaxMeleeSlotsPerPlayer` | 10 | `PureEntity` + 근접 |
| `MaxRangedSlotsPerPlayer` | 20 | `PureEntity` + 원거리 |
| `MaxPromotedSlotsPerPlayer` | 2 | `ActorPromoted` (근접·원거리 구분 없음) |

- **`ActorPromoted`는 근접/원거리를 나누지 않는다.** 개체 수가 적고, 이 풀을 가르는 실제 비용 축은
  교전 거리가 아니라 **Actor 스폰 수**이기 때문이다.
- ⚠️ **이 한도가 곧 플레이어당 적 Actor 수 상한이 된다.** 슬롯을 못 얻으면 `Confirmed`가 아니고,
  `Confirmed`가 아니면 `ULNPEnemyLODOverrideProcessor`가 High를 강제하지 않으므로 Actor가 스폰되지 않는다.
  슬롯 시스템이 처음부터 성능 예산 장치였다는 점이 여기서 드러난다.
- **풀 분리가 필요한 이유**는 "잡몹에 둘러싸여 슬롯이 찬 탓에 엘리트가 구경만 하는" 그림을
  원천 차단하기 위해서다. 같은 풀에 점수 가산으로 처리하면 가산치가 크면 잡몹이 통째로 밀려나고
  작으면 거리로 다시 뒤집힌다 — 튜닝 축만 하나 늘어난다.

### 7.1 검증 기록 (2026-09-05~06, 완료)

2P `-game` 밀도 4, 게스트가 혼성 무리에서 교전. 14표본에서 **한도 초과 0건**, `promoted` 최대 2,
`ranged`는 `promoted`가 꽉 찬 동안에도 7까지 올라갔다 — **풀이 서로 예산을 뺏지 않는다**는 분할의
목적이 그대로 성립했다. 2026-09-06에 `DA_Enemy_PureEntity_Melee01`이 생기면서
*"승격 Actor 2기와 **별개로** 근접 잡몹 10기가 동시에 달려든다"* 를 육안으로 확인했다.

⚠️ **에셋 이름에 `CombatMode`를 넣는다.** `DA_Enemy_ActorPromoted_Melee01` /
`DA_Enemy_PureEntity_Melee01` / `DA_Enemy_PureEntity_Ranged01`처럼 enum 이름을 그대로 쓰고,
EntityConfig도 같은 규칙으로 1:1 짝을 맞춘다 — 모드와 표현 매핑이 어긋나는 §6.2의 함정을
`ValidateTemplate` 경고가 나기 전에 **에셋 목록에서 눈으로** 막기 위해서다.
신규 `PureEntity` 설정은 **표현 매핑이 이미 `PureEntity`인 것을 복제**해 만드는 편이,
Actor 단계가 든 배열을 손으로 고치는 것보다 안전하다.

#### `MaxPromotedSlotsPerPlayer`를 일시적으로 올릴 때의 상한

**실측 근거가 있는 값은 10이다** — Stage 6 이전 구성이 정확히 그것이었고(근접 적이 `melee` 풀 10칸을
썼다), 밀도 4·2P·게스트 전투 피크가 **58.9 KB/s = 안전판(150,000)의 39%, 포화 0건**이었다.
다만 이는 "10까지 허용된 상태의 실측"이지 **10기 동시 승격이 찍혔다는 증거는 아니다.**
6 이하는 재측정 없이 올려도 되고, **10을 넘기면 반드시 재측정한다.**

- ⚠️ **산술 외삽으로 정하지 말 것.** 승격 1기당 700~900 B/s로 나눠 "아직 25칸 남았다"를 계산할 수 있지만,
  두 점으로 세운 선을 외삽하지 않는다는 규약(→ [Guide_NetBandwidth.md](Guide_NetBandwidth.md) §3.8)에 걸리고
  **CPU(Actor 틱·Mover·애니메이션)는 아예 측정된 적이 없다** — 대역폭보다 먼저 물릴 가능성이 높다.
- ⚠️ **슬롯은 플레이어당이다.** 월드 전체 승격 수 = 값 × 플레이어 수. 4P는 같은 값에서 2P의 두 배가 된다.
- ⚠️ **초과의 증상은 렉이 아니라 "조용한 소실"이다** (→ [TechDesign_Networking.md](TechDesign_Networking.md) §4.7).
  **"플레이해 보니 괜찮더라"로는 판단할 수 없고**, 송신량을 직접 재야 한다.

**부수 정리(완료 — Stage 1로 앞당겼다):** `EnemyTypeTag.ToString().Contains(TEXT("Melee"))` 문자열 비교를
Config의 `ELNPEnemyAttackType` 필드로 교체했다. 원래 이 Stage의 항목이었으나, **순수 엔티티 공격 경로가
같은 판별을 필요로 하는 순간 판별 원본이 둘이 되기 때문에** 앞당겼다. 슬롯 풀 3분할 자체는 이 Stage에 남는다.

⚠️ 이 교체는 데이터 손질을 동반한다. 기본값이 `Melee`라 **`AttackType`을 지정하지 않은 원거리 적은
근접 슬롯 풀로 분류된다** — 태그 이름(`LNP.Enemy.Minion.Ranged`)만 보고 넘어가면 조용히 잘못된다.

---

## 8. 사망 처리

`ULNPHealthProcessor`가 모드로 분기한다.

| 모드 | 처리 |
|:---|:---|
| `ActorPromoted` | 기존 그대로 — `TriggerRagdoll()` 방송 + `DeathCountdown = ULNPSettings::EnemyRagdollDuration` |
| `PureEntity` | 랙돌 방송 없음 + `DeathCountdown = ULNPEnemyConfig::PureEntityDeathDuration` (기본 2.2초) |

랙돌은 `ActorPromoted` 전용 연출로 남는다. `PureEntity`가 그냥 사라지지 않는 이유는 행동 상태 채널이
`Dying`을 전파하고 ISKM이 Death 시퀀스를 재생하기 때문이다 — **트랙 B가 없으면 순수 엔티티는
소리 없이 소멸한다.**

⚠️ **`FLNPEnemyDyingTag`를 `None`으로 거는 쿼리에는 죽음이 안 보인다.** 적 프로세서 대부분이 그렇게 하고
있어 그대로 베끼기 쉬운데, 죽는 순간 엔티티가 쿼리에서 빠지므로 **아무도 `Dying`을 싣지 못하고**
사망 팝도 한 번 적분되지 않는다. 행동 상태·애니·이동 프로세서는 태그를 배제하지 않고,
시체의 AI 이동·회전·StateTree 신호는 `Execute` 안에서 태그로 따로 차단한다.
(랙돌이 붙은 시체는 물리의 주인이 Actor이므로 통째로 건너뛴다.)

### 사망 팝 (2026-09-07)

랙돌이 없어도 "맞고 날아간다"는 그림은 남긴다. `ULNPHealthProcessor`가 사망 순간
`FLNPEnemyVelocityFragment`에 표면 Up 방향 속도를 실어 주고, 이동 프로세서의 공중 분기가 중력과 함께
적분해 표면에 스냅한다. 값(`PureEntityDeathPopSpeed`, 기본 2000)은 승격 개체가 랙돌에 주는 팝
(`ALNPEnemyCharacter::RagdollPopSpeed`)과 **같다.** 공중 물리는 **넉백과 같은 람다**를 쓴다 —
복제했다면 죽는 순간에만 다른 곡선을 그리는 어긋남이 생겼을 자리다.

⚠️ **`PureEntityDeathDuration`이 두 하한을 동시에 덮어야 한다.**
① 엔티티 파괴가 곧 버블 제거라, 복제 LOD의 최장 갱신 주기(0.3초)와 넷 틱 몇 개를 덮지 못하면
**게스트가 `Dying`을 받기도 전에 적이 사라진다**(일회성 전이라 게이트는 우회하지만 패킷은 한 번 나가야 한다).
② 팝의 체공 시간(대략 `2 * PopSpeed / GravityStrength`)보다 짧으면 **시체가 공중에서 사라진다.**
기본값 조합(2000 / 2000)의 왕복이 2.0초라 소멸 기본값을 1.5 → **2.2초**로 올렸다.
`ActorPromoted`는 랙돌 지속이 5초라 이 제약이 드러나지 않는다 — 같은 팝 속도를 쓰려면 소멸 시간도 함께 본다.

---

## 9. 남은 결정 / 검증 필요 항목

**해소:** Nanite 스킨 요건(2026-09-06 — 요건은 Nanite가 아니라 `bOptimizeForInstancing` + 머티리얼
사용 플래그였다, §6.7) · 시퀀스 에셋 준비(2026-09-13 — 리타게팅·무기 스킨드 에셋 완료, §6.3) ·
칼날 아크와 애니 궤적의 정합(2026-09-13 — 롱소드 기준 확정, §6.5) ·
상태 채널 확장(Aim Pitch·HP 비율 둘 다 들어왔다, §5.5).

| # | 남은 항목 | 성격 |
|:---:|:---|:---|
| 3 | 수치 튜닝 — Promoted 슬롯 수, 유의도 경계 | 플레이 테스트 |
| 4 | 피격 방향을 상태 채널에 넣을지 | 보류 — "없어서 못 읽겠다"가 확인되면(§5.5) |
| 5 | 사망 연출 최종 형태 — Death 시퀀스 + 소멸 VFX 여부 | 기획 |
| 6 | 엘리트 고도화 행동(특수 어빌리티) | **범위 밖.** `ActorPromoted` 경로는 이 작업에서 바뀌지 않았다 |

---

## 10. 구현 플랜

**전 Stage 완료.** 남긴 이유는 순서 자체가 검증 경로였기 때문이다 — 각 Stage가
"이것 하나만 보면 성립을 안다"는 단일 증거를 갖도록 잘랐다.

| Stage | 내용 | 그 Stage의 단일 증거 |
|:---:|:---|:---|
| **0** ✅ | `ELNPEnemyCombatMode` + LODOverride 분기 + `PureEntity`용 표현 매핑 분리 | `Confirmed`가 되어도, **가까이 가도** Actor가 안 뜬다 |
| **1** ✅ | 순수 엔티티 원거리 공격 (트랙 A) | 게스트에 아무것도 안 보여도 서버에서 HP가 깎이고, **패링 반사가 성립한다** |
| **2** ✅ | 순수 엔티티 근접 공격 — 가상 칼날 (트랙 A) | **플레이어가 순수 엔티티의 근접을 패링할 수 있다** = 판정 파이프라인 재사용의 증거 |
| **3** ✅ | 행동 상태 채널 (트랙 B) | **연속 공격 2회가 2회로 보인다** — 전이 카운터 |
| **4** ✅ | 발사체 관전 가시성 | **새 RPC 없이** 게스트 발사체가 보이고 임팩트가 서버 판정과 일치 |
| **5a** ✅ | ISKM 파이프라인 개통 — Idle/Move | 인스턴스가 보이고 걷는다. 렌더 요건 실측이 여기서 끝났다(§6.7) |
| **5b** ✅ | Attack/Stagger/Death/Parried + PlayRate 파생 | Config 수치를 바꾸면 모션 속도가 따라온다 |
| **5c** ✅ | 무기 스킨드 메시 | 칼날 디버그 선분이 칼 궤적과 겹친다 (§6.3, §6.5) |
| **6** ✅ | 슬롯 풀 3분할 + 문자열 비교 제거 | 잡몹에 둘러싸인 상태에서 `ActorPromoted` 개체가 교전에 진입한다 (§7.1) |

**의존 관계:** 1·2 → 0 / 4·5 → 3 / 6은 독립. §8(사망 배선)은 3에 딸려 끝났고,
5c는 나머지에 영향을 주지 않는다(5a·5b만으로 적은 완전히 애니메이션된다).
Stage 0~2의 서버 판정은 로그·디버그 드로우로 자동 확인되지만, **3~5의 2P 체감은 실제 플레이가 필요했다.**

### 10.1 트랙 A 검증 기록 (2026-09-05, 완료)

승격 차단 · 근접 적(`ActorPromoted`) 회귀 없음 · 산탄 · 가드 · 패링 반사(적도 부근 포함) ·
상하 조준 사격(고저차 지형) · **그로기 진입 시 공격·이동 정지와 임계 이탈 시 재개** · 호스트/게스트 양쪽.

⚠️ **경직 관찰에는 임시값이 필요했다.** 경직이 쌓이기 전에 적이 죽어 그로기 구간을 볼 수 없었다 —
공격력·HP 밸런스 문제이지 임계값 문제가 아니다. 플레이어 무기 레벨1 공격력을 0으로, 원거리 NPC 체력을
5배로 올려 관찰했고 **2026-09-16에 전부 원복했다.**

⚠️ **적 엔티티의 HP·방어력 원본은 무기(`WeaponData`)의 스탯 수정자다.**
`ULNPEnemyConfig::InitialAttributeValues`는 Actor 승격 후 ASC 초기화에만 쓰여 순수 엔티티에는 반영되지 않는다.

### 10.2 트랙 B 검증 기록 (2026-09-05, 완료)

**2P Standalone(`-game`), 밀도 1, 게스트가 전투 안에서 플레이.**

- 게스트 화면에 엔티티 발사체가 보이고 **관통하지 않는다** — 플레이어에 막히고 착탄 이펙트가 재생된다.
- **빨간 상자 1회 = 공격 1회.** 누락도 중복도 없었다(전이 카운터가 하는 일이 이것 전부다).
- 호스트와 게스트에서 **같은 적의 상자 색이 동시에 같은 값으로 갱신**된다 — 단일 소비 경로 성립.
- 고저차에서 위·아래·평면 사격을 모두 유도해 **조준각 동기 확인.**
- 근접 적(`ActorPromoted`) 회귀 없음. assert·ensure 0건.

**대역폭 (210구간 = 17.5분):** 행동 사유만인 Dirty **1.39%**, 게이트 우회 **0.24회/s(Dirty의 1.0%)**,
피크 송신 22.1 KB/s(상한 150,000의 15%), 포화 0건. 상세와 방법은
[Guide_NetBandwidth.md](Guide_NetBandwidth.md) §3.7.1(사유별 Dirty 카운터)·§1.1.

⚠️ **계측 세션의 함정 — 버블은 클라이언트별이라 계측이 게스트 카메라를 따라간다.**
호스트로 플레이하면 게스트 폰이 스폰 지점에 서 있어 전투 대역폭이 전혀 잡히지 않는다. 리슨 서버의
호스트에는 `NetConnection`이 없어 송신량이 0이다 — **측정하려는 클라이언트가 직접 전투 안에 있어야 한다.**

### 10.3 트랙 C 검증 기록 (2026-09-06 5a·5b / 2026-09-13 5c, 완료)

**2P Standalone(`-game`), 게스트가 전투 안에서 플레이.**

- **5a** — T포즈로 굳어 있던 순수 엔티티가 정지 시 Idle, 이동 시 Run을 재생한다. 제자리에서 돌고
  (루트 본 제거가 실제로 동작) 이동 방향으로 정상 회전한다. ISKM ↔ ISM 전환에 크래시·깜빡임 없음.
- **5b** — 사선 내려베기가 공격 창 안에 완결되고, 경직·사망·패링이 각각 다른 모션으로 재생된다.
  길이가 다른 두 공격 클립(2.47s·2.00s)이 **같은 공격 창에 맞춰져 스윙 길이가 같게** 보였다 —
  §6.5의 PlayRate 파생이 성립했다는 증거다(이후 변형을 하나로 줄여 이 대조는 다시 만들 수 없다).
- **5c** — ISKM 거리의 적이 칼·샷건을 들고 Idle·Run·공격 내내 손에 붙어 움직인다. 칼날 디버그 선분을
  `slomo 0.1`로 궤적과 대조해 아크·위상을 확정했다(§6.5). 샷건은 방향과 왼손 위치까지 맞았다.

**1·2차에서 밟은 함정은 전부 규약으로 남겼다** — 추가(additive) 클립이 메시를 지운 것(§6.7),
`Seq` 패리티로 공격 변형이 고정된 것(§6.4), ISKM 렌더 오진단의 세 겹 원인(바인드 포즈 · EMPTY 본 ·
프로바이더 시퀀스 불일치, §6.3), 리타겟 pelvis 수평 이동(§6.7).

⚠️ **에셋을 바꿀 때마다 확인하지 않아 플레이 테스트를 여러 번 버렸다.** 썸네일(메시 유효성) →
애니 프리뷰(바인드 포즈) → `get_bone_names`(스켈레톤 일치) 순서를 건너뛰지 말 것.

**미해결 1건:** 사망 시 디버그 마커가 간헐적으로 검정이 아닌 경우가 관찰됐으나 **재현되지 않았다.**
애니메이션은 정상이라 표시 계층만의 문제로 보인다. 재발하면 `LNP.Debug.DrawEnemyAction 2`의
호스트·게스트 전이 로그를 대조할 것.

---

## 11. 위험 요소

증상에서 원인으로 가는 표다.

| 징후 | 먼저 볼 곳 |
|:---|:---|
| 특정 상태에서만 ISKM 메시가 안 보임 | 그 상태의 시퀀스가 **추가(additive) 클립**인지 (§6.7) |
| 메시가 전부 안 보이거나 머티리얼이 깨짐 | `bOptimizeForInstancing` · 머티리얼 사용 플래그 (§6.7) |
| 몸만 옆으로 밀렸다 돌아옴 | 루트 본이 제거되므로 **변위 큰 클립**을 쓰면 안 된다 (§6.7) |
| 애니가 걸리는 순간 메시가 터짐 | 바인드 포즈 — 메시 에디터 `Preview Animation`으로 판별 (§6.3) |
| 서버에선 공격하는데 게스트는 가만히 서 있음 | Fast Array Dirty 표시 누락 (§5.3) |
| "칼이 안 닿았는데 맞는다" | `BladeOuter`와 무기 메시 길이 불일치 — 디버그 드로우로 대조 (§6.3) |
| 프레임당 엔티티 수가 단조 증가 | 스윙 엔티티 누수 — `TimeToLive`가 그물이지만 파괴 경로를 확인 (§4.5) |
