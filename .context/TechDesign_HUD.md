# HUD 기술 설계

## 1. 아키텍처 개요

MVVM 플러그인(`ModelViewViewModel`) 기반 반응형 HUD. GAS 이벤트가 위젯을 직접 건드리지 않고 ViewModel을
거치므로, 위젯 레이아웃 변경이 전투 로직에 영향을 주지 않는다.

표현 경로는 데이터의 성격에 따라 셋으로 갈린다 — **이 구분이 이 문서의 뼈대다.**

| 성격 | 경로 | 예 |
|:---|:---|:---|
| 값 | ASC 델리게이트 → ViewModel FieldNotify → BP 바인딩 | HP 바·탄약·조준점 (§3) |
| 이벤트("지금 시작됐다") | 게임플레이 델리게이트 → 위젯 함수 직접 호출 | 대시 쿨다운 파이 (§9) |
| 매 프레임 화면 좌표 | `ULNPHudWidget::NativeTick` → 커스텀 Slate 위젯 | 락온 마커·적 HP 바 (§11) |

---

## 2. 초기화 흐름

```
ALNPPlayerController::BeginPlay()              ← 로컬 컨트롤러만
  └─ CreateWidget<ULNPHudWidget>(HudWidgetClass) → AddToViewport()   ZOrder 0

OnPossess(Pawn)              ← 서버·리슨 호스트
AcknowledgePossession(Pawn)  ← 원격 클라이언트 (OnPossess는 서버에서만 돈다)
  └─ HudWidget->InitViewModel(PlayerState의 ASC, 폰의 ULNPCharacterMoverComponent)
       ├─ NewObject<ULNPHudViewModel>                          ← 한 번만
       ├─ ViewModel->Initialize(ASC)                           ← 초기값 + 델리게이트 등록
       ├─ UMVVMView->SetViewModel("HUD_ViewModel", ViewModel)  ← BP 바인딩 활성화
       └─ Mover->OnDashExecuted 구독 (기존 구독을 먼저 끊는다 — 재빙의 중복 방지)

OnUnPossess() / NativeDestruct()
  └─ DeinitViewModel() → ASC·Mover 델리게이트 전체 해제
```

- **ASC는 `GetPlayerState<ALNPPlayerState>()`로 직접 얻는다** — 폰의 PlayerState 캐싱 타이밍에 무관하다.
- **두 진입점이 겹쳐도 안전하다.** ViewModel은 1회 생성이고 `Initialize`가 선두에서 `Deinitialize()`를 부른다.
- 수명 관리 3중 안전망: `OnUnPossess` / `NativeDestruct` / `Initialize` 선두 — 어떤 순서로 파괴·재빙의가
  일어나도 델리게이트가 새지 않는다.
- Mover 인자는 대시 쿨다운 표시용이며 null이어도 무방하다.

---

## 3. 런타임 갱신 흐름 (`HealthPercent` 예시)

```
GE 적용 → ASC가 Health 수정 → GetGameplayAttributeValueChangeDelegate
  └─ ULNPHudViewModel::OnHealthChanged  (CachedHealth 갱신)
       └─ UpdateHealthPercent → UE_MVVM_SET_PROPERTY_VALUE(HealthPercent, …)
            ├─ 값이 같으면 조기 종료          ← 틱 없는 이벤트 기반의 핵심
            └─ FieldNotify → MVVM View → ProgressBar->SetPercent
```

`bIsFreeAiming`도 같은 패턴 — `RegisterGameplayTagEvent(TAG_AimMode_FreeAim)`가 추가/제거를 알리면
`SetIsFreeAiming(Count > 0)`.

---

## 4. 클래스 목록

| 클래스 | 파일 | 역할 |
|:---|:---|:---|
| `ULNPHudViewModel` | `UI/LNPHudViewModel.h/.cpp` | ViewModel. FieldNotify 프로퍼티 보유, ASC 델리게이트 구독 |
| `ULNPHudWidget` | `UI/LNPHudWidget.h/.cpp` | HUD 위젯 C++ 기반. ViewModel 주입 + 마커 드라이버(`NativeTick`) |
| `ALNPPlayerController` | `Player/LNPPlayerController.h/.cpp` | 위젯 생성(BeginPlay), ViewModel 초기화(빙의), 사망 오버레이 표시·해제 |
| `ULNPDeathScreenWidget` | `UI/LNPDeathScreenWidget.h/.cpp` | 사망~리스폰 반투명 오버레이 + 카운트다운 (→ §12) |

커스텀 Slate 위젯(`LNPUI` 모듈)과 마커 수집 계층은 §9·§11.9.

---

## 5. ViewModel 프로퍼티

| 프로퍼티 | 타입 | 갱신 트리거 |
|:---|:---|:---|
| `HealthPercent` | `float` (0~1) | `Health` 또는 `MaxHealth` 어트리뷰트 변경 |
| `bIsFreeAiming` | `bool` | `TAG_AimMode_FreeAim` 태그 추가/제거 |
| `AmmoText` | `FText` (`"{0} / {1}"` = 잔량/탄창) | `MagazineAmmo` 또는 `MagazineSize` 변경 |
| `bHasMagazine` | `bool` | `MagazineSize > 0` — 근접 무기면 탄약 표시를 숨긴다 |

> ⚠️ `AmmoText`는 `UE_MVVM_SET_PROPERTY_VALUE`를 쓰지 않는다 — FText는 값 비교로 알림을 거를 수 없어
> 매번 통지된다. 정수 캐시(`CachedMagazineAmmo/Size`)로 먼저 거른 뒤
> `UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED`로 직접 통지한다.
> 예측 발사의 차감도 현재값 변경이라 같은 델리게이트로 즉시 들어온다 — 연사 중 표시가 서버 확정을 기다리지 않는다.

