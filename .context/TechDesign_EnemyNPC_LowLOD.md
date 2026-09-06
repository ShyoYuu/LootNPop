# Enemy NPC — Low LOD(순수 엔티티) 전투 기술 설계

## 1. 한눈에 보기

현재 모든 Enemy는 전투 진입(`Confirmed`) 시 예외 없이 High LOD Actor로 승격되어 GAS·몽타주로 싸운다
(→ [TechDesign_EnemyNPC.md](TechDesign_EnemyNPC.md) §7.2). 이 문서는 **승격을 Config 옵션으로 바꾸고**,
승격하지 않는 개체가 순수 MassEntity 상태 그대로 기본 공격까지 수행하도록 만드는 설계다.

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

A는 B·C 없이도 "보이지 않지만 실제로 때리는 적"으로 완성되고, B는 C의 입력이 된다.
이 분리가 곧 단계별 검증 경로다 (§10).

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

- **등급은 늘어나고 축은 늘어나지 않는다.** 2단계가 3단계·5단계가 되어도 시스템이 가르는 것은 영원히
  두 값이다. 등급을 코드에 넣으면 등급이 늘 때마다 분기가 늘고, 그 분기 대부분은 같은 코드로 수렴한다.
- **조합이 어긋난다.** "엘리트인데 PureEntity"(대규모 정예 웨이브)나 "잡몹인데 ActorPromoted"
  (튜토리얼 1마리)는 기획상 충분히 있을 수 있다. 이름이 모드를 단정하는 순간 코드가 거짓말을 시작한다.
- **판정이 한 값으로 끝난다.** 슬롯·비주얼·공격 경로가 전부 이 옵션 하나에서 갈리므로,
  "이 개체가 무엇을 못 하는가"를 다른 데이터를 보지 않고 답할 수 있다.

`bCanPromoteToActor` + `bHasEntityAttack` 같은 bool 두 개로 쪼개지 않는 이유도 같다.
bool 조합은 정의되지 않은 상태(둘 다 false / 둘 다 true)를 만들고, 그 상태의 동작을 문서로 방어해야 한다.
enum에는 그 자리가 없다.

---

## 3. 이미 준비되어 있는 것 (재사용 자산)

이 설계가 성립하는 이유는 기존 코드가 이미 Actor 비의존이기 때문이다.
**새로 만드는 것보다 그대로 쓰는 것이 훨씬 많다.**

| 자산 | 왜 그대로 쓰이는가 |
|:---|:---|
| `FLNPWeaponTraceFragment` | 칼날을 **좌표 4점 + 반경**으로만 표현한다. 본·소켓·몽타주 참조가 하나도 없다 |
| `ULNPWeaponTraceHitDetectionProcessor` | `AttackerQuery`가 요구하는 것은 `FLNPWeaponTraceFragment` **하나뿐**이다 (`LNPWeaponTraceProcessors.cpp:221`) |
| `JudgePlayerTarget` 람다 | 패링→가드→피격 2단계 판정. 근접 PvP와 Enemy→Player가 이미 같은 코드를 탄다 |
| 발사체 스폰 | `ULNPAbility_RangedAttack::SpawnProjectile`은 껍데기만 어빌리티고 내용은 전부 `PushCommand<FMassCommandBuildEntityWithSharedFragments<…>>`다 |
| `FLNPApplyDamageGECommand` | **피격자 ASC만** 필요하다. 공격자 Actor는 없어도 된다 (`LNPHitDetectionShared.h:348`) |
| `AimPitchMin/MaxDeg` | 이미 Actor가 아니라 Config에 있다 — *"Mass 프로세서는 Actor가 없는 Low LOD에서도 돌아야 한다"* 는 이유로 |
| `FLNPPoiseFragment` | 경직은 원래 엔티티 단위 값이다. 순수 엔티티도 그대로 굳는다 |
| `FLNPEnemyMovementConfig::AttackInterval` | **선언만 되어 있고 소비처가 0이다.** 엔티티 공격 쿨다운이 이 필드의 첫 소비처가 된다 |

---

## 4. 트랙 A — 순수 엔티티 공격

### 4.1 데이터

무기 상수(발사체 속도·수명·폭발 반경·피해 GE 클래스)는 **이미 `ULNPEnemyConfig::WeaponData`에 있다.**
새로 정의할 것은 지금까지 *어빌리티 인스턴스가* 공급하던 값뿐이다 — 즉 기존 `FLNPProjectileSharedFragment`를
채우던 두 출처(WeaponData / Ability) 중 **Ability 쪽만 대체**한다.

```cpp
USTRUCT()
struct FLNPEntityAttackConfig
{
    // 공용
    float Damage            = 10.f;
    float PoiseDamage       = 10.f;
    float KnockbackStrength = 0.f;
    float ParryRadius       = 40.f;   // 무기 HitRadius보다 크게 — 2단계 판정 규약
    float WindupTime        = 0.35f;  // 선딜: 플레이어가 읽고 반응할 구간
    float ActiveTime        = 0.20f;  // 근접 = 칼날 생존 구간 / 원거리 = 미사용
    float RecoveryTime      = 0.45f;  // 후딜

    // 근접 전용 — 가상 칼날 (§4.3)
    float PivotForward = 20.f;   // 캡슐 중심 기준 회전 원점
    float PivotUp      = 30.f;
    float BladeInner   = 30.f;   // 원점~칼밑
    float BladeOuter   = 140.f;  // 원점~칼끝 (시각 무기 길이와 반드시 일치시킬 것)
    float ArcStartDeg  = -70.f;  // 로컬 Yaw 시작
    float ArcEndDeg    =  70.f;  // 로컬 Yaw 끝
    float ArcPitchDeg  = -15.f;  // 내려베기 기울기
    float HitRadius    = 12.f;

    // 원거리 전용
    FVector MuzzleLocalOffset = FVector(40.f, 0.f, 10.f);  // 캡슐 중심 기준 로컬
};
```

