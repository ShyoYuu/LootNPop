# TechDesign — 인게임 메뉴 (CommonUI)

> 기획: [GameDesign_InGameMenu.md](GameDesign_InGameMenu.md)
> 2026-08-07 C++ 구현 완료(빌드 성공). 에셋 제작·PIE 검증 진행 중.

## 1. 왜 CommonUI인가

기존 인벤토리 패널(`ULNPInventoryWidget` + `WBP_Inventory`)은 세로 `UListView` 2개에 엔트리마다 Equip/Drop
버튼이 박힌 **마우스 전용 디버그 패널**이었다. 게임패드로는 조작할 수 없고, 탭 카테고리를 담을 구조도 없었다.

CommonUI가 직접 해결해 주는 것:
- **탭 이동(L1/R1)** — `UCommonTabListWidgetBase`가 입력 액션 행 하나로 처리
- **Back(○) 전파** — `UCommonActivatableWidget`의 액션 바인딩 + 스택 자동 pop
- **입력 방식 자동 전환** — 키보드/게임패드에 따라 하단 액션 바 아이콘이 바뀜 (`UCommonBoundActionBar`)
- **포커스 관리** — 활성 위젯의 `GetDesiredFocusTarget()`으로 포커스가 따라감

## 2. 위젯 계층

```
[ALNPPlayerController]
  BeginPlay → CreateWidget<ULNPUILayoutWidget> → AddToViewport(Z=10)
  IA_OpenMenu     → OpenMenu(NAME_None)              ← 마지막으로 보던 탭
  IA_OpenSettings → OpenMenu(TabId_Settings())       ← 항상 환경설정

[ULNPUILayoutWidget : UCommonUserWidget]              (WBP_LNPUILayout, 뷰포트 상주)
  └── UCommonActivatableWidgetStack  MenuStack

      [ULNPMenuRootWidget : UCommonActivatableWidget]  (WBP_LNPMenuRoot, 스택에 push/pop)
        ├── ULNPMenuTabListWidget : UCommonTabListWidgetBase   ← L1/R1, 선택 탭 스케일 확대
        ├── UCommonActivatableWidgetSwitcher  ContentSwitcher  ← TabList의 LinkedSwitcher
        │     ├── ULNPStatsTabWidget      ┐
        │     ├── ULNPInventoryTabWidget  ├ : ULNPMenuTabContentWidget : UCommonActivatableWidget
        │     └── ULNPSettingsTabWidget   ┘
        └── UCommonBoundActionBar  ActionBar
```

**왜 Stack + Switcher 둘 다인가**
- 탭 전환은 push/pop이 아니라 형제 간 전환 → `UCommonActivatableWidgetSwitcher`
  (`UCommonTabListWidgetBase::SetLinkedSwitcher`가 정확히 이 조합용)
- 메뉴 전체의 열기/닫기는 활성화·포커스 복원이 필요 → `UCommonActivatableWidgetStack`
  (스택이 `OnDeactivated`를 듣고 자동 pop한다 — `CommonActivatableWidgetContainer.cpp`)

## 3. Back(○) 전파 규칙 — 이 설계의 핵심

기획이 요구하는 계층: **디테일 → Grid → 메뉴 닫기**.

⚠️ **탭 컨텐츠에 `bIsBackHandler = true`를 주면 안 된다.** 그러면 탭이 Back을 **항상** 소비해서
"Grid 포커스에서 ○를 누르면 메뉴가 닫힌다"가 성립하지 않는다 (액션 바인딩 스택은 통과 개념이 없다).

채택한 구조 — **루트 하나만 Back 핸들러**이고, 활성 탭에게 먼저 물어본다:

```cpp
// ULNPMenuRootWidget
bool NativeOnHandleBackAction()
{
    if (ULNPMenuTabContentWidget* Active = GetActiveTabContent())
        if (Active->HandleMenuBack())      // 탭이 소비했으면 여기서 끝
            return true;
    return Super::NativeOnHandleBackAction();   // 기본 = Deactivate → 스택이 pop
}

// ULNPInventoryTabWidget
bool HandleMenuBack()
{
    if (bDetailFocused) { FocusGrid(); return true; }   // 소비
    return false;                                        // 메뉴 닫기로 전파
}
```

