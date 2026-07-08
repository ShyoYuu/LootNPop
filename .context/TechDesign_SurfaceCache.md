# 표면 캐시 (SurfaceCache) 기술 설계

## 1. 한눈에 보기

**문제:** 구형 월드(Dyson Sphere 내벽)에서는 NavMesh가 동작하지 않고, 수백~수천 규모 MassEntity가 Worker Thread에서 매 프레임 지표면 좌표를 조회해야 한다.

**해결:** 월드 생성 직후 구형 내벽 전체를 등장방형(Equirectangular) 그리드로 **사전 베이킹**. 베이킹 완료 후 배열은 읽기 전용으로 확정되므로 Mass Worker Thread에서 **락 없이 O(1) 조회**.

```
[월드 생성 완료]
      ↓
BeginBaking() — 전체 샘플을 AsyncLineTraceByChannel로 일괄 발사 (물리 스레드 처리)
      ↓
OnAsyncTraceComplete 콜백(게임 스레드)마다 결과 수집
      ↓
[CompletedCount ≥ TotalSamples] → bBakingComplete = true (배열 읽기 전용 확정)
      ↓
게임 스레드: GetSurfacePoint() / Mass Worker: TakeSnapshot() → Snapshot.GetPoint()
```

### 핵심 구성 요소

| 요소 | 역할 |
|:---|:---|
| `ULNPSurfaceCacheSubsystem` | 베이킹 오케스트레이션 + 게임 스레드 조회 API |
| `FLNPSurfaceCacheSnapshot` | `TSharedPtr` 공유 기반 스레드 안전 스냅샷 (복사 없음) |
| `SampleSurfaceGrid()` (파일 내부 공용 함수) | 바이리니어 보간 조회 로직 단일 구현 — Subsystem/Snapshot이 공유 |

---

## 2. 왜 이 방식인가

### 2.1 NavMesh (기각)

언리얼 엔진 5의 NavMesh(Recast)는 **전역 Z-up을 가정**. 위치마다 Up 방향이 다른 구형 월드에서는 근본적으로 동작하지 않으며, 적용하려면 Recast 자체를 교체해야 한다.

### 2.2 Mass 프로세서 내 실시간 라인트레이스 (기각)

Mass 프로세서의 `Execute()`는 여러 스레드에서 병렬 실행된다. 라인트레이스는 물리 엔진 씬 쿼리라서 Worker Thread에서 직접 호출하면 스레드 안전성을 보장할 수 없고, 수천 엔티티가 매 프레임 트레이스하면 Mass의 성능 이점이 희석된다.

---

## 3. 구현 상세

### 3.1 그리드 구조

등장방형 투영. 위도 × 경도 인덱스로 1차원 배열에 저장.

```
CacheIndex = LatIdx × LonResolution + LonIdx
```

각 셀의 방향 벡터는 셀 중심점으로 계산 (`IndexToDirection`):

```cpp
float Lat = ((LatIdx + 0.5f) / LatRes) * 180.0f - 90.0f;  // -90 ~ +90
float Lon = ((LonIdx + 0.5f) / LonRes) * 360.0f;           //   0 ~ 360
FVector Dir = FVector(cos(Lat) * cos(Lon), cos(Lat) * sin(Lon), sin(Lat));
```

### 3.2 라인트레이스 방향

구형 내벽을 찾기 위해 구 중심에서 바깥쪽으로 트레이스.

```
시작점: Origin + Dir × (SphereRadius × 0.5)   ← 구 내부
끝점:   Origin + Dir × (SphereRadius × 1.5)   ← 구 외부
```

내벽 어딘가에서 충돌 → `ImpactPoint`를 해당 셀에 저장. 샘플 인덱스는 비동기 트레이스의 `UserData`에 담아 전달하므로, 단일 공유 `FTraceDelegate` 하나로 전체 콜백을 처리한다.

### 3.3 조회 방식 — 공용 `SampleSurfaceGrid()`

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

- 경도는 0°/360° 경계에서 래핑, 위도는 극에서 클램프.
- 이웃 셀 중 하나라도 유효하지 않으면 nearest-neighbor로 Fallback (주로 극점 부근 — 극지방은 셀이 촘촘해 계단 현상이 거의 없음).
- 이 로직은 `SampleSurfaceGrid()` 단일 구현으로, 게임 스레드 API(`GetSurfacePoint`)와 워커 스레드 API(`Snapshot.GetPoint`)가 공유한다.

### 3.4 스레드 안전성 모델

| 시점 | 접근 패턴 | 안전성 |
|:---|:---|:---|
| 베이킹 중 | `OnAsyncTraceComplete` 콜백(게임 스레드)에서만 쓰기 | 안전 — 단일 스레드 쓰기 |
| 베이킹 완료 후 | 읽기 전용, 복수 스레드 동시 접근 | 안전 — 쓰기 없음 |