공격 간격은 새 필드를 만들지 않고 **`FLNPEnemyMovementConfig::AttackInterval`을 쓴다.**
지금 아무도 읽지 않는 필드이고, 의미가 정확히 일치한다.

### 4.2 상태 기계 — 판단은 Task가, 진행은 Processor가

```cpp
USTRUCT()
struct FLNPEntityAttackFragment : public FMassFragment
{
    ELNPEntityAttackPhase Phase = None;   // None / Windup / Active / Recovery
    float             PhaseElapsed      = 0.f;
    float             CooldownRemaining = 0.f;
    FMassEntityHandle SwingEntity;        // 근접: 살아 있는 칼날 엔티티
    uint8             bAttackRequested : 1 = 0;
};
```

⚠️ **이 프래그먼트는 `ActorPromoted` 개체에도 붙인다.** 모드로 아키타입을 가르지 않는다.

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
  θ     = Lerp(ArcStartDeg, ArcEndDeg, t)
  Dir   = (Fwd*cos θ + Right*sin θ) 를 Right축 기준 ArcPitchDeg 회전
  Prev ← Curr
  SwordRootCurr = Pivot + Dir * BladeInner
  SwordTipCurr  = Pivot + Dir * BladeOuter
```

- 첫 프레임 `Prev == Curr` 축퇴는 판정 프로세서가 이미 선분-선분 폴백으로 처리한다.
- 스윙 엔티티는 `NotifyBegin` 대신 **Active 진입 시 생성**, Active 종료 시 파괴.
  `TimeToLive` 안전장치는 그대로 살린다 — 프로세서가 종료를 놓쳤을 때의 그물이 된다.
- 채워야 하는 필드: `InstigatorEntity`(적 엔티티), `InstigatorTeam = Enemy`,
  `InstigatorActor = nullptr`, `bIsLocalInstigator = false`.
- 기울임은 **고정 Right축 회전이 아니라** 접평면 성분을 Up으로 들어 올려 만든다.
  고정 축으로 돌리면 Yaw가 ±90°에 가까울 때 축과 방향이 겹쳐 회전이 사라진다.

**칼날 갱신은 2패스다.** 칼날은 적과 **다른 엔티티**라 서로의 프래그먼트에 임의 접근할 수 없다.
`ULNPWeaponTraceHitDetectionProcessor`가 쓰는 Pass 1/2/3과 같은 형태로 나눈다 —
Pass 1(적 쿼리)이 위상을 진행하며 4점을 계산해 모으고, Pass 2(칼날 쿼리)가 핸들로 매칭해 기록한다.

⚠️ **칼날 마커는 Tag가 아니라 Fragment(`FLNPEntitySwingFragment`)다.** 칼날은
`FMassCommandBuildEntity` 한 번으로 만들어야 하는데, `BuildEntity`와 `AddTag`를 같은 배치에 디퍼드하면
아키타입 전환 타이밍 때문에 쿼리가 그 엔티티를 못 찾는다 (`UANS_LNPMeleeHitWindow`가 같은 이유로 Tag를 쓰지 않는다).

**이 접근을 택하는 이유는 패링이다.** 순간 원뿔·구 판정으로 때우면 `JudgePlayerTarget`의
패링→가드→피격 2단계를 새로 짜야 하고, 그것은 [TechDesign_HitDetection.md](TechDesign_HitDetection.md) §7.5가
"PvP 쪽에만 패링 체크가 누락됐던" 사고로 못 박아 둔 **분기 복제 함정을 그대로 재생산**하는 일이다.
플레이어가 순수 엔티티의 공격도 패링할 수 있어야 전투 규칙에 구멍이 생기지 않는다.

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
캐릭터에서는 **골반**이고, 그대로 겨누면 "하반신을 노리는" 그림이 된다. 가슴께를 겨누려면 양수를 준다.
좌표 규약이 캡슐 중심이라는 사실과 "사람이 겨누는 곳"이 다르다는 점을 데이터로 흡수하는 자리다.

**산탄은 어빌리티와 공식을 공유한다.** `HexRingCount`(0=단발, 1=7발, 2=19발)·`HexStepDegrees`를 Config에
두고, 배치는 `LNPSpread::BuildHexRingDirections`(`HitDetection/LNPSpreadPattern.h`)를 부른다 —
`ULNPAbility_RangedSpreadAttack`이 쓰던 코드를 그대로 꺼낸 것이다. 복제했다면 중력 Up 기준 직교 기저와
짐벌 수렴 방지(→ [TechDesign_Ability.md](TechDesign_Ability.md) §3.2)가 한쪽에만 남는 사고를 재생산했을 것이다.

⚠️ **SalvoID는 한 번의 발사에 하나다.** Ghost 식별자가 `{PlayerID, KeyOrSalvo, SpawnIndex}` 조합이라
펠릿마다 키를 새로 발급하면 같은 발사의 펠릿들이 서로 다른 발사로 잡힌다. 펠릿 구분은 `SpawnIndex`가 맡는다.

예측 사격(리드샷)은 하지 않는다 — 파라미터가 하나 늘고(예측 계수), 순수 엔티티는 정의상
"단순한 기본 공격"이 담당 영역이다. 필요해지면 그때는 `ActorPromoted` 쪽 어빌리티의 일이다.

### 4.5 중단 규칙

| 사유 | 처리 |
|:---|:---|
| 경직(`bIsGroggy`) 진입 | 위상 즉시 `None`, 칼날 엔티티 파괴, 쿨다운 유지 |
| 사망(`FLNPEnemyDyingTag`) | 동일 + 행동 상태 `Dying` |
| 타겟 상실 / 사거리 이탈 | Windup 중이면 취소, Active 이후면 끝까지 재생 (헛스윙이 자연스럽다) |

⚠️ **경직 취소 경로를 반드시 새로 넣어야 한다.** Actor 경로에서는 `FLNPStaggerCommand::Run`이
`CancelCurrentAttackAbility()`로 끊지만(`LNPPoiseTypes.cpp:92`), 그 함수는 *"Actor가 없는 Low LOD 적은
연출도 어빌리티도 없다"* 며 조기 반환한다(69~73행). **끊어 줄 주체가 아무도 없다.**
`ULNPEntityAttackProcessor`가 매 프레임 `FLNPPoiseFragment`를 읽어 스스로 끊는 것이 유일한 경로다.

### 4.6 감수하는 한계

| 항목 | 내용 | 판단 근거 |
|:---|:---|:---|
| 동적 수치 | 버프·디버프로 공격력·경직력이 변하지 않는다 | 적 GAS 버프는 원래 백로그 항목이다(Actor 풀 반납 시 GE 소멸). 여기서 새로 생기는 제약이 아니다 |
| 콤보 | 콤보 인덱스별 경직력·넉백 없음 | 단타 고정 |
| Lag Compensation | `RewindSeconds = 0` | **이미 그렇다** — 공격자에 PlayerState가 없으면 되감기가 0이다 (`LNPWeaponTraceProcessors.cpp:467` 주석) |
| 클라이언트 예측 | 없음 | 예측은 로컬 공격자 전용 개념 |
| 공격자 HitStop | 없음 | 피격자(플레이어) 측 HitStop·임팩트 큐는 정상 동작한다 |
| **적이 피격자인 방향** | ⚠️ **이 표가 다루지 않는다** | 위 항목들은 전부 *"순수 엔티티가 공격자"* 인 방향이다. **반대 방향(플레이어가 순수 엔티티를 때리는 경우)에는 별도의 구멍이 있다** — 임팩트 VFX와 넉백이 피격자 Actor/ASC를 경유하는데 순수 엔티티에는 둘 다 없다. 감수 항목이 아니라 **미해결이며 트래커에 등록돼 있다** |
| 피격 리액션 몽타주 | 없음 | Stagger 시퀀스로 대체 (§6.4) |
| 랙돌 | 없음 | 사망 시퀀스로 대체. 랙돌은 `ActorPromoted` 전용 연출로 남긴다 |
| 월드 HP Bar | 없음 | MassEntity용 커스텀 Slate HP Bar 도입 시 재검토 (→ [TechDesign_HUD.md](TechDesign_HUD.md)) |
| 상하 조준 **자세** | 복제되지 않는다 | **발사 방향은 클램프된 Pitch를 쓴다**(§4.4) — 없는 것은 게스트가 보는 *자세*뿐이다. 필요하면 상태 채널에 추가 (§5.5) |

---

## 5. 트랙 B — 행동 상태 채널

### 5.1 왜 이벤트가 아니라 상태인가

순수 엔티티의 공격은 **서버 전용 Mass 로직**이고, 트랙 B 이전의 복제 페이로드
`FLNPReplicatedAgent`에는 **위치 + 접평면 Yaw**뿐이었다.
클라이언트는 이 개체가 공격 중인지 알 방법이 없다.

⚠️ 이 문서가 원래 적었던 "EnemyTypeTag(스폰 1회)"는 페이로드에 없다 — 타입 식별은 베이스
`FReplicatedAgentBase`의 `TemplateID`가 이미 하고 있고, 그것으로 스폰된 클라이언트 엔티티는
**서버와 같은 템플릿**이라 `FLNPEnemySharedFragment`(→ `ULNPEnemyConfig`)를 그대로 갖는다.
즉 게스트는 태그로 Config를 되찾을 필요 없이 **직접 읽는다**(§5.4가 그 위에 선다).

발사 이벤트를 `ALNPGameState` Multicast RPC로 쏘는 방법도 있지만 택하지 않는다 —
**RPC 수 = 발사 수 × 개체 수**가 되어, "순수 엔티티는 다수"라는 이 설계의 전제 자체와 충돌한다.
다수를 전제하는 순간 연출은 개별 이벤트가 아니라 **상태 복제**로 흘러야 한다.

### 5.2 인코딩 — 1바이트

```cpp
UENUM() enum class ELNPEnemyAction : uint8 { Idle, Move, Attack, Stagger, Dying };  // 3비트