`ULNPMenuTabContentWidget`(추상 베이스)의 `HandleMenuBack()`은 기본 `false` — 스탯·환경설정 탭은
아무것도 하지 않으므로 ○가 곧 메뉴 닫기가 된다.

### 3.0 탭 관련 함정 3종 — 전부 조용히 실패한다

`UCommonTabListWidgetBase`는 기본값이 "아무것도 안 함"에 가까워서, 다음 셋을 놓치면
에러 없이 탭이 안 보이거나 안 먹는다. 셋 다 실측으로 겪은 것들이다.

**① 스택이 위젯을 재사용한다 → 두 번째 열기부터 탭이 사라진다**

`UCommonActivatableWidgetStack::AddWidgetInternal`은 `GeneratedWidgetsPool.GetOrCreateInstance()`로
**클래스별 인스턴스를 풀링·재사용**한다. 한편 `UCommonTabListWidgetBase::NativeDestruct`는
`RemoveAllTabs()`로 등록된 탭을 전부 지운다. 그런데 `NativeOnInitialized`는 인스턴스당 한 번뿐이다.

→ 1회차: Initialize(등록) + Construct = 정상 / 닫기: Destruct(전부 삭제)
→ 2회차: Construct만 = **탭 0개**. 탭 바가 작은 사각형으로 쪼그라들고 클릭도 안 먹는다.

그래서 등록은 **열 때마다 실행되는 `NativeOnActivated`**에서 하고,
`GetTabCount() > 0`으로 중복 등록만 막는다 (`ULNPMenuRootWidget::EnsureTabsRegistered`).
`SetLinkedSwitcher`는 같은 값이면 자체적으로 조기 반환하므로 같이 불러도 안전하다.

**② `bAutoListenForInput`의 기본값이 `false`다 → 탭 이동 키가 전부 무반응**

`NativeConstruct`가 이 플래그를 보고서야 `SetListeningForInput(true)`를 호출하고,
그래야 `Next/PreviousTabInputActionData`가 바인딩된다. 켜지 않으면 L1/R1도 Q/E도 아무 반응이 없다.
`ULNPMenuTabListWidget` 생성자에서 `true`로 켠다.

**③ `HandleTabCreation_Implementation`이 빈 구현이다 → 버튼이 만들어져도 안 붙는다**

⚠️ **`UCommonTabListWidgetBase::HandleTabCreation_Implementation`은 엔진 기본 구현이 비어 있다.**
`RegisterTab`은 버튼 인스턴스를 만들기만 하고 **어떤 패널에도 붙이지 않는다.** 파생 클래스가
이 이벤트를 구현해 컨테이너에 `AddChild` 해야 비로소 탭 바가 보인다
(`ULNPMenuTabListWidget::HandleTabCreation_Implementation`). 로그도 경고도 안 뜨고 조용히 안 보인다.

⚠️ 그리고 `BindWidget`은 **위젯 BP 안에서만** 해석되므로, `ULNPMenuTabListWidget`을 C++ 클래스인 채로
루트 위젯 트리에 직접 넣으면 트리가 비어 `TabButtonContainer`가 영원히 null이다.
반드시 `WBP_LNPMenuTabList`로 감싸서 그 BP 클래스를 배치한다.

### 3.2 Esc 키 충돌

`Esc`는 CommonUI Back 행과 `IA_OpenSettings` 양쪽에 매핑돼 있다.
`ALNPPlayerController::HandleOpenSettingsInput`이 **메뉴가 이미 열려 있으면 즉시 return**해
Back(닫기)에 양보한다 — 그래야 "Esc로 열고 Esc로 닫는" 관례가 성립한다.

## 4. 스탯 합/곱 분해

UE 5.8 어그리게이터 평가식(`GameplayEffectAggregator.cpp:98`):

