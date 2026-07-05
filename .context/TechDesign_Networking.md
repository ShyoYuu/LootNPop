# 멀티플레이 네트워크 기술 설계 (Iris 기반)

> **진행 현황 (2026-07-05):** Phase 1~6.5 전부 완료 (PIE 2·3인 기능 검증 포함). **잔여: Phase 7(LootPod)** 및 아래 항목.
>
> | 분류 | 잔여 항목 |
> |:---|:---|
> | 에디터·에셋 | GameplayCue VFX/Sound 에셋 지정 (에셋 자체가 프로젝트에 아직 없음), Parry CameraShake 커스텀 교체 |
> | 기술 부채 (동작엔 문제 없음) | `DOREPLIFETIME` → Iris Descriptor 마이그레이션 (`EquippedWeaponData`, Attribute — §2.2 방침상 임시 허용 중), 수백 엔티티 규모 대역폭·CPU 프로파일링 (리스크 A 잔여) |
> | 후속 후보 (기능) | 발사 각도(피치) 동기화 — 에임 오프셋 동기화와 한 작업 (§9 Phase 4.5 후보), 관전 Ghost catch-up 수렴, 고지연(`PktLag=100+`) 재검증, Enemy `bIsDead` 명시 복제 (현재 사망 연출은 Attribute 경유로 동작) |

---

## 1. 설계 원칙

### 1.1 서버 권위 + 클라이언트 예측 분리

| 구분 | 담당 | 근거 |
|:---|:---|:---|
| **서버 전용** | GE 적용(HP 감소), 패링 판정, 승리 조건, 엔티티 스폰/소멸 최종 결정 | 변조 불가, 모든 클라에 권위적 동기화 |
| **클라이언트 예측** | HitStop, 충격 VFX, 사운드, 공격 몽타주 즉시 재생, 쿨다운 반영 | 코스메틱이므로 예측 오류가 게임플레이 무결성에 영향 없음 |

클라이언트 예측이 틀려도 실제 HP는 서버가 결정하므로 치팅이 불가능하다.

### 1.2 MassEntity 생성 분류 원칙

프로젝트 내 Mass 엔티티는 두 경로로 분류된다.

| 경로 | 대상 | 생성 주체 | 복제 방식 |
|:---|:---|:---|:---|
| **경로 A** — 공격자 소유 | WeaponTrace, Projectile (자신의 발사체) | 서버·클라이언트 각자 독립 생성 | MassReplication 없음 — 로컬 조회만 |
| **경로 B** — 서버 소유 | Enemy NPC, LootPod | 서버에서 시뮬레이션 | MassReplication으로 전 클라 동기화 |

**경로 B의 이중 복제 구조:**

| 레이어 | 수단 | 수신 대상 | 역할 |
|:---|:---|:---|:---|
| Mass 엔티티 복제 | MassReplication (FMassNetworkID) | **모든** 클라이언트 | Low LOD 시각화 기반 데이터 |
| Actor 복제 | UE Actor bReplicates (거리 Relevancy) | **근접** 클라이언트만 | High LOD 시각화·HP Bar·애니메이션 |

### 1.3 FMassEntityHandle 절대 규칙

> **FMassEntityHandle은 절대 복제 스키마에 포함하지 않는다.**

`FMassEntityHandle`은 각 머신의 EntityManager 슬롯 인덱스다. 네트워크로 전송하면 수신 측에서 엉뚱한 엔티티를 참조하는 버그가 된다.

| 크로스 머신 참조 용도 | 대신 사용할 것 |
|:---|:---|
| 경로 B 엔티티 식별 | `FMassNetworkID` |
| 플레이어 식별 | `APlayerState::GetPlayerId()` |
| 팀/타입 식별 | `FGameplayTag` |

---

## 2. Iris 활성화 및 기반 설정

### 2.1 빌드 설정

`LootNPop.Build.cs`에 `IrisCore`, `NetCore`, `MassReplication` 모듈 추가.
`DefaultEngine.ini`에 `net.Iris.UseIrisReplication=1`, `net.MaxTickRate=60` 설정.

### 2.2 복제 경로 구분 — 하이브리드 설계

| 경로 | 대상 | 비고 |
|:---|:---|:---|
| **Iris 네이티브 경로** | Actor 기반 객체 전체 | `RegisterReplicationFragments` + `BuildAndRegisterObjectReplicationFragment` — 대역폭 최적화 완전 적용 |
| **Iris 레거시 호환 경로** | Mass 엔티티 (Enemy, LootPod) | MassReplication은 IrisCore 의존성 없음 — Iris 활성화 시 레거시 경로로 동작 |

MassReplication의 자체 최적화(공간 Relevancy, LOD 기반 감소, 컴팩트 직렬화)가 수백 명 규모에서 충분히 효율적이므로 Iris 델타 압축 미적용은 수용 가능한 트레이드오프다.

**프로젝트 방침:** Iris 네이티브 경로(`RegisterReplicationFragments`)를 선호한다. `DOREPLIFETIME` 매크로는 레거시 경로이므로 기존 코드 마이그레이션 시 임시 방편으로만 허용.

**Iris 네이티브 경로 대상 클래스:**

| 클래스 | 복제 필드 |
|:---|:---|
| `ALNPGameState` | `ServerPhase`, `OctantGenSeed` |
| `ALNPPlayerState` | ASC 전체 (GAS 내장 복제) |
| `ALNPEnemyCharacter` | `Health`, `bIsDead` |
| `ALNPLootPod` | `PodState`, `CurrentGaugePercent` |
| `ALNPCharacterBase` | `EquippedWeaponData` |

---

## 3. GAS 복제 설정

### 3.1 플레이어 ASC — Mixed 모드

`EGameplayEffectReplicationMode::Mixed` 적용. GA 스펙은 모든 클라이언트에, GE와 Attribute는 소유자에게 완전 복제하고 나머지는 태그로 추론한다.

### 3.2 적 ASC — Minimal 모드

`EGameplayEffectReplicationMode::Minimal` 적용. Gameplay Tag 집합만 복제한다. HP Bar 갱신과 상태 표시에 충분하며 개별 GE 인스턴스 복제 없이 대역폭을 절약한다.

### 3.3 InitAbilityActorInfo 호출 시점

- **서버:** `ALNPPlayerState::PossessedBy()` → `InitAbilityActorInfo(PlayerState, Pawn)`
- **클라:** `ALNPPlayerState::OnRep_PlayerState()` → `InitAbilityActorInfo(PlayerState, Pawn)`

### 3.4 쿨다운 — FScopedPredictionWindow

공격, 대시, 가드 등 모든 어빌리티(`MeleeAttack`, `RangedAttack`, `Dash`, `Guard`)를 클라이언트에서 즉시 예측 실행한다. `ActivateAbility` 내 `CommitAbility` 호출로 쿨다운·코스트 GE가 클라이언트에 즉시 적용되고, 서버 불승인 시 GAS가 자동 롤백한다. 어빌리티 내 추가 GE 적용 시 `FScopedPredictionWindow`로 자식 예측 컨텍스트를 생성한다.

---

## 4. MassEntity 네트워크 아키텍처

### 4.1 FMassNetworkID

MassReplication 플러그인이 제공하는 네트워크 안정 ID(`uint32`). 서버가 발급하는 세션 내 전역 고유 ID(단순 증가 카운터)로, 머신에 관계없이 동일한 논리 엔티티를 가리킨다.

클라이언트 BubbleHandler는 `TMap<FMassNetworkID, FMassEntityHandle>` 매핑을 로컬에서 관리한다. 클라이언트 로컬 핸들은 서버와 다른 값이어도 무방하다.

### 4.2 엔티티 타입별 네트워크 처리 요약

| 엔티티 | 경로 | 생성 주체 | 식별자 | 복제 스키마 |
|:---|:---|:---|:---|:---|
| WeaponTrace | A | ANS — 서버·클라 각자 | 없음 (로컬 전용) | 없음 |
| Projectile | A | ActivateAbility — 서버·클라 각자 | `PredictionKeyID + SpawnIndex` (Ghost 매핑) | 없음 |
| Enemy NPC | B | 서버 | `FMassNetworkID` | `FLNPEnemyReplicationData` (위치·타입) |
| LootPod | B | 서버 | `FMassNetworkID` | `FLNPLootPodReplicationData` (초기 위치) |

**복제 스키마에서 절대 제외해야 하는 필드:**

| 필드 | 이유 |
|:---|:---|
| `FMassEntityHandle` 타입 전체 | 서버 전용 로컬 핸들 |
| `InstigatorPlayerID` (WeaponTrace·서버 전용) | 서버 내부에서만 사용 |
| `FLNPEnemyTargetingFragment::TargetPlayerID` | 서버 전용 |
| `FLNPEnemyTargetingCandidateFragment::PotentialTargets[]` | 서버 전용 |
| `bIsLocalInstigator`, `AlreadyHit[]` | 로컬 머신 결정값 |

---

## 5. 시스템별 상세 설계

### 5.0 Lag Compensation — 서버 위치 되감기

공격자 RTT/2만큼 과거의 피격 대상 위치로 히트 판정을 수행해, "분명히 맞았는데 빗나감" 현상을 해소한다.

**위치 히스토리 버퍼 정책:**
- 대상: Enemy NPC, 플레이어 폰 모두에 `FLNPPositionHistoryFragment` 부착 (동일 프로세서 재사용)
- 샘플: 50ms 간격 × 5개 → 최대 200ms 되감기 지원
- 되감기 클램프 상한: 200ms (버퍼 범위 초과 요청 시 최고령 샘플 사용)
- 서버 전용 프로세서(`ULNPPositionHistoryRecordProcessor`)가 기록. 클라이언트에서는 실행하지 않는다.

**적용 범위:**

| 대상 | 히스토리 저장 | 비고 |
|:---|:---|:---|
| Enemy NPC | `FLNPPositionHistoryFragment` | 아키타입에 직접 부착 |
| 플레이어 캐릭터 | `FLNPPositionHistoryFragment` | PvP 히트 판정용 |
| WeaponTrace·Projectile | 불필요 | 공격자 소유 — 되감기 대상 아님 |

**서버 히트 판정 경로:** `GetAttackerPlayerController()->GetPingInMilliseconds() * 0.0005f`로 RTT/2(초)를 계산하고, `MaxRewindSeconds`로 클램프한 뒤 과거 위치를 보간 조회한다.

---

### 5.1 근접 HitDetection (Melee)

**문제점:**
1. ANS에서 `HasAuthority()` 가드 없이 클라이언트 엔티티 생성 → 의도치 않은 동작
2. HitStop이 서버에서만 발동 → 공격자 클라에서 RTT 지연 또는 미수신
3. 클라이언트가 적 위치를 알 수 없음 (Mass 엔티티가 클라에 없음)

**설계 방향:**

```
[공격자 클라이언트]
  ANS::NotifyBegin() → 클라 WeaponTrace 엔티티 생성 (bIsLocalInstigator=true)
  HitDetectionProcessor → Physics Scene에서 복제된 ALNPEnemyCharacter 캡슐 쿼리
                       → 충돌 감지 → HitStop + VFX 즉시 재생 (코스메틱)

[서버]
  ANS::NotifyBegin() → 서버 WeaponTrace 엔티티 생성 (bIsLocalInstigator=false)
  HitDetectionProcessor → Mass 엔티티 쿼리 + Lag Compensation 적용 (→ 섹션 5.0)
                       → 충돌 감지 → GE 적용 (실제 HP 감소)
                       → GameplayCue.LNP.Character.HitReact (피격자 HitReact + HitStop 전파)
                       → GameplayCue.LNP.Melee.Impact (충격 VFX 전파)
```

**핵심 정책:**
- `ANS`는 서버·클라이언트 양쪽에서 각자 독립적으로 엔티티를 생성한다. `HasAuthority()` 분기 없음.
- 서버 전용 필드(`InstigatorPlayerID`, `DamageEffectClass`)는 `HasAuthority()` 시에만 채운다.
- 클라이언트 타겟 조회는 `CollectReplicatedActorTargets()`로 복제된 Actor 캡슐을 Physics Scene 쿼리한다. (선언 위치: `LNPHitDetectionShared.h`) 쿼리 대상 타입은 `ALNPCharacterBase` — `ALNPEnemyCharacter`와 플레이어 캐릭터 양쪽을 커버해 PvP 히트도 처리한다. 공격자 자신과 같은 팀은 `InstigatorPlayerID`·Team Tag로 필터링한다.
- `Multicast_ApplyHitStop` RPC 제거. 피격자 HitStop은 `GameplayCue.LNP.Character.HitReact`로 일원화.
- 공격자 로컬 HitStop(`ApplyLocalHitFeedback()`)은 `bIsLocalInstigator` 플래그로 실행. Listen Server와 Autonomous Proxy 모두 `IsLocallyControlled()==true`이므로 `HasAuthority()` 분기 불필요.
- `GameplayCue.LNP.Melee.Impact` 수신 시, `bIsLocalInstigator=true`인 공격자 클라이언트는 로컬 경로에서 HitStop·VFX를 이미 실행했으므로 GameplayCue 핸들러에서 no-op 처리한다.

**패링 타이밍 RTT 역보정:**
패링은 위치가 아닌 입력 타이밍 판정이므로 Lag Compensation과 별개로 처리한다. 서버가 Guard 입력을 수신한 시각에서 `RTT/2`를 빼 실제 입력 시각을 복원한다. 보정 클램프 상한은 패링 창 절반(`0.075s`)으로 보수적 설정 후 플레이테스트로 조정한다. 근접·원거리 양쪽 패링 커맨드에 동일하게 적용한다.

> 고 RTT 환경 재현: `net.PktLag=150`, `net.PktLoss=5` 조합으로 시뮬레이션.

