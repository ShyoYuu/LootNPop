# HUD 기술 설계

## 1. 아키텍처 개요

MVVM 플러그인(`ModelViewViewModel`)을 활용한 반응형 HUD. GAS 이벤트가 직접 위젯을 건드리지 않고 ViewModel을 통해 간접 갱신되므로, 위젯 레이아웃 변경이 전투 로직에 영향을 주지 않는다.

```
[ALNPPlayerController]
  ├── BeginPlay()   → CreateWidget<ULNPHudWidget> + AddToViewport
  └── OnPossess()   → HudWidget->InitViewModel(PlayerState->ASC)

[ULNPHudViewModel]  (UMVVMViewModelBase)
  ├── Initialize(ASC)  → 초기값 읽기 + 델리게이트 등록
  ├── HealthPercent    (FieldNotify float)
  └── bIsFreeAiming    (FieldNotify bool)

[ULNPHudWidget]  (UUserWidget)
  └── MVVM View가 FieldNotify를 받아 Blueprint 바인딩 자동 평가
```

---

## 2. 초기화 흐름

```
ALNPPlayerController::BeginPlay()
  └─ CreateWidget<ULNPHudWidget>(this, HudWidgetClass)
  └─ HudWidget->AddToViewport()           ← 위젯 화면에 등록, NativeConstruct 호출

ALNPPlayerController::OnPossess(Pawn)            ← 서버/리슨호스트 경로
ALNPPlayerController::AcknowledgePossession(Pawn) ← 원격 클라이언트 경로 (OnPossess는 서버에서만 호출됨)
  └─ GetPlayerState<ALNPPlayerState>()    ← Controller 직접 접근 (Pawn 캐싱 불필요)
  └─ HudWidget->InitViewModel(ASC)
       ├─ NewObject<ULNPHudViewModel>      ← ViewModel 생성 (한 번만)
       ├─ ViewModel->Initialize(ASC)
       │    ├─ ASC->GetNumericAttribute()  ← Health/MaxHealth 초기값 읽기
       │    ├─ ASC->HasMatchingGameplayTag(TAG_AimMode_FreeAim)  ← 초기 조준 모드
       │    ├─ GetGameplayAttributeValueChangeDelegate().AddUObject()  ← HP 변경 구독
       │    └─ RegisterGameplayTagEvent(TAG_AimMode_FreeAim).AddUObject()  ← 태그 변경 구독
       └─ UMVVMView->SetViewModel(FName("HUD_ViewModel"), ViewModel)
            └─ Blueprint 바인딩 활성화

ALNPPlayerController::OnUnPossess()
  └─ HudWidget->DeinitViewModel()
       └─ ViewModel->Deinitialize()       ← ASC 델리게이트 전체 해제
```

---

## 3. 런타임 갱신 흐름 (`HealthPercent` 예시)

```
피해량 적용 (GAS GameplayEffect)
  └─ ASC가 Health 어트리뷰트 수정
       └─ GetGameplayAttributeValueChangeDelegate 발송
            └─ ULNPHudViewModel::OnHealthChanged(Data)
                 ├─ CachedHealth = Data.NewValue
                 └─ UpdateHealthPercent()
                      └─ SetHealthPercent(CachedHealth / CachedMaxHealth)
                           └─ UE_MVVM_SET_PROPERTY_VALUE(HealthPercent, value)
                                ├─ 값 대입 (변경 없으면 조기 종료)
                                └─ FieldNotify 알림 발송
                                     └─ MVVM View → ProgressBar->SetPercent(value)  ← 화면 갱신
```

`bIsFreeAiming`도 동일 패턴. `RegisterGameplayTagEvent`가 `TAG_AimMode_FreeAim` 추가/제거를 감지하면 `SetIsFreeAiming(Count > 0)` 호출 → FieldNotify → Blueprint 바인딩 평가.

---

## 4. 클래스 목록

| 클래스 | 파일 | 역할 |
|:---|:---|:---|
| `ULNPHudViewModel` | `UI/LNPHudViewModel.h/.cpp` | ViewModel. FieldNotify 프로퍼티 보유, ASC 델리게이트 구독 |
| `ULNPHudWidget` | `UI/LNPHudWidget.h/.cpp` | HUD 위젯 C++ 기반. ViewModel 생성·주입·해제 담당 |
| `ALNPPlayerController` | `Player/LNPPlayerController.h/.cpp` | 위젯 생성(BeginPlay), ViewModel 초기화(OnPossess), 사망 오버레이 표시·해제 |
| `ULNPDeathScreenWidget` | `UI/LNPDeathScreenWidget.h/.cpp` | 사망~리스폰 반투명 오버레이 + 카운트다운 (→ §12) |