// FLNPReplicatedAgent에 추가되는 필드 — 둘 다 PositionYaw의 **형제 멤버**다
uint8 ActionAndSeq;   // 상태 3비트 + 전이 카운터 5비트
int8  AimPitch;       // 발사 순간의 상하 조준각 (∓90도를 int8 전 범위에, 약 0.7도) — §5.4
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

**실측 (2026-09-05, 2P `-game`, 밀도 1, 게스트 전투 17.5분):** 이 채널이 갱신 **횟수**를 늘린 몫은
**1.39%**(행동 변화만이 사유였던 Dirty의 비율), 일회성 전이의 게이트 우회는 **0.24회/s = Dirty의 1.0%**다.
피크 송신은 22.1 KB/s로 상한(150,000)의 15%, 포화 0건.

⚠️ **이 문서가 처음 적었던 "약 8% 증가"는 갱신 1회당 페이로드 기준이고 델타 압축을 빼고 센 값이다.**
총 송신량에 대한 실제 영향은 그보다 두 자릿수 배 작다. **비용은 바이트 수가 아니라 갱신 횟수로 세야 한다.**

### 5.3 서버·클라 단일 소비 경로

```
[서버]  프로세서가 FLNPEnemyActionFragment에 기록 ─┬─▶ (리슨 호스트) 애니 프로세서가 읽음
                                                    └─▶ 복제 페이로드에 실림
[클라]  버블 핸들러가 수신값을 같은 Fragment에 기록 ──▶ 애니 프로세서가 읽음
```