---

## 6. Blueprint 설정 (WBP_LNPHud)

1. 기반 클래스 `ULNPHudWidget`
2. **View Model 패널** → `+ Add` → 클래스 `ULNPHudViewModel`, 이름 **`HUD_ViewModel`**, 생성 모드 **Manual**
3. `ProgressBar.Percent` ← `HUD_ViewModel.HealthPercent` (One Way)
4. 조준점 가시성: **Function Binding**으로 `bIsFreeAiming`을 읽어 `HitTestInvisible` / `Collapsed` 반환
5. `BP_LNPPlayerController.HudWidgetClass` = `WBP_LNPHud`
6. 커스텀 위젯 배치는 §9(대시 파이)·§11.11(마커 2종)

---

## 7. 어필 포인트 (설계 원칙)

- **GAS ↔ Widget 직접 결합 없음.** ViewModel이 유일한 중개자이고, 위젯 없이 단위 검증이 가능하다.
- **틱 없는 이벤트 기반.** ASC 델리게이트 → FieldNotify 체인. 동일 값 재전달은 알림이 생략된다.
  단, 이 원칙이 적용되지 않는 표현이 둘 있고 그 이유가 §9(이벤트)와 §11(매 프레임 좌표)이다.
- **멀티플레이 이중 진입점.** `OnPossess`(서버) / `AcknowledgePossession`(원격 클라) 양쪽에서 같은 초기화.

---

## 8. 인벤토리 패널 — ⛔ 폐기 (2026-08-07, 인게임 메뉴로 이관)

`ULNPInventoryWidget`·`ULNPInventoryEntryWidget`과 `WBP_Inventory`/`WBP_InventoryEntry`/`WBP_BuffEntry`,
`IA_ToggleInventory`는 **모두 삭제**되었다. 인벤토리는 CommonUI 기반 인게임 메뉴의 인벤토리 탭으로
대체되었다 — [TechDesign_InGameMenu.md](TechDesign_InGameMenu.md) 참조.

이관되며 바뀐 것: ListView → CommonTileView Grid + 디테일 패널, 엔트리 내장 버튼 → 디테일 패널 버튼,
장착본 숨김 → **장착 배지 표시**, 스탯 최종값 1줄 → 합/곱 분해 RichText 6행.

남길 가치가 있는 것은 이관되며 사라지지 않은 제약 둘뿐이다.

- ⚠️ **MVVM은 `UListView::ListItems`에 바인딩할 수 없다** (런타임 쓰기 불가 — 컴파일러가 거부).
  리스트·타일 데이터는 C++가 직접 채운다. 인게임 메뉴의 타일 뷰도 같은 이유로 ViewModel을 쓰지 않는다.
- ⚠️ **`BindWidget`/`BindWidgetOptional`은 위젯 BP에서 Is Variable이 켜져 있어야 붙는다.**
  이 프로젝트의 위젯 BP는 이 플래그가 기본 off인 경우가 잦아 §9·§11에서도 같은 함정을 밟았다.

(구 패널 상단의 스탯 리드아웃은 인게임 메뉴 스탯 탭의 합/곱 분해로 대체됐다.)

---

## 9. 대시 쿨다운 파이 — 커스텀 Slate 위젯 ✅ 완료 (2026-08-17)

방사형 쿨다운 스윕은 `UProgressBar`·`UImage` 조합으로 그릴 수 없다 — 부채꼴이 필요하다.
[Guide_CustomSlateWidget.md](Guide_CustomSlateWidget.md)의 절차를 실제로 적용한 첫 사례.

```
[LNPUI] (신규 Runtime 모듈 — LootNPop을 참조하지 않는다)
  FLNPRadialCooldownStyle : FSlateWidgetStyle   브러시·틴트·시작각(-90=12시)·시계방향·희망 크기
  SLNPRadialCooldown      : SLeafWidget         MakeCustomVerts로 삼각형 팬을 직접 그림
  ULNPRadialCooldownWidget: UWidget             UMG 래퍼 (팔레트 "LNP UI")

[LootNPop]
  ULNPCharacterMoverComponent  OnDashExecuted 델리게이트 + GetDashCooldown() (기본 1.0초)
       └─ ULNPHudWidget::HandleDashExecuted → DashCooldownWidget->StartCooldown(GetDashCooldown())
```

**왜 별도 모듈인가.** 위젯이 게임 타입을 하나도 모르게 하려면 물리적으로 참조할 수 없는 곳에 두는 게 가장
확실하다. `LNPUI`는 `LootNPop`에 의존하지 않으므로 게임 타입을 끌어 쓰는 순간 링크가 깨진다.

**왜 진행률을 `SLATE_ATTRIBUTE`로 열지 않았나.** 어트리뷰트로 열면 값을 매 프레임 밀어 주는 쪽에 Tick이
생기고, 어트리뷰트가 매 프레임 평가되며 위젯이 volatile이 된다. 대신 **위젯이 시작 시점에 duration 하나만
받고 경과 시간을 스스로 누적**한다 — 쿨다운이 없는 동안 `SetCanTick(false)`라 비용이 정확히 0이다.
무효화 사유는 `Paint`만 쓴다(부채꼴만 달라지고 희망 크기는 그대로). 희망 크기가 스타일에서 나오므로
**스타일 교체만 `Layout`**이다.

