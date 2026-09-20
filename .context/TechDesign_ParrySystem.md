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

**구현 상태:** Guard·근접 Parry·투사체 Parry·반경 분리·RTT 보정 모두 완료 + PIE 검증. GameplayCue VFX 에셋도 연결됐다(사운드·카메라 쉐이크는 미설정, §8).

---

## 2. 데이터 구조

### 2.1 FLNPParryStateFragment

`HitDetection/LNPGuardParryTypes.h`.

| 필드 | 기본 | 뜻 |
|:---|---:|:---|
| `bIsParrying` / `bIsGuarding` | false | `TAG_State_ParryWindow` / `TAG_State_Guarding` 미러 |
| `ParryAngleCos` / `GuardAngleCos` | 0.707 / 0.5 | cos(45°) / cos(60°) |
| `ParryWindowExpiryTime` | −1 | 서버 전용 — RTT 역보정된 절대 만료 시각 (§6) |

### 2.2 상태 갱신 (ULNPInputHandlerComponent)

```
OnGuardStarted()          ← Enhanced Input Started (GuardAction)
├─ 게이트: 사망 입력 차단 / FreeAim(총기)이면 반환 / State.Staggered면 반환
├─ ASC: +TAG_State_Guarding, +TAG_State_ParryWindow
├─ Fragment: bIsGuarding = true, bIsParrying = true, ParryWindowExpiryTime = 지금 + Duration
├─ 로컬 타이머(ParryWindowDuration, 기본 0.15초) → 만료 시 ParryWindow만 해제
└─ Server_SetGuardState(true) RPC — 서버 Fragment 갱신 + RTT 역보정 만료 시각 기록 (§6)

ReleaseGuardState()       ← 해제는 전부 이 하나를 거친다
├─ ASC 태그 2종 해제 · Fragment 3필드 초기화 · 로컬 타이머 취소
└─ Server_SetGuardState(false)
```

⚠️ 해제 진입점은 세 곳(`OnGuardReleased` · 가드 브레이크의 `Client_ForceReleaseGuard` · 무기 교체의 `NotifyAimModeChanged`)이고 **모두 `ReleaseGuardState()` 하나로 모인다.** 태그와 프래그먼트는 폴링이 아니라 명령형이라 스스로 풀리지 않는다 — 한 경로라도 빠뜨리면 총을 든 채 가드가 유지된다 (→ [TechDesign_CharacterMovement.md](TechDesign_CharacterMovement.md)).

### 2.3 GameplayTag

| 태그 | 설명 |
|:---|:---|
| `LNP.State.Guarding` / `LNP.State.ParryWindow` | 상태 태그 (InputHandler 관리) |
| `GameplayCue.LNP.Guard.Block` / `GameplayCue.LNP.Parry.Success` | VFX/SFX 큐. 두 에셋 모두 `ULNPGameplayCueNotify_VFXSound` 파생이며 Niagara(`NS_Guard_Block`/`NS_Parry_Success`)가 물려 있다. Guard.Block은 `HitStopDuration`도 함께 쓴다 (§4). 사운드는 둘 다 미설정 (§8) |
| `LNP.GameplayEvent.Parry.Success` | 방어자 GA_ParrySuccess 트리거 |
| `LNP.Montage.Value.Parry.Parrier` | 방어자 패링 성공 몽타주 (Chooser 밸류) |
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
      ├─ bIsGuarding && Dot >= GuardAngleCos → 경직 누적(×PoiseGuardMultiplier) + FLNPGuardBlockCommand
      │                                          + (순수 엔티티 공격자면) FLNPEntityHitStopCommand
      └─ 그 외 → 경직 누적(전량) + 임팩트 큐 + FLNPApplyDamageGECommand
                 + (순수 엔티티 공격자면) FLNPEntityHitStopCommand
