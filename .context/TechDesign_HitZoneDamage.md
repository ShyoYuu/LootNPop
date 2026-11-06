# 부위별 데미지 차등 판정 기술 설계

> **상태: 단위 1·2 마감(2026-11-06) · 단위 3 근접은 미구현, 새 세션으로 이관 · 인수인계는 §12**
> 판정 기하·되감기·팀 규칙의 바탕은 [TechDesign_HitDetection.md](TechDesign_HitDetection.md)가 소유한다. 이 문서는 그 위에 얹는 "어디를 맞았는가" 한 축만 다룬다.

**현재 구현은 투사체 직격까지다.** 아래 근접 관련 흐름은 단위 3의 목표 설계이며 아직 게임에 적용되지 않았다. 리스폰 시 RPC 오류는 사용자가 이전 세션에서 조사한 동일 현상으로 확인했고, 이번 작업에서는 무시해도 된다고 결정했다. 단위 3의 선행 차단 조건으로 삼지 않는다.

## 1. 한눈에 보기

**러프한 2단계다.** 피격자 하나에 몸 충돌용 캡슐과 약점 분류용 구가 하나씩 있다. 명중 여부는 캡슐만 결정한다.

| 단계 | 형상 | 예 | 피해 배율 |
|:---|:---|:---|:---|
| 1단계 (몸) | 기존 판정 캡슐 전체 | — | ×1.0 |
| 2단계 (약점) | 약점 부위의 캡슐 표면을 덮도록 **돌출한 분류 구** 하나 | 인간형 적·플레이어 = 머리, 비행 드론 = 전방 빨간 센서, 지상 드론 = 정수리 꼭지 | ×1.5 |

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
- **몸 캡슐이 명중을 결정하고, 구는 약점 여부만 분류한다**(§2 D2). 구는 의도한 약점 부위의 캡슐 표면을 덮도록 돌출시킨다. 구에만 닿은 공격은 빗나감이다.
- **구의 위치는 몸 로컬 고정 오프셋이다.** 본(bone)을 따라가지 않는다(§2 D1).
- **약점 배율은 피해와 그 피해를 입력으로 쓰는 물리 HitReact에 반영한다.** 경직·이동 넉백·플린치 타이머·패링·가드·회피·복제 형식·예측은 그대로다.

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
| 지상 드론(Crawler)은 **Z 오프셋**을 쓴다 | 2026-11-06 사용자 요청으로 정수리 꼭지에 추가했다. 구 내벽 방향이 아니라 엔티티 회전 Z를 따라 벽·천장에서도 몸 위쪽에 놓인다. 메시 꼭지 Z=-2, 판정 중심 오프셋 -40이므로 판정 중심 기준 +38cm다 |

### D2. 판정 순서 — "몸 먼저, 명중 시에만 돌출한 구로 분류" (확정)

**몸 명중과 약점 분류의 역할을 분리한다.** 구가 캡슐 밖으로 나와도 명중 범위는 늘어나지 않는다.

- 몸 캡슐에 빗나감 → 구에 닿았더라도 빗나감. 구 판정은 실행하지 않는다.
- 몸 캡슐에 명중하고 같은 프레임 선분·칼날 궤적이 구에도 닿음 → WeakSpot.
- 몸 캡슐에 명중하고 구에는 닿지 않음 → Body.
- 구 접촉을 다음 프레임까지 저장하지 않는다. 구에 먼저 닿았더라도 **몸 명중 프레임에 같은 판정 경로가 구에도 닿아야** 약점이다.

**약점 구는 약점으로 인정하려는 부위의 캡슐 표면을 덮도록 돌출시킨다.** 내부에 파묻힌 구는 정상적인 머리 조준에서도 캡슐이 먼저 맞고 `AlreadyHit`·탄 소멸로 끝나는 문제가 있다. 단지 어느 한쪽으로 돌출하기만 해서는 충분하지 않다. 인간형은 의도한 머리 영역의 정면·측면·후면·위쪽, 드론은 의도한 센서 접근 방향에서 몸에 처음 닿는 순간에도 구에 닿도록 배치한다. 몸통을 먼저 친 뒤 나중에 머리로 진행하는 궤적은 Body로 확정한다(§4.2·§10).

포함 제약과 "구 먼저와 동치" 주장은 폐기한다. 몸 먼저라는 게이트 자체가 `WeakSpot = BodyHit && SphereHit`를 보장한다. `SphereHit`만으로 명중을 만들지 않으므로 다음은 모두 유지한다.

| 항목 | 처리 |
|:---|:---|
| 구 판정 횟수 | **몸 명중한 대상만** |
| 광역 단계(`SegmentMayHitCapsule`)·격자 질의 반경(`MaxCapsuleExtent`) | 몸 캡슐 기준 그대로. 구 돌출 때문에 키우지 않는다 |
| 클라이언트 예측(Ghost 소멸·착탄 VFX·근접 HitStop) | 기존 몸 명중 기준 그대로 |
| 패링·가드·회피(`LNPGuardParry::Judge`) | 몸 기하 기준 그대로 |
| 격자 검증(`ValidateSpatialGrid`)·조준점 깊이·락온 | 몸 기하 기준 그대로 |
| `HitPoint`·임팩트·스플래시 중심 | 기존 몸 판정 결과 그대로. 구 판정은 좌표를 덮어쓰지 않는다 |

적·플레이어의 `IsDataValid`는 활성 약점의 캡슐 겹침과 돌출을 검사하고 오류를 보고한다. 이 검사는 필요한 부위의 표면 전체가 덮였음을 보장하지 않으므로 대표 방향의 첫 접촉 테스트·디버그 드로우로 보완한다(§9).

기본 배치 검사식(캡슐 로컬, 축 반길이 `L = max(HalfHeight − Radius, 0)`):
```
d = sqrt(X² + Y² + max(|Z| − L, 0)²)      // 구 중심 ~ 캡슐 축 선분 거리
활성 약점: d < CapsuleRadius + WeakSpotRadius  // 부피가 겹침
        && d + WeakSpotRadius > CapsuleRadius // 내부 포함·내접을 넘어 돌출
```
반지름 0은 약점 없음으로 검사에서 제외한다. 돌출 부분에만 닿아도 몸 명중 전에는 어떤 효과도 내지 않는다.

### D3. 배율은 방어력 적용 **전** 원시 피해에 곱한다