---

## 5. ViewModel 프로퍼티

| 프로퍼티 | 타입 | 갱신 트리거 |
|:---|:---|:---|
| `HealthPercent` | `float` (0~1) | `Health` 또는 `MaxHealth` 어트리뷰트 변경 시 |
| `bIsFreeAiming` | `bool` | `TAG_AimMode_FreeAim` 태그 추가/제거 시 |

---

## 6. Blueprint 설정 (WBP_LNPHud)

1. 기반 클래스: `ULNPHudWidget`
2. **View Model 패널** → `+ Add` → 클래스 `ULNPHudViewModel`, 이름 **`HUD_ViewModel`**, 생성 모드 **Manual**
3. HP 바 바인딩: `ProgressBar.Percent` ← `HUD_ViewModel.HealthPercent` (One Way)
4. 조준점 가시성: 조준점 위젯에 **Function Binding** → `HUD_ViewModel.bIsFreeAiming` 읽어 `true`이면 `HitTestInvisible`, `false`이면 `Collapsed` 반환
5. **BP_PlayerController** Details → `Hud Widget Class` = `WBP_LNPHud`

---

## 7. 어필 포인트 (설계 원칙)

- **GAS ↔ Widget 직접 결합 없음.** ViewModel이 유일한 중개자 — 위젯 레이아웃 변경이 전투 로직에 영향을 주지 않고, ViewModel은 위젯 없이 단위 검증 가능.
- **틱 없는 이벤트 기반.** ASC 델리게이트 → FieldNotify 체인. `UE_MVVM_SET_PROPERTY_VALUE`의 변경 감지로 동일 값 재전달 시 알림 생략.
- **ASC는 PlayerState에서 직접 접근.** `GetPlayerState<ALNPPlayerState>()`로 획득 — Pawn의 PlayerState 캐싱 타이밍에 무관.
- **멀티플레이 이중 진입점.** `OnPossess`는 서버에서만 호출되므로 원격 클라이언트는 `AcknowledgePossession`에서 동일 초기화를 수행 — 어느 경로든 `InitViewModel`이 멱등(ViewModel 1회 생성, Initialize가 기존 구독 해제 후 재구독)이라 중복 호출에 안전.
- **수명 관리 3중 안전망.** `OnUnPossess` / `NativeDestruct` / `Initialize` 선두의 `Deinitialize()` — 어떤 순서로 파괴·재빙의가 일어나도 델리게이트 누수가 없다.

---

## 8. 인벤토리 패널 — ⛔ 폐기 (2026-08-07, 인게임 메뉴로 이관)

> `ULNPInventoryWidget`·`ULNPInventoryEntryWidget`과 `WBP_Inventory`/`WBP_InventoryEntry`/`WBP_BuffEntry`,
> `IA_ToggleInventory`는 **모두 삭제**되었다. 인벤토리는 CommonUI 기반 인게임 메뉴의 인벤토리 탭으로 대체되었다 —
> [TechDesign_InGameMenu.md](TechDesign_InGameMenu.md) 참조. 아래 §8·§8.5는 그 설계의 배경으로만 남긴다.
>
> 이관되며 바뀐 것: ListView → CommonTileView Grid + 디테일 패널, 엔트리 내장 버튼 → 디테일 패널 버튼,
> 장착본 숨김 → 장착 배지 표시, 스탯 최종값 1줄 → 합/곱 분해 RichText 6행.

### (구) 인벤토리 패널 — 구현 완료 (2026-07-12) / 인스턴스 모델 전환 (2026-07-17)

전체 인벤토리 패널(가방 아이템 + 활성 버프, 장착/드랍). `I`키(Enhanced Input `IMC_Player`→`IA_ToggleInventory`, 컨트롤러 상시 IMC) 또는 `LNP.Debug.ToggleInventory`로 토글.

**아키텍처 (HUD MVVM과 다른 판단):** ViewModel로 시작했으나 **MVVM은 `UListView::ListItems`에 바인딩 불가**(런타임 쓰기 불가 — 컴파일러가 거부)임을 확인. 리스트는 C++가 직접 채운다. 데이터 모델은 [TechDesign_Inventory.md](TechDesign_Inventory.md)의 **아이템 인스턴스 모델**(`ULNPInventoryItemInstance`)로 전환됨.

