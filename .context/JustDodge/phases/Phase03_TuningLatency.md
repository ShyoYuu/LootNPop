# Phase 3 — 수치 1차 튜닝·지연 측정·문서 마감

> 상태: 완료 (2026-11-04) — 단위 1~3. 단위 4(문서 마감)는 사용자 결정으로 Phase 4 끝으로 옮겼다
> 선행: Phase 2 저스트 회피 판정 + 버프 + 연출 경계 (완료)
> 순서: 단위 1 → 2 → 3 → 4. 각 단위를 끝내고 검증한 뒤 사용자에게 진행 여부를 확인받는다.

## 배경

Phase 2까지 판정·버프·연출 경계가 모두 붙었고, 수치는 초안(무적 0.15초, 저스트 0.12초, 버프 +20%·2.5초)이다. 이 Phase는 세 가지를 한다.

1. 플레이 테스트로 수치를 1차 확정한다.
2. 지연 환경에서 게스트가 얼마나 손해를 보는지 재고, J-007(연출 예측 없음)·J-010(RTT 역보정 없음)을 재검토한다.
3. 작업 폴더 밖의 프로젝트 공통 설계 문서를 마감한다.

지금 로그로는 **성공만 셀 수 있고 "아깝게 놓친" 경우를 셀 수 없다.** 성공률도, 지연이 얼마나 성공을 깎는지도 분모가 없다. 그래서 단위 1에서 계측을 먼저 붙인다.

## 착수 전 코드 확인 (2026-11-04)

문서와 실제 코드가 맞는지 확인했다. 어긋난 곳은 없다.

| 항목 | 위치 | 결론 |
|:---|:---|:---|
| 창 기록 | `Movement/LNPCharacterMoverComponent.cpp:382-386` | `Min(JustDodgeWindow, IFrameDuration)`, 소비 플래그 초기화, `ReportDodgeWindowOpened` |
| `Judge` | `HitDetection/LNPGuardParryTypes.h:124-146` | Parried → JustDodged → (반경 밖 None) → Dodged → Guarded → Hit (J-006) |
| 근접 분기 | `LNPWeaponTraceProcessors.cpp:587-627` | `Hit`은 함수 끝까지 내려가 경직·임팩트·데미지 커맨드를 넣는다. 계측 지점이다 |
| 투사체 직격 분기 | `LNPProjectileProcessors.cpp:1075-1130` | 같은 구조. `bInParryRange`는 `NeedsParryRange`일 때만 계산한다 |
| 성공 커맨드 | `LNPHitDetectionShared.h:526-580` | 버프 SetByCaller는 `1 + 설정값` |
| 설정 수치 | `Config/LNPSettings.h:209-220` | 커맨드가 실행 시점에 읽으므로 **PIE 중에 프로젝트 설정을 바꿔도 다음 성공부터 반영된다** |
| 판정 창 | `Movement/LNPCharacterMoverComponent.h:196-199` | 폰 BP 기본값이다. 바꾸면 PIE를 다시 시작해야 한다 |
| 성공 큐 | `GCN_LNP_JustDodge_Success` (부모 `ULNPGameplayCueNotify_VFXSound`) | 소유 클라이언트가 큐를 받은 시각을 남기는 곳이 없다 |
| 플레이어 ASC | `Player/LNPPlayerState.cpp:21` | `Mixed` — GE는 소유자에게, 큐는 모두에게 간다 |
| 지연 에뮬레이션 | 프로젝트 문서 선례 `net.PktLag=150` (TechDesign_Networking.md) | 단일 프로세스 PIE에서는 서버·게스트 넷 드라이버 모두에 걸린다. **설정값과 실제 RTT를 같다고 가정하지 않고 로그로 잰다**(단위 1-3) |

## 목표와 범위

범위:

- 놓친 저스트 회피와 큐 도착 시각을 재는 계측(디버그 로그 2종)과 로그 집계 스크립트
- 플레이 테스트 3회(지연 0/중/고)로 수치 1차 확정, J-03 실측
- J-007·J-010 재검토와 Phase 1부터 이어진 사용자 판단 3건 결정
- 공통 설계 문서 신설·교차 링크·인덱스 3곳

