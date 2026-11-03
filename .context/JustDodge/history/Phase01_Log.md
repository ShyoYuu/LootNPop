# Phase 1 로그 — 회피 무적

> 상태: 완료
> 실행 계획: [Phase01_DodgeIFrame.md](../phases/Phase01_DodgeIFrame.md)

## 2026-11-03 — 단위 0 사전 조사

기준 커밋: `f9164fa`. 문서의 파일:라인 인용(2026-10-26 기준)은 아직 맞다. `ExecuteDash`의 리시뮬레이션 게이트만 `LNPCharacterMoverComponent.cpp:370`으로 몇 줄 밀려 있다.

### 0-1. `Judge`를 거치지 않는 피해 경로

플레이어 ASC에 피해·경직·넉백을 넣는 곳을 `FLNPApplyDamageGECommand`, `ApplyGameplayEffect*`, `LNPPoise::Accumulate`, `ApplyKnockback`으로 모두 찾았다.

| 경로 | `Judge` 경유 | 회피 적용 |
|:---|:---|:---|
| 근접(플레이어 무기·Actor 적 무기) | 예 — `LNPWeaponTraceProcessors.cpp:587` | 적용 |
| 순수 엔티티 적의 가상 칼날 | 예 — `LNPEntityAttackProcessor.cpp:315`이 `FLNPWeaponTraceFragment` 엔티티를 만들어 같은 프로세서를 탄다 | 적용(별도 작업 없음) |
| 투사체 직격·스플래시 | 예 — `LNPProjectileProcessors.cpp:1050`, `:723` | 적용 |
| 화염 원뿔 틱 | 예 — `LNPFlameConeProcessors.cpp:344` | 적용. 무적 중에는 새 화상 스택(`FLNPApplyPlayerBurnGECommand`)도 걸지 않는다 |
| **플레이어 화상 지속 틱** | 아니오 — `ULNPGameplayEffect_Burn`의 주기 GE가 GAS 안에서 직접 깎는다 | **적용하지 않는다.** 이미 걸린 상태 이상이고, 대시로 지속 피해를 끊을 수 있으면 화상이 의미를 잃는다. 막을 수 있는 것은 "새로 붙는 스택"까지다 |
| `LNPBurnProcessor`의 `FLNPApplyDamageGECommand` | 해당 없음 — 적(`FLNPEnemyTag`) 전용 | — |
| `ApplyKnockback`(`LNPCharacterBase.cpp:393`) | 피해 GE 커맨드 안에서만 불린다 | 피해 커맨드를 막으면 함께 막힌다 |
| `LNPStatModifier`·장비·인벤토리의 `ApplyGameplayEffectSpecToSelf` | 피해가 아니라 스탯·버프 | 해당 없음 |

결과 enum을 `switch`로 처리하는 곳은 없다. 네 경로 모두 `if` 연쇄라 컴파일러 경고로는 누락을 잡을 수 없다. 단위 2에서는 다음 세 곳을 직접 고쳐야 한다. 그대로 두면 `Dodged`가 피격으로 흘러간다.

- 화염: `Guarded`가 아니면 모두 `else`(피격)로 간다.
- 스플래시: `== Guarded`만 확인한다.
- 투사체 직격: `Parried`·`Guarded`가 아니면 `else`(피격)로 간다.

근접은 `None` 다음에 바로 `MarkHit`을 하므로 `Dodged` 분기를 `MarkHit` 앞에 둔다.

### 0-2. 프래그먼트 접근 수단

`ULNPInputHandlerComponent::GetParryFragment()`(`LNPInputHandlerComponent.cpp:607-631`)는 소유 Actor의 `UMassAgentComponent`만 쓴다. InputHandler 고유 상태는 쓰지 않는다.