애니 프로세서에는 **서버/클라 분기가 없다.** 이 프로젝트의 다른 Mass 프로세서가 전부
`LNPMass::IsClientWorld()` 가드로 시작하는 것과 대조되는 유일한 예외이고, 그것이 의도다 —
"양쪽이 같은 입력을 보고 같은 그림을 그린다"가 이 채널의 존재 이유이기 때문이다.

⚠️ `FLNPMassFastArrayItem`의 주석 규약 — *"멤버가 바뀌면 반드시 Dirty 표시할 것"* 을 지켜야 한다.
`TemplateID`는 스폰 1회지만 이 필드는 **매 갱신 대상**이다. `AddEntityCallback`(스폰 시드)과
`ModifyEntityCallback`(갱신) **양쪽 모두** 채워야 한다 — 시드를 빠뜨리면 버블에 새로 들어온 적이
실제 상태와 무관하게 `Idle`로 시작해 다음 전이까지 굳어 보인다.

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
→ 양쪽이 `f(FMassNetworkID, 전이 카운터)`로 같은 키를 스스로 유도한다. **추가 대역폭 0.**

- 키 공간이 겹치면 안 된다 — 예측 키는 0~65535, `IssueServerSalvoID`는 65536부터이므로 **음수 영역**을 쓴다.
- `FMassNetworkIDFragment`는 복제 트레이트가 붙이므로 **Standalone에는 없다.** 그때만 전역 카운터로 돌아간다.

**감수하는 오차 (전부 코스메틱 — 임팩트 지점은 서버 큐가 확정한다):**

- 발사 지점과 수평 방향이 미세하게 어긋난다(게스트는 보간값, 서버는 실측값).
  `ALNPCharacterBase::Multicast_SpawnGhostProjectiles`가 이미 감수하는 것과 같은 종류다.
- 발사 시각을 싣지 않으므로 Dead Reckoning의 업스트림 지연은 0이다(수신자 RTT/2만 적용된다).
- ⚠️ **총구는 조준각과 무관한 고정 오프셋이다**(캡슐 중심 기준). 캡슐 중심이 골반 높이라 총구는
  허리쯤이고, 조준 자세가 없는 ISM 상태에서는 **위로 쏘면 하반신에서, 아래로 쏘면 상반신에서**
  나오는 것처럼 읽힌다(2026-09-05 관찰). 스폰 지점이 실제로 움직이는 것은 아니며 —
  총구 높이는 데이터(`MuzzleLocalOffset.Z`)로, 나머지는 트랙 C의 조준 자세로 해소된다.

### 5.5 무엇을 넣지 않는가 (지금은)

피격 방향, HP 비율 — **뺀다.** 플레이 테스트에서 "이게 없어서 못 읽겠다"가
확인된 것만 골라서 넣는다. 상태 채널은 한 번 넓히면 좁히기 어렵고, 대역폭은 개체 수에 곱해진다.

**Aim Pitch는 그 절차를 거쳐 들어왔다 (2026-09-05).** 처음엔 빼고 내보냈고, 2P 고저차 교전에서
*"게스트 화면에선 충분히 피했는데 서버에선 맞고, 피격 모션과 HP 차감까지 실행된다"* 가 확인되어 추가했다.
값이 **발사 순간에만 바뀌므로** 나머지 갱신에서는 델타 압축이 1비트로 접는다 — 넓히더라도
"매 갱신 변하지 않는 축"을 고르면 비용이 거의 붙지 않는다는 것이 이 사례의 교훈이다.

### 5.6 함정 — 복제 주기 vs 공격 길이

복제 LOD가 Low(0.3초)인 거리에서 짧은 공격(총 1.0초)은 시작과 끝이 두 갱신 사이에 들어가
**통째로 스킵될 수 있다.** 전이 카운터가 "전이가 있었다"는 사실은 알려주지만 이미 늦은 시점이다.

**해법 — 일회성 전이만 게이트를 우회한다 (채택, 2026-09-05).**

`Attack`·`Stagger`·`Dying` **진입**은 `UpdateInterval` 게이트를 건너뛰고 즉시 Dirty를 건다.
`Idle <-> Move`는 게이트를 그대로 탄다.

⚠️ **전이 전부를 우회시키면 안 된다.** 루프 상태는 늦게 도착해도 그림이 같은 반면,
멈췄다 걷기를 반복하는 배회 개체는 `Idle <-> Move` 전이를 초당 여러 번 만들어 갱신 수를
통제 없이 밀어올린다. **일회성/루프 구분 하나가 스킵과 플랩을 동시에 막는다.**

같은 이유로 `Idle <-> Move` 판별에는 **실제 변위 기준 히스테리시스**(진입 40 / 이탈 15 cm/s)를 둔다.
이 데드밴드는 연출 장치이자 대역폭 장치다 — 실측에서 개체당 전이가 **중앙값 0.26회/s
(최대 0.31, 연속 전이 간격 중앙값 3.03초)** 로 억제됐다.