---

### 5.2 원거리 HitDetection (Projectile)

**문제점:**
1. `ULNPAbility_RangedAttack`이 서버에서만 엔티티 생성 → 클라이언트에 발사체 없음
2. 발사 즉시 클라이언트 시각 피드백 없음
3. HitStop이 서버에서만 호출됨

**설계 방향:**

```
[공격자 클라이언트]
  ActivateAbility() → 클라 Projectile 엔티티 생성 (Ghost, bIsLocalInstigator=true)
  MovementProcessor → 로컬 시뮬레이션
  HitDetectionProcessor → 복제된 Actor 캡슐 쿼리
                       → 충돌 감지 → HitStop + VFX + Ghost 즉시 소멸 (브랜치 A)

[서버]
  ActivateAbility() → 서버 Projectile 엔티티 생성
  HitDetectionProcessor → Mass 엔티티 쿼리 + Lag Compensation 적용 (→ 섹션 5.0)
                       → 충돌 감지 → GE 적용
                       → GameplayCue.LNP.Projectile.Impact (토큰 포함)
```

**Ghost 식별 정책 (Phase 4.5 개정):**
Ghost는 `FLNPGhostKey { InstigatorPlayerID, KeyOrSalvo, SpawnIndex }`로 식별한다. PredictionKey는 클라이언트별 로컬 카운터라 단독으로는 전역 고유가 아니므로(서로 다른 두 플레이어의 키 충돌, 리슨 호스트·NPC 발사는 항상 키=0) 공격자 `InstigatorPlayerID`를 함께 묶고, 예측 키가 없는 발사는 `ULNPGhostProjectileSubsystem::IssueServerSalvoID()`(서버 발급 단조 증가, 65536부터 시작해 uint16 예측 키 공간과 분리)로 대체한다.

**Ghost 수명 정책 (Phase 4.5 개정):**

- **per-entry 만료:** `RegisterGhost` 시 `ProjectileLifetime + 1s`를 만료 시각으로 저장한다. 정상 비행 중인 Ghost는 MovementProcessor의 수명/지형 판정이 먼저 소멸시키므로, 스윕(`SweepExpiredGhosts`)에 걸리는 건 stale 엔트리뿐이다. (구 설계의 "고정 500ms TTL"은 0.5초 이상 비행하는 모든 발사체를 클라이언트 화면에서 중도 소멸시키던 버그였음.)
- **GAS 불승인 롤백:** `DestroyAllGhostsForKey(InstigatorPlayerID, KeyOrSalvo)`로 해당 발사 이벤트의 Ghost만 제거.

**임팩트 VFX 중복 방지 — 로컬 재생 기록 (Phase 4.5 개정):**
클라이언트 로컬 코스메틱 판정(공격자 예측 Ghost + 관전용 Ghost 공통)이 Ghost를 파괴할 때 `DestroyGhostFromLocalImpact`가 키를 잠시 기록하고, 서버 확정 GameplayCue 핸들러는 `ConsumeRecentLocalImpact`로 기록 유무만 확인한다. 공격자/관전자 분기 자체가 사라져 핸들러가 단일 경로가 된다:

```
OnExecute(Params):
  key ← { ctx.InstigatorPlayerID, ctx.PredictionKeyID(KeyOrSalvo), ctx.SpawnIndex }
  DestroyGhost(key)                          ← 남아있는 Ghost 정리 (없으면 no-op)
  if not ConsumeRecentLocalImpact(key):      ← 로컬 판정이 이미 VFX 재생했으면 스킵
    SpawnImpactVFX(Params.Location)          ← 브랜치 B·리슨 호스트·방송 유실 케이스 커버
```

이 구조로 리슨 호스트 공격자(예측 경로가 없어 Ghost 미등록)도 항상 서버 확정 위치 VFX를 받는다 — 구 설계의 "공격자인데 Ghost 없음 → no-op" 분기가 호스트 화면의 임팩트 VFX를 누락시키던 버그 해결.

`HasAuthority()` 분기를 `ActivateAbility`에서 제거한다. GAS가 서버·Autonomous Proxy 양쪽에서 실행하므로 각자 독립적으로 엔티티를 생성한다.

**`FLNPProjectileImpactContext` — GameplayCue 커스텀 컨텍스트:**

`RawMagnitude`/`NormalizedMagnitude` float 필드 인코딩 대신 `FGameplayEffectContext`를 상속해 토큰과 InstigatorPlayerID를 타입 안전하게 전달한다.

```cpp
USTRUCT()
struct FLNPProjectileImpactContext : public FGameplayEffectContext
{
    GENERATED_BODY()

    int32 PredictionKeyID    = 0;          // KeyOrSalvo — 예측 키(<=65535) 또는 서버 SalvoID(>=65536)
    uint8 SpawnIndex         = 0;
    int32 InstigatorPlayerID = INDEX_NONE; // FLNPGhostKey 구성용

    // 반드시 오버라이드: GetScriptStruct(), Duplicate(), NetSerialize()
};
```

서버에서 GameplayCue 전송 시 (`FLNPApplyDamageGECommand::Run`):

```
FLNPProjectileImpactContext* Ctx = new FLNPProjectileImpactContext()
Ctx->PredictionKeyID    ← Proj.PredictionKeyID
Ctx->SpawnIndex         ← Proj.SpawnIndex
Ctx->InstigatorPlayerID ← Proj.InstigatorPlayerID

FGameplayCueParameters Params
Params.Location       ← HitLocation
Params.EffectContext  ← FGameplayEffectContextHandle(Ctx)
TargetASC->ExecuteGameplayCue(TAG_GameplayCue_LNP_Projectile_Impact, Params)
```

---

### 5.3 Enemy NPC (이중 복제: MassReplication + Actor 복제)

**정책:**

| 레이어 | 대상 | 역할 |
|:---|:---|:---|
| MassReplication | 모든 클라이언트 | 엔티티 존재·위치 동기화. Low LOD(ISMC/Niagara) 기반 |
| Actor 복제 (거리 Relevancy) | 근접 클라이언트만 | High LOD. HP Bar·애니메이션·HitDetection 타겟 |

**서버 LOD 정책:** 서버도 기본적으로 Mass 엔티티만 사용한다. `UMassRepresentationProcessor`가 플레이어와의 거리 기준으로 `ALNPEnemyCharacter`를 동적 스폰/소멸하며, 모든 플레이어에게서 멀면 서버에도 Actor가 없다. 이것이 수백 명 규모 적을 처리하는 핵심 부하 절감 포인트다.

**MassReplication 스키마 (최소 데이터):** `FMassNetworkID`, `Location`, `Rotation`, `EnemyTypeTag`.
Health는 High LOD Actor 복제로 전달되므로 스키마에서 제외. Low LOD에서 체력 시각화가 필요해지면 그때 추가.

**BubbleHandler 정책:** `PostReplicatedAdd`에서 클라이언트 엔티티 생성. 아키타입 필수 Fragment: `FTransformFragment`, `FLNPEnemyTypeFragment`, `FMassRepresentationFragment`, `FMassRepresentationLODFragment`.

**클라이언트 프로세서 권한 분리:**

| 프로세서 | 서버 | 클라이언트 |
|:---|:---:|:---:|
| AI 계열 (Scoring, Targeting, Movement) | ✅ | ❌ |
| `UMassRepresentationProcessor` (LOD·ISMC) | ✅ | ✅ |
| `ULNPEnemyActorSyncProcessor` | ✅ | ❌ |
| Health, DeathTimer | ✅ | ❌ |

모든 게임 로직 프로세서에 `NM_Client` 조기 반환 추가.

**Actor 복제 설정:** `bReplicates=true`, `NetUpdateFrequency=30`, `NetCullDistanceSquared=30000²`. 복제 필드: `Health`, `bIsDead`. 속도는 Mover SyncState가 자동 처리하므로 별도 복제 불필요.

**이동 보간:** `ULNPEnemyActorSyncProcessor`가 서버에서 `SetActorTransform`을 호출하면 Mover의 `UMoverNetworkPredictionLiaisonComponent`가 감지해 클라이언트 simulated proxy에서 위치 보간을 자동으로 수행한다. Dead reckoning(속도 기반 예측)은 플레이테스트 후 필요 시 추가.

**공격 애니메이션 전파:**
- 원거리 공격: `NetMulticast Unreliable` (시각적 연출 — 손실 허용)
- 근접 공격: `NetMulticast Reliable` + 0.2s 쿨다운 (클라이언트가 회피 타이밍을 읽어야 하므로 신뢰성 필수)
- 피격 애니메이션: `GameplayCue.LNP.Character.HitReact`로 처리 (GAS 자동 전파)

---

### 5.4 LootPod (이중 복제: MassReplication + Actor 복제)

Enemy NPC와 동일한 이중 복제 원칙. LootPod은 정적 위치이므로 MassReplication은 초기 스폰 시 위치를 1회만 전송한다.

| 레이어 | 담당 |
|:---|:---|
| MassReplication | 엔티티 존재·초기 위치 (1회성) |
| Actor 복제 | `PodState`, `CurrentGaugePercent` — 상태 변화 전파 |

**MassReplication 스키마:** `FMassNetworkID`, `Location`. `PodState`·Gauge는 Actor 복제가 담당하므로 제외.

**Actor 복제 설정:** `bReplicates=true`, `NetCullDistanceSquared=20000²`. 복제 필드: `PodState`, `CurrentGaugePercent`.

**게이지 갱신 최적화:** `CurrentGaugePercent`는 매 프레임 변하므로 2% 이상 변화 시에만 갱신 플래그를 세운다.

**프로세서 정책:** `ULNPIdleToLootingProcessor`, `ULNPLootingProcessor`에 `NM_Client` 조기 반환 추가. `FLNPLootPodFragment` 자체는 복제하지 않는다.

---

## 6. 플레이어 캐릭터 복제

### 6.1 이동 — Mover 2.0

Mover 2.0이 클라이언트 예측과 서버 검증을 내장한다. `SprintModifier`, `GuardModifier`가 SyncState에 포함되어 롤백 시 자동 복구된다.

**검증 완료 (리스크 B 해소):** `ULNPPawnGravityComponent::UpdateControllerOrientation()`의 `CurvatureDelta` 누산이 Mover SyncState에 정상 포함되어 Up 벡터 오차 없음 확인.

### 6.2 무기 장착 — 서버 권한화 ✅ 구현 완료 (Phase 5)

실제 구현된 흐름:

```
클라이언트 → EquipWeapon() → CharacterBase::EquipWeapon (비주얼 즉시 로컬 적용)
                            → EqComp->EquipWeapon (WeaponSlot.Definition 즉시 갱신, GAS 스킵)
                            + Server_EquipWeapon() RPC 발송
서버       → ALNPCharacterBase::EquipWeapon (비주얼 + EquippedWeaponData 마킹)
           → EqComp->EquipWeapon (GAS 어빌리티 부여)
           → EquippedWeaponData DOREPLIFETIME 복제
클라이언트 → OnRep_CurrentWeapon() → ALNPCharacterBase::EquipWeapon 직접 호출 (비주얼만)
```

`RequestEquipWeapon()` 래퍼는 도입하지 않음 — `ALNPPlayerCharacter::EquipWeapon` 오버라이드가 동일 역할 수행. `EquippedWeaponData`는 현재 DOREPLIFETIME(레거시 경로) 사용 중. Phase 6 착수 전 Iris Descriptor로 마이그레이션 권장.

### 6.3 대시·가드 입력 — FScopedPredictionWindow 예측

| 항목 | 예측 여부 |
|:---|:---|
| 대시 이동 | ✅ Mover 내장 예측 |
| 가드 상태 진입 (`GuardModifier`) | ✅ `FScopedPredictionWindow` |
| 패링 창 활성화 | ✅ 클라이언트 즉시 — 판정은 서버 |
| GE 부작용 (스태미나 감소 등) | ✅ GAS Mixed 모드 내장 |

---

## 7. GameplayCue 설계

GameplayCue는 GAS가 관련 클라이언트에 자동 브로드캐스트한다. Multicast RPC 대비 코드가 단순해지고 수신 대상을 GAS가 자동 관리한다. `ExecuteGameplayCue` 자체는 Unreliable UDP 기반이므로 코스메틱 단락으로 허용한다. HitReact 미수신 빈도가 플레이테스트에서 문제가 되면 Reliable Multicast RPC로 전환한다.

### 전파 수단 선택 기준

| 상황 | 수단 |
|:---|:---|
| GAS 이벤트(GE 적용, 피격 판정)가 트리거인 코스메틱 | GameplayCue — GAS가 수신 대상 자동 관리 |
| GAS와 독립적인 애니메이션(공격 시작, 스킬 시전) | Multicast RPC — Reliable/Unreliable 직접 선택 |

Autonomous Proxy는 어빌리티 예측 컨텍스트 안에서 몽타주를 즉시 재생한다. 시뮬레이티드 프록시는 서버 재생 후 전파를 받아 재생한다.

**신뢰성 정책:**
- 근접 공격 몽타주: `Reliable` + 0.2s 쿨다운 — 회피 타이밍 정보로 손실 불허
- 원거리 공격 몽타주: `Unreliable` — 시각 연출로 손실 허용
- HitReact·Impact VFX: GameplayCue(`ExecuteGameplayCue`) — Unreliable UDP, 손실 허용

### 생성할 GameplayCue 에셋