- **결론:** 본문을 `LNPGuardParry::FindFragment(const AActor*)`로 옮긴다(새 `HitDetection/LNPGuardParryTypes.cpp`). `GetParryFragment()`는 이 함수에 위임하고, `ULNPCharacterMoverComponent::ExecuteDash`도 이 함수를 부른다. 이동 컴포넌트가 InputHandler에 의존하지 않게 하려는 것이다.
- 서버의 원격 폰: `UMassAgentComponent`는 폰 클래스의 컴포넌트라 모든 머신에 있다. 서버 판정 쿼리가 모든 플레이어의 `FLNPParryStateFragment`를 읽으므로 서버에는 엔티티가 항상 있다.
- 소유 클라이언트: 클라이언트 투사체 예측 경로의 `PlayerQuery`도 `FLNPParryStateFragment`를 요구한다(`LNPProjectileProcessors.cpp:218`). 따라서 클라이언트에도 플레이어 엔티티가 있다.
- 스레드: `ExecuteDash`는 `Montage_Play`를 부르므로 게임 스레드에서 돈다. InputHandler의 가드 미러와 같은 조건에서 쓴다.

### 0-3. 리스폰과 엔티티 수명

- 리스폰(`ALNPGameMode` 리스폰 타이머)은 옛 폰을 `UnPossess` → `Destroy`하고 `RestartPlayer`로 **새 폰**을 만든다. 새 폰의 `UMassAgentComponent`가 새 엔티티를 만들고, 필드는 기본값 -1로 시작한다.
- 사망부터 리스폰까지는 `FLNPPlayerDeadTag`가 서버 판정 쿼리 세 곳에서 플레이어를 뺀다.
- 만료 시각이 절대 시각이고 창은 0.15초라 재사용되더라도 저절로 닫힌다.
- **결론:** 초기화 지점을 따로 두지 않는다. D-08은 구조적으로 성립하며, 단위 4에서 확인만 한다. `design/Architecture.md` §2.2를 이에 맞게 고쳤다.

### 0-4. 투사체 회피 대상 칸

- `FLNPProjectileFragment`(`LNPProjectileMassTypes.h:73`)에 `FMassEntityHandle DodgedEntity`를 둔다. `Instigator` 바로 아래에 둔다(둘 다 "이 탄이 건너뛸 엔티티"이기 때문이다).
- 이 프래그먼트는 복제되지 않는다. 각 머신이 스폰할 때 직접 만든다(`LNPAbility_RangedAttack.cpp:208`, `LNPEntityAttackProcessor.cpp:275`, `LNPGhostProjectileSubsystem.cpp:187`). 기본값으로 시작하므로 스폰 코드는 고치지 않는다.
- **패링 반사 시 초기화한다.** 반사탄은 원래 공격자를 노린다. 반사한 사람은 새 `Instigator`가 되어 어차피 제외된다.
- 클라이언트 Ghost도 같은 칸을 쓴다(0-5).

### 0-5. 클라이언트 예측 경로

| 경로 | 로컬 플레이어가 피격 대상인가 | 처리 |
|:---|:---|:---|
| 투사체 Ghost 충돌(`LNPProjectileProcessors.cpp:393-484`) | 예 — 관전용 Ghost(적 탄·다른 플레이어 탄)가 로컬 플레이어 캡슐에 닿으면 Ghost를 지우고 임팩트 VFX를 낸다 | 대상 플레이어의 무적 창이 열려 있으면 임팩트를 생략하고 그 Ghost의 `DodgedEntity`에 기록한다. 무적이 끝난 뒤 같은 Ghost가 다시 겹쳐도 서버(D-04)처럼 맞지 않는다 |
| 근접 예측 HitStop(`LNPWeaponTraceProcessors.cpp:250-368`) | 아니오 — 로컬 공격자의 HitStop 예측이다. 로컬 플레이어는 자기 공격의 대상이 아니다 | 처리하지 않는다. PvP에서 상대가 회피해도 공격자 화면에 예측 HitStop이 한 번 날 수 있다. 상대의 무적 창은 그 머신에 기록되지 않는다(시뮬레이티드 프록시에서는 기록하지 않는다). 연출뿐이라 허용한다 |
| 화염·스플래시 | 서버 전용(`ExecutionFlags = Server \| Standalone`, 스플래시는 서버 분기 안에만 있음) | 해당 없음 |