**실측 결과:** 게이트 우회는 **0.24회/s = 전체 Dirty의 1.0%**. 감수하기로 했던 스킵은 남지만
(두 전이가 한 갱신에 뭉치면 그 발사의 고스트를 건너뛴다) 체감 문제로 보고되지 않았다.

⚠️ **타임스탬프를 추가하는 방향으로는 가지 않는다** — 대역폭이 늘고 스킵 문제는 그대로 남는다.

---

## 6. 트랙 C — ISM ↔ ISKM 하이브리드

### 6.1 엔진 기반 (UE 5.8)

ISM(정적 메시 인스턴싱) 자체로는 스켈레탈 애니메이션이 불가능하다. 그러나 **5.8의 MassRepresentation은
`SkinnedMeshInstance`를 정식 표현 타입으로 갖고 있다.**

| 요소 | 클래스 / 구조체 |
|:---|:---|
| 표현 타입 | `EMassRepresentationType::SkinnedMeshInstance` |
| Trait 필드 | `UMassVisualizationTrait::SkinnedMeshInstanceDesc` — 현재 쓰는 `UMassCrowdVisualizationTrait`이 이미 상속한다 |
| 메시 서술 | `FMassSkinnedMeshInstanceVisualizationMeshDesc` (Asset / TransformProvider / Min·MaxLODSignificance / MaterialOverrides / bCastShadows) |
| 애니 주입 | `FMassInstancedSkinnedMeshInfo::AddBatchedAnimationData(FAnimSequenceTrackAutoPlayData)` |
| 소비 프로세서 | `UMassConsumeInstancedSkinnedMeshAnimationProcessor` |
| 백엔드 | `UInstancedSkinnedMeshComponent` + `UAnimSequenceTransformProviderData` (GPU 전용) |

프로세서가 채워 넣는 값은 이것뿐이다:

```cpp
FAnimSequenceTrackAutoPlayData { SequenceIndex, Position, PlayRate, BlendTime, LoopMode }
```

`BlendTime`이 있으므로 Idle ↔ Move ↔ Attack 전환에 블렌드까지 걸린다.
즉 직접 만들 것은 **"행동 상태 → SequenceIndex 매핑" 프로세서 하나**뿐이다.

### 6.2 유의도 구간 분배

| 구간 | 표현 | 근거 |
|:---|:---|:---|
| High (근거리) | ISKM | 공격 모션이 읽혀야 하는 거리 |
| Medium | ISKM (그림자 off) | 실루엣과 동작만 |
| Low | ISM | 점처럼 보이는 거리에서 스키닝 비용을 낼 이유가 없다 |
| Off | 없음 | `ULNPEnemyTrait::ReplicationCullDistance`와 값을 맞출 것 (기존 규칙) |

`MinLODSignificance` / `MaxLODSignificance`가 메시 서술 단위로 있으므로
**이 분배는 코드가 아니라 데이터로 조정된다.**

⚠️ **`PureEntity`용 EntityConfig는 `LODRepresentation`에서 Actor 단계를 없애고 템플릿 Actor를 비워야 한다.**
`ULNPEnemyLODOverrideProcessor`가 High 강제를 건너뛰어도, 거리가 가까우면 거리 기반 LOD가
자연히 High가 되고 표현 매핑에 Actor가 있으면 그대로 스폰된다. **모드 enum과 표현 매핑이 어긋나면
"승격 안 하기로 한 개체가 가까이 가니까 Actor가 된다"** — 실제로 밟았다(2026-09-05).
단일 진실은 enum이고, EntityConfig는 그 모드에서 실제로 쓸 비주얼을 정의할 뿐이다.
어긋남은 `ULNPEnemyTrait::ValidateTemplate`이 **양방향으로** 경고한다(`PureEntity`인데 Actor가 남았다 /
`ActorPromoted`인데 Actor가 없다). 경고만 남기고 `false`는 돌려주지 않는다 — 어긋남은 고쳐야 할 설정이지
스폰을 막을 사유가 아니다. 트레이트는 `GetTypedOuter<UMassEntityConfigAsset>()`로 소유 Config를 얻어
부모 체인의 `UMassVisualizationTrait::Params`를 읽는다(`BuildContext`의 템플릿 데이터는 protected다).

⚠️ **차단을 코드로 강제하려던 접근은 폐기했다.** LOD 값을 "Actor를 쓰지 않는 첫 단계"까지 눌러
덮어쓰는 방식을 먼저 시도했는데, **`FMassRepresentationLODFragment::LOD`는 표현뿐 아니라 유의도·틱
레이트까지 정하는 값**이라 표현 하나를 막으려고 나머지까지 끌어내리게 된다. `MassCrowdVisualizationTrait`이
이미 "LOD 단계별로 무엇으로 그릴지"를 데이터로 갖고 있으므로, 그 위에서 싸우지 말고 **그 트레이트에게
시키는 것**이 맞다. 코드는 "전투로 LOD를 끌어올리지 않는다"까지만 한다.

### 6.3 무기 — 손 본 웨이팅 스킨드 메시

ISKM에는 소켓 본 어태치가 없다. 대신 `Desc.Meshes`가 배열이고, 항목마다 ISKM 컴포넌트가 하나씩 생성되어
**동일 트랜스폼·동일 애니 데이터**를 받는다 (`MassVisualizationComponent.cpp:1047`).

그러므로 무기를 **같은 스켈레톤의 손 본에 100% 웨이팅한 스킨드 에셋**으로 만들어 두 번째 항목으로 등록하면
GPU 스키닝이 무기를 손 위치로 옮겨 준다. 엔진의 모듈러 캐릭터 방식과 같은 원리다.

