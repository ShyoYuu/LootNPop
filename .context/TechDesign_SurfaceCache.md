# 표면 캐시 (SurfaceCache) 기술 설계

## 1. 한눈에 보기

**문제:** 구형 월드(Dyson Sphere 내벽)에서는 NavMesh가 동작하지 않는데, 수천 규모 MassEntity가 워커 스레드에서 매 프레임 지표면 좌표를 조회해야 한다.

**해결:** 월드 생성 직후 내벽 전체를 등장방형(Equirectangular) 그리드로 **사전 베이킹**한다. 완료 후 배열은 불변이므로 워커 스레드에서 **락 없이 O(1) 조회**한다.

```
[월드 생성 완료]
      ↓
BeginBaking() — 해상도 계산 + 배열 할당 (발사하지 않음)
      ↓
Tick()마다 SurfaceCacheSamplesPerFrame개씩 AsyncLineTraceByChannel 발사
      ↓
OnAsyncTraceComplete(게임 스레드)마다 결과 수집
      ↓
[CompletedCount ≥ TotalSamples] → bBakingComplete.store(true) (배열 불변 확정)
      ↓
Mass 프로세서(워커 포함): GetSurfacePoint()
비 Mass 백그라운드 태스크: TakeSnapshot() → Snapshot.GetPoint()
```

| 요소 | 역할 |
|:---|:---|
| `ULNPSurfaceCacheSubsystem` | 베이킹 + 스레드 안전 조회 API |
| `FLNPSurfaceCacheSnapshot` | `TSharedPtr` 공유 스냅샷(복사 없음) — 서브시스템 UObject를 만질 수 없는 태스크용 |
| `SampleSurfaceGrid()` | 바이리니어 조회 단일 구현 — 위 둘이 공유 |

---

## 2. 왜 이 방식인가

- **NavMesh (기각):** Recast는 전역 Z-up을 가정한다. 위치마다 Up이 다른 구형 월드에는 Recast 교체 없이 적용할 수 없다.
- **프로세서 내 실시간 라인트레이스 (기각):** 워커 스레드에서 물리 씬 쿼리는 안전하지 않고, 수천 엔티티가 매 프레임 쏘면 Mass의 성능 이점이 사라진다.

---

## 3. 구현 상세

### 3.1 그리드 구조

위도 × 경도 인덱스를 1차원 배열에 저장한다: `CacheIndex = LatIdx × LonResolution + LonIdx`. 셀 방향은 셀 중심(`+0.5`)으로 계산한다(`IndexToDirection`).

### 3.2 라인트레이스

구 중심(월드 원점)에서 바깥으로 쏜다.

```
시작점: Dir × (SphereRadius × 0.5)
끝점:   Dir × (SphereRadius × 1.5)
채널:   ECC_WorldStatic, bTraceComplex = false
```

첫 히트의 `ImpactPoint`를 셀에 저장하고, 히트가 없으면 `bValid = false` + 경고 로그. 샘플 인덱스는 `UserData`에 실어 단일 `FTraceDelegate`로 전체 콜백을 받는다.

**발사는 `Tick()`이 프레임당 나눠서 한다.** 전량을 한 프레임에 쏘면 다음 프레임 `UWorld::ResetAsyncTrace`가 `WaitForAllAsyncTraceTasks`로 게임 스레드를 막아 큰 히치가 난다. `FTickableGameObject::Tick`은 `ResetAsyncTrace`와 `FinishAsyncTrace` 사이에 돌아 비동기 트레이스 요청이 허용된 구간이다.

### 3.3 조회 — `SampleSurfaceGrid()`

방향을 위도·경도 **분수 인덱스**(0.0 = 셀 0 중심)로 바꾸고 주변 4셀을 **바이리니어 보간**한다.

- 경도는 0°/360°에서 래핑, 위도는 극에서 클램프.
- 이웃 중 하나라도 무효면 최근접 셀로 폴백(주로 극점 부근 — 셀이 촘촘해 계단이 거의 없다).

### 3.4 스레드 안전성 모델

| 시점 | 접근 | 안전성 |
|:---|:---|:---|
| 베이킹 중 | 게임 스레드 콜백만 쓰기, 조회는 `false` 반환 | 단일 스레드 쓰기 |
| 완료 후 | 읽기 전용, 다중 스레드 | 쓰기 없음 |

