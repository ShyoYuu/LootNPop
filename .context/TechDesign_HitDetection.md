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
  BatchedCommand (Game Thread flush) — §6

[클라이언트 예측 경로 — 코스메틱 전용, GE 미적용]
  같은 프로세서의 !bIsServer 분기: 로컬 공격자의 스윙/Ghost 발사체만 판정
  → HitStop·임팩트 VFX·Ghost 소멸을 즉시 재생. 서버 확정 결과가 최종.
```

### 파일 구성 (`Source/LootNPop/HitDetection/`)

| 파일 | 내용 |
|:---|:---|
| `LNPProjectileMassTypes.h` · `LNPProjectileProcessors.*` | 발사체 Fragment 3종 + DeadTag / 4단 프로세서 + 에디터 DebugDraw (§3.1) |
| `LNPWeaponTraceMassTypes.h` · `LNPWeaponTraceProcessors.*` | 칼날 4점·반경·TTL·AlreadyHit Fragment / 판정 + Lifetime + DebugDraw |
| `LNPHitDetectionShared.h` | 후처리 BatchedCommand 5종 + 공용 헬퍼(`GetCapsuleSize`·`ResolveEnemyCapsuleCenter`·`SegmentHitsCapsule`·`ApplyEntityKnockback`) |
| `LNPGuardParryTypes.h` | `FLNPParryStateFragment` (가드/패링 상태 Mass 미러) |
| **단일 정의 헤더** `LNPFireGeometry.h` · `LNPProjectileMotion.h` · `LNPSpreadPattern.h` | 총구·조준 방향 / 적분·지면 판정·예상 궤적 / 산탄 배치. 실탄과 예측이 같은 식을 보게 하는 장치다 (§7.6) |
| `LNPPositionHistoryFragment.*` · `LNPPositionHistoryProcessors.*` | Lag Compensation 링 버퍼와 기록 프로세서 (§5) |
| `LNPProjectileVisualSubsystem.h` · `LNPGhostProjectileSubsystem.h` | Niagara 트레일 풀·임팩트 큐 / 예측 Ghost 등록·소멸 |
| `LNPTargetQuery*` | 적 탐색 질의 → [TechDesign_TargetQuery.md](TechDesign_TargetQuery.md) |
| `LNPTrajectoryGuideComponent.*` | ADS 궤도 가이드 (§3.4) |
| `Animation/ANS_LNPMeleeHitWindow` | 근접 히트 윈도우 — Mass 엔티티 수명 관리 + 본 위치 기록 |

---

## 2. 근거리 판정 (Melee — Swept Volume)

### 2.1 데이터 흐름

`UANS_LNPMeleeHitWindow`(AnimNotifyState)가 몽타주 히트 구간 동안:
- **NotifyBegin:** `FLNPWeaponTraceFragment`를 가진 Mass 엔티티 생성(Deferred). 무기 DataAsset·활성 어빌리티에서 Damage/HitRadius/ParryRadius/Knockback/PoiseDamage를 수집해 초기화.
- **NotifyTick:** 무기 메시의 칼끝(`sword_tip`)·칼밑(`sword_root`) 본 위치를 매 프레임 Prev/Curr로 기록 → 스윕 궤적 완성.
- **NotifyEnd:** 엔티티 파괴(Deferred). 미호출 대비 `TimeToLive` 안전장치는 `ULNPWeaponTraceLifetimeProcessor`가 처리.

### 2.2 판정 알고리즘

이전/현재 프레임 칼날이 쓸고 지나간 Quad면과 타겟 **캡슐 축 선분** 사이의 최단 거리를 잰다.
명중 조건은 `min_dist(SweptQuad, CapsuleSegment) <= SwordRadius + CapsuleRadius`.

- Quad는 삼각형 둘(`{RootPrev, RootCurr, TipCurr}`, `{RootPrev, TipCurr, TipPrev}`)로 나눈다.
- 삼각형 vs 선분은 Möller–Trumbore로 관통을 먼저 보고(관통이면 0), 아니면 선분 양끝→삼각형과 삼각형 3변→선분의 최솟값을 취한다.
- 캡슐 축은 `Center ∓ UpDir * (HalfHeight - Radius)`. **`UpDir`은 구 내벽이라 위치에서 나온다**(`(-Location).GetSafeNormal()`) — Actor가 없어도 성립한다.
- 첫 프레임은 `Prev == Curr`로 축퇴하므로 현재 칼날 선분 vs 캡슐 축으로 폴백한다.

구현은 `LNPWeaponTraceProcessors.cpp`의 `SweptQuadCapsuleDistSq` / `TriangleSegmentDistSq`.

### 2.3 서버 판정 분기

Pass 3에서 공격자 팀에 따라 분기하되, **Player 타겟에 대한 2단계 판정(패링→가드/피격)은 단일 람다(`JudgePlayerTarget`)를 공유**한다 — 근접 PvP(아군 사격)와 Enemy→Player가 항상 동일 로직을 탄다(→ §7.5).

- Player 공격 → Enemy: Actor 있으면 GE 커맨드, 순수 엔티티면 Fragment HP 직접 차감.
- 중복 피격 방지: 스윙 1회당 `AlreadyHit[8]` 배열에 명중 엔티티 기록.

---

## 3. 원거리 판정 (Projectile — Line Segment)

### 3.1 발사체 프로세서 4단 파이프라인

| 프로세서 | 페이즈 | 역할 |
|:---|:---|:---|
| `ULNPProjectileMovementProcessor` | PrePhysics | `PreviousPos` 갱신 → 위치 적분(§3.4), 수명 감산. **그것만 한다** (→ §6.4) |
| `ULNPProjectileHitDetectionProcessor` | StartPhysics | 선분-캡슐 판정, 패링/가드/피격 분기, 지형 충돌·수명 만료 판정, 스플래시, GE 커맨드 |
| `ULNPProjectileVisualizationProcessor` | StartPhysics (게임 스레드) | Niagara trail 할당/갱신, 큐잉된 트레일 해제·임팩트 VFX flush |
| `ULNPProjectileDestructionProcessor` | PostPhysics | DeadTag 엔티티의 트레일 해제 큐잉(**유일한 해제 소유자**, §6.5) + 일괄 파괴 |

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
   GameplayCue.LNP.Projectile.Impact 실행 (Ghost 대조 토큰을 FLNPProjectileImpactContext로)
   → DeadTag → ApplySplash → bHit = true

종말 판정 (어느 캡슐에도 안 닿았을 때 — IsTerminated, §6.4)
   수명 만료 || SurfaceCache 표면 안쪽 진입
      → 로컬 임팩트 VFX → DeadTag → 스플래시(제외 대상 없음)

트레일 해제는 판정이 하지 않는다 — DeadTag를 본 DestructionProcessor가 건다 (§6.5)
```

