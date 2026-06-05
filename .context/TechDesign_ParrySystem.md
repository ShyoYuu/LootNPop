# Guard / 패링 시스템 기술 설계

> **구현 상태:**
> - **Guard(막기):** ✅ 완료 + 동작 확인 — Fragment 기반 각도 판정, 데미지 차단, FLNPGuardBlockCommand
> - **Parry(근접):** ✅ 완료 — 0.15초 창, FLNPMeleeParryCommand (방어자 GA_ParrySuccess + 공격자 GA_Stagger)
> - **Parry(투사체):** ✅ 완료 + 동작 확인 — Processor에서 Velocity 반전/InstigatorTeam 전환, FLNPProjectileParryCommand
> - **판정 구조 리팩토링:** ✅ 완료 — FLNPApplyDamageGECommand 간소화, Command → Processor 판정 이전
> - **패링/피격 반경 분리:** ✅ 완료 + 동작 확인 — ParryRadius(패링) / HitRadius(피격) 독립 제어, 2단계 판정 흐름
> - **Guard 자세 애니메이션:** 🔲 에디터 작업 필요 (C++ 변수 준비 완료, ABP 분기 미연결)
> - **GameplayCue 에셋:** 🔲 에디터 작업 필요 (태그 정의 완료, 에셋 연결 대기)

---

## 1. 설계 원칙

| 역할 | 수행 위치 | 근거 |
|:---|:---|:---|
| 패링/가드 **상태 보관** | `FLNPParryStateFragment` (Mass) | Worker Thread에서 직접 읽기 가능 |
| 패링/가드 **판정** | Processor (Worker Thread) | Fragment 직접 접근, 판정 비용이 낮음 |
| **후처리** (GAS 이벤트, VFX) | BatchedCommand (Game Thread) | GAS/ASC 접근 필요 |
| Projectile **Fragment 변경** | Processor (Worker Thread) | ReadWrite Fragment이면 직접 가능, 별도 Command 불필요 |

ASC 태그(`TAG_State_Guarding`, `TAG_State_ParryWindow`)는 기존 GAS Ability 조건, 애니메이션 상태머신과의 호환을 위해 그대로 유지한다. `FLNPParryStateFragment`는 Worker Thread에서 동일 상태를 읽기 위한 **Mirror** 역할을 담당한다.

---

## 2. 데이터 구조

### 2.1 FLNPParryStateFragment (확장)

```cpp
// LNPGuardParryTypes.h
USTRUCT()
struct LOOTNPOP_API FLNPParryStateFragment : public FMassFragment
{
    bool  bIsParrying   = false;   // 패링 창 활성 여부 (TAG_State_ParryWindow 미러)
    bool  bIsGuarding   = false;   // 가드 중 여부 (TAG_State_Guarding 미러)
    float ParryAngleCos = 0.707f;  // cos(45°) — 패링 허용 각도
    float GuardAngleCos = 0.5f;    // cos(60°) — 가드 허용 각도
};
```

### 2.2 Fragment 갱신 방법

`ULNPInputHandlerComponent`에서 ASC 태그 변경과 동시에 Fragment를 직접 갱신한다 (Game Thread).
Player의 Mass Entity Handle은 Player Character 또는 InputHandlerComponent가 보유.

```
OnGuardStarted()
├─ ASC: AddLooseGameplayTag(TAG_State_Guarding)
├─ ASC: AddLooseGameplayTag(TAG_State_ParryWindow)
├─ Fragment: bIsGuarding = true, bIsParrying = true
└─ 타이머(ParryWindowDuration, 기본 0.15초) 시작
    └─ 만료 → ASC: RemoveLooseGameplayTag(TAG_State_ParryWindow)
            → Fragment: bIsParrying = false

OnGuardReleased()
├─ ASC: RemoveLooseGameplayTag(TAG_State_Guarding)
├─ ASC: RemoveLooseGameplayTag(TAG_State_ParryWindow)
├─ Fragment: bIsGuarding = false, bIsParrying = false
└─ 타이머 취소
```

### 2.3 FLNPProjectileFragment (변경 없음, ReadWrite 접근 추가)

```cpp
struct FLNPProjectileFragment : public FMassFragment
{
    FVector Velocity;          // 반사 시 Processor에서 직접 반전
    ELNPInstigatorTeam InstigatorTeam;  // 반사 시 Player로 변경
    FMassEntityHandle  Instigator;      // 반사 시 Player Entity Handle로 교체 (자기 피격 방지)
    // ...
};
```

### 2.4 GameplayTag