- ⚠️ 무기 종류가 늘면 무기마다 스킨드 에셋을 구워야 한다. 다만 적은 **무기를 교체하지 않으므로**
  (`ULNPEnemyConfig::WeaponData` 고정 1개) 조합 폭발이 없다 — 이 제약이 여기서 이득이 된다.
- ⚠️ **`BladeInner` / `BladeOuter`는 이 메시의 실제 길이와 손으로 맞춰야 한다.** 애니메이션에서 뽑을 수
  없으므로 Config 상수이고, 어긋나면 "칼이 안 닿았는데 맞는다"가 된다.
  `ULNPWeaponTraceDebugDrawProcessor`로 눈으로 맞출 것.

### 6.4 시퀀스 인덱스 규약

| Index | 시퀀스 | Loop |
|:---:|:---|:---|
| 0 | Idle | Loop |
| 1 | Move | Loop |
| 2 | Attack | Once |
| 3 | Stagger | Once |
| 4 | Death | Once (마지막 프레임 유지) |

⚠️ 인덱스는 Provider에 구워진 배열 순서다. **애니 프로세서에 하드코딩하지 말고 Config에 매핑 테이블을
둔다** — 적 타입마다 시퀀스 수와 순서가 다르다.

### 6.5 애니 타이밍과 판정 타이밍의 단일 정의

⚠️ **AnimNotify가 없으므로 애니메이션에서 히트 타이밍을 뽑을 수 없다.**
Windup/Active/Recovery는 `FLNPEntityAttackConfig` 상수가 정의하고, 애니는 그 상수에 **맞춰 재생된다.**

```
PlayRate = SequenceLength / (WindupTime + ActiveTime + RecoveryTime)
```

- 이 관계는 데이터로만 묶여 있어 어긋나도 컴파일도 실행도 실패하지 않는다 — **조용히 어긋난다.**
- 그래서 `PlayRate`를 **프로세서가 계산**한다. 애니 속도를 상수에서 파생시키는 방향이지 그 반대가 아니다.
  Config를 고치면 그림이 따라온다.
- `FLNPEnemyMovementConfig::ComputeStopDistance()`가 정지 거리에 대해 하는 일과 같은 계열의 규약이다.

### 6.6 포즈 연속성은 포기한다

`ActorPromoted` 개체의 ISKM ↔ Actor 전환에서 포즈가 튄다. 지금도 ISM ↔ Actor 팝이 있으므로
새 문제는 아니지만, 애니가 붙으면 더 눈에 띈다. **수용한다.**

- 승격은 전투 진입 순간(거리 무관)에 일어나므로 화면 밖이거나 카메라 주목 대상이 아닌 경우가 많다.
- 연속성을 맞추려면 Actor 쪽 ABP의 포즈를 ISKM 시퀀스 위상과 동기화해야 하는데, 그것은
  Motion Matching 로코모션(→ [TechDesign_CombatAnimation.md](TechDesign_CombatAnimation.md))과
  근본적으로 맞지 않는다. 비용이 이득을 압도한다.

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

### 7.1 검증 기록 (2026-09-05, 완료)

2P `-game` 밀도 4, 게스트가 혼성 무리에서 교전. 14표본에서 **한도 초과 0건**,
`promoted` 최대 2, `ranged`는 `promoted`가 꽉 찬 동안에도 7까지 올라갔다 —
**풀이 서로 예산을 뺏지 않는다**는 것이 이 분할의 목적이고 그대로 성립했다.
사용자 확인: *"근접 적이 한번에 2기까지만 나를 공격하고 나머지는 쳐다보기만, 원거리는 계속 추격·공격."*

처음 측정에서는 **`melee` 풀이 0/10으로 비어 있었다** — `PureEntity` 근접 에셋이 없었기 때문이다.
그 상태에서는 §10의 기준(*"잡몹에 둘러싸인 상태에서 `ActorPromoted` 개체가 교전에 진입한다"*)을
구성할 수 없어, 대칭형(원거리 잡몹 20칸 + 승격 2칸)으로만 확인했다.

**2026-09-06에 `DA_Enemy_PureEntity_Melee01`을 만들어 기준 그대로 검증했다.**
근접 잡몹이 `melee` 풀로 이동하면서 *"승격 Actor 2기와 **별개로** 근접 잡몹 10기가 동시에 달려든다"* 가
육안으로 확인됐다. 순수 엔티티 근접 공격·피격 모션·HP 차감·패링 이펙트도 함께 정상이었다
(⚠️ 단, 패링 **넉백**은 걸리지 않는다 — 트래커에 등록된 별건이다).

⚠️ **에셋 이름에 `CombatMode`를 넣는다.** `DA_Enemy_ActorPromoted_Melee01` /
`DA_Enemy_PureEntity_Melee01` / `DA_Enemy_PureEntity_Ranged01`처럼 enum 이름을 그대로 쓰고,
EntityConfig도 같은 규칙으로 1:1 짝을 맞춘다 — 모드와 표현 매핑이 어긋나는 §6.2의 함정을
`ValidateTemplate` 경고가 나기 전에 **에셋 목록에서 눈으로** 막기 위해서다.
신규 `PureEntity` 설정은 **표현 매핑이 이미 `PureEntity`인 것을 복제**해 만드는 편이,
Actor 단계가 든 배열을 손으로 고치는 것보다 안전하다.

#### `MaxPromotedSlotsPerPlayer`를 일시적으로 올릴 때의 상한

**실측 근거가 있는 값은 10이다** — Stage 6 이전 구성이 정확히 그것이었고(근접 적이 `melee` 풀 10칸을
썼다), 그 구성에서 밀도 4·2P·게스트 전투 피크가 **58.9 KB/s = 안전판(150,000)의 39%, 포화 0건**이었다.
다만 이는 "10까지 허용된 상태의 실측"이지 **10기 동시 승격이 실제로 찍혔다는 증거는 아니다.**