**스플래시:** `ExplosionRadius > 0`이면 직격 대상을 제외한 반경 내 대상에 거리 감쇠된 GE + `SplashKnockbackStrength` 넉백 (→ §6.3).

### 3.3 발사 (ULNPAbility_RangedAttack)

- 스폰 위치: 무기 메시 `Muzzle` 소켓 (+ `MuzzleOffset`). 소켓이 없으면 ActorLocation 폴백.
- 발사 방향: **총구 → 조준점**으로 수렴시킨다. 조준점과 시선 방향은 소유 클라이언트가 **발사 순간** 스냅샷해
  발동 요청에 실은 값(`FLNPFireAimTargetData`)이고, **예측 클라이언트와 서버가 그 값 하나만 읽는다**
  (InputCmd가 아닌 이유 → [TechDesign_Networking.md](TechDesign_Networking.md) §4.8).
  스냅샷이 없는 사수(적 NPC)만 `GetBaseAimRotation()`으로 폴백한다. 수렴 허용치는 각도가 아니라
  시선 축에서의 **수직 이탈 거리**다(→ §7.7). 조준점의 깊이는 물리 트레이스와 **적 엔티티 광선 질의**
  중 더 가까운 쪽에서 온다(→ §7.8).
- 산탄(`ULNPAbility_RangedSpreadAttack`): Cube 좌표계 육각 링 순회로 중앙 1 + 링 N 배치
  (`LNPSpread::ComputeShotCount` = `1 + 3N(N+1)`, 기본 2링 = **19발**).
  기준 방향 하나에서 난수 없이 전부 파생되므로 조준 보정이 패턴 전체에 그대로 전파된다.
  ⚠️ 확산 축은 **조준 방향 자신의 직교 기저**에서 잡는다. 월드 Yaw/Pitch 덧셈으로 만들면 조준이 월드 Z와
  나란해질수록 가로 폭이 0으로 붕괴하고(짐벌 수렴), 구면 중력에서는 서 있는 위치에 따라 극/적도에서 증상이 뒤집힌다.
- 네트워크 예측 식별자(PredictionKey/SalvoID), Ghost 등록, 관전자 방송: → [TechDesign_Networking.md](TechDesign_Networking.md)

### 3.4 포물선 발사체 (Lobbed — 유탄) ⭐ 2026-09-10

`ELNPProjectileType::Lobbed`는 오래 enum에만 있었다. 이제 유탄 발사기(`DA_Launcher`)가 쓴다.

**런타임 단일 출처는 스칼라 하나다.** `FLNPProjectileSharedFragment::GravityAccel`(cm/s²)이 0이면
등속 직선, 0보다 크면 포물선이다. enum은 저작용 스위치일 뿐이고 해석은 스폰 시 1회
(`ULNPWeaponData::GetEffectiveProjectileGravity`)로 끝나, 이동 프로세서에 타입 분기가 없다.
적분은 `LNPProjectileMotion::Step` 한 곳에만 있고 실제 비행과 예상 궤도가 그것을 공유한다.

| 판단 | 근거 |
|:---|:---|
| **속도 Verlet** (명시적 오일러 아님) | 오일러는 스텝 분할에 따라 궤적이 달라진다. 예측은 고정 1/30초, 실행은 프레임 Dt이므로 **가이드와 실탄이 갈린다.** Verlet은 상수 중력에서 분할에 불변이다 |
| 중력 방향은 **매 스텝 재계산** (`Pos.GetSafeNormal() * G`) | 방사 중력이라 엄밀히는 케플러 궤도다. 사거리 대비 반지름(25,000cm) 스케일에서 국소 균일 근사로 충분하다 |
| `GravityAccel == 0`이 **직선의 정의** | 타입 분기·별도 쿼리·별도 프로세서가 전부 사라진다. 기존 무기는 0이라 회귀가 없다 |
| 조준은 **조준선 그대로** (탄도 해를 풀지 않는다) | 자동 명중시키면 궤도 가이드가 장식이 된다. 가이드를 보고 올려 조준하는 것이 이 무기의 조작이다 |
| 착탄은 **즉시 폭발** (바운스 없음) | 종말 판정(§6.4)을 그대로 쓴다 — 판정 코드 변경 0, 예측 궤도가 실제와 **같은 식**이 된다 |

⚠️ **이동 프로세서 쿼리에 `FLNPProjectileSharedFragment` ConstShared 요구가 생겼다.** SharedFragment 없이
만든 발사체는 그 순간부터 **이동 자체가 멈춘다.** 현행 스폰 경로 3곳(어빌리티 / 순수 엔티티 공격 / Ghost)은 모두 붙이고 있다.

⚠️ **Ghost의 Dead Reckoning 외삽도 같은 적분을 써야 한다.** 직선 외삽(`SpawnPos + V*t`)으로 두면 포물선 탄이
시작부터 어긋난다. `Velocity`도 외삽된 값으로 넣어야 이후 비행이 서버와 겹친다.

#### ADS 궤도 가이드

`ULNPTrajectoryGuideComponent`(`ALNPPlayerCharacter`)가 ADS 중 예상 궤도를 Niagara 리본으로 그린다.