범위 밖:

- 비주얼(트랙 V). 단, 단위 1에서 만드는 성공 큐 C++ 클래스는 V1이 그대로 확장한다.
- 단위 3에서 대응을 "채택"으로 정한 항목의 구현. 작으면 이 Phase에 넣고, 크면 별도 단계로 범위를 다시 잡는다.

## 단위 1 — 계측

### 1-1. 놓친 저스트 회피 로그 (서버, Worker Thread)

근접·투사체 직격에서 결과가 `Hit`일 때, `LNP.Debug.Dodge`면 한 줄을 남긴다.

```
[Dodge] Hit path=<Melee|Projectile> player=<폰> sinceJust=<ms|->
```

- `sinceJust`: `Now - PS.JustDodgeExpiryTime`(ms). 마지막 대시의 저스트 창이 1초 안에 닫혔으면 숫자, 아니면 `-`. **"너무 일찍 대시함"**의 분포다.
- **"너무 늦게 대시함"**은 이 줄 뒤 짧은 시간(집계 기본 300ms) 안에 같은 폰의 서버 `IFrame open`이 오는 경우로, 집계 스크립트가 짝짓는다. 피격 시점을 프래그먼트에 기록하지 않아도 된다(Worker Thread에서 `const` 프래그먼트를 쓰지 않는 규약 유지).
- 구현은 `LNPGuardParry::ReportDodgeHit(Path, Actor, PS, Now)`를 기존 `Report*` 옆에 두고 두 경로의 `Hit` 분기에서 부른다. `ReportDodgeIgnored`처럼 Worker Thread에서 `UE_LOG`만 한다(화면 문구 없음).
- 화염·스플래시는 저스트 회피 대상이 아니라 넣지 않는다(J-002).

### 1-2. 성공 큐 도착 로그 (모든 머신)

`ULNPGameplayCueNotify_JustDodgeSuccess`(부모 `ULNPGameplayCueNotify_VFXSound`, 새 `.cpp`)를 만들고 `GCN_LNP_JustDodge_Success`의 부모를 이것으로 바꾼다. `OnExecute`에서 `LNP.Debug.Dodge`면 남기고 `Super`를 부른다.

```
[Dodge] Success cue arrived player=<폰> local=<0|1> role=<역할>
```

- `local=1`인 줄(소유 화면)과 서버 `JustDodge success` 줄의 차이가 **연출 지연**이고, 같은 폰의 `IFrame open role=Autonomous`와의 차이가 **"내가 누른 뒤 연출이 보이기까지"**다. 대시 길이(0.2초)와 비교한다.
- 이 클래스는 V1(순보)이 메시 숨김을 붙일 자리다. 계측 때문에 앞당겨 만드는 것이라 V1이 버리지 않는다.
- 범용 `ULNPGameplayCueNotify_VFXSound`에 태그 분기를 넣지 않기 위해 하위 클래스로 둔다.

### 1-3. 집계 스크립트

`Scripts/Profiling/AnalyzeDodgeLog.py <로그> [--since HH:MM:SS]` (단일 프로세스 PIE라 서버·게스트 줄이 한 로그 파일에 같은 시계로 찍힌다).

출력:

| 표 | 내용 |
|:---|:---|
| 폰별 요약 | 서버 대시 수, 성공 수(Melee/Projectile), `Hit` 수, 이른 놓침·늦은 놓침 수 |
| 놓침 분포 | 이른 놓침 `sinceJust`, 늦은 놓침(피격 → 서버 대시) 간격을 50ms 구간으로 |
| 업링크 지연 | 게스트 `IFrame open role=Autonomous` → 같은 폰 서버 `IFrame open` 간격(입력 버퍼 포함). **실제 적용된 지연값**이다 |
| 연출 지연 | 서버 `JustDodge success` → `Success cue arrived local=1`, Autonomous 대시 → 큐 도착 |

