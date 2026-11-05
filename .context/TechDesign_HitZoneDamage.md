# 부위별 데미지 차등 판정 기술 설계 (계획)

> **상태: 설계·결정 확정(2026-11-05, §11) · 구현 전**
> 판정 기하·되감기·팀 규칙의 바탕은 [TechDesign_HitDetection.md](TechDesign_HitDetection.md)가 소유한다. 이 문서는 그 위에 얹는 "어디를 맞았는가" 한 축만 다룬다.

## 1. 한눈에 보기

**러프한 2단계다.** 피격자 하나에 판정 형상이 두 개 있다.

| 단계 | 형상 | 예 | 피해 배율 |
|:---|:---|:---|:---|
| 1단계 (몸) | 기존 판정 캡슐 전체 | — | ×1.0 |
| 2단계 (약점) | 캡슐 **안에** 놓인 구 하나 | 인간형 적·플레이어 = 머리, 드론 = 전방 빨간 센서 | ×1.5 |

| 적용 범위 | |
|:---|:---|
| 공격 경로 | **투사체 직격**(단발·산탄 펠릿·유탄 직격)과 **근접** |
| 피격자 | **적과 플레이어 모두** — 부위는 피격자의 형상이 정하고 공격자가 누구인지는 묻지 않는다(PvP·적 → 플레이어 포함) |
| 미적용 | 스플래시·화염 원뿔·화상 (범위·틱 피해라 "맞은 부위"가 없다) |
| 연출 | **추가하지 않는다** — 세게 들어간 피해 자체가 피드백이다 |

```
[서버 판정 — 투사체 직격·근접 공통]
  기존 몸 판정 (되감기·광역 단계·격자·Judge 그대로)
     │ 빗나감 → 다음 후보 (변경 없음)
     ▼ 몸 명중 (플레이어 피격자는 Judge 결과가 Hit일 때만)
  약점 구 판정 (명중한 대상에만, 같은 선분·칼날 궤적, 같은 되감긴 중심)
     │ 명중 → Zone = WeakSpot, Damage × 1.5
     │ 빗나감 → Zone = Body
     ▼
  기존 피해 경로 — 배율 반영된 값만 넘긴다
    적 Actor: GE SetByCaller / 순수 엔티티: HP 직접 차감 / 플레이어: FLNPPlayerHitEffects.Damage
```

핵심은 셋이다.
- **약점 구는 판정 캡슐 안에 있어야 한다**(포함 제약, §2 D2). 그러면 "구를 먼저 본다"와 "몸 명중 뒤 구로 분류한다"가 **수학적으로 같은 결과**를 내고, 뒤쪽이 훨씬 싸고 건드릴 곳이 적다.
- **구의 위치는 몸 로컬 고정 오프셋이다.** 본(bone)을 따라가지 않는다(§2 D1).
- **바뀌는 것은 피해 숫자 하나뿐이다.** 경직·넉백·플린치·패링·가드·회피·복제·예측은 그대로다.

---

## 2. 핵심 설계 결정

### D1. 약점 형상은 "몸 로컬 고정 오프셋 구"다 — 본을 따라가지 않는다

반지름과 로컬 오프셋(판정 캡슐 중심 기준, X=전방·Y=우측·Z=Up)을 데이터로 둔다(적 = `ULNPEnemyConfig`, 플레이어 = `ALNPPlayerCharacter` 기본값, §3). 월드 위치는
`되감긴 캡슐 중심 + 몸 회전 × 로컬 오프셋`이다.

| 판단 | 근거 |
|:---|:---|
| 본 소켓(`head`)을 읽지 **않는다** | 순수 엔티티는 서버에 스켈레톤이 없다(ISKM은 클라이언트 표현). Actor 승격 적만 본을 읽으면 **같은 적이 모드에 따라 다른 형상으로 판정된다** — [TechDesign_HitDetection.md](TechDesign_HitDetection.md) §7.6이 경고한 "LOD·모드로 갈리는 분기"를 새로 만드는 일이다. 판정 프로세서는 워커 스레드라 플레이어 메시 본을 읽는 것도 피한다 |
| 되감기는 **위치 차이만 더한다** | 위치 이력(`FLNPPositionHistoryFragment`)은 위치만 기록한다. 오프셋을 캡슐 중심 기준으로 두면 캡슐과 구가 **한 덩어리로** 되감긴다 — 기존 되감기 결과에 오프셋만 더하면 끝이다 |
| 인간형 머리(적·플레이어)는 **Z 오프셋만** 쓴다 | 머리가 몸 축 위에 있으므로 회전과 무관해진다. 회전을 되감지 않는 한계(§10)를 아예 밟지 않는다 |
| 드론 센서는 **X 오프셋**을 쓴다 | 센서가 전방(+X)에 있다(SurfaceSupportNavigation `history/Phase03c_Log.md` — 블렌더 센서 중심 (0.39, 0, 0)m). 드론 ISM은 엔티티 Transform을 오프셋 없이 그리므로 **회전 × 오프셋이 곧 보이는 센서 위치**다 |

