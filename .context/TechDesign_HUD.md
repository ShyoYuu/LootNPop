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

- **적 HP 바 스크린 스페이스 오버레이:** 설계 정리 완료, 착수 대기 — §11.

---

## 11. 적 HP 바 스크린 스페이스 오버레이 — 📐 설계만 (2026-08-17)

착수하지 않았다. 이 절은 착수 시점에 다시 조사하지 않기 위한 기록이다.

### 11.1 동기 — 두 가지이고 무게가 다르다

| 동기 | 성격 |
|:---|:---|
| 위젯 N개 → 1개로 줄여 드로우·Tick 비용 절감 | **추정** — High LOD 적 동시 수가 실측되지 않았다 |
| **Low LOD(순수 엔티티) 적의 HP도 표시** | **기능 요구** — 액터가 없는 엔티티에는 `UWidgetComponent`를 달 방법이 자체가 없다 |

두 번째가 진짜 이유다. 성능은 실측 전까지 근거가 약하다.

### 11.2 현재 구조와 잃게 되는 것

적 1마리당 `UWidgetComponent` 2개(HP 바·락온 마커) + `UUserWidget` + 매 프레임 카메라 빌보드 회전
(`LNPEnemyCharacter.cpp:33-39`, `73-87`, `215-225`).

⚠️ **지금 공짜로 얻고 있는 것은 뎁스 가림이다.** World space `UWidgetComponent`는 씬에 쿼드로 그려져
뎁스 테스트를 타므로 벽 뒤 적의 HP 바가 자동으로 가려진다. 스크린 스페이스로 옮기면 렌더러가 대신
해주던 이 일을 직접 짜야 한다. "원근 스케일·깊이 가림"을 한 덩어리로 보면 크게 느껴지지만 쪼개면 다르다.

| 항목 | 실제 작업량 |
|:---|:---|
| 마커끼리 앞뒤 정렬 | 거리 정렬 후 그리는 순서만 바꿈 — 사실상 0 |
| 원근 스케일 | `Scale = Base / Distance` 클램프 한 줄 |
| **월드 지오메트리 가림** | **여기만 진짜 작업** |

### 11.3 ⚠️ 선결 조건 — Health가 복제되지 않는다

```cpp
// LNPMassReplication.h:72 — 복제 스트림에 실리는 것 전부
struct FLNPReplicatedAgent : public FReplicatedAgentBase
{
    FReplicatedAgentPositionYawData PositionYaw;
    FGameplayTag                    EnemyTypeTag;
};
```

`FLNPEnemyFragment::Health`는 **서버 전용**이다. 클라이언트가 적 HP를 아는 유일한 경로는 High LOD 액터의
ASC 어트리뷰트 복제이고, Low LOD 엔티티에는 액터가 없으므로 HP를 아예 모른다.

따라서 §11.1의 두 번째 동기를 실현하려면 **Health를 Mass 복제 스트림에 싣는 작업이 선행**된다.
수백 엔티티의 자주 바뀌는 값이라 그대로 실으면 안 되고, 양자화(예: 비율을 uint8)와 변화량 임계
(예: 5% 이상 변할 때만 Dirty)가 함께 필요하다. → [TechDesign_Networking.md](TechDesign_Networking.md)

### 11.4 표시 개수 제한

화면에 HP 바가 너무 많으면 보기 나쁘다. 상한을 두면 **가림 트레이스 대상도 같이 줄어들어** 비용이
상수화되는 부수 효과가 있다.

- 기본 조건: `Current < Max` + 화면 안 + 최대 표시 거리 이내
- 거리순 정렬 후 상한 N개 (`LNP.HpBar.MaxCount` / `LNP.HpBar.MaxDistance` CVar로 조정)
- ⚠️ **거리순 단독은 안 된다** — 라이플로 저격한 먼 적이 근처 잡몹에 밀려 안 보인다.
  **최근 N초 내에 플레이어가 피해를 입힌 적은 상한과 무관하게 강제 포함**한다.
- ⚠️ **경계 점멸 방지** — 20위와 21위가 순위를 오가면 HP 바가 깜빡인다.
  진입은 20위, 이탈은 24위처럼 히스테리시스를 두고 이탈은 짧게 페이드아웃한다.

### 11.5 가림 처리 방안

프로젝트에 재활용할 LOS 데이터가 없다(적 Mass 쪽에 가시성 트레이스 없음, 전체에서 가시성 트레이스는
`LNPAbility_RangedAttack.cpp:217` 한 곳뿐). 새로 짜야 한다.

- 대상은 §11.4에서 이미 수십 개로 줄어 있다
- 비동기 라인 트레이스를 프레임당 N개씩 라운드로빈(예: 8개/프레임, 결과는 다음 갱신까지 유지) —
  100~200ms 지연은 HP 바에서 눈에 띄지 않는다
- 0/1 토글이 아니라 알파 페이드로 반영하면 판정 지연이 더 가려진다

### 11.6 위젯 쪽

§9에서 만든 `LNPUI` 모듈의 형틀(스타일 구조체 + `SLeafWidget` + UMG 래퍼)을 그대로 재사용한다.
위젯은 **스크린 좌표·비율·알파·스케일 배열만 받고** 적이 무엇인지 모른다.
`MakeCustomVerts`로 전체 마커를 한 번에 배칭하면 드로우 콜도 한 덩어리가 된다.

### 11.7 착수 조건

둘 중 하나가 충족될 때. 그전에는 하지 않는다.

1. `stat Slate`로 현재 HP 바 비용이 실제 문제로 측정될 때 (성능 동기)
2. Low LOD 적 HP 표시가 기획으로 확정될 때 (기능 동기 — 이 경우 §11.3 복제 작업이 함께 범위에 들어온다)

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