- 성공률은 `성공 / (성공 + 이른 놓침 + 늦은 놓침)`으로 정의한다. 놓침 판정 구간은 스크립트 인자 `--early-ms`·`--late-ms`(기본 둘 다 300)다. 로그는 이른 쪽을 1초까지 남기므로 상한을 바꿔 다시 집계할 수 있다.
- 리스폰한 게스트 폰은 서버·게스트 월드에서 이름이 달라서, 게스트 쪽 줄(Autonomous 대시, 큐 도착)은 시각으로 서버 폰과 짝짓는다(2026-11-04 발견).

완료 조건: 풀 빌드 통과(새 `.cpp`), GCN 부모 변경 후 태그 바인딩 유지(에셋 레지스트리 `GameplayCueName`), 1인 PIE 짧은 로그에서 스크립트 출력이 나온다. `design/Architecture.md` §7·`design/Validation.md` §6에 새 문자열 2종을 넣는다.

## 단위 2 — 플레이 테스트와 수치 1차 확정

요인설계로 리슨 2인 PIE 3회를 묶는다. 매회 호스트·게스트가 같은 행동을 한다(게스트 팀 2, `LNP.Team.Set 1 2`).

| 회차 | 지연 | 편성 | 추가 행동 |
|:---|:---|:---|:---|
| R1 | 없음 | 기본 `DA_MassSpawnConfig`(군중) | 군중 속 저스트 회피가 지나치게 쉬운지. **J-03**(게스트 화염·폭발 안에서 호스트 대시) |
| R2 | 중간 (게스트 RTT 약 50ms 목표) | `DA_MassSpawnConfig_TestOnePerSpecies` + `LNP.Spawn.ConfigOverride` | 근접·직격 저스트 회피 반복, 버프 경계 보정(`mover.debug.ShowCorrections 1`) |
| R3 | 높음 (게스트 RTT 약 100ms 목표) | R2와 같음 | R2와 같음. 게스트 체감 "피했는데 맞았다" 빈도 |

- 매회 `LNP.Debug.Dodge 1`, `LNP.Debug.ShowDodge 1`. 지연값은 시작 시 넣고, 실제 값은 1-3 업링크 표로 확정한다.
- 매회 뒤 사용자에게 받는 체감 질문(짧게): 저스트 창이 좁은가·넓은가, 버프가 체감되는가·과한가, 지연 회차에서 "피했는데 맞음"이 몇 번 거슬렸는가.
- 수치 조정은 회차 사이에 한다. 버프 수치는 PIE 중 프로젝트 설정 `Combat|Just Dodge`에서 바로 바뀌고, 창 두 개는 폰 BP 기본값이라 다음 회차부터 반영된다.
- 판정 기준(초안, 결과를 보고 사용자와 확정):
  - 지연 없음에서 이른·늦은 놓침이 한쪽으로 쏠리면 그쪽으로 창을 옮기거나 넓힐 근거다.
  - 군중 R1에서 성공률이 높고 대부분의 대시가 성공이면 "너무 쉬움" 후보다(패링 반경이 넓은 효과).

완료 조건: `IFrameDuration`·`JustDodgeWindow`·버프 3수치가 사용자 확인으로 1차 확정, J-03 결과 기록, 회차별 집계 표가 로그에 남는다.

## 단위 3 — 재검토와 사용자 판단

R2·R3 집계를 근거로 사용자와 정한다.

| 항목 | 판단 근거 | 선택지 |
|:---|:---|:---|
| J-010 RTT 역보정 없음 | 늦은 놓침 분포가 지연 회차에서 업링크 지연만큼 밀리는가 | 유지 / 피격 유예 버퍼(Architecture §6 후보 2) |
| J-007 연출 예측 없음 | Autonomous 대시 → 큐 도착이 대시 길이(0.2초)를 넘는가 | 유지 / 소유 클라 연출 예측(후보 1) — 비주얼 트랙 V1과 묶임 |
| 화상 지속 틱 회피 안 함 | R1 J-03, Phase 1 실측(화염 사망 주원인) | 유지 / 무적 중 틱도 무시 |
| 근접 예측 HitStop | PvP에서 한 번 어긋나는 것이 거슬리는가 | 유지 / 수정 |
| D-02 일찍 대시해 칼날 안에 남음 | 이른 놓침 로그에 근접 사례가 잡히는가 | 코드 근거 유지 / 실측 통과로 기록 |