```
[ALNPPlayerController] BeginPlay → CreateWidget<ULNPInventoryWidget>(뷰포트, 기본 Collapsed)
  OnPossess/AcknowledgePossession → InitViewModel(PS->InventoryComponent)
  BeginPlay → AddMappingContext(IMC_Player) / SetupInputComponent → BindAction(IA_ToggleInventory)

[ULNPInventoryComponent] (PlayerState) BagList·ActiveBuffList (인스턴스 FastArray, COND_OwnerOnly)
  + OnInventoryChanged 델리게이트 (서버 변경 시 / 클라 FastArray·OnRep 콜백 시 발송)

[ULNPInventoryWidget] OnInventoryChanged 구독 → RefreshLists()
  → StorageList->AddItem(가방 인스턴스 중 !IsEquipped())   ← 장착본은 자동 숨김 (Option 2)
  → BuffList->AddItem(활성 버프 인스턴스)                    ← 래퍼 불필요 (인스턴스가 UObject)

[ULNPInventoryEntryWidget : IUserObjectListEntry] (WBP_InventoryEntry / WBP_BuffEntry)
  NativeOnListItemObjectSet → Instance->GetDefinition()의 아이콘·이름(DisplayName, 비면 에셋명 폴백)
                            → UpdateDetailText() + 버프면 1초 반복 타이머 재설정(잔여 시간 카운트다운)
  DropButton → Character->DropItem(ItemId) / EquipButton → Character->RequestEquipWeaponInstance(Instance)
```

**클래스/에셋:** `UI/LNPInventoryWidget`, `UI/LNPInventoryEntryWidget`. BP: `/Game/UI/WBP_Inventory`(+ RootVBox·StorageList·BuffList), `WBP_InventoryEntry`, `WBP_BuffEntry`. `BP_LNPPlayerController.InventoryWidgetClass` 지정. (구 `ULNPInventoryViewModel`/`ULNPBuffEntryObject`는 삭제됨.)

**PIE 검증(호스트, 2026-07-17):** 획득→표시, 장착 시 가방에서 즉시 사라짐, 드랍→Dice 스폰·재획득 정상. 이름 표시됨. ⚠️ **BindWidget 함정:** `NameText`가 트리에 있어도 **Is Variable이 꺼져 있으면 BindWidgetOptional이 null** — 위젯 BP에서 bIsVariable 켜야 한다.

## 8.5 인벤토리 스탯 리드아웃 ✅ 완료 (2026-07-27)

인벤토리 패널 상단(`WBP_Inventory`의 `Stats` 라벨 + `StatsText` TextBlock)에 **버프가 모두 합산된 최종 스탯**을
표시한다. 플레이어가 자신이 보유한 버프의 실제 효과를 확인하는 창구.

```
HP            150 / 150
AttackPower   30.0
AttackSpeed   1.30
DefensePower  50.0
MoveSpeed     1.30
LootSpeed     2.00
```

- `ULNPInventoryWidget`이 `InitViewModel(Inventory, ASC)`에서 **ASC 어트리뷰트 변경 델리게이트 7종을 구독**하고,
  어느 하나라도 바뀌면 `UpdateStatsText()`가 리드아웃 전체를 다시 만든다 (Tick 폴링 아님).
  버프 적용·만료가 곧 어트리뷰트 변경이므로 별도 인벤토리 이벤트 구독은 불필요하다.
- 구독 대상은 `GetDisplayedAttributes()`가, 출력 서식은 `UpdateStatsText()`가 정의한다
  (Health/MaxHealth가 한 줄을 공유해 1:1 대응이 아니다). **스탯 추가 시 두 곳을 함께 고친다.**
- 핸들은 `TArray<FDelegateHandle>`에 같은 순서로 보관해 `DeinitViewModel`에서 인덱스로 짝지어 해제한다.
- `StatsText`는 `BindWidgetOptional` — 없으면 스탯 표시만 조용히 생략된다.
- ⚠️ 리드아웃이 공백 패딩으로 열을 맞추므로 **모노스페이스 폰트**(`bForceMonospaced=true`)가 필수다.

---

## 9. 대시 쿨다운 파이 — 커스텀 Slate 위젯 ✅ 완료 (2026-08-17)

방사형 쿨다운 스윕은 `UProgressBar`·`UImage` 조합으로 그릴 수 없다 — 부채꼴이 필요하다.
[Guide_CustomSlateWidget.md](Guide_CustomSlateWidget.md)의 절차를 실제로 적용한 첫 사례.