- **로컬 전용성은 스폰 위치에서 나온다** — Niagara를 로컬 클라이언트에서만 스폰하므로 복제되지 않고,
  `SetOnlyOwnerSee` 같은 가시성 플래그가 필요 없다(프로젝트 사용처 0건).
- **총구·조준 방향·지면 판정이 모두 실탄과 같은 함수다**(`LNPFireGeometry::ResolveMuzzleLocation` /
  `ResolveAimDirection` / `LNPProjectileMotion::IsUnderSurface`). 어빌리티·판정 프로세서의 본문을 헤더로
  뽑아 공유한다 — §7.6이 경고하는 "같은 식 두 벌"을 처음부터 만들지 않기 위해서다.
- 궤적은 호 길이 기준으로 **정확히 64점**(`ArcPointCount`)에 재표집한다. 개수가 고정이라 Niagara는 64개를
  버스트하고 `ExecIndex`로 읽기만 하면 된다 — 배열 길이 조회도, 여분 정점 숨기기도 없다. 마지막 점이 곧
  착탄 예상 지점이라 착탄 표식도 같은 배열에서 읽는다(User 파라미터 `Points` 하나뿐).
- 상태를 명령형으로 세우지 않고 **매 Tick 게이트를 다시 평가**한다. 무기 교체·ADS 해제·사망 어느 쪽으로
  빠져나가도 저절로 풀린다 — 가드가 눌린 입력을 남겨 겪었던 문제
  (→ [TechDesign_CharacterMovement.md](TechDesign_CharacterMovement.md) §2.6)를 되풀이하지 않기 위해서다.
- `SurfaceCache` 베이킹 전에는 `PredictArc`가 false를 돌려주고 가이드를 숨긴다. 지면을 모르면 궤적이 지형을 뚫고 뻗는다.
- **착탄점은 `GetImpactPoint()`으로 노출만 한다** — HUD의 사거리 라벨이 당겨 읽는다
  ([TechDesign_HUD.md](TechDesign_HUD.md) §11.13). 여기서 위젯을 밀지 않는 이유는 락온 마커와 같다.

⚠️ **`UNiagaraComponent::Deactivate()`는 가이드를 지우지 못한다**(2026-09-10 실측). Deactivate는 **스폰만 멈추고
살아 있는 파티클은 수명이 다할 때까지 둔다.** 가이드는 매 프레임 위치를 갈아끼우는 방식이라 파티클 수명이
사실상 무한(9999초)이고, 그래서 ADS를 풀어도 궤적이 화면에 남았다. `DeactivateImmediate()`가 즉시 지운다 —
컴포넌트는 살려 두므로 반복 진입 시 재생성 비용은 여전히 없다.

#### 스플래시 반경 표식은 바닥 데칼이다

착탄 예상 지점의 반경 표시는 Niagara 스프라이트가 아니라 `UDecalComponent` + `M_LNP_BlastRadiusDecal`
(Deferred Decal / Translucent / Unlit)이다.

| 판단 | 근거 |
|:---|:---|
| 스프라이트가 아니라 **데칼** | 카메라를 향하는 스프라이트는 반경만큼 커지면 **반구처럼 서서 전투 시야를 가린다.** 데칼은 지형에 투영되므로 경사면에서도 바닥에만 붙는다 |
| **반투명 + 얇은 링** (가장자리 0.55, 안쪽 채움 0.18) | 가이드는 조준을 돕는 것이지 표적을 가리는 것이 아니다 |
| `DecalSize`의 Y·Z = `ExplosionRadius` | 데칼 상자는 반크기 규약이라 투영 사각형의 한 변이 정확히 지름이 되고 머티리얼의 UV 원이 거기에 내접한다 — **표식의 반경이 곧 스플래시 반경**이다 |
| 투영 축 `+ImpactPoint.GetSafeNormal()`, 화면 크기 페이드 off | 데칼은 로컬 +X로 투영하고 구 내벽에서 "아래"는 바깥쪽이다. 페이드를 두면 정작 멀리 던질 때 표식이 사라진다 |
| **카메라가 이 표식을 볼 수 있게 기울어져야 한다** | 지표면에 깔리므로, 멀리 쏘려고 총구를 들면 화면 아래로 밀려나 정작 조준할 때 보이지 않는다. 유탄 ADS 동안 카메라를 내리는 것은 이 표식을 프레임 안에 붙잡아 두기 위한 것이다 (→ [TechDesign_CharacterMovement.md](TechDesign_CharacterMovement.md) §2.7) |

곡률 걱정은 없다 — 반지름 25,000cm 구에서 600cm 원의 새그(sagitta)는 `r²/2R ≈ 7cm`다.

⚠️ **Niagara User 파라미터를 MCP로 만들 때 `AddUserVariables`를 쓰면 에디터가 죽는다**(실측 2회).
대신 모듈 입력을 `User.<이름>`으로 **링크**하면 파라미터가 자동 생성된다 — `NS_TrajectoryGuide`의
`User.Points`(Position 배열 DI)가 그렇게 만들어졌다.

---

## 4. 클라이언트 예측 판정 (코스메틱)

두 HitDetection 프로세서 모두 클라이언트에서는 **로컬 공격자 전용 예측 경로**를 탄다. GE는 절대 적용하지 않고 즉각 피드백만 재생한다.

| 구분 | 근접 | 원거리 |
|:---|:---|:---|
| 대상 | `bIsLocalInstigator` 스윙 | 로컬 예측 Ghost + 관전용 Ghost |
| 피드백 | 공격자 HitStop 1회 (`bLocalFeedbackFired`) | Ghost 즉시 소멸 + 임팩트 VFX |
| 중복 방지 | 서버 확정 큐가 도착해도 스윙당 1회 | `DestroyGhostFromLocalImpact`가 키 기록 → 서버 큐 VFX 중복 차단 |

타겟 캡슐 수집은 서버 경로와 동일하게 Mass 쿼리를 쓴다 — MassReplication 이후 클라이언트에도 유효한 엔티티가 있으므로 게임 스레드 전용 `TActorIterator`가 필요 없다.

---

## 5. Lag Compensation (서버 되감기)