```
((BaseValue + AddBase) * MultiplyAdditive / DivideAdditive * MultiplyCompound) + AddFinal
```

기획의 "합연산 먼저, 곱연산 나중"과 정확히 일치한다.

| 표시 항목 | 계산 | 색 |
|:---|:---|:---|
| 최종값 | `ASC->GetNumericAttribute(Attr)` | 흰색 `<final>` |
| 합연산결과 | `GetNumericAttributeBase(Attr)` + 활성 GE의 **AddBase** 모디파이어 합 | 회색 `<sub>` |
| 곱연산증가량 | `최종값 − 합연산결과` | 초록 `<buff>` |

**공격력 행만 예외** — `AttackPower`와 `AttackMultiplier`를 한 행으로 합친다:
최종값 = `AttackPower(최종) × AttackMultiplier(최종)`, 합연산결과 = `AttackPower`의 `Base + AddBase`.
데미지 공식(`LNPAbility_BasicAttack.cpp:78`)의 `(AttackPower + 무기보너스) × AttackMultiplier`와 같은 관계이며,
무기 보너스는 어빌리티가 장착 무기에서 읽는 지역 값이라 캐릭터 스탯에는 넣지 않는다.

`AddBase` 합산(`ULNPStatsViewModel::GetAdditiveResult`)은 전부 공개 API로 한다 —
`ASC->GetActiveGameplayEffects().CreateConstIterator()` → `Effect.Spec.Def->Modifiers[i].ModifierOp`.

- ⚠️ `EGameplayModOp::Additive`는 5.8에서 **`AddBase`로 개명**(구 이름은 Hidden 하위호환 별칭). `AddBase`를 쓴다.
- ⚠️ **한계**: 조건부 모디파이어의 태그 평가 파라미터는 재현하지 않고 `bIsInhibited`만 거른다.
  버프 시스템 개선 시 **이 함수 내부만 교체**하면 되도록 static 헬퍼 하나로 격리해 두었다.

**출력**: `URichTextBlock` 하나 + 인라인 마크업. ViewModel은 `FText StatsRichText` FieldNotify 필드 **1개**만
노출하고 MVVM 바인딩 한 줄로 끝낸다(6행 × 3값 = 18개 필드를 피하려는 선택). 색상은 Rich Text Style Set에서 정의한다.
⚠️ 열 정렬을 공백 패딩(`RightPad`)으로 하므로 세 스타일 모두 **모노스페이스 폰트**여야 한다.

## 5. 클래스 목록

| 클래스 | 파일 (`UI/Menu/`) | 역할 |
|:---|:---|:---|
| `ULNPUILayoutWidget` | `LNPUILayoutWidget.*` | 뷰포트 상주. `MenuStack` 보유, `OpenMenu`/`CloseMenu`, `OnMenuClosed` 통지 |
| `ULNPMenuRootWidget` | `LNPMenuRootWidget.*` | 탭 등록·초기 탭 선택·Back 위임·마지막 탭 산출 |
| `ULNPMenuTabListWidget` | `LNPMenuTabListWidget.*` | 탭 버튼을 컨테이너에 부착 + 라벨 주입 + 선택 탭 `SetRenderScale` 강조 |
| `ULNPMenuTabButtonWidget` | `LNPMenuTabButtonWidget.*` | 탭 버튼. `SetTabLabel`로 이름 주입 + 선택 밑줄 토글 |
| `ULNPMenuButtonWidget` | `LNPMenuButtonWidget.*` | 라벨 있는 범용 버튼(Equip/Drop). `SetButtonLabel` |
| `ULNPMenuTabContentWidget` | `LNPMenuTabContentWidget.h` | 탭 컨텐츠 추상 베이스 (`HandleMenuBack`) |
| `ULNPStatsTabWidget` | `LNPStatsTabWidget.*` | 스탯 리드아웃 + 무기 아이콘 + 버프 칩 |
| `ULNPStatsViewModel` | `LNPStatsViewModel.*` | ASC 8종 구독 → `StatsRichText` (합/곱 분해 포함) |
| `ULNPBuffChipWidget` | `LNPBuffChipWidget.*` | 버프 아이콘 + 잔여 초 (1초 타이머) |
| `ULNPInventoryTabWidget` | `LNPInventoryTabWidget.*` | TileView + 디테일, 포커스 전환/Back 소비 |
| `ULNPMenuItemCellWidget` | `LNPMenuItemCellWidget.*` | `UCommonButtonBase` + `IUserObjectListEntry`. 아이콘 + 배지 |
| `ULNPItemDetailPanelWidget` | `LNPItemDetailPanelWidget.*` | 상세 + Equip/Drop |
| `ULNPSettingsTabWidget` | `LNPSettingsTabWidget.h` | 빈 껍데기 |

