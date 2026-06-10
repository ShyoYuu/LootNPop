# HitDetection 시스템 기술 설계

> **구현 상태:** **근거리·원거리 판정 및 패링 연계 구현 완료.** 공간 쿼리 최적화는 미구현.

---

## 1. 설계 원칙

- **No Physics Collision:** Chaos 엔진의 콜리전 이벤트 미사용. 물리 엔진을 거치지 않으므로 수천 개의 엔티티에서도 일정한 성능 유지.
- **Mass Processor 집중:** 모든 판정은 `FMassProcessor::Execute()` 내에서 수학적으로 일괄 계산.
- **서버 권위:** 판정 결과는 서버 프로세서에서만 최종 결정. 클라이언트는 VFX/HitStop만 처리.

---

## 2. 근거리 판정 (Melee — Swept Volume)

### 2.1 데이터 구조

**`FLNPWeaponTraceFragment`** (`HitDetection/LNPWeaponTraceMassTypes.h`)
```cpp
FVector SwordTipPrev;    // 이전 프레임 칼끝 위치
FVector SwordTipCurr;    // 현재 프레임 칼끝 위치
FVector SwordRootPrev;   // 이전 프레임 칼밑 위치
FVector SwordRootCurr;   // 현재 프레임 칼밑 위치
float   HitRadius   = 10.f;  // 칼날 피격 판정 반경
float   ParryRadius = 12.f;  // 칼날 패링 판정 반경 (HitRadius보다 크게 설정)
float   Damage      = 0.f;
float   TimeToLive  = 0.f;   // NotifyEnd 미호출 시 자동 파괴 안전장치
UClass* DamageEffectClass;   // UPROPERTY() 있음
FMassEntityHandle InstigatorEntity;
ELNPInstigatorTeam InstigatorTeam = ELNPInstigatorTeam::Player;
// 중복 피격 방지 (공격 한 번에 최대 8개 타겟까지 기록)
static constexpr int32 MaxAlreadyHit = 8;
FMassEntityHandle AlreadyHit[MaxAlreadyHit];
int32             AlreadyHitCount = 0;
```

**`FLNPMeleeActiveTag`** — 현재 미사용. `FMassCommandBuildEntity` + `FMassCommandAddTag`를 동일 배치에서 디퍼드하면 아키타입 전환 타이밍 문제로 쿼리가 엔티티를 인식하지 못함. Fragment 존재 자체가 활성 윈도우를 의미하는 것으로 대체.

### 2.2 판정 알고리즘

이전/현재 프레임 칼날이 쓸고 지나간 Quad면(Swept Area)과 타겟 **캡슐 축 선분** 사이의 최단 거리를 계산.

```
hit 조건: min_dist(SweptQuad, CapsuleSegment) <= SwordRadius + CapsuleRadius
```

**Quad → 삼각형 2개 분해:**
- T1 = `{Prev_Root, Curr_Root, Curr_Tip}`
- T2 = `{Prev_Root, Curr_Tip, Prev_Tip}`

**삼각형 vs 선분 최단 거리 (`TriangleSegmentDistSq`):**
1. Möller–Trumbore 알고리즘으로 선분이 삼각형 내부를 관통하면 → 거리 = 0
2. 관통하지 않으면: min( 선분 끝점 → 삼각형, 삼각형 3변 → 선분 ) = 5회 비교

**캡슐 축 선분:**
```
CylHalfLen = CapsuleHalfHeight - CapsuleRadius
CapsuleBot = CapsuleCenter - UpDir * CylHalfLen   // 하단 구체 중심
CapsuleTop = CapsuleCenter + UpDir * CylHalfLen   // 상단 구체 중심
UpDir = (-EntityLocation).GetSafeNormal()          // 구형 내벽 세계: 머리 방향 = 구 중심 방향
```

**첫 프레임 축퇴(degenerate) 처리:**
- `SwordTipPrev == SwordTipCurr` (Prev==Curr)이면 Quad가 축퇴됨
- 이 경우 `SegmentDistToSegment(현재 칼날, CapsuleSegment)`로 직접 계산

### 2.3 구현 파일

| 파일 | 역할 |
|:---|:---|
| `HitDetection/LNPWeaponTraceMassTypes.h` | `FLNPWeaponTraceFragment` + `FLNPMeleeActiveTag`(미사용) 정의 |
| `GAS/Abilities/LNPAbility_MeleeAttack.h/.cpp` | `CommitAbility` → `AttackMontage` 재생 → 즉시 `EndAbility`. 쿨다운 `FireCooldown` per-spec 주입 |
| `Animation/ANS_LNPMeleeHitWindow.h/.cpp` | AnimNotifyState. `NotifyBegin`: Mass 엔티티 생성(Deferred). `NotifyTick`: 무기 메시 본 위치 읽어 Fragment 갱신. `NotifyEnd`: 엔티티 파괴(Deferred) |
| `HitDetection/LNPWeaponTraceProcessors.h/.cpp` | `ULNPWeaponTraceHitDetectionProcessor` (StartPhysics), `ULNPWeaponTraceLifetimeProcessor` (TimeToLive 만료 엔티티 파괴), 에디터 전용 `ULNPWeaponTraceDebugDrawProcessor` |

