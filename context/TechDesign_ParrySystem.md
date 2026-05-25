# 패링 시스템 기술 설계

> **구현 상태:** 미구현. HitDetection 시스템 구현 후 연계 작업. `Phase 3` 작업 대상.  
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

## 5. 입력 윈도우 관리

패링은 정확한 타이밍에 입력해야 성공하는 스킬. `FLNPParryStateFragment::bIsParrying`의 활성 시간 제어가 핵심.

**GAS 어빌리티로 구현 (권장):**
```
GA_Parry::ActivateAbility()
├─ PlayMontage(ParryMontage)
├─ FMassDeferredSetCommand → bIsParrying = true  (입력 윈도우 시작)
├─ WaitForAnimNotify("ParryWindowEnd")
└─ FMassDeferredSetCommand → bIsParrying = false (입력 윈도우 종료)
```

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
