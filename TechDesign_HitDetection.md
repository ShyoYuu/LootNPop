# HitDetection 시스템 기술 설계

## 1. 핵심 설계 개념
- **No Physics Collision:** Chaos/PhysX 엔진의 콜리전을 전혀 사용하지 않음.
- **Mass Processing:** 모든 공격 판정은 `FMassProcessor`에서 매 프레임 일괄 계산.
- **Spatial Query:** `MassNavigationSubsystem` 또는 커스텀 Spatial Grid를 활용하여 주변 엔티티 필터링.

## 2. 근거리 판정 (Melee: Swept Volume)
- **Data (Fragment):** `SwordTipPos`, `SwordRootPos` (이전/현재 프레임).
- **Algorithm:** 
  1. 이전 프레임과 현재 프레임의 위치로 사각형(2개 삼각형) 영역 생성.
  2. 해당 영역(Swept Area)과 타겟 엔티티의 `Radius` 사이의 최단 거리 계산.
  3. `Distance <= HitRadius` 일 경우 피격 처리.
- **Update:** `AnimNotifyState`를 통해 본(Bone) 위치 정보를 Mass Fragment로 전달.

## 3. 원거리 판정 (Projectile: Line Segment)
- **Data (Fragment):** `CurrentPos`, `PreviousPos`, `Velocity`, `ProjectileType`.
- **Algorithm:**
  1. `PreviousPos`에서 `CurrentPos`를 잇는 선분(Line Segment) 생성.
  2. 선분과 주변 엔티티 사이의 거리 계산.
  3. `Distance <= HitRadius` 일 경우 피격 처리.

## 4. 후속 처리 (Post-Hit)
- **Individual HitStop:** 전역 시간 확장이 아닌, 개별 엔티티의 `ExecutionSpeed` 프래그먼트 또는 애니메이션 재생 속도를 조정하여 구현.
- **VFX/SFX:** Mass 시각화 도구 또는 전용 FX Pool 시스템을 호출하여 렌더링.
