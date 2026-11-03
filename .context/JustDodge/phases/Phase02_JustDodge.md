# Phase 2 — 저스트 회피 판정 + 버프 + 연출 경계

> 상태: 완료 (2026-11-03)
> 선행: Phase 1 회피 무적 (완료)
> 순서: 단위 1 → 2 → 3 → 4. 각 단위를 끝내고 검증한 뒤 사용자에게 진행 여부를 확인받는다.

## 배경

Phase 1에서 대시가 무적 창을 열고, `Judge`가 `Dodged`를 판정하며, 네 개 경로가 그 결과를 처리하게 됐다. 이 Phase는 같은 기록 지점과 같은 `Judge` 위에 **저스트 창**을 얹는다. 저스트 창 안에서 근접·투사체 직격이 패링 반경에 들어오면 서버가 버프 GE를 걸고 성공 큐를 실행한다. 비주얼은 만들지 않고, 큐 태그 2개와 빈 GCN 에셋까지만 연결한다(J-012).

## 착수 전 코드 확인 (2026-11-03)

문서와 실제 코드가 맞는지 확인했다. 어긋난 곳은 없다.

| 항목 | 위치 | 결론 |
|:---|:---|:---|
| 무적 창 기록 | `Movement/LNPCharacterMoverComponent.cpp:373-383` | 리시뮬레이션 게이트 뒤, 권위·오토노머스 게이트 안. 저스트 창을 같은 블록에 기록한다 |
| `Judge` | `HitDetection/LNPGuardParryTypes.h` | Parried → (반경 밖 None) → Dodged → Guarded → Hit. `JustDodged`는 Parried 바로 뒤에 넣는다 |
| 근접 | `LNPWeaponTraceProcessors.cpp:587` | 패링 반경을 항상 계산한다. 조건 변경 없음 |
| 투사체 직격 | `LNPProjectileProcessors.cpp:1070-1071` | `bInParryRange = PS.bIsParrying && ...` — 넓혀야 한다(Phase 1 인계) |
| 스플래시·화염 | `LNPProjectileProcessors.cpp:739`, `LNPFlameConeProcessors.cpp:344` | `bInParryRange=false`를 넘긴다. `JustDodged`가 구조적으로 나올 수 없다(J-002). 변경 없음 |
| 클라 Ghost 예측 | `LNPProjectileProcessors.cpp:479-484` | 무적이면 임팩트 생략. 생략 로그가 없다(Phase 1 인계) |
| 버프 GE 패턴 | `GAS/Effects/LNPGameplayEffect_Burn.cpp` | `AggregateByTarget`·`StackLimitCount`·`RefreshOnSuccessfulApplication`·`bSuppressStackingCues` 패턴을 그대로 쓴다 |
| 스탯 탭 | `UI/Menu/LNPStatsViewModel.cpp:101-118` | 곱연산 배율을 최종값 ÷ 합연산 결과로 역산하고, 어트리뷰트 변경 델리게이트로 갱신한다. **버프 GE가 걸리면 코드 수정 없이 표기된다** |
| 큐 에셋 | `Content/GAS/GameplayCues/` | 일회성은 `ULNPGameplayCueNotify_VFXSound`(Static), 지속은 `GameplayCueNotify_Looping`(`GCN_LNP_Status_Burn`)이 선례다 |

## 목표와 범위

범위:

- 프래그먼트에 `JustDodgeExpiryTime`·`bJustDodgeConsumed` 추가, `ExecuteDash`에서 기록
- `Judge`에 `JustDodged` 추가, 근접·투사체 직격에서 처리
- 성공 커맨드 `FLNPJustDodgeCommand`와 버프 GE `ULNPGameplayEffect_JustDodgeBuff`
- 큐 태그 2개와 빈 GCN 에셋 2개
- 디버그 문구(저스트 창, `JUST DODGE`)와 Ghost 예측 생략 로그(Phase 1 인계)

범위 밖:

- 수치 확정과 지연 측정(Phase 3)
- 비주얼(트랙 V)
- 화상 지속 틱 회피, 근접 예측 HitStop(Phase 3에서 사용자 판단)

## 단위 1 — 상태와 판정

1. `FLNPParryStateFragment`에 `double JustDodgeExpiryTime = -1.0`, `bool bJustDodgeConsumed = false`를 추가한다. 소비 플래그는 Game Thread 커맨드만 쓴다는 점을 주석으로 남긴다.
2. `ULNPCharacterMoverComponent`에 `JustDodgeWindow` UPROPERTY(기본 0.12)를 추가한다. 기록할 때 `FMath::Min(JustDodgeWindow, IFrameDuration)`으로 묶는다 — 저스트 창이 무적 창보다 길어지면 `JustDodged`인데 무적이 닫힌 상태가 생긴다(Architecture §3).
3. `ExecuteDash`의 기존 무적 기록 블록에서 `JustDodgeExpiryTime`과 `bJustDodgeConsumed = false`를 함께 기록한다.
4. `ELNPPlayerDefenseResult`에 `JustDodged`를 추가하고 `Judge`의 ② 자리에 넣는다: `bInParryRange && Now <= PS.JustDodgeExpiryTime && !PS.bJustDodgeConsumed`.
5. `LNPGuardParry`에 "패링 반경을 계산할 필요가 있는가" 술어(`NeedsParryRange(PS, Now)` = 패링 중이거나 저스트 창이 열려 있고 미소비)를 `Judge` 옆에 둔다. 투사체 직격의 `bInParryRange` 조건을 이 술어로 바꾼다. 조건을 경로에 복제하지 않기 위해서다(J-001).
6. 경로별 처리:
   - 근접: `JustDodged`면 `MarkHit` 후 커맨드를 Push하고 끝낸다. 피해·경직·큐 없음.
   - 투사체 직격: `JustDodged`면 `Dodged`와 똑같이 `DodgedEntity`에 기록하고 관통시키며, 커맨드를 Push한다.