⚠️ **단, 무효화 사유 최소화는 위젯이 무효화 루트(fast path) 안에 있을 때만 이득이다.** 현재 HUD는
뷰포트에 그냥 올라가 있어 루트 밖이고, 그 경우 Slate가 매 프레임 전부 다시 그리므로 `Invalidate(Paint)`는
성능상 무의미하다(Invalidation Box로 감싸거나 `Slate.EnableGlobalInvalidation`을 켜면 그때부터 값을 한다).
반면 **`SetCanTick(false)`의 이득은 무효화 루트와 무관하게 항상 유효하다** —
`SWidget::Paint`가 `bCanTick`일 때만 `Tick`을 부른다.

**왜 MVVM을 안 쓰나.** MVVM은 *값* 바인딩용인데 쿨다운은 "지금 시작됐다"는 *이벤트*다.
`float Duration`을 FieldNotify로 노출하면 `UE_MVVM_SET_PROPERTY_VALUE`가 **같은 값(1.0초) 재대입 시
알림을 생략**해서(§7) 두 번째 대시부터 스윕이 안 도는 함정이 있다.

**구현 메모**
- 부채꼴 반지름은 위젯 사각형의 **반대각선** — 정사각형 아이콘의 네 모서리까지 덮어야 한다.
  넘치는 부분은 `SetClipping(EWidgetClipping::ClipToBounds)`로 잘라낸다.
- ⚠️ **Slate 쪽 `Construct`에서만 `SetClipping`을 걸면 UMG로 쓸 때 안 먹는다** (2026-08-17 실측).
  `UWidget::SynchronizeProperties`가 래퍼의 `Clipping` 프로퍼티(기본 `Inherit`)를 덮어쓴다.
  디자이너 프리뷰에서 부채꼴이 아이콘 밖으로 원반처럼 삐져나오는 증상으로 드러난다 —
  **래퍼 생성자에서도** 같은 값을 줘야 한다.
- 브러시에 리소스가 없으면 `MakeCustomVerts`에 넘길 `FSlateResourceHandle`이 무효가 되므로
  엔진 기본 `GenericWhiteBox`로 대체한다.
- 색은 `OverlayTint × 브러시 틴트 × InWidgetStyle.GetColorAndOpacityTint()` — 마지막 항을 빠뜨리면
  HUD 전체를 페이드아웃해도 이 위젯만 남는다.

**검증 (2026-08-17, PIE·디자이너 실측)** — 파이 스윕(12시·시계방향·1초), 반복 대시 시 처음부터 재시작,
클리핑(초기에 실패했던 항목), 부모 페이드 전파(루트 `RenderOpacity` 0.35), 쿨다운 중에만 Tick 활성
(`stat Slate`의 `SWidget::Tick (Count)` 481→482 후 복귀 — Dash Cooldown을 10초로 늘려 측정).
**무효화 사유가 Paint뿐인지는 ⛔ 현재 구성에서 측정 불가**(위 ⚠️ 참조).

**스킬 슬롯 연결 시:** `StartCooldown(Duration)`을 호출할 지점만 추가하면 된다. 액티브 스킬은 GAS 쿨다운 GE를
쓰므로 `ASC->GetActiveEffectsTimeRemainingAndDuration`으로 duration을 얻어 넘긴다. 단 현재는 스킬 입력이
무기 교체 테스트에 점유돼 있고(`ULNPInputHandlerComponent::OnActiveSkillStarted`가 `EquipTestWeapon`을 부른다)
스킬 DataAsset도 없다.

⚠️ **WBP 배선:** 팔레트 **LNP UI → LNP Radial Cooldown**을 변수명 `DashCooldownWidget`으로 배치하고
**Is Variable을 켠다**(§8의 함정과 동일). 배치는 HP 바 좌측 상단(하단 중앙 앵커, 오프셋 -200/-130, 64×64),
그 아래 `DashIcon`(`UImage`)이 `/Game/UI/Icons/T_DashIcon`을 그린다. 소스 PNG는 `Art/Icons/T_DashIcon.png`
(에디터 Reimport 가능), 텍스처 설정은 UI용 `TEXTUREGROUP_UI` + `TC_EditorIcon` + `TMGS_NoMipmaps` + `NeverStream`.

---

## 10. 미구현 항목

- **HUD 추가 요소:** 미니맵, 점수/메달 카운터 등 (DevelopmentPlan Phase 6).
- **루팅 게이지 HUD:** `ALNPLootPod::GetGaugePercent()`는 복제되고 있으나 읽는 위젯이 없다.
  시안 미확정으로 보류 — 요소 후보와 기각 사유는 [Idea_Backlog.md](Idea_Backlog.md).
- **Active Skill 슬롯 쿨다운:** 스킬 발동 자체가 미구현이라 보류 — 생기면 §9의 위젯을 그대로 재사용한다.
- **적 HP 바 가림 처리:** 스크린 스페이스 마커는 완료(§11), 월드 지오메트리 뎁스 가림만 보류 — §11.5.

---

## 11. 락온 마커 · 적 HP 바 — 스크린 스페이스 마커 ✅ 완료 (2026-09-09)

순수 엔티티(`CombatMode::PureEntity`)에는 Actor가 없어 `UWidgetComponent`를 달 방법이 **자체가 없다.**
그래서 적 위에 뜨는 두 표현(락온 마커·HP 바)을 월드 스페이스 위젯에서 **스크린 스페이스 커스텀 Slate 위젯**으로
옮겼다. §9의 `LNPUI` 형틀을 재사용한 두 번째 사례다.