모든 피해 경로가 같은 자리에서 곱해지게 한다.
- 적 Actor·플레이어: `FLNPApplyDamageGECommand`에 넘기는 `Damage`(SetByCaller)를 배율 반영 값으로 넘긴다 → AttributeSet이 방어력을 적용한다.
- 순수 엔티티: `LNPDamage::ApplyDefense(Damage × 배율, Defense)`.

방어 감쇠식 `100/(100+Def)`는 선형이므로 최종 피해 비율도 정확히 1.5배가 된다. 검증에서 그대로 확인할 수 있다(§9).

### D4. 경직·이동 넉백·타이머는 유지하고, 물리 HitReact에는 약점 피해를 반영한다

- **경직도:** "몇 번 맞았는가"의 눈금이다. 스플래시가 거리 감쇠를 경직에 걸지 않는 것과 같은 이유다([TechDesign_HitDetection.md](TechDesign_HitDetection.md) §6.3).
- **이동 넉백:** 무기의 밀어내기 특성이고 맞은 부위와 무관하다. `KnockbackStrength`에는 약점 배율을 걸지 않는다.
- **플린치·피격 주시 타이머:** 이미 "맞았다"는 사실만으로 정해진다. 기존 시간을 유지한다.
- **Actor 물리 HitReact:** 약점 배율이 반영된 피해를 기존 경로 그대로 전달한다(사용자 결정, 2026-11-05). `FLNPApplyDamageGECommand::Damage` → HitReact 큐 `RawMagnitude` → `QueueHitReact`의 피해 기반 세기 계산을 유지하며, 반응용 원시 피해를 별도로 분리하지 않는다. 실제 세기는 기존 `Clamp(Damage / RefDamage, 0.5, 2)`와 근접 배율을 거치므로 항상 정확히 1.5배가 되는 것은 아니다. PIE에서 육안으로 확인하고 어색하면 그때 별도 대응한다. 순수 엔티티의 기존 플린치에는 새 물리 반응을 추가하지 않는다.

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
- **클라이언트 예측 무변경.** D2의 몸 명중 게이트 덕분에 몸 명중 집합이 바뀌지 않아 Ghost 소멸·로컬 착탄 VFX·근접 로컬 HitStop이 지금과 똑같이 맞는다.
- **약점 전용 연출(소리·히트마커·VFX 변형)은 만들지 않는다**(사용자 결정). 사수 화면 착탄 VFX는 로컬 Ghost 판정이 내고 서버 큐는 중복 차단되므로, 나중에 연출을 넣는다면 그 구조부터 다시 봐야 한다는 점만 남겨 둔다([TechDesign_HitDetection.md](TechDesign_HitDetection.md) §4).

---

## 3. 데이터 모델

### 3.1 `ULNPEnemyConfig` (Category `LNP|Collision|WeakSpot`)

기존 판정 몸체 필드(`HitCenterUpOffset`·`HitCapsuleRadius`·`HitCapsuleHalfHeight`)와 같은 평면 필드로 둔다.

| 필드 | 형 | 기본값 | 의미 |
|:---|:---|:---|:---|
| `WeakSpotRadius` | float (cm, ClampMin 0) | 0 | **0이면 약점 없음** — 이 적은 1단계만 쓴다 |
| `WeakSpotLocalOffset` | FVector (cm) | (0,0,0) | 판정 캡슐 중심 기준 몸 로컬 오프셋. X=전방·Y=우측·Z=Up(구 내벽 Up) |

- `#if WITH_EDITOR` `IsDataValid` — D2의 겹침·돌출 배치 검사 위반 시 `EDataValidationResult::Invalid`와 수치가 든 오류 메시지(영어). 판정 캡슐 치수는 `GetHitCapsuleRadius/HalfHeight()`로 읽는다.
- `LNPEnemyConfig.cpp`가 아직 없으므로 새로 만든다. 헤더 변경이라 Live Coding이 아니라 에디터를 닫고 전체 빌드한다.

### 3.2 `ALNPPlayerCharacter` (`UPROPERTY(EditDefaultsOnly)`, Category `LNP|Collision|WeakSpot`)

플레이어에는 Config 에셋이 없고 판정 캡슐도 캐릭터의 `CapsuleComponent`(네이티브 서브오브젝트, 기본 42/96)다. 그래서 약점 치수를 **캡슐과 같은 클래스 기본값**에 둔다 — 캡슐을 고치는 사람이 같은 자리에서 머리 구를 본다.

| 필드 | 기본값 | 의미 |
|:---|:---|:---|
| `WeakSpotRadius` | 40 (검증 전 후보) | 0이면 약점 없음. 머리 부위의 캡슐 표면을 덮도록 보정 |
| `WeakSpotLocalOffset` | (0, 0, +78) | 캡슐 중심 기준. 플레이어 판정 Up은 위치에서 나오는 구 내벽 Up이라 Z만 쓴다 |

- `#if WITH_EDITOR` `IsDataValid` — `GetCapsule()`의 스케일 치수로 같은 겹침·돌출 배치 검사를 한다(BP CDO의 캡슐 템플릿 값이 들어온다).
- 수집 패스(워커 스레드)는 이미 `PlayerPawn->GetCapsule()`의 치수를 읽고 있다. 같은 자리에서 이 두 값을 읽는다(불변 기본값이라 경합이 없다).

### 3.3 초기값 (디버그 드로우로 메시에 맞춰 보정한다)

| 대상 | 판정 캡슐 | 약점 초기값 (검증 전 후보) | 표면 덮기 기준 |
|:---|:---|:---|:---|
| 플레이어 (`ALNPPlayerCharacter` BP) | HalfHeight 96 · Radius 42 | (0, 0, +78), 반지름 40 | 중심 높이의 캡슐 표면 반경은 약 34.5. 해당 높이 둘레를 구가 덮고 정면·측면·후면·위쪽 접근 검증 |
| `DA_Enemy_PureEntity_Melee01` · `_Ranged01` · `DA_Enemy_ActorPromoted_Melee01` (인간형) | 실제 에셋 HalfHeight 96 · Radius 42(2026-11-06 확인) | (0, 0, +78), 반지름 40 | 실제 캡슐이 플레이어와 같아 같은 초기값을 입력했다. 중심 높이의 표면 반경은 약 34.5. 대표 접근 방향·메시 정합은 PIE에서 확인 |
| `DA_Enemy_PureEntity_Flyer01` (드론) | `HitCapsuleRadius` 64 | (+39, 0, 0), 반지름 32 | 정면 캡슐 표면(+64)까지 25. 센서 접근 방향의 첫 몸 접촉을 덮도록 보정 |
| `DA_Enemy_PureEntity_Crawler01` (지상 드론) | HalfHeight 78 · Radius 78 · HitCenterUpOffset -40 | (0, 0, +38), 반지름 44 | 메시 꼭지 Z=-2에 구 중심을 맞춘다. 몸 판정 구 윗면이 꼭지보다 40cm 높아 이를 덮도록 반지름 44를 입력했다. 위쪽 첫 접촉과 옆 몸통 구분은 PIE에서 확인 |

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

