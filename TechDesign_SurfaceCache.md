# 표면 캐시 (SurfaceCache) 기술 설계

## 1. 배경 및 문제 정의

### 1.1 왜 지형 충돌 쿼리가 필요한가

Enemy NPC의 MassEntity 기반 구현에서 지형 표면 좌표가 필요한 상황이 두 가지.

**무브먼트:** 적 NPC는 구형 내벽을 따라 이동. 이동 목표 지점을 계산할 때 단순히 구 표면의 수학적 좌표를 사용하면 지형 굴곡(계단, 경사, 장애물)을 무시하게 됨. 실제 표면 위 좌표가 필요.

**HitDetection (예정):** 피격 시 날아가는 궤적 계산, 바닥 착지 지점 예측 등에서도 실제 표면 좌표 쿼리가 필요.

### 1.2 시도한 방법과 한계

#### NavMesh (기각)

언리얼 엔진 5의 NavMesh(Recast)는 **전역 Z-up을 가정**. 구형 월드처럼 위치마다 Up 방향이 다른 환경에서는 근본적으로 동작하지 않음. 방사형 중력 환경에 NavMesh를 적용하려면 Recast 자체를 교체해야 함.

#### Mass 프로세서 내 실시간 라인트레이스 (기각)

Mass 프로세서의 `Execute()`는 여러 스레드에서 병렬로 실행됨. 라인트레이스(`LineTraceSingleByChannel`)는 물리 엔진의 씬 쿼리이며, 멀티스레드 Mass 워커에서 직접 호출하면 스레드 안전성을 보장할 수 없음. 또한 엔티티 수가 수백~수천 단위인 경우 매 프레임 라인트레이스를 수행하면 Mass를 쓰는 성능상 이점이 크게 희석됨.

---

## 2. 해결책: 사전 베이킹 표면 캐시

월드 생성이 완료된 직후, 구형 내벽 전체를 **위도-경도 그리드**로 사전 샘플링하여 메모리에 저장. 베이킹이 완료되면 배열은 읽기 전용이 되므로, Mass 워커 스레드가 락 없이 안전하게 조회 가능.

```
[월드 생성 완료]
      ↓
BeginBaking() 호출
      ↓
Tick()마다 TracesPerFrame개씩 라인트레이스 수행
 (GameThread에서 실행 — 물리 씬 쿼리 안전)
      ↓
[모든 샘플 완료]
      ↓
bBakingComplete = true  →  배열 읽기 전용 확정
OnBakingComplete 브로드캐스트
      ↓
[Mass 프로세서에서 GetSurfacePoint() 호출 가능]
 (WorkerThread에서 읽기 전용 접근 — 스레드 안전)
```

---

## 3. 구현 상세

### 3.1 그리드 구조

등장방형(Equirectangular) 투영 사용. 위도 × 경도 인덱스로 1차원 배열에 저장.

```
CacheIndex = LatIdx × LonResolution + LonIdx
```

각 셀의 방향 벡터는 셀 중심점으로 계산:

```cpp
float Lat = ((LatIdx + 0.5f) / LatRes) * 180.0f - 90.0f;  // -90 ~ +90
float Lon = ((LonIdx + 0.5f) / LonRes) * 360.0f;           //   0 ~ 360
FVector Dir = FVector(cos(Lat) * cos(Lon), cos(Lat) * sin(Lon), sin(Lat));
```

### 3.2 라인트레이스 방향

구형 내벽(내부 표면)을 찾기 위해 구 중심에서 바깥쪽으로 트레이스.

```
시작점: Origin + Dir × (SphereRadius × 0.5)   ← 구 내부
끝점:   Origin + Dir × (SphereRadius × 1.5)   ← 구 외부
```

내벽 어딘가에서 충돌 → `ImpactPoint`를 해당 셀에 저장.

### 3.3 조회 방식

방향 벡터를 위도-경도 **분수 인덱스**로 변환한 뒤, 주변 4개 셀을 **바이리니어 보간**하여 반환. O(1).