### 11.1 동기 — 두 가지이고 무게가 다르다

| 동기 | 성격 |
|:---|:---|
| 위젯 N개 → 1개로 줄여 드로우·Tick 비용 절감 | **추정** — High LOD 적 동시 수가 실측되지 않았다 |
| **Low LOD(순수 엔티티) 적의 표현** | **기능 요구** — 액터가 없으면 `UWidgetComponent`를 달 수 없다 |

두 번째가 진짜 이유다. 성능은 실측 전까지 근거가 약하고, 이번 작업의 근거도 성능이 아니었다.

### 11.2 잃은 것은 뎁스 가림 하나뿐이다

⚠️ **월드 스페이스 `UWidgetComponent`에서 공짜로 얻고 있던 것은 뎁스 가림이다.** 씬에 쿼드로 그려져
뎁스 테스트를 타므로 벽 뒤 적의 HP 바가 자동으로 가려진다. "원근 스케일·깊이 가림"을 한 덩어리로 보면
크게 느껴지지만 쪼개면 다르다.

| 항목 | 실제 작업량 | 결과 |
|:---|:---|:---|
| 마커끼리 앞뒤 정렬 | 거리 정렬 후 그리는 순서만 바꿈 — 사실상 0 | ✅ |
| 원근 스케일 | `Scale = HpBarScaleDistance / Distance` 클램프 한 줄 | ✅ (하한 `HpBarMinScale`) |
| **월드 지오메트리 가림** | **여기만 진짜 작업** | ⛔ 보류 (§11.5) |

옮기면서 매 프레임 카메라 빌보드 회전(구 `ALNPEnemyCharacter::Tick`)도 함께 없어졌다.

### 11.3 선결 조건이었던 Health 복제 — ✅ 해소 (2026-09-09)

`FLNPEnemyFragment::Health`는 서버 전용이라 클라이언트에서는 기본값에 머물렀다. **AimPitch가 상태 채널에
들어온 절차 그대로** `FLNPReplicatedAgent`에 `uint8 HealthPct`를 더해 해소했다 —
`PositionYaw`의 형제 멤버라 위치가 바뀔 때 함께 실린다.

- **비율만 싣는다.** MaxHealth는 전투 중 불변이고 표시에 필요한 것도 비율뿐이다.
- 클라이언트에서 버블 핸들러(`ApplyReplicatedHealth`)가 **서버가 쓰는 것과 같은 프래그먼트**
  (`FLNPEnemyFragment::Health`)에 `Pct/255 × MaxHealth`로 되쓴다 — 소비처가 넷 모드를 모르게 하는
  단일 소비 경로 규약([TechDesign_EnemyNPC_LowLOD.md](TechDesign_EnemyNPC_LowLOD.md) §5.3) 그대로다.
  ⚠️ 클라의 `MaxHealth`는 템플릿 기본값이라 서버 실제값과 다를 수 있다(HP 원본이 무기의 스탯 수정자다).
  복원되는 것은 **비율만 정확한 근사 절대값**이므로 판정에 쓰면 안 된다.
- ⚠️ **양자화 함수를 두 벌 두지 않는다.** 복제 페이로드와 표시 장부가
  `FLNPEnemyHealthDisplayFragment::EncodePct` 하나를 공유한다. 갈라지면 게스트가 수신값을 다시
  인코딩했을 때 값이 미세하게 달라져 **매 프레임 "HP가 변했다"로 읽힌다**(= §11.4의 강제 포함이 항상 참이 된다).
- **갱신 주기 게이트는 우회시키지 않았다.** 우회는 시작 시각을 놓치면 통째로 못 보는 일회성 연출에만 준다.
  HP 바가 최대 한 주기(Low 0.3초) 늦는 것은 눈에 띄지 않고, 여기까지 넓히면 피격이 곧 갱신이 되어 갱신 수가 는다.
- 비용: 형제 멤버라 안 바뀐 갱신에서 **1비트**. 피격 순간에는 이미 위치·Stagger로 Dirty가 걸려 있으므로
  **갱신 횟수 증가는 사실상 0**이다 (비용은 바이트가 아니라 갱신 횟수로 센다 —
  [Guide_NetBandwidth.md](Guide_NetBandwidth.md) §2.1). ⚠️ 아직 실측하지 않았다.

### 11.4 표시 개수 제한 — 채택한 규칙

화면에 HP 바가 너무 많으면 보기 나쁘다. 상한을 두면 **가림 트레이스 대상도 같이 줄어들어** 비용이
상수화되는 부수 효과가 있다(§11.5를 나중에 붙일 때).

- 게이트: 살아 있을 것(`Health > 0` **그리고** `Action != Dying`) + 만피가 아닐 것 + 카메라 반각 이내 +
  최대 표시 거리 이내. 사망은 HP가 먼저 0이 되고 `Dying` 전이가 한 틱 늦을 수 있어 둘 다 본다.
- 정렬은 **최근 피해 여부가 1순위, 거리가 2순위**다. 거리순 단독이면 라이플로 저격한 먼 적이 근처
  잡몹에 밀려 안 보인다. 방금 회복해 만피가 된 적도 같은 이유로 잠깐 남는다.
- 파라미터는 `ULNPHudWidget`의 `HpBarMaxCount`(20) / `HpBarMaxDistance`(4000cm) / `HpBarMaxAngleDeg`(70) /
  `HpBarRecentDamageSeconds`(3) / `HpBarExitMargin`(4) / `HpBarFadeSeconds`(0.2).