- 결정은 `Decisions.md`에 반영한다. J-007·J-010을 확정하거나 `대체됨` + 새 ID로 바꾼다.
- "채택"이 나온 항목은 크기를 재서, 작으면 이 단위에서 구현하고 크면 Roadmap에 별도 단계로 넣는다.

## 단위 4 — 문서 마감과 커밋

1. `GameDesign_JustDodge.md`(무적·저스트 회피 규칙, 버프, 확정 수치, 플레이어 경험 의도)와 `TechDesign_JustDodge.md`(프래그먼트 미러, `Judge` 순서, 경로별 처리, 성공 커맨드·버프 GE, 지연 특성, 디버그)를 새로 만든다. 작업 폴더의 `design/` 내용을 요약·이관하고, 작업 폴더 문서에는 링크만 남긴다(원본은 한 곳).
2. 교차 링크: `TechDesign_ParrySystem.md`(Judge 순서), `TechDesign_HitDetection.md` §7.5(결과 enum), `TechDesign_CharacterMovement.md` §5(대시 무적).
3. 문서 인덱스 3곳(CLAUDE.md, ProjectOverview, Notion)과 `DevelopmentPlan.md`를 갱신한다.
4. 공통 회귀(`design/Validation.md` §2)는 단위 2 회차에 함께 본다. 커밋한 뒤 깨끗한 트리에서 Build.bat을 다시 돌린다.
5. `Roadmap.md` 메인 트랙 완료 표기, `Current.md`를 비주얼 트랙(V) 대기 기준으로 다시 쓴다.

## 위험

- **지연 설정의 의미:** 단일 프로세스 PIE에서 `net.PktLag`는 양쪽 드라이버에 걸려 RTT가 설정값의 약 2배가 될 수 있다. 그래서 목표는 RTT로 적고 실제 값은 업링크 표로 확인한다.
- **늦은 놓침 짝짓기 오탐:** 피격 직후 반사적으로 누른 대시(회피 의도 없는 탈출)도 늦은 놓침으로 잡힌다. 분포의 꼬리로 보고, 판정 구간(300ms)은 지연 없음 R1의 분포를 보고 조정한다.
- **군중 로그량:** 기본 편성에서 `Hit` 줄이 많아질 수 있다. 화면 문구는 넣지 않으므로 플레이에는 영향이 없다.

## 완료 조건 체크리스트

- [x] 단위 1: `[Dodge] Hit` 로그, 성공 큐 클래스와 도착 로그, 집계 스크립트, 문서 문자열 (2026-11-04, 1인 PIE 확인 — 로그 참조)
- [x] 단위 2: R1~R3, 수치 1차 확정(현행 유지), J-03 실측(화염 R1·폭발 R2 통과) (2026-11-04 — 로그 참조. 지연 시 넉백이 게스트 대시를 거부하는 현상 발견)
- [x] 단위 3: J-007 확정, J-010 대응 설계(`design/Architecture.md` §6.2, A 채택 → Phase 4), 화상 틱 유지(J-013)·근접 예측 HitStop 유지(J-014)·D-02 코드 근거 유지 (2026-11-04)
- [ ] 단위 4: Phase 4 끝으로 이동 — 공통 문서를 피격 유예 버퍼가 들어간 뒤 한 번만 쓰기 위해서다

## 다음 Phase 인계 항목

- Phase 4는 `design/Architecture.md` §6.2 A(피격 유예 버퍼)를 구현하고, 이 실행서의 단위 4(문서 마감)를 이어받는다.
- 재측정 기준: 이 Phase의 R2·R3. `AnalyzeDodgeLog.py`의 `dropped by server`(R2 10/38, R3 4/19)가 0에 가까워져야 한다. 지연 설정은 두 창 모두에 `net.PktLag=25`·`50`이다.
- 수치는 현행 유지로 1차 확정했다(무적 0.15, 저스트 0.12, 버프 +20%·2.5초). 버프는 사용자가 천천히 조정한다.
- `LNP.Debug.Dodge` CVar 도움말은 아직 무적 로그만 설명한다. `Hit`·큐 도착 로그가 추가됐으니 다음 풀 빌드 때 도움말을 고친다.
