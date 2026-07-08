# HitDetection 시스템 기술 설계

## 1. 한눈에 보기

**물리 엔진 콜리전 이벤트를 전혀 사용하지 않는다.** 모든 Hit·Guard·Parry 판정은 Mass Processor의 `Execute()` 안에서 수학적으로 일괄 계산된다 — 수천 개의 발사체·적 엔티티에서도 성능이 일정하고, 판정 로직이 한 곳에 모인다.

| 판정 | 형상 | 담당 프로세서 |
|:---|:---|:---|
| 원거리 (Projectile) | 선분(전 프레임 위치 → 현 위치) vs 캡슐 | `ULNPProjectileHitDetectionProcessor` |
| 근거리 (Melee) | Swept Quad(칼날 궤적) vs 캡슐 축 선분 | `ULNPWeaponTraceHitDetectionProcessor` |

```
[서버 판정 경로 — 권위]
  Pass 1: Enemy 캡슐 수집    Pass 2: Player 캡슐 수집 (+ParryState 미러)
       └──────────────┬──────────────┘
  Pass 3: 공격 엔티티 × 타겟 판정 (Worker Thread, Lag Compensation 되감기 포함)
       │  1단계 패링(ParryRadius) → 2단계 가드/피격(HitRadius)
       ▼
  BatchedCommand (Game Thread flush)
   FLNPMeleeParryCommand / FLNPProjectileParryCommand / FLNPGuardBlockCommand / FLNPApplyDamageGECommand

[클라이언트 예측 경로 — 코스메틱 전용, GE 미적용]
  같은 프로세서의 !bIsServer 분기: 로컬 공격자의 스윙/Ghost 발사체만 판정
  → HitStop·임팩트 VFX·Ghost 소멸을 즉시 재생. 서버 확정 결과가 최종.
```

### 파일 구성

| 파일 | 내용 |
|:---|:---|
| `LNPProjectileMassTypes.h` | `FLNPProjectileSharedFragment`(무기 상수) / `FLNPProjectileFragment`(개별 상태) / VisualFragment / DeadTag |
| `LNPProjectileProcessors.h/.cpp` | Movement → HitDetection → Visualization → Destruction 4단 + 에디터 DebugDraw |
| `LNPWeaponTraceMassTypes.h` | `FLNPWeaponTraceFragment` (칼날 4점 + 반경 + TTL + AlreadyHit) |
| `LNPWeaponTraceProcessors.h/.cpp` | HitDetection + Lifetime(TTL 안전장치) + 에디터 DebugDraw |
| `LNPHitDetectionShared.h` | 후처리 BatchedCommand 4종 + ASC/캡슐 헬퍼 |
| `LNPGuardParryTypes.h` | `FLNPParryStateFragment` (가드/패링 상태 Mass 미러) |
| `Animation/ANS_LNPMeleeHitWindow` | 근접 히트 윈도우 — Mass 엔티티 수명 관리 + 본 위치 기록 |

---

## 2. 근거리 판정 (Melee — Swept Volume)

### 2.1 데이터 흐름

`UANS_LNPMeleeHitWindow`(AnimNotifyState)가 몽타주 히트 구간 동안:
- **NotifyBegin:** `FLNPWeaponTraceFragment`를 가진 Mass 엔티티 생성 (Deferred). 무기 DataAsset·활성 어빌리티에서 Damage/HitRadius/ParryRadius/Knockback을 수집해 초기화.
- **NotifyTick:** 무기 메시의 칼끝(`sword_tip`)·칼밑(`sword_root`) 본 위치를 매 프레임 Prev/Curr로 기록 → 스윕 궤적 완성.
- **NotifyEnd:** 엔티티 파괴 (Deferred). 미호출 대비 `TimeToLive` 안전장치는 `ULNPWeaponTraceLifetimeProcessor`가 처리.

### 2.2 판정 알고리즘

이전/현재 프레임 칼날이 쓸고 지나간 Quad면과 타겟 **캡슐 축 선분** 사이의 최단 거리를 계산.

```
hit 조건: min_dist(SweptQuad, CapsuleSegment) <= SwordRadius + CapsuleRadius

Quad → 삼각형 2개 분해:
  T1 = {Prev_Root, Curr_Root, Curr_Tip}
  T2 = {Prev_Root, Curr_Tip, Prev_Tip}

삼각형 vs 선분 최단 거리 (TriangleSegmentDistSq):
  1) Möller–Trumbore로 선분이 삼각형을 관통하면 거리 0
  2) 아니면 min(선분 양끝→삼각형, 삼각형 3변→선분) — 5회 비교

캡슐 축 선분:
  CylHalfLen = HalfHeight - Radius
  Bot/Top = Center ∓ UpDir * CylHalfLen
  UpDir = (-EntityLocation).GetSafeNormal()   ← 구형 내벽 세계: 머리 방향 = 구 중심 방향

첫 프레임 축퇴(Prev==Curr): SegmentDistToSegment(현재 칼날, 캡슐 축)로 폴백
```

### 2.3 서버 판정 분기

