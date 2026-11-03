# Phase 2 로그 — 저스트 회피 판정 + 버프 + 연출 경계

> 실행서: `phases/Phase02_JustDodge.md`

## 2026-11-03 — 단위 1·2 구현 (PIE 확인을 한 번으로 묶음)

사용자 결정으로 단위 1의 PIE 확인을 단위 2와 묶었다. 둘 다 풀 빌드가 필요해서(구조체 레이아웃, 새 `.cpp`) 에디터 재시작을 한 번으로 줄이기 위해서다.

### 변경

- `FLNPParryStateFragment`: `JustDodgeExpiryTime`, `bJustDodgeConsumed`.
- `LNPGuardParry`: `IsJustDodgeOpen`, `NeedsParryRange` 술어. `Judge`의 Parried 바로 뒤에 `JustDodged`.
- `ULNPCharacterMoverComponent::JustDodgeWindow`(0.12). 기록은 `Min(JustDodgeWindow, IFrameDuration)`.
- 근접: `JustDodged`면 `MarkHit` + 커맨드. 투사체 직격: `bInParryRange` 조건을 `NeedsParryRange`로 넓히고, `JustDodged`면 `DodgedEntity` 기록(관통) + 커맨드.
- 태그 `GameplayCue.LNP.JustDodge.Success`·`.Buff`.
- `ULNPSettings` `Combat|Just Dodge`: 지속 2.5, 공격속도 0.2, 이동속도 0.2.
- `ULNPGameplayEffect_JustDodgeBuff`(새 파일): `MultiplyAdditive` 두 개, 1중첩 + 적용마다 지속 리셋, `bSuppressStackingCues`, `.Buff` 지속 큐.
- `FLNPJustDodgeCommand`(`LNPHitDetectionShared.h`): 소비 플래그 확인·설정 → 버프 GE → `.Success` 큐 → `ReportJustDodgeSuccess`.
- 화면 문구 `JUST DODGE <공격> (<폰>)`(밝은 자홍, 1.5초)는 플레이 테스트에 필요해서 단위 3에서 앞당겼다.

### 실수와 수정 — 버프가 오히려 감속

- 첫 PIE에서 근접·직격 모두 성공했지만 이동속도가 크게 느려졌다.
- 원인: `MultiplyAdditive`는 배율 자체(항등원 1)를 받는다. 커맨드가 설정값 0.2를 그대로 SetByCaller에 넣어 ×0.2가 됐다. 아이템 스탯 경로(`LNPStat::ApplyModifiers`)는 항등원 1에서 누적하므로 문제가 없었다.
- 수정: `1.f + 설정값`을 넣는다. Live Coding으로 반영하고 `design/Architecture.md` §5.2에 주의를 적었다.

### 검증 (1인 PIE, TestMap03, 수정 후 재확인)

빌드: 게임 타깃 컴파일 통과 → 에디터 풀 빌드 통과 → 감속 수정은 Live Coding.

로그(`LNP.Debug.Dodge 1`) 판정:

- 대시(`IFrame open`) 약 50회, `JustDodge success` 21건(Projectile 13, Melee 8), `IFrame ignored` 14건.
- 모든 성공 줄이 바로 앞의 `IFrame open`과 같은 프레임이거나 1~2프레임 뒤다. **한 대시에 성공이 두 줄 찍힌 경우는 없다**(J-011, 대시당 1회).
- 2.5초 안에 연속 성공한 구간이 있다(13:41:06.517 Melee → 13:41:07.781 Projectile, 1.26초 간격). J-05 조건이 실제로 발생했다.
- PIE 구간에 ensure·error·AbilitySystem 경고가 없다.

| 시나리오 | 결과 |
|:---|:---|
| J-01 근접 | 통과(로그·화면 문구) |
| J-02 직격 | 통과(로그·화면 문구) |
| J-04 대시당 1회 | 로그상 통과. 다수 동시 공격을 의도해 연출한 테스트는 아니다 |
| J-05 연속 성공 | 통과(사용자 육안: MoveSpeed x1.20에서 늘지 않음) |
| J-06 스탯 탭 | 통과(사용자 육안) |
| J-07 공격속도 | 통과(사용자 육안) |

단위 1·2 완료.

## 2026-11-03 — 단위 3 연출 경계와 디버그

### 변경

- 빈 GCN 2개(`Content/GAS/GameplayCues/`). Unreal MCP `CreateCueNotifyAsset`으로 만든 뒤 부모를 바꿨다(이 도구는 Static/Actor 기본 클래스만 만든다).
  - `GCN_LNP_JustDodge_Success`: 부모 `ULNPGameplayCueNotify_VFXSound`, 필드 비움.
  - `GCN_LNP_JustDodge_Buff`: 부모 `GameplayCueNotify_Looping`, 비움. 도구가 Actor로 만들 때 붙인 BP 컴포넌트 1개가 남아 있다(`GCN_LNP_Status_Burn`은 0개). 비어 있는 씬 컴포넌트라 무해하고, 비주얼 트랙(V3)에서 오라를 붙일 때 정리한다.
  - 부모를 바꿔도 태그는 유지된다. 에셋 레지스트리 `GameplayCueName`이 각각 `GameplayCue.LNP.JustDodge.Success`·`.Buff`다.
  - ⚠️ `GameplayCueToolset.GetCueInfo`·`FindCueNotifyAssets`는 기존 `GCN_LNP_Status_Burn`까지 "에셋 없음"으로 답한다. 바인딩 확인에 쓸 수 없다. 레지스트리 태그(`AssetTools.get_asset_tags`)로 확인한다.