- ⚠️ 이때 필요한 "언제 변했는가"를 **서버가 실어 보내지 않는다.** `ULNPEnemyMarkerProcessor`가
  *자기가 본 HP가 바뀐 순간*을 각 머신에서 로컬로 기록한다(`FLNPEnemyHealthDisplayFragment`) —
  호스트는 실제 피해를, 게스트는 복제된 비율의 변화를 본다. 같은 사건을 각자 관측하므로 **추가 대역폭이 0**이다.
- ⚠️ **경계 점멸 방지** — 20위와 21위가 순위를 오가면 HP 바가 깜빡인다.
  **진입은 상위 `HpBarMaxCount`, 이탈은 `HpBarExitMargin`만큼 아래까지** 버티게 하고,
  이탈은 `HpBarFadeSeconds` 동안 페이드아웃한다. 히스테리시스의 절반은 수집 상한을
  `MaxCount + ExitMargin`으로 넉넉히 받아 두는 데 있고, 나머지 절반은 게임 스레드의 표시 상태 맵에 있다.
- 관측 갱신은 **게이트보다 먼저, 표시 여부와 무관하게** 한다. 화면 밖에서 맞은 적이 돌아섰을 때
  "최근 피해"로 잡혀야 하기 때문이다.

### 11.5 가림 처리 — ⛔ 보류

프로젝트에 재활용할 LOS 데이터가 없다(적 Mass 쪽에 가시성 트레이스 없음, 전체에서 가시성 트레이스는
`LNPAbility_RangedAttack.cpp` 한 곳뿐). 새로 짜야 한다. **이번 범위에서 뺐다** — 표현이 아예 없는 것과
가려지지 않는 것 사이의 거리가, 가리는 것과 안 가리는 것 사이보다 훨씬 멀기 때문이다.

착수하게 되면: 대상은 §11.4에서 이미 수십 개로 줄어 있다 / 비동기 라인 트레이스를 프레임당 N개씩
라운드로빈(예: 8개/프레임, 결과는 다음 갱신까지 유지 — 100~200ms 지연은 HP 바에서 눈에 띄지 않는다) /
0·1 토글이 아니라 알파 페이드로 반영하면 판정 지연이 더 가려진다(이미 페이드 경로가 있어 값만 곱하면 된다).

⚠️ **락온 마커에는 애초에 필요 없다.** 내가 지목한 대상이 엄폐물 뒤로 들어갔다고 표식이 사라지면 곤란하다.

### 11.6 위젯 — 한 클래스, 스타일로 룩 분기

§9에서 만든 `LNPUI` 모듈의 형틀(스타일 구조체 + `SLeafWidget` + UMG 래퍼)을 그대로 재사용했다.
위젯은 **스크린 좌표·비율·알파·스케일 배열만 받고** 적이 무엇인지 모른다.

**락온 마커와 HP 바는 같은 클래스의 두 인스턴스다.** 스타일의 `bDrawFill` 하나가 룩을 가른다 —
락온은 배경 브러시만, HP 바는 배경 + `Ratio`만큼의 채움. 두 클래스로 나누면 배칭·틴트·클리핑 처리가
두 벌이 되는데, 정작 다른 것은 사각형을 하나 더 그리는지뿐이었다.

⚠️ **원소가 1개라고 API를 스칼라로 좁히지 않았다.** 배열로 두었기에 HP 바가 위젯 코드 변경 없이 그대로 올라탔다.

- 진행률을 `SLATE_ATTRIBUTE`로 열지 않는다 — 매 프레임 평가되며 위젯이 volatile이 된다. 세터로 민다.
- `SetCanTick(false)` — 위젯이 스스로 세는 시간이 없다(대시 파이는 경과를 자기가 쌓았다는 점이 다르다).
- 전체 마커를 브러시당 `MakeCustomVerts` **한 번**으로 배칭한다 — 마커가 몇 개든 드로우 콜은 최대 2개.
  채움은 배경보다 레이어를 하나 올려 그린다(같은 레이어에서는 순서가 보장되지 않는다).
- ⚠️ 커스텀 정점이라 **단순 `Image` 브러시만 지원한다**(9-slice·Tile 불가).
- `ComputeDesiredSize`는 0을 돌려준다 — 화면 전체를 덮는 오버레이라 희망 크기를 주장하지 않는다.
- `SetClipping(ClipToBounds)`는 §9와 같은 이유로 **Slate `Construct`와 UMG 래퍼 생성자 양쪽에** 건다.

### 11.7 락온 마커가 먼저였다 (2026-09-06 결정 → 2026-09-09 구현)

락온 마커도 `UWidgetComponent`였고, 순수 엔티티에 락온을 걸면 **표시할 방법이 없다는 같은 벽**에
부딪혔다 → [TechDesign_TargetQuery.md](TechDesign_TargetQuery.md) §7.

**ISM 인스턴스·Niagara로 엔티티용 마커만 따로 만드는 안은 기각했다.** *Actor용 위젯 마커*와 *엔티티용 월드
마커* 두 갈래를 영구히 유지하게 되기 때문이다 — 이 프로젝트가 순수 엔티티 도입 이후 반복해 밟은 그 함정이다.
락온 마커는 성격상 UI라(거리와 무관하게 일정한 크기) 스크린 스페이스가 자연스럽기도 하다.

⚠️ **락온 마커는 §11의 비싼 부분을 하나도 쓰지 않는다** — 그래서 HP 바를 기다릴 이유가 없었고,
형틀(§11.6)의 첫 소비자로 먼저 태워 구조를 검증한 뒤 HP 바를 얹었다.

