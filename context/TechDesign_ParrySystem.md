# 패링 시스템 기술 설계

> **⚠️ 구현 상태: 전체 미구현.** `FLNPParryStateFragment`, 패링 판정 로직, 반사/넉백 처리 등 관련 C++ 코드 일체 존재하지 않습니다.
> 단, **패링 입력(`GuardAction`)은 `ULNPInputHandlerComponent`에 이미 바인딩**되어 있으며, `bIsGuardPressed` / `bIsGuardJustPressed` 신호를 제공합니다.
> HitDetection 시스템 구현 완료 후 연계 작업. `Phase 3` 작업 대상.
> 전제: [TechDesign_HitDetection.md](TechDesign_HitDetection.md)의 판정 프로세서 구현 완료 필요.

---

## 1. 설계 방향

HitDetection Processor가 이미 공격자와 방어자 사이의 거리와 방향을 계산하므로, 패링 판정을 별도 프로세서로 분리하지 않고 **HitDetection Processor 내부에서 일괄 처리**.

```
ULNPHitDetectionProcessor::Execute()
├─ 공격 엔티티 → 타겟 엔티티 거리/방향 계산
├─ if (Distance <= ParryRadiusSq)
│       → FParryStateFragment 활성 여부 확인
│       → 방향(Dot Product) 조건 검사
│       └─ if (패링 성공) → 패링 처리 분기
│          else → 일반 피격 처리
└─ 피격 처리: 데미지, 넉백, HitStop
```

---

## 2. 데이터 구조

### 2.1 신규 Fragment

**`FLNPParryStateFragment`** — 패링 가능 엔티티에 부착
```cpp
bool  bIsParrying;       // 패링 입력 활성 여부 (입력 윈도우 동안 true)
float ParryAngleCos;     // cos(45°) = 0.707 — 방향 판정 임계값
float ParryRadiusSq;     // 패링 유효 거리 제곱
```

### 2.2 기존 Fragment 활용

`FLNPHitDetectionFragment` (→ [TechDesign_HitDetection.md](TechDesign_HitDetection.md) 참조)에서 이미 계산된 `AttackDirection`과 거리 정보를 재사용.

---

## 3. 패링 성공 조건

```
if (DistanceSq <= Parry.ParryRadiusSq)
    && (Parry.bIsParrying)
    && (Dot(DefenderForward, -AttackDirection) >= Parry.ParryAngleCos)
→ 패링 성공
```

**방향 조건 시각화:**
```
공격 방향: →
방어자 정면: ←  (공격 반대 방향과 45° 이내면 성공)
```

---

## 4. 성공 시 처리

### 4.1 근거리 패링 (Melee)

**방어자:**
- 패링 성공 애니메이션 재생
- HitStop (역경직, `CustomTimeDilation` 또는 `ExecutionSpeed` 조정)

**공격자:**
- Stagger 애니메이션 재생
- 방어자 반대 방향 + 위쪽으로 포물선 궤적 Launch
  ```
  LaunchDir = Normalize(-DefenderForward + UpDir * 0.5f)
  Launch(LaunchDir × LaunchSpeed)
  ```

### 4.2 원거리 패링 (Projectile)

투사체 타입별 반사 처리는 `ULNPProjectileMovementProcessor`에 위임.

| 타입 | 처리 |
|:---|:---|
| `Linear` | `Velocity = -Velocity` |
| `Guided` | `bIsGuided = false`, `Velocity = DefenderForward × OriginalSpeed` |
| `Lobbed` | 포물선 함수의 시간 매개변수 역전, 또는 시작/목표 지점 스왑 |

---

## 5. 입력 및 윈도우 관리

패링은 정확한 타이밍에 입력해야 성공하는 스킬. `FLNPParryStateFragment::bIsParrying`의 활성 시간 제어가 핵심.

### 5.1 입력 소스

`GuardAction`은 이미 `ULNPInputHandlerComponent`에 바인딩되어 있음. 콜백은 다음 두 신호를 유지함:

```cpp
// ULNPInputHandlerComponent (구현 완료)
bool bIsGuardPressed;      // GuardAction 유지 중
bool bIsGuardJustPressed;  // 해당 프레임 최초 입력 (ProduceInput 이후 false로 초기화)
```

### 5.2 입력 윈도우 처리 흐름

`GuardAction` 입력 → `bIsGuardJustPressed = true` → 이를 `FLNPParryStateFragment::bIsParrying`에 연결하여 짧은 윈도우 동안 true로 유지.

**구현 옵션 A — GuardAction 콜백 직접 연결:**
```
OnGuardStarted()
└─ FMassDeferredSetCommand → bIsParrying = true
OnGuardReleased() 또는 타이머
└─ FMassDeferredSetCommand → bIsParrying = false
```
- 별도 GA 없이 가장 단순한 경로.
- 단, 패링 성공 시 재생할 몽타주(패링 성공 애니메이션)가 필요하면 GAS 호출을 추가해야 함.

**구현 옵션 B — GAS 어빌리티 경유:**
```
GA_Parry::ActivateAbility()   ← GuardAction이 GA를 트리거
├─ PlayMontage(ParryMontage)
├─ FMassDeferredSetCommand → bIsParrying = true  (입력 윈도우 시작)
├─ WaitForAnimNotify("ParryWindowEnd")
└─ FMassDeferredSetCommand → bIsParrying = false (입력 윈도우 종료)
```
- 애니메이션 타이밍과 판정 윈도우가 자동으로 일치하는 장점.
- AnimBP 슬롯 구조(→ [TechDesign_CombatAnimation.md](TechDesign_CombatAnimation.md)) 완성 후 권장.

> **현재 방향:** `GuardAction` 입력 소스는 확정. 윈도우 제어 방식(옵션 A or B)은 AnimBP 레이어 구현 진척에 따라 결정.

---

## 6. 아키텍처 고려사항

### Mass-GAS 브릿지

MassEntity는 Actor가 아니므로 GAS의 `GameplayEffect`가 직접 적용되지 않음. 패링 결과를 GAS에 전달하는 방법:

**방법 A: Lightweight Actor 경유**
- 각 엔티티에 대응하는 `ALNPEnemyCharacter`가 있을 때 (High LOD 상태)
- 패링 판정 결과 → `ALNPEnemyCharacter::ApplyParryEffect()` 호출 → GAS Effect 적용

**방법 B: Fragment 중계**
- 판정 결과를 `FLNPPendingEffectFragment`에 기록
- 별도 `ULNPGASBridgeProcessor`가 게임스레드에서 GAS Effect로 변환

**권장:** 방법 A (High LOD 상태에서만 패링 가능하도록 설계하면 자연스럽게 해결)

### 쿼리 최적화

패링 조건 검사는 `FLNPParryStateFragment`를 보유한 엔티티에 대해서만 수행. HitDetection Processor의 쿼리에 옵셔널 필터로 추가하여 `bIsParrying == false`인 엔티티는 패링 체크를 생략.