공격자 RTT/2만큼 **과거 시점의 타겟 위치**로 판정한다. 위치 이력은 `FLNPPositionHistoryFragment`(링 버퍼)가
50ms 간격으로 기록하고 `GetInterpolatedLocation(과거 시각)`으로 보간 조회한다.

- **상한은 핑 항에만 200ms**(`LNPHitDetection::MaxPingRewindSeconds`). 보간 지연 항은 상한 밖이며, 합의 최댓값은 0.5초다.
- 근접은 판정 프레임마다 공격자 Ping을 조회하고, 원거리는 **발사 시점에 1회 캐싱**(`CachedRewindSeconds`)해
  비행 내내 재사용한다(→ §7.3). 패링 반사 시에는 방어자 RTT/2로 갱신.
- **대상이 순수 엔티티 적이면 `RTT/2`에 클라이언트 보간 지연을 더한다** — 그 값은 대상의 복제 LOD 갱신 주기다
  (High 0.1 / Medium 0.2 / Low 0.3초). 대상별로 다르므로 되감기 람다 안에서 더한다(`RewoundEnemyCenter`).
  플레이어 대상은 `RTT/2`뿐이다 — 위치가 Mass 버블이 아니라 Actor 복제로 오고 Mover `ForwardPredict`가
  현재까지 전방 예측하므로 보간을 타지 않는다.
- ⚠️ **원거리만 켜져 있고 근접은 꺼 두었다**(`bEnableMeleeInterpolationLag = false`). 원거리는 이동 표본 명중
  +127%였지만 근접은 두 번 모두 순손실이었다(RESCUED:LOST = 0:3, 1:7). 유력 가설은 **플레이어의 리드 조준**
  (미검증) — 근접은 선딜 동안 적이 움직여 진행 방향 앞을 겨누게 되는데, 그 자체가 보간 지연을 갚는 행위라
  되감기를 더 얹으면 이중 보상이 된다. 판정 코드는 그대로라 상수 하나로 되살린다.
- 링버퍼는 합의 최댓값(0.5초)을 덮어야 한다 — `MaxSamples = 11`, static_assert로 고정.
  ⚠️ 모자라면 `GetInterpolatedLocation`이 **조용히** 최고령 샘플로 클램프한다. 그리고 `Count == 0`이면
  `ZeroVector`를 돌려주는데, 구 내벽에서 그것은 원점이 아니라 **행성 중심**이라 되감기 델타에 그대로 넣으면
  판정 캡슐이 반지름만큼 아래로 튄다 — 호출 전에 `Count > 0`을 확인한다(스폰 직후 50ms가 이 창이다).

상세: → [TechDesign_Networking.md](TechDesign_Networking.md)

---

## 6. 후처리 커맨드 (Game Thread)

판정(Worker Thread)과 GAS 접근(Game Thread)을 `FMassBatchedCommand`로 분리한다.

| 커맨드 | 처리 |
|:---|:---|
| `FLNPApplyDamageGECommand` | SetByCaller 피해 GE 적용 + HitReact GameplayCue + 넉백 |
| `FLNPImpactCueCommand` | 임팩트 GameplayCue(근접·원거리 공용) + (근접) 공격자 HitStop |
| `FLNPMeleeParryCommand` | 방어자 Parry.Success 이벤트/큐 + 공격자 Stagger 이벤트 + 공격자 넉백(Actor는 `ApplyKnockback`, 엔티티는 속도 프래그먼트) |
| `FLNPProjectileParryCommand` | 방어자 이벤트/큐 + 반사 Ghost 소멸·재스폰 방송 |
| `FLNPGuardBlockCommand` | Guard.Block GameplayCue |

HitStop은 개별 액터의 `CustomTimeDilation`(기본 배율 0.1, 타이머 복원)이다 — 전역 시간 확장이 아니라 엔티티 단위.

### 6.1 연출은 데미지에 딸려 있지 않다 (⭐ 2026-09-07 개정)

임팩트 VFX와 공격자 HitStop은 원래 `FLNPApplyDamageGECommand` 안에 있었다. 그 묶임이 그대로 결함이 됐다 —
**순수 엔티티는 데미지를 GE로 받지 않으므로 그 커맨드를 아예 타지 않고, 따라서 연출도 통째로 사라졌다.**
ISM으로 그려지는 적을 베면 타격감이 하나도 없었다. 원거리는 처음부터 옳은 형태였으므로(판정 뒤 `FinishHit`이
데미지와 무관하게 임팩트 큐를 낸다) 근접을 그 형태로 맞추고, 두 경로가 `FLNPImpactCueCommand` 하나를 공유하게 했다.

**갈라야 하는 것과 합쳐야 하는 것의 기준은 "수단이 다른가"다.**

| | Actor 피격자 | 순수 엔티티 피격자 | |
|:---|:---|:---|:---|
| 피해 적용 | 데미지 GE | 프래그먼트 HP 직접 차감 | 수단이 다르다 → **갈린다** |
| 피격 리액션 | 방향별 몽타주 | 행동 상태 채널의 플린치 | 수단이 다르다 → **갈린다** |
| 넉백 | Mover Instant Effect | `FLNPEnemyVelocityFragment` | 수단이 다르다 → **갈린다** |
| 임팩트 VFX | | | 위치 기반이다 → **합친다** |
| 공격자 HitStop | | | 공격자 대상이다 → **합친다** |

### 6.2 ASC는 연출의 주인이 아니라 전송 수단이다 (⭐)

`GameplayCue.LNP.Projectile.Impact` 핸들러가 실제로 하는 일은 전부 **월드 서브시스템 호출**이고(Ghost 정리 ·
임팩트 VFX 스폰) 지점은 `CueParameters`에서 읽는다. 즉 ASC는 **"전 클라이언트에 전파하는 통로"** 로만 쓰이는데
코드는 그것을 피격자 ASC로 고정해 두었다. 순수 엔티티에는 Actor가 없으니 전파가 통째로 빠졌고, 호스트 화면에서
ISM 적을 쏘면 착탄 이펙트가 뜨지 않았다.