/** 활성 구가 캡슐과 겹치고 돌출하는지 검사한다(D2). 반지름 0은 약점 없음으로 허용한다. */
bool IsWeakSpotPlacementValid(float CapsuleHalfHeight, float CapsuleRadius,
                              const FVector& LocalOffset, float WeakSpotRadius);

/** 투사체용. 몸 명중이 확정된 뒤에만 부른다. 구는 반높이 = 반지름인 캡슐이라 SegmentHitsCapsule을 그대로 쓴다. */
ELNPHitZone ClassifyProjectileHitZone(const FPreparedSegment& Segment, const FVector& WeakSpotCenter,
                                      const FVector& UpDir, float WeakSpotRadius, float HitRadius);

float ScaleDamageForZone(float RawDamage, ELNPHitZone Zone);   // WeakSpot이면 × WeakSpotDamageMultiplier
```

- **기존 기하 함수를 재사용하되, 근접의 길이 0 축 처리를 수정한다.** `SegmentHitsCapsule`은 이미 "구형 캡슐(축 길이 0)"을 정확히 처리한다(`LNP.HitDetection.SegmentCapsule.*` 테스트가 덮는다). 근접의 Swept Quad 경로는 구의 축이 점이면 삼각형-점 거리가 된다. 다만 첫 프레임·칼끝 이동량 1cm 미만의 `CalcDistSq` 폴백이 현재 쓰는 `FMath::SegmentDistToSegment`는 길이 0인 두 번째 선분에서 칼날 시작점까지의 거리로 잘못 계산할 수 있다. 이 호출은 `FMath::SegmentDistToSegmentSafe`로 바꾸고 실제 폴백 경로의 회귀 테스트를 추가한다(§4.2·§9.1). 근접 분류는 같은 파일에서 `CalcDistSq`를 한 번 감싼다.
- **약점 분류는 `HitPoint`를 받거나 바꾸지 않는다.** 구 검사의 최근접점 출력은 헬퍼 내부 임시 변수로만 받는다. 기존 몸 판정의 최근접점을 피해·임팩트·`FinishHit`·스플래시 중심에 그대로 사용한다. 표면 진입점을 새로 계산하거나 몸 최근접점의 구 포함 여부로 분류하지 않는다. 분류는 같은 선분·칼날 궤적과 구의 접촉 여부다.
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
               Enemy.UpDir, Enemy.WeakSpotRadius, HitRadius);
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
               Player.UpDir, Player.WeakSpotRadius, HitRadius);
Effects.Damage.Emplace({ ..., ScaleDamageForZone(Shared.Damage, Zone), ... });
LNPHitGrace::Dispatch(...);   // 기존 그대로
```

- 되감기·격자·광역 단계·후보 순서("수집 순서의 첫 명중")·Judge 입력은 **바뀌지 않는다.**
- 패링 반사탄(팀이 방어자 팀으로 바뀐 탄)도 같은 분기를 지나므로 머리에 맞으면 약점이다 — 별도 처리 없음.
- 계측 경로(`[RewindLag]`, `ValidateSpatialGrid`)는 몸 판정 그대로 둔다.

### 4.2 근접 (서버, `ULNPWeaponTraceHitDetectionProcessor`)

같은 파일의 `CalcDistSq` 람다를 감싸는 분류 람다를 하나 둔다. 적·플레이어 두 분기가 이것만 부른다.

선행 수정: `bDegenerate` 폴백의 칼날 선분 ↔ 캡슐 축 거리 계산을 `FMath::SegmentDistToSegmentSafe`로 교체한다. 약점 구의 축은 길이 0이므로 이 수정 없이는 칼끝이 머리를 관통해도 Body로 오분류할 수 있다. 이 수치 계산 오류는 구가 돌출해도 별도로 수정해야 한다.

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
- **스윙당 첫 몸 접촉이 부위를 정한다.** 그 프레임의 칼날 궤적이 약점 구에도 닿으면 WeakSpot, 아니면 Body로 확정하고 `AlreadyHit`을 기록한다. 의도한 머리 접근은 D2의 표면 덮기로 같은 프레임에 구에도 닿게 한다. 몸통을 먼저 친 뒤 이후 프레임에 머리를 지나는 공격은 다시 판정하거나 추가 피해를 주지 않는다. 구에만 닿은 프레임에는 `AlreadyHit`을 기록하지 않는다.
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
| 유탄 | 직격 대상만 분류. 몸 판정 `HitPoint`를 유지하므로 폭발 중심·스플래시 감쇠는 기존 그대로. 직격 대상 스플래시 제외도 유지 |
| 패링 반사탄 | §4.1 — 같은 경로라 자동 적용 |
| 패링·가드·회피·저스트 회피 | 무변경 — Judge는 몸 기하만 보고, 분류는 `Hit` 결과에서만 한다 |
| 피격 유예(`LNPHitGrace`) | 무변경 — 배율이 반영된 피해가 `FLNPPlayerHitEffects`에 담겨 보류·해제된다. 소급 회피되면 통째로 사라지는 것도 같다 |
| 경직·그로기 | 무변경(D4) |
| Actor 물리 HitReact | 약점 배율 반영 피해를 기존 세기 계산에 전달. 기존 클램프·근접 배율 유지, PIE 육안 확인 후 필요 시 재검토(D4) |
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

**완료(2026-11-06, 커밋 `e3fa788`).** 데이터·배치 검증·공용 헬퍼와 자동화 테스트를 추가했다. `LootNPopEditor Win64 Development` 전체 빌드 성공, `HitZone` 6개 + 기존 `SegmentCapsule` 3개 모두 통과(테스트 경고·오류 0, 종료 코드 0). 보고서: `Saved/Automation/2026-11-06_HitZoneUnit01_Final/index.json`. 이 단위 완료 당시에는 적 에셋의 반지름이 0이었고, 실제 약점 값 입력·피해 연결·PIE 검증은 단위 2에서 진행했다. `MeleeDegenerate`는 단위 3에서 추가한다.