- **완료 플래그는 `std::atomic<bool>`이다.** 배열을 다 채운 뒤 `memory_order_release`로 세우고 조회 쪽이 `memory_order_acquire`로 읽는다. 그래서 `true`를 본 스레드는 배열 쓰기도 전부 본다 — 락 없는 안전의 유일한 근거다. 평범한 `bool`은 x86에서만 우연히 동작한다.
- **Mass 프로세서는 `GetSurfacePoint()`를 워커에서 직접 호출한다**(적 이동·StateTree 배회 지점·발사체 지면 판정). 이를 위해 `TMassExternalSubsystemTraits`를 `GameThreadOnly = false`, `ThreadSafeWrite = false`로 선언했다. 없으면 이 서브시스템을 요구하는 프로세서가 통째로 게임 스레드로 승격된다.
- `TakeSnapshot()`의 현재 소비처는 `ULNPMassSpawnSubsystem`의 TaskGraph 스폰 위치 계산뿐이다.
- ⚠️ **베이킹은 머신당 1회다.** `BeginBaking()`은 완료 후 재호출도 차단한다. 재대입하면 워커가 읽는 도중 `TSharedPtr` 참조 카운트 조작이 깨진다. 매치 재시작으로 재베이킹이 필요해지면 가드를 풀지 말고, **모든 워커 접근 정지를 보장하는 리셋 진입점**을 따로 만든다.

---

## 4. 설정 (LNPSettings)

| 항목 | C++ 기본값 | `DefaultGame.ini` | 설명 |
|:---|:---:|:---:|:---|
| `SphereRadius` | 25000 cm | 25000 cm | 구체 반지름 |
| `SurfaceCacheCellSpacing` | 200 cm | **100 cm** | 적도 기준 셀 간 호 길이 |
| `SurfaceCacheSamplesPerFrame` | 2000 | **3000** | 프레임당 트레이스 발사 수 |

해상도는 `Lat = round(π·R / Spacing)`, `Lon = round(2π·R / Spacing)`로 역산한다. 현재 설정이면 785 × 1571 = **1,233,235 샘플**, 발사에 약 412프레임(60fps 약 7초)이 걸린다.

---

## 5. API

| 함수 | 설명 |
|:---|:---|
| `BeginBaking()` | 해상도·배열 준비. 진행 중이거나 완료됐으면 no-op |
| `GetSurfacePoint(Dir, OutPoint)` | 스레드 안전 조회. 완료 전에는 `false` |
| `TakeSnapshot()` | 복사 없는 스냅샷 |
| `GetBakingProgress()` | 0~1 진행률 |
| `OnBakingComplete` | GameMode(페이즈 진행)와 PlayerController(로딩 해제)가 구독 |

---

## 6. 어필 포인트 (트러블슈팅 & 설계 판단)

### 6.1 NavMesh를 버리고 도메인 특화 캐시로

"구형 정적 지형 + 방사형 중력"이라는 제약을 역이용해 NavMesh를 위도-경도 그리드 사전 베이킹으로 대체했다. 런타임 경로 탐색 비용이 사라지고 워커 병렬 조회가 가능해졌다.

### 6.2 락 프리 멀티스레드 설계

뮤텍스 대신 **불변성**으로 안전을 확보했다: 게임 스레드 단독 쓰기 → atomic 게시 → 이후 불변. `TMassExternalSubsystemTraits`로 이를 Mass 스케줄러에 알려 프로세서가 게임 스레드로 끌려오지 않는다.

### 6.3 120만 발 비동기 트레이스 분할 발사

처음에는 전량을 한 번에 쐈다가 `ResetAsyncTrace`의 전체 대기로 큰 히치가 났다. 프레임당 발사 수를 나눠 부하를 분산했고, `GetBakingProgress()`도 실제로 차오르는 값이 됐다.

---

## 7. 한계 및 향후 고려사항

- **정적 지형 전제:** 베이킹 후 지형이 바뀌면 캐시가 무효다.
- **극점 밀집:** 등장방형 특성상 극점 부근 셀이 밀집해 메모리 효율이 낮다.
- ⚠️ **수직 단차는 표현할 수 없다 — 캐시의 구조적 한계이자 지형 생성이 지켜야 할 규약이다.**
  - 100cm 균일 격자 + 바이리니어는 한 셀 안의 1m 낙차를 표현하지 못한다. 절벽 위·아래 셀을 섞어 **지면이 없는 중간 높이**를 만들고, 소비처가 그 값으로 스냅해 벽 안에 파묻힌다.
  - 실측(2026-09-10): 이음매 도랑(폭 2m·깊이 1m)이 있을 때 최대 ±140cm, 없앤 뒤 전체 평균 2.2cm.
  - **해상도로 풀리지 않는다.** 간격을 절반으로 줄이면 오차도 절반일 뿐이고, 25cm면 19.7M 샘플(약 630MB)이다.
  - **"불연속이면 최소 반지름" 같은 임계값 분기도 답이 아니다.** 100cm 격자에서는 45° 경사도 셀 간 차이가 ~100cm라 1m 절벽과 같은 신호다.
  - → 절벽 지형을 도입하려면 **캐시 표현을 다시 설계**하거나 **최대 경사를 규약으로** 못박아야 한다. 접지 스냅에 스텝 제한을 두고 초과 시 공중 물리로 넘기는 안전장치(매몰 대신 부양)는 별개로 가치가 있다.