그래서 폴백을 **전송 수단에만** 건다 — 피격자 ASC가 유효하면 그대로, 없으면 공격자 ASC로 나르고(그림은 같고
전파 범위만 달라진다), 둘 다 없으면 스킵. 공격자 경유는 새 발상이 아니다. `GameplayCue.LNP.Melee.AttackerHitStop`이
이미 그렇게 쓰고 있었다.

⚠️ **이 폴백을 아무 큐에나 적용하면 안 된다.** `Character.HitReact` 핸들러는 `MyTarget`을 **피격자로** 쓴다
(`PlayHitReact`가 그 액터의 로컬 좌표계로 방향을 판정한다). 공격자로 보내면 공격자가 피격 몽타주를 재생한다.
**대상 액터를 `GetWorld()`에만 쓰는 큐에서만 성립한다.**

### 6.3 스플래시 — 세 가지가 빠져 있었다 (2026-09-07)

**① 순수 엔티티를 통째로 건너뛰었다.** `ApplySplash`가 `if (!SE.Actor) continue;`로 걸러 폭발 반경 안에 서 있어도
피해를 받지 않았다. 직격 분기와 같은 형태로 맞췄다.

**② 거리 감쇠가 없었다.** 반경 안이면 어디서나 직격과 같은 값이 들어갔다. 기본 반경이 5cm이던 시절에는 드러날 수
없었고, 반경을 키우는 순간 표면화됐다.

```
t = 폭심까지 거리 / 폭발 반경
데미지  x (1 - t)      선형 — 가장자리에서도 남아야 광범위 무기가 제 역할을 한다
넉백    x (1 - t²)     폭심 근처는 평평, 가장자리에서 급락
경직도   감쇠 없음      "몇 번 맞았는가"의 눈금이라 거리로 희석하면 누적 규칙과 축이 어긋난다
```

⚠️ **`1-t²`와 `(1-t)²`는 전혀 다른 곡선이다.** 끝점이 같아 헷갈리지만 **꺾이는 위치가 반대**다.

| t | 0 | 0.25 | 0.5 | 0.75 | 1.0 |
|:---|--:|--:|--:|--:|--:|
| `1-t²` | 1.00 | 0.94 | 0.75 | 0.44 | 0 |
| `(1-t)²` | 1.00 | 0.56 | 0.25 | 0.06 | 0 |

넉백이 원하는 것은 앞쪽이다 — *"폭발에 휘말리면 확실히 날아가고, 반경을 겨우 벗어난 쪽만 안 날아간다."*
뒤쪽을 먼저 넣었다가 "넉백이 너무 약하다"는 체감으로 되돌렸다. 폭심 3m(반경 6m)에서 밀림 속도가 3배 차이다(750 → 2,250 cm/s).

**③ 방향 부호가 규약과 반대였다.** `HitFromDirection`의 규약은 **"피격자 → 공격자"** 인데 스플래시 두 곳만
`(피격자 - 폭심)`을 넣고 있었다. 소비처가 부호를 뒤집으므로 결과는 **폭심 쪽으로 빨아들이는** 넉백이었고,
`PlayHitReact`의 방향 판정도 앞뒤가 뒤집혀 있었다. 직격·근접은 처음부터 올바른 규약이었다.

### 6.4 지면 착탄이 안 터지던 이유는 페이즈였다 (⭐ 2026-09-09)

폭발 무기를 발밑·벽에 쏘면 임팩트 VFX만 뜨고 **범위 피해도 넉백도 없었다.** 구현이 빠진 것이 아니라
**도달 자체가 불가능한 구조**였다 — 지형 충돌 감지와 파괴는 이동 프로세서(**PrePhysics**)에 있었고
`ApplySplash`는 판정 프로세서(**StartPhysics**)에 있었다. PrePhysics 끝에서 `FLNPProjectileDeadTag`가
flush되고 판정 쿼리는 그 태그를 `None`으로 요구하므로, **지면에 맞은 탄은 판정 프로세서의 쿼리에서 통째로 빠진다.**

**해결은 판정 소유권을 한 곳으로 모으는 것이었다.** 이동 프로세서는 전진과 수명 감산만 하고, 표면 조회·파괴·
임팩트 VFX·트레일 해제가 전부 판정 프로세서로 넘어갔다(트레일 해제는 이후 다시 파괴 프로세서로 — §6.5).
판정 프로세서는 캐릭터 캡슐이 전부 빗나간 **뒤에** 공용 람다 `IsTerminated(Pos, LifetimeRemaining)`를 부르고,
서버 분기는 거기서 `ApplySplash`를 제외 대상 없이 호출한다. 부수 효과로 "지면과 캐릭터에 같은 프레임에 닿는"
탄이 이제 캐릭터 판정을 먼저 받는다(예전에는 지면이 이겼다).

⚠️ **함께 옮기지 않으면 이펙트가 두 번 뜬다.** 임팩트 VFX(`EnqueueImpact`)와 트레일 해제(`EnqueueTrailRelease`)는
이동 프로세서에서 **지우고** 옮겨야 한다.

⚠️ **조기 반환이 종말 판정을 막는다.** 판정 프로세서에 있던 "수집된 타겟이 없으면 반환"은 최적화였을 뿐이지만
남겨두면 **"적 없는 곳에 쏜 탄은 안 터진다"** 가 된다. 빈 배열 순회는 공짜다 — 걷어냈다.

⚠️ **패링 반사탄은 종말 판정을 그대로 통과해야 한다.** 반사는 `FinishHit`을 부르지 않으므로 "직격 있었나"
플래그가 서지 않는다 — 이 플래그를 `FinishHit` 자신이 세우게 두면 자연히 성립하고, 호출부에서 따로 세우면
반사탄이 그 자리에서 사라진다.

설계 판단 둘: **수명 만료도 폭발로 취급한다**(임팩트 VFX는 예전부터 두 경우 모두 재생했으므로 스플래시만
붙여야 대칭이 맞는다). **지면 폭발의 임팩트는 로컬 VFX로 남긴다** — 캐릭터 피격과 달리 GameplayCue로 올리지
않는다. Ghost 대조 토큰이 필요 없고, 큐로 올리면 게스트가 자기 Ghost 착탄 VFX와 겹쳐 두 번 보게 된다.