### 2.4 본 위치 업데이트

`UANS_LNPMeleeHitWindow`가 매 프레임 무기 메시(`GetWeaponMesh()`)의 지정 본 위치를 읽어 `FLNPWeaponTraceFragment`의 Prev/Curr 필드를 갱신한다.

- `BoneTipName` (기본값 `"sword_tip"`): 칼끝 본
- `BoneRootName` (기본값 `"sword_root"`): 칼밑 본
- `HitRadiusOverride`: 0이면 `WeaponData->ProjectileHitRadius` 사용
- `ParryRadiusOverride`: 0이면 기본값 사용

엔티티는 `NotifyBegin`에서 `FMassCommandBuildEntity`로 생성(Deferred), `NotifyEnd`에서 `FMassCommandDestroyEntities`로 파괴(Deferred). 엔티티가 활성화되기 전인 첫 Tick은 `IsEntityActive()` 확인 후 스킵.

---

## 3. 원거리 판정 (Projectile — Line Segment)

### 3.1 데이터 구조

**`FLNPProjectileFragment`** (엔티티별 시뮬레이션 상태)
```cpp
FVector PreviousPos;       // 이전 프레임 위치 — 스윕 선분 시작점
// CurrentPos는 별도 필드 없음. Entity Transform이 현재 위치를 담당.
FVector Velocity;
FVector SpawnLocation;     // 스폰 위치 (VFX 스폰 이펙트에 한 번 사용)
float   LifetimeRemaining = 5.0f;
FMassEntityHandle  Instigator;
ELNPInstigatorTeam InstigatorTeam;  // 패링 반사 시 Player로 전환
```

**`FLNPProjectileSharedFragment`** (투사체 종류별 공유 상수 — `FMassConstSharedFragment`)
```cpp
TObjectPtr<ULNPVFXData>      VFXData;          // Niagara 에셋 참조
TSubclassOf<UGameplayEffect> DamageEffectClass;
ELNPProjectileType           Type;             // Linear, Guided, Lobbed
float Damage      = 10.0f;
float HitRadius   =  5.0f;  // 피격 판정 반경
float ParryRadius =  6.0f;  // 패링 판정 반경 (HitRadius보다 크게 설정)
```

### 3.2 판정 알고리즘

`PreviousPos → CurrentPos(Transform)` 선분과 타겟 중심점 사이의 최단 거리를 계산.

```
// 1단계: 패링 체크 (우선)
if bIsParrying && Dot >= ParryAngleCos && Distance <= ParryRadius → 패링 처리
// 2단계: 피격 체크
if Distance <= HitRadius → 가드 또는 데미지 처리

Distance = DistancePointToSegment(TargetCenter, PreviousPos, CurrentPos)
// CurrentPos는 Fragment 필드 아님 — Transform.GetLocation()에서 읽음
```

### 3.3 투사체 이동 업데이트

**`ULNPProjectileMovementProcessor`** — 구현 완료 (Linear 타입)

| 타입 | 이동 방식 | 구현 여부 |
|:---|:---|:---|
| `Linear` | `PreviousPos = CurrentPos; Transform += Velocity × DeltaTime` | ✅ |
| `Guided` | `Velocity = Lerp(Velocity, (TargetPos - CurrentPos).Norm × Speed, TurnRate × DeltaTime)` | 🔲 미구현 |
| `Lobbed` | 포물선 함수 (시간 매개변수 t 기반) | 🔲 미구현 |

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
- Actor 상태: `FLNPApplyDamageGECommand`에서 피격자·공격자 양쪽에 `ALNPCharacterBase::ApplyHitStop(0.08f)` 호출.
- Mass 상태: `FLNPExecutionSpeedFragment` 배율 조정 방식은 미구현.

### 5.3 VFX / SFX

Mass 비주얼화 도구(`UMassVisualizationComponent`) 또는 전용 FX Pool을 통해 피격 이펙트 재생. 게임스레드에서 일괄 처리.

---

## 6. 패링과의 연계

HitDetection Processor가 판정을 내리기 전에 타겟의 `FLNPParryStateFragment` 활성 여부를 먼저 확인. 패링이 활성화되어 있고 방향 조건을 만족하면 피격 처리 대신 패링 처리로 전환. 근접·원거리 양쪽 모두 구현 완료. (→ [TechDesign_ParrySystem.md](TechDesign_ParrySystem.md))

---

## 7. 구현 순서 (권장)

1. `FLNPProjectileFragment` + `ULNPProjectileMovementProcessor` (Linear 타입만)
2. Line Segment 거리 계산 + 피격 이벤트 브로드캐스트
3. HitStop + 넉백 (GAS 인프라 구축 병행)
4. Melee Swept Volume 판정
5. 공간 쿼리 최적화 (엔티티 수 증가 후 필요 시)
6. Guided / Lobbed 투사체, 패링 연계