| GameplayCue Tag | 트리거 시점 | 효과 | 상태 |
|:---|:---|:---|:---|
| `GameplayCue.LNP.Guard.Block` | Guard 성공 시 | 방어 파티클 + 사운드 | ✅ C++(`ULNPGameplayCueNotify_VFXSound`) 연결 완료, VFX/Sound 에셋 자체가 아직 없어 재생 내용은 없음 |
| `GameplayCue.LNP.Parry.Success` | Parry 성공 시 | 패링 파티클 + 카메라 쉐이크 | ✅ C++ 연결 완료. CameraShake는 엔진 기본값 임시 지정, VFX/Sound 미지정 |
| `GameplayCue.LNP.Melee.Impact` | 근접 히트 시 | 충격 파티클 + 히트 사운드 | ✅ C++ 연결 완료, VFX/Sound 에셋 미지정 |
| `GameplayCue.LNP.Character.HitReact` | 피격 시 | HitReact 몽타주 + 피격자 HitStop | ✅ 완료 — `ULNPGameplayCueNotify_HitReact`가 `PlayHitReact`/`ApplyHitStop` 호출. PIE 2인(저·고지연) 양방향 재생 확인 |
| `GameplayCue.LNP.Projectile.Impact` | 원거리 히트 시 (Phase 4) | 탄착 파티클 + 사운드 (`FLNPProjectileImpactContext` 커스텀 컨텍스트 사용 — Ghost 토큰 + InstigatorPlayerID 전달) | ✅ C++(`ULNPGameplayCueNotify_ProjectileImpact`) 연결 완료, VFX/Sound 에셋 미지정. PIE 검증 미실시 (Phase 4 §9 참조) |
| `GameplayCue.LNP.Melee.AttackerHitStop` | 근접 히트 시, 공격자 ASC에서 발동 (Phase 4 후속) | 공격자 HitStop을 제3자(구경꾼) 화면에 전파. 공격자 본인 화면은 no-op(예측 경로가 이미 처리) | ✅ C++(`ULNPGameplayCueNotify_AttackerHitStop`) 연결 완료. 에디터 자산(`GCN_LNP_Melee_AttackerHitStop`) 생성·리페어런트·디스크 저장 확인. PIE 검증 미실시 |

`GameplayCue.LNP.Character.HitReact` 수신 시 `Multicast_ApplyHitStop` RPC 불필요. `FLNPApplyDamageGECommand`에서 이 두 Cue만 실행하고 RPC를 삭제한다.

### 7.1 GameplayCueNotify 구현 정책 — C++ 우선

GameplayCueNotify 로직은 블루프린트 이벤트 그래프 대신 C++로 구현하는 쪽을 선호한다 (§2.2의 "Iris 네이티브 경로 선호" 방침과 동일한 취지). `UGameplayCueNotify_Static::OnExecute`류는 `BlueprintNativeEvent`이므로 C++에서 `_Implementation`을 오버라이드하면 그래프 없이 동작하고, 블루프린트 애셋은 부모 클래스·`GameplayCueTag`(및 `ULNPGameplayCueNotify_VFXSound` 계열의 경우 `VFX`/`Sound`/`CameraShake` Class Defaults)만 지정하는 얇은 래퍼로 남긴다. 상세 내용은 Phase 2 절 참조.

---

## 8. 프로세서 공통 권한 패턴

모든 Mass 프로세서는 다음 3-구역 패턴을 따른다.

```
Execute()
├── 공통 시뮬레이션 (서버·클라 동일): 위치 갱신, 수명 감소 등 순수 수학 연산
├── 서버 전용 (bIsServer): GE 적용, 엔티티 소멸, 게임 상태 변경
└── 공격자 로컬 (bIsLocalInstigator): HitStop, VFX, 사운드
```

`bIsServer = (GetWorld()->GetNetMode() < NM_Client)`로 판단한다. `NM_Standalone(0)`, `NM_DedicatedServer(1)`, `NM_ListenServer(2)` → true. PIE 싱글 플레이어도 `NM_Standalone`이므로 서버 로직이 정상 실행된다.

---

## 9. 구현 단계별 계획

### Phase 1 — 기반 인프라 ✅ 완료

| 작업 | 변경 파일 | 결과 |
|:---|:---|:---|
| Iris 활성화 (`net.Iris.UseIrisReplication=1`) | `DefaultEngine.ini`, `LootNPop.Build.cs` | ✅ PIE 2플레이어 정상 연결 확인 |
| GAS 복제 모드 설정 (Mixed/Minimal) | `LNPPlayerState.cpp`, `LNPEnemyCharacter.cpp` | ✅ `LNPBaseAttributeSet` DOREPLIFETIME 추가. 2P HP Bar 정상 갱신 확인 |
| `ALNPEnemyCharacter` `bReplicates=true` | `LNPEnemyCharacter.cpp` | ✅ 클라이언트에서 적 Actor 표시 확인 |
| GameplayCue 에셋 생성 (Guard.Block, Parry.Success) | `/Game/GAS/GameplayCues/` (MCP 생성) | ✅ `GCN_LNP_Guard_Block`, `GCN_LNP_Parry_Success` 에셋 생성 |
| `CurvatureDelta` Mover SyncState 포함 여부 검증 | PIE 2플레이어 | ✅ **리스크 B 해소** — 적도 부근 이동 시 1P 화면에서 2P Up 벡터 정상 확인 |

**Phase 1 부수 작업 (동시 수행):**
- `ALNPPlayerController::AcknowledgePossession` 추가 → 클라이언트 HUD `InitViewModel` 호출 (OnPossess는 서버 전용)
- `ULNPEquipmentComponent` Grant/Revoke 경로에 `HasAuthority()` 가드 추가
- `ALNPCharacterBase::InitAbilitySystem` DefaultAbilities 부여에 `HasAuthority()` 가드 추가
- `ALNPPlayerCharacter::TryActivateAttack_Impl` 클라이언트 폴백 → `TryActivateAbilityByClass` 사용

### Phase 2 — HitStop 전파 (체감 개선) ✅ 완료 (PIE 2인, 고지연 포함 검증 완료)

> 전제: Phase 1 완료

**착수 전 발견한 버그 (수정 완료):** `TAG_GameplayCue_Guard_Block`/`TAG_GameplayCue_Parry_Success` 네이티브 태그 문자열이 `"LNP.GameplayCue.*"`로 정의되어 있었으나, Phase 1에서 MCP `AddCueTag`로 등록된 실제 태그와 `GCN_LNP_Guard_Block`/`GCN_LNP_Parry_Success` 에셋은 `"GameplayCue.LNP.*"` 계층이었다. 두 문자열이 일치하지 않아 `ExecuteGameplayCue` 호출이 어떤 Notify와도 매칭되지 않는 상태였다 (Guard/Parry 큐가 조용히 no-op). `LNPGameplayTags.cpp`에서 `"GameplayCue.LNP.Guard.Block"` / `"GameplayCue.LNP.Parry.Success"`로 수정. GameplayCue 태그는 반드시 `GameplayCue.` 루트로 시작해야 Notify 매칭이 성립한다 (MCP `AddCueTag` 툴도 이를 강제).

| 작업 | 변경 파일 | 결과 |
|:---|:---|:---|
| `FLNPApplyDamageGECommand`에서 두 GameplayCue 실행 | `LNPHitDetectionShared.h` | ✅ `Run()`에서 피격자 `VictimChar->PlayHitReact()`/`ApplyHitStop()` 직접 호출 제거 → `ExecuteGameplayCue(TAG_GameplayCue_Character_HitReact, ...)` 로 대체. 근접 히트(`bIsMeleeHit=true`)에 한해 `TAG_GameplayCue_Melee_Impact` 추가 발동. `FEntry`에 `bIsMeleeHit` 필드 추가, `LNPWeaponTraceProcessors.cpp` 3개 근접 호출부에서 `true` 전달(발사체 호출부는 기본값 `false`) |
| `PlayHitReact`/`ApplyHitStop`를 `BlueprintCallable`로 노출 | `LNPCharacterBase.h` | ✅ C++ GameplayCueNotify 서브클래스에서 직접 호출 가능하도록 노출 (아래 "GameplayCueNotify C++ 전환" 참조) |
| `GameplayCue.LNP.Character.HitReact` / `Melee.Impact` / `Guard.Block` / `Parry.Success` 태그·에셋 생성 | MCP (`AddCueTag` + `CreateCueNotifyAsset`) | ✅ 태그 등록 확인 (`ListCues`). 4개 `GCN_LNP_*` (Static) 생성 |

**빌드 검증:** Live Coding 컴파일 성공 확인 (`CompileLiveCoding`).

**후속 세션에서 발견 및 해결 — GameplayCueNotify 에셋이 디스크에 저장된 적이 없던 문제:**

지연/손실 PIE 재검증 중 HitStop/HitReact가 저·고지연 모두에서 동일하게 전혀 재생되지 않는 현상을 발견. 원인 추적 결과 `GCN_LNP_*` 4개 블루프린트가 **에디터 세션 메모리에만 존재하고 실제 디스크에 저장된 적이 없는 상태**였음이 확인됨 (`is_dirty=false`로 "이미 저장됨"처럼 보였으나 `Content/GAS/GameplayCues/`에 `.uasset` 파일 자체가 없었고, git에도 흔적 없음 — MCP `CreateCueNotifyAsset`으로 생성한 애셋이 저장 단계 없이 세션에만 남아있던 것으로 추정). `save_assets` MCP 호출도 "All files are already saved"로 무반응이었고, 에디터에서 직접 애셋을 열어 수동으로 컴파일·저장한 뒤에야 디스크에 실제 파일이 생성됨을 확인. **교훈: MCP로 생성한 애셋은 생성 직후 반드시 디스크 저장 여부를 (에디터 UI 또는 파일시스템에서) 직접 확인할 것 — `is_dirty=false`/`save_assets`의 성공 응답만으로는 신뢰할 수 없다.**

**GameplayCueNotify C++ 전환 (블루프린트 그래프 대신 네이티브 구현) — 프로젝트 방침 확정:**

블루프린트 이벤트 그래프 대신 C++로 로직을 구현하는 쪽을 선호하기로 결정. `UGameplayCueNotify_Static`의 `OnExecute`(`OnActive`/`WhileActive`/`OnRemove`도 동일)는 `BlueprintNativeEvent`라 C++ 서브클래스가 `_Implementation`을 오버라이드하면 그래프 없이 그대로 동작한다. 블루프린트 애셋은 부모 클래스 + `GameplayCueTag`만 지정하는 얇은 래퍼로 남긴다.

| 클래스 | 역할 | 대상 |
|:---|:---|:---|
| `ULNPGameplayCueNotify_HitReact` | `MyTarget`을 `ALNPCharacterBase`로 캐스팅해 `PlayHitReact(Parameters.Normal)` + `ApplyHitStop(0.08f)` 호출 | `GCN_LNP_Character_HitReact` |
| `ULNPGameplayCueNotify_VFXSound` | `EditDefaultsOnly`로 노출한 `VFX`(Niagara)·`Sound`·`CameraShake`를 재생하는 공용 베이스. 캐릭터 로직 호출이 필요 없는 순수 코스메틱 큐 3종이 공유 | `GCN_LNP_Guard_Block`, `GCN_LNP_Melee_Impact`, `GCN_LNP_Parry_Success` |

신규 파일: `Source/LootNPop/GAS/GameplayCues/LNPGameplayCueNotify_HitReact.h/.cpp`, `LNPGameplayCueNotify_VFXSound.h/.cpp`. 기존 4개 블루프린트는 `set_parent` MCP 툴로 위 클래스에 리페어런트 완료 (`get_parent`로 검증).

**현재 상태:** VFX/Sound 에셋(파티클·사운드)이 프로젝트에 아직 하나도 없어 `VFX`/`Sound` 프로퍼티는 3개 큐 모두 미설정 상태로 남겨둠 — 에셋이 준비되면 Class Defaults에서 지정만 하면 됨(그래프 수정 불필요). `Parry.Success`의 `CameraShake`만 임시로 엔진 기본값(`DefaultCameraShakeBase`)이 지정돼 있어 추후 커스텀 클래스로 교체 필요.

**PIE 2인 검증 결과 (저지연 + `net.PktLag=150/250` 고지연 양쪽):** P1↔P2 양방향 근접 공격에서 HitReact 몽타주 재생 및 HitStop 정상 적용 확인.

### Phase 3 — 근접 클라이언트 예측 + Lag Compensation ✅ 코드 완료 + PIE 2인 검증 완료 (지연·손실 시뮬레이션 포함)

> 전제: Phase 1 + Phase 2 완료