- 예측 경로의 무적 확인에는 `IsLocallyControlled()`를 따로 붙이지 않는다. 시뮬레이티드 프록시는 만료 시각을 기록하지 않으므로, 창이 열려 있는 플레이어는 곧 로컬 플레이어다. 시계도 그 머신 자신의 `GetTimeSeconds()`로 기록하고 비교하므로 서버와 맞출 필요가 없다.

### 단위 0 완료

다섯 항목의 결론을 위에 기록했다. 설계 변경은 `design/Architecture.md` §2.2(초기화 지점 없음, 접근 헬퍼)와 §4(반사 시 칸 초기화, 예측 경로 범위)에 반영했다.

## 2026-11-03 — 단위 1 상태 미러

### 변경

- `HitDetection/LNPGuardParryTypes.cpp`를 새로 만들고 `LNPGuardParry::FindFragment(const AActor*)`를 넣었다. `ULNPInputHandlerComponent::GetParryFragment()`는 이 함수에 위임하고, 쓰지 않게 된 Mass include 두 개를 지웠다.
- `FLNPParryStateFragment`에 `double DodgeIFrameExpiryTime = -1.0`을 추가했다. 헤더 주석은 "방어 상태(가드·패링·회피)"로 넓혔다.
- `ULNPCharacterMoverComponent::IFrameDuration`(기본 0.15)을 추가했다.
- `ExecuteDash`의 리시뮬레이션 게이트 뒤에서, 권위·오토노머스일 때만 `GetTimeSeconds() + IFrameDuration`을 기록한다.
- 확인용 임시 로그 `[Dodge] IFrame open <폰> role=<역할> expiry=<시각>`를 남겼다. 단위 3에서 `LNP.Debug.Dodge`로 대체한다(코드에 TODO).
- 사망·리스폰 초기화는 단위 0의 결론대로 넣지 않았다.

### 검증

- 빌드: 게임 타깃 Build.bat 통과. 이어서 에디터를 닫고 에디터 타깃 Build.bat 통과. 새 `.cpp`와 구조체 레이아웃 변경이 있어 Live Coding은 쓰지 않았다.
- 1인 PIE(TestMap03)에서 대시 1회:

```
[Dash] BP_LNPPlayer_C_0 execute | role=ROLE_Authority resim=0 montage=AM_MM_Dash_Backward_UEFN
[Dodge] IFrame open BP_LNPPlayer_C_0 role=ROLE_Authority expiry=7.363
```

같은 프레임(712)에 대시와 무적 창 기록이 함께 찍혔다. 서버 프래그먼트가 갱신된다.

- 아직 확인하지 않은 것: 리슨 2인에서 게스트 대시가 서버(Authority)와 소유 클라이언트(AutonomousProxy) 양쪽에 기록되는지. 단위 4의 D-07에서 확인한다.

## 2026-11-03 — 단위 2 `Dodged`와 경로별 처리

### 변경

- `ELNPPlayerDefenseResult::Dodged`를 추가했다. `Judge` 순서: Parried → (피격 반경 밖이면 None) → Dodged → Guarded → Hit. 각도는 보지 않는다.
- 근접: `Dodged`면 `MarkHit` 없이 반환한다.
- 스플래시: `Dodged`면 피해·넉백·경직 없이 `continue`.
- 화염: `Dodged`면 틱 피해·경직·새 화상 스택 없이 `continue`.
- 투사체 직격: `Dodged`면 `FLNPProjectileFragment::DodgedEntity`에 기록하고 `FinishHit` 없이 다음 후보로 간다. 후보 루프는 `DodgedEntity`를 건너뛴다. 패링 반사 때는 이 칸을 비운다.
- 클라 Ghost 예측: 플레이어 대상에 엔티티 핸들과 무적 여부를 싣는다. 무적이면 임팩트 없이 `DodgedEntity`에 기록하고, 이후 그 대상을 건너뛴다.
- `LNP.Debug.Dodge`(기본 0)와 `LNPGuardParry::LogDodgeIgnored`를 단위 3에서 앞당겨 넣었다. D-01~D-06의 판정 근거가 서버 로그이기 때문이다. 단위 3에는 화면 표시와 임시 `IFrame open` 로그 교체가 남는다.
- 빌드: 게임 타깃과 에디터 타깃 Build.bat 모두 통과.