완료 조건: 빌드 통과(구조체 레이아웃 변경이라 에디터를 닫고 풀 빌드). 단위 2의 커맨드가 아직 없으면 임시 로그로 `JustDodged` 판정만 확인한다.

## 단위 2 — 성공 커맨드와 버프 GE

1. 태그: `TAG_GameplayCue_JustDodge_Success`(`GameplayCue.LNP.JustDodge.Success`), `TAG_GameplayCue_JustDodge_Buff`(`GameplayCue.LNP.JustDodge.Buff`)를 `LNPGameplayTags`에 추가한다.
2. `ULNPGameplayEffect_JustDodgeBuff`(`GAS/Effects/`, 새 `.cpp`):
   - `HasDuration`, 지속은 적용 커맨드가 `SetDuration`으로 넣는다.
   - Modifier: `AttackSpeed`·`MoveSpeed` 각각 `MultiplyAdditive`, 크기는 `TAG_GE_Data_Stat_AttackSpeed`·`TAG_GE_Data_Stat_MoveSpeed` SetByCaller.
   - 스택: Burn과 같은 패턴. `AggregateByTarget`, `StackLimitCount = 1`, `RefreshOnSuccessfulApplication`, `bSuppressStackingCues = true`(갱신마다 큐가 다시 나가지 않게 — Burn에서 겪은 문제).
   - `GameplayCues`에 `.Buff` 태그(지속 큐).
3. 수치 위치: `ULNPSettings`에 `Combat|JustDodge` 카테고리로 `JustDodgeBuffDuration`(2.5초), `JustDodgeBuffAttackSpeed`(0.2), `JustDodgeBuffMoveSpeed`(0.2)를 둔다. Phase 3 튜닝을 재컴파일 없이 프로젝트 설정에서 하기 위해서다. 판정 창 두 개(`IFrameDuration`·`JustDodgeWindow`)는 대시 파라미터 옆(Mover 컴포넌트)에 그대로 둔다.
4. `FLNPJustDodgeCommand`(`HitDetection/LNPHitDetectionShared.h`, `FLNPGuardBlockCommand`와 같은 형태). 항목은 방어자 엔티티 핸들, 방어자 Actor, 공격 종류(`Melee`/`Projectile`)다. `Run`에서:
   1. `EntityManager`로 방어자 프래그먼트를 얻어 `bJustDodgeConsumed`를 확인한다. 이미 세워져 있으면 버린다(같은 프레임에 여러 공격이 `JustDodged`로 판정된 경우, J-011). 아니면 세운다.
   2. 버프 GE spec을 만들어 SetByCaller 두 개와 지속을 넣고 방어자 ASC에 적용한다.
   3. `.Success` 큐를 실행한다. `Location`은 방어자 위치, `Normal`은 방어자 속도 방향(대시 방향 — V1이 쓴다).
   4. `LNP.Debug.Dodge`면 로그 `[Dodge] JustDodge success player=<폰> attack=<Melee|Projectile>`.
5. 소비 플래그가 다음 프레임부터 `Judge`의 ②를 막으므로, 같은 대시 안의 두 번째 공격은 `Dodged`로 떨어진다(무적 창이 아직 열려 있음).

완료 조건: 풀 빌드 통과. 1인 PIE에서 J-01(근접)·J-02(직격) 성공 로그와 `LNP.Debug.ShowSpeed`의 MoveSpeed x1.20, 2.5초 뒤 x1.00 복귀.

## 단위 3 — 연출 경계와 디버그

1. 빈 GCN 에셋 2개를 Unreal MCP로 만든다(위치 `Content/GAS/GameplayCues/`).
   - `GCN_LNP_JustDodge_Success`: 부모 `ULNPGameplayCueNotify_VFXSound`, 필드는 비워 둔다. 순보(V1)에 메시 숨김 같은 동작이 필요하면 그때 부모를 바꾼다.
   - `GCN_LNP_JustDodge_Buff`: 부모 `GameplayCueNotify_Looping`, 비워 둔다.
   - 태그가 에셋에 바인딩됐는지 GameplayCue 매니저 로그나 에디터에서 확인한다.