```
[LNPUI] (신규 Runtime 모듈 — LootNPop을 참조하지 않는다)
  FLNPRadialCooldownStyle : FSlateWidgetStyle   브러시·틴트·시작각·희망 크기
  SLNPRadialCooldown      : SLeafWidget         MakeCustomVerts로 삼각형 팬을 직접 그림
  ULNPRadialCooldownWidget: UWidget             UMG 래퍼 (팔레트 "LNP UI")

[LootNPop]
  ULNPCharacterMoverComponent  OnDashExecuted 델리게이트 + GetDashCooldown()
       └─ ULNPHudWidget::HandleDashExecuted → DashCooldownWidget->StartCooldown(1.0s)
```

**왜 별도 모듈인가.** 위젯이 게임 타입을 하나도 모르게 하려면 물리적으로 참조할 수 없는 곳에 두는 게 가장
확실하다. `LNPUI`는 `LootNPop`에 의존하지 않으므로 게임 타입을 끌어 쓰는 순간 링크가 깨진다.

**왜 진행률을 `SLATE_ATTRIBUTE`로 열지 않았나.** 어트리뷰트로 열면 값을 매 프레임 밀어 주는 쪽에 Tick이
생기고, 어트리뷰트가 매 프레임 평가되며 위젯이 volatile이 된다. 대신 **위젯이 시작 시점에 duration 하나만
받고 경과 시간을 스스로 누적**한다 — 쿨다운이 없는 동안 `SetCanTick(false)`라 비용이 정확히 0이다.
무효화 사유는 `Paint`만 쓴다(부채꼴만 달라지고 희망 크기는 그대로). 스타일 교체만 `Layout`이다.

⚠️ **단, 무효화 사유 최소화가 실제로 이득이 되는 건 위젯이 무효화 루트(fast path) 안에 있을 때뿐이다.**
현재 HUD는 뷰포트에 그냥 올라가 있어 무효화 루트 밖이고, 그 경우 Slate는 매 프레임 전부 다시 그리므로
`Invalidate(Paint)` 호출은 성능상 무의미하다 — 나중에 HUD를 Invalidation Box로 감싸거나
`Slate.EnableGlobalInvalidation`을 켜면 그때부터 값을 한다. 반면 **`SetCanTick(false)`의 이득은
무효화 루트와 무관하게 항상 유효하다** (`SWidget::Paint`가 `bCanTick`일 때만 `Tick`을 호출하므로).

**왜 MVVM을 안 쓰나.** MVVM은 *값* 바인딩용인데 쿨다운은 "지금 시작됐다"는 *이벤트*다.
`float Duration`을 FieldNotify로 노출하면 `UE_MVVM_SET_PROPERTY_VALUE`가 **같은 값(1.0초) 재대입 시
알림을 생략**해서(§7) 두 번째 대시부터 스윕이 안 도는 함정이 있다. `ULNPHudWidget`이 Mover 델리게이트를
직접 받아 위젯 함수를 호출한다.

**구현 메모**
- 부채꼴 반지름은 위젯 사각형의 **반대각선** — 정사각형 아이콘의 네 모서리까지 덮어야 한다.
  넘치는 부분은 `SetClipping(EWidgetClipping::ClipToBounds)`로 잘라낸다.
- ⚠️ **Slate 쪽 `Construct`에서만 `SetClipping`을 걸면 UMG로 쓸 때 안 먹는다** (2026-08-17 실측).
  `UWidget::SynchronizeProperties`가 래퍼의 `Clipping` 프로퍼티(기본 `Inherit`)를 Slate 위젯에 덮어쓴다.
  디자이너 프리뷰에서 부채꼴이 아이콘 밖으로 원반처럼 삐져나오는 증상으로 드러난다 —
  래퍼 생성자에서도 `Clipping = EWidgetClipping::ClipToBounds`를 줘야 한다.
- 브러시에 리소스가 없으면 `MakeCustomVerts`에 넘길 `FSlateResourceHandle`이 무효가 되므로
  엔진 기본 `GenericWhiteBox`로 대체한다.
- 색은 `OverlayTint × 브러시 틴트 × InWidgetStyle.GetColorAndOpacityTint()` — 마지막 항을 빠뜨리면
  HUD 전체를 페이드아웃해도 이 위젯만 남는다.

**검증 현황 (2026-08-17)**