| 항목 | HP 바 | 락온 마커 |
|:---|:---|:---|
| §11.3 Health 복제 | 필수 선결 | 불필요 — 체력을 표시하지 않는다 |
| §11.4 개수 제한·히스테리시스 | 필수 | 불필요 — 항상 정확히 1개 |
| §11.5 가림 트레이스 | 진짜 작업(보류) | 불필요 — 가려지지 않는 편이 옳다 |
| §11.6 위젯 형틀 + 스크린 투영 | 필요 | **필요 — 이것만 쓴다** |

⚠️ **`ALNPEnemyCharacter::LockOnMarkerComponent`는 이 작업 전에 이미 제거됐다 (2026-09-06).**
"마커 표현이 생길 때까지 승격 Actor용 옛 경로를 남겨 둔다"고 적었던 이전 판을 뒤집은 결과다.
남겨 두었더니 **락온 해제 경로에서 게스트가 크래시했다** — `ClearTarget()`이 도는 가장 흔한 이유가
"대상 소멸"인데, 바로 그 경로에서 이미 사라진 엔티티로 Actor를 조회했기 때문이다
(`GetActorFromHandle` → `InternalGetFragmentDataPtr`의 `CurrentArchetype` assert).

> 교훈: **"나중에 지울 코드"를 과도기에 살려 두는 비용에 크래시 위험이 포함될 수 있다.**
> 특히 그 코드가 Actor 수명과 엔티티 수명을 섞어 쓰는 자리라면 더 그렇다.

### 11.8 승격 Actor의 월드 HP 바도 함께 제거했다

`FLNPEnemyFragment::Health`가 두 모드 공통의 단일 원본이고(승격 중에는 ASC가 권위이되
`ALNPEnemyCharacter::SyncToEntity`가 매 틱 프래그먼트로 되돌린다), 새 경로가 승격 Actor도 그대로 덮는다.
그래서 `HpBarComponent`·`HpBarWidgetClass`·`ULNPHpBarWidget`·`RefreshHpBar`·ASC Health 구독·빌보드 회전을
**전부 삭제**했다. 남겨 두었다면 적 종류에 따라 표현이 갈려 §11.7이 기각한 두 갈래가 다시 생긴다.
대가인 승격 Actor의 뎁스 가림은 §11.5와 함께 되찾는다.

### 11.9 구현 구조

| 계층 | 파일 | 역할 |
|:---|:---|:---|
| 위젯 | `Source/LNPUI/.../LNPScreenMarkerStyle.h` | 브러시 2종·틴트·크기·피벗·`bDrawFill`·`FillInset` |
| 위젯 | `Source/LNPUI/.../SLNPScreenMarkers.h` | `SLeafWidget`. `FLNPScreenMarker` 배열을 받아 `MakeCustomVerts`로 배칭 |
| 위젯 | `Source/LNPUI/.../LNPScreenMarkerWidget.h` | UMG 래퍼 (팔레트 "LNP UI"). 디자이너 프리뷰 포함 |
| 드라이버 | `UI/LNPHudWidget.cpp` `NativeTick` | 투영·히스테리시스·페이드·원근 스케일. 마커 위젯 2개와 사거리 라벨(§11.13)을 먹인다 |
| 투영 | `UI/LNPScreenProjection.h` | 월드 → 위젯 로컬. 카메라 뒤 판정과 DPI 나눗셈을 여기서 닫는다 |
| 수집 | `Enemy/LNPEnemyMarkerSubsystem.h` | 파라미터 쓰기 / 결과 읽기. 잠금 관례는 `ULNPTargetQuerySubsystem`과 같다 |
| 수집 | `Enemy/LNPEnemyMarkerProcessor.cpp` | 게이트·정렬·상한 N. 관측 시각도 여기서 갱신 |
| 복제 | `Replication/LNPMassReplication.h` · `LNPMassReplicator.cpp` | `HealthPct` 1바이트 |

수집 프로세서는 **`PrePhysics`**(엔진 표현 체인·HUD Tick과 같은 페이즈 — 같은 프레임에 쓰고 읽는다)이고
**`ExecutionFlags = All`**이다. 리슨 호스트도 HP 바를 봐야 하므로 클라이언트 전용이 아니다.
⚠️ 순서 선언은 페이즈를 건너지 못하므로, 순서 제약이 필요해지면 반드시 같은 페이즈 안에서 건다.

**HUD 위젯이 Tick하게 된 것이 이번 작업의 유일한 구조 변경이다.** 대시 쿨다운은 이벤트라 푸시로 족했지만,
마커는 **매 프레임 다시 계산되는 화면 좌표**라 값 바인딩으로 표현할 수 없다. 락온 컴포넌트가 위젯을 미는
방식은 게임플레이 컴포넌트가 HUD를 알게 되므로 택하지 않았다.

수집을 `ULNPTargetQuerySubsystem`에 얹지 않은 이유는 계약이 다르기 때문이다 — 저쪽은 **최선 1개**,
이쪽은 **상위 N개**다. 잠금 관례와 `TMassExternalSubsystemTraits` 선언은 그대로 베꼈다.

### 11.10 ⚠️ 함정 넷

1. **좌표 단위.** `ProjectWorldLocationToScreen`은 **뷰포트 픽셀**을, 위젯 로컬은 **슬레이트 단위**를 쓴다.
   DPI 스케일로 나누지 않으면 스케일이 1이 아닌 화면에서 마커가 대상에서 비례해 밀린다.
   이것이 성립하려면 마커 위젯이 **Canvas 앵커 (0,0)-(1,1)·오프셋 0**이어야 한다.