**설계 변경 사항 (구현 중 확정):**
- `FLNPCapsuleFragment`는 도입하지 않았다. Enemy 캡슐 크기는 기존 `FLNPEnemySharedFragment::Config`로, Player는 `UCapsuleComponent`로 이미 조회 가능해 별도 Mass Fragment로 이중화할 필요가 없었다 (단순성 우선).
- `CollectReplicatedActorTargets()`는 Fragment 타입별 오버로드 대신 `(UWorld*, FVector Center, float Radius)` 시그니처 하나로 통일했다. Physics Scene 쿼리 대신 `TActorIterator<ALNPCharacterBase>` + 거리 필터를 사용한다 — `ALNPCharacterBase`의 `CapsuleComponent`가 `Pawn` 콜리전 프로파일을 명시적으로 설정하지 않아 오브젝트 채널 쿼리 신뢰성이 불확실했기 때문. 근접(Phase 3)·원거리(Phase 4) 양쪽에서 그대로 재사용 가능하다.
- **부수 발견 및 수정 (Guard/Parry 입력이 서버에 전혀 전달되지 않던 버그):** `ULNPInputHandlerComponent::OnGuardStarted/Released`는 Enhanced Input 콜백이라 입력을 소유한 머신에서만 실행되고, `FLNPParryStateFragment`는 로컬 갱신만 하고 있었다. 즉 리슨 서버 호스트를 제외한 모든 원격 클라이언트는 Guard/Parry가 서버에 절대 반영되지 않는 상태였다 — Phase 3 착수 전부터 존재하던 문제. `Server_SetGuardState(bool)` RPC를 추가해 해결했다 (아래 표 참조). `TechDesign_ParrySystem.md` §2.2는 이 RPC 경로를 반영하지 못한 상태이므로 참고 시 주의.
- **부수 발견 및 수정 (`UAnimNotifyState` 싱글턴 공유로 인한 근접 판정 완전 실패, PIE 2인 실측 중 발견):** `UAnimNotifyState`(`UANS_LNPMeleeHitWindow`)는 그 몽타주를 재생하는 **모든 AnimInstance가 공유하는 단일 오브젝트**다 (UE 공식 문서에 명시된 제약). 기존 코드는 진행 중인 스윙의 `FMassEntityHandle MeleeEntity`를 노티파이 자신의 멤버 변수로 저장했는데, 이 때문에 서버 월드의 `NotifyBegin`이 값을 세팅한 직후 같은 캐릭터의 클라이언트 월드 `NotifyBegin`이 같은 필드를 덮어써 서버 쪽 판정용 엔티티가 참조를 잃고 TTL로만 파괴되는 문제였다. 호스트(서버=클라이언트가 동일 월드)가 공격할 때는 경합이 없어 정상 동작했지만, 원격 클라이언트가 근접 공격하면 서버가 히트 판정을 전혀 수행하지 못해 데미지가 100% 유실됐다. **PIE 특유의 문제가 아니다** — 데디케이티드 서버 등 완전히 분리된 프로세스 환경에서도, 같은 무기를 쓰는 캐릭터 둘 이상이 스윙 구간을 겹치게 실행하면 동일하게 재현된다 (서버 프로세스가 여러 플레이어를 함께 처리하므로). PIE 2인 환경은 "호스트 혼자 공격해도 서버 실행분·클라이언트 실행분이 항상 동시에 겹치는" 조건이라 100% 재현됐던 것뿐. `MeshComp`(캐릭터별로 유일)를 키로 하는 `TMap<TWeakObjectPtr<USkeletalMeshComponent>, FActiveSwing>`으로 상태를 분리해 해결. 다른 `UAnimNotifyState` 4종(`ANS_LNPComboWindow`, `ANS_LNPAttackInputBlock`, `ANS_LNPBlockMovementInput`, `ANS_LNPCancelMontageOnMovement`)은 전부 상태 없는(stateless) 구조라 동일 버그 없음을 확인.
- **부수 발견 및 수정 (근접 PvP는 Guard/Parry를 애초에 체크한 적이 없던 기존 설계 공백):** `ULNPWeaponTraceHitDetectionProcessor` Pass 3의 "Player 공격 → Player 타겟(아군 사격)" 분기는 거리 체크 후 바로 `FLNPApplyDamageGECommand`를 실행했고, "Enemy 공격 → Player 타겟" 분기에만 존재하던 패링·가드 2단계 판정이 아예 없었다 — Phase 3 이전부터의 설계 공백이며 원거리(`ULNPProjectileHitDetectionProcessor`)는 `bShouldProcess = InstigatorTeam==Enemy || bFriendlyFire`로 두 케이스를 이미 통합해 PvP 패링·가드가 정상 동작하고 있었으므로 근접·원거리 구현이 서로 비대칭이었다. 근접 PvP 분기에도 동일한 2단계 판정(패링 → 가드 → 피격)을 추가해 해결.
- **부수 발견 및 수정 (지연/손실 재검증 세션 — 무기 애니메이션 레이어 레이스 컨디션):** `ALNPCharacterBase::BeginPlay()`가 `EquippedWeaponData` 값과 무관하게 무조건 `AnimSourceMesh->LinkAnimClassLayers(UnarmedAnimLayerClass)`를 호출하고 있었다. 원격 관전 대상(시뮬레이티드 프록시) 폰이 스폰될 때 `OnRep_CurrentWeapon()`(서버가 이미 부여한 무기로 정확히 링크)과 `BeginPlay()`(무조건 Unarmed로 링크)의 실행 순서가 고정돼 있지 않아, `BeginPlay`가 나중에 실행되면 이미 올바르게 적용된 무기 레이어가 Unarmed로 덮어써지고 이후 무기가 다시 바뀌기 전까지 고정되는 문제였다 — 상대 캐릭터가 무기를 들고 있는데 간헐적으로 맨손 포즈를 취하는 현상으로 관측됨. `BeginPlay()`가 `OnRep_CurrentWeapon()`과 동일하게 `ALNPCharacterBase::EquipWeapon(EquippedWeaponData)`를 (virtual 우회로) 호출하도록 수정 — `EquippedWeaponData`가 아직 미수신이면 `EquipWeapon`의 기존 폴백 로직이 자동으로 Unarmed를 적용하므로 동작은 동일하되 이미 수신된 값을 존중하게 됨. 처음엔 virtual 호출(`EquipWeapon(...)`)로 수정했다가, `ALNPPlayerCharacter::EquipWeapon`의 `Server_EquipWeapon()` RPC가 소유하지 않은 액터(원격 관전 대상)에서도 호출 시도되는 부작용을 발견해 스코프 지정 호출로 재수정. PIE 재검증에서는 재현되지 않았으나 원래도 간헐적으로만 발생하던 레이스라 추가 관찰 필요.

**PIE 2인 검증 결과:**
- ✅ 1P(호스트)↔2P(클라이언트) 상호 근접 공격 — 양방향 HP 차감·HUD 반영 정상 (수정 전: 클라이언트→호스트 방향 100% 실패)
- ✅ 1P↔2P 원거리(투사체) 공격 — 기존부터 정상 (Ranged는 GA가 서버에서 직접 엔티티를 생성해 AnimNotifyState 경로를 타지 않음)
- ✅ `LNP.Debug.AuthorityAutoAction`/`ClientAutoAction` 디버그 콘솔 토글로 자동 공격/가드 검증 — 각각 의도한 쪽(호스트/클라이언트)에만 정확히 적용됨 확인
- ✅ **지연/손실 시뮬레이션 재검증 완료** (`net.PktLag=150`, `net.PktLag=250` 양방향): 1P↔2P 상호 근접 공격을 이리저리 뛰어다니며 맞추기/빗나가게 하기 모두 시도 — Lag Compensation 체감상 위화감 없음 (정밀 측정치는 아니고 정성적 확인)
- ✅ 근접 PvP Guard/Parry — 저지연(PktLag=0)·고지연(PktLag=250) 모두 정상 동작 확인 (1P 오토 공격 켜고 2P 가드/패링, 반대 방향도 확인). 고지연에서는 수동 타이밍이 어려웠지만 가드 연타 중 패링이 간헐적으로 성공하는 것 확인
- ✅ 고지연(적도 부근 이동 관찰)에서도 Up 벡터 정상 — 리스크 B 저지연 검증과 동일 결과
- ✅ HitReact/HitStop 전파 — Phase 2 재작업(GameplayCueNotify C++ 전환) 이후 P1↔P2 양방향, 저·고지연 모두에서 HitReact 몽타주 정상 재생 확인

| 작업 | 변경 파일 | 결과 |
|:---|:---|:---|
| `FLNPWeaponTraceFragment`에 `bIsLocalInstigator`·`bLocalFeedbackFired` 추가 | `LNPWeaponTraceMassTypes.h` | ✅ |
| `UANS_LNPMeleeHitWindow`에서 `bIsLocalInstigator = Character->IsLocallyControlled()` 설정 | `ANS_LNPMeleeHitWindow.cpp` | ✅ 엔티티 생성 자체는 기존부터 서버·클라 각자 무조건 실행 중이었음 (설계 의도와 일치, ANS에는 HasAuthority 분기 없음) |
| `ULNPWeaponTraceHitDetectionProcessor` 권한 분기 (`bIsServer` 조기 분기) | `LNPWeaponTraceProcessors.cpp` | ✅ 서버 경로(GE 적용)와 클라 경로(코스메틱만) 완전 분리. 이전에는 클라이언트에서도 GE 적용을 시도하던 잠재 버그였음 |
| `CollectReplicatedActorTargets()` 구현 | `LNPHitDetectionShared.h` | ✅ 근접·원거리 공용 단일 함수로 구현 (위 설계 변경 참조) |
| `ApplyLocalHitFeedback()` 추가 | `LNPCharacterBase.cpp` | ✅ 현재는 `ApplyHitStop(0.08f)`만 수행. VFX는 미정 — 필요 시 후속 추가 |
| `FLNPPositionHistoryFragment` 정의 + Enemy 아키타입 추가 | `LNPPositionHistoryFragment.h/.cpp` (신규), `LNPEnemyMassTypes.cpp`(`ULNPEnemyTrait::BuildTemplate`) | ✅ 컴파일 통과. 링버퍼 최대 5샘플 |
| `ULNPPositionHistoryRecordProcessor` 구현 (서버 전용, 50ms 간격) | `LNPPositionHistoryProcessors.h/.cpp` (신규) | ✅ `PrePhysics` 단계, `GetNetMode() >= NM_Client` 조기 반환 |
| 플레이어 폰 아키타입에 `FLNPPositionHistoryFragment` 추가 | `DA_PlayerEntityConfig`(`MassAssortedFragmentsTrait.Fragments`, MCP로 편집) | ✅ 캡슐 동기화 Processor는 불필요(설계 변경 참조) |
| `ULNPWeaponTraceHitDetectionProcessor` 서버 경로 Lag Compensation 적용 | `LNPWeaponTraceProcessors.cpp` | ✅ 공격자 RTT/2 기반 `RewoundCenter()`로 Enemy·Player 캡슐 중심을 되감아 판정. 클램프 상한 200ms |
| Guard/Parry 상태 서버 복제 (부수 발견 수정) | `LNPInputHandlerComponent.h/.cpp` | ✅ `Server_SetGuardState(bool)` RPC 추가 — 리슨 서버 호스트도 동일 경로로 호출(HasAuthority 분기 없음, 자기 자신 호출 시 즉시 로컬 실행됨) |
| `FLNPMeleeParryCommand` / `FLNPProjectileParryCommand` 패링 타이밍 RTT 역보정 | `LNPWeaponTraceProcessors.cpp`, `LNPProjectileProcessors.cpp`, `LNPGuardParryTypes.h` | ✅ `FLNPParryStateFragment::ParryWindowExpiryTime`(서버 절대 만료 시각) 추가. `Server_SetGuardState`가 방어자 RTT/2를 앞당겨 계산, 클램프 상한 0.075s(패링 창 절반) |

**빌드 검증:** Live Coding 컴파일 성공 (총 5회 반복 — `APlayerState` 미포함으로 인한 1회 실패 후 `#include "GameFramework/PlayerState.h"` 추가로 해결).

**공격자 HitStop 네트워크 커버리지 정리 (세션 중 확인):** `CustomTimeDilation`은 `AActor`에서 복제되지 않는 순수 로컬 프로퍼티다. 공격자 HitStop은 토폴로지별로 서로 다른 두 경로가 각각 커버한다 — Standalone·리슨서버 호스트 공격은 `FLNPApplyDamageGECommand::Run()`(서버 전용)의 `AttackerChar->ApplyHitStop(0.08f)` 직접 호출로(호스트 자신의 액터 인스턴스라 지연 없이 즉시 반영), 원격 클라이언트 공격은 해당 클라이언트 자신의 `NM_Client` 프로세스에서 도는 `ApplyLocalHitFeedback()`(클라 전용 예측 분기)로 처리된다. `bIsServer` 분기가 프로세서 전체 단위로 걸리기 때문에(`NM_Standalone`/`NM_DedicatedServer`/`NM_ListenServer` 전부 `bIsServer=true`) 리슨서버·데디케이티드 서버 프로세스 자체에서는 `ApplyLocalHitFeedback()`이 실행되지 않지만, 데디케이티드 서버는 렌더링하는 로컬 플레이어가 없어 애초에 필요 없다. 두 경로 모두 삭제하면 안 됨.

### Phase 4 — 원거리 클라이언트 예측 (Ghost Projectile) ✅ 코드 완료, B안(발사 시점 방송) 유지로 최종 결정 — PIE 재검증 대기

> 전제: Phase 3 완료.