### 6.5 트레일 해제는 "죽인 쪽"이 아니라 "지우는 쪽"이 한다 (⭐ 2026-09-15)

간헐적으로 발사체가 **제자리에 멈춘 채 남았다.** 엔티티는 이미 파괴됐고 트레일 Niagara 컴포넌트
(`bAutoDestroy=false`)만 해제되지 못한 누수였다 — 위치 갱신이 오지 않으니 멈춰 보인다. 해제는 사망을
**판정한 쪽**이 걸고 있었고 조건은 `FLNPProjectileVisualFragment::bInitialized`의 **그 순간 스냅샷**이었는데,
트레일은 판정 **뒤에** 붙는다:

```
StartPhysics  HitDetection    사망 판정. bInitialized == false → 해제 안 걸음. DeadTag는 Defer
              Visualization   (같은 페이즈, 판정 뒤) 태그가 아직 없음 → 트레일 할당
  ── 페이즈 끝: DeadTag flush
PostPhysics   Destruction     엔티티 파괴 — 해제는 아무도 걸지 않았다 → 누수
```

**처리되는 첫 프레임에 죽는 탄**만 걸린다(밀착 사격, 표면 아래 스폰, 트레일 할당 전에 서버 착탄 큐가 온 Ghost).
창이 한 프레임뿐이라 재현 조건을 찾을 수 없었다.

**해결은 해제를 `ULNPProjectileDestructionProcessor` 한 곳으로 모은 것이다.** Dead 엔티티 전부를 조건 없이
해제 큐에 넣는다(할당된 적 없으면 `ReleaseTrails`가 no-op). 태그가 flush된 뒤에 돌고 엔티티는 그 페이즈 끝에
사라지므로, **해제 뒤로 새 할당이 끼어들 틈이 구조적으로 없다.** 원칙은 §6.4와 같은 방향이다 — 자원의 수명은
흩어진 판정 지점이 아니라 **수명이 끝나는 단 한 곳**이 닫는다. 교환은 명중 탄의 트레일이 1프레임 늦게
사라지는 것뿐이고, 같은 지점에 착탄 VFX가 떠 체감되지 않는다.

- ⚠️ **스냅샷 플래그로 "나중에 생길 자원"의 정리를 판단하지 말 것.** 판정 시점의 `false`는 "안 붙었다"가
  아니라 "**아직** 안 붙었다"다.
- ⚠️ **곁가지 — 예약만 된 엔티티는 `IsEntityActive`가 거짓이다.** UE 5.8 엔티티 스토리지는 상태가 `Created`일
  때만 참을 돌려준다. Ghost 파괴가 그것으로 조기 반환하고 있어서, 스폰 방송과 서버 착탄 큐가 같은 프레임에 오면
  Ghost는 **맵에서는 빠지고 태그는 안 붙어** 캐릭터를 관통해 계속 날았다. `IsEntityValid`로 바꿨다 — 커맨드
  버퍼는 `Create`(순서 0)를 `Add`(순서 2)보다 먼저 실행하므로(`MassCommandBuffer.cpp` `CommandTypeOrder`)
  예약 상태에서 태그를 걸어도 생성 뒤에 적용된다.

---

## 7. 어필 포인트 (트러블슈팅 & 설계 판단)

### 7.1 물리 엔진 없는 전투 판정

Chaos 콜리전 이벤트 대신 순수 수학 판정을 Mass Worker Thread에서 병렬 실행한다(§1). 대상 수집 → 판정 → 후처리로
단계를 나눠 스레드 경계를 명확히 했다 — 판정은 어느 스레드에서든 안전하고, GAS·액터 접근은 커맨드 flush로만 일어난다.

### 7.2 게임 스레드 API를 Worker Thread에서 안전하게 — Command Buffer 위탁 패턴

클라이언트 예측 피드백(`ApplyHitStop`의 타이머, Ghost TMap 갱신)은 게임 스레드 전용이라 워커에서 직접 부르면
데이터 레이스다. 전용 BatchedCommand(`FLNPLocalHitFeedbackCommand`, `FLNPGhostDestroyCommand`)로 위탁해
flush 시점에 실행되게 했다. §6의 후처리 커맨드와 같은 패턴이지만 이쪽은 **코스메틱 전용**이라 서버에도 남지 않는다.

### 7.3 "과거 잔상 추적" 버그 — 되감기 시점의 캐싱

원거리 되감기를 매 프레임 재계산하던 초기 구현은 수명이 긴 발사체가 **비행 내내 대상의 200ms 과거 위치를 판정**해
"분명 피했는데 맞는" 현상을 만들었다. 보정해야 할 지연은 "조준-발사한 순간"의 지연뿐이므로 발사 시점에 1회 캐싱한다(§5).

### 7.4 판정·패링 반경의 분리와 2단계 판정

`ParryRadius`를 `HitRadius`보다 크게 두고 먼저 검사한다(§3.2). 패링이 살짝 더 관대해져 "막을 수 있을 것 같았는데
맞는" 억울함을 줄이고, 두 반경 사이 거리에서는 패링 조건을 만족할 때만 반응한다.

### 7.5 근접 PvP 패링 누락 버그 — 분기 복제의 위험

"Enemy→Player"와 "Player→Player(아군 사격)" 분기가 복제된 코드였을 때 PvP 쪽에만 패링·가드 체크가 누락되어
근접 PvP에서 패링이 무시됐다(Networking Phase 3에서 발견). 단일 람다(`JudgePlayerTarget`)로 통합해 재발을
구조적으로 차단했다 — §7.6이 같은 교훈의 두 번째 사례다.

### 7.6 판정 캡슐 중심 규약 — 단일 헬퍼로 강제