### 5.1 UCommonButtonBase에는 텍스트가 없다

⚠️ `UCommonButtonBase`는 라벨 개념이 아예 없다. 버튼 WBP 안에 TextBlock을 넣어 두기만 하면
**아무도 글자를 쓰지 않아 배경만 있는 빈 상자로 보인다**(스타일 알파가 낮으면 사실상 안 보인다).
소유 위젯이 명시적으로 라벨을 넣어 줘야 한다 —
탭은 `ULNPMenuTabListWidget::HandleTabCreation`이, Equip/Drop은 `ULNPItemDetailPanelWidget::UpdateButtons`가 넣는다.
Equip 버튼은 장착 중이면 문구가 `Equipped`로 바뀌고 비활성된다.

## 6. 데이터 연결

- **탭이 스스로 붙는다.** 각 탭 위젯이 `NativeOnActivated`에서 `GetOwningPlayerState<ALNPPlayerState>()`로
  ASC·InventoryComponent를 찾아 구독하고, `NativeOnDeactivated`에서 해제한다.
  PlayerController가 위젯에 데이터를 밀어 넣던 구 방식(빙의 타이밍 의존)을 없앴다.
- **Grid 데이터** = `GetBagInstances()` + `GetActiveBuffInstances()`를 합친 한 배열.
  기존 UI와 달리 **장착 중인 무기도 숨기지 않는다** — 배지로 장착 여부를 표시하는 게 기획이다.
- MVVM은 `UListView::ListItems`에 바인딩 불가(기존 결론 유지)이므로 TileView는 C++가 직접 채운다.

⚠️ **`SetListItems`는 "항목 내부 변화"를 셀에 반영하지 않는다.** 장착/해제는 목록의 추가·제거가 아니라
같은 인스턴스의 플래그 변경이라 목록이 **포인터·순서까지 동일**하다. 그러면 SListView가 기존 행을 그대로
재사용하고 `NativeOnListItemObjectSet`을 다시 호출하지 않아, 장착 배지("E")가 인벤토리를 닫았다 열기 전까지
갱신되지 않는다. `RefreshGrid` 끝에서 **`RegenerateAllEntries()`**를 호출해 해결한다
(엔진도 `ListViewBase.h`의 `RequestRefresh` 주석에서 이 경우 이 함수를 권한다).

## 7. 입력

| 액션 | 키보드 | 게임패드 | 처리 |
|:---|:---|:---|:---|
| `IA_OpenMenu` | `I` | `Gamepad_Special_Left` | PC의 `PlayerMappingContext`(상시). **토글** |
| `IA_OpenSettings` | `O` | `Gamepad_Special_Right` | 〃. **토글** |
| Back | `Esc` | `Gamepad_FaceButton_Right`(○) | CommonUI `DT_LNPCommonInputActions` |
| Click | `Enter`, `Space` | `Gamepad_FaceButton_Bottom`(✕) | 〃 |
| TabLeft | `Q` | `Gamepad_LeftShoulder`(L1) | 〃 |
| TabRight | `E` | `Gamepad_RightShoulder`(R1) | 〃 |
| 방향 이동 | `←↑→↓`, `WASD` | D-Pad, 좌스틱 | Slate 네비게이션 (UI 액션 아님) |