| 태그 | 설명 |
|:---|:---|
| `LNP.State.Guarding` | Guard 버튼 누르는 동안 ASC에 유지 |
| `LNP.State.ParryWindow` | Guard 입력 직후 ParryWindowDuration 동안만 활성 |
| `GameplayCue.LNP.Guard.Block` | 가드 성공 VFX/SFX |
| `GameplayCue.LNP.Parry.Success` | 패링 성공 VFX/SFX |
| `LNP.GameplayEvent.Parry.Success` | 방어자 GAS Ability 트리거 (몽타주, HitStop 등) |
| `LNP.GameplayEvent.Parry.Stagger` | 공격자 GAS Ability 트리거 (Stagger, Launch) |

---

## 3. 판정 흐름

패링과 피격은 **독립된 반경**으로 2단계에 걸쳐 판정한다. 이므로 패링이 우선 검사되고, 두 반경 사이 거리에서는 패링 조건을 충족할 때만 반응한다.

```
[Processor — Worker Thread]
  각도 사전 계산 (AttackerDir / IncomingDir → Dot)

  ── 1단계: 패링 체크 (ParryRadius) ──────────────────────────────
  bIsParrying && Dot >= ParryAngleCos && dist <= ParryRadius
      ├─ 근접(Melee)   → MarkHit + FLNPMeleeParryCommand(victimActor, attackerActor)
      └─ 투사체(Proj)  → [Fragment 직접 변경]
                          Proj.Velocity      = -Proj.Velocity
                          Proj.InstigatorTeam = Player
                          Proj.Instigator     = Player.Handle
                        → FLNPProjectileParryCommand(victimActor)
                        → FLNPProjectileDeadTag 추가 안 함 (투사체 계속 비행)

  ── 2단계: 피격 체크 (HitRadius) — 패링 미발동 시에만 도달 ──────
  dist <= HitRadius
      ├─ bIsGuarding && Dot >= GuardAngleCos
      │       └─ FLNPGuardBlockCommand(victimActor)
      │          + 투사체인 경우 소멸 처리 (DeadTag + Impact VFX)
      └─ 위 조건 없음
              └─ FLNPApplyDamageGECommand(victimActor, effectClass, damage)
                 + 투사체인 경우 소멸 처리

[Commands — Game Thread]
  FLNPMeleeParryCommand     victimASC → HandleGameplayEvent(TAG_GameplayEvent_Parry_Success)
                            attackerASC → HandleGameplayEvent(TAG_GameplayEvent_Parry_Stagger)
                            victimASC → ExecuteGameplayCue(TAG_GameplayCue_Parry_Success)

  FLNPProjectileParryCommand victimASC → HandleGameplayEvent(TAG_GameplayEvent_Parry_Success)
                             victimASC → ExecuteGameplayCue(TAG_GameplayCue_Parry_Success)
                             (Velocity/Team 반전은 Processor에서 이미 처리됨)

  FLNPGuardBlockCommand     victimASC → ExecuteGameplayCue(TAG_GameplayCue_Guard_Block)

  FLNPApplyDamageGECommand  GE 적용만 (판정 로직 없음, AttackerActor 필드 제거)
```

---

## 4. Command별 책임 정의

### 4.1 FLNPMeleeParryCommand

**근접 공격 패링 성공 시 발동.** 방어자와 공격자 양쪽에 GAS 이벤트를 전달한다.

```cpp
struct FEntry { TWeakObjectPtr<AActor> VictimActor, AttackerActor; };

Run():
  victimASC → ExecuteGameplayCue(TAG_GameplayCue_Parry_Success, VictimPos)
  victimASC → HandleGameplayEvent(TAG_GameplayEvent_Parry_Success)
  attackerASC → HandleGameplayEvent(TAG_GameplayEvent_Parry_Stagger)
```

### 4.2 FLNPProjectileParryCommand

**투사체 패링 성공 시 발동.** GAS 이벤트·VFX만 담당. Velocity 반전은 Processor에서 이미 처리.

```cpp
struct FEntry { TWeakObjectPtr<AActor> VictimActor; };

Run():
  victimASC → ExecuteGameplayCue(TAG_GameplayCue_Parry_Success, VictimPos)
  victimASC → HandleGameplayEvent(TAG_GameplayEvent_Parry_Success)
  // 공격자 Stagger 없음 (투사체 패링 스펙)
```

### 4.3 FLNPGuardBlockCommand

**가드 성공 시 발동.** VFX/SFX 큐 실행. 향후 스태미나 소모 GE 추가 지점.