**설계 변경 사항 (구현 중 확정):**
- **`HasAuthority()` 분기 제거는 애초에 불필요했다.** `ULNPAbility_RangedAttack::ActivateAbility`는 원래부터 `HasAuthority()` 가드가 없었고, `NetExecutionPolicy`를 어디서도 오버라이드하지 않아 엔진 기본값 `LocalPredicted`(열거형 0번)를 그대로 쓰고 있어 이미 서버·소유 클라 양쪽에서 각자 `SpawnProjectile()`을 독립 실행하고 있었다 (Path A 구조 기 성립).
- **`FScopedPredictionWindow`는 사용하지 않는다.** GAS가 `LocalPredicted` 어빌리티의 `ActivateAbility` 호출 구간에 예측 키(`ActivationInfo.GetActivationPredictionKey()`)를 이미 세팅해 두므로 새 윈도우 없이 값을 읽기만 하면 된다.
- **진짜 선행 버그:** `ULNPProjectileHitDetectionProcessor::Execute`에 `bIsServer` 분기가 아예 없어 서버·클라 양쪽에서 동일하게 GE를 적용하고 있었다 (근접은 Phase 3에서 이미 고친 문제였으나 원거리는 미착수 상태였음). 이번 Phase 4에서 WeaponTrace와 동일한 패턴으로 클라이언트 조기 분기를 추가해 해결.
- **롤백 훅은 `FPredictionKeyDelegates`를 쓴다.** 서버가 `CommitAbility`를 거부하면 `ClientActivateAbilityFailed_Implementation`이 `FPredictionKeyDelegates::BroadcastRejectedDelegate(PredictionKey)`를 호출한다(`AbilitySystemComponent_Abilities.cpp`). 다만 정적 메서드 `FPredictionKeyDelegates::NewRejectedDelegate(KeyType)`는 `UE_API`가 누락돼 모듈 밖에서 링크되지 않아(`LNK2019`), `FPredictionKey`를 로컬 사본으로 복사해 멤버 버전 `FPredictionKey::NewRejectedDelegate()`(노출됨)를 사용하도록 우회했다.
- **`CollectReplicatedActorTargets()` 오버로드 불필요.** Phase 3에서 이미 `(UWorld*, FVector Center, float Radius)` 시그니처 하나로 통일돼 있어 원거리 클라이언트 분기에서 그대로 재사용했다.
- **Ghost 등록 조건은 `IsLocallyControlled() && IsPredictingClient()`.** `IsPredictingClient()`(`!IsNetAuthority() && IsLocallyControlled() && NetExecutionPolicy가 LocalPredicted/ServerInitiated`)로 제한해 Standalone/리슨서버 호스트는 애초에 Ghost로 등록하지 않는다 — 그 경우 엔티티가 하나뿐이라 서버 Pass 3의 `FLNPApplyDamageGECommand` 직접 호출 경로 하나만으로 HitStop이 충분하고, Ghost 매커니즘까지 개입하면 중복 재생된다.
- **공격자 HitStop 부수 수정 → 근접 전용으로 재확정 (PIE 플레이테스트 후).** 기존 Pass 3의 모든 `FLNPApplyDamageGECommand::Add(...)` 호출이 `AttackerEntity` 자리에 빈 `FMassEntityHandle{}`을 넘기고 있어 공유 커맨드의 "공격자 HitStop 직접 호출" 경로가 Projectile에서는 한 번도 발동한 적이 없었다. `Proj.Instigator`(실제 핸들)를 넘기도록 인자만 수정해 처음엔 근접·원거리 공통으로 재생되게 했으나, PIE 검증 중 "원거리(총기류)는 물리적 충돌감이 없어 공격자 HitStop이 어색하다"는 피드백으로 근접 전용으로 재조정했다. `FLNPApplyDamageGECommand::Run()`에서 `Entry.bIsMeleeHit`일 때만 공격자 HitStop을 재생하도록 가드를 추가하고, Ghost 경로 양쪽(`ULNPProjectileHitDetectionProcessor` 클라 분기의 브랜치 A, `ULNPGameplayCueNotify_ProjectileImpact`의 브랜치 B)에서는 `ApplyLocalHitFeedback()` 호출 자체를 제거했다 — Ghost 소멸·임팩트 VFX는 그대로 유지. **피해자 HitStop/HitReact는 근접·원거리 구분 없이 항상 재생** (`GameplayCue.LNP.Character.HitReact`, 변경 없음).
- **Impact VFX는 GameplayCue로 일원화.** Pass 3가 캐릭터 피격 시 직접 호출하던 `VisualSub.EnqueueImpact(...)`를 제거하고 `GameplayCue.LNP.Projectile.Impact`로 대체했다. 지형 충돌 시(`ULNPProjectileMovementProcessor`, 데미지·공격자 개념 없음)의 `EnqueueImpact` 호출은 스코프 밖이라 그대로 둠 — 비공격자(관전) 클라이언트는 여전히 지형 임팩트를 보지 못하는 기존 한계가 남아있다 (리스크 A 섹션 참조 대상 아님, 별도 트래킹 필요).
- **Ghost TTL 스윕은 별도 Tick 없이** `ULNPProjectileHitDetectionProcessor`의 클라이언트 분기가 매 프레임 실행되는 지점에서 `ULNPGhostProjectileSubsystem::SweepExpiredGhosts(0.5f)`를 호출해 구현.
- **트레일 Niagara Component 누수 방지 (구현 중 발견):** `ULNPGhostProjectileSubsystem`이 Ghost 엔티티를 파괴하는 모든 경로(브랜치 B, 롤백, TTL)에서 `FLNPProjectileVisualFragment::bInitialized`를 확인해 `ULNPProjectileVisualSubsystem::EnqueueTrailRelease`를 함께 호출하도록 했다 — 안 하면 클라이언트 예측 Ghost가 파괴될 때마다 트레일 Niagara Component가 고아로 남아 영구 렌더링된다.
- **원거리 Lag Compensation 적용 완료 (후속 세션).** `ULNPProjectileHitDetectionProcessor` 서버 경로(Pass 1~3)에 WeaponTrace와 동일한 RTT 되감기를 이식했다. Enemy/Player 아키타입에는 이미 `FLNPPositionHistoryFragment`가 있었으므로(Phase 3에서 구축) 새 Fragment·새 프로세서 없이 기존 히스토리를 "읽기"만 추가하면 됐다 — **히스토리 버퍼는 피격 대상(Enemy·Player) 쪽에만 존재하고 Projectile 자신은 히스토리를 저장하지 않으므로**, 수백 개가 빈번히 생성·소멸하는 Projectile 특성과 무관하게 메모리·기록 비용이 전혀 늘지 않는다. `RewindSeconds`는 WeaponTrace와 동일하게 **매 프레임 `Proj.Instigator`의 현재 핑으로 재계산**한다(클램프 200ms).
  - **알려진 트레이드오프 (미해결, 향후 개선 후보):** 근접 스윙은 활성 창이 1초 미만이라 매 프레임 재계산해도 체감 문제가 없지만, Projectile은 `ProjectileLifetime`이 최대 5초까지 간다. 매 프레임 재계산 방식은 발사체가 날아가는 **내내** 대상의 최대 200ms 전(前) 위치와 비교하므로, 느리거나 유도되는 발사체일수록 "대상이 이미 피했는데도 계속 과거 잔상을 쫓아가 맞는" 체감으로 이어질 수 있다. 실제 플레이테스트에서 이 현상이 느껴지면, **`RewindSeconds`를 발사체가 서버에서 처음 처리되는 시점(발사 시점)에 1회만 계산해 `FLNPProjectileFragment`에 캐싱하고 비행 내내 재사용**하는 방식으로 바꾼다 — "공격자가 조준해서 쏜 순간의 지연"만 보정하고 이후엔 대상의 실제(현재) 위치와 비교하게 되어 더 직관적으로 동작하며, 매 프레임 `Instigator → Actor → PlayerState → Ping` 조회를 반복하지 않아도 되어 오히려 더 가볍다.
- **넉백 방향 반전 버그 수정 (PIE 플레이테스트 중 발견).** `FLNPApplyDamageGECommand::Run()`이 `Entry.HitFromDirection`("피격자→공격자" 방향, `PlayHitReact`의 방향 판정 컨벤션)을 `ApplyKnockback`에 그대로 넘기고 있었다. `ApplyKnockback`은 전달받은 방향으로 그대로 속도를 가하므로, 넉백이 매번 **공격자 쪽으로** 걸리는 게 실제 동작이었다(원거리 연사 시 피해자가 조금씩 당겨지는 현상, 근접 "넉백 무반응"의 원인으로 추정). `-Entry.HitFromDirection`으로 반전해 수정 — 근접·원거리·스플래시 전부 이 한 곳을 공유하므로 단일 수정으로 전체 커버.
- **공격자 HitStop 제3자(구경꾼) 가시성 — 신규 `GameplayCue.LNP.Melee.AttackerHitStop`.** 근접 공격자 HitStop(`CustomTimeDilation`)은 복제 안 되는 로컬 전용 값이라, 기존에는 "공격자 본인 화면"만 커버했지 "공격자를 지켜보는 제3자" 화면에는 아예 전달할 경로가 없었다(버그가 아니라 미구현 범위). 서버 Pass 3(`FLNPApplyDamageGECommand::Run`, `Entry.bIsMeleeHit`)에서 **공격자의 ASC**에 신규 Cue를 실행해 해결 — GameplayCue는 그 액터가 리플리케이트되는 모든 클라이언트에 자동 전파되므로 피해자 HitReact와 동일한 신뢰성을 얻는다. 핸들러(`ULNPGameplayCueNotify_AttackerHitStop`)는 `MyTarget->IsLocallyControlled()`가 true면 no-op — **공격자 본인 화면은 계속 예측 경로(서버 직접 호출/`ApplyLocalHitFeedback`)로만 처리한다** — 근접은 공격 모션 도중 HitStop이 걸려 체감이 매우 커서 예측 없이 서버 왕복을 기다리면 안 된다는 판단.
- **공격자 본인 HitStop 예측 경로 버그 → 근본 해결.** "원격 클라이언트가 공격할 때 자기 자신의 화면에서도 예측 HitStop이 안 걸린다"는 리포트로 시작. 진단 로그(`[HitStopTrace][ANS][InstigatorEntity]`)로 확인한 결과, `AgentComp->GetEntityHandle()`이 매번 실패하는 게 아니라 **같은 캐릭터에 대해 짧은 간격(3ms)의 연속 두 `NotifyBegin` 호출 중 첫 번째는 성공, 두 번째는 실패**하는 간헐적 레이스였다 — `UMassActorSubsystem::GetEntityHandleFromActor` 역조회 폴백도 같은 호출에서 함께 실패해, "Mass 엔티티 핸들 경유 조회" 자체가 이 타이밍에 근본적으로 신뢰할 수 없다고 판단.
  - **근본 원인:** 클라이언트 예측 분기는 `ANS_LNPMeleeHitWindow::NotifyBegin`이 이미 `Character` 포인터를 직접 들고 있는데도, 이를 Mass 엔티티 핸들(`InstigatorEntity`)로 변환했다가 `ULNPWeaponTraceHitDetectionProcessor`에서 다시 `UMassActorSubsystem::GetActorFromHandle(...)`로 역조회하는 불필요한 왕복을 거치고 있었다. 이 왕복은 원래 **서버 Pass 3(Mass 워커 스레드)**가 스레드 안전하게 공격자를 찾기 위해 필요한 것이었지만, 클라이언트 예측 분기는 게임 스레드에서 로컬로만 도는 코드라 애초에 이 왕복이 필요 없었다.
  - **수정:** `FLNPWeaponTraceFragment`에 클라이언트 예측 전용 필드 `TWeakObjectPtr<ALNPCharacterBase> InstigatorActor`를 추가. `NotifyBegin`이 `Character`를 직접 대입하고, 클라이언트 분기는 `Frag.InstigatorActor.Get()`으로 즉시 조회 — Mass 핸들 경유 자체를 제거해 레이스의 근본 원인을 없앴다. 서버 경로가 쓰는 `InstigatorEntity`(Mass 핸들)는 이미 정상 동작하므로 그대로 유지.
  - 진단 로그(`[HitStopTrace][...]`)는 원인 확인 완료로 전부 제거.