2. **카메라 뒤 대상.** 투영은 뒤쪽 대상에도 `true`를 돌려주면서 좌표를 뒤집는다 — 뒤에 있는 적의 마커가
   화면 반대편에 찍힌다. 투영 **전에** 전방 판정(`Dot > 0`)을 건다.
3. **엔티티 핸들을 표시 측에서 다시 해석하지 않는다.** 락온 마커는 `Track` 질의가 채워 둔 **캐시 좌표**를 쓴다.
   핸들을 다시 조회하면 "대상이 사라져서 락온이 풀리는" 가장 흔한 경로에서 §11.7의 크래시를 되풀이한다.
4. **`BindWidgetOptional`은 Is Variable을 켜야 붙는다** (§8·§9에서 두 번 밟았다).

### 11.11 WBP 배선 (`WBP_LNPHud`) ✅ 완료

팔레트 **LNP UI → LNP Screen Marker** 2개. 둘 다 루트 Canvas에 **앵커 (0,0)-(1,1)·오프셋 0·정렬 (0,0)** —
§11.10의 좌표 변환이 성립하는 조건이다.

| 변수명 | 스타일 | ZOrder |
|:---|:---|---:|
| `LockOnMarkerWidget` | `bDrawFill=false`, `BackBrush`=`T_LockOnReticle`, `BackTint` 금색(1, 0.85, 0, 0.9), 64×64, 피벗 (0.5, 0.5) | -1 |
| `EnemyHpBarWidget` | `bDrawFill=true`, 브러시 없음(흰 박스 폴백), 배경 α0.75 검정 + 채움 붉은색, 56×7, 피벗 **(0.5, 1)**, `FillInset` 1 | -2 |

- HP 바의 피벗이 **아래 중앙**인 이유: 넘겨받는 좌표가 `HpBarHeightOffset`(110cm)만큼 올린 머리 지점이라,
  바가 그 위에 얹혀야 머리를 가리지 않는다.
- 락온 레티클은 `Art/Icons/T_LockOnReticle.png`(128², 십자 4방향이 끊긴 링)을 `/Game/UI/Icons/T_LockOnReticle`로
  임포트했다 — 텍스처 설정은 `T_DashIcon`과 같다.
  ⚠️ **옛 `WBP_LockOnMarker`는 텍스처 없는 RoundedBox 링이었고 그대로 못 옮긴다** —
  커스텀 정점 경로는 단순 Image 브러시만 지원하기 때문이다(§11.6). 링은 흰색으로 굽고 색은 틴트가 준다.
- HP 바는 브러시를 비워 둔다. 리소스가 없으면 엔진 기본 `GenericWhiteBox`로 폴백하므로
  **단색 사각형이 정확히 필요한 그림**이고, 텍스처를 하나도 안 만들어도 된다.

⚠️ **EntityConfig DA는 재저장할 필요가 없다.** 이 문서의 이전 판이 그렇게 적었으나 틀렸다 —
`FLNPEnemyHealthDisplayFragment`는 `ULNPEnemyTrait::BuildTemplate`이 **런타임에** 붙이고,
DA에 직렬화되는 것은 트레이트 목록이지 프래그먼트 목록이 아니다.

**함께 지운 것:** `WBP_LNPHpBar`, `WBP_LockOnMarker`. 둘 다 `BP_LNPEnemy`가 참조하고 있었는데,
그 참조는 이미 제거된 UPROPERTY의 **잔존 값**이었다.
⚠️ **잔존 참조는 컴파일만으로는 안 지워진다** — 패키지를 실제로 다시 써야 한다.
`compile_blueprint` 후 `save_assets`는 더티가 아니라 아무것도 안 쓰고 true를 돌려준다.
CDO 프로퍼티를 하나 건드려 더티로 만든 뒤 저장해야 의존성 목록에서 빠진다.

### 11.12 검증 현황 (2026-09-09, 2P 호스트·게스트)

✅ 컴파일 / `BindWidgetOptional` 성립(`GetWidgets`가 두 위젯을 `bInherited: true`로 돌려주는 것이 증거) /
디자이너 프리뷰·클리핑 / 락온 마커 추적(순수 엔티티·승격 Actor, 호스트·게스트) / 적 HP 바 게스트 동기화 /
DPI 스케일 / 개수 상한·히스테리시스·페이드 / 부모 페이드 전파 / 사망 순간 시체 잔류 0건.
⛔ **HP 비율 복제의 대역폭 영향은 미측정** — §11.3의 "갱신 횟수 증가 0"은 계산이지 실측이 아니다.

⭐ **개수 상한을 재는 법 — 적을 20마리 모아 때리지 말고 상한을 내린다.**
`HpBarMaxCount = 3` · `HpBarExitMargin = 1`로 두고 10마리 이상에게 난사하면, 표시가
**정확히 4개**(진입 상위 3위 + 이탈 여유 1)에서 멈추고 페이드가 걸리는 것이 바로 보인다.
20마리를 동시에 피격시키는 구성을 만드는 것보다 훨씬 싸고, **히스테리시스 산수를 직접 확인**해 준다 —
기본값(20/24)에서는 경계에 도달하는 상황 자체를 만들기 어려워 사실상 검증 불가다.

⚠️ **DPI 스케일은 창 크기를 크게·작게 두 번 바꿔야만 검증된다.** 스케일이 1에 가까운 해상도 하나만 보면
§11.10-1의 나눗셈이 틀려도 증상이 안 보인다.

---
### 11.13 유탄 사거리 라벨 (`LobbedRangeLabel`)

