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
| `LNPHitDetectionShared.h` | 후처리 BatchedCommand 4종 + ASC/캡슐 헬퍼 (`GetCapsuleSize`, `ResolveEnemyCapsuleCenter` — §7.6) |
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
- 발사 방향: **총구 → 조준점**으로 수렴시킨다. 조준점(`ALNPCharacterBase::GetAimTargetLocation`)은
  소유 클라이언트가 카메라 크로스헤어 트레이스로 만들어 Mover InputCmd에 실어 보낸 월드 좌표이고,
  **서버·클라이언트가 모두 그 값 하나만 읽는다.** 조준점이 없는 사수(적 NPC)만 `GetBaseAimRotation()`으로 폴백한다.
  → 이 단일화의 근거는 §7.7.
- 산탄(`ULNPAbility_RangedSpreadAttack`): Cube 좌표계 육각 링 순회로 중앙 1 + 링 2 = **19발** 방사형 배치.
  기준 방향 하나에서 난수 없이 전부 파생되므로 조준 보정이 패턴 전체에 그대로 전파된다.
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
| `FLNPMeleeParryCommand` | 방어자 Parry.Success 이벤트/큐 + 공격자 Stagger 이벤트 + 공격자 넉백(`ApplyKnockback` — Instant Effect) |
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

### 7.6 판정 캡슐 중심 규약 — 단일 헬퍼로 강제

**적 엔티티의 Transform 위치는 발밑이 아니라 캡슐 중심이다 — 두 모드 모두 그렇다.**
Actor 구간은 `MassAgentCapsuleCollisionSyncTrait`(ActorToMass)가 캡슐 컴포넌트 Transform을 그대로 넣고,
순수 엔티티 구간은 `ULNPEnemyMovementProcessor`가 `표면반지름 - CapsuleHalfHeight` 위치에 놓는다
(구 내벽이라 반지름 감소 방향이 Up이다). **따라서 보정은 어느 쪽에도 필요 없다.**

```cpp
LNPHitDetection::ResolveEnemyCapsuleCenter(EntityLocation, UpDir, HalfH, EnemyActor)
```

⚠️ **이 헬퍼는 한동안 Actor가 없으면 `+ Up*HalfHeight`를 더했고, 그것은 이중 보정이었다** (2026-09-05 수정).
좌표 규약이 캡슐 중심으로 통일되기 전의 잔재인데, **전투에 진입한 적이 예외 없이 Actor로 승격되던 동안에는
그 분기가 실전에서 거의 실행되지 않아** 드러나지 않았다. `ELNPEnemyCombatMode::PureEntity`(→
[TechDesign_EnemyNPC_LowLOD.md](TechDesign_EnemyNPC_LowLOD.md))를 도입해 승격을 막자 즉시 표면화됐다 —
판정 캡슐이 몸통 위로 96cm 떠서 디버그 드로우와 실제 판정이 함께 어긋났고, **멀리 있는 Low LOD 적을
저격할 때도 같은 오차가 있었다.**

교훈은 규약 자체가 아니라 **검증 경로에 있다.** "어느 한쪽 분기가 실전에서 거의 실행되지 않는" 상태는
그 분기가 틀려도 아무도 모른다는 뜻이다. LOD·모드로 갈리는 분기는 **양쪽을 모두 도는 상황을 만들어
검증**해야 한다 (적 NPC 이슈에서 LOD 조합 4가지를 모두 보는 규약과 같은 이유다).

이 분기가 근접·원거리·디버그 드로우 5곳에 **복제돼 있던 동안 원거리 두 곳이 서로 반대 방향으로
틀어져 있었다** — 서버는 무조건 보정해 High LOD 적의 판정 캡슐이 96cm 떠올랐고(몸통 아래 절반이
관통, 머리 위 빈 공간이 명중), 클라이언트 예측은 보정을 아예 안 해 Low LOD에서 반대로 어긋났다.
증상은 "게스트가 분명히 맞혔는데 HP가 안 깎인다"로 나타났다. 클라이언트 예측이 *올바른* 캡슐로
판정해 임팩트 VFX를 띄우는 바람에 **거짓 명중 확인**까지 겹쳐 원인 추적이 늦어졌다.

⚠️ **§7.5와 같은 교훈이다 — 같은 판정식을 두 곳에 적으면 언젠가 갈라진다.** 새 판정 경로를
추가할 때 이 분기를 다시 쓰지 말고 헬퍼를 호출할 것.