Pass 3에서 공격자 팀에 따라 분기하되, **Player 타겟에 대한 2단계 판정(패링→가드/피격)은 단일 람다(`JudgePlayerTarget`)를 공유**한다 — 근접 PvP(아군 사격)와 Enemy→Player가 항상 동일 로직을 탄다.

- Player 공격 → Enemy: Actor 있으면 GE 커맨드, 순수 엔티티면 Fragment HP 직접 차감.
- 중복 피격 방지: 공격(스윙) 1회당 `AlreadyHit[8]` 배열에 명중 엔티티 기록.

---

## 3. 원거리 판정 (Projectile — Line Segment)

### 3.1 발사체 프로세서 4단 파이프라인

| 프로세서 | 단계 | 역할 |
|:---|:---|:---|
| `ULNPProjectileMovementProcessor` | PrePhysics | `PreviousPos` 갱신 → 위치 적분, 수명 감산, SurfaceCache로 지형 충돌 → DeadTag |
| `ULNPProjectileHitDetectionProcessor` | StartPhysics | 선분-캡슐 판정, 패링/가드/피격 분기, 스플래시, GE 커맨드 |
| `ULNPProjectileVisualizationProcessor` | StartPhysics (게임 스레드) | Niagara trail 할당/갱신, 큐잉된 임팩트 VFX flush |
| `ULNPProjectileDestructionProcessor` | PostPhysics | `FLNPProjectileDeadTag` 엔티티 일괄 파괴 |

`CurrentPos`는 Fragment에 저장하지 않는다 — Entity Transform이 현재 위치를 담당하고, `PreviousPos → Transform` 이 곧 스윕 선분이다.

### 3.2 판정과 후처리

```
Enemy 판정 (Player 발사체만 피해; 그 외 발사체는 닿으면 파괴만)
   └─ SegmentHitsCapsule(Prev, Curr, 되감긴 캡슐 중심, Up, HalfH, CapsuleR + HitRadius)

Player 판정 (2단계)
   1단계: bIsParrying && 창 미만료 && Dot >= ParryAngleCos && (CapsuleR + ParryRadius) 교차
          → 반사 처리 (Fragment 직접 변경 + 식별자 재발급, §TechDesign_ParrySystem)
   2단계: (CapsuleR + HitRadius) 교차
          → 가드(Dot >= GuardAngleCos): GuardBlockCommand / 아니면: ApplyDamageGECommand

FinishHit (공통 후처리 람다):
   트레일 해제 → GameplayCue.LNP.Projectile.Impact 실행
   (Ghost 대조 토큰을 FLNPProjectileImpactContext로 전달) → DeadTag → 스플래시(ApplySplash)
```

**스플래시:** `ExplosionRadius > 0`이면 직격 대상을 제외한 반경 내 대상에 동일 GE + `SplashKnockbackStrength` 넉백.

### 3.3 발사 (ULNPAbility_RangedAttack)

- 스폰 위치: 무기 메시 `Muzzle` 소켓 (+ `MuzzleOffset`).
- 발사 방향: 로컬 컨트롤이면 카메라 크로스헤어 LineTrace 수렴점, 서버의 원격 플레이어면 복제된 `GetBaseAimRotation()` (카메라가 서버에 없으므로).
- 산탄(`ULNPAbility_RangedSpreadAttack`): Cube 좌표계 육각 링 순회로 중앙 1 + 링 2 = **19발** 방사형 배치.
- 네트워크 예측 식별자(PredictionKey/SalvoID), Ghost 등록, 관전자 방송: → [TechDesign_Networking.md](TechDesign_Networking.md)

---

## 4. 클라이언트 예측 판정 (코스메틱)

두 HitDetection 프로세서 모두 클라이언트에서는 **로컬 공격자 전용 예측 경로**를 탄다. GE는 절대 적용하지 않고, 즉각 피드백만 재생한다.

| 구분 | 근접 | 원거리 |
|:---|:---|:---|
| 대상 | `bIsLocalInstigator` 스윙 | 로컬 예측 Ghost + 관전용 Ghost |
| 피드백 | 공격자 HitStop 1회 (`bLocalFeedbackFired`) | Ghost 즉시 소멸 + 임팩트 VFX |
| 중복 방지 | 서버 확정 큐가 도착해도 스윙당 1회 | `DestroyGhostFromLocalImpact`가 키 기록 → 서버 큐 VFX 중복 차단 |

타겟 캡슐 수집은 서버 경로와 동일하게 Mass 쿼리를 사용한다 — Enemy/Player MassReplication 이후 클라이언트에도 유효한 엔티티가 존재하므로 게임 스레드 전용 `TActorIterator`가 필요 없다.

---

## 5. Lag Compensation (서버 되감기)

공격자 RTT/2만큼 **과거 시점의 타겟 위치**로 판정한다. 타겟의 위치 이력은 `FLNPPositionHistoryFragment`(링 버퍼)가 기록하고, `GetInterpolatedLocation(과거 시각)`으로 보간 조회한다.