### 사용자 플레이 테스트 (리슨 2인 PIE, `DA_MassSpawnConfig_TestOnePerSpecies`, 게스트 팀 2 + 화염방사기)

호스트가 대시를 50회 했다(전부 `role=ROLE_Authority`). 중간에 한 번 사망해 폰이 `BP_LNPPlayer_C_0`에서 `C_2`로 바뀌었다. 무시 로그는 17건이다.

| 시나리오 | 결과 | 근거 |
|:---|:---|:---|
| D-01 근접 | 통과 | f151 대시 → 같은 프레임부터 f151~153 `path=Melee` 3건(같은 휘두르기가 3프레임 겹침). 이어서 f156 `Target moved out of range`, 이 휘두르기의 피해 로그 없음 |
| D-02 일찍 대시 | 미확인 | 의도적으로 재현하기 어렵다. 코드상 `MarkHit`을 하지 않는 것으로만 보장된다 |
| D-03·D-04 투사체 | 로그상 통과 | `path=Projectile` 13건. 같은 프레임 2~3건은 산탄 여러 발이다. 무시 직후 ~2초 안에 이어진 피해 로그 없음(다음 피해는 별개 사격). 탄이 뒤로 지나가는지는 시각 확인 필요 |
| D-05 폭발 | 미실시 | — |
| D-06 화염 | 통과(1회) | 화염 틱 5프레임 간격(f482·487·492 피해) → f495 대시 → f497 `path=Flame`으로 1틱 무시 → 무적(0.15초)이 끝난 f502에 사망(HP 5.5). 피해가 다시 들어온 것은 기대대로다. 한 번뿐이라 반복 확인이 더 필요하다 |

### 사용자 피드백

- "아슬아슬하게 피한 게 캡슐이 이동해서인지, 무적 판정 때문인지 플레이 중에는 구분할 수 없다."
- 로그 해석: `IFrame ignored`는 **그 프레임에 피격 반경 안에 있었다**는 뜻이다. 즉 무적이 없었다면 맞았을 공격이다. 이동만으로 피한 경우에는 로그가 남지 않는다. 다만 플레이 중에는 로그를 볼 수 없으므로, 단위 3의 화면 표시에서 이 구분을 보여 줘야 한다.

### 추가 테스트 (같은 날 두 번째 PIE 세션) — 무효

- 사용자가 폭발(런처)과 화염을 게스트로 다시 시험했다. 대시는 12회였다.
- 로그에는 `IFrame ignored`가 0건이고, 세션 전체에 `[GE] HP` 피해 로그도 0건이다.
- 원인: 이번 세션에서 `LNP.Team.Set 1 2`를 입력하지 않았다. 두 플레이어가 같은 팀이라, 게스트의 탄과 화염이 `LNPTeam::CanHit`에서 걸러져 `Judge`까지 가지 않았다.
- D-05는 여전히 미확인이고, D-06도 첫 세션의 1회뿐이다. `design/Validation.md` §6에 이 함정을 적었다.

## 2026-11-03 — 단위 3 디버그

- 사용자 요청: 무적이 막은 순간을 화면 문구로 보여 주고, CVar로 켜고 끌 수 있게, 기본은 꺼짐.
- CVar를 둘로 나눴다.
  - `LNP.Debug.Dodge`: 로그. 단위 2에서 넣은 것이다. 임시 `IFrame open` 로그도 이 CVar 뒤로 옮겼다.
  - `LNP.Debug.ShowDodge`: 화면 문구.
- 화면 문구 두 가지(보라):
  - `IFRAME  0.15s  (<폰>)`: 로컬 폰이 창을 연 순간 창 길이만큼 표시한다.
  - `IFRAME BLOCK  <경로>  (<폰>)`: 서버가 무시할 때마다 1초 표시한다. 경로별로 한 줄을 덮어쓴다.