2. 화면 문구(`LNP.Debug.ShowDodge`, 보라 계열):
   - 창 열림 문구에 저스트 창 길이를 덧붙인다: `IFRAME 0.15s  JUST 0.12s  (<폰>)`.
   - 성공: `JUST DODGE  <Melee|Projectile>  (<폰>)`, 1.5초. 기존 두 색과 구분되는 밝은 보라를 쓴다. 서버 커맨드에서 띄우므로 Phase 1 문구처럼 단일 프로세스 PIE에서는 모든 창에 보인다.
3. Ghost 예측 생략 로그(Phase 1 인계): 클라 Ghost가 무적 때문에 예측 임팩트를 생략할 때 `LNP.Debug.Dodge`면 `[Dodge] Ghost impact skipped player=<폰>`을 남긴다. 탄 하나당 한 번(그 순간 `DodgedEntity`에 기록되므로 자연히 한 번)이다.
4. `design/Architecture.md` §7·`design/Validation.md` §6의 문자열을 실제 구현과 맞춘다.

완료 조건: CVar 두 개를 켜고 끄는 동작과 문자열이 문서와 일치한다.

## 단위 4 — 검증과 마무리

1. 1인 PIE: J-01~J-07, J-09.
2. 리슨 2인 PIE: J-08(게스트 저스트 회피와 버프 경계 보정, `mover.debug.ShowCorrections 1`, 필요하면 `LNP.Spawn.EnemyDensity 0`), D-07 재확인(게스트 Ghost 생략 로그).
3. 공통 회귀(`design/Validation.md` §2).
4. J 시나리오를 요인설계로 묶어 사용자 플레이 테스트를 1회 요청한다. 편성은 Phase 1과 같은 `DA_MassSpawnConfig_TestOnePerSpecies` + `LNP.Spawn.ConfigOverride`, PvP 경로는 `LNP.Team.Set 1 2`.
5. 커밋한 뒤 깨끗한 트리에서 Build.bat을 다시 돌린다.
6. 문서: `Current.md`를 Phase 3 기준으로 다시 쓰고, `Roadmap.md` 상태, `history/Phase02_Log.md`를 갱신한다. J-006·J-011을 실측 결과에 따라 초안에서 확정으로 올릴지 사용자에게 묻는다.

## 위험

- **MoveSpeed 보정:** MoveSpeed는 ASC에서 직접 읽어 버프가 켜지고 꺼지는 순간 서버·게스트 틱이 어긋날 수 있다(CharacterMovement §4.1). J-08에서 본다. 체감되면 Phase 3에서 대응을 정한다(이 Phase에서 고치지 않는다).
- **소비 플래그 지연:** 커맨드는 프레임 끝에 실행된다. 같은 프레임의 중복은 커맨드가 거르고, 다음 프레임부터는 `Judge`가 막는다. 구멍은 없지만 J-04로 확인한다.
- **오토노머스 기록:** 소유 클라도 저스트 창을 기록하지만 아무 데서도 읽지 않는다(J-007, 연출 예측 없음). Phase 3에서 예측을 채택하면 이 값을 쓴다.

## 완료 조건 체크리스트

- [x] 단위 1: 프래그먼트 필드·기록, `JustDodged`, 패링 반경 술어, 근접·직격 처리 (2026-11-03)
- [x] 단위 2: 태그, 버프 GE, 설정 수치, `FLNPJustDodgeCommand`, J-01·J-02 1인 확인 (2026-11-03. J-04·J-05·J-06·J-07도 1인에서 통과 — 로그 참조)
- [x] 단위 3: 빈 GCN 2개, 화면 문구, Ghost 생략 로그, 문서 문자열 일치 (2026-11-03. 출력 확인은 단위 4 PIE에서 함께)
- [x] 단위 4: J-01~J-09, D-07 재확인, 공통 회귀, 사용자 플레이 테스트, 클린 빌드 (2026-11-03. J-03 화염·폭발은 미수행 — 코드 근거로 둔다. 로그 참조)

## 다음 Phase 인계 항목

- J-03(화염·폭발 안에서 대시)은 실측하지 않았다. 두 경로는 `bInParryRange=false`라 `JustDodged`가 나올 수 없다는 코드 근거만 있다. Phase 3 플레이 테스트에 끼워 넣는다.
- 버프 경계 보정은 지연 없는 리슨 2인에서 체감되지 않았다(J-08). `PktLag` 50/100ms에서 다시 본다.
- 튜닝 손잡이: `ULNPCharacterMoverComponent`의 `IFrameDuration`·`JustDodgeWindow`, 프로젝트 설정 `Combat|Just Dodge`의 버프 지속·배율.
- 소유 클라이언트도 저스트 창을 기록하지만 아무 데서도 읽지 않는다. J-007을 재검토해 연출 예측을 채택하면 이 값을 쓴다.
- `GCN_LNP_JustDodge_Buff`에 MCP 도구가 붙인 빈 BP 컴포넌트 1개가 있다. V3에서 오라를 붙일 때 정리한다.
- J-006(판정 순서)·J-011(대시당 1회)은 구현·실측과 일치한다. 초안에서 확정으로 올릴지 사용자 결정이 남았다.