### 7.7 조준 원본 단일화 — 서버가 클라이언트의 조준점을 읽는다

발사 방향을 로컬/원격으로 분기해 각자 계산하던 시절, 서버의 원격 폰은 `GetBaseAimRotation()`
방향으로만 쐈다. **그 광선은 카메라 광선과 평행할 뿐 크로스헤어로 수렴하지 않으므로, 총구와
카메라의 간격만큼 거리와 무관하게 일정하게 빗나간다.** 총구 소켓이 오른손에 있어 오차는 가로
방향이었고, 로컬 제어인 리슨 호스트는 해당이 없어 **게스트에서만** 나타났다.

해결은 계산을 잘하는 것이 아니라 **원본을 하나로 만드는 것**이다. 소유 클라이언트가
`FLNPModifierInputs::AimTargetLocation`에 크로스헤어 지점을 실어 보내고, 모든 머신이 그 값을 읽는다.
**로컬 클라이언트도 자기 카메라를 다시 트레이스하지 않는다** — 각자 최선을 계산하는 순간
서버 판정과 클라 예측이 그 시차만큼 갈라지기 때문이다.

| 판단 | 근거 |
|:---|:---|
| 방향이 아니라 **점**을 보낸다 | 총구 소켓 위치는 애니메이션 포즈에 따라 서버·클라가 다르다. 방향을 보내면 서버가 *자기* 총구에서 그 방향으로 쏴 평행 오차가 되살아난다. 점이면 서버가 자기 총구에서 같은 점으로 수렴한다 |
| 전송은 **Mover InputCmd** | 새 RPC 없음, 채널 간 순서 문제 없음. 이미 조준의 원본인 `ControlRotation` 바로 옆이며 `LockOnTarget`과 같은 선례 |
| 클램프 **15°** | 조준 회전 자체가 이미 클라이언트 권위라 새로 생기는 권위는 없다. 여기서 메우는 것은 총구-카메라 시차뿐이므로 몇 도면 충분하고, 넘으면 조작으로 보고 시선 회전으로 되돌린다 |

⚠️ `ShouldReconcile`에는 넣지 않는다 — Mover 시뮬레이션이 읽지 않는 전달용 필드라,
넣으면 조준을 움직일 때마다 이동 리시뮬레이션이 돈다 (`LockOnTarget`과 동일).

**조준점 트레이스는 이 프로젝트의 유일한 동기 물리 쿼리다** (§7.1의 "물리 엔진 없는 판정"에 대한
의도적 예외). 발사 프레임에만 채우지 않는 이유는, 서버가 발사 RPC를 처리하는 시점의
`GetLastInputCmd()`가 **발사한 그 프레임의 cmd라는 보장이 없기** 때문이다 — 이웃 틱을 읽으면
조준점이 비어 폴백으로 떨어지고 위 결함이 간헐적으로 되살아난다. 대신 **발사체 무기를 들었을
때만**(`ULNPWeaponData::ProjectileDamageEffect` 유무) 돌게 게이팅해 근접 플레이 중에는 0회다.
조준 모드 태그가 아니라 무기 데이터로 판정하는 이유는, 원거리인데 FreeAim이 아닌 무기가 생기면
그 무기에서만 조용히 결함이 되살아나기 때문이다.

트레이스 없이 고정 거리(예: 500m) 지점을 조준점으로 쓰는 방법은 **성립하지 않는다** —
수렴 거리가 실제 표적 거리 근처여야 하므로, 30m 표적에서는 원래 오차의 약 94%가 그대로 남는다.

---

## 8. 미구현 / 한계

- **공간 쿼리 최적화:** 현재 Pass 3는 공격 엔티티 × 전체 타겟 O(n×m) 전수 검사. 엔티티 수가 늘면 `UMassNavigationSubsystem`의 Hash Grid 재활용 또는 구형 월드용 커스텀 Grid로 인접 셀만 검사하도록 개선 예정.
- **Guided / Lobbed 투사체:** `ELNPProjectileType`에 정의만 존재. Movement 프로세서는 Linear만 구현.
- **Mass(Low LOD) 상태 HitStop:** Actor 상태는 `CustomTimeDilation`으로 처리 완료. 순수 엔티티는 `FLNPExecutionSpeedFragment` 배율 방식 미구현.
- **피격 아이템 드랍:** 넉백은 완료, 피격 시 보유 아이템 드랍 및 LootPod Interruption 연동 미구현.
