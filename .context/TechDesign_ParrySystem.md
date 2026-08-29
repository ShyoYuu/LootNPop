# Guard / 패링 시스템 기술 설계

## 1. 한눈에 보기

Guard(막기)와 Parry(저스트 가드)를 **Mass Processor의 HitDetection 판정에 통합**한 시스템. 상태는 GAS 태그와 Mass Fragment에 이중으로 유지되고(미러), 판정은 Worker Thread에서, 후처리(GAS 이벤트·VFX)는 Game Thread 커맨드에서 실행된다.

| 역할 | 수행 위치 | 근거 |
|:---|:---|:---|
| 상태 보관 | `FLNPParryStateFragment` (Mass) | Worker Thread에서 락 없이 읽기 |
| 판정 | HitDetection Processor (Worker Thread) | Fragment 직접 접근, 캡슐 수집 데이터 재사용 |
| 후처리 (GAS 이벤트·VFX·방송) | BatchedCommand (Game Thread) | ASC 접근 필요 |
| 반사체 Fragment 변경 | Processor (Worker Thread) | ReadWrite Fragment 직접 수정 |

ASC 태그(`TAG_State_Guarding`/`TAG_State_ParryWindow`)는 GAS·애니메이션 호환용으로 유지하고, `FLNPParryStateFragment`가 Worker Thread용 **미러** 역할을 한다.

**구현 상태:** Guard·근접 Parry·투사체 Parry·반경 분리·RTT 보정 모두 완료 + PIE 검증. GameplayCue 에셋 연결만 에디터 잔여.

---

## 2. 데이터 구조

### 2.1 FLNPParryStateFragment

```cpp
// LNPGuardParryTypes.h
struct FLNPParryStateFragment : public FMassFragment
{
    bool   bIsParrying   = false;   // 패링 창 활성 (TAG_State_ParryWindow 미러)
    bool   bIsGuarding   = false;   // 가드 중 (TAG_State_Guarding 미러)
    float  ParryAngleCos = 0.707f;  // cos(45°)
    float  GuardAngleCos = 0.5f;    // cos(60°)
    double ParryWindowExpiryTime = -1.0;  // 서버 전용: RTT 역보정된 절대 만료 시각 (§6)
};
```

### 2.2 상태 갱신 (ULNPInputHandlerComponent)

```
OnGuardStarted()
├─ ASC: +TAG_State_Guarding, +TAG_State_ParryWindow
├─ Fragment: bIsGuarding = true, bIsParrying = true
├─ Server_SetGuardState(true) RPC — 서버 Fragment 갱신 + RTT 역보정 만료 시각 기록
└─ 로컬 타이머(ParryWindowDuration, 기본 0.15초) → 만료 시 ParryWindow만 해제

OnGuardReleased()
├─ ASC: -TAG_State_Guarding, -TAG_State_ParryWindow
├─ Fragment: bIsGuarding = false, bIsParrying = false
├─ Server_SetGuardState(false)
└─ 타이머 취소
```

### 2.3 GameplayTag

| 태그 | 설명 |
|:---|:---|
| `LNP.State.Guarding` / `LNP.State.ParryWindow` | 상태 태그 (InputHandler 관리) |
| `GameplayCue.LNP.Guard.Block` / `GameplayCue.LNP.Parry.Success` | VFX/SFX 큐 (에셋 연결 잔여) |
| `LNP.GameplayEvent.Parry.Success` | 방어자 GA_ParrySuccess 트리거 |
| ~~`LNP.GameplayEvent.Parry.Stagger`~~ | **제거됨 (2026-08-29).** 공격자 경직은 전용 이벤트가 아니라 경직도(`LNPPoise::ApplyParryBreak`)를 거쳐 `Stagger.Light`로 들어온다 (→ [TechDesign_Poise.md](TechDesign_Poise.md) §8) |

---

## 3. 판정 흐름 — 반경 분리 2단계

패링(`ParryRadius`)과 피격(`HitRadius`)은 **독립 반경**이며 패링이 더 크고 먼저 검사된다. 두 반경 사이 거리에서는 패링 조건을 충족할 때만 반응한다.