**적 엔티티의 Transform 위치는 발밑이 아니라 캡슐 중심이다 — 두 모드 모두 그렇다.** Actor 구간은
`MassAgentCapsuleCollisionSyncTrait`(ActorToMass)가 캡슐 컴포넌트 Transform을 그대로 넣고, 순수 엔티티 구간은
`ULNPEnemyMovementProcessor`가 `표면반지름 - CapsuleHalfHeight` 위치에 놓는다(구 내벽이라 반지름 감소 방향이
Up이다). **따라서 보정은 어느 쪽에도 필요 없고**, 그것을 강제하는 단일 헬퍼가
`LNPHitDetection::ResolveEnemyCapsuleCenter`다.

⚠️ **이 헬퍼는 한동안 Actor가 없으면 `+ Up*HalfHeight`를 더했고, 그것은 이중 보정이었다**(2026-09-05 수정).
좌표 규약 통일 전의 잔재인데, **전투에 진입한 적이 예외 없이 Actor로 승격되던 동안에는 그 분기가 실전에서 거의
실행되지 않아** 드러나지 않았다. `ELNPEnemyCombatMode::PureEntity`(→
[TechDesign_EnemyNPC_LowLOD.md](TechDesign_EnemyNPC_LowLOD.md))가 승격을 막자 판정 캡슐이 몸통 위로 96cm 떠올랐다.
게다가 같은 분기가 5곳에 복제돼 있어 원거리 두 곳은 **서로 반대 방향**으로 틀어져 있었다 — 서버는 무조건 보정,
클라이언트 예측은 보정 없음. 증상("게스트가 맞혔는데 HP가 안 깎인다")에 예측이 *올바른* 캡슐로 띄운
**거짓 명중 VFX**까지 겹쳐 추적이 늦어졌다.

교훈 둘:
- **검증 경로가 규약보다 중요하다.** "어느 한쪽 분기가 실전에서 거의 실행되지 않는" 상태는 그 분기가 틀려도
  아무도 모른다는 뜻이다. LOD·모드로 갈리는 분기는 **양쪽을 모두 도는 상황을 만들어 검증**한다.
- ⚠️ **§7.5와 같다 — 같은 판정식을 두 곳에 적으면 언젠가 갈라진다.** 새 판정 경로를 추가할 때 이 분기를 다시
  쓰지 말고 헬퍼를 호출할 것.

### 7.7 조준 원본 단일화 — 서버가 클라이언트의 조준점을 읽는다

발사 방향을 로컬/원격으로 분기해 각자 계산하던 시절, 서버의 원격 폰은 `GetBaseAimRotation()` 방향으로만 쐈다.
**그 광선은 카메라 광선과 평행할 뿐 크로스헤어로 수렴하지 않으므로, 총구와 카메라의 간격만큼 거리와 무관하게
일정하게 빗나간다.** 총구 소켓이 오른손에 있어 오차는 가로 방향이었고, 로컬 제어인 리슨 호스트는 해당이 없어
**게스트에서만** 나타났다.

해결은 계산을 잘하는 것이 아니라 **원본을 하나로 만드는 것**이다. 소유 클라이언트가 크로스헤어 지점을 발사 순간
발동 요청(`FLNPFireAimTargetData`)에 실어 보내고, 예측 클라이언트와 서버가 그 값을 읽는다.

| 판단 | 근거 |
|:---|:---|
| 방향이 아니라 **점**을 보낸다 | 총구 소켓 위치는 애니메이션 포즈에 따라 서버·클라가 다르다. 방향을 보내면 서버가 *자기* 총구에서 그 방향으로 쏴 평행 오차가 되살아난다. 점이면 서버가 자기 총구에서 같은 점으로 수렴한다 (2026-09-15 실측: 근거리에서 발사 방향이 수 ° 달라도 "서버 총구 → 게스트 조준점"과의 각차는 ≤0.5°) |
| 전송은 **발동 요청** (처음엔 Mover InputCmd) | InputCmd는 서버의 `GetLastInputCmd()`가 입력 버퍼만큼 과거라 조준을 옮기는 순간 발사 방향이 갈리고, 총을 든 동안 60Hz × 6중복으로 상시 나간다 → [TechDesign_Networking.md](TechDesign_Networking.md) §4.8 |
| **시선 방향도 함께** 보낸다 | 시선은 폴백이자 아래 검증의 **기준 축**이다. 축을 서버의 과거 커맨드에서 가져오면 빠르게 조준을 옮기는 순간 둘이 어긋나 검증에 걸린다 |
| 검증은 각도가 아니라 **수직 이탈 거리** (상한 150cm) | 조준 회전 자체가 이미 클라이언트 권위라 새로 생기는 권위는 없고, 메우는 것은 총구-카메라 시차뿐이다. 그 시차는 **거리에 무관한 상수**(실측 75cm)이므로 각도로 재면 근거리에서 발산해 **게이트가 자기가 통과시키려던 보정을 막는다**(예전 고정 15° 게이트가 그랬다). 수직 이탈은 그 상수 자체라 임계값이 안정적이고, 방어 의미도 곧다 — 조준점을 조작해도 탄착점을 그 거리 이상 옆으로 끌 수 없다 |
| 조준점이 총구보다 **뒤이거나 50cm 미만**이면 폴백 | 수렴시키면 뒤로 쏘게 된다. 총구 기준이라 카메라 기준이던 예전 1.5m 사각지대와 달리 눈앞의 적은 이 구간에 들어오지 않는다 |
| 조준 광선의 **방향은 `ControlRotation`, 원점은 카메라 위치** | 카메라 회전은 프레이밍 사정으로 조준축에서 떨어질 수 있다(유탄 ADS의 시선 하향 — [TechDesign_CharacterMovement.md](TechDesign_CharacterMovement.md) §2.7). 카메라 회전을 방향으로 쓰면 **기울인 만큼 조준선이 따라 내려가 효과가 정확히 상쇄된다.** `ControlRotation`은 발동 요청에 함께 실리는 축이기도 해, 위 표의 "시선 방향"과 조준점이 같은 축에서 나온다. 기울이지 않는 무기에서는 카메라 전방과 정확히 일치하므로 동작 차이가 없다 |