| 항목 | 결과 |
|:---|:---|
| 파이 스윕 (12시 시작·시계방향·1초) | ✅ PIE 실측 |
| 반복 대시 시 처음부터 재시작 | ✅ PIE 실측 |
| 클리핑 (부채꼴이 아이콘 사각형 밖으로 안 넘침) | ✅ 디자이너 실측 — 초기에 실패했던 항목 |
| 부모 페이드 전파 (`InWidgetStyle` 틴트 곱셈) | ✅ 루트 `RenderOpacity` 0.35에서 가림막도 함께 흐려짐 확인 |
| 쿨다운 중에만 Tick 활성 | ✅ `stat Slate`의 `SWidget::Tick (Count)`가 481→482로 올랐다가 복귀 (`Dash Cooldown`을 10초로 늘려 측정). 카운터는 최근 프레임 이동평균으로 표시되므로 전이 구간에서 소수점이 나오는 게 정상 |
| 무효화 사유가 Paint뿐인지 | ⛔ 현재 구성에서 측정 불가 (위 ⚠️ 참조) |

**스킬 슬롯 연결 시:** `StartCooldown(Duration)`을 호출할 지점만 추가하면 된다. 액티브 스킬은 GAS 쿨다운 GE를
쓰므로 `ASC->GetActiveEffectsTimeRemainingAndDuration`으로 duration을 얻어 넘긴다. 단 현재는 스킬 입력 키가
무기 교체 테스트에 점유되어 있고(`LNPInputHandlerComponent.cpp:571-586`) 스킬 DataAsset도 없다.

⚠️ **WBP 배선:** `WBP_LNPHud`에 팔레트 **LNP UI → LNP Radial Cooldown**을 배치하고 변수명을
`DashCooldownWidget`으로, **Is Variable을 켠다**(끄면 `BindWidgetOptional`이 null — §8의 함정과 동일).
현재 배치는 HP 바 좌측 상단(하단 중앙 앵커, 오프셋 -200/-130, 64×64)이고,
그 아래 `DashIcon`(`UImage`)이 `/Game/UI/Icons/T_DashIcon`을 그린다 — 옆에서 본 달리는 실루엣.
소스 PNG는 `Art/Icons/T_DashIcon.png`에 두었으므로 에디터에서 Reimport가 된다.
텍스처 설정은 UI용으로 `TEXTUREGROUP_UI` + `TC_EditorIcon` + `TMGS_NoMipmaps` + `NeverStream`.

---

## 10. 미구현 항목

- **HUD 추가 요소:** 미니맵, 점수/메달 카운터 등 (DevelopmentPlan Phase 6).
- **루팅 게이지 HUD 연동:** `ALNPLootPod::GetGaugePercent()`(복제 완료)를 읽는 월드 스페이스 또는 HUD 게이지 위젯.
- **쿨다운 표시:** 대시는 ✅ 완료(§9). **Active Skill 슬롯 쿨다운**은 스킬 발동 자체가 미구현이라 보류 —
  스킬 시스템이 생기면 §9의 위젯을 그대로 재사용한다.
- **인벤토리 폴리시:** 스킬 슬롯 장착 UI, 환경설정 탭 내용 — 인게임 메뉴로 이관됐으므로 [TechDesign_InGameMenu.md](TechDesign_InGameMenu.md) §12를 따른다. 버프 잔여시간 라이브 카운트다운은 ✅ 완료(2026-07-17, [TechDesign_Inventory.md](TechDesign_Inventory.md) §5).

- **적 HP 바 가림 처리:** 스크린 스페이스 마커 자체는 완료(§11). 월드 지오메트리 뎁스 가림만 보류 — §11.5.

---

## 11. 락온 마커 · 적 HP 바 — 스크린 스페이스 마커 ✅ 완료 (2026-09-09)

순수 엔티티(`CombatMode::PureEntity`)에는 Actor가 없어 `UWidgetComponent`를 달 방법이 **자체가 없다.**
그래서 적 위에 뜨는 두 표현(락온 마커·HP 바)을 월드 스페이스 위젯에서 **스크린 스페이스 커스텀 Slate 위젯**으로
옮겼다. §9의 `LNPUI` 형틀을 재사용한 두 번째 사례다.

### 11.1 동기 — 두 가지이고 무게가 다르다

| 동기 | 성격 |
|:---|:---|
| 위젯 N개 → 1개로 줄여 드로우·Tick 비용 절감 | **추정** — High LOD 적 동시 수가 실측되지 않았다 |
| **Low LOD(순수 엔티티) 적의 표현** | **기능 요구** — 액터가 없는 엔티티에는 `UWidgetComponent`를 달 방법이 자체가 없다 |

두 번째가 진짜 이유다. 성능은 실측 전까지 근거가 약하고, 이번 작업의 근거도 성능이 아니었다.

### 11.2 잃은 것은 뎁스 가림 하나뿐이다