```
[Processor — Worker Thread]
  각도 사전 계산 (AttackerDir / IncomingDir → Dot)

  ── 1단계: 패링 체크 (ParryRadius) ─────────────────────────────
  bIsParrying && 창 미만료(§6) && Dot >= ParryAngleCos && dist <= ParryRadius
      ├─ 근접   → MarkHit + FLNPMeleeParryCommand
      └─ 투사체 → [Fragment 직접 변경 + 식별자 재발급 (§5)]
                 → FLNPProjectileParryCommand (DeadTag 없음 — 계속 비행)

  ── 2단계: 피격 체크 (HitRadius) — 패링 미발동 시에만 ───────────
  dist <= HitRadius
      ├─ bIsGuarding && Dot >= GuardAngleCos → FLNPGuardBlockCommand (+투사체 소멸)
      └─ 그 외 → FLNPApplyDamageGECommand (+투사체 소멸)
```

**근접 판정의 단일화:** Player 타겟 2단계 판정은 `JudgePlayerTarget` 람다 하나로 구현되어, Enemy→Player와 근접 PvP(아군 사격)가 항상 같은 로직을 탄다 (§7.3의 버그 재발 방지).

### 각도 판정

```
[근접]   AttackerDir = Normalize(칼날 중점 - VictimPos);  Dot = Dot(VictimFwd, AttackerDir)
[투사체] IncomingDir = Normalize(Proj.PreviousPos - CurrentPos);  Dot = Dot(VictimFwd, IncomingDir)

Dot >= cos(45°) → 패링 각도 충족  /  Dot >= cos(60°) → 가드 각도 충족
```

---

## 4. Command별 책임

| 커맨드 | 처리 내용 |
|:---|:---|
| `FLNPMeleeParryCommand` | 방어자: Parry.Success 큐 + GameplayEvent + Parrier 몽타주. 공격자: **경직도 대량 누적**(`LNPPoise::ApplyParryBreak`, 저항 미적용) + **넉백**(`ApplyKnockback` — Instant Effect, 방어자 반대 0.7 + Up 0.3 방향, 2000 cm/s — 구형 곡률 포물선). ⚠️ 공격자 몽타주를 직접 재생하지 않는다 — 경직 진입이 같은 프레임에 잡혀 `Montage_Stop`으로 끊기므로, 연출은 `Value.Stagger.Parried` 밸류 태그로 경직 시스템이 낸다 (→ [TechDesign_Poise.md](TechDesign_Poise.md)) |
| `FLNPProjectileParryCommand` | 방어자: Parry.Success 큐 + 이벤트. **반사 재현 방송**: `Multicast_RespawnReflectedGhost`(구 Ghost 소멸 + 새 Ghost 스폰). 공격자 경직 없음 (투사체 패링 스펙) |
| `FLNPGuardBlockCommand` | Guard.Block 큐 (향후 스태미나 GE 지점) |
| `FLNPApplyDamageGECommand` | 피해 GE + HitReact/임팩트 큐 + 넉백 + 공격자 HitStop(근접만) |

---

## 5. 투사체 반사 상태 전환

| 항목 | 변경 전 | 변경 후 |
|:---|:---|:---|
| `Velocity` | 원래 방향 | `-Velocity` (Linear 180° 반사) |
| `InstigatorTeam` | Enemy | Player (반사체는 Enemy 피격 경로를 탄다) |
| `Instigator` | 공격자 핸들 | 방어자 핸들 (자기 피격 방지) |
| `InstigatorPlayerID` / `PredictionKeyID` | 공격자 식별자 | **방어자 ID + 서버 SalvoID 재발급** — 이후 Ghost 대조·임팩트 큐가 새 식별자 기준 |
| `CachedRewindSeconds` | 공격자 RTT/2 | 방어자 RTT/2 (Lag Compensation 기준 교체) |
| `FLNPProjectileDeadTag` | 히트 시 추가 | 추가 안 함 (계속 비행) |