| 파일 | 작업 |
|:---|:---|
| `Enemy/LNPEnemyConfig.h` · 신규 `Enemy/LNPEnemyConfig.cpp` | §3.1 필드 2개, `IsDataValid` |
| `Character/LNPPlayerCharacter.h/.cpp` | §3.2 필드 2개, `IsDataValid` |
| `Config/LNPSettings.h` | §3.4 `WeakSpotDamageMultiplier` |
| `HitDetection/LNPHitDetectionShared.h` | §3.5 `ELNPHitZone`·헬퍼 4개 |
| 신규 `Tests/LNPHitZoneTest.cpp` | §9.1 자동화 테스트 |

완료 기준: 빌드 성공, 신규 테스트와 기존 `LNP.HitDetection.SegmentCapsule.*` 통과. 판정 프로세서가 아직 필드를 읽지 않으므로 게임 동작 변화 없음.

### 단위 2 — 투사체 직격 (적·플레이어 피격자) + 디버그 + 에셋 값

**마감(2026-11-06).** 몸 명중 뒤 적·플레이어 직격 피해에 배율을 연결했다. 플레이어는 Judge의 피격 분기에서만 분류하며, 배율 피해가 기존 피격 유예 묶음에 들어간다. 스플래시·몸 HitPoint·경직·이동 넉백·예측·계측 경로는 유지했다. 노란 약점 구와 `LNP.HitZone.Log`(기본 0)를 추가했다. 인간형 3종은 실제 캡슐 96/42에 맞춰 +78Z/40, 비행 드론은 +39X/32, 지상 드론은 +38Z/44를 저장했다. 플레이어 BP는 네이티브 기본 +78Z/40을 상속하므로 재설정하지 않았다. 수정 전 에셋 백업은 `Saved/HitZoneUnit02/BeforeAssetChanges/`에 있다. 전체 빌드 성공, 에디터 MCP로 `LNP.HitDetection.*` 12/12 통과(테스트 경고·오류 0), 결과는 `Saved/Automation/2026-11-06_HitZoneUnit02/Results.json`과 `CrawlerResults.json`이다. 아래 1인·2인 PIE 주요 검증과 한계를 기록하고, 사용자의 세션 종료 지시에 따라 코드·에셋·문서를 함께 커밋한다. 유탄 폭심 동일성 A/B 등 미계측 범위는 §12.3에 남기며 전체 기능 완료로 표시하지 않는다.

- 1인 PIE 1차(2026-11-06 20:38~20:40 KST): 사용자 보고는 종류별 머리 사격, 인간형 구가 몸 캡슐과 비슷한 크기로 보이며 지상·비행 드론 구는 보이지 않음이다. 로그는 적 WeakSpot 6건/Body 2건, 플레이어 WeakSpot 3건/Body 9건이다. 적 원시 피해 20→30과 순수 엔티티·Actor GE 전달을 확인했다. 적 종류 식별자가 로그에 없어 종별 성공으로 확정하지 않는다. 짧은 세션의 플레이어 약점 빈도는 밸런스 통과 판정에 쓰지 않는다.
- 표시 점검: 이 시점의 Crawler는 약점 추가 전이라 반지름 0이었다. Flyer는 +39X/32가 저장돼 있으나 기존 적 디버그 거리 제한이 5m(`DebugDrawProximityDistSq=250000`)라 교전 거리에서 보이지 않을 수 있었다. 추가 육안 검증을 위해 에디터의 설정 CDO만 임시 25m(`6250000`)로 변경했다. ini 파일은 저장하지 않았으며 이후 `250000`으로 복원했다. 이 표시 거리 조절은 약점 판정 형상을 바꾸지 않는다.
- 지상 드론 약점 추가(2026-11-06 사용자 결정): 비행 드론 구 확인 후 지상 드론 정수리 꼭지에도 약점을 요청했다. 기존 약점 없음 결정을 변경해 Crawler에 +38Z/44를 저장했다. 메시 bounds Z=-78~-2와 판정 중심 -40을 읽어 꼭지 높이를 맞췄다. 벽·천장의 몸 Up을 기존 공용 헬퍼로 따르므로 코드 변경은 없다. 위쪽 캡슐 표면까지 거리가 40cm여서 반지름을 44cm로 정했으며, 작은 메시 꼭지만큼 좁은 영역을 의미하지는 않는다. 에셋 배치 검사를 포함한 자동화 12/12 재통과(경고·오류 0, `Saved/Automation/2026-11-06_HitZoneUnit02/CrawlerResults.json`). 새 배치의 사용자 PIE 확인 결과는 다음 항목에 기록했다.
- 지상 드론 추가 후 1인 PIE(2026-11-06 21:06 KST): 사용자가 정수리·옆 몸통 확인을 보고했다. 해당 로그 구간에서 적 Body 원시 피해 20→20·HP 100→81.8, WeakSpot 20→30·HP 81.8→54.5를 확인했다. 약점/몸통 피해 차이는 반올림을 감안해 1.5배다. 적 종류 식별자는 없으므로 로그 단독으로 Crawler 식별을 증명하지는 않으며 사용자 조작 보고와 함께 기록한다. PIE 종료 뒤 임시 표시 거리는 원래 `250000`(5m)으로 복원·재조회했고 ini 저장은 없다. 산탄·유탄·2인 지연/PvP/피격 유예는 미검증이며 단위 2 완료로 표시하지 않는다.
- 2인 PIE 1차(2026-11-06 21:10:03~21:15:47 KST): 사용자는 게스트 단발·양방향 PvP·가드/패링·산탄·유탄까지 먼저 수행했고 인위적 지연·피격 직전 대시는 아직 수행하지 않았다고 보고했다. `LNP.Team.Set 1 2`의 팀 1→2 성공을 확인했다. 분류 79건 모두 Body ×1 / WeakSpot ×1.5로 배율 불일치 0이다. 적 단발 20→20/30, PvP도 두 플레이어 방향 모두 20→20/30이며, 게스트 피격은 약점 97ms·몸통 127ms 보류 뒤 GE에 각각 30/20을 유지했다(HP 100→97.3→95.5). 산탄 원시 피해 15는 적 Body 40건/WeakSpot 12건, 플레이어 Body 6건/WeakSpot 4건으로 펠릿별 15/22.5가 혼재한다. 가드 성공 39건·투사체 패링 성공 8건을 확인했다. 건수는 발사 횟수가 아니라 펠릿 단위 효과를 포함한다.
  - 유탄은 런처 장착·트레일/폭발 VFX 로드와 사용자 수행 보고가 있으나, 현재 로그에 무기 식별자·순수 엔티티 스플래시 HP 로그가 없어 유탄 직격/스플래시의 개별 피해와 폭심 불변을 정량 통과로 확정하지 않는다. 코드 검토상 `ApplySplash`는 기존 `Shared.Damage`와 몸 `HitPoint`를 유지하며 이번 diff에서 변경하지 않았다.
  - `Error:` 등급 로그는 0이지만 산탄 `Multicast_SpawnGhostProjectiles` RPC 분할 경고 60건, 카메라 평가 컨텍스트 재생성 경고, NetworkPrediction 입력 부족/복구·롤백 경고 및 PIE 종료 시 `ClientSetViewTarget` 복제 경고가 있다. 이 세션만으로 부위 배율 변경과의 인과를 단정하지 않는다. 인위적 지연 입력은 없고 소급 회피 `Retro`는 0건이다.
  - 원본 구간과 분석은 `Saved/Automation/2026-11-06_HitZoneUnit02/PIE_NoLag/Session.log`, `Analysis.json`에 보관했다. 이 시점에는 단위 2 검증 중이었으며 이후 2차 PIE를 진행했다.