```

투사체는 두 단계 모두 소멸(`FinishHit`)하지만, **패링된 투사체만 소멸하지 않고 계속 비행한다.**

**공격자는 Actor든 순수 엔티티든 같다.** 순수 엔티티 적의 가상 칼날·투사체도 `FLNPEntityAttackConfig::ParryRadius`(기본 40)를 실어 보내므로 같은 판정을 탄다.

**근접 판정의 단일화:** Player 타겟 2단계 판정은 `JudgePlayerTarget` 람다 하나로 구현되어, Enemy→Player와 근접 PvP(아군 사격)가 항상 같은 로직을 탄다 (§7.3의 버그 재발 방지).

### 각도 판정

방어자 전방과 **공격이 오는 방향** 벡터의 내적 하나로 끝난다. 근접은 `칼날 중점 − 방어자`, 투사체는 `이전 위치 − 현재 위치`를 쓴다(둘 다 "공격자 쪽을 가리키는" 부호). 패링은 `>= cos(45°)`, 가드는 `>= cos(60°)`.

---

## 4. Command별 책임

| 커맨드 | 처리 내용 |
|:---|:---|
| `FLNPMeleeParryCommand` | **방어자:** Parry.Success 큐 + GameplayEvent(→ `ULNPAbility_ParrySuccess`가 `ReactionMontage` 재생) + `Value.Parry.Parrier` 몽타주. **공격자:** 경직도 대량 누적(`LNPPoise::ApplyParryBreak`, 저항 미적용) + 넉백(방어자 반대 0.7 + Up 0.3, 2000 cm/s — 구형 곡률 포물선) |
| `FLNPProjectileParryCommand` | 방어자: Parry.Success 큐 + 이벤트. **반사 재현 방송**: `Multicast_RespawnReflectedGhost`(구 Ghost 소멸 + 새 Ghost 스폰). 공격자 경직 없음 (투사체 패링 스펙) |
| `FLNPGuardBlockCommand` | Guard.Block 큐 (향후 스태미나 GE 지점). **방어자 HitStop은 큐 노티파이가 건다** — 커맨드 `Run`은 서버에서만 돌아 거기서 직접 부르면 호스트 화면에만 걸린다 |
| `FLNPEntityHitStopCommand` | 순수 엔티티 **공격자**의 `HitStopSeq`를 +1. 재생 감속 자체는 그리는 머신이 ISKM 트랙에 건다 (→ [TechDesign_EnemyNPC_LowLOD.md](TechDesign_EnemyNPC_LowLOD.md) §4.8). **패링에는 걸지 않는다** — 자세 붕괴(`Parried`)와 겹치면 가드·패링의 차이가 흐려진다 |
| `FLNPApplyDamageGECommand` | 피해 GE + 넉백 + 공격자 HitStop(근접만) |

**근접 패링은 공격자가 Actor인지 순수 엔티티인지에 따라 수단만 갈린다 — 세기·방향 공식은 같다.**

| | Actor 공격자 | 순수 엔티티 공격자 |
|:---|:---|:---|
| 넉백 | `ApplyKnockback`(Mover) | `FLNPEnemyVelocityFragment`에 `ApplyEntityKnockback` |
| 자세 붕괴 연출 | 경직 시스템의 `Value.Stagger.Parried` 몽타주 | `FLNPEntityAttackFragment::ParriedTimeRemaining`(= `ParriedRecoveryTime`, 기본 1.05초) → 행동 상태 `Parried` |

⚠️ **공격자 몽타주를 직접 재생하지 않는다.** 같은 프레임에 경직 진입이 잡혀 `Montage_Stop`이 즉시 끊는다 — 연출 소유권은 경직 시스템에 있다 (→ [TechDesign_Poise.md](TechDesign_Poise.md) §8).

⚠️ `ParriedRecoveryTime`은 재생할 시퀀스 길이와 **손으로** 맞춘다. 서버는 어떤 시퀀스가 재생되는지 모르고, 시퀀스 길이를 서버 판정에 끌어들이면 애니가 게임플레이를 정하게 된다.

---

## 5. 투사체 반사 상태 전환

| 항목 | 변경 전 | 변경 후 |
|:---|:---|:---|
| `Velocity` | 원래 방향 | `-Velocity` (180° 반사) |
| `InstigatorTeam` | Enemy | Player (반사체는 Enemy 피격 경로를 탄다) |
| `Instigator` | 공격자 핸들 | 방어자 핸들 (자기 피격 방지) |
| `InstigatorPlayerID` / `PredictionKeyID` / `SpawnIndex` | 공격자 식별자 | **방어자 ID + 서버 SalvoID 재발급 + 인덱스 0** — 이후 Ghost 대조·임팩트 큐가 새 식별자 기준 |
| `CachedRewindSeconds` / `bInstigatorIsRemoteClient` | 공격자 기준 | 방어자 기준 (Lag Compensation 기준 교체) |
| `InstigatorViewLocation` | 공격자 시점 | 방어자 위치 (반사 주체가 새 뷰어다) |
| `FLNPProjectileDeadTag` | 히트 시 추가 | 추가 안 함 (계속 비행) |

**포물선(`Lobbed`)도 이 한 줄로 끝난다 — 전용 분기가 없는 것이 설계다.** 중력 가속도는 공유 프래그먼트의 `GravityAccel`에 있어 패링과 무관하게 계속 적분되고, 보존장에서 속도만 뒤집으면 **시간 역행이 되어 날아온 포물선을 그대로 되짚는다**(적분기가 Verlet이라 이산화로도 안 깨진다). "Lobbed는 경로 역행"이라는 기획이 반사 코드가 아니라 물리에서 저절로 나온다.

클라이언트 재현: 반사는 아무도 예측하지 않으므로 서버가 "구 Ghost 소멸 + 새 Ghost 스폰"을 전 클라이언트에 방송한다. 재스폰 페이로드에 같은 `GravityAccel`(공유 프래그먼트 원본)이 실려 가므로 Ghost도 같은 포물선을 그린다. 공격자 클라이언트가 오예측으로 구 Ghost를 이미 파괴했어도 새 스폰으로 반사체가 반드시 보인다.

---

## 6. 패링 창의 네트워크 지연 보정

문제: 방어자가 "완벽한 타이밍"에 가드를 눌러도, RPC가 서버에 도착할 때쯤엔 서버 시계로 창이 이미 닫혀 있을 수 있다.

**해결 (`Server_SetGuardState`):** 서버는 수신 시각에서 **방어자 RTT/2를 뺀 시각**을 창 시작점으로 간주하고 `ParryWindowExpiryTime = (수신 시각 − RTT/2) + ParryWindowDuration`을 Fragment에 기록한다. 같은 보정치로 서버 전용 타이머를 걸어 `bIsParrying`도 그때 내린다. HitDetection은 두 조건을 함께 본다.

- 역보정 상한은 **패링 창의 절반**으로 클램프한다 — 고핑 클라이언트가 창을 실질적으로 늘리지 못하게 하는 보수적 설정이다.
- `ParryWindowExpiryTime == -1`(미기록)이면 bool만으로 판정하는 하위 호환을 유지한다.
- 로컬(예측) 갱신은 역보정 없이 즉시 만료 시각을 쓰고, 서버 RPC 처리 시 보정값으로 덮인다.

---

## 7. 어필 포인트 (트러블슈팅 & 설계 판단)

### 7.1 Mass-GAS 브릿지 — 상태 미러링 패턴

GAS 태그는 Game Thread 전용이라 Worker Thread 판정에서 읽을 수 없다. 태그 변경 지점(InputHandler)에서 Mass Fragment를 동시 갱신하는 **미러 패턴**을 썼다 — 판정은 Fragment만 보고, GAS 생태계(어빌리티 조건·ABP 분기)는 태그를 그대로 쓴다. 단일 진실 공급원을 포기하는 대신 양쪽의 관용 표현을 모두 유지하는 트레이드오프.

### 7.2 반사 발사체의 신원 재발급

반사체가 공격자의 예측 식별자를 그대로 가지면, 공격자 클라이언트의 Ghost 대조·임팩트 큐 필터가 반사체를 "자기가 쏜 발사체"로 오인한다. 방어자 귀속으로 **식별자를 완전히 재발급**하고 Lag Compensation 기준까지 방어자 RTT로 교체해, 반사체를 사실상 "방어자가 새로 발사한 발사체"로 취급한다.

### 7.3 분기 복제가 만든 패링 누락 버그

근접 판정에서 Enemy→Player와 PvP 분기가 복제 코드였던 시절, PvP 쪽에만 패링·가드 체크가 빠져 있었다(원거리는 처음부터 통합 구현이라 무사 — 비대칭이 힌트였다). PIE 2인 근접 PvP 왕복 테스트에서 발견해 공용 람다로 통합, 구조적으로 재발을 차단했다.

### 7.4 방향이 반대인 두 보정이 한 판정 안에 있다

패링 성패는 서버 Fragment로만 판정(치팅 불가)하되 방어자의 입력 타이밍은 RTT/2 역보정으로 존중한다(§6). Lag Compensation은 **공격을 과거로 되감고**, 이쪽은 **방어를 미래로 연장한다.**

---

## 8. 미구현 / 제약사항

- **패링 성공 시 방어자 HitStop:** 기획서 항목이지만 아직 없다. **가드 성공 쪽은 2026-09-19에 들어왔다** — `ULNPGameplayCueNotify_VFXSound::HitStopDuration`(GCN_LNP_Guard_Block에 0.08초)으로, 피격(HitReact 큐)과 같은 수단이다. 패링에 같은 것을 얹을지는 미정이다.
- **가드 성공 시 움찔 리액션(2026-09-20):** `LNP.Montage.Situation.Block` → `AM_SW_Guard_Hit`(`A_SW_Blocking_Hit`를 `RateScale 2.0`으로 0.35초에 압축, BlendIn/Out 0.05/0.10). 여러 NPC에게 연타당하는 상황을 고려해 짧게 잡았다.
  ⚠️ **몽타주를 잘라서(`animEndTime`) 짧게 만들면 안 된다** — 잘린 지점의 포즈를 유지한 채 블렌드 아웃하므로 "잠깐 굳었다가 돌아오는" 그림이 된다. 길이를 줄이려면 `RateScale`로 압축해 **복귀 동작까지 재생되게** 할 것.
  어디티브(`A_SW_Guard_Hit_Add`, 기준 포즈 = 자기 0번 프레임)도 만들어 두었으나 채택하지 않았다 — 가드 자세는 보존되지만 움직임 크기가 원본과 비슷해 이점이 크지 않았다. 연타 누적이 문제가 되면 몽타주 참조만 되돌리면 된다.
- **큐 사운드:** 두 큐 모두 `Sound`가 비어 있다 (MCP로 CDO 확인). 프로젝트에 사운드 에셋 자체가 아직 없다.
- **가드 VFX 구성(2026-09-20 완료):** `NS_Guard_Block`은 `Sparks` 하나뿐이라 **큐는 정상 발동하는데 화면에서 지각되지 않았다**
  (서버 로그 `[Guard] Block success`는 계속 찍히고 있었다). 임팩트/패링과 같은 구성으로 `Flash`(단발 스프라이트)와
  `Light`(라이트 렌더러)를 더했고, 값은 패링보다 낮게 잡았다 — 패링 플래시가 청백색 (500,620,900)·라이트 반경 760인 반면
  가드는 주황 (520,170,40)·라이트 (150,47,9)·반경 340이다. **색 계열로 둘을 가른다**(가드=따뜻한 주황, 패링=청백).
  플래시에는 `ScaleSpriteSize`(0.45 → 1.15 → 1.35)를 넣었다 — 크기가 고정이면 원이 켜졌다 꺼지는 그림이 된다.

- ⚠️⚠️ **스파크가 한 픽셀도 안 그려지고 있었다 — 원인은 `bLocalSpace`였다 (2026-09-20).**
  `NS_Guard_Block`·`NS_Parry_Success`의 `Sparks`가 월드 스페이스였고, 플레이어가 월드 원점에서 약 24,000cm 떨어진
  구 내벽에 있는 이 월드에서는 그 조합이 화면에 아무것도 남기지 않았다. 같은 시스템의 `Flash`(로컬)는 멀쩡히 보였다.
  **증상의 지문: 크기·색·머티리얼·힘 모듈을 아무리 바꿔도 화면이 1px도 변하지 않는다** — 픽셀 단계에 도달조차 못 하기 때문이다.
  이 지문이 보이면 이미터 값 디버깅을 멈추고, **같은 시스템 안에서 보이는 이미터와 안 보이는 이미터의 `bLocalSpace`를
  나란히 비교**하는 것이 가장 빠르다.
  ⚠️ **"월드 스페이스면 무조건 컬링"은 아니다** — `NS_Impact_Generic`의 `Sparks`는 월드 스페이스인데 정상이었다.
  두 이미터의 차이는 속도 모듈이다(가드 `AddVelocity` From Point = 위치 빼기 / 임팩트 `AddVelocityInCone` = 방향 축).
  24,000cm 좌표에서 float32로 위치를 빼면 유효 자릿수가 무너지는 쪽이 유력하나 **미확정**이다.
  ⚠️ 로컬 스페이스 전환은 공짜가 아니다 — `PointAttractionForce`의 `AttractorPosition=(0,0,0)`이 행성 중심이 아니라
  **컴포넌트 원점**을 가리켜 구면 중력이 조용히 뒤집힌다. 수명 0.3초 버스트에는 중력이 무의미해 힘 모듈을 껐다.
- **패링 카메라 쉐이크가 사실상 안 보인다:** `GCN_LNP_Parry_Success`의 `CameraShake`는 엔진 기본 클래스 `UDefaultCameraShakeBase`로 지정돼 있다 — 값이 비어 있는 것은 아니지만 튜닝된 쉐이크도 아니다. 루트 패턴이 Perlin 노이즈 1초에 **위치 진폭 1cm·1Hz, 회전 배율 0**이라 체감되지 않는다. 전용 쉐이크 클래스를 만들어 교체할 자리다. (`GCN_LNP_Guard_Block`은 `CameraShake` 자체가 비어 있다)
- **Guided 반사:** `ELNPProjectileType::Guided`는 열거값만 있고 유도 로직 자체가 미구현이라, 기획서상 "유도 소실 + 방어자 시선 방향 직선화"를 적용할 대상이 없다 (→ [TechDesign_HitDetection.md](TechDesign_HitDetection.md)).
- **패링하는 쪽은 플레이어 전용:** 판정 대상은 `FLNPParryStateFragment`를 가지고 Actor도 있는 엔티티뿐이고, 현재 그 조건을 만족하는 것은 플레이어밖에 없다. Enemy가 패링하려면 StateTree/GA에서 Fragment를 갱신하는 연결이 필요하다.
  **패링당하는 쪽에는 이 제약이 없다** — Actor 적도 순수 엔티티 적도 모두 패링된다 (§4).