- 되감기 상한 200ms 클램프.
- 근접: 판정 프레임마다 공격자 Ping 조회.
- 원거리: **발사 시점에 1회 캐싱**(`CachedRewindSeconds`)한 값을 비행 내내 재사용 — 매 프레임 재계산하면 느린 발사체가 "이미 피한 대상의 과거 잔상"을 쫓아가 맞는 문제가 생긴다. 패링 반사 시에는 방어자 RTT/2로 갱신.

상세: → [TechDesign_Networking.md](TechDesign_Networking.md)

---

## 6. 후처리 커맨드 (Game Thread)

판정(Worker Thread)과 GAS 접근(Game Thread)을 `FMassBatchedCommand`로 분리한다.

| 커맨드 | 처리 |
|:---|:---|
| `FLNPApplyDamageGECommand` | SetByCaller 피해 GE 적용 + HitReact/임팩트 GameplayCue + 넉백 + (근접) 공격자 HitStop 큐 |
| `FLNPMeleeParryCommand` | 방어자 Parry.Success 이벤트/큐 + 공격자 Stagger 이벤트 + 공격자 넉백 Launch |
| `FLNPProjectileParryCommand` | 방어자 이벤트/큐 + 반사 Ghost 소멸·재스폰 방송 |
| `FLNPGuardBlockCommand` | Guard.Block GameplayCue |

HitStop은 개별 액터의 `CustomTimeDilation`(0.1, 타이머 복원)으로 처리 — 전역 시간 확장이 아니라 엔티티 단위.

---

## 7. 어필 포인트 (트러블슈팅 & 설계 판단)

### 7.1 물리 엔진 없는 전투 판정

Chaos 콜리전 이벤트 대신 순수 수학 판정(Swept Quad·선분-캡슐)을 Mass Worker Thread에서 병렬 실행. 판정 대상 수집(Pass 1/2) → 판정(Pass 3) → 후처리(Command)로 단계를 나눠 스레드 경계를 명확히 했다. 판정은 어느 스레드에서든 안전하고, GAS/액터 접근은 커맨드 flush(게임 스레드 보장)로만 일어난다.

### 7.2 게임 스레드 API를 Worker Thread에서 안전하게 — Command Buffer 위탁 패턴

클라이언트 예측 피드백(`ApplyHitStop`의 타이머, Ghost TMap 갱신)은 게임 스레드 전용이다. Mass `Execute()`는 워커 스레드에서 돌 수 있으므로 직접 호출하면 데이터 레이스 — 전용 BatchedCommand(`FLNPLocalHitFeedbackCommand`, `FLNPGhostDestroyCommand`)로 위탁해 flush 시점(게임 스레드)에 실행되게 했다.

### 7.3 "과거 잔상 추적" 버그 — Lag Compensation 되감기 시점의 캐싱

원거리 되감기를 매 프레임 공격자 Ping으로 재계산하던 초기 구현은, 수명이 긴 발사체가 **비행 내내 대상의 200ms 과거 위치를 판정**해 "분명 피했는데 맞는" 현상을 만들었다. 보정해야 할 지연은 "공격자가 조준-발사한 순간"의 지연뿐이므로, 발사 시점에 RTT/2를 1회 캐싱해 재사용하는 방식으로 수정 (프레임당 Ping 조회 비용도 함께 제거).

### 7.4 판정·패링 반경의 분리와 2단계 판정

`HitRadius`(피격)와 `ParryRadius`(패링, 더 큼)를 독립 필드로 분리하고 패링을 먼저 검사한다. 패링이 살짝 더 관대해져 "막을 수 있을 것 같았는데 맞는" 억울함을 줄이고, 반경 사이 거리에서는 패링 조건을 만족할 때만 반응한다. 반경값의 제곱근 왕복(`RadiusSq` + `Sqrt`)을 없애고 쓰기 지점에서 반경을 직접 저장.

### 7.5 근접 PvP 패링 누락 버그 — 분기 복제의 위험

과거 "Enemy→Player"와 "Player→Player(아군 사격)" 분기가 복제된 코드였을 때, PvP 쪽에만 패링·가드 체크가 누락되어 근접 PvP에서 패링이 무시됐다 (Networking Phase 3에서 발견). 두 분기를 단일 람다로 통합해 재발을 구조적으로 차단했다.

---

## 8. 미구현 / 한계

- **공간 쿼리 최적화:** 현재 Pass 3는 공격 엔티티 × 전체 타겟 O(n×m) 전수 검사. 엔티티 수가 늘면 `UMassNavigationSubsystem`의 Hash Grid 재활용 또는 구형 월드용 커스텀 Grid로 인접 셀만 검사하도록 개선 예정.
- **Guided / Lobbed 투사체:** `ELNPProjectileType`에 정의만 존재. Movement 프로세서는 Linear만 구현.
- **Mass(Low LOD) 상태 HitStop:** Actor 상태는 `CustomTimeDilation`으로 처리 완료. 순수 엔티티는 `FLNPExecutionSpeedFragment` 배율 방식 미구현.
- **피격 아이템 드랍:** 넉백은 완료, 피격 시 보유 아이템 드랍 및 LootPod Interruption 연동 미구현.