```cpp
struct FEntry { TWeakObjectPtr<AActor> VictimActor; };

Run():
  victimASC → ExecuteGameplayCue(TAG_GameplayCue_Guard_Block, VictimPos)
```

### 4.4 FLNPApplyDamageGECommand (간소화)

**일반 피해 적용.** 판정 로직 없음. `AttackerActor` 필드 제거.

```cpp
struct FEntry { TWeakObjectPtr<AActor> Actor; TSubclassOf<UGameplayEffect> EffectClass; float Damage; };

Run():
  ASC → ApplyGameplayEffectSpecToSelf(*Spec)
```

---

## 5. 각도 판정

```
// VictimFwd: 방어자 전방 벡터
// Dot >= 임계값이면 "공격이 전방에서 왔음"

[근접]
AttackerDir = Normalize(AttackerLocation - VictimPos)
Dot = Dot(VictimFwd, AttackerDir)

[투사체]
// 투사체 비행 방향의 역 = 공격이 날아온 방향
IncomingDir = Normalize(Proj.PreviousPos - CurrentPos)
Dot = Dot(VictimFwd, IncomingDir)

Dot >= cos(45°) = 0.707  →  패링 성공
Dot >= cos(60°) = 0.5    →  가드 성공
```

---

## 6. Projectile 패링 후 투사체 상태

| 항목 | 변경 전 | 변경 후 |
|:---|:---|:---|
| `Velocity` | 원래 방향 | `-Velocity` (반전) |
| `InstigatorTeam` | Enemy | Player |
| `Instigator` | Enemy Entity Handle | Player Entity Handle |
| `FLNPProjectileDeadTag` | 히트 즉시 추가 | 추가 안 함 (계속 비행) |
| Trail VFX | 해제 | 유지 |
| Impact VFX | 재생 | 재생 안 함 |

`InstigatorTeam = Player` 전환으로 반사 투사체는 Enemy 히트 판정 코드 경로를 탄다. `Instigator = Player.Handle`로 교체하여 반사한 Player 자신은 피격 제외 목록에 들어간다.

---

## 7. Processor 수정 사항

### 공통 변경 (WeaponTrace / Projectile 양쪽)

- `PlayerQuery`에 `FLNPParryStateFragment` **Required** ReadOnly 요구사항 추가 (Optional 아님).
- `FCollectedPlayer`: `const FLNPParryStateFragment* ParryFrag` (포인터) → `FLNPParryStateFragment ParryState` (값 복사, 널 체크 불필요).
- `FCollectedPlayer`에 `FVector UpDir` 추가, `FCollectedEnemy`에 `FVector UpDir` + `FVector CapsuleCenter` 추가 (수집 단계 선계산).

### 판정 반경 분리

`FLNPProjectileSharedFragment` / `FLNPWeaponTraceFragment` 양쪽에 독립 반경 필드 추가:

| 필드 | 설명 | 기본값 (투사체/근접) |
|:---|:---|:---|
| `HitRadius` | 피격 판정 반경 | 5.0 / 10.0 |
| `ParryRadius` | 패링 판정 반경 (`> HitRadius`) | 6.0 / 12.0 |

이전의 `HitRadiusSq` + `FMath::Sqrt()` 왕복 패턴은 제거. 쓰기 지점(`ANS_LNPMeleeHitWindow`, `LNPAbility_RangedAttack`)에서 반경값을 직접 저장한다.

### ULNPProjectileHitDetectionProcessor 추가 변경

- `ProjectileQuery`의 `FLNPProjectileFragment` 접근을 **ReadOnly → ReadWrite**로 변경.
- 히트 분기 (2단계):
  1. `ParryRadius` 체크 → 패링 성공: Fragment 직접 변경 + `FLNPProjectileParryCommand` (DeadTag 없음, `break`)
  2. `HitRadius` 체크 → 가드: `FLNPGuardBlockCommand` + 소멸 / 일반 피격: `FLNPApplyDamageGECommand` + 소멸

---

## 8. 제약사항

- **액터 보유 엔티티 전용:** 가드/패링 판정은 `Actor` 포인터가 있는 엔티티에서만 작동. 순수 엔티티 NPC는 판정 대상에서 자동 제외.
- **Player만 현재 지원:** Enemy Actor 패링은 `FLNPParryStateFragment` 연결 후 별도 Phase에서 지원.
- **투사체 타입:** 현재 모든 투사체는 Linear 반사(180도)만 지원. Guided/Lobbed 타입은 게임 기획서의 Phase 2 범위.