⚠️ **월드 스페이스 `UWidgetComponent`에서 공짜로 얻고 있던 것은 뎁스 가림이다.** 씬에 쿼드로 그려져
뎁스 테스트를 타므로 벽 뒤 적의 HP 바가 자동으로 가려진다. 스크린 스페이스로 옮기면 렌더러가 대신
해주던 이 일을 직접 짜야 한다. "원근 스케일·깊이 가림"을 한 덩어리로 보면 크게 느껴지지만 쪼개면 다르다.

| 항목 | 실제 작업량 | 결과 |
|:---|:---|:---|
| 마커끼리 앞뒤 정렬 | 거리 정렬 후 그리는 순서만 바꿈 — 사실상 0 | ✅ |
| 원근 스케일 | `Scale = Base / Distance` 클램프 한 줄 | ✅ `HpBarScaleDistance` / `HpBarMinScale` |
| **월드 지오메트리 가림** | **여기만 진짜 작업** | ⛔ 보류 (§11.5) |

옮기면서 매 프레임 카메라 빌보드 회전(구 `ALNPEnemyCharacter::Tick`)도 함께 없어졌다.

### 11.3 선결 조건이었던 Health 복제 — ✅ 해소 (2026-09-09)

`FLNPEnemyFragment::Health`는 서버 전용이라 클라이언트에서는 기본값에 머물렀다. **AimPitch가 상태 채널에
들어온 절차 그대로** 1바이트를 더해 해소했다.

```cpp
// FLNPReplicatedAgent — PositionYaw의 형제 멤버다 (중첩하면 위치가 바뀔 때마다 함께 실린다)
uint8 HealthPct = MAX_uint8;   // HP 비율을 0~255로 양자화
```

- **비율만 싣는다.** MaxHealth는 전투 중 불변이고 표시에 필요한 것도 비율뿐이다.
- 클라이언트에서 버블 핸들러(`ApplyReplicatedHealth`)가 **서버가 쓰는 것과 같은 프래그먼트**
  (`FLNPEnemyFragment::Health`)에 `Pct/255 × MaxHealth`로 되쓴다 — 소비처가 넷 모드를 모르게 하는
  단일 소비 경로 규약([TechDesign_EnemyNPC_LowLOD.md](TechDesign_EnemyNPC_LowLOD.md) §5.3) 그대로다.
  ⚠️ 클라의 `MaxHealth`는 템플릿 기본값이라 서버 실제값과 다를 수 있다(HP 원본이 무기의 스탯 수정자다).
  복원되는 것은 **비율이 정확한 근사 절대값**이고, 표시는 비율만 쓰므로 무해하다. 판정에 쓰면 안 된다.
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

- 게이트: `HP < Max` + 카메라 반각 이내 + 최대 표시 거리 이내 + 살아 있을 것
- 거리순 정렬 후 상한 N개 (`ULNPHudWidget`의 `HpBarMaxCount` / `HpBarMaxDistance` / `HpBarMaxAngleDeg`)
- ⚠️ **거리순 단독은 안 된다** — 라이플로 저격한 먼 적이 근처 잡몹에 밀려 안 보인다.
  **최근 `HpBarRecentDamageSeconds` 안에 HP가 변한 적은 상한과 무관하게 강제 포함**한다.
  ⚠️ 이때 필요한 "언제 변했는가"를 **서버가 실어 보내지 않는다.** `ULNPEnemyMarkerProcessor`가
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

착수하게 되면:

- 대상은 §11.4에서 이미 수십 개로 줄어 있다
- 비동기 라인 트레이스를 프레임당 N개씩 라운드로빈(예: 8개/프레임, 결과는 다음 갱신까지 유지) —
  100~200ms 지연은 HP 바에서 눈에 띄지 않는다
- 0/1 토글이 아니라 알파 페이드로 반영하면 판정 지연이 더 가려진다 — 이미 페이드 경로가 있으므로 값만 곱하면 된다

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
- ⚠️ 커스텀 정점이라 **단순 `Image` 브러시만 지원한다**(9-slice·Tile 불가).
- `ComputeDesiredSize`는 0을 돌려준다 — 화면 전체를 덮는 오버레이라 희망 크기를 주장하지 않는다.
- `SetClipping(ClipToBounds)`는 §9와 같은 이유로 **Slate `Construct`와 UMG 래퍼 생성자 양쪽에** 건다.

### 11.7 락온 마커가 먼저였다 (2026-09-06 결정 → 2026-09-09 구현)

락온 마커도 `UWidgetComponent`였고, 순수 엔티티에 락온을 걸면 **표시할 방법이 없다는 같은 벽**에
부딪혔다 → [TechDesign_TargetQuery.md](TechDesign_TargetQuery.md) §7.