- 창 열림 로그·문구에 저스트 창 길이를 붙였다(`just=`, `JUST 0.12s`). `ReportDodgeWindowOpened`에 인자를 추가했다.
- `[Dodge] Ghost impact skipped player=<폰>`(클라, 탄 하나당 1줄) — Phase 1 D-07 부분 통과를 확정하기 위한 로그.
- `design/Architecture.md` §7, `design/Validation.md` §6의 문자열을 구현에 맞췄다.

빌드: Live Coding 통과. 새 문자열의 실제 출력 확인은 단위 4의 리슨 2인 PIE에서 함께 한다(Ghost 로그는 게스트가 있어야 나온다).

## 2026-11-03 — 단위 4 검증 (리슨 2인 PIE, `DA_MassSpawnConfig_TestOnePerSpecies`, 게스트 팀 2)

사용자가 게스트 화염·폭발(J-03)을 빼고 나머지를 수행했다. 사용자 체감: 이상 없음, 버프 경계에서 보정 캡슐이 체감되지 않음(J-08).

로그 집계(PIE 시작 13:51:01 이후, `LNP.Team.Set 1 2` 확인):

| 항목 | 값 |
|:---|:---|
| `IFrame open` | 143줄. 호스트 `BP_LNPPlayer_C_0` Authority 10. 게스트는 서버 Authority 53 / 게스트 Autonomous 60, 리스폰 뒤 폰 10 / 10 |
| `JustDodge success` | 27건. 호스트 Melee 3·Projectile 1, 게스트 Melee 8·Projectile 15 |
| 대시당 중복 성공 | 0건 (J-011) |
| 성공 시점 − 서버 창 열림 | 0.004~0.121초. 저스트 창 0.12초 + 커맨드가 프레임 끝에 도는 지연과 맞는다 |
| `IFrame ignored` | 게스트 Projectile 2건 (저스트 회피를 소비한 뒤 같은 대시에서 들어온 탄 — 의도대로 `Dodged`로 떨어짐) |
| `Ghost impact skipped` | 게스트 19건 |
| `[Guard] Block success` | 15건 (가드 회귀) |
| ensure·error·AbilitySystem 경고 | 없음 |

- 새 문자열이 문서대로 찍힌다: `IFrame open ... just=0.12`, `Ghost impact skipped player=...`.
- **D-07 확정:** 게스트 Ghost가 무적 중 예측 임팩트를 생략한 줄이 19건 있다. Phase 1의 부분 통과를 통과로 올린다.
- 게스트 Autonomous 60 대 서버 Authority 53의 차이 7은 `LNP Dash Cooldown Modifier was ended on BP_LNPPlayer_C_1 after a rollback` 7건과 개수가 같다. 게스트가 예측으로 대시했지만 서버는 대시하지 않은 경우다(Phase 1 로그 단위 4에 기록된 현상과 같다). 서버가 판정 권위이므로 결과는 맞다. 게스트 쪽에서 무적 창 값이 롤백되지 않아 Ghost 예측 임팩트가 0.15초 동안 빠질 수 있는 것은 Phase 1에서 받아들였다.

| 시나리오 | 결과 |
|:---|:---|
| J-01·J-02 (호스트·게스트) | 통과 |
| J-03 화염·폭발 | **미수행.** 스플래시·화염 경로는 `bInParryRange=false`를 넘기므로 `JustDodged`가 구조적으로 나올 수 없고, 이 Phase에서 두 경로 코드는 바뀌지 않았다 |
| J-04 | 통과(대시당 중복 0) |
| J-05~J-07 | 1인에서 통과 |
| J-08 게스트 버프·보정 | 통과(사용자 체감) |
| J-09 등 뒤 공격 | 사용자 확인 "이상 없음". 로그에는 각도가 없어 로그로는 판정할 수 없다 |
| D-07 게스트 예측 임팩트 | 통과(Phase 1 부분 통과 해소) |
| 공통 회귀 | 가드·패링·대시 이상 없음(사용자), `Block success` 15건 |

## Phase 2 완료 (2026-11-03)

사용자 결정으로 J-03은 코드 근거로 두고 마감했다. 인계 항목은 `phases/Phase02_JustDodge.md` 끝에 있다.

같은 날 마감 뒤 J-006(판정 순서)·J-011(대시당 1회)을 사용자가 스펙으로 확정했다. 확정 과정에서 짚은 부작용 두 가지(같은 프레임 근접 칼날 일괄 종료, 같은 프레임 인정 순서)도 의도에 맞는다고 확인받아 `design/Architecture.md` §3에 적었다.