키보드 보조 키는 각 행의 `keyboardInputTypeInfo.AdditionalKeys`에 넣는다.

### 7.2 화살표는 UI 액션에 쓰지 않는다 — 그리드 이동과 배타적

⚠️ **화살표를 CommonUI 액션 행에 넣으면 인벤토리 Grid의 셀 이동이 죽는다.** CommonUI 액션 라우터는
Slate 네비게이션보다 **먼저** 키를 소비하므로, 4방향을 액션에 배정하면 방향 이동이 전혀 남지 않는다.
그래서 탭 이동은 `Q`/`E`, 선택은 `Enter`/`Space`, 뒤로는 `Esc`로 두고 **화살표는 네비게이션 전용**으로 비워 둔다.

WASD는 `ALNPPlayerController::SetMenuNavigationEnabled`가 메뉴 수명 동안만
`FLNPMenuNavigationConfig`(엔진 기본 위에 WASD만 추가)를 `FSlateApplication`에 끼워 제공한다.

⚠️ **네비게이션 설정은 `FSlateApplication` 전역이고 PIE는 에디터와 Slate를 공유한다.** 상시 등록하면
에디터 패널까지 WASD로 이동하게 되고, 메뉴를 연 채 PIE를 끝내면 그대로 남는다.
그래서 열기/닫기와 `EndPlay` 양쪽에서 반드시 원복한다.

### 7.1 메뉴가 열린 동안 같은 키로 닫기 — 입력 모드가 핵심

⚠️ **`UCommonActivatableWidget`의 기본 입력 모드 `ECommonInputMode::Menu`는 "UI만 입력을 받음"이라
더 낮은 우선순위의 입력 컴포넌트를 전부 차단한다**(`CommonUIInputSettings.h` 주석).
그러면 PlayerController의 상시 매핑 컨텍스트에 있는 `IA_OpenMenu`/`IA_OpenSettings`가
메뉴가 열린 동안 **아예 들어오지 않아 같은 키로 닫을 수 없다.**

`ULNPMenuRootWidget::GetDesiredInputConfig()`가 `ECommonInputMode::All`을 돌려주어 해결한다.
게임 입력이 함께 살아나지만, 메뉴를 열 때 폰의 `DefaultMappingContext`를 이미 제거하므로
실제로 살아 있는 게임 입력은 메뉴 열기/닫기 키뿐이다.

⚠️ **`Esc`는 `IA_OpenSettings`에서 뺐다.** Esc가 CommonUI Back과 Enhanced Input 양쪽에 걸리면
"Back이 닫음 → 그 직후 EI가 다시 엶"으로 도로 열린다. Esc는 Back(닫기) 전용, 여는 키는 `O`다.

⚠️ **화살표 키를 UI 액션에 쓰면 그리드 방향 이동이 죽는다.** 네 방향이 모두 CommonUI 액션에
소비되므로 인벤토리 Grid의 셀 간 키보드 이동이 불가능하다(마우스 클릭은 가능).
키보드로 Grid를 옮겨 다녀야 한다면 별도 `FNavigationConfig`로 WASD를 네비게이션 키로 추가해야 한다.

⚠️ **터치패드 버튼은 Windows(XInput/Raw)에서 UE에 이벤트가 오지 않는다.** 위치가 가장 가까운
`Gamepad_Special_Left`(Share/Create)로 대체했다. DualSense 전용 입력 플러그인 도입 시 교체 가능.

⚠️ 메뉴 입력은 **폰이 아니라 PlayerController의 상시 매핑 컨텍스트**에 둔다. 메뉴가 폰 입력을 꺼도
메뉴 열기 액션은 살아 있어야 하기 때문이다.

**게임플레이 입력 차단**: `ULNPInputHandlerComponent::SetGameplayInputEnabled(false)`가
`DefaultMappingContext`를 통째로 제거한다. ⚠️ 매핑을 떼면 Completed/Released가 오지 않으므로
눌린 상태(`bIsAttackPressed` 등)가 굳지 않도록 캐시된 입력을 직접 초기화한다.

