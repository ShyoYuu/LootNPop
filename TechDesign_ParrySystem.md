# Parry System 기술 설계

## 1. 패링 판단 주체 (Decision Logic)
**선택:** **방법 2 (HitDetection Processor 중심)**
- **이유:** HitDetection 프로세서가 이미 주변 엔티티를 검색하고 거리를 계산하므로, 추가적인 검색 비용이 들지 않음.
- **흐름:** 
  1. 공격 엔티티가 타겟의 `ParryDistance` 내에 들어옴을 감지.
  2. 타겟 엔티티에 `FParryStateFragment` 또는 특정 `MassTag`가 활성화되어 있는지 확인.
  3. 활성화 상태라면 방향(Dot Product) 검사 후 최종 패링 판정.

## 2. 데이터 구조 (Fragments)
- **FHitDetectionFragment:** `HitRadiusSq`, `ParryRadiusSq`, `AttackDirection`.
- **FParryStateFragment:** `bIsParrying` (입력 시 활성화되는 윈도우 시간), `ParryAngleCos`.

## 3. 타입별 반사 로직 구현
- **Linear:** `Velocity = -Velocity`.
- **Guided:** `bIsGuided = false`, `Velocity = DefenderForward * OriginalSpeed`.
- **Lobbed:** 기존 궤도 함수(Parabolic Function)의 진행 시간을 역전(Reverse)시키거나, 시작/목표 지점을 스왑.

## 4. 아키텍처 및 최적화
- **Mass-GAS Integration:** MassEntity는 Actor가 아니므로, 엔티티를 대변하는 **Lightweight Actor**를 두거나 엔티티 데이터를 `GameplayEffect`가 해석할 수 있도록 연결하는 브릿지 컴포넌트 활용.
- **Query Optimization:** 패링 시도 시에만 `FParryStateFragment`를 가진 엔티티를 쿼리에 포함시켜 프로세서 부하 감소.