**ISM 인스턴스·Niagara로 엔티티용 마커만 따로 만드는 안은 기각했다.** 그렇게 하면 *Actor용 위젯 마커*와
*엔티티용 월드 마커* 두 갈래를 영구히 유지하게 된다 — 이 프로젝트가 순수 엔티티 도입 이후 반복해 밟은
바로 그 함정(Actor 경유 경로의 갈라짐)이다. 락온 마커는 성격상 UI라(거리와 무관하게 또렷하고 일정한 크기)
스크린 스페이스가 자연스럽기도 하다.

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
그래서 `HpBarComponent`(`UWidgetComponent`) · `HpBarWidgetClass` · `ULNPHpBarWidget` · `RefreshHpBar` ·
ASC Health 구독 · 매 프레임 빌보드 회전을 **전부 삭제**했다.

남겨 두었다면 적 종류에 따라 HP 바 표현이 갈리고, §11.7이 기각한 것과 똑같은 두 갈래가 다시 생긴다.
대가는 승격 Actor에 한해 뎁스 가림을 잃는 것이고, 그것은 §11.5와 함께 되찾는다.

### 11.9 구현 구조

| 계층 | 파일 | 역할 |
|:---|:---|:---|
| 위젯 | `Source/LNPUI/.../LNPScreenMarkerStyle.h` | 브러시 2종·틴트·크기·피벗·`bDrawFill` |
| 위젯 | `Source/LNPUI/.../SLNPScreenMarkers.h` | `SLeafWidget`. `FLNPScreenMarker` 배열을 받아 `MakeCustomVerts`로 배칭 |
| 위젯 | `Source/LNPUI/.../LNPScreenMarkerWidget.h` | UMG 래퍼 (팔레트 "LNP UI"). 디자이너 프리뷰 포함 |
| 드라이버 | `UI/LNPHudWidget.cpp` `NativeTick` | 투영·히스테리시스·페이드·원근 스케일. 마커 위젯 2개를 먹인다 |
| 투영 | `UI/LNPScreenProjection.h` | 월드 → 위젯 로컬. 카메라 뒤 판정과 DPI 나눗셈을 여기서 닫는다 |
| 수집 | `Enemy/LNPEnemyMarkerSubsystem.h` | 파라미터 쓰기 / 결과 읽기. 잠금 관례는 `ULNPTargetQuerySubsystem`과 같다 |
| 수집 | `Enemy/LNPEnemyMarkerProcessor.cpp` | `PrePhysics`. 게이트·점수·상한 N. 관측 시각도 여기서 갱신 |
| 복제 | `Replication/LNPMassReplication.h` · `LNPMassReplicator.cpp` | `HealthPct` 1바이트 |

**HUD 위젯이 Tick하게 된 것이 이번 작업의 유일한 구조 변경이다.** 대시 쿨다운은 "시작됐다"는 이벤트라
푸시로 족했지만, 마커는 **매 프레임 다시 계산되는 화면 좌표**라 값 바인딩으로 표현할 수 없다.
락온 컴포넌트가 위젯을 미는 방식은 게임플레이 컴포넌트가 HUD를 알게 되므로 택하지 않았다.

수집을 `ULNPTargetQuerySubsystem`에 얹지 않고 따로 둔 이유는 계약이 다르기 때문이다 —
저쪽은 **최선 1개**, 이쪽은 **상위 N개**다. 잠금 관례와 `TMassExternalSubsystemTraits` 선언은 그대로 베꼈다.

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

- HP 바의 피벗이 **아래 중앙**인 이유: 넘겨받는 좌표가 `HpBarHeightOffset`만큼 올린 머리 지점이라,
  바가 그 위에 얹혀야 머리를 가리지 않는다.
- 락온 레티클은 `Art/Icons/T_LockOnReticle.png`(128², 십자 4방향이 끊긴 링)을 새로 만들어
  `/Game/UI/Icons/T_LockOnReticle`로 임포트했다 — 텍스처 설정은 `T_DashIcon`과 같다
  (`TEXTUREGROUP_UI` + `TC_EditorIcon` + `TMGS_NoMipmaps` + `NeverStream`).
  ⚠️ **옛 `WBP_LockOnMarker`는 텍스처 없는 RoundedBox 링이었고 그대로 못 옮긴다** —
  커스텀 정점 경로는 단순 Image 브러시만 지원하기 때문이다(§11.6). 링은 흰색으로 굽고 색은 틴트가 준다.