- **Projectile 시뮬레이티드 프록시(제3자) 가시성 — B안(발사 시점 1회 방송) 채택.** 기존엔 서버·소유 클라만 각자 엔티티를 생성해(Path A), 구경꾼(제3자) 클라이언트에는 날아가는 발사체가 전혀 보이지 않았다(리스크 A에 남겨둔 장기 과제였으나, 실사용 중 "매우 심각한 문제"로 재분류). 두 가지 대안을 검토했다:
  - **A안 — Enemy와 동일한 완전 MassReplication.** 서버 권위 엔티티를 `FMassNetworkID`로 전 클라 복제. 가장 정확하지만 발사체는 Enemy보다 훨씬 빠르고 수가 많아(초당 다수 발사) 매 프레임 위치 복제 대역폭 부담이 크고, BubbleHandler·복제 스키마 신설 등 Phase 6급 작업이 필요하다.
  - **B안(채택) — 발사 시점 1회 Unreliable Multicast로 스폰 파라미터만 방송, 각 클라이언트가 독립적으로 시각 전용 로컬 엔티티를 생성해 시뮬레이션.** 대역폭이 발사당 1회로 매우 저렴하고, 이미 구축된 Ghost 인프라를 거의 그대로 재사용할 수 있어 구현 규모가 작다. 대가로 궤적이 시간이 지나며 서버와 미세하게 어긋날 수 있으나, 순수 코스메틱이라 게임플레이 무결성에는 영향이 없다(리스크 D와 동일한 트레이드오프).
  - **구현:** `ALNPCharacterBase::Multicast_SpawnGhostProjectiles`(Unreliable) 신규 — 서버만(`HasAuthority()`) 호출, `FLNPProjectileSharedFragment` + 스폰 위치·속도 배열·수명·PredictionKeyID·InstigatorPlayerID를 방송한다. 수신 측은 `NM_Client`이면서 `IsLocallyControlled()==false`인 경우에만(자기 자신이 쏜 발사체·서버/호스트 자신은 스스로 걸러냄) 로컬 엔티티를 생성하고 `bIsLocalInstigator=false`로 표시(로컬 히트 판정에서 자동 제외됨) + `ULNPGhostProjectileSubsystem`에 **공격자 Ghost와 동일한 (PredictionKeyID, SpawnIndex) 키로 등록**한다. 서버 확정 `GameplayCue.LNP.Projectile.Impact`의 비공격자 분기가 이 키로 `DestroyGhost(...)`를 호출해 정리하므로, 신규 소멸 로직 없이 기존 인프라만으로 완결된다. 부수 수정: `FLNPProjectileSharedFragment`의 `ParryRadius`/`KnockbackStrength`/`SplashKnockbackStrength`에 누락돼 있던 `UPROPERTY()`를 추가 — RPC 파라미터로 쓰이려면 리플렉션 기반 직렬화가 필요한데 이 세 필드만 빠져 있었다(로컬 전용으로만 쓰일 때는 문제없었음).
  - **패링 반사 전파 보강 (PIE 플레이테스트로 확인 후 해결).** 실사용 중 "패링 반사 후 제3자 화면에서 반사된 발사체가 안 보인다"가 확인돼, 애초 계획대로 B안의 가장 큰 한계였음이 실증됐다. 서버 Pass 3가 `Proj.Velocity`/`InstigatorTeam`을 뒤집는 바로 그 지점에서 `FLNPProjectileParryCommand`에 새 속도·진영·(PredictionKeyID, SpawnIndex)를 실어(`LNPHitDetectionShared.h`), 방어자(패링 성공자) 캐릭터의 신규 `ALNPCharacterBase::Multicast_ReflectGhostProjectile`(Unreliable)로 전 클라에 방송한다. 수신 측은 `ULNPGhostProjectileSubsystem::ReflectGhost(...)`가 같은 키로 등록된 관전용 Ghost를 찾아 속도·진영만 갱신한다(엔티티 재생성 없음). 서버/호스트 자신은 `Multicast_SpawnGhostProjectiles`와 동일하게 `NM_Client`가 아니면 스스로 걸러낸다.
  - **A안 재검토 후 기각, B안 유지로 최종 결정 (PIE 플레이테스트 후 논의, 2026-07-03).** 패링 반사 전파 보강 이후에도 반사된 발사체 속도가 체감상 너무 빨라 보기 어렵고, `HitRadius`(무기 데이터 기준 5cm — 더 줄이기 곤란)와 캐릭터 캡슐 반경 합산으로 인한 캐릭터 충돌 시 조기 소멸까지 겹쳐 전반적인 어색함이 크다는 문제로 한때 A안(MassReplication) 전환을 고려했으나, 재검토 결과 **두 문제 모두 네트워크 동기화 방식과 무관한 별개의 튜닝 이슈**였음이 드러나 A안 전환을 기각했다:
    - 반사 속도 문제는 반사된 발사체의 실제 속도값 자체가 빠른 것 — VFX/밸런스 튜닝 대상이며, A안으로 바꿔도 서버가 계산한 같은 속도값을 그대로 복제할 뿐이라 동일하게 빨라 보인다.
    - 조기 소멸은 순수 충돌 판정 반경(HitRadius+캡슐 반경) 문제 — A안이어도 서버 권위 판정 자체는 동일한 반경으로 이뤄진다.
    - 발사체처럼 빠르고 수명 짧고 개수 많은 엔티티는 "매 프레임 서버 위치 복제"보다 "스폰 파라미터만 정확히 동기화 + 로컬 독립 시뮬레이션"이 네트워크 관점에서도 더 적합하다고 판단(총알 속도가 빠르면 복제 지연을 물리적으로 못 이기는 건 A안도 마찬가지이며 보간·외삽 아티팩트만 추가됨). Enemy(적은 수·긴 수명·정밀한 실시간 위치 중요)와 Projectile(많은 수·짧은 수명·대략적인 궤적이면 충분)은 애초에 성격이 달라 같은 해법이 적합하지 않다.
    - **결론: B안(발사 시점 1회 방송)을 그대로 유지한다.** A안 전환 계획은 폐기.
  - **향후 개선 후보 — 관전용 Ghost 스폰에 Dead Reckoning(외삽) 적용.** 지금까지 써온 Lag Compensation(`RewoundCenter`, `FLNPPositionHistoryFragment`)과는 다른 방향의 보정이다: Lag Compensation은 서버가 히트 판정 시 대상 위치를 **과거로** 되감지만, 여기서 필요한 건 관전용 Ghost 스폰 시 발사체 위치를 **미래로** 외삽하는 것이다 — `Multicast_SpawnGhostProjectiles`가 관전자에게 도착하는 시점엔 이미 네트워크 지연만큼 시간이 흘러 실제 서버 발사체는 더 날아가 있으므로, Ghost를 `SpawnPos`가 아니라 `SpawnPos + Velocity * 예상지연(핑 기반, 클램프 적용)` 위치에서 시작시키면 다른 클라이언트가 보는 위치와의 괴리를 줄일 수 있다. 히스토리 버퍼 조회가 필요 없어(스폰 시점 상태 + 경과 시간만으로 계산) `FLNPPositionHistoryFragment` 재사용 없이 구현 가능. 미구현 — 체감상 필요하면 추가.

| 작업 | 변경 파일 | 결과 |
|:---|:---|:---|
| `FLNPProjectileFragment`에 `bIsLocalInstigator`, `InstigatorPlayerID`, `PredictionKeyID`, `SpawnIndex` 추가 | `LNPProjectileMassTypes.h` | ✅ |
| `FLNPProjectileImpactContext` 정의 (`PredictionKeyID`, `SpawnIndex`, `InstigatorPlayerID`, `VFXData`) + `GetScriptStruct`·`Duplicate`·`NetSerialize` 오버라이드 | `LNPProjectileImpactContext.h/.cpp` (신규) | ✅ `UAbilitySystemGlobals::EffectContextStructCache`가 리플렉션으로 자동 스캔하므로 별도 등록 코드 불필요 확인 |
| `ULNPGhostProjectileSubsystem` 구현 (`RegisterGhost`, `DestroyGhost`, `DestroyAllGhostsForKey`, `SweepExpiredGhosts`) | `LNPGhostProjectileSubsystem.h/.cpp` (신규) | ✅ 트레일 해제 포함 |
| `ULNPAbility_RangedAttack::SpawnProjectile`에 예측 식별자 부여 + Ghost 등록 + 거부 롤백 바인딩 | `LNPAbility_RangedAttack.cpp` | ✅ |
| `ULNPProjectileHitDetectionProcessor` 권한 분기 — 클라이언트 조기 분기(코스메틱 HitStop + Ghost 소멸)와 서버 전용 GE 적용 경로 완전 분리 | `LNPProjectileProcessors.cpp` | ✅ 이전에는 클라이언트에서도 GE 적용을 시도하던 버그였음 |
| `GameplayCue.LNP.Projectile.Impact` 핸들러 구현 — 공격자·비공격자 분기 | `LNPGameplayCueNotify_ProjectileImpact.h/.cpp` (신규) | ✅ |
| `TAG_GameplayCue_Projectile_Impact` 태그 등록 | `LNPGameplayTags.h/.cpp`, `DefaultGameplayTags.ini` | ✅ |
| `GCN_LNP_Projectile_Impact` 에디터 자산 생성 + `ULNPGameplayCueNotify_ProjectileImpact`로 리페어런트 | MCP (`CreateCueNotifyAsset` + `set_parent`) | ✅ 디스크 저장 확인(`Content/GAS/GameplayCues/GCN_LNP_Projectile_Impact.uasset`, 6.6KB) |
| `ULNPProjectileHitDetectionProcessor` 서버 경로 Lag Compensation 적용 (`RewoundCenter`, WeaponTrace 패턴 이식) | `LNPProjectileProcessors.cpp` | ✅ Enemy/Player 캡슐 판정 3곳(Enemy 피격, Player 패링, Player 피격) 모두 적용. 매 프레임 재계산 방식 — 트레이드오프는 위 항목 참조 |
| 넉백 방향 반전 수정 (`-Entry.HitFromDirection`) | `LNPHitDetectionShared.h` | ✅ 근접·원거리·스플래시 공용 |
| 공격자 HitStop 근접 전용 재확정 | `LNPHitDetectionShared.h`, `LNPProjectileProcessors.cpp`, `LNPGameplayCueNotify_ProjectileImpact.cpp` | ✅ |
| `GameplayCue.LNP.Melee.AttackerHitStop` 신규 — 공격자 HitStop 제3자 가시성 | `LNPGameplayCueNotify_AttackerHitStop.h/.cpp`(신규), `LNPHitDetectionShared.h`, `LNPGameplayTags.h/.cpp`, `DefaultGameplayTags.ini` | ✅ 코드 완료 + 에디터 자산(`GCN_LNP_Melee_AttackerHitStop`) 생성·리페어런트·디스크 저장 확인 |
| Projectile 시뮬레이티드 프록시 가시성 (B안, 최종 유지 결정) | `LNPCharacterBase.h/.cpp`(`Multicast_SpawnGhostProjectiles` 신규), `LNPAbility_RangedAttack.cpp`, `LNPGameplayCueNotify_ProjectileImpact.cpp`, `LNPProjectileMassTypes.h`(UPROPERTY 누락 수정) | ✅ 체감 문제(반사 속도·조기 소멸)는 네트워크 동기화와 무관한 별개 튜닝 이슈로 재분류 — A안 전환 기각, 위 항목 참조 |
| 패링 반사 관전용 Ghost 전파 — 신규 `ALNPCharacterBase::Multicast_ReflectGhostProjectile` + `ULNPGhostProjectileSubsystem::ReflectGhost` | `LNPCharacterBase.h/.cpp`, `LNPGhostProjectileSubsystem.h/.cpp`, `LNPHitDetectionShared.h`(`FLNPProjectileParryCommand` 확장), `LNPProjectileProcessors.cpp` | ✅ PIE 플레이테스트로 한계 실증 후 보강 |
| 근접 예측 HitStop 근본 수정 — `FLNPWeaponTraceFragment::InstigatorActor`(`TWeakObjectPtr<ALNPCharacterBase>`) 신규, Mass 핸들 왕복 제거 | `LNPWeaponTraceMassTypes.h`, `ANS_LNPMeleeHitWindow.cpp`, `LNPWeaponTraceProcessors.cpp`(진단 로그 제거) | ✅ |

**빌드 검증:** UBT DebugGame 빌드 4회 성공(넉백/HitStop 근접 전용화, Projectile 방송 기능, 패링 반사 전파, 근접 예측 HitStop 근본 수정). 마지막 수정은 에디터가 닫혀 있어 Live Coding 반영 미확인 — 에디터 재실행 후 확인 필요. **PIE 재검증 대상**: 근접 예측 HitStop(2P 자신의 화면), 넉백 방향, AttackerHitStop 제3자 가시성, 패링 반사 전파. **Projectile 시뮬레이티드 프록시 가시성(B안)은 최종 유지로 결정** — 반사 속도·HitRadius 튜닝, 관전용 Ghost 스폰 Dead Reckoning 적용이 후속 개선 후보로 남음(위 항목 참조).

### Phase 4.5 — Ghost Projectile 정합성·관전 경험 개선 ✅ 코드 완료 (2026-07-04) — PIE 3인 검증 대기

> 전제: Phase 4 완료. 문서·코드 전수 대조 리뷰에서 발견된 정합성 버그 4건 수정 + 관전(제3자) 경험 개선.
> 설계 원칙: **"엔티티 최초 스폰의 위치·시간을 RTT에 맞게 보정해 각 머신이 독립 시뮬레이션"** (B안 유지·강화).

**발견·수정한 정합성 버그:**

| # | 버그 | 수정 |
|:---|:---|:---|
| 1 | 고정 500ms TTL 스윕이 비행 중인 모든 Ghost(공격자 예측·관전용)를 0.5초에 중도 소멸 — `ProjectileLifetime`(최대 5s)과 충돌 | per-entry 만료(`Lifetime + 1s`)로 전환. 스윕은 stale 엔트리 GC 역할만 수행 |
| 2 | Ghost 키 `(PredictionKeyID, SpawnIndex)`가 전역 고유 아님 — 두 플레이어 간 키 충돌, 리슨 호스트/NPC 발사는 항상 키=0이라 연사 즉시 충돌(덮어쓰기 → 엔티티·트레일 누수, 오파괴) | `FLNPGhostKey{InstigatorPlayerID, KeyOrSalvo, SpawnIndex}` + 예측 키 없는 발사는 서버 발급 `IssueServerSalvoID()`(≥65536, 스레드 세이프) |
| 3 | 리슨 호스트 공격자의 캐릭터 피격 임팩트 VFX 누락 — 호스트는 Ghost 미등록인데 큐 핸들러가 "공격자+Ghost 없음 → 브랜치 A가 처리했다고 가정 → no-op" | 핸들러를 단일 경로로 재작성: `DestroyGhost` + `ConsumeRecentLocalImpact`(로컬 재생 기록) 기반 중복 방지. 기록이 없으면 항상 서버 확정 위치 VFX 재생 |
| 4 | 패링 반사 시 공격자 화면 공백 — 공격자 클라가 (패링을 모른 채) 히트 오예측으로 Ghost를 이미 파괴 → `ReflectGhost` no-op → 반사 발사체가 안 보인 채 데미지만 수신. 관전자도 속도만 교체돼 위치 오차 유지 | 반사를 **"구 Ghost 소멸 + 새 Ghost 스폰"**으로 재설계 (아래 참조) |

**패링 반사 재설계 — 소멸+재스폰 (스폰 경로 일원화):**
- **서버:** 권위 엔티티는 파괴하지 않고 기존처럼 in-place로 속도·진영을 반전하되, 식별자를 재발급한다 — `PredictionKeyID ← IssueServerSalvoID()`, `InstigatorPlayerID ← 방어자 PlayerId`, `CachedRewindSeconds ← 방어자 RTT/2` (반사 후 Lag Comp 기준은 방어자).
- **클라이언트:** `Multicast_RespawnReflectedGhost`(Reliable)가 구 키 Ghost를 파괴(없어도 no-op)하고, 서버 확정 반사 지점·속도·잔여 수명으로 새 Ghost를 스폰한다. 스폰은 발사 방송과 동일한 `ULNPGhostProjectileSubsystem::SpawnSpectatorGhosts` 공용 경로를 타므로 Dead Reckoning이 자동 적용된다.
- **RPC 래퍼는 발사용/반사용 2개 유지:** 발사 방송은 "쏜 본인 스킵"(이미 예측 Ghost 보유), 반사는 아무도 예측하지 않았으므로 방어자 본인 포함 전원 스폰(서버/호스트만 스킵) — 스킵 규칙이 달라 엔티티 스폰 계층만 공용화했다.