| 값 | 근거 |
|---:|:---|
| ~6 | 실측된 구성(10) 안쪽. 재측정 없이 올려도 되는 범위 |
| 10 | **예전에 돌던 구성 그 자체.** 밀도 4에서 상한의 39% |
| 10 초과 | **미측정.** 반드시 재측정할 것 |

⚠️ **산술 외삽으로 정하지 말 것.** 승격 1기당 700~900 B/s라는 단가로 나눠서 "아직 25칸 남았다"를
계산할 수 있지만, 두 점으로 세운 선을 외삽하지 않는다는 규약(→ [Guide_NetBandwidth.md](Guide_NetBandwidth.md) §3.8)에
정면으로 걸리고, **CPU(Actor 틱·Mover·애니메이션)는 아예 측정된 적이 없다** — 대역폭보다 먼저 물릴 가능성이 높다.

⚠️ **슬롯은 플레이어당이다.** 월드 전체 승격 수 = 값 × 플레이어 수이고, 각 클라이언트는 자기에게
릴러번트한 승격 Actor 전부의 비용을 낸다. 4P는 같은 값에서 2P의 두 배가 된다.

⚠️ **초과의 증상은 렉이 아니라 "조용한 소실"이다** (→ [TechDesign_Networking.md](TechDesign_Networking.md) §4.7).
포화하면 어트리뷰트 갱신이 재전송되지 않고 다음 값에 덮여 사라진다(먼 적의 HP 바가 아예 안 뜬다).
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
| `PureEntity` | 랙돌 방송 없음 + `DeathCountdown = ULNPEnemyConfig::PureEntityDeathDuration` (기본 1.5초) |

⚠️ **소멸 지연에 하한이 있다.** 엔티티 파괴가 곧 버블 제거이므로, 이 시간이 복제 LOD의 최장
갱신 주기(0.3초)와 넷 틱 몇 개를 덮지 못하면 **게스트가 `Dying`을 받기도 전에 적이 사라진다.**
`Dying` 전이는 일회성이라 게이트를 우회하지만, 그래도 패킷이 한 번은 나가야 한다.

랙돌은 `ActorPromoted` 전용 연출로 남는다. `PureEntity`가 그냥 사라지지 않는 이유는
행동 상태 채널이 `Dying`을 전파하고 ISKM이 Death 시퀀스를 재생하기 때문이다 —
**트랙 B가 없으면 순수 엔티티는 소리 없이 소멸한다.**

⚠️ **`Dying`을 싣는 프로세서의 쿼리에 `FLNPEnemyDyingTag`를 `None`으로 걸면 안 된다.**
적 프로세서 대부분이 그렇게 하고 있어 그대로 베끼기 쉬운데, 그러면 죽는 순간 엔티티가 쿼리에서 빠져
**아무도 `Dying`을 싣지 못한다.**

---

## 9. 남은 결정 / 검증 필요 항목

| # | 항목 | 성격 |
|:---:|:---|:---|
| 1 | **Nanite 스킨 요건** — `UInstancedSkinnedMeshComponent` 렌더 경로가 현 메시·머티리얼 설정과 호환되는가 | **트랙 C의 유일한 미지수.** Stage 5a에서 판명된다 |
| 2 | 시퀀스 에셋 준비 — 적 전용 Attack/Stagger/Death, 무기 스킨드 에셋 굽기 | 에셋 파이프라인 작업 |
| 3 | 수치 튜닝 — Promoted 슬롯 수, 위상 시간, 가상 칼날 치수, 유의도 경계 | 플레이 테스트 |
| 4 | 상태 채널 확장 대상 | **Aim Pitch는 넣었다**(2026-09-05 고저차 교전에서 확인). 피격 방향·HP 비율은 계속 보류 (§5.5) |
| 5 | 사망 연출 최종 형태 — Death 시퀀스 + 소멸 VFX 여부 | 기획 |
| 6 | 엘리트 고도화 행동(특수 어빌리티) | **이번 범위 밖.** `ActorPromoted` 경로는 이번 작업에서 바뀌지 않는다 |

---

## 10. 구현 플랜

| Stage | 내용 | 검증 기준 |
|:---:|:---|:---|
| **0** ✅ | `ELNPEnemyCombatMode` 도입 + LODOverride 분기 + `PureEntity`용 EntityConfig 표현 매핑 분리 | `PureEntity` 개체가 `Confirmed`가 되어도 Actor가 스폰되지 않고, **가까이 가도** 스폰되지 않는다. 추격까지는 정상 동작 |
| **1** ✅ | 순수 엔티티 원거리 공격 (트랙 A) | 게스트 화면에 아무것도 안 보여도 서버에서 플레이어 HP가 깎인다. **패링 반사도 그대로 성립한다** |
| **2** ✅ | 순수 엔티티 근접 공격 — 가상 칼날 (트랙 A) | **플레이어가 순수 엔티티의 근접 공격을 패링할 수 있다.** 파이프라인 재사용이 성립했다는 단일 증거. 가드 블록·경직 누적도 함께 확인 |
| **3** ✅ | 행동 상태 채널 (트랙 B) | 2P에서 게스트가 공격 시작을 인지한다(먼저 디버그 드로우로 확인). **연속 공격 2회가 2회로 보인다** — 전이 카운터 검증 |
| **4** ✅ | 발사체 관전 가시성 | 게스트 화면에 엔티티 발사체가 보이고 임팩트 지점이 서버 판정과 일치. **새 RPC 없이** 상태 채널만으로 |
| **5a** | ISKM 파이프라인 개통 — Idle/Move 두 시퀀스만 | 인스턴스가 보이고 걷는다. **Nanite 요건 실측이 여기서 끝난다** |
| **5b** | Attack/Stagger/Death 시퀀스 + PlayRate 파생 | 공격 위상과 모션 길이가 맞는다. Config 수치를 바꾸면 모션 속도가 따라온다 |
| **5c** | 무기 스킨드 메시 | 무기가 손에 붙어 따라오고, 가상 칼날 디버그 드로우가 무기 실루엣과 겹친다 |
| **6** ✅ | 슬롯 풀 3분할 + 문자열 비교 제거 | 잡몹에 둘러싸인 상태에서 `ActorPromoted` 개체가 교전에 진입한다 (→ §7.1 — 현재는 대칭형으로만 검증 가능) |