### D2. 판정 순서 — "몸 먼저, 명중 시에만 구로 분류" + 포함 제약 (확정)

**약점 구가 캡슐 안에 들어 있으면 "구 먼저"와 "몸 먼저"의 분류 결과는 항상 같다.**

- 구 S ⊂ 캡슐 C 이면, 같은 반경 h로 팽창해도 S⊕h ⊂ C⊕h 다(민코프스키 합은 포함을 보존한다).
  투사체는 `HitRadius`, 근접은 칼날 반경 `SwordRadius`가 그 h다.
- 그러므로 팽창된 S에 닿는 선분(또는 칼날 궤적 Quad)은 반드시 팽창된 C에도 닿는다. 즉 **약점 명중 ⊂ 몸 명중**이다.
- "S 먼저 → 아니면 C"와 "C 먼저 → 맞았으면 S로 분류"는 S·C·빗나감 세 결과가 모든 입력에서 같다.

비용과 수정 범위는 크게 다르다.

| | 구 먼저 (돌출 허용) | **몸 먼저 + 포함 제약 (채택)** |
|:---|:---|:---|
| 구 판정 횟수 | 모든 후보 × 매 프레임 | **몸 명중한 대상만** (명중은 후보보다 훨씬 드물다) |
| 광역 단계(`SegmentMayHitCapsule`)·격자 질의 반경(`MaxCapsuleExtent`) | 구가 튀어나온 만큼 키워야 한다 | 그대로 |
| 클라이언트 예측(Ghost 소멸 판정) | 구를 추가하지 않으면 "서버는 머리 명중, 게스트 Ghost는 통과"가 생긴다 | 그대로 — 몸 명중 집합이 변하지 않는다 |
| 패링·가드·회피 반경(`LNPGuardParry::Judge` 입력) | 구를 넣을지 따로 정해야 한다 | 그대로 — Judge는 몸 기하만 본다 |
| 격자 검증(`ValidateSpatialGrid`)·조준점 깊이·락온 | 구를 반영해야 일관된다 | 그대로 |

포함 제약은 데이터에서 강제한다 — 적은 `ULNPEnemyConfig::IsDataValid`, 플레이어는 `ALNPPlayerCharacter::IsDataValid`가 위반을 에셋 저장 오류로 낸다(§3). 머리가 캡슐 위로 튀어나와야 하는 대상이 생기면 구를 줄이거나 판정 캡슐을 늘린다.

포함 검사식(캡슐 로컬, 축은 Z, 축 반길이 `L = max(HalfHeight − Radius, 0)`):
```
d = sqrt(X² + Y² + max(|Z| − L, 0)²)      // 구 중심 ~ 캡슐 축 선분 거리
포함 ⇔ d + WeakSpotRadius ≤ CapsuleRadius
```

### D3. 배율은 방어력 적용 **전** 원시 피해에 곱한다

모든 피해 경로가 같은 자리에서 곱해지게 한다.
- 적 Actor·플레이어: `FLNPApplyDamageGECommand`에 넘기는 `Damage`(SetByCaller)를 배율 반영 값으로 넘긴다 → AttributeSet이 방어력을 적용한다.
- 순수 엔티티: `LNPDamage::ApplyDefense(Damage × 배율, Defense)`.

방어 감쇠식 `100/(100+Def)`는 선형이므로 최종 피해 비율도 정확히 1.5배가 된다. 검증에서 그대로 확인할 수 있다(§9).

### D4. 경직·넉백·플린치·피격 주시에는 배율을 걸지 않는다

- **경직도:** "몇 번 맞았는가"의 눈금이다. 스플래시가 거리 감쇠를 경직에 걸지 않는 것과 같은 이유다([TechDesign_HitDetection.md](TechDesign_HitDetection.md) §6.3).
- **넉백:** 무기의 밀어내기 특성이고 맞은 부위와 무관하다.
- **플린치·HitReact·주시 타이머:** 이미 "맞았다"는 사실만으로 정해진다.

### D5. 배율은 전역 설정 값 하나다

`ULNPSettings::WeakSpotDamageMultiplier = 1.5`. 적·플레이어·공격 경로 모두 같은 값이다. 갈라야 할 요구가 생기면 그때 옮긴다.

### D6. 공격 경로 — 투사체 직격과 근접 (확정)