- 2인 PIE 2차(2026-11-06 21:30:59~21:35:39 KST): 사용자가 유탄 재시험과 `net.PktLag 50` 저스트 회피 시도를 보고했다. 유탄 원시 피해 60의 Body 3건(60 유지)·WeakSpot 4건(90)을 확인했다. 순수 엔티티·Actor GE 양쪽에 배율 피해가 전달됐다. 런처 사용 구간의 직격 분류 없는 GE 피해 18.0, 약점 직격 90과 같은 프레임의 다른 대상 GE 피해 1.9는 스플래시 감쇠 경로와 일치한다. 무기 사용 시점·주변 로그에 따른 귀속 추론이며, 로그에 폭심·대상 거리·공격 식별자가 없어 같은 몸 명중의 약점 반지름 변경 A/B를 정량 입증한 것은 아니다.
  - `PktLag set to 50` 두 줄(21:34:34·21:34:37), 이후 게스트 RTT 약 162~192ms와 피격 유예 약 166~181ms를 확인했다. 지연 적용 이후 `Retro JustDodged` 8건, `Retro Dodged` 22건이다. 저스트 소급 성공의 보류 시간은 35~111ms이며, 22건은 후속 펠릿 등을 포함한 피격 묶음 수다. 전체 세션 기준은 저스트 성공 13건·소급 처리 31건으로 지연 전 성공도 포함하므로 구분한다.
  - 21:34:51에는 같은 프레임의 약점 22.5 펠릿 2개와 몸통 15 펠릿 4개를 분류한 뒤, 약 72ms 보류 상태에서 저스트 1개·일반 회피 5개로 소급 취소했다. 해당 프레임 묶음의 GE 피해 적용 로그가 없고 기존 `Held.RemoveAt` 경로와 일치한다. 전체 분류 103건의 배율 불일치는 0이다.
  - 별도 발견: 21:35:23 게스트 사망 후 21:35:33 새 폰 `C_2`가 생성되는 구간에서 NetRefHandle 756 객체/함수 해석 실패로 RPC 4개가 거부됐다(`Error:` 8줄). 함수명은 `unknown`이라 로그만으로 RPC 종류와 부위 피해 변경과의 인과를 확정하지 않았다. 산탄 Ghost 스폰 RPC 분할 경고는 78건이다. 오류 내용은 보존하며, 마감 시 사용자가 확인한 기존 동일 현상 수용 결정은 §11 Q8을 따른다.
  - 원본 구간과 단계별 분석은 `Saved/Automation/2026-11-06_HitZoneUnit02/PIE_PktLag50/Session.log`, `Analysis.json`에 보관했다. 이번 턴은 로그 분석만 수행했고 코드·에셋 추가 변경이나 단위 3 착수는 없다.

| 파일·에셋 | 작업 |
|:---|:---|
| `HitDetection/LNPProjectileProcessors.cpp` | §4.1 수집 필드·적/플레이어 분류·배율, §7 디버그 드로우·로그 CVar |
| `DA_Enemy_PureEntity_Melee01` · `_Ranged01` · `DA_Enemy_ActorPromoted_Melee01` · `DA_Enemy_PureEntity_Flyer01` · `DA_Enemy_PureEntity_Crawler01`, 플레이어 캐릭터 BP | §3.3 초기값 입력 → 디버그 드로우로 메시에 맞춰 보정 (Unreal MCP 또는 사용자 편집) |

완료 기준: §9.2 #1~#6, #9(투사체) 통과, #12(투사체) 육안 확인, 저장 시 `IsDataValid` 오류 0.

### 단위 3 — 근접 (적·플레이어 피격자)

**미착수 · 새 세션으로 이관(2026-11-06 사용자 지시).** 현재 근접은 약점 배율을 적용하지 않는다. 구현 시작점·검증 순서·재조사하지 않을 항목은 §12를 먼저 읽는다.

| 파일 | 작업 |
|:---|:---|
| `HitDetection/LNPWeaponTraceProcessors.cpp` | §4.2 길이 0 축에 안전한 폴백으로 교체·회귀 테스트, 분류 람다, 적 분기·`JudgePlayerTarget`의 되감긴 중심 공유와 배율, 로그 |

완료 기준: §9.1 `MeleeDegenerate`와 §9.2 #7~#10 통과, #12(근접) 육안 확인.

---

## 9. 검증 계획

### 9.1 자동화 테스트 (`LNP.HitDetection.HitZone.*`)