**의존 관계:** 1·2 → 0 / 4 → 3 / 5 → 3 / 6은 독립. §8(사망 배선)은 3에 딸려 함께 끝났다.
트랙 C(5a~5c)가 Nanite 문제로 막혀도 트랙 A·B는 영향받지 않는다 — ISM 비주얼 그대로 전투는 성립한다.

**PIE 검증 분담:** Stage 0~2의 서버 판정은 로그·디버그 드로우로 자동 확인 가능하다.
Stage 3~5의 2P 체감(연출 타이밍·모션 자연스러움)은 실제 플레이 확인이 필요하다.

### 10.1 트랙 A 검증 기록 (2026-09-05, 완료)

승격 차단 · 근접 적(`ActorPromoted`) 회귀 없음 · 산탄 · 가드 · 패링 반사(적도 부근 포함, 2회 반사로 처치) ·
상하 조준 사격(고저차 지형) · **그로기 진입 시 공격·이동 정지와 임계 이탈 시 재개** · 호스트/게스트 양쪽.

⚠️ **경직 관찰에는 임시값이 필요했다.** 경직이 쌓이기 전에 적이 죽어 그로기 구간을 볼 수 없었다 —
공격력·HP 밸런스 문제이지 임계값 문제가 아니다(→ [DevelopmentPlan.md](DevelopmentPlan.md) Phase 3 경직 항목의
같은 지적). 플레이어 무기 레벨1 공격력을 0으로, 원거리 NPC 체력을 5배로 올려 관찰했고 **원복 대상이다.**

⚠️ **적 엔티티의 HP·방어력 원본은 무기(`WeaponData`)의 스탯 수정자다.**
`ULNPEnemyConfig::InitialAttributeValues`는 Actor 승격 후 ASC 초기화에만 쓰여 순수 엔티티에는 반영되지 않는다.

### 10.2 트랙 B 검증 기록 (2026-09-05, 완료)

**2P Standalone(`-game`), 밀도 1, 게스트가 전투 안에서 플레이.** 확인된 항목:

- 게스트 화면에 엔티티 발사체가 보이고 **관통하지 않는다** — 플레이어에 막히고 착탄 이펙트가 재생된다.
- **빨간 상자 1회 = 공격 1회.** 누락도 중복도 없었다(전이 카운터가 하는 일이 이것 전부다).
- 호스트와 게스트에서 **같은 적의 상자 색이 동시에 같은 값으로 갱신**된다 — 단일 소비 경로가 성립했다.
- 고저차에서 위·아래·평면 사격을 모두 유도해 **조준각 동기 확인**. "피했다고 생각하면 피해지고
  맞았다고 생각하면 맞는" 타이밍까지 자연스럽다.
- 사망 시 `Dying` 후 1.5초 뒤 소멸.
- 근접 적(`ActorPromoted`) 회귀 없음. assert·ensure 0건.

**대역폭 (210구간 = 17.5분):** 행동 사유만인 Dirty **1.39%**, 게이트 우회 **0.24회/s(Dirty의 1.0%)**,
피크 송신 22.1 KB/s(상한의 15%), 포화 0건. 상세와 방법은 [Guide_NetBandwidth.md](Guide_NetBandwidth.md) §7.

⚠️ **계측 세션의 함정 — 버블은 클라이언트별이라 계측이 게스트 카메라를 따라간다.**
첫 회차에서 호스트로 플레이했더니 게스트 폰이 스폰 지점에 서 있어 가시 적이 2~10기뿐이었고,
전투 대역폭이 전혀 잡히지 않았다. 리슨 서버의 호스트에는 `NetConnection`이 없어 송신량이 0이다 —
**측정하려는 클라이언트가 직접 전투 안에 있어야 한다.**

---

## 11. 위험 요소

| 위험 | 징후 | 대응 |
|:---|:---|:---|
| Nanite 스킨 미호환 | ISKM이 안 보이거나 머티리얼이 깨짐 | 트랙 C만 중단. A·B는 영향 없음(ISM 유지) |
| 스윙 엔티티 누수 | 프레임당 엔티티 수가 단조 증가 | `TimeToLive` 안전장치가 이미 있다. 경직·사망 파괴 경로를 반드시 넣을 것 (§4.5) |
| 상태가 게스트에 안 감 | 서버에선 공격하는데 게스트는 가만히 서 있음 | Fast Array Dirty 표시 누락을 먼저 의심 (§5.3) |
| 판정과 그림의 어긋남 | "칼이 안 닿았는데 맞는다" | `BladeOuter`와 무기 메시 길이 불일치. 디버그 드로우로 대조 (§6.3) |
| 아키타입 분리 비용 | 청크 파편화 | 공격 프래그먼트를 두 모드가 공유하므로 새 분리가 생기지 않는다 (§4.2) |