유탄 ADS 중 착탄 표식 우상단에 **사거리**를 띄운다(`58 m`). 박격포 조준경의 계기 판독을 의도한 표현이다.

⚠️ **체공시간을 함께 띄웠다가 뺐다**(2026-09-18 플레이 테스트). "곡사포는 체공이 길어 리드가 필요하다"는
계산은 맞았지만, 실제로는 아무도 그 숫자를 보고 리드하지 않았다 — 리드는 장판을 옮겨서 하지 초 단위를
읽어서 하지 않는다. 숫자 두 개는 판독만 느리게 했다. `PredictArc`가 체공시간을 함께 돌려주던 경로도
같이 걷어냈다(쓰는 곳이 없어졌으므로).

| 판단 | 근거 |
|:---|:---|
| **스크린 스페이스 `UTextBlock`** (월드 `UTextRenderComponent` 아님) | §11.7과 같은 판단이다. 거리와 무관하게 일정한 크기인 UI이고, 월드 텍스트는 지형에 가리거나 원근으로 찌그러진다. 투영도 `LNPScreenProjection` 하나를 그대로 쓴다 |
| **HUD가 당겨 읽는다** — 가이드가 위젯을 밀지 않는다 | §11.9에서 락온에 대해 내린 것과 같은 판단. `ULNPTrajectoryGuideComponent::GetImpactReadout()`이 착탄점·체공시간을 노출만 하고, 게임플레이 컴포넌트는 HUD의 존재를 모른다 |
| 착탄점을 **다시 풀지 않는다** | 라벨이 궤적을 자기 나름대로 적분하면 라벨이 말하는 거리와 장판이 놓인 곳이 갈린다. `ArcPoints.Last()` — 장판이 쓰는 바로 그 좌표를 읽는다 |
| 폰트 **`bForceMonospaced`** | 비례폭이면 숫자가 바뀔 때마다 라벨 폭이 변해 좌우로 떨린다. 계기 판독이라는 인상도 여기서 나온다 |
| 오프셋은 **화면 단위**(기본 (44, -44) = 우상단), 월드 오프셋 아님 | §11.7과 같다 — 거리와 무관하게 표식에서 일정한 자리에 붙어야 한다. 표식 **바로 위**에 두면 장판의 위쪽 테두리와 겹쳐 읽기 나쁘다 |

**WBP 배선:** 루트 Canvas 직속 `UTextBlock` `LobbedRangeLabel` — 앵커 (0,0)·정렬 **(0.5, 0.5)**·`bAutoSize`·
ZOrder 5. 정렬이 중앙이라 오프셋이 곧 표식으로부터의 거리가 된다. Roboto Bold 16·호박색(1, 0.82, 0.45)이고
`BindWidgetOptional`이라 위젯을 지우면 표시만 조용히 사라진다.

⚠️ **폰 컴포넌트 캐시(`CachedPawn`·`CachedLockOn`·`CachedTrajectoryGuide`)를 `NativeTick`으로 올렸다.**
이전에는 `UpdateLockOnMarker` 안에 있었는데, 그 함수는 락온 마커 위젯이 없으면 먼저 빠져나간다 —
그대로 두면 WBP에서 락온 마커를 지우는 순간 사거리 라벨도 함께 죽는다.

---

## 12. 사망 오버레이 — 리스폰 카운트다운 ✅ 완료 (2026-08-21)

사망~리스폰 사이에만 화면을 덮는 반투명 오버레이. 사망 흐름 전체는
[TechDesign_CharacterMovement.md](TechDesign_CharacterMovement.md) §9 참조.

| 요소 | 내용 |
|:--|:--|
| C++ | `ULNPDeathScreenWidget` — `ShowCountdown(초)` / `HideCountdown()`, `BindWidget CountdownText` |
| 에셋 | `WBP_LNPDeathScreen`(`/Game/UI/`) — Border(검정 α0.65) → VerticalBox → `TitleText`("YOU DIED") + `CountdownText` |
| 연결 | `BP_LNPPlayerController.DeathScreenWidgetClass` |
| 생성 시점 | **첫 사망 때 lazy 생성.** 죽지 않는 판에서는 위젯을 아예 만들지 않는다 |
| ZOrder | **5** — HUD(0)보다 위, 메뉴 레이아웃(10)보다 아래. 죽은 채로 인벤토리 메뉴를 열 수 있어야 한다 |

**남은 시간은 각 클라이언트가 로컬로 센다.** 서버 타이머와 시계를 맞추지 않는다 —
`ShowCountdown`이 로컬 월드 시각 + `ULNPSettings::PlayerRespawnDelay`로 만료 시각을 한 번 확정하고,
1초 반복 타이머가 `CeilToInt`로 갱신한다. 오차는 편도 지연 수준이고, 이 방식은 버프 잔여 시간 표시와 같은 패턴이다.

**0에 닿아도 위젯이 스스로 숨지 않는다** — 서버의 리스폰 타이머가 로컬 카운트보다 조금 늦게 도착할 수 있어서,
오버레이를 걷는 것은 리스폰 빙의(`OnPossess` / 원격 클라의 `AcknowledgePossession`)의 몫이다. 카운트만 멈춘다.

**문구는 영문 원본**(`Respawning in {0}`, `NSLOCTEXT`) — 프로젝트 로컬라이제이션 규약
([TechDesign_InGameMenu.md](TechDesign_InGameMenu.md) §12)을 따른다.
한국어 표시는 `Content/Localization/Game/ko/Game.po`에 번역을 채우면 된다.