| 테스트 | 확인 내용 |
|:---|:---|
| `Placement` | `IsWeakSpotPlacementValid`: 겹치며 돌출한 구는 허용, 내부에 파묻힌 구·내접 구·분리 구·외접만 하는 구는 거부. 반지름 0·구형 캡슐·음의 Z 오프셋 포함 |
| `Classify` | 머리·가슴·정수리 스침의 WeakSpot/Body. 돌출 구만 닿고 몸에는 빗나간 선분은 최종 명중을 만들지 않음. 몸 명중 게이트와 분류를 함께 검증하고, 분류 전후 기존 몸 `HitPoint` 불변 확인 |
| `FirstContact` | 머리의 대표 접근 방향에서 캡슐에 처음 닿는 짧은 선분도 구에 닿아 WeakSpot. 서로 다른 선분 분할 위치·작은 이동 간격·투사체 반경으로 내부 구 시절의 첫 접촉 누락이 재발하지 않는지 확인 |
| `MeleeDegenerate` (단위 3) | 근접의 실제 정지·저속 칼날 폴백 경로를 검증. 몸 중심 0·HalfHeight 96·Radius 42, 약점 중심 (0,0,78)·반지름 40, 칼날 (−100,0,78) → (100,0,78)·반지름 5·Prev=Curr에서 WeakSpot. 칼날 끝점 순서 반전·구에 닿지 않는 Body 사례도 확인 |
| `OffsetRotation` | 드론형 전방 오프셋이 Yaw 90°·임의 Up에서 올바른 월드 위치로 돌아감 |
| `DamageScale` | `ScaleDamageForZone` 1.0/1.5배, `ApplyDefense` 뒤 최종 비율도 1.5 |
| `ConfigAssets` | 적 에셋 5종과 플레이어 BP CDO를 로드해 겹침·돌출 배치 검사 통과(에셋 값이 바뀌어도 회귀를 잡는다) |