- 판정 프로세서는 Worker Thread에서 돌기 때문에 화면 문구는 `AsyncTask(GameThread)`로 넘긴다.
- `LogDodgeIgnored`의 이름을 `ReportDodgeIgnored`로 바꿨다(로그와 화면을 함께 다루므로). `ReportDodgeWindowOpened`를 추가했다.
- 빌드: 게임 타깃 통과. 에디터 타깃과 화면 확인은 남았다.

### 재검증 (같은 날 세 번째 PIE 세션 — D-05·D-06, 화면 문구)

설정: `LNP.Debug.Dodge 1`, `LNP.Debug.ShowDodge 1`, `LNP.Team.Set 1 2`(`team 1 -> 2` 확인). 게스트는 런처와 화염방사기로 공격했다. 호스트는 대시 17회, 무시 로그 10건, 사망 1회.

| 시나리오 | 결과 | 근거 |
|:---|:---|:---|
| D-05 폭발 | 통과 | f924 대시 → 같은 프레임 `path=Splash`, 이후 피해 없음. f431 대시 → f432 `path=Projectile`(런처 탄 직격 관통) → f436 그 탄이 착탄하며 `path=Splash`(창이 열린 지 0.146초, 아직 창 안). 직격 관통과 이어지는 폭발까지 한 대시로 막혔다. 대조군: 대시 없이 맞은 폭발은 `[GE] HP` 24.9·20.4, 대시가 한 프레임 늦은 f167→f168은 28.0 피해 |
| D-06 화염 | 통과 | 대시 13회 중 7회에서 `path=Flame` 1건씩. 화염 틱 간격이 약 5프레임(0.14초)이라 0.15초 창에는 틱이 하나 들어간다. 창이 닫힌 직후 틱은 다시 피해를 준다(예: f170 무시 → f175 피해) |

관찰:

- **화상 지속 틱이 사망의 주원인이었다.** `[GE] HP` 줄 사이에 HP가 크게 빠지는 구간(88.1→84.5, 53.5→48.1, 42.6→20.8, 19.0→4.5)은 피해 GE 로그를 남기지 않는 화상 주기 GE다. "화상 지속 틱은 회피하지 않는다"는 결정(단위 0)대로 동작하지만, 체감상 대시로 화염을 피해도 화상 때문에 계속 깎인다. Phase 3 튜닝 때 사용자 판단이 필요하다.
- **시뮬레이티드 프록시도 `ExecuteDash`를 리시뮬레이션이 아닌 첫 시뮬레이션으로 지난다.** f241·f470에 `[Dash] ... role=ROLE_SimulatedProxy resim=0`이 찍혔다(게스트 화면의 호스트 폰). 헤더 주석은 "프록시는 이 함수를 첫 시뮬레이션으로 지나가지 않는다"고 하지만 실제로는 지나간다. 무적 기록은 역할 게이트(권위·오토노머스)로 막혀 있어 영향이 없다. 게이트가 꼭 필요하다는 근거다.

## 2026-11-03 — 단위 4 검증 (리슨 2인 PIE)

설정: `LNP.Debug.Dodge 1`, `LNP.Debug.ShowDodge 1`, `mover.debug.ShowCorrections 1`, `DA_MassSpawnConfig_TestOnePerSpecies`. 도중에 `LNP.Spawn.EnemyDensity 1`을 주고 PIE를 다시 시작했다.

| 시나리오 | 결과 | 근거 |
|:---|:---|:---|
| D-07 게스트 투사체 | 서버 판정 통과 | 게스트 대시마다 `role=ROLE_AutonomousProxy`(게스트) → 약 2프레임 뒤 `role=ROLE_Authority`(서버) 두 줄이 찍혔다. 서버 시각 기준 만료는 게스트보다 약 0.88초 뒤다(두 PIE 월드의 시계가 다르다 — 각자 자기 시계로 비교하므로 문제없다). 게스트 폰 `path=Projectile` 12건 |
| D-07 게스트 근접 | 미발생 | 게스트의 `path=Melee` 0건 |
| D-07 게스트 화면 예측 임팩트 | 사용자 확인 대기 | — |
| D-08 리스폰 | 통과 | 호스트 `LNP.Debug.KillPlayer 0` → 새 폰 `BP_LNPPlayer_C_3`. 리스폰 뒤 첫 피격(대시 6초 뒤)은 피해 6.0이 정상 적용됐고 무시 로그는 없다 |