**관전용 Ghost Dead Reckoning (스폰 위치·시간 RTT 보정):**
`SpawnSpectatorGhosts`가 `UpstreamDelaySeconds`(서버 발신 전 이미 흐른 지연 — 발사 방송은 공격자 RTT/2, 반사 방송은 0) + 수신자 자신의 RTT/2를 합산·클램프(200ms)해 `SpawnPos + Velocity × 지연`으로 외삽 스폰하고 잔여 수명도 그만큼 차감한다. 외삽 시점에 수명이 다한 발사체는 스폰을 생략한다. 순간이동 없는 catch-up 가속 수렴은 후속 튜닝 후보로 남김(외삽만으로 부족하다고 체감되면 도입).

**함께 적용한 개선:**

| 항목 | 내용 |
|:---|:---|
| 방송 Reliable 전환 | 스폰·반사 멀티캐스트는 발사당 1회뿐인 "존재 결정" 이벤트 — 유실 시 관전자가 발사체를 통째로 못 보므로 Reliable로 승격. 기준: *지속 위치 복제는 Unreliable, 존재를 결정하는 1회성 이벤트는 Reliable* |
| 관전용 Ghost 로컬 충돌 | 클라 예측 판정의 `bIsLocalInstigator` 게이트 제거 — 관전용 Ghost도 로컬에서 캐릭터 충돌 시 즉시 소멸+VFX ("관통 후 뒤늦게 터지는" 잔상 제거). 발사자 자신 제외는 Instigator 핸들(예측 Ghost)·`InstigatorPlayerID`(관전용) 이중으로 처리. 서버 확정 큐와의 VFX 중복은 로컬 재생 기록으로 방지 |
| 클라 타겟 Actor 미링크 허용 | 클라 예측 타겟 수집에서 `!Target.Actor → continue` 제거 — Phase 6 bubble로 수신한 Enemy 엔티티는 클라에서 Actor 미링크(nullptr)라 원거리 적 전체가 예측 대상에서 빠지던 공백 해소. Actor는 발사자 제외 비교에만 사용 |
| Lag Comp Rewind 발사 시점 캐싱 | §9 Phase 4의 "매 프레임 재계산" 트레이드오프 항목을 채택 이행 — `FLNPProjectileFragment::CachedRewindSeconds`에 발사(또는 반사) 시점 공격자 RTT/2를 1회 저장, 비행 내내 재사용. 느린/유도 발사체가 대상의 과거 잔상을 쫓는 문제와 매 프레임 Ping 조회 비용 동시 제거 |
| 클라 예측 브랜치 A의 임팩트 VFX 재생 | 기존 코드는 Ghost 파괴만 하고 VFX를 재생하지 않아(§5.2 표의 "브랜치 A에서 이미 재생"과 불일치) 공격자가 예측 승리 시 임팩트 VFX가 없었음 — 로컬 충돌 경로에 `EnqueueImpact` 추가로 해소 |

**변경 파일:** `LNPGhostProjectileSubsystem.h/.cpp`(전면 개정 — `FLNPGhostKey`, per-entry 만료, 로컬 임팩트 기록, `SpawnSpectatorGhosts` 공용화, `IssueServerSalvoID`), `LNPCharacterBase.h/.cpp`(`Multicast_SpawnGhostProjectiles` Reliable+지연 파라미터, `Multicast_ReflectGhostProjectile` → `Multicast_RespawnReflectedGhost`), `LNPAbility_RangedAttack.cpp`(KeyOrSalvo·Rewind 캐싱·업스트림 지연), `LNPProjectileProcessors.cpp`(클라 분기 재작성, Pass 3 반사 재설계·Rewind 캐싱), `LNPHitDetectionShared.h`(`FLNPProjectileParryCommand` 재스폰 방송화), `LNPProjectileMassTypes.h`(`CachedRewindSeconds`), `LNPGameplayCueNotify_ProjectileImpact.cpp`(단일 경로 재작성)

**미착수 — 후속 후보:**
- **발사체 방송 채널 분리:** 현재 스폰 방송이 공격자 캐릭터 Actor의 Multicast RPC라, 향후 NPC가 Low LOD(Actor 없음) 상태에서 원거리 공격하면 방송 채널이 없다. NPC 원거리 도입 시 영속 복제 액터(GameState 산하 또는 Bubble류)로 이전 — SalvoID 체계는 이번에 선반영 완료.
- **관전용 Ghost catch-up 수렴:** 외삽 스폰의 총구 이펙트 단절이 체감되면 "총구에서 스폰 후 짧은 가속으로 외삽 위치에 수렴" 방식 도입.
- **지형 임팩트:** 관전용 Ghost가 로컬 지형 판정으로 자체 임팩트를 재생하므로 Phase 4의 "관전자는 지형 임팩트를 못 본다" 한계는 B안 도입으로 사실상 해소 — 서버 위치와의 미세 오차만 남음(코스메틱 수용).
- **발사 각도(피치) 동기화 — 에임 오프셋 동기화와 묶어서 진행 (2026-07-04 PIE에서 확인):** 원격 클라이언트의 발사 피치가 서버에 전달되지 않아, 서버 권위 엔티티(및 그로부터 방송되는 관전 Ghost)가 항상 캐릭터 수평으로 발사된다 — 공격자 본인 화면(로컬 카메라 기준 예측 Ghost)만 정확한 각도. 서버의 `GetFireDirections`가 원격 PC의 `GetPlayerViewPoint`에 의존하는데 원격 플레이어의 카메라 피치가 서버에 없기 때문. 캐릭터 상체 자세(Aim Offset) 동기화 작업과 같은 데이터(시선 피치 복제)를 쓰므로 한 작업으로 묶는다.

**1차 PIE 검증 결과 (2026-07-04, 저지연 PktLag=0):**
- ✅ 발사체 조기 소멸(0.5s TTL) 완전 해결 — 연사에서도 잔상·조기 소멸 없음
- ✅ NPC 원거리 공격 패링 + 반전 발사체 가시성 정상 (호스트·클라 양쪽 화면)
- ✅ 1P(호스트) 발사체가 2P·3P 관전 화면에 정상 표시. 넉백 방향 근접·원거리 정상
- ✅ **2P↔3P(클라↔클라) 관전 Ghost 상호 미표시 — 해소** — `[GhostTrace]` 3인 로그(2026-07-04)로 RPC 전달·관전자 스폰·Dead Reckoning(≈184ms 외삽) 모두 정상임을 확인 (방송 채널 이전 C-7 불필요). 3인 기능 검증(2026-07-05)에서 관전 화면의 발사체 표시·피격자 몸에서의 소멸(관통 해소)까지 확인. 당초 "안 보임" 보고는 고핑+근거리에서 관전 Ghost 수명이 수 프레임에 불과했던 지각 문제로 결론
- ⚠️ 3P 관전 화면에서 1P 발사체가 2P를 관통해 보임 — 관전 Ghost 로컬 충돌이 동작하지 않는 정황. 클라이언트에서 플레이어 엔티티 Transform 갱신 여부를 `[GhostTrace][Targets]`(엔티티↔Actor 위치 괴리)로 확인 예정. 테스트 PC 성능 저하로 인한 지연 가능성도 병기
- ❌ (별개 버그) 원격 클라이언트 근접 콤보 미진행 — `CancelCurrentAttackAbility`가 서버 전용 `GrantedAbilities`에만 의존해 클라에서 no-op → 이전 어빌리티가 안 끝나 재활성화 실패. 클래스 탐색 폴백 추가 + `Server_SetComboIndex` RPC로 서버 몽타주 섹션 동기화 (수정 완료, 재검증 대기)
- ❌ (별개 버그) 클라이언트 화면에서 Enemy Actor의 AnimSourceMesh 노출 — 숨김 처리가 서버 전용 `SyncFromEntity` 경로에만 있었음. `ALNPEnemyCharacter::BeginPlay`에서 공통 숨김 (수정 완료, 재검증 대기)
- ✅ 근접 예측 HitStop(원격 클라 자기 화면) — 원인은 클라 월드의 플레이어 Mass 엔티티 부재(`players=0`)였고 **Phase 6.5로 해결·검증 완료** (2026-07-05)
- ✅ **(해소) `ProjectileLifetime=5000`은 의도된 설계** — 플레이 공간이 구(Sphere) 내벽이라 직선 비행은 기하학적으로 반드시 지형에 충돌하며, 실제 비행 시간은 (지름+오차)÷탄속으로 바운드된다. 5000은 "지형 충돌까지 유지"를 뜻하는 도달 불가능한 상한. 이에 따른 유일한 실질 문제였던 **클라 Ghost 레지스트리 stale 엔트리 누적**(지형 사망은 임팩트 큐가 없어 만료 시각까지 맵에 잔류)은 `SweepExpiredGhosts`가 무효 엔티티 엔트리를 만료와 무관하게 즉시 GC하도록 수정해 해소. 잔여 참고: 수명의 "안전망" 기능(Guided 선회·SurfaceCache 조회 실패 등 병리 케이스)이 사실상 무장해제되므로, 필요 시 "지름÷최저 탄속 × 여유"(30~60s) 수준으로 낮추면 정상 비행 영향 없이 안전망이 복원된다 (선택 사항)
- ✅ **(간극 우려 해소) 클라 Enemy 엔티티가 히트 쿼리 요구 충족 실증** — 3인 로그에서 `[Targets] enemies=1` 확인: bubble 스폰 클라 엔티티가 `FLNPEnemySharedFragment(Config)` 포함 EnemyQuery에 정상 매칭됨. 직전 세션의 `enemies=0`은 아키타입 간극이 아니라 "캡처 시점에 주변에 적이 없었음". `actor:0`(Actor 링크 부재)만 확인 대상으로 남음 — 적 High LOD 액터 근처에서 재측정

### Phase 5 — 무기 교체 서버 권한화 ✅ 완료 (Phase 1과 동시 수행)

> 전제: Phase 1 완료. Phase 3·4와 독립적 — 병렬 진행 가능.

| 작업 | 실제 변경 파일 | 결과 |
|:---|:---|:---|
| `Server_EquipWeapon()` RPC 추가 | `LNPPlayerCharacter.h/.cpp` (CharacterBase 아님) | ✅ 클라이언트 무기 장착 → 서버 GAS 부여 정상 동작 |
| `EquippedWeaponData` 복제 등록 | `LNPCharacterBase.h/.cpp` — **DOREPLIFETIME 임시 사용** (Iris Descriptor 미구현) | ✅ 무기 비주얼 전 클라이언트 동기화 확인 |
| `OnRep_CurrentWeapon()` 구현 | `LNPCharacterBase.cpp` | ✅ `ALNPCharacterBase::EquipWeapon` 직접 호출로 RPC 루프 방지 |

**구현 세부사항:**
- `Server_EquipWeapon_Implementation`: `ALNPCharacterBase::EquipWeapon` (비주얼 + `EquippedWeaponData` 마킹) + `EqComp->EquipWeapon` (GAS 부여) 순서로 실행
- `OnRep_CurrentWeapon`: virtual dispatch 우회(`ALNPCharacterBase::EquipWeapon` 직접 호출)로 비주얼·태그만 갱신 — `Server_EquipWeapon` 재호출 루프 방지
- `ALNPPlayerCharacter::PossessedBy`: `EqComp->GetWeaponSlot().Definition`으로 DefaultWeapon 감지 후 초기 비주얼 동기화
- `EquippedWeaponData`의 Iris 네이티브 경로(`RegisterReplicationFragments`) 마이그레이션은 Phase 6 착수 전 수행 권장

### Phase 6 — Enemy MassReplication + Actor 복제 ✅ 완료 (2026-07-05, PIE 2·3인 검증)

> **검증 결과:** 2P 화면에서 원거리 Enemy는 ISMC(Low LOD), 근거리는 복제 Actor(High LOD)로 정상 표시 — 이중 표시 없음.
> 클라 bubble 엔티티가 히트 쿼리 요구(SharedFragment 포함)를 충족함을 실측(`enemies=1(actor:1)`), Enemy 복제 Actor의
> 퍼펫 링크(Actor↔엔티티)도 자동 성립. NPC와의 근접·원거리 전투, NPC 발사체 패링·반사까지 클라이언트에서 정상 동작.
> 리스크 A(레거시 호환 경로 동작)도 이로써 해소 — 수백 엔티티 프로파일링만 잔여.

| 작업 | 변경 파일 | 결과 |
|:---|:---|:---|
| 복제 스키마·FastArray·BubbleHandler·Serializer·BubbleInfo | `LNPEnemyReplication.h/.cpp` (신규) | ✅ `FLNPReplicatedEnemyAgent`(PositionYaw + EnemyTypeTag), PostReplicatedAdd/Change 구현. 클라 엔티티 생성 확인 |
| Replicator (PositionYaw 지속 갱신) | `LNPEnemyReplicator.h/.cpp` (신규) | ✅ 서버 AI가 매 틱 이동시키므로 갱신마다 위치/Yaw 반영 |
| Bubble 등록 + 클라 템플릿 warm-up | `LNPMassSpawnSubsystem.cpp` | ✅ `RegisterBubbleInfoClass` + Enemy Config `GetOrCreateEntityTemplate` (클라는 스폰 기회가 없어 TemplateID 등록이 선행돼야 함) |
| 복제 트레잇 연결 | `LNPEnemyMassTypes.cpp` (`ULNPEnemyTrait`가 `UMassReplicationTrait` 내장 위임) | ✅ NetworkID 발급은 엔진 옵저버(`UMassNetworkIDFragmentInitializer`)가 자동 수행 — 별도 발급 로직 불필요했음 |
| `ALNPEnemyCharacter` Actor 복제 설정 | `LNPEnemyCharacter.cpp` | ✅ `bReplicates`(Phase 1) + `SetNetUpdateFrequency(30)`. Health는 AttributeSet DOREPLIFETIME으로 HP Bar 갱신 확인 — Iris Descriptor 마이그레이션·`bIsDead` 명시 복제는 잔여 항목(문서 상단 진행 현황 참조) |
| 공격 몽타주 Multicast RPC | (미구현) | ✅ **불필요로 판명** — High LOD Actor의 ASC 몽타주 복제(GAS 내장)로 클라이언트 재생이 충족됨을 PIE 전투 검증에서 확인. 몽타주 유실·타이밍 문제가 관측되면 계획대로 Reliable Multicast 도입 재검토 |
| 게임 로직 Enemy 프로세서 `NM_Client` 조기 반환 | 각 Enemy 프로세서 (9곳) | ✅ 클라이언트에서 AI 프로세서 미실행 |