근접의 길이 0 축 폴백은 `MeleeDegenerate`로 회귀를 막고, 스윙 전체·방어 판정과의 연계는 PIE(#7·#8)로 확인한다.

### 9.2 PIE (1인 → 2인)

검증 편성은 기본 `DA_MassSpawnConfig`를 건드리지 않고 별도 에셋 + `LNP.Spawn.ConfigOverride`로 띄운다. 모든 항목에서 `LNP.HitZone.Log 1`.

| # | 시나리오 | 통과 기준 |
|:---|:---|:---|
| 1 | 디버그 드로우로 인간형 적(순수 엔티티·승격)·드론·플레이어의 노란 구가 머리·센서에 겹치는지 육안 | 겹친다. 승격 적·플레이어의 공격·피격 모션 중 머리가 구를 벗어나는 정도를 기록(수용 범위 판단) |
| 2 | 단발 무기로 같은 적의 머리/몸통을 각각 쏜다 | zone 정확, HP 감소량 비율 1.5 (순수 엔티티·Actor 둘 다) |
| 3 | 비행 드론 정면 센서 / 측면 몸체, 지상 드론 정수리 꼭지 / 옆 몸통 | 지정 부위만 WeakSpot. 비행 드론 회전·지상 드론 벽·천장 이동 시 구가 몸을 따라간다 |
| 4 | 산탄 근거리 머리 조준 | 펠릿별 zone 혼재, 총 피해 증가 |
| 5 | 주변 대상이 있는 유탄 직격 vs 바닥 착탄 | 직격만 분류. 구 크기를 바꿔 약점 여부가 달라져도 같은 몸 명중의 폭발 중심·주변 스플래시 피해는 무변경 |
| 6 | 2인 PIE 게스트가 적을 사격(+`NetEmulation` 지연) | 게스트가 머리를 맞히면 서버 로그도 WeakSpot. Ghost 소멸·착탄 VFX가 지금과 다르지 않다 |
| 7 | 롱소드 수평 베기 / 내려베기로 적 공격 | 첫 몸 접촉 프레임의 궤적으로 분류·스윙당 1회·HP 비율 1.5. 구만 스친 뒤 몸에 닿는 경우와 몸통을 친 뒤 머리를 지나는 경우도 확인 |
| 8 | 2인 PIE `LNP.Team.Set`으로 팀 분리 → PvP 사격·근접, 호스트·게스트 양방향 | 머리 명중 시 플레이어 HP 감소 1.5배. 가드 정면이면 피해 없음(분류 안 함), 패링·회피 결과 무변경 |
| 9 | PvP에서 원격 게스트 피격자 + 피격 유예 창 안의 소급 회피 | 보류 후 해제된 피해가 배율 값 그대로, 소급 회피 시 피해 없음 |
| 10 | 적 무리(근접·원거리·승격)와 1~2분 교전 | 적 → 플레이어 `zone=WeakSpot` 줄 수 / 전체 피격 수를 기록 — 높으면 D7 재검토 |
| 11 | 회귀 | Body 피해 무변경, 반지름 0 비활성 분류는 자동화 회귀로 확인. `ProjectileProfile` 상세 계측에서 몸 캡슐 판정 횟수 무변경 |
| 12 | 승격 적·플레이어의 Body/WeakSpot 물리 HitReact 비교 | 약점 피해가 기존 피해 기반 세기 계산으로 전달되는지 확인하고 육안 인상을 기록. 세기가 항상 1.5배일 것을 요구하지 않으며, 어색하면 별도 대응(D4) |

---

## 10. 한계·수용

- **애니메이션을 따라가지 않는다.** 숙이기·돌진·공격 모션에서 머리가 몸 축 꼭대기를 벗어나면 구와 어긋난다. 플레이어도 같다(행글라이더 활공 중 메시 자세가 바뀌어도 판정 캡슐·구는 구 내벽 Up을 따른다). 러프 2단계의 수용 범위이고, PIE #1에서 정도를 기록한다.
- **회전은 되감지 않는다.** 원격 사수의 되감기(최대 0.5초) 동안 드론이 크게 돌면 센서 위치가 현재 회전 기준이 된다. 인간형(적·플레이어)은 Z 오프셋만 써서 해당 없다. 문제가 되면 위치 이력에 회전(또는 Yaw 하나)을 기록한다.
- **한 프레임 선분이 몸통을 지나 머리까지 닿으면 약점으로 친다.** 아래에서 올려 쏘는 등 선분이 길고 가파른 경우다. 진입 순서까지 보려면 선분 위 진입 매개변수(구·캡슐 각각의 최초 교차 t)를 비교해야 하는데, 지금 판정 함수는 진입점이 아니라 최근접점을 돌려준다 — 러프 단계에서는 하지 않는다.
- **첫 접촉 문제는 약점 구의 배치로 보완한다.** D2처럼 의도한 약점 부위의 캡슐 표면을 구가 덮게 한다. 한쪽으로만 돌출해 다른 접근 방향의 표면이 드러나 있으면 첫 접촉이 Body가 되는 문제가 남으므로 대표 방향을 검증한다. 이후 프레임의 약점 접촉을 기다리거나 피해를 추가 지급하지 않는다.
- **근접 약점은 모션이 정한다.** 머리 높이를 지나는 모션이 보너스를 자주 받는다 — 사용자 결정으로 수용(D6).
- **돌출 구만 닿는 공격은 빗나감이다.** 약점 구는 명중 범위를 늘리지 않으므로 캡슐 밖의 보이는 머리만 스친 공격을 구만으로 명중시켜 주지는 않는다. 필요하면 몸 판정 캡슐을 별도로 조정한다.
- **승격 적에 전방 오프셋을 쓰면** 엔티티 회전이 Actor 회전과 동기되는지 먼저 확인해야 한다. 지금 승격 적은 인간형뿐이라 Z 오프셋만 쓴다.

---

## 11. 확정된 결정 (2026-11-05~06, 사용자)

| # | 질문 | 결정 |
|:---|:---|:---|
| Q1 | 근접에도 적용하나? | **적용한다.** 근접은 플레이어 실력과 무관한 보너스가 되지만 그 정도는 인정한다(D6) |
| Q2 | 판정 순서·구 배치 | **몸(캡슐) 먼저 + 약점 부위의 캡슐 표면을 덮는 돌출 분류 구**. 포함 제약은 폐기하고 몸 명중 때만 같은 선분·칼날 궤적으로 구를 검사한다(D2) |
| Q3 | 플레이어도 피격 부위를 갖나? | **갖는다.** 플레이 중 팀이 나뉘면 PvP가 되고 헤드샷 보너스가 필요하다(D7). 부위는 피격자가 정하므로 적 → 플레이어에도 적용된다 |
| Q4 | 약점 명중 피드백 | **만들지 않는다.** 세게 들어간 피해 자체가 피드백이다(D8) |
| Q5 | Actor 물리 HitReact의 피해 기반 세기에 약점 보너스를 반영하나? | **반영한다.** 기존 배율 반영 피해 전달과 세기 클램프를 유지하고, PIE 육안 확인 후 어색하면 그때 별도 대응한다(D4) |
| Q6 | 약점 분류가 `HitPoint`를 바꾸나? | **바꾸지 않는다.** 기존 몸 판정의 최근접점을 임팩트·피해·스플래시에 그대로 사용한다. 구는 같은 선분·칼날 궤적의 접촉 여부만 반환한다(§3.5) |
| Q7 | 지상 드론에도 약점을 두나? | **정수리 꼭지에 둔다.** 2026-11-06 요청. Crawler의 약점 없음 결정을 변경하며, 몸 로컬 Z 오프셋을 쓴다 |
| Q8 | 지연 시험 중 리스폰 RPC 오류를 먼저 고치나? | **이번 작업에서는 무시해도 된다.** 사용자가 이전 다른 세션에서 조사한 동일 현상으로 확인했다(2026-11-06). 새로 발생한 HitZone 회귀로 단정하거나 단위 3의 선행 작업으로 재조사하지 않는다 |

---

## 12. 새 세션 인수인계 (2026-11-06 마감)

### 12.1 현재 상태와 작업 경계

| 단위 | 상태 | 다음 세션에서의 취급 |
|:---|:---|:---|
| 1 데이터·헬퍼·테스트 | 완료, `e3fa788` | 기존 필드·헬퍼를 재사용한다 |
| 2 투사체 직격·디버그·에셋 | 구현·빌드·자동화·주요 PIE 검증 마감 | 수치·검증 근거는 §3.3·§8에 있다. 유탄 폭심 A/B 미계측은 아래에 구분했다 |
| 3 근접 | **미구현, 사용자 지시로 새 세션 이관** | §4.2·§8 단위 3을 구현하고 자동화·PIE 검증한다. 이번 세션에서 코드를 먼저 바꾸지 않았다 |

이번 마감 커밋은 투사체 코드·적 Config 5종·설계 및 현황 문서다. 플레이어 BP는 네이티브 기본값을 상속하므로 변경하지 않았고, 기본 스폰 편성·맵·ini도 변경하지 않았다. 작업 시작 전부터 수정돼 있던 Flyer·Crawler·Ranged 에셋의 기존 내용은 보존한 상태로 약점 값을 추가했다. 설계 문서의 돌출 구·물리 HitReact 결정도 기존 로컬 변경을 보존해 반영했다. `Saved/`의 백업·시험 보고서·PIE 원본 구간은 로컬 산출물로 커밋하지 않으며, 아래와 §8의 수치 요약이 저장소에 남는 검증 기록이다.

### 12.2 단위 3 구현 시작점

소스 경로는 모두 `Source/LootNPop/` 기준이다. 새 세션에서는 `CLAUDE.md`와 이 문서를 읽고 현재 코드를 다시 확인한다. 단위 3 자체의 구현은 이번 사용자 지시로 넘긴 작업이며, PIE/GUI가 더 빠르면 즉시 사용자에게 요청하고 다른 구현 단위를 임의로 이어가지 않는다.

1. `HitDetection/LNPWeaponTraceProcessors.cpp`의 서버 `FCollectedEnemy`·`FCollectedPlayer`에 `WeakSpotOffset`·`WeakSpotRadius`를 수집한다. 투사체 수집 패스와 같은 Config/Player 기본값·현재 몸 회전 헬퍼를 사용하고, Crawler의 `HitCenterUpOffset=-40`이 적용된 판정 중심 위에 약점 오프셋을 더한다. 스켈레톤 본 조회는 추가하지 않는다.
2. 서버의 `CalcDistSq`에서 정지·저속(`bDegenerate`) 칼날 폴백을 `FMath::SegmentDistToSegmentSafe`로 바꾼다. 현재는 길이 0인 구의 축에 안전하지 않은 `SegmentDistToSegment`가 남아 있다. **실제 런타임 폴백 경로**를 검증하는 `LNP.HitDetection.HitZone.MeleeDegenerate`를 `Tests/LNPHitZoneTest.cpp`에 추가한다. 필요하면 이 계산만 테스트 가능한 최소 공용 함수로 옮기되 전체 판정 구조를 리팩터링하지 않는다.
3. 같은 `CalcDistSq`를 감싼 `ClassifyMeleeHitZone`으로 구를 분류한다(§4.2). 적은 몸 명중 뒤, 플레이어는 `JudgePlayerTarget`의 `Hit` 분기에서만 호출한다. 몸 판정에 사용한 되감긴 중심을 변수로 꺼내 재사용하고, 배율을 `Frag.Damage`에 적용한 값을 적 Actor GE·순수 엔티티 HP·플레이어 피격 유예 묶음에 전달한다.
4. `AlreadyHit`·첫 몸 접촉·기존 임팩트 좌표·경직·이동 넉백·플린치·방어·클라이언트 로컬 HitStop 예측을 유지한다. 새 연출·복제 필드·약점 접촉 프레임 누적은 추가하지 않는다. 물리 HitReact는 기존 배율 피해 전달을 그대로 받는다.
5. `LNP.HitZone.Log`는 이미 `LNPProjectileProcessors.cpp` 익명 namespace에서 등록된다. 근접 로그는 이 CVar를 재사용하고 같은 이름으로 중복 등록하지 않는다. `path=Melee`·`victim=Enemy|Player`·`zone`·`raw/scaled`를 남긴다.

`.cpp`만 바꾸면 에디터의 Live Coding을 사용할 수 있다. 헤더 변경이 필요하면 저장·에디터 종료 후 전체 빌드한다. 샌드박스 밖 에디터 프로세스 확인 없이 닫혔다고 가정하지 않는다(이번 단위 1 첫 링크에서 DLL 점유로 실패했고, 사용자 종료 뒤 재빌드 성공). 전체 빌드는 `LootNPopEditor Win64 Development`와 루트 `AGENTS.md`의 권한 확장 규칙을 따른다.

### 12.3 검증 기준과 남긴 한계

- 현재 자동화는 `LNP.HitDetection.*` **12/12 성공**이다: HitZone 6개, SegmentCapsule 3개, SpatialGrid 2개, RewindCache 1개. 단위 3의 `MeleeDegenerate`는 아직 없다. 실제 폴백의 정지·저속·끝점 반전·Body 사례를 추가한 뒤 이 묶음 전체를 실행한다.
- 근접 PIE는 §9.2 #7~#10·#12(근접)를 확인한다. 플레이어→적(순수 엔티티·승격)·호스트/게스트 양방향 PvP·적→플레이어(승격 ANS·순수 엔티티 가상 칼날)를 모두 포함한다. 스윙당 1회·첫 몸 접촉의 부위 확정·가드/패링/회피·지연 중 피격 유예를 확인한다.
- 단위 2의 핵심 확인값은 단발/PvP 20→20/30, 산탄 펠릿 15→15/22.5, 유탄 직격 60→60/90이다. 양쪽 `PktLag 50` 설정 뒤 소급 저스트 8건·후속 피격 묶음 22건이 취소됐고 약점 펠릿도 포함했다. 마지막 2인 두 세션의 분류는 각각 79건·103건으로 배율 불일치 0이다.
- **정량 완료로 확대하지 않을 범위:** 약점 반지름만 바꾼 동일 몸 명중의 유탄 폭심·주변 스플래시 A/B, `ProjectileProfile` 몸 캡슐 횟수 실측, 모션 중 메시 머리 이탈량·물리 HitReact 인상의 별도 정량 기록, Crawler 벽·천장 전환의 전용 육안 기록, 적→플레이어 약점 빈도의 독립 1~2분 밸런스 표본은 없다. 단위 2에서는 기존 몸 `HitPoint`·스플래시 코드 유지와 주요 사용자 플레이까지 확인했다. 이 미계측 항목을 모두 PIE 통과로 바꾸거나 작업을 이미 했다고 적지 않는다.
- 리스폰 RPC 오류는 §11 Q8의 사용자 판단에 따라 이번 범위에서 수용한다. 오류 레벨·내용은 §8에 보존하지만 재조사를 새 세션의 선행 과제로 추가하지 않는다. 산탄 Ghost RPC 분할 경고도 관측했으며 이번 작업에서는 수정하지 않았다.

### 12.4 에디터·검증 편성과 로컬 산출물

- 마지막 확인 시 PIE는 정지돼 있다. 적 Config 5종은 저장했고 값은 §3.3과 같다. 임시 `DebugDrawProximityDistSq=6250000`(25m)는 **원래 `250000`(5m)으로 복원**했다. 따라서 멀리 있는 비행 드론의 노란 구가 안 보이는 것은 기존 표시 거리 제한일 수 있다. 표시 거리와 실제 판정 크기를 혼동하지 않는다.
- 기본 `DA_MassSpawnConfig` 대신 에디터 콘솔에서 `LNP.Spawn.ConfigOverride /Game/SpawnData/DA_MassSpawnConfig_TestOnePerSpecies.DA_MassSpawnConfig_TestOnePerSpecies`를 사용했다. 새 세션에서 필요하면 다시 설정한다. 검증 목적으로 기본 편성 에셋을 바꾸지 않는다.
- 분류 로그는 `LNP.HitZone.Log 1`, 회피 로그는 `LNP.Debug.Dodge 1`, 2인 팀 분리는 호스트의 `LNP.Team.Set 1 2`다. 지연을 쓰려면 두 창에 각각 `net.PktLag 50`, 지연 없는 비교는 두 창 각각 `net.PktLag 0`을 명시한다. 이번 시험의 자동 사격 명령 `LNP.Debug.ClientAutoAction`·`LNP.Debug.AuthorityAutoAction`은 마지막 로그에서 모두 0으로 종료했다. 콘솔 설정은 새 세션에서 확인하며 ini 변경으로 간주하지 않는다.
- 자동화: `Saved/Automation/2026-11-06_HitZoneUnit01_Final/index.json`, `Saved/Automation/2026-11-06_HitZoneUnit02/Results.json`, `CrawlerResults.json`.
- 2인 PIE 원본·분석: `Saved/Automation/2026-11-06_HitZoneUnit02/PIE_NoLag/` 및 `PIE_PktLag50/`의 `Session.log`·`Analysis.json`.
- 수정 전 에셋 백업: `Saved/HitZoneUnit02/BeforeAssetChanges/`. 플레이어 BP는 변경하지 않았고 Crawler는 추가 약점 요청 직전 상태도 백업했다. 로컬 산출물이 다른 체크아웃에 없더라도 §3·§8·이 인계를 기준으로 이어간다.