- Mass Worker Thread에서는 `TakeSnapshot()`으로 `FLNPSurfaceCacheSnapshot`을 생성해 사용. 스냅샷은 내부 배열을 `TSharedPtr<const TArray<FPoint>>`로 공유하므로 **데이터 복사 없이** 참조 카운트만 증가.
- `BeginBaking()`은 매번 **새 배열을 할당**해 교체하므로, 이전 매치의 라이브 스냅샷은 자신이 잡은 `TSharedPtr`가 해제될 때까지 유효하다 → 재베이킹이 진행 중인 조회를 손상시키지 않는다.

---

## 4. 설정 (LNPSettings)

| 항목 | 기본값 | 설명 |
|:---|:---:|:---|
| `SphereRadius` | 25000 cm | 구체 반지름 |
| `SurfaceCacheCellSpacing` | 200 cm | 적도 기준 인접 셀 간 호 길이 목표값 |

`SurfaceCacheCellSpacing`으로부터 해상도를 자동 역산:

```
LatResolution = round(π  × SphereRadius / CellSpacing)
LonResolution = round(2π × SphereRadius / CellSpacing)
```

**기본값 기준 (SphereRadius=25000, CellSpacing=200):**
- LatResolution = 393, LonResolution = 785
- 총 샘플 수: 393 × 785 = **308,505개**
- 트레이스 전부를 `BeginBaking()` 시점에 일괄 발사하므로 프레임 예산 설정 없음. 베이킹 완료 시점은 물리 스레드 처리 속도에 의존.

`CellSpacing`을 줄이면 굴곡 지형 정밀도가 올라가지만 메모리와 `BeginBaking()` 비용이 늘어난다.

---

## 5. API

```cpp
/** 월드 생성 완료 후 호출. 전체 트레이스를 비동기로 일괄 발사. 이미 베이킹 중이면 no-op. */
void BeginBaking();

/** 게임 스레드 전용 조회. 베이킹 완료 전에는 false. */
bool GetSurfacePoint(const FVector& WorldDirection, FVector& OutPoint) const;

/** 스레드 안전 스냅샷 생성 (TSharedPtr 공유, 복사 없음). Mass Worker Thread에서 GetPoint() 호출 가능. */
FLNPSurfaceCacheSnapshot TakeSnapshot() const;

/** 베이킹 진행률 0.0 ~ 1.0. 로딩 스크린 게이지에 사용. */
float GetBakingProgress() const;

/** 베이킹 완료 시 브로드캐스트. GameMode(서버 페이즈 진행)와 PlayerController(로딩 해제)가 구독. */
FLNPOnBakingComplete OnBakingComplete;
```

---

## 6. 어필 포인트 (트러블슈팅 & 설계 판단)

### 6.1 NavMesh를 버리고 도메인 특화 캐시로

Recast NavMesh의 전역 Z-up 전제를 분석하고, "구형 정적 지형 + 방사형 중력"이라는 도메인 제약을 역이용해 위도-경도 그리드 사전 베이킹으로 대체. 런타임 경로 탐색 비용이 완전히 사라지고 Worker Thread 병렬 조회가 가능해졌다.

### 6.2 락 프리(Lock-Free) 멀티스레드 설계

뮤텍스 대신 **불변성(Immutability)으로 스레드 안전을 확보**한 사례.
- 쓰기는 게임 스레드 콜백에서만 발생 (비동기 트레이스 결과 수집)
- `bBakingComplete` 이후 배열은 불변 → 어떤 스레드든 자유 접근
- 재베이킹은 배열 교체(새 할당)로 처리 → 기존 스냅샷 소유자는 영향 없음 (Copy-on-Replace)

### 6.3 30만 발 비동기 트레이스 일괄 발사

`AsyncLineTraceByChannel` + `UserData`에 샘플 인덱스를 실어 단일 델리게이트로 수집하는 구조. 게임 스레드 블로킹 없이 물리 스레드가 처리량을 소화하며, `GetBakingProgress()`로 로딩 스크린 게이지와 자연스럽게 연동된다.

---

## 7. 한계 및 향후 고려사항

- **정적 지형 전제:** 베이킹 이후 지형이 변경되면 캐시가 무효화됨. 현재 구현은 월드 생성 완료 후 지형이 고정되는 것을 전제.
- **셀 경계 오차:** 바이리니어 보간으로 셀 경계 불연속은 제거됐으나, 선형 보간 특성상 곡률이 매우 큰 구간에는 미세 오차가 남을 수 있음.
- **극점 밀집:** 등장방형 투영 특성상 극점(위도 ±90°) 부근 셀이 밀집됨. 메모리 대비 극점 지역 정밀도 효율이 낮음.