```
분수 인덱스 (0.0 = 셀 0 중심, 1.0 = 셀 1 중심):
  LatFrac = (Lat + 90°) / 180° × LatRes − 0.5
  LonFrac =  Lon        / 360° × LonRes − 0.5

주변 4개 셀:
  P00 = Cache[LatLo][LonLo]   P01 = Cache[LatLo][LonHi]
  P10 = Cache[LatHi][LonLo]   P11 = Cache[LatHi][LonHi]

결과:
  OutPoint = Lerp(Lerp(P00, P01, tLon), Lerp(P10, P11, tLon), tLat)
```

경도는 0°/360° 경계에서 래핑 처리. 이웃 셀 중 하나라도 유효하지 않으면 nearest-neighbor로 폴백 (주로 극점 부근).

### 3.4 스레드 안전성

| 시점 | 접근 패턴 | 안전성 |
|:---|:---|:---|
| 베이킹 중 | GameThread에서만 쓰기 | 안전 |
| 베이킹 완료 후 | 읽기 전용, 복수 스레드 동시 접근 | 안전 |

`bBakingComplete` 플래그가 `true`로 설정된 이후에는 배열에 쓰기가 발생하지 않음. Mass 워커 스레드는 이 플래그를 확인한 후에만 `GetSurfacePoint()`를 호출해야 함.

---

## 4. 설정 (LNPSettings)

| 항목 | 기본값 | 설명 |
|:---|:---:|:---|
| `SurfaceCacheCellSpacing` | 200 cm | 적도 기준 인접 셀 간 호 길이 목표값 |
| `SurfaceCacheTracesPerFrame` | 64 | 프레임당 처리할 트레이스 수 |

`SurfaceCacheCellSpacing` 값으로부터 해상도를 자동 역산:

```
LatResolution = round(π  × SphereRadius / CellSpacing)
LonResolution = round(2π × SphereRadius / CellSpacing)
```

**기본값 기준 (SphereRadius=10000, CellSpacing=200):**
- LatResolution = 157, LonResolution = 314
- 총 샘플 수: 157 × 314 = **49,298개**
- 베이킹 소요 시간: 49,298 ÷ 64 ≈ 770 프레임 ≈ **약 13초** (60fps 기준)

`CellSpacing`을 줄이면 굴곡 지형 정밀도가 올라가지만 메모리와 베이킹 시간이 늘어남. `TracesPerFrame`을 높이면 베이킹 속도가 빨라지지만 해당 프레임에서 물리 씬 쿼리 부하가 증가.

---

## 5. API

```cpp
/** 월드 생성 완료 후 호출. 프레임 예산 베이킹을 시작. */
void BeginBaking();

/**
 * 주어진 방향에서 가장 가까운 내벽 표면 좌표를 반환.
 * 베이킹 완료 전에는 false를 반환.
 * Mass 워커 스레드에서 호출 가능 (베이킹 완료 후).
 */
bool GetSurfacePoint(const FVector& WorldDirection, FVector& OutPoint) const;

/** 베이킹 진행률. 0.0 ~ 1.0. 완료 시 1.0. */
float GetBakingProgress() const;

/** 베이킹 완료 시 브로드캐스트. */
FLNPOnBakingComplete OnBakingComplete;
```

---

## 6. 한계 및 향후 고려사항

- **정적 지형 전제:** 베이킹 이후 지형이 변경되면 캐시가 무효화됨. 현재 구현은 월드 생성 완료 후 지형이 고정되는 것을 전제.
- **셀 경계 오차:** 바이리니어 보간 도입으로 셀 경계 불연속이 제거됨. 다만 보간은 선형이므로 표면 곡률이 매우 큰 구간에서는 미세한 오차가 남을 수 있음.
- **극점 밀집:** 등장방형 투영 특성상 극점(위도 ±90°) 부근에서는 셀이 밀집됨. 극점 지역의 지형 정밀도가 상대적으로 낮음.