| 경로 | 적용 | 비고 |
|:---|:---|:---|
| 투사체 직격(단발·산탄 펠릿·유탄 직격·패링 반사탄) | **적용** | 조준 실력이 보상으로 이어진다 |
| 근접 | **적용** | 칼날 궤적은 애니메이션이 정하므로 내려베기처럼 머리 높이를 지나는 모션이 보너스를 자주 받는다. **모션이 주는 보너스로 인정한다**(사용자 결정) |
| 스플래시 | 미적용 | 범위 피해다. 직격 대상은 원래 스플래시에서 제외된다 |
| 화염 원뿔·화상 | 미적용 | 원뿔·틱 피해라 "맞은 부위"가 없다 |

### D7. 피격자 — 적과 플레이어 모두, 부위는 피격자가 정한다 (확정)

플레이 중 팀이 나뉘어 PvP가 될 수 있으므로 플레이어에게도 머리 약점이 있다.

- **규칙은 피격자 쪽 하나다.** "이 대상의 약점 구에 닿았는가"만 묻고 공격자 종류(플레이어·적, 같은 팀 여부)로 분기하지 않는다. 대상 후보를 고르는 것은 여전히 `LNPTeam::CanHit` 하나다 — 같은 팀은 원래 맞지 않으므로 헤드샷도 없다.
- 그 결과 **적 → 플레이어 공격도 머리에 닿으면 1.5배다.** 적 원거리는 캡슐 중심(골반)을 겨누고(`AimTargetUpOffset` 기본 0), 순수 엔티티 가상 칼날은 Pivot +30cm에서 아래로 기운 궤적(`ArcPitch` −15°)이라 드물 것으로 본다. 빈도는 PIE에서 로그로 센다(§9.2 #10). 적 밸런스가 흔들리면 그때 공격자 팀으로 거르는 술어를 따로 정한다.
- 플레이어 피격 4경로 중 **근접·투사체 직격 둘만** 해당한다(스플래시·화염은 D6로 제외). 둘 다 `LNPGuardParry::Judge` 결과가 **`Hit`일 때만** 분류한다 — 패링·저스트 회피·회피는 피해가 없고, 가드는 직접 피해를 막으므로 분류할 피해가 없다.

### D8. 네트워크·연출 — 서버 판정만 바뀐다

- 분류는 **서버 권위 판정 안에서만** 한다. 피해는 원래 서버만 적용한다.
- **복제 추가 0바이트.** 약점 치수는 양쪽에 같은 에셋(적 Config·플레이어 BP 기본값)으로 있고, 피해는 기존 경로(어트리뷰트 복제 / 순수 엔티티 HP 비율 복제)로 반영된다.
- **클라이언트 예측 무변경.** D2의 포함 제약 덕분에 몸 명중 집합이 바뀌지 않아 Ghost 소멸·로컬 착탄 VFX·근접 로컬 HitStop이 지금과 똑같이 맞는다.
- **약점 전용 연출(소리·히트마커·VFX 변형)은 만들지 않는다**(사용자 결정). 사수 화면 착탄 VFX는 로컬 Ghost 판정이 내고 서버 큐는 중복 차단되므로, 나중에 연출을 넣는다면 그 구조부터 다시 봐야 한다는 점만 남겨 둔다([TechDesign_HitDetection.md](TechDesign_HitDetection.md) §4).

---

## 3. 데이터 모델

### 3.1 `ULNPEnemyConfig` (Category `LNP|Collision|WeakSpot`)

기존 판정 몸체 필드(`HitCenterUpOffset`·`HitCapsuleRadius`·`HitCapsuleHalfHeight`)와 같은 평면 필드로 둔다.

| 필드 | 형 | 기본값 | 의미 |
|:---|:---|:---|:---|
| `WeakSpotRadius` | float (cm, ClampMin 0) | 0 | **0이면 약점 없음** — 이 적은 1단계만 쓴다(벽보행 로봇청소기 등) |
| `WeakSpotLocalOffset` | FVector (cm) | (0,0,0) | 판정 캡슐 중심 기준 몸 로컬 오프셋. X=전방·Y=우측·Z=Up(구 내벽 Up) |

- `#if WITH_EDITOR` `IsDataValid` — D2 포함 검사 위반 시 `EDataValidationResult::Invalid`와 수치가 든 오류 메시지(영어). 판정 캡슐 치수는 `GetHitCapsuleRadius/HalfHeight()`로 읽는다.
- `LNPEnemyConfig.cpp`가 아직 없으므로 새로 만든다. 헤더 변경이라 Live Coding이 아니라 에디터를 닫고 전체 빌드한다.

### 3.2 `ALNPPlayerCharacter` (`UPROPERTY(EditDefaultsOnly)`, Category `LNP|Collision|WeakSpot`)

플레이어에는 Config 에셋이 없고 판정 캡슐도 캐릭터의 `CapsuleComponent`(네이티브 서브오브젝트, 기본 42/96)다. 그래서 약점 치수를 **캡슐과 같은 클래스 기본값**에 둔다 — 캡슐을 고치는 사람이 같은 자리에서 머리 구를 본다.

| 필드 | 기본값 | 의미 |
|:---|:---|:---|
| `WeakSpotRadius` | 16 | 0이면 약점 없음 |
| `WeakSpotLocalOffset` | (0, 0, +78) | 캡슐 중심 기준. 플레이어 판정 Up은 위치에서 나오는 구 내벽 Up이라 Z만 쓴다 |

- `#if WITH_EDITOR` `IsDataValid` — `GetCapsule()`의 스케일 치수로 같은 포함 검사를 한다(BP CDO의 캡슐 템플릿 값이 들어온다).
- 수집 패스(워커 스레드)는 이미 `PlayerPawn->GetCapsule()`의 치수를 읽고 있다. 같은 자리에서 이 두 값을 읽는다(불변 기본값이라 경합이 없다).

### 3.3 초기값 (디버그 드로우로 메시에 맞춰 보정한다)

| 대상 | 판정 캡슐 | 약점 초기값 | 포함 여유 |
|:---|:---|:---|:---|
| 플레이어 (`ALNPPlayerCharacter` BP) | HalfHeight 96 · Radius 42 | (0, 0, +78), 반지름 16 | 축 끝(54)까지 24 + 16 = 40 ≤ 42 |
| `DA_Enemy_PureEntity_Melee01` · `_Ranged01` · `DA_Enemy_ActorPromoted_Melee01` (인간형) | 에셋 값 확인 필요(코드 기본 HalfHeight 88 · Radius 35) | (0, 0, +70), 반지름 16 | 기본 치수 기준 축 끝(53)까지 17 + 16 = 33 ≤ 35 |
| `DA_Enemy_PureEntity_Flyer01` (드론) | `HitCapsuleRadius` 64 | (+39, 0, 0), 반지름 15 | 39 + 15 = 54 ≤ 64 |
| `DA_Enemy_PureEntity_Crawler01` | — | 반지름 0 (없음) | — |

### 3.4 `ULNPSettings`

| 필드 | 기본값 | 의미 |
|:---|:---|:---|
| `WeakSpotDamageMultiplier` | 1.5 (ClampMin 1.0) | 약점 명중 시 원시 피해 배율 |

### 3.5 공용 헬퍼 (`LNPHitDetectionShared.h`, `namespace LNPHitDetection`)

판정식은 한 곳에만 둔다 — 투사체·근접·디버그 드로우·테스트·`IsDataValid` 두 곳이 모두 이것만 부른다(§7.6의 교훈).

```cpp
/** 맞은 부위. 러프 2단계 — 피해 배율과 로그가 쓴다. */
enum class ELNPHitZone : uint8 { Body, WeakSpot };

/** 약점 구 중심 = 캡슐 중심 + 이 값. 몸 회전으로 로컬 오프셋을 돌린 월드 벡터다. */
FVector ResolveWeakSpotOffset(const FQuat& BodyRotation, const FVector& LocalOffset);

/** 포함 제약(D2). 캡슐 로컬 좌표 — IsDataValid 두 곳과 자동화 테스트가 쓴다. */
bool IsWeakSpotInsideCapsule(float CapsuleHalfHeight, float CapsuleRadius,
                             const FVector& LocalOffset, float WeakSpotRadius);

/** 투사체용. 몸 명중이 확정된 뒤에만 부른다. 구는 반높이 = 반지름인 캡슐이라 SegmentHitsCapsule을 그대로 쓴다. */
ELNPHitZone ClassifyProjectileHitZone(const FPreparedSegment& Segment, const FVector& WeakSpotCenter,
                                      const FVector& UpDir, float WeakSpotRadius, float HitRadius,
                                      FVector& InOutHitPoint);

float ScaleDamageForZone(float RawDamage, ELNPHitZone Zone);   // WeakSpot이면 × WeakSpotDamageMultiplier
```

- **새 기하 함수는 만들지 않는다.** `SegmentHitsCapsule`은 이미 "구형 캡슐(축 길이 0)"을 정확히 처리한다(`LNP.HitDetection.SegmentCapsule.*` 테스트가 덮는다). 근접 `CalcDistSq`도 축이 점으로 줄어들면 삼각형-점 거리가 되어 그대로 성립한다 — `SegmentIntersectsTriangle`은 길이 0 선분에서 행렬식 0으로 거짓을 돌려주고, 나머지 최근접점 항이 거리를 낸다. 근접 분류는 그 람다가 `LNPWeaponTraceProcessors.cpp` 안에 있으므로 거기서 한 번 감싼다(§4.2).
- 약점 명중이면 `InOutHitPoint`를 구 판정의 최근접점으로 바꾼다 — 기존 임팩트 VFX가 머리·센서 쪽에 뜬다(새 연출은 아니다).
- 몸 회전: 적은 엔티티 회전(`LNPMass::GetEntityUp`이 쓰는 회전), 플레이어는 수집 패스가 이미 쓰는 엔티티 Transform 회전이다. 플레이어는 Z 오프셋만 쓰므로 Up만 의미가 있다(§5).

---

## 4. 판정 흐름

### 4.1 투사체 직격 (서버, `ULNPProjectileHitDetectionProcessor`)

**Pass 1·2 수집** — `FCollectedEnemy`·`FCollectedPlayer`에 두 필드를 더한다.
```cpp
FVector WeakSpotOffset;   // ResolveWeakSpotOffset(회전, LocalOffset), 약점 없으면 0
float   WeakSpotRadius;   // 0이면 약점 없음
```

**적 후보 루프** — 기존 캡슐 판정 직후에 분류 한 단계를 끼운다.
```cpp
const FVector Center = RewoundEnemyCenter(Enemy);           // 기존 그대로
FVector HitPoint;
if (!SegmentHitsCapsule(Segment, Center, Enemy.UpDir, ...)) // 기존 그대로
    continue;

ELNPHitZone Zone = ELNPHitZone::Body;
if (Enemy.WeakSpotRadius > 0.f)
    Zone = LNPHitDetection::ClassifyProjectileHitZone(Segment, Center + Enemy.WeakSpotOffset,
               Enemy.UpDir, Enemy.WeakSpotRadius, HitRadius, HitPoint);
const float Damage = LNPHitDetection::ScaleDamageForZone(Shared.Damage, Zone);

// 이하 기존 분기 — Shared.Damage 자리에 Damage를 넣는다
//   Actor   : FLNPApplyDamageGECommand(..., Damage, ...)
//   Entity  : Health -= ApplyDefense(Damage, Defense)
// 경직(Shared.PoiseDamage)·넉백·플린치·FinishHit은 손대지 않는다
```

**플레이어 후보 루프** — `Judge` 결과가 `Hit`인 분기에서만 분류한다. 몸 판정 `bInHitRange`가 이미 낸 `HitPoint`와 되감긴 `JudgedCenter`를 그대로 쓴다.
```cpp
// Result == Hit 분기 (Guarded·Parried·Dodged·JustDodged는 무변경)
ELNPHitZone Zone = ELNPHitZone::Body;
if (Player.WeakSpotRadius > 0.f)
    Zone = ClassifyProjectileHitZone(Segment, JudgedCenter + Player.WeakSpotOffset,
               Player.UpDir, Player.WeakSpotRadius, HitRadius, HitPoint);
Effects.Damage.Emplace({ ..., ScaleDamageForZone(Shared.Damage, Zone), ... });
LNPHitGrace::Dispatch(...);   // 기존 그대로
```

- 되감기·격자·광역 단계·후보 순서("수집 순서의 첫 명중")·Judge 입력은 **바뀌지 않는다.**
- 패링 반사탄(팀이 방어자 팀으로 바뀐 탄)도 같은 분기를 지나므로 머리에 맞으면 약점이다 — 별도 처리 없음.
- 계측 경로(`[RewindLag]`, `ValidateSpatialGrid`)는 몸 판정 그대로 둔다.

### 4.2 근접 (서버, `ULNPWeaponTraceHitDetectionProcessor`)

같은 파일의 `CalcDistSq` 람다를 감싸는 분류 람다를 하나 둔다. 적·플레이어 두 분기가 이것만 부른다.
```cpp
auto ClassifyMeleeHitZone = [&](const FVector& RewoundCenter, const FVector& UpDir,
                                const FVector& WeakSpotOffset, float WeakSpotRadius) -> ELNPHitZone
{
    if (WeakSpotRadius <= 0.f)
        return ELNPHitZone::Body;
    // 구 = 반높이와 반지름이 같은 캡슐 → MakeCapsuleSeg가 축을 점으로 만든다
    return CalcDistSq(RewoundCenter + WeakSpotOffset, UpDir, WeakSpotRadius, WeakSpotRadius)
            <= FMath::Square(SwordRadius + WeakSpotRadius)
        ? ELNPHitZone::WeakSpot : ELNPHitZone::Body;
};
```

- **적 분기:** 몸 명중 확정(`CalcDistSq ≤ (SwordRadius + CapsuleR)²`) 직후 분류하고 `Frag.Damage` 대신 배율 값을 Actor GE·엔티티 HP 차감에 넘긴다. 되감긴 중심은 몸 판정에 쓴 값을 변수로 받아 다시 계산하지 않는다(지금은 `RewoundEnemyCenter` 호출이 조건식 안에 있어 꺼내야 한다).
- **플레이어 분기(`JudgePlayerTarget`):** `Result == Hit` 분기에서만 분류해 `Effects.Damage`의 피해를 배율 값으로 넣는다. 되감긴 중심은 `DistSq` 계산에 쓴 값을 변수로 꺼내 공유한다.
- **스윙당 첫 접촉이 부위를 정한다.** `AlreadyHit`이 스윙당 1회 피해를 보장하므로, 몸에 먼저 닿은 프레임에 Body로 확정되고 이후 프레임에 칼이 머리를 지나도 다시 판정하지 않는다. 같은 프레임에 둘 다 닿으면 WeakSpot이다.
- 칼날의 주인은 묻지 않는다 — 플레이어 근접(PvE·PvP), 승격 적 근접(ANS), 순수 엔티티 가상 칼날이 같은 처리를 받는다(D7).
- 클라이언트 예측(로컬 공격자 HitStop)은 몸 명중만 보므로 무변경이다.

---

## 5. 좌표·되감기 규약

- 기준점은 **판정 캡슐 중심**이다. 적은 `ResolveEnemyCapsuleCenter`(`HitCenterUpOffset` 포함), 플레이어는 엔티티 위치(= 캡슐 중심)다. 약점 오프셋은 그 위에 얹는다.
- 축: 적은 엔티티 회전 Z가 Up, 회전 X가 전방이다. 플레이어 판정 Up은 위치에서 나오는 구 내벽 Up(`(-Location).GetSafeNormal()`)이라 오프셋의 Z 성분은 그 Up을 따라 놓는다 — 플레이어 오프셋은 Z만 쓰므로 `UpDir * Offset.Z`와 같다.
- 되감기: `약점 중심 = 되감긴 캡슐 중심 + WeakSpotOffset`. 위치만 되감고 **회전은 현재 값**을 쓴다(§10 한계).
- 오프셋은 수집 시점에 한 번 계산해 같은 프레임의 모든 투사체·칼날이 공유한다.

---

## 6. 다른 시스템과의 상호작용

| 시스템 | 영향 |
|:---|:---|
| 산탄 | 펠릿마다 독립 분류. 머리에 몰린 펠릿만 1.5배 |
| 유탄 | 직격 대상만 분류, 스플래시는 원래 직격 대상을 제외하므로 이중 적용 없음 |
| 패링 반사탄 | §4.1 — 같은 경로라 자동 적용 |
| 패링·가드·회피·저스트 회피 | 무변경 — Judge는 몸 기하만 보고, 분류는 `Hit` 결과에서만 한다 |
| 피격 유예(`LNPHitGrace`) | 무변경 — 배율이 반영된 피해가 `FLNPPlayerHitEffects`에 담겨 보류·해제된다. 소급 회피되면 통째로 사라지는 것도 같다 |
| 경직·그로기 | 무변경(D4) |
| 방어력 | 배율 뒤에 적용(D3) |
| 팀 규칙 | 무변경 — 후보 선택은 `LNPTeam::CanHit`, 부위는 피격자 형상(D7) |
| 적 HP 바 · 플레이어 HUD HP | 복제된 HP가 그대로 반영 |
| 조준점 깊이·락온·근접 보정(`ULNPTargetQuerySubsystem`) | 무변경 — 여전히 몸통 축 최근접점을 쓴다 |
| 클라이언트 예측(Ghost·로컬 HitStop) | 무변경(D2·D8) |
| 사망한 대상 | 원래 후보에서 제외 |
| 대역폭 | 0(D8) |

---

## 7. 디버그·관측

- **디버그 드로우:** 기존 `ULNPProjectileDebugDrawProcessor`가 적 판정 캡슐(빨강)과 플레이어 캡슐(초록)을 그린다. 둘 옆에 **노란 구**로 약점을 그린다. 같은 헬퍼(`ResolveEnemyCapsuleCenter` + `ResolveWeakSpotOffset`)를 부르므로 그림과 판정이 갈리지 않는다. 메시에 대 보며 §3.3 초기값을 보정하는 도구다.
  - 플레이어 캡슐 그리기는 지금 54/42를 하드코딩하고 있다. 약점 구는 실제 캡슐 치수·필드 값을 읽어 그린다(캡슐 그리기 자체는 이 작업 범위가 아니다).
- **분류 로그:** CVar `LNP.HitZone.Log`(기본 0, 서버). 켜면 몸 명중마다 한 줄 —
  `[HitZone] path=Projectile|Melee victim=Enemy|Player zone=WeakSpot raw=%.1f scaled=%.1f`.
  기존 HP 로그(`[HitDetection][Entity] HP: ...`, `[WeaponTrace][Entity] HP: ...`, `[GE] HP: ...`)가 배율 반영 값을 찍으므로 두 줄로 비율을 확인할 수 있다. 적 → 플레이어 약점 빈도(§9.2 #10)도 이 로그로 센다.

---

## 8. 구현 계획

각 단위는 빌드·검증 후 커밋한다. 헤더를 바꾸는 단위 1은 에디터를 닫고 `Build.bat` 전체 빌드, 이후 `.cpp`만 바꾸는 단위는 Live Coding이 가능하다.

### 단위 1 — 데이터·헬퍼·테스트 (코드만, 동작 변화 0)

| 파일 | 작업 |
|:---|:---|
| `Enemy/LNPEnemyConfig.h` · 신규 `Enemy/LNPEnemyConfig.cpp` | §3.1 필드 2개, `IsDataValid` |
| `Character/LNPPlayerCharacter.h/.cpp` | §3.2 필드 2개, `IsDataValid` |
| `Config/LNPSettings.h` | §3.4 `WeakSpotDamageMultiplier` |
| `HitDetection/LNPHitDetectionShared.h` | §3.5 `ELNPHitZone`·헬퍼 4개 |
| 신규 `Tests/LNPHitZoneTest.cpp` | §9.1 자동화 테스트 |

완료 기준: 빌드 성공, 신규 테스트와 기존 `LNP.HitDetection.SegmentCapsule.*` 통과. 판정 프로세서가 아직 필드를 읽지 않으므로 게임 동작 변화 없음.

### 단위 2 — 투사체 직격 (적·플레이어 피격자) + 디버그 + 에셋 값

| 파일·에셋 | 작업 |
|:---|:---|
| `HitDetection/LNPProjectileProcessors.cpp` | §4.1 수집 필드·적/플레이어 분류·배율, §7 디버그 드로우·로그 CVar |
| `DA_Enemy_PureEntity_Melee01` · `_Ranged01` · `DA_Enemy_ActorPromoted_Melee01` · `DA_Enemy_PureEntity_Flyer01`, 플레이어 캐릭터 BP | §3.3 초기값 입력 → 디버그 드로우로 메시에 맞춰 보정 (Unreal MCP 또는 사용자 편집) |

완료 기준: §9.2 #1~#6, #9(투사체) 통과, 저장 시 `IsDataValid` 오류 0.

### 단위 3 — 근접 (적·플레이어 피격자)

| 파일 | 작업 |
|:---|:---|
| `HitDetection/LNPWeaponTraceProcessors.cpp` | §4.2 분류 람다, 적 분기·`JudgePlayerTarget`의 되감긴 중심 공유와 배율, 로그 |

완료 기준: §9.2 #7~#10 통과.

---

## 9. 검증 계획

### 9.1 자동화 테스트 (`LNP.HitDetection.HitZone.*`)

| 테스트 | 확인 내용 |
|:---|:---|
| `Containment` | 포함/경계/돌출 사례로 `IsWeakSpotInsideCapsule` 참·거짓, 축 길이 0(구형 캡슐)·음의 Z 오프셋 포함 |
| `Classify` | 수평 선분이 머리 높이·가슴 높이·정수리 스침을 지날 때 WeakSpot/Body. 무작위 선분 10,000개에서 **"약점 명중이면 몸 명중"**(D2 동치)이 깨지지 않음 — 포함 제약을 만족하는 무작위 구로 |
| `OffsetRotation` | 드론형 전방 오프셋이 Yaw 90°·임의 Up에서 올바른 월드 위치로 돌아감 |
| `DamageScale` | `ScaleDamageForZone` 1.0/1.5배, `ApplyDefense` 뒤 최종 비율도 1.5 |
| `ConfigAssets` | 적 에셋 5종과 플레이어 BP CDO를 로드해 포함 제약 통과(에셋 값이 바뀌어도 회귀를 잡는다) |

근접 분류는 프로세서 안 람다라 단위 테스트 대신 PIE(#7·#8)로 확인한다.

### 9.2 PIE (1인 → 2인)

검증 편성은 기본 `DA_MassSpawnConfig`를 건드리지 않고 별도 에셋 + `LNP.Spawn.ConfigOverride`로 띄운다. 모든 항목에서 `LNP.HitZone.Log 1`.

| # | 시나리오 | 통과 기준 |
|:---|:---|:---|
| 1 | 디버그 드로우로 인간형 적(순수 엔티티·승격)·드론·플레이어의 노란 구가 머리·센서에 겹치는지 육안 | 겹친다. 승격 적·플레이어의 공격·피격 모션 중 머리가 구를 벗어나는 정도를 기록(수용 범위 판단) |
| 2 | 단발 무기로 같은 적의 머리/몸통을 각각 쏜다 | zone 정확, HP 감소량 비율 1.5 (순수 엔티티·Actor 둘 다) |
| 3 | 드론 정면 센서 / 측면 몸체 | 센서만 WeakSpot. 드론이 돌면 구도 따라 돈다 |
| 4 | 산탄 근거리 머리 조준 | 펠릿별 zone 혼재, 총 피해 증가 |
| 5 | 유탄 직격 vs 바닥 착탄 | 직격만 분류, 스플래시 피해는 무변경 |
| 6 | 2인 PIE 게스트가 적을 사격(+`NetEmulation` 지연) | 게스트가 머리를 맞히면 서버 로그도 WeakSpot. Ghost 소멸·착탄 VFX가 지금과 다르지 않다 |
| 7 | 롱소드 수평 베기 / 내려베기로 적 공격 | 첫 접촉 부위로 판정, 스윙당 1회, HP 비율 1.5 |
| 8 | 2인 PIE `LNP.Team.Set`으로 팀 분리 → PvP 사격·근접, 호스트·게스트 양방향 | 머리 명중 시 플레이어 HP 감소 1.5배. 가드 정면이면 피해 없음(분류 안 함), 패링·회피 결과 무변경 |
| 9 | PvP에서 원격 게스트 피격자 + 피격 유예 창 안의 소급 회피 | 보류 후 해제된 피해가 배율 값 그대로, 소급 회피 시 피해 없음 |
| 10 | 적 무리(근접·원거리·승격)와 1~2분 교전 | 적 → 플레이어 `zone=WeakSpot` 줄 수 / 전체 피격 수를 기록 — 높으면 D7 재검토 |
| 11 | 회귀 | 벽보행 로봇청소기(약점 없음) 피해 무변경, `ProjectileProfile` 상세 계측에서 캡슐 판정 횟수 무변경 |

---

## 10. 한계·수용

- **애니메이션을 따라가지 않는다.** 숙이기·돌진·공격 모션에서 머리가 몸 축 꼭대기를 벗어나면 구와 어긋난다. 플레이어도 같다(행글라이더 활공 중 메시 자세가 바뀌어도 판정 캡슐·구는 구 내벽 Up을 따른다). 러프 2단계의 수용 범위이고, PIE #1에서 정도를 기록한다.
- **회전은 되감지 않는다.** 원격 사수의 되감기(최대 0.5초) 동안 드론이 크게 돌면 센서 위치가 현재 회전 기준이 된다. 인간형(적·플레이어)은 Z 오프셋만 써서 해당 없다. 문제가 되면 위치 이력에 회전(또는 Yaw 하나)을 기록한다.
- **한 프레임 선분이 몸통을 지나 머리까지 닿으면 약점으로 친다.** 아래에서 올려 쏘는 등 선분이 길고 가파른 경우다. 진입 순서까지 보려면 선분 위 진입 매개변수(구·캡슐 각각의 최초 교차 t)를 비교해야 하는데, 지금 판정 함수는 진입점이 아니라 최근접점을 돌려준다 — 러프 단계에서는 하지 않는다.
- **근접 약점은 모션이 정한다.** 머리 높이를 지나는 모션이 보너스를 자주 받는다 — 사용자 결정으로 수용(D6).
- **포함 제약 때문에 머리가 캡슐 밖으로 튀어나올 수 없다.** 필요하면 판정 캡슐을 키운다(적은 이동 캡슐과 이미 분리돼 있다. 플레이어는 이동 캡슐 = 판정 캡슐이라 키우면 이동에도 영향이 있다).
- **승격 적에 전방 오프셋을 쓰면** 엔티티 회전이 Actor 회전과 동기되는지 먼저 확인해야 한다. 지금 승격 적은 인간형뿐이라 Z 오프셋만 쓴다.

---

## 11. 확정된 결정 (2026-11-05, 사용자)

| # | 질문 | 결정 |
|:---|:---|:---|
| Q1 | 근접에도 적용하나? | **적용한다.** 근접은 플레이어 실력과 무관한 보너스가 되지만 그 정도는 인정한다(D6) |
| Q2 | 판정 순서 | **몸(캡슐) 먼저 + 포함 제약**(D2) |
| Q3 | 플레이어도 피격 부위를 갖나? | **갖는다.** 플레이 중 팀이 나뉘면 PvP가 되고 헤드샷 보너스가 필요하다(D7). 부위는 피격자가 정하므로 적 → 플레이어에도 적용된다 |
| Q4 | 약점 명중 피드백 | **만들지 않는다.** 세게 들어간 피해 자체가 피드백이다(D8) |