- HP 바는 브러시를 비워 둔다. 리소스가 없으면 엔진 기본 `GenericWhiteBox`로 폴백하므로
  **단색 사각형이 정확히 필요한 그림**이고, 텍스처를 하나도 안 만들어도 된다.

⚠️ **EntityConfig DA는 재저장할 필요가 없다.** 이 문서의 이전 판이 그렇게 적었으나 틀렸다 —
`FLNPEnemyHealthDisplayFragment`는 `ULNPEnemyTrait::BuildTemplate`이 **런타임에** 붙이고,
DA에 직렬화되는 것은 트레이트 목록이지 프래그먼트 목록이 아니다. C++에 `AddFragment` 한 줄을
더하는 변경은 에셋을 건드리지 않는다.

**함께 지운 것:** `WBP_LNPHpBar`, `WBP_LockOnMarker`. 둘 다 `BP_LNPEnemy`가 참조하고 있었는데,
그 참조는 이미 제거된 UPROPERTY의 **잔존 값**이었다.
⚠️ **잔존 참조는 컴파일만으로는 안 지워진다** — 패키지를 실제로 다시 써야 한다.
`compile_blueprint` 후 `save_assets`는 더티가 아니라 아무것도 안 쓰고 true를 돌려준다.
CDO 프로퍼티를 하나 건드려 더티로 만든 뒤 저장해야 의존성 목록에서 빠진다.

### 11.12 검증 현황 (2026-09-09, 2P 호스트·게스트)

| 항목 | 결과 |
|:---|:---|
| 컴파일 (에디터 풀 빌드) | ✅ |
| `BindWidgetOptional` 성립 | ✅ `GetWidgets`가 두 위젯을 `bInherited: true`로 돌려준다 — 이것이 곧 성립의 증거다 |
| 디자이너 프리뷰 / 클리핑 | ✅ 링 1개 + HP 바 3개가 그려지고 위젯 밖으로 새지 않는다 |
| 락온 마커 추적 (순수 엔티티·승격 Actor) | ✅ 호스트·게스트 양쪽 |
| 적 HP 바 표시 · 게스트 동기화 | ✅ 두 화면이 일치 |
| **DPI 스케일** — 창을 크게·작게 각각 | ✅ 바가 NPC 머리에서 밀리지 않는다 (§11.10-1) |
| **개수 상한 · 히스테리시스 · 페이드** | ✅ 아래 방법으로 실측 |
| 부모 페이드 전파 (`InWidgetStyle` 틴트 곱셈) | ✅ 루트 `RenderOpacity`를 움직이면 락온 마커·HP 바가 함께 흐려진다 |
| 사망 순간 바가 시체에 남지 않는지 | ✅ 페이드와 함께 사라지고 시체 잔류 0건 — 락온의 "시체 락온"과 같은 필터가 그대로 받아 준다 |
| HP 비율 복제의 대역폭 영향 | ⛔ **미측정.** §11.3의 "갱신 횟수 증가 0"은 계산이지 실측이 아니다 |

⭐ **개수 상한을 재는 법 — 적을 20마리 모아 때리지 말고 상한을 내린다.**
`HpBarMaxCount = 3` · `HpBarExitMargin = 1`로 두고 10마리 이상에게 난사하면, 표시가
**정확히 4개**(진입 상위 3위 + 이탈 여유 1)에서 멈추고 페이드가 걸리는 것이 바로 보인다.
20마리를 동시에 피격시키는 구성을 만드는 것보다 훨씬 싸고, **히스테리시스 산수를 직접 확인**해 준다 —
기본값(20/24)에서는 경계에 도달하는 상황 자체를 만들기 어려워 사실상 검증 불가다.

⚠️ **DPI 스케일 검증은 창 크기를 바꿔야만 성립한다.** 마커 좌표는 뷰포트 픽셀을 DPI 스케일로
나눠 쓰는데(§11.10-1), 스케일이 1에 가까운 해상도 하나만 보면 **그 나눗셈이 틀려도 증상이 안 보인다.**
크게·작게 두 번 보는 것이 이 함정을 여는 유일한 방법이다.

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
오버레이를 걷는 것은 리스폰 빙의(`OnPossess` / 원격 클라의 `AcknowledgePossession`)의 몫이다.
카운트만 멈춘다.

**문구는 영문 원본**(`Respawning in {0}`, `NSLOCTEXT`) — 프로젝트 로컬라이제이션 규약(§ InGameMenu §12)을 따른다.
한국어 표시는 `Content/Localization/Game/ko/Game.po`에 번역을 채우면 된다.