**일시정지**: `GetNetMode() == NM_Standalone`일 때만 `SetPause(true)`.

## 8. 필수 설정

`Config/DefaultEngine.ini`:
```ini
[/Script/Engine.Engine]
GameViewportClientClassName=/Script/CommonUI.CommonGameViewportClient
```

`Config/DefaultGame.ini`:
```ini
[/Script/CommonInput.CommonInputSettings]
InputData=/Game/UI/Input/DA_LNPCommonInputData.DA_LNPCommonInputData_C
```

⚠️ **`UCommonInputSettings`는 `UCLASS(config = Game)`이라 반드시 `DefaultGame.ini`에 둬야 한다.**
`DefaultEngine.ini`에 쓰면 조용히 무시되고 `InputData`가 `None`으로 남아,
`LogUIActionRouter: Error: Cannot create action binding for widget [...] - no action provided.`
로 나타난다 (Back·Click·탭 이동이 전부 죽는다). 2026-08-07 실측으로 확인한 함정.

⚠️ 이 설정은 **에디터 시작 시 1회만 로드**된다(`bInputDataLoaded` 캐시). ini를 고쳤거나
`DA_LNPCommonInputData`를 새로 만든 직후에는 **에디터를 재시작**해야 반영된다 —
실행 중 CDO 프로퍼티를 직접 써도 `InputDataClass`가 갱신되지 않는다.

`LootNPop.Build.cs`에 `"CommonUI"`, `"CommonInput"` 추가.

## 9. 에셋

| 에셋 | 타입 | 비고 |
|:---|:---|:---|
| `/Game/UI/Input/DT_LNPCommonInputActions` | DataTable (`CommonInputActionDataBase`) | 행: Back / Click / TabLeft / TabRight |
| `/Game/UI/Input/DA_LNPCommonInputData` | `UCommonUIInputData` 파생 BP | DefaultClickAction / DefaultBackAction |
| `/Game/UI/Menu/WBP_LNPUILayout` | `ULNPUILayoutWidget` | `MenuStack` |
| `/Game/UI/Menu/WBP_LNPMenuRoot` | `ULNPMenuRootWidget` | `TabList`·`ContentSwitcher`·3탭·`TabButtonClass`, 배경 블러·반투명 Border 2개 |
| `/Game/UI/Menu/WBP_LNPMenuTabList` | `ULNPMenuTabListWidget` | `TabButtonContainer`(HorizontalBox). ⚠️ BindWidget 때문에 **BP로 감싸야** 한다 |
| `/Game/UI/Menu/WBP_LNPMenuTabButton` | `ULNPMenuTabButtonWidget` | `TabLabel`(CommonTextBlock) |
| `/Game/UI/Menu/WBP_LNPMenuButton` | `ULNPMenuButtonWidget` | 범용 버튼 — Equip/Drop에 사용. `ButtonLabel`(TextBlock) |
| `/Game/UI/Menu/WBP_LNPActionBarButton` | `UCommonBoundActionButton` | `Text_ActionName` 필수 BindWidget |
| `/Game/UI/Menu/WBP_MenuTab_Stats` | `ULNPStatsTabWidget` | ViewModel 등록 + RichText 바인딩, `WeaponIcon`, `BuffContainer` |
| `/Game/UI/Menu/WBP_MenuTab_Inventory` | `ULNPInventoryTabWidget` | `ItemGrid`(CommonTileView), `DetailPanel`. ⚠️ 아래 주의 참조 |
| `/Game/UI/Menu/WBP_MenuTab_Settings` | `ULNPSettingsTabWidget` | "준비 중" 문구 |
| `/Game/UI/Menu/WBP_MenuItemCell` | `ULNPMenuItemCellWidget` | `IconImage`, `BadgeText`(아웃라인 2 — 아이콘 위 시인성 확보) |
| `/Game/UI/Menu/WBP_BuffChip` | `ULNPBuffChipWidget` | `IconImage`, `TimeText` |
| `/Game/UI/Menu/WBP_ItemDetailPanel` | `ULNPItemDetailPanelWidget` | `IconImage`·`NameText`·`DetailText`·`EquipButton`·`DropButton` |
| `/Game/UI/Menu/DT_LNPMenuTextStyles` | Rich Text Style Set | `final`/`sub`/`buff`, 모노스페이스 |

