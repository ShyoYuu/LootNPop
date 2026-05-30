# HitDetection 시스템 기술 설계

> **구현 상태:** 미구현. Mass 프로세서 구조와 인터페이스 설계 완료. `Phase 3` 작업 대상.

---

## 1. 설계 원칙

- **No Physics Collision:** Chaos 엔진의 콜리전 이벤트 미사용. 물리 엔진을 거치지 않으므로 수천 개의 엔티티에서도 일정한 성능 유지.
- **Mass Processor 집중:** 모든 판정은 `FMassProcessor::Execute()` 내에서 수학적으로 일괄 계산.
- **서버 권위:** 판정 결과는 서버 프로세서에서만 최종 결정. 클라이언트는 VFX/HitStop만 처리.

---

## 2. 근거리 판정 (Melee — Swept Volume)

### 2.1 데이터 구조

**`FLNPMeleeAttackFragment`**
```cpp
FVector SwordTipPrev;   // 이전 프레임 칼끝 위치
FVector SwordRootPrev;  // 이전 프레임 칼밑 위치
FVector SwordTipCurr;   // 현재 프레임 칼끝 위치
FVector SwordRootCurr;  // 현재 프레임 칼밑 위치
float   HitRadiusSq;    // 판정 반경 제곱
```

### 2.2 판정 알고리즘

이전/현재 프레임의 칼날 위치가 쓸고 지나간 사각형 영역(Swept Area)과 타겟의 반구 캡슐 중심점 사이의 최단 거리를 계산.

1. 사각형을 두 개의 삼각형으로 분해 (`{Prev_Root, Curr_Root, Curr_Tip}`, `{Prev_Root, Curr_Tip, Prev_Tip}`)
2. 각 삼각형과 타겟 중심점 사이의 최단 거리 계산 (`ClosestPointOnTriangle`)
3. `MinDist² <= HitRadiusSq` → 피격

### 2.3 본 위치 업데이트

`AnimNotifyState` (공격 애니메이션 활성 윈도우)가 매 프레임 해당 본(Bone)의 월드 위치를 `FLNPMeleeAttackFragment`에 기록.

---

## 3. 원거리 판정 (Projectile — Line Segment)

### 3.1 데이터 구조

**`FLNPProjectileFragment`**
```cpp
FVector PreviousPos;
FVector CurrentPos;
FVector Velocity;
ELNPProjectileType Type;  // Linear, Guided, Lobbed
float   HitRadiusSq;
float   Damage;
FMassEntityHandle Instigator;
```

### 3.2 판정 알고리즘

`PreviousPos → CurrentPos` 선분과 타겟 중심점 사이의 최단 거리를 계산.

```
Distance² = DistancePointToSegment²(TargetCenter, PreviousPos, CurrentPos)
if Distance² <= HitRadiusSq → 피격
```

### 3.3 투사체 이동 업데이트

**`ULNPProjectileMovementProcessor`** (별도 구현 예정)

| 타입 | 이동 방식 |
|:---|:---|
| `Linear` | `CurrentPos += Velocity × DeltaTime` |
| `Guided` | `Velocity = Lerp(Velocity, (TargetPos - CurrentPos).Norm × Speed, TurnRate × DeltaTime)` |
| `Lobbed` | 포물선 함수 (시간 매개변수 t 기반) |

---

## 4. 공간 쿼리 최적화 (Spatial Query)

매 프레임 모든 엔티티 쌍을 검사하면 O(n²). 이를 방지하기 위해 공간 분할 사용.

**구현 방향:** `UMassNavigationSubsystem`의 Avoidance Hash Grid를 재활용하거나, 구형 월드 특성에 맞는 커스텀 Grid 구현.

```
공격 엔티티의 위치 → Grid Cell 조회
→ 인접 Cell의 타겟 엔티티 목록만 검사
→ O(n × k), k = 인접 Cell 평균 엔티티 수
```

---

## 5. 후속 처리 (Post-Hit)

### 5.1 피격 반응

- **넉백:** GAS `GameplayEffect`로 Launch 벡터 적용. 구형 지형 특성상 구 곡률을 따르는 포물선 궤적 발생.
- **피격 시 아이템 드랍:** 보유 무기/버프를 바닥에 드랍 → `LootPod Interruption`과 연동 (→ [TechDesign_LootPod.md](TechDesign_LootPod.md))

### 5.2 HitStop (역경직)

전역 시간 확장이 아닌 개별 엔티티 단위로 처리.
- Mass 상태: `FLNPExecutionSpeedFragment`의 배율값 일시적 감소
- Actor 상태: 애니메이션 재생 속도(`CustomTimeDilation`) 직접 조정

### 5.3 VFX / SFX

Mass 비주얼화 도구(`UMassVisualizationComponent`) 또는 전용 FX Pool을 통해 피격 이펙트 재생. 게임스레드에서 일괄 처리.

---

## 6. 패링과의 연계

HitDetection Processor가 판정을 내리기 전에 타겟의 `FParryStateFragment` 활성 여부를 먼저 확인. 패링이 활성화되어 있고 방향 조건을 만족하면 피격 처리 대신 패링 처리로 전환. (→ [TechDesign_ParrySystem.md](TechDesign_ParrySystem.md))

---

## 7. 구현 순서 (권장)

1. `FLNPProjectileFragment` + `ULNPProjectileMovementProcessor` (Linear 타입만)
2. Line Segment 거리 계산 + 피격 이벤트 브로드캐스트
3. HitStop + 넉백 (GAS 인프라 구축 병행)
4. Melee Swept Volume 판정
5. 공간 쿼리 최적화 (엔티티 수 증가 후 필요 시)
6. Guided / Lobbed 투사체, 패링 연계