클라이언트 재현: 반사는 아무도 예측하지 않으므로 서버가 "구 Ghost 소멸 + 새 Ghost 스폰"을 전 클라이언트에 방송한다. 공격자 클라이언트가 오예측으로 구 Ghost를 이미 파괴했어도 새 스폰으로 반사체가 반드시 보인다.

---

## 6. 패링 창의 네트워크 지연 보정

문제: 방어자가 "완벽한 타이밍"에 가드를 눌러도, RPC가 서버에 도착할 때쯤엔 서버 시계로 창이 이미 닫혀 있을 수 있다.

**해결 (`Server_SetGuardState`):** 서버는 수신 시각에서 **방어자 RTT/2를 뺀 시각**을 창 시작점으로 간주하고, `ParryWindowExpiryTime = (수신 시각 - RTT/2) + ParryWindowDuration`을 Fragment에 기록. HitDetection은 `bIsParrying`과 함께 이 만료 시각을 검사한다. `-1`(미기록)이면 bool만으로 판정하는 하위 호환 유지.

---

## 7. 어필 포인트 (트러블슈팅 & 설계 판단)

### 7.1 Mass-GAS 브릿지 — 상태 미러링 패턴

GAS 태그는 Game Thread 전용이라 Worker Thread 판정에서 읽을 수 없다. 태그 변경 지점(InputHandler)에서 Mass Fragment를 동시 갱신하는 **미러 패턴**으로, 판정은 Fragment만 보고 GAS 생태계(어빌리티 조건·ABP 분기)는 태그를 그대로 쓴다. 단일 진실 공급원을 포기하는 대신 양쪽 시스템의 관용 표현을 모두 유지하는 트레이드오프.

### 7.2 반사 발사체의 신원 재발급

반사체가 공격자의 예측 식별자를 그대로 가지면, 공격자 클라이언트의 Ghost 대조·임팩트 큐 필터가 반사체를 "자기가 쏜 발사체"로 오인한다. 반사 순간 방어자 귀속으로 **식별자를 완전히 재발급**하고 Lag Compensation 기준까지 방어자 RTT로 교체 — 반사체는 사실상 "방어자가 새로 발사한 발사체"로 취급된다.

### 7.3 분기 복제가 만든 패링 누락 버그

근접 판정에서 Enemy→Player와 PvP 분기가 복제 코드였던 시절, PvP 쪽에만 패링·가드 체크가 빠져 있었다 (원거리는 처음부터 통합 구현이라 무사 — 비대칭이 힌트였다). PIE 2인 근접 PvP 왕복 테스트에서 발견, 공용 람다로 통합해 구조적으로 재발을 차단.

### 7.4 창 판정의 서버 권위 + 지연 역보정

패링 성패는 서버 Fragment로만 판정(치팅 불가)하되, 방어자의 입력 타이밍은 RTT/2 역보정으로 존중(§6). Lag Compensation(공격 되감기)과 방향이 반대인 보정이 한 판정 안에 공존한다 — 공격은 과거로 되감고, 방어는 미래로 연장한다.

---

## 8. 미구현 / 제약사항

- **GameplayCue 에셋:** `GameplayCue.LNP.Guard.Block` / `GameplayCue.LNP.Parry.Success` VFX·SFX 에셋 연결 (태그·코드 경로 완성).
- **Guided/Lobbed 반사:** 현재 모든 투사체는 Linear 180° 반사. 기획서상 Guided는 "유도 소실 + 방어자 시선 방향 직선화", Lobbed는 "경로 역행" — 투사체 타입 자체가 미구현 (→ [TechDesign_HitDetection.md](TechDesign_HitDetection.md)).
- **Enemy Actor 패링:** 판정은 `FLNPParryStateFragment` 보유 엔티티(현재 Player만) 대상. Enemy가 패링하려면 StateTree/GA에서 Fragment를 갱신하는 연결 필요.
- **액터 보유 엔티티 전용:** Actor 포인터가 없는 순수 엔티티는 가드/패링 판정에서 자동 제외.