### 9.1 버튼 스타일 (`/Game/UI/Menu/Style/`)

시각 방향(사용자 결정): **무채색 + 밑줄 강조 + 둥근 모서리**. 강조색은 쓰지 않고 흰색/회색 명도로만 구분한다.

| 에셋 | 타입 | 용도 |
|:---|:---|:---|
| `TS_MenuTab_Normal / _Hovered / _Selected` | `UCommonTextStyle` | 탭 라벨 20pt, 명도 0.55 / 0.85 / 1.0, 자간 80 |
| `TS_MenuButton_Normal / _Hovered / _Disabled` | `UCommonTextStyle` | 버튼 라벨 16pt, 명도 0.78 / 1.0 / 0.32 |
| `BS_MenuTab` | `UCommonButtonStyle` | 브러시 전부 투명 — 선택은 **밑줄 위젯**과 텍스트 명도로만 |
| `BS_MenuAction` | `UCommonButtonStyle` | Equip/Drop·액션 바. 둥근 사각(반경 5) + 흰 테두리, 상태별 알파 |
| `BS_MenuItemCell` | `UCommonButtonStyle` | 인벤토리 셀. 반경 3, 선택·호버 시 테두리 2px |

⚠️ **밑줄은 `UCommonButtonStyle`로 만들 수 없다.** 버튼 브러시는 버튼 지오메트리 **전체**를 채우므로
"아래 몇 px만" 그릴 방법이 없다. `WBP_LNPMenuTabButton`의 트리에 `SelectionUnderline`(UImage)을 두고
`ULNPMenuTabButtonWidget::NativeOnSelected/NativeOnDeselected`가 가시성을 토글한다.
`Collapsed`가 아니라 **`Hidden`**을 쓰는 이유는 레이아웃 공간을 유지해 선택 전환 시 탭이 들썩이지 않게 하기 위함이다.

⚠️ 브러시의 둥근 모서리는 텍스처 없이 `DrawAs=RoundedBox` + `outlineSettings.roundingType=FixedRadius`로 만든다.

⚠️ **`Style`은 CDO에 넣어도 이미 배치된 인스턴스에는 안 먹는다.** `UCommonButtonBase::Style`은 `EditAnywhere`라,
위젯을 다른 BP 트리에 배치하는 순간 그 시점 값(대개 `None`)이 **인스턴스에 직렬화**되어 CDO를 덮어쓴다.
그래서 버튼을 먼저 배치하고 나중에 CDO 스타일을 지정하면 그 인스턴스만 CommonUI 기본 스타일(밝은 회색)로 남는다.
배치형 버튼(`WBP_ItemDetailPanel`의 Equip/Drop)은 **인스턴스에도 직접** `Style`을 지정해야 한다.
반대로 런타임 생성형(탭 버튼·TileView 셀·액션 바 버튼)은 클래스에서 만들어지므로 CDO만으로 충분하다.

⚠️ **채움은 흰색이 아니라 검은색 기준으로 만든다.** 패널이 반투명(검정 0.72)이라 그 위에 흰색 알파 채움을 얹으면
**뒤 월드가 밝을 때(설원 등) 버튼이 하얗게 떠서 라벨이 묻힌다.** 카메라 방향에 따라 보였다 안 보였다 하므로
원인을 잡기 어렵다. 검정 채움 + 흰 테두리로 두면 배경과 무관하게 대비가 유지된다.
| `IA_OpenMenu`, `IA_OpenSettings` | InputAction | `IMC_Player`에 등록 |