`LNPFireGeometry::ResolveAimDirection`은 캐릭터에서 값을 꺼내지 않고 `(총구, 시선, 조준점*)`을 인자로 받는
순수 함수다 — 서버는 스냅샷을, ADS 궤도 가이드는 로컬 캐시를 넘긴다.

**조준점 트레이스는 이 프로젝트의 유일한 동기 물리 쿼리다**(§7.1에 대한 의도적 예외). 발사 프레임에만
트레이스하지 않고 `ULNPInputHandlerComponent`가 **매 틱 캐시**하는 이유는, 실탄과 ADS 궤도 가이드가 **같은
캐시**를 읽어야 "가이드가 가리키는 곳"과 "실제 착탄"이 갈리지 않기 때문이다. 대신 **원거리 무기를 들었을
때만**(`IsFreeAimMode()`) 돌게 게이팅해 근접 플레이 중에는 0회다 — 가드·ADS·근접 보정 대상 전송이 모두 이
태그 하나를 기준으로 쓰므로, 기준을 바꾸려면 `IsFreeAimMode`의 소비처만 조사하면 된다.
⚠️ `ULNPWeaponData::ProjectileDamageEffect` 유무로 가르던 시절이 있었으나 틀렸다 — 근접 판정도 이 필드를
데미지 GE로 쓰므로 롱소드를 든 동안에도 트레이스가 돌았다.
⚠️ 트레이스 없이 고정 거리(예: 500m) 지점을 조준점으로 쓰면 **성립하지 않는다.** 수렴 거리가 실제 표적 거리
근처여야 하므로 30m 표적에서는 원래 오차의 약 94%가 그대로 남는다.

---
### 7.8 조준점의 **깊이** — 물리 트레이스는 적을 하나도 못 본다

§7.7로 원본을 하나로 만들고도 근거리에서 탄이 크로스헤어 왼쪽으로 치우쳤다. 원인은 조준점의 방향이 아니라 **깊이**였다.

크로스헤어 트레이스는 `ECC_Visibility` 라인 트레이스인데, **적은 어느 쪽도 여기에 걸리지 않는다** — 승격 Actor의
캡슐은 콜리전 응답이 `Visibility: ECR_Ignore`이고, `CombatMode::PureEntity`는 **콜리전 바디 자체가 없다**
(→ [TechDesign_EnemyNPC_LowLOD.md](TechDesign_EnemyNPC_LowLOD.md)). 그래서 광선이 적을 통과해 **뒤 배경**에
찍혔고, 계측에서 근거리 교전 중에도 조준점이 34m 뒤 지형에 찍히는 표본이 다수였다. 그러면 §7.7이 메우려던 시차
보정이 `d·(1 − D/t)`만큼 남는다(`d`=총구-카메라 75cm, `D`=적까지, `t`=조준점까지) — 적 2.5m·조준점 34m면 70cm로
**보정하지 않은 것과 거의 같다.** 멀수록 `D→t`라 사라진다.

**판정은 전부 수학인데 조준점만 물리에 의존하고 있었다**는 것이 결함의 형태다. 해결은 같은 광선을 **적 엔티티
캡슐과 수학으로 교차**시키고 물리 히트와 더 가까운 쪽을 채택하는 것이다(`ULNPTargetQuerySubsystem` — →
[TechDesign_TargetQuery.md](TechDesign_TargetQuery.md)). 벽 뒤의 적은 여전히 벽이 이긴다.

| 판단 | 근거 |
|:---|:---|
| 적 캡슐을 `Visibility: Block`으로 바꾸지 **않는다** | 순수 엔티티는 캡슐이 없어 절반만 고쳐진다. 게다가 근거리에서 더 흔한 쪽이 그 절반이 아니다 |
| 수렴 깊이는 **몸통 중심** | 광선 위에서 캡슐 축에 가장 가까운 점을 쓴다. 표면 진입점보다 적중 여유가 크다 |
| 판정 함수는 **원거리 판정과 공유** | `LNPHitDetection::SegmentHitsCapsule`. §7.6과 같은 사유 — 같은 기하를 두 벌 두면 조용히 어긋난다 |

⚠️ **락온·근접 보정도 같은 결함을 갖고 있었다** — `SphereOverlapActors` + `ALNPEnemyCharacter`로 브로드페이즈를
돌아 순수 엔티티를 보지 못했다. 처방은 조준점과 같고, [TechDesign_TargetQuery.md](TechDesign_TargetQuery.md) §9의
2·3단계로 모두 교체됐다.

---

## 8. 미구현 / 한계

- **판정의 공간 쿼리 최적화:** Pass 3는 여전히 공격 엔티티 × 전체 타겟 O(n×m) 전수 검사다. 구형 월드용 격자
  (`ULNPEnemySpatialGridSubsystem`)는 이미 있지만 첫 소비자가 회피 분리력이고 판정은 아직 붙이지 않았다 —
  착수 판단 근거는 [TechDesign_TargetQuery.md](TechDesign_TargetQuery.md) §6.
- **Guided 투사체:** `ELNPProjectileType`에 정의만 존재. Lobbed는 §3.4로 구현 완료, Guided는 미착수.
- **Mass(Low LOD) 상태 HitStop:** Actor 상태는 `CustomTimeDilation`으로 처리 완료. 순수 엔티티의 "맞은 쪽 재생이
  잠깐 멈추는" 연출은 미구현이다(엔티티 실행 속도 배율 프래그먼트가 후보). **공격자 쪽 HitStop은 순수 엔티티를
  때릴 때도 정상 동작한다**(→ §6.1).
- **근접 임팩트 큐 에셋이 비어 있다:** `GCN_LNP_Melee_Impact`의 `VFX`·`Sound`·`CameraShake`가 모두 미설정이라
  배선은 살아 있어도 재생할 것이 없다. 순수 엔티티만의 문제가 아니라 Actor 적도 마찬가지다 — 저작 대기.
- **피격 아이템 드랍:** 넉백은 완료, 피격 시 보유 아이템 드랍 및 LootPod Interruption 연동 미구현.