관찰:

- **게스트만 대시하고 서버는 대시하지 않은 경우가 있다.** 12:17:33 f394에 게스트가 `ROLE_AutonomousProxy`로 대시를 기록했지만 서버 줄은 없다. 다음 프레임 `Dash Cooldown Modifier was ended ... after a rollback`이 찍혔다. 같은 프레임에 피격 로그가 있어 서버에서는 `CanDash`가 거짓이었던 것으로 보인다(경직 등).
  - 이때 게스트 프래그먼트의 만료 시각은 롤백되지 않는다. 리시뮬레이션 때는 쓰지 않기만 하기 때문이다. 그래서 0.15초 동안 게스트 Ghost가 예측 임팩트를 생략할 수 있다. 서버는 피해를 주므로 결과는 맞고, 임팩트 VFX만 빠지는 연출 어긋남이다.
  - 드물고 연출뿐이라 허용한다. 횟수: 게스트 기록 66건 중 서버 미실행 3건(f394·f359·f649).
- 게스트 대시 중 `IFrame open ... AutonomousProxy`가 연달아 두 번 찍힌 경우가 있다(f160·f161, f553·f554, 만료 값 동일). 같은 값을 다시 쓸 뿐이라 영향은 없다. 원인은 조사하지 않았다.

### 사용자 피드백과 추가 변경

- 화면 문구는 쓸 만하다. 남은 JustDodge 작업 내내 쓸 것이다.
- 요청: 무적 판정 성공과 단순 대시의 색을 구분해 달라(짙은 보라/연한 보라).
  - `IFRAME BLOCK`은 짙은 보라 `(140, 50, 230)`, `IFRAME`(창 열림)은 연한 보라 `(215, 185, 255)`로 바꿨다. 게임 타깃 빌드는 통과했다.

### 마무리 확인 (사용자)

- 색 구분: 확인했고 문제없다.
- 공통 회귀(가드·패링, 일반 피격, 대시 이동·대시 점프, 쿨다운 파이, 보정 캡슐): 육안으로는 어색한 부분이 없었다.
- D-07 게스트 화면의 예측 임팩트: 육안으로는 판정하기 어렵다. NPC 탄이 산탄(7발)이라 전부 피했는지 일부만 피했는지 구분되지 않는다. 짙은 보라 `IFRAME BLOCK`과 함께 7발이 모두 지나가는 것을 본 적이 있다는 체감만 있다. **부분 통과로 기록한다.** 확정하려면 Ghost가 예측 임팩트를 생략할 때 남기는 클라이언트 로그가 필요하다(Phase 2에서 투사체 예측 경로를 다시 건드릴 때 함께 넣는 것을 제안한다).

## Phase 1 완료 (2026-11-03)

| 조건 | 결과 |
|:---|:---|
| 4개 경로 모두 무적 중 피해 0 | 근접(D-01)·투사체(D-03)·폭발(D-05)·화염(D-06) 통과 |
| 투사체 관통, 재피격 없음 | D-03·D-04 로그상 통과. 런처 탄 직격 관통 뒤 폭발까지 막힌 사례 있음 |
| 무적이 끝난 뒤 정상 피격 | 화염 재피격(D-06)으로 확인. D-02(일찍 대시해 칼날 안에 남음)는 재현이 어려워 코드 근거로만 둔다 |
| 리슨 2인 게스트 대시 무적 | D-07 서버 판정 통과, 게스트 화면 예측 임팩트는 부분 통과 |
| 리스폰 | D-08 통과 |
| 공통 회귀 | 사용자 육안상 이상 없음 |
| 클린 빌드 | 커밋 뒤 깨끗한 트리에서 게임 타깃 Build.bat 통과(unity 블롭 3개 재컴파일) |