> ⚠️ **TileView가 1열로 늘어서는 함정**: `ItemGrid`가 든 HorizontalBox 슬롯의 Size Rule이 `Automatic`이면
> TileView는 **희망 너비(= 셀 하나 폭)**만 받아 20개가 세로로 한 줄이 된다. 슬롯을 `Fill`로 두어야
> 열 수 = 할당 너비 ÷ `EntryWidth`가 성립한다. 현재 Grid : Detail = **Fill 1.5 : 1.0**,
> `EntryWidth/Height` 96, 엔트리 간격 6 → 약 5열.

> ⚠️ **`bIsVariable` 함정**: 이 프로젝트의 위젯 BP는 `Is Variable`이 기본 off인 경우가 잦아
> `BindWidget`/`BindWidgetOptional`이 null이 된다. 신규 위젯의 바인딩 대상마다 반드시 켠다.

## 10. 폐기된 것

- `ULNPInventoryWidget`, `ULNPInventoryEntryWidget` (C++ 삭제 완료)
- `WBP_Inventory`, `WBP_InventoryEntry`, `WBP_BuffEntry`, `IA_ToggleInventory` (에셋 삭제 필요)
- 콘솔 커맨드 `LNP.Debug.ToggleInventory` → `LNP.Debug.OpenMenu [Tab]` / `LNP.Debug.CloseMenu`

## 11. 검증 현황 (2026-08-07)

✅ **동작 확인 (PIE, 호스트)** — `LNP.Debug.OpenMenu`로 메뉴가 열리고 캐릭터 스탯 탭이 활성화된다
(첫 진입 규칙 성립). 스탯 리드아웃이 MVVM 바인딩으로 6행 렌더링됨:

```
HP            100 / 100
Attack        10.00 (10.00 + 0.00)
Attack Speed   1.00 ( 1.00 + 0.00)
Defense        0.00 ( 0.00 + 0.00)
Move Speed     1.00 ( 1.00 + 0.00)
Loot Speed     1.00 ( 1.00 + 0.00)
```

로그: `LogUIActionRouter: Applying input config for leaf-most node [StatsTab]`,
`InputMode: New (ECommonInputMode::Menu)`.

추가 확인 (에디터 재시작 후):
- Back 액션 바인딩 오류 해소 — `Cannot create action binding` 로그가 사라짐 (§8의 `DefaultGame.ini` 반영)
- 상단 탭 바 렌더링 — 배경 블러 + 상·하단 두 팝업 분리 + 탭 전환(`LNP.Debug.OpenMenu Inventory`) 동작
- 인벤토리 탭: 빈 인벤토리에서 디테일 패널이 "No item" 표시
- 위젯 BP 12종 전부 클린 컴파일

✅ **스타일 적용 확인 (PIE, 2026-08-07)** — 탭 바가 `CHARACTER / INVENTORY / SETTINGS`로 렌더링되고,
선택 탭만 흰 글자 + 밑줄, 비선택은 회색. `LNP.Debug.OpenMenu Settings`로 전환 시 밑줄이 따라 이동하며
컨텐츠도 "Settings - Coming Soon"으로 바뀐다.

✅ **키보드 조작 확인 (사용자 실측)** — `I`/`O` 토글, `Q`/`E` 탭 이동, 화살표·WASD Grid 셀 이동,
5열 그리드, Equip/Drop 버튼, 장착 시 `Equipped` 전환 모두 정상.

## 12. 잔여

- 게임패드 실기기 조작 검증 — 탭 L1/R1, Grid 셀 네비게이션, ✕로 디테일 진입, ○로 Grid 복귀·메뉴 닫기.
  ⚠️ MCP의 Slate 입력은 게임 뷰포트로 전달되지 않으므로 **사람이 직접 확인**해야 한다.
- 인벤토리 실사용 검증 — 아이템 획득 후 Grid 표시, 장착 배지, 버프 잔여시간 카운트다운, Equip/Drop
- 2인 PIE에서 일시정지가 걸리지 않는지 확인 (스탠드얼론 전용 규칙)
- 환경설정 탭 내용
- 스탯 리드아웃의 곱연산 증가량은 현재 콘텐츠에 곱연산 버프가 없어 항상 0으로 표시된다
  (버프 시스템 개선 세션에서 실측 예정)