### Phase 6.5 — Player MassReplication (퍼펫 링크) — ✅ 완료 (2026-07-05, PIE 2인 핵심 지표 검증)

> **인프라 검증:** `players=2(actor:2)`, 양측 이동 중 `maxPlayerEntityActorGap=0cm`(클라 퍼펫 Transform이 캡슐 동기화
> 트레잇으로 프레임 단위 갱신됨 — 별도 클라 동기화 프로세서 불필요), 자기 폰·상대 폰 모두 `isPuppet=1` + `actorLinked=1`.
>
> **기능 검증 (2026-07-05, PIE 2·3인):**
> - ✅ 근접 예측 HitStop — 1P↔2P 양방향, 공격자 자기 화면 포함 모두 정상 (세션 내내 미해결이던 항목 최종 해소)
> - ✅ Enemy AnimSourceMesh 클라 노출 해소 (`bHiddenInGame` 방식)
> - ✅ 3P 관전 화면 관통 해소 — 발사체가 피격자 몸에서 정상 소멸 (관전 Ghost 로컬 충돌 동작)
> - ✅ 패링 반사(소멸+재스폰, Phase 4.5 A-4)가 원래 공격자(1P)·방어자(2P) 화면 모두 표시 — 재설계 목표 실증
> - 임시 진단 로그(`[PlayerRepTrace]`/`[GhostTrace]`) 전량 제거. 영구 유지: warm-up 실패·NetID resolve 실패 Warning 2종
> - 잔여(선택): 고지연(`net.PktLag=100+`) 재검증, 클라간(2P→3P) 발사 시각 확인(스폰·충돌은 로그로 기입증)

> 전제: Phase 6 bubble 인프라. 목적: 클라이언트 월드에 플레이어 Mass 엔티티가 없어(PIE 실측 `players=0`)
> 클라 예측 판정(근접 HitStop·관전 Ghost 로컬 충돌)이 동작하지 않는 문제의 근본 해결.

**설계 결정 — 엔진 정식 퍼펫(Puppet) 경로 채택:**
`UMassAgentComponent`는 복제되는 `NetID`(FMassNetworkID)와 퍼펫 상태 머신(`PuppetPendingReplication` →
`SetReplicatedPuppetHandle`, 액터·엔티티 도착 순서 레이스는 `PuppetReplicatedOrphan`으로 엔진이 처리)을
내장한다. 플레이어 엔티티를 MassReplication에 등록하면 클라이언트에서 "bubble이 스폰한 엔티티 ↔ 복제된
폰 액터" 연결이 자동으로 성립한다. 현재 `players=0`의 정체는 이 컴포넌트가 NetID를 영원히 기다리다
orphan으로 남는 것 — 퍼펫 핸드셰이크를 실제로 완료시켜주는 작업이다.
(대안이던 "클라 액터 스냅샷 캐시"와 "수동 엔티티 스폰+링크"는 기각 — 전자는 중간 산물, 후자는 엔진
퍼펫 플로우의 수동 재구현. 프로젝트 취지상 엔진 정석 경로로 학습·구현한다.)

**중복 복제 회피 — 스키마는 "존재만":**
플레이어 위치는 이미 Mover가 전 클라이언트에 고빈도·예측·보간으로 복제 중이다. bubble 스키마에 위치를
넣으면 같은 데이터의 저품질 사본이 한 번 더 회선에 실리고, 클라 히트 예측이 따라가야 할 소스는 어차피
보간된 폰 액터라 소비처도 없다. 따라서 **bubble은 존재+식별(NetID, PlayerId)만 1회성으로 복제**하고,
클라 퍼펫 엔티티의 Transform은 서버와 동일한 액터→엔티티 translator(trait)가 매 프레임 채운다
(LootPod Phase 7의 1회성 복제와 같은 패턴).

| 작업 | 상태 | 내용 |
|:---|:---|:---|
| Player 복제 트리오 | ✅ | `Character/LNPPlayerReplication.h/.cpp` — `FLNPReplicatedPlayerAgent`(스폰 시점 PositionYaw 1회) / FastArrayItem / BubbleHandler / Serializer / `ALNPPlayerClientBubbleInfo`. `PostReplicatedChange`는 no-op (스폰 후 갱신 없음) |
| Replicator | ✅ | `Character/LNPPlayerReplicator.h/.cpp` — Add 시 PositionYaw 1회, `ModifyEntityCallback`은 no-op (위치는 Mover 채널 담당) |
| Bubble 등록 | ✅ | `LNPMassSpawnSubsystem::Initialize`에 `RegisterBubbleInfoClass(ALNPPlayerClientBubbleInfo)` 추가 |
| 클라 템플릿 warm-up | ✅ | Player TemplateID는 폰 BP에 저장된 `MassAgentComponent::EntityConfig` 구조체 GUID에서 파생 — DA 에셋이 아니라 **폰 CDO의 컴포넌트 기준**으로 `GetOrCreateEntityTemplate` 호출. 폰 클래스는 신규 `ULNPSettings::PlayerPawnClass`(DefaultGame.ini)로 참조 |
| 에디터 잔여 | ⏳ | `DA_PlayerEntityConfig`에 **Mass Replication 트레잇 추가** — BubbleInfoClass=`ALNPPlayerClientBubbleInfo`, ReplicatorClass=`ULNPPlayerReplicator`, LOD Distance 대폭 확대(기본 Off=50m는 플레이어에게 부적합 — 예: Medium=500m, Low=1km, Off=10km) |
| 퍼펫 링크 검증 | ⏳ | 클라에서 `MassAgentComponent` NetID 수신 → 엔티티 자동 연결, 접속/리스폰/재빙의 시나리오 |
| 클라 Transform 경로 검증 | ⏳ | 액터→엔티티 translator가 클라 퍼펫에서도 실행되는지 — `[GhostTrace][Targets]`의 `maxPlayerEntityActorGap`으로 측정 (크면 클라 전용 동기화 후속 필요) |
| Enemy 클라 아키타입 확인 | ⏳ | PIE 실측 `enemies=0`이 "주변에 적이 없었음"인지 "클라 아키타입이 히트 쿼리 요구를 못 채움"인지 — 적 근처 전투 중 `[Targets]` 로그로 판정. 참고: Enemy 복제 액터도 base의 `MassAgentComponent`를 가지므로 Phase 6.5와 동일한 퍼펫 링크가 자동 성립할 것으로 예상 |
| 완료 기준 | ⏳ | 클라 `[GhostTrace][Targets]`에서 players=접속 인원, 근접 예측 HitStop(원격 클라 자기 화면) 동작, 3P 관전 화면 관통 해소 |

**🔍 발견·수정한 엔진 타이밍 갭 — 에이전트 경로의 NetID 캐싱 (2026-07-05, `[PlayerRepTrace]` 단계별 진단으로 특정):**
파이프라인 전 구간(트레잇 반영 → 서버 엔티티 NetID → bubble AddAgent → 클라 수신·스폰·아키타입 5/5 충족)이 정상인데도
클라이언트 컴포넌트가 `netIDValid=0 pendingRep=1`로 영구 대기하는 현상. 원인: 엔진 `UMassAgentComponent::SetEntityHandleInternal`은
핸들 설정 순간 엔티티의 `FMassNetworkIDFragment`를 읽어 복제 프로퍼티 NetID에 **1회만** 캐싱하는데, **에이전트 경로(컴포넌트 등록이
엔티티를 생성하는 플레이어 폰)는 그 시점에 NetworkID 초기화 옵저버가 아직 실행되기 전**이라 무효값이 캐싱되고 갱신 경로가 없다.
Enemy(스포너가 엔티티 먼저 생성 → 액터 나중 링크)는 링크 시점에 프래그먼트가 이미 채워져 있어 정상 동작 — Enemy만 퍼펫이 성립하던
비대칭(`enemies actor:1` vs `players actor:0`)의 원인. **수정:** `ULNPMassAgentComponent`(신규 서브클래스)가
`SetEntityHandleInternal` 오버라이드에서 서버·핸들 유효·NetID 무효 조합일 때 옵저버 실행 이후 재조회(0.1s 간격, 최대 10회)로
NetID를 채운다. `ALNPCharacterBase` 생성자에서 컴포넌트 클래스 교체. 엔진 퍼펫 플로우 자체는 무수정 유지.

**퍼펫 링크 시 엔진 동작 메모 (구현 중 확인, `MassAgentComponent.cpp`):**
- 서버: 에이전트 엔티티 생성 시(`SetEntityHandleInternal`) 엔티티의 `FMassNetworkIDFragment`를 읽어 컴포넌트 `NetID`(복제 프로퍼티)에 저장. NetID 발급은 `UMassNetworkIDFragmentInitializer`(Add 옵저버, 서버 전용)가 수행.
- 클라: `OnRep_NetID` → `NotifyMassAgentComponentReplicated` → bubble이 스폰한 엔티티를 `FindEntity(NetID)`로 조회해 연결. 엔티티가 아직 없으면 `PuppetReplicatedOrphan`으로 대기했다가 bubble 도착 시 연결 (레이스 엔진 처리).
- **주의:** 퍼펫 링크 시 엔진이 엔티티 Transform으로 폰 위치를 초기화한다 — bubble 스키마에 스폰 위치를 1회 실어야 하는 이유 (비워두면 폰이 원점으로 텔레포트).
- 퍼펫 초기화가 EntityConfig 템플릿 조성(composition)을 엔티티에 더해주고 `FMassActorFragment`도 폰으로 링크하므로, 서버와 동일한 아키타입(FLNPPlayerTag·ParryState·PositionHistory 포함)이 클라에서 성립한다.

### Phase 7 — LootPod MassReplication + Actor 복제

> 전제: Phase 6 완료 (동일한 이중 복제 패턴 참조).

| 작업 | 변경 파일 | 완료 기준 |
|:---|:---|:---|
| `FLNPLootPodReplicationData` 구조체 및 BubbleHandler 구현 | `LNPLootPodReplication.h/.cpp` (신규) | 클라이언트에 LootPod 엔티티 생성 확인 |
| `ALNPLootPod` `bReplicates=true`, `PodState`·`CurrentGaugePercent` Iris Descriptor 등록 | `LNPLootPod.h/.cpp` | 클라이언트에서 LootPod VFX 색상·게이지 동기화 |
| `ULNPIdleToLootingProcessor`, `ULNPLootingProcessor`에 `NM_Client` 조기 반환 추가 | `LNPLootPodProcessors.cpp` | LootPod 상태 전환이 서버에서만 발생 |

---

## 10. 리스크 및 주의사항

### 리스크 A: MassReplication 레거시 호환 경로 동작 검증 ✅ 기능 해소 (Phase 6 검증 완료) — 프로파일링 잔여

`MassReplication` 플러그인은 `IrisCore` 의존성이 없어 Iris 활성화 시 레거시 호환 경로로 동작한다. **PIE 2·3인 검증(2026-07-05)에서 Iris ON 상태로 Enemy·Player bubble 모두 정상 수신·스폰됨을 확인** — 폴백 플랜은 불필요해져 폐기. 잔여: 수십 → 수백 개 엔티티 점진 증가 대역폭·CPU 프로파일링 (문서 상단 진행 현황 참조).

**Projectile Mass 엔티티 동기화 — 종결:** Spawn-only 방송 + 클라이언트 독립 시뮬레이션(B안)으로 Phase 4에서 구현·확정, Phase 4.5에서 Reliable 승격·Dead Reckoning·전역 고유 키로 보강 완료 (§5.2 참조).

### 리스크 B: 구형 중력 + Mover 예측 불일치 ✅ 해소 (Phase 1 검증 완료)

`LastUpDir` 누산값이 Mover SyncState에 미포함 시 롤백 때 Up 벡터가 순간 틀어진다. **PIE 2플레이어 검증 결과:** 1P 화면에서 2P가 적도 부근까지 구면 이동 시 Up 벡터가 항상 구 중심을 올바르게 향하는 것을 확인. `CurvatureDelta` 누산이 Mover SyncState에 포함되어 정상 복제되고 있으며 추가 대응 불필요.

### 리스크 C: 클라-서버 히트 감지 타이밍 차이

클라이언트와 서버가 독립적으로 히트를 감지하므로, 고지연 환경에서는 HitStop 후 데미지 적용이 눈에 띄게 늦어 보일 수 있다. 코스메틱 단락 범위이므로 플레이테스트로 허용 가능 여부를 판단한다.

### 리스크 D: 클라이언트 히트 오탐(False Positive)

클라이언트 적 Actor 위치는 복제 지연(최대 RTT/2)이 있어 서버와 다른 히트 결과가 나올 수 있다. 코스메틱에만 영향을 주므로 게임플레이 무결성 문제는 없다. HitStop 한 번 오재생 정도로 수용한다.

