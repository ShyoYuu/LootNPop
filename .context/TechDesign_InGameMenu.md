# TechDesign — 인게임 메뉴 (CommonUI)

> 기획: [GameDesign_InGameMenu.md](GameDesign_InGameMenu.md)
> 2026-08-07 C++ 구현 완료(빌드 성공). 에셋 제작·PIE 검증 진행 중.

## 1. 왜 CommonUI인가

기존 인벤토리 패널(`ULNPInventoryWidget` + `WBP_Inventory`)은 세로 `UListView` 2개에 엔트리마다 Equip/Drop
버튼이 박힌 **마우스 전용 디버그 패널**이었다. 게임패드로는 조작할 수 없고, 탭 카테고리를 담을 구조도 없었다.

CommonUI가 직접 해결해 주는 것:
- **탭 이동(L1/R1)** — `UCommonTabListWidgetBase`가 입력 액션 행 하나로 처리
- **Back(○) 전파** — `UCommonActivatableWidget`의 액션 바인딩 + 스택 자동 pop
- **입력 방식 감지** — `UCommonInputSubsystem::GetCurrentInputType()` + `OnInputMethodChangedNative`
- **포커스 관리** — 활성 위젯의 `GetDesiredFocusTarget()`으로 포커스가 따라감

⚠️ **하단 액션 바만은 CommonUI가 해결해 주지 못한다.** 상세는 §3.3 참조 —
`UCommonBoundActionBar`를 버리고 커스텀 힌트 바를 직접 만들었다.

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
        └── ULNPMenuHintBarWidget  HintBar                     ← 하단 조작 안내 (§3.3)
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

### 3.3 하단 힌트 바 — `UCommonBoundActionBar`를 버린 이유

기획 §3·§8은 "현재 포커스에서 가능한 조작을 아이콘 + 라벨로 표시하며, 키보드/게임패드 전환 시 아이콘이 자동으로
바뀐다"를 요구한다. 처음에는 `WBP_LNPMenuRoot`에 `UCommonBoundActionBar`(`ActionBar`)를 두고 `ActionButtonClass`도
`WBP_LNPActionBarButton`으로 지정했는데, **PIE에서 하단이 완전히 비어 있었다.**

`UCommonBoundActionBar::HandleDeferredDisplayUpdate`는 액션 라우터에 등록된 바인딩 중
`Binding->bDisplayInActionBar`가 켜진 것만 그린다(`CommonBoundActionBar.cpp:184`). 그런데 후보가 하나도 없었다:

| 조작 | 왜 후보가 아닌가 |
|:---|:---|
| Back(○) | `UCommonActivatableWidget::bIsBackActionDisplayedInActionBar` 기본값이 `false` (`CommonActivatableWidget.h:193`) |
| TabLeft/Right | `UCommonTabListWidgetBase`가 `FBindUIActionArgs(..., /*bShouldDisplayInActionBar=*/false, ...)`로 **명시적으로 끔** (`CommonTabListWidgetBase.cpp:207-220`) |
| Click(✕) | **액션 라우터 바인딩이 아예 없다.** ✕/Enter/Space는 Slate 네비게이션이 포커스된 버튼을 직접 누르는 경로다 |
| 방향 이동(L3·방향키) | 위와 동일. 순수 Slate 네비게이션 |

앞의 둘은 플래그로 살릴 수 있지만 **뒤의 둘은 어떤 설정으로도 살릴 수 없다** — 표시할 바인딩 자체가 존재하지 않는다.
즉 `UCommonBoundActionBar`로는 기획 §8 표를 절대 다 그릴 수 없다. 그래서 힌트 목록을 우리가 공급하는
`ULNPMenuHintBarWidget`으로 교체했다.

**힌트 공급 흐름** — 탭이 자기 힌트를 선언하고, 루트가 공통 힌트를 얹어 조립한다.

```
ULNPMenuTabContentWidget::GetMenuHints()   ← 탭 고유 (인벤토리: ✕ 의미, 방향 이동)
ULNPMenuTabContentWidget::GetMenuBackHintLabel()  ← ○ 라벨 (Close / Back)
              ↓
ULNPMenuRootWidget::RebuildHints()  = [Back] + 활성 탭 힌트 + [L1/R1 Tab]
              ↓
ULNPMenuHintBarWidget::SetHints()   → 글리프 해석 → UCommonTextBlock 쌍 생성
```

**힌트는 입력 타입에 따라 감추지 않는다 — 글리프만 바꾼다.** 한때 "마우스로 직접 누를 수 있으니
키보드에서는 `Details`/`Confirm`을 숨기자"는 안을 넣었다가 되돌렸다. 이유 둘:

- `ECommonInputType`은 `MouseAndKeyboard` / `Gamepad` / `Touch` 셋뿐이라 **"마우스 유저"와
  "키보드 온리 유저"를 구분할 수 없다.** 같은 버킷이므로 어느 쪽에 맞춰도 나머지는 추측이다.
- 그리고 `WASD Move`는 애초에 마우스 유저에게 쓸모없는 힌트다(마우스면 셀을 직접 클릭한다).
  이동 키는 안내하면서 실행 키만 감추면 **커서는 옮길 줄 아는데 확정은 못 하는** 앞뒤 안 맞는 세트가 된다.

힌트 바의 존재 이유가 발견 가능성이므로, 칸 하나를 아끼려고 조작 경로를 끊지 않는다.

### 3.5 힌트 칸은 엔트리 WBP다 — C++에 시각 프로퍼티를 두지 않는다

칸 하나(`ULNPMenuHintEntryWidget` / `WBP_LNPMenuHintEntry`)는 `UCommonTextBlock` 둘(`GlyphText`·`LabelText`)이고,
`UDynamicEntryBox`(`HintContainer`)가 생성·풀링한다. C++은 `SetHint(글리프, 라벨)`로 값만 넣는다.

처음엔 힌트 바가 `UCommonTextBlock`을 직접 `ConstructWidget`하고 텍스트 스타일·간격을 C++ 프로퍼티로 들고
있었다. 코드는 짧았지만 **칸의 생김새가 코드에 박혀** 디자이너가 만질 수 있는 게 사실상 폰트뿐이었다 —
글리프를 키캡 모양 테두리로 감싸거나, 라벨을 아래로 내리거나, 칸 사이에 구분선을 넣는 게 전부 불가능했다.
입력 프롬프트에서 가장 흔한 시각 처리가 바로 그 키캡 테두리라 이건 실질적인 제약이었다.

엔트리 WBP로 바꾸면서 C++에서 **시각 관련 프로퍼티가 전부 사라졌다** —
텍스트 스타일은 엔트리 WBP 안 TextBlock이, 칸 간격·정렬·배치 방향은 `UDynamicEntryBox`가 갖는다.
덤으로 §9.1의 "CDO 값이 배치 인스턴스에 안 먹는" 함정도 같이 소멸했고, 엔트리 위젯 풀링이 공짜로 붙었다.

이 프로젝트의 다른 반복 엔트리(`WBP_MenuItemCell`, `WBP_BuffChip`, `WBP_LNPMenuTabButton`)와도 같은 형태다.

갱신 트리거는 넷이다:
1. `ULNPMenuTabContentWidget::NativeOnActivated`의 `OnMenuHintsChanged` 브로드캐스트 — 메뉴 열기·탭 전환·재진입을 전부 덮는다
2. `ULNPInventoryTabWidget::FocusGrid()` / `FocusDetailPanel()`
3. `ULNPInventoryTabWidget::RefreshGrid()` — 목록이 비었는지에 따라 힌트 구성이 달라지므로(아래) 목록 변경 시에도 알린다
4. `UCommonInputSubsystem::OnInputMethodChangedNative` — 힌트 바 내부에서 글리프만 다시 그린다(루트 미관여)

⚠️ **힌트는 밀어 넣는 방식이라 상태가 바뀌면 반드시 브로드캐스트해야 한다.** 포커스 링은 `TAttribute`로
매번 재평가되어 저절로 맞춰지지만(§3.6) 힌트는 그렇지 않다 — 3번을 빠뜨리면 빈 인벤토리에서 아이템이
들어와도 힌트가 빈 채로 굳는다.

⚠️ **`UCommonTabListWidgetBase::OnTabSelected`를 갱신 훅으로 쓰면 안 된다.** 그 델리게이트는
`LinkedSwitcher->SetActiveWidget()` 직후 발화하지만(`CommonTabListWidgetBase.cpp:488-509`), 새 탭의
`ActivateWidget()`은 `SCommonAnimatedSwitcher` 트랜지션(기본 0.4s)이 끝난 뒤 `HandleSlateActiveIndexChanged`에서
비동기로 돈다. `OnTabSelected` 시점의 `bDetailFocused`는 **직전 방문의 잔값**이라, 실제로는 Grid에 포커스가
가는데 "Back"(디테일→Grid) 라벨이 뜬다.

⚠️ **루트의 구독은 `NativeOnInitialized`에서 한다.** `NativeOnActivated`에서 하면 늦다 —
루트의 `Super::NativeOnActivated()` 안에서 스위처가 탭을 활성화하며 브로드캐스트가 이미 지나간다.

### 3.4 키 글리프 — 텍스트 심볼로 그린다

⚠️ **이 프로젝트에는 `UCommonInputBaseControllerData` 에셋이 0개다.** `[/Script/CommonInput.CommonInputPlatformSettings]`
설정도 없고 엔진도 기본 제공을 하지 않는다(`CommonUI/Content` 확인). 따라서 `UCommonActionWidget`은 항상 null
브러시를 얻어 **스스로 `Collapsed`** 된다(`CommonActionWidget.cpp:344`). 키 아이콘을 쓰려면 글리프 텍스처 세트를
먼저 만들어야 한다.

그래서 아이콘 대신 **텍스트 심볼**을 쓴다. 해석은 `LNPInputGlyph`(`Source/LootNPop/UI/LNPInputGlyph.*`) 한 곳에
격리했다 — 나중에 텍스처로 갈아탈 때 **고칠 파일은 이것 하나뿐**이다.

| 입력 | 해석 경로 |
|:---|:---|
| CommonUI 액션 행 (○·L1/R1) | `FCommonInputActionDataBase::GetCurrentInputTypeInfo(Subsystem).GetKey()` |
| Enhanced Input 액션 (상호작용 F/□) | `CommonUI::GetFirstKeyForInputType()` → `QueryKeysMappedToAction` |
| 바인딩 없는 조작 (방향 이동) | `FLNPMenuHint`의 고정 글리프 2종(키보드/게임패드) |

게임패드는 기획 §1대로 PlayStation 표기(`○ × □ △ L1 R1 L3`)로 덮어쓴다 — 엔진의 `FKey::GetDisplayName`은
"Gamepad Face Button Bottom"처럼 길고 Xbox 계열 명명이라 쓸 수 없다. 키보드는 엔진이 이미 짧게 준다("Esc", "Space").

⚠️ **폰트 함정.** Roboto에는 `○`(U+25CB)·`□`(U+25A1)·`△`(U+25B3)·`✕`(U+2715)가 **하나도 없다**(cmap 직접 확인).
`/Engine/EngineFonts/Roboto`의 `CompositeFallbackFont`(DroidSansFallback)가 받아 주므로 ○·□·△는 렌더되지만,
✕는 Dingbats라 위험이 커서 **`×`(U+00D7, Roboto 네이티브)**를 쓴다.

⚠️ **글리프는 `NSLOCTEXT`로 만들지 않는다.** `○`·`L1` 같은 기호가 번역 대상으로 수집되면 매니페스트가 오염된다.
글리프는 `FText::FromString`, 라벨만 `NSLOCTEXT("LNPMenu", ...)`.

⚠️ **키 해석은 "현재 적용 중인" 매핑 컨텍스트만 읽는다.** 메뉴가 열리면 폰의 `IMC_Pawn`이 통째로 제거되므로
그 창에서는 상호작용 키가 무효로 나온다. `ULNPInteractionPromptWidget::RefreshKeyGlyph`는 **빈 결과면 텍스트를
건드리지 않고** 직전 글리프를 유지한다.

### 3.6 포커스 링 — 코드로 옮긴 포커스에는 안 그려진다

⚠️ **Slate는 포커스 링을 "사용자가 직접 이동했을 때"만 그린다.** 판정은 `SlateApplication.cpp:3095`:

```cpp
bool ShowFocus = false;
if (NewFocusedWidgetPath.IsValid())
{
    ShowFocus = InCause == EFocusCause::Navigation;
    for (int32 i = Path.Widgets.Num() - 1; i >= 0; --i)   // 말단 → 루트
    {
        TOptional<bool> Query = Path.Widgets[i].Widget->OnQueryShowFocus(InCause);
        if (Query.IsSet()) { ShowFocus = Query.GetValue(); break; }
    }
}
```

`UWidget::SetFocus()`는 `EFocusCause::SetDirectly`를 쓰므로 **코드가 옮긴 포커스에는 링이 안 생긴다.**
이 메뉴는 코드로 포커스를 옮기는 지점이 여럿이라(CommonUI의 탭 활성화 `DesiredTarget->SetFocus()`,
인벤토리 Grid↔디테일 전환) 그대로 두면 "포커스는 갔는데 어디 있는지 안 보이는" 상태가 된다.
실제로 겪은 증상: 디테일 패널 진입 직후엔 링이 없다가 방향키를 한 번 누르면 나타났다.

해결 — `ULNPMenuRootWidget::RebuildWidget()`이 메뉴 전체를 `SLNPFocusRingScope`로 감싼다.
이 위젯은 `OnQueryShowFocus`에서 `SetDirectly`일 때만 `true`를 돌려주고 나머지는 엔진 기본에 위임한다.
위 루프가 **값을 돌려주는 첫 위젯**을 따르고 `SWidget::OnQueryShowFocus` 기본 구현이 빈 `TOptional`이라
(`SWidget.cpp:623`), 최상단에 하나만 두면 그 아래 전부에 적용된다. 마우스 클릭 동작은 그대로다.

> UMG 컨테이너 위젯(+슬롯 클래스)을 새로 만들 필요는 없다. `UUserWidget::RebuildWidget()`은
> 위젯 트리 루트를 그대로 돌려줄 뿐이라(`UserWidget.cpp:1214`) 그 반환값을 감싸면 된다.

**⚠️ 포커스가 앉을 대상이 없으면 링을 강제하면 안 된다.** 빈 인벤토리에서는 `SCommonListView`가
포커스를 셀로 넘겨줄 수 없어(넘길 셀이 없다) 포커스가 `UCommonTileView` 컨테이너에 머무는데,
그 상태로 링을 강제하면 **그리드 전체가 파랗게 둘러싸인다.**
그래서 `ULNPMenuTabContentWidget::ShouldForceFocusRing()`(기본 `true`)을 두고,
`SLNPFocusRingScope`가 `TAttribute<bool>`로 활성 탭에 매번 물어본다.
인벤토리 탭은 `HasGridItems()`로 답한다 — 포커스 링과 힌트 노출이 **같은 조건**을 공유하도록 헬퍼로 묶었다.

`TAttribute`라 포커스 이동마다 재평가되므로, 목록이 비거나 채워지면 별도 갱신 호출 없이 따라온다.
빈 상태에서도 사용자가 방향키로 직접 이동하면 링은 나온다 — `Navigation` 원인은 엔진 기본 경로다.

**링 자체의 커스터마이즈** — 파란 사각형은 `FStarshipCoreStyle`의 `FocusRectangle` 브러시다
(`StarshipCoreStyle.cpp:217`, `FStyleColors::Primary` 테두리 + 투명 채움). 프로젝트 에셋이 아니라서
위젯 디버거에 안 잡힌다. 바꾸려면:

| 방법 | 내용 |
|:---|:---|
| 브러시 교체 | `FStarshipCoreStyle::SetFocusBrush(new FSlateRoundedBoxBrush(...))` — public static. ⚠️ `SWidget::GetFocusBrush()`는 `FAppStyle`에서 읽는데 에디터와 게임의 앱 스타일셋이 달라, PIE에서 안 먹으면 패키징 빌드로 확인해야 한다 |
| 위젯별 브러시 | `SWidget::GetFocusBrush()`가 virtual |
| 끄고 직접 그리기 | `OnQueryShowFocus`에서 `false` 반환 후 프로젝트 위젯으로 표현 |

⚠️ **`PLATFORM_UI_NEEDS_FOCUS_OUTLINES`가 Android/iOS에서 `0`이다**(`AndroidPlatform.h:61`, `IOSPlatform.h:50`).
모바일에서는 이 링이 아예 그려지지 않으므로, 모바일까지 가려면 포커스 표현을 직접 만들어야 한다.

## 4. 스탯 합/곱 분해

UE 5.8 어그리게이터 평가식(`GameplayEffectAggregator.cpp:98`):

```
((BaseValue + AddBase) * MultiplyAdditive / DivideAdditive * MultiplyCompound) + AddFinal
```

기획의 "합연산 먼저, 곱연산 나중"과 정확히 일치한다. 표기는 **`C (A × B)`**:

| 표시 항목 | 계산 | 색 |
|:---|:---|:---|
| **C** 최종값 | `ASC->GetNumericAttribute(Attr)` | 흰색 `<final>` |
| **A** 기초 스탯 총량 | `GetNumericAttributeBase(Attr)` + 활성 GE의 **AddBase** 모디파이어 합 | 회색 `<sub>` |
| **B** 곱연산 총 배율 | `C / A` (A ≈ 0이면 100%) | 초록 `<buff>` |

**B를 GE 순회로 재계산하지 않고 `C / A`로 역산한다** — 그래야 표시값이 실제 게임플레이 값과
항상 일치한다. 그리고 이 항등식은 스탯 파이프라인이 `AddBase`·`MultiplyAdditive` **두 채널만**
쓸 때만 성립한다 (→ `GAS/LNPStatModifier.h`의 규약).

무기 스텟도 장착 GE의 `AddBase`로 들어오므로 A에 자동 포함된다 — 별도 특례가 없다.

`AddBase` 합산(`ULNPStatsViewModel::GetAdditiveResult`)은 전부 공개 API로 한다 —
`ASC->GetActiveGameplayEffects().CreateConstIterator()` → `Effect.Spec.Def->Modifiers[i].ModifierOp`.

- ⚠️ `EGameplayModOp::Additive`는 5.8에서 **`AddBase`로 개명**(구 이름은 Hidden 하위호환 별칭). `AddBase`를 쓴다.
- ⚠️ **한계**: 조건부 모디파이어의 태그 평가 파라미터는 재현하지 않고 `bIsInhibited`만 거른다.

행 목록·표시명·표기 방식(정수/수치/퍼센트)은 `LNPStat::GetStatMetaTable()` 하나가 단일 출처다 —
스탯을 추가하면 GE 모디파이어·구독 목록·리드아웃 행이 함께 따라온다.

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
| `ULNPMenuTabContentWidget` | `LNPMenuTabContentWidget.*` | 탭 컨텐츠 추상 베이스 (`HandleMenuBack`·`GetMenuHints`·`OnMenuHintsChanged`) |
| `ULNPMenuHintBarWidget` | `LNPMenuHintBarWidget.*` | 하단 조작 안내 바. 힌트 목록 → 엔트리 생성, 입력 타입 변경 구독 |
| `ULNPMenuHintEntryWidget` | `LNPMenuHintEntryWidget.*` | 힌트 한 칸 — `GlyphText` + `LabelText`. 시각 요소는 전부 WBP 쪽 |
| `FLNPMenuHint` | `LNPMenuHint.h` | 힌트 한 칸 (액션 행 배열 또는 고정 글리프 2종 + 라벨) |
| `LNPInputGlyph` | `UI/LNPInputGlyph.*` | 키 → 텍스트 심볼 해석기. 힌트 바와 인터랙션 프롬프트가 공유 |
| `ULNPStatsTabWidget` | `LNPStatsTabWidget.*` | 스탯 리드아웃 + 무기 아이콘 + 버프 칩 |
| `ULNPStatsViewModel` | `LNPStatsViewModel.*` | ASC 8종 구독 → `StatsRichText` (합/곱 분해 포함) |
| `ULNPBuffChipWidget` | `LNPBuffChipWidget.*` | 버프 아이콘 + 잔여 초 (1초 타이머) |
| `ULNPInventoryTabWidget` | `LNPInventoryTabWidget.*` | TileView + 디테일, 포커스 전환/Back 소비 |
| `ULNPMenuItemCellWidget` | `LNPMenuItemCellWidget.*` | `UCommonButtonBase` + `IUserObjectListEntry`. 아이콘 + 배지 3종(모서리별) |
| `ULNPItemDetailPanelWidget` | `LNPItemDetailPanelWidget.*` | 상세 + Equip/Merge/Drop |
| `ULNPSettingsTabWidget` | `LNPSettingsTabWidget.h` | 빈 껍데기 |

### 5.1 UCommonButtonBase에는 텍스트가 없다

⚠️ `UCommonButtonBase`는 라벨 개념이 아예 없다. 버튼 WBP 안에 TextBlock을 넣어 두기만 하면
**아무도 글자를 쓰지 않아 배경만 있는 빈 상자로 보인다**(스타일 알파가 낮으면 사실상 안 보인다).
소유 위젯이 명시적으로 라벨을 넣어 줘야 한다 —
탭은 `ULNPMenuTabListWidget::HandleTabCreation`이, Equip/Merge/Drop은 `ULNPItemDetailPanelWidget::UpdateButtons`가 넣는다.
Equip 버튼은 장착 중이면 문구가 `Equipped`로 바뀌고 비활성된다.

### 5.2 인벤토리 셀 배지 — 모서리 3분할 (2026-08-20)

배지 하나가 장착 표시와 잔여 시간을 겸하던 것을 셋으로 나눴다. 무기 레벨이 생기면서 한 칸으로는
서로를 가리기 때문이다. 위치는 BP 레이아웃(아이콘 위 Overlay의 각 모서리)이 정하고, C++는 내용만 쓴다.

| 바인딩 | 위치 | 내용 |
|:--|:--|:--|
| `EquipMarkText` | 좌상단 | 장착 중이면 `EquippedBadgeText`("E") |
| `DurationText` | 우상단 | 버프 잔여 초 — **1초 반복 타이머가 이것만** 다시 쓴다 |
| `LevelText` | 우하단 | `LevelFormat`("Lv.{0}"). 버프는 비운다 |

셋 다 `BindWidgetOptional`이라 BP가 아직 없어도 크래시하지 않는다.
타이머 콜백을 `UpdateDurationText()`로 분리한 이유는, 매초 세 배지를 전부 다시 쓸 필요가 없기 때문이다.

### 5.3 Merge 버튼의 3상태

`ULNPItemDetailPanelWidget::UpdateButtons`가 무기에만 표시하고 세 상태로 그린다:

| 조건 | 문구 | 활성 |
|:--|:--|:--:|
| 재료 충분 | `Merge (3/3)` | ✅ |
| 재료 부족 | `Merge (1/3)` | ❌ |
| 최대 레벨 | `Max Lv.` | ❌ |

재료 수는 **소유 클라이언트가 로컬로 센다** — 가방이 `COND_OwnerOnly`로 복제되므로 가능하다
(`ULNPInventoryComponent::CanMergeItem`). 서버는 `TryMergeItem`에서 같은 판정을 처음부터 다시 한다.
포커스 순서는 Equip → Merge → Drop (`GetFirstFocusTarget`).

#### 버튼 줄바꿈 — HorizontalBox가 아니라 WrapBox다

버튼이 3개가 되면서 `HorizontalBox`로는 **패널 오른쪽으로 넘쳤다**(실측). 담는 패널을 `WrapBox`로 바꿔
폭이 모자라면 다음 줄로 내려가게 했다. 셋 다 `Collapsed`가 될 수 있어 **`UniformGridPanel`은 쓸 수 없다** —
슬롯의 행/열이 고정이라 버튼이 접히면 빈 칸이 남는다. WrapBox는 접힌 자식이 자리를 차지하지 않아 자연히 흐른다.

⚠️ **WrapBox는 부모가 폭을 정해 줘야 접힌다.** 부모가 Auto 사이즈면 WrapBox의 희망 크기(=한 줄에 다 편 폭)를
그대로 주므로 영원히 줄바꿈이 안 된다. 그래서 중간의 `ButtonRow`(HorizontalBox)를 없애고
**WrapBox를 Root VerticalBox의 직계 자식**으로 두었다 (VerticalBox 슬롯은 가로가 Fill이다).

한 줄에 몇 개가 들어갈지는 **버튼 폭이 결정한다.** 처음엔 `Equipped`(1) + `Merge`·`Drop`(2)로 갈려
위아래가 뒤집혀 보였다. 2+1로 만들려고 두 곳을 줄였다:
- `BS_MenuAction.ButtonPadding` 좌우 20 → 10, `MinWidth` 120 → 100
- `WBP_LNPMenuButton.ButtonLabel` 폰트 24 → 18

두 에셋 모두 **이 디테일 패널에서만 쓰인다**(`BS_MenuAction` ← `WBP_LNPMenuButton` ← `WBP_ItemDetailPanel`)
— 다른 메뉴에 파급되지 않음을 확인하고 고쳤다. 라벨이 더 길어지면(예: `Merge (10/10)`) 다시 1+2로 갈릴 수
있으니, 그때는 폰트를 한 단계 더 줄이거나 디테일 패널 폭을 넓힌다.

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

### 7.3 PIE 게임패드 라우팅 — 메뉴를 닫으면 조작 대상 창이 바뀐다 (에디터 전용)

`Route Gamepad to Second Window`(2인 PIE, Single Process)를 켜고 1P를 키보드/마우스로,
2P를 게임패드로 조작하는 테스트 구성에서, **메뉴를 닫는 순간 게임패드 조작 대상이 반대 창으로 넘어간다.**
메뉴 버튼을 연달아 누르면 2P 열기/닫기 → 1P 열기/닫기가 교대로 반복되고,
뒤집힌 뒤에는 메뉴뿐 아니라 **이동·공격 등 게임패드 입력 전체가 반대 창으로** 간다.

원인은 세 가지가 겹친 것이다.

1. 그 옵션은 "2번 창으로 보낸다"가 아니라 **"게임패드 입력이 도착한 PIE 뷰포트에서 무조건 *다음*
   PIE 뷰포트로 한 칸 넘긴다"** 이다 — `UGameViewportClient::InputKey`(`GameViewportClient.cpp:743`)가
   `GEngine->GetNextPIEViewport(this)`(`PlayLevel.cpp:2532`, WorldList 순환)로 넘긴다. 포커스와 무관하다.
2. 따라서 **어느 뷰포트로 "도착"하느냐**가 결과를 정하고, 그건 Slate 포커스가 정한다.
   PIE 클라이언트들의 첫 LocalPlayer는 모두 `ControllerId == 0`이라
   `ULocalPlayer::GetSlateUser()`(`LocalPlayer.cpp:1802`)가 **FSlateUser(0) 하나를 공유**한다.
3. `SetInputMode`가 그 포커스를 옮긴다 — `FInputModeGameOnly::ApplyInputMode`가
   `SlateOperations.SetUserFocus(자기 뷰포트 위젯)`(`PlayerController.cpp:6446`)을 건다.
   즉 **메뉴를 닫은 창이 포커스를 가져가고**, 다음 게임패드 입력이 그 창으로 도착해 홉이 뒤집힌다.

**대응**(`ALNPPlayerController::CapturePIEForeignFocus` / `RestorePIEForeignFocus`, `#if WITH_EDITOR`):
`OpenMenu`에서 `SetInputMode` 직전에 — 그때 포커스를 쥔 창이 **내 창이 아니면** — 그 포커스 위젯을
약참조로 기억하고, `HandleMenuClosed`에서 `SetInputMode` **직후**에 되돌린다.
CVar `LNP.PIE.RestoreGamepadFocusOnMenuClose`(기본 1)로 끌 수 있다.

⚠️ **되돌릴 때 `FSlateApplication::SetUserFocus`를 직접 부르면 안 된다.** `SetInputMode`가 쌓은 것은
LocalPlayer의 지연 `FReply`이고 엔진 틱 말미의 `ProcessLocalPlayerSlateOperations`(`LaunchEngineLoop.cpp:5236`)에서
뒤늦게 적용되므로, 직접 옮긴 포커스를 도로 빼앗아 간다. 그래서 **같은 `FReply`의
`SetUserFocus`를 덮어쓴다**(포커스 수신자가 필드 하나라 마지막 지정이 이긴다).

한계 — 2P 메뉴가 **열려 있는 동안**에는 키보드/마우스도 2P 메뉴로 간다(Slate 유저 공유 구조상 불가피,
닫으면 복구). 기억해 둔 위젯이 그 사이 파괴되면 복원을 건너뛰므로 1P 창을 클릭해 수동 복구한다.

> 진짜 로컬 분할화면(한 창·한 뷰포트·LocalPlayer N개)에서는 이 문제가 성립하지 않는다.
> 입력이 `InputDevice → PlatformUserId → LocalPlayer`로 배분되고 포커스도 FSlateUser별로 갈리기 때문이다.
> 즉 이 코드는 **PIE 테스트 비계이지 분할화면 대비가 아니다.**

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

`Config/DefaultGame.ini` — ⚠️ **이게 없으면 어떤 키로도 버튼을 누를 수 없다:**
```ini
[/Script/CommonUI.CommonUISettings]
CommonButtonAcceptKeyHandling=TriggerClick
```

`ECommonButtonAcceptKeyHandling`의 기본값은 `Ignore`이고(UE 5.6 이전 프로젝트 기준, `CommonUISettings.cpp:16`),
그 모드에서는 `SCommonButton::OnKeyDown`이 **Slate의 Accept 키를 통째로 `Unhandled`로 흘려버린다**
(`CommonButtonTypes.cpp:136`). Accept 키는 `FNavigationConfig` 기본값 기준 `Enter`·`SpaceBar`·
`Virtual_Gamepad_Accept`(게임패드 ✕) 셋이므로, **키보드도 게임패드도 버튼을 누르지 못하고 마우스 클릭만 동작한다.**
인벤토리 Grid의 셀(`ULNPMenuItemCellWidget::NativeOnClicked` → 디테일 포커스 이동)도, Equip/Drop 버튼도 마찬가지다.

`Ignore`의 원래 의도는 "버튼은 각자의 `TriggeringInputAction`으로 눌러라"인데, 그러면 화면에 보이는 모든
버튼이 같은 Click 행을 두고 경쟁한다(액션 라우터는 포커스가 아니라 **가시성**으로 대상을 고른다 —
`UIActionRouterTypes.cpp:1047 IsWidgetReachableForInput`). 그래서 `TriggerClick`을 쓴다. 5.6부터의 신규 프로젝트 기본값이다.

⚠️ `TriggerClick`은 버튼이 포커스된 동안 Accept 키에 걸린 CommonUI 액션 바인딩을 막는다. 이 프로젝트는
Back이 `Esc`, 탭 이동이 `Q`/`E`라 겹치지 않는다. 폰의 `SpaceBar`(점프)는 메뉴가 열릴 때 `IMC_Pawn`이
통째로 제거되므로 역시 무관하다.

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
| `/Game/UI/Menu/WBP_LNPMenuRoot` | `ULNPMenuRootWidget` | `TabList`·`ContentSwitcher`·3탭·`HintBar`·`TabButtonClass`, 배경 블러·반투명 Border 2개 |
| `/Game/UI/Menu/WBP_LNPMenuHintBar` | `ULNPMenuHintBarWidget` | `HintBackground`(Border, 반투명 검정 — 상·하단 팝업과 같은 톤) → `HintContainer`(**UDynamicEntryBox**, `EntryWidgetClass`·`EntrySpacing` 지정) |
| `/Game/UI/Menu/WBP_LNPMenuHintEntry` | `ULNPMenuHintEntryWidget` | `GlyphText`·`LabelText`(CommonTextBlock). 칸의 생김새는 전부 여기서 결정 |
| `/Game/UI/Menu/Style/TS_MenuHintGlyph` | `UCommonTextStyle` | 키 심볼. **Regular** 16pt 흰색 (§11의 폰트 폴백 사유로 Bold 금지) |
| `/Game/UI/Menu/Style/TS_MenuHintLabel` | `UCommonTextStyle` | 조작 설명. Regular 14pt, 명도 0.72 |
| `/Game/UI/Menu/WBP_LNPMenuTabList` | `ULNPMenuTabListWidget` | `TabButtonContainer`(HorizontalBox). ⚠️ BindWidget 때문에 **BP로 감싸야** 한다 |
| `/Game/UI/Menu/WBP_LNPMenuTabButton` | `ULNPMenuTabButtonWidget` | `TabLabel`(CommonTextBlock) |
| `/Game/UI/Menu/WBP_LNPMenuButton` | `ULNPMenuButtonWidget` | 범용 버튼 — Equip/Drop에 사용. `ButtonLabel`(TextBlock) |
| `/Game/UI/Menu/WBP_MenuTab_Stats` | `ULNPStatsTabWidget` | ViewModel 등록 + RichText 바인딩, `WeaponIcon`, `BuffContainer` |
| `/Game/UI/Menu/WBP_MenuTab_Inventory` | `ULNPInventoryTabWidget` | `ItemGrid`(CommonTileView), `DetailPanel`. ⚠️ 아래 주의 참조 |
| `/Game/UI/Menu/WBP_MenuTab_Settings` | `ULNPSettingsTabWidget` | "준비 중" 문구 |
| `/Game/UI/Menu/WBP_MenuItemCell` | `ULNPMenuItemCellWidget` | `IconImage`, `EquipMarkText`(좌상단)·`DurationText`(우상단)·`LevelText`(우하단) — 모두 아웃라인 2(아이콘 위 시인성 확보) |
| `/Game/UI/Menu/WBP_BuffChip` | `ULNPBuffChipWidget` | `IconImage`, `TimeText` |
| `/Game/UI/Menu/WBP_ItemDetailPanel` | `ULNPItemDetailPanelWidget` | `IconImage`·`NameText`·`DetailText`·`EquipButton`·`MergeButton`·`DropButton` — 세 버튼은 `WrapBox` 안에 둔다(§5.3) |
| `/Game/UI/Menu/DT_LNPMenuTextStyles` | Rich Text Style Set | `final`/`sub`/`buff`, 모노스페이스 |

### 9.1 버튼 스타일 (`/Game/UI/Menu/Style/`)

시각 방향(사용자 결정): **무채색 + 밑줄 강조 + 둥근 모서리**. 강조색은 쓰지 않고 흰색/회색 명도로만 구분한다.

| 에셋 | 타입 | 용도 |
|:---|:---|:---|
| `TS_MenuTab_Normal / _Hovered / _Selected` | `UCommonTextStyle` | 탭 라벨 20pt, 명도 0.55 / 0.85 / 1.0, 자간 80 |
| `TS_MenuButton_Normal / _Hovered / _Disabled` | `UCommonTextStyle` | 버튼 라벨 16pt, 명도 0.78 / 1.0 / 0.32 |
| `BS_MenuTab` | `UCommonButtonStyle` | 브러시 전부 투명 — 선택은 **밑줄 위젯**과 텍스트 명도로만 |
| `BS_MenuAction` | `UCommonButtonStyle` | Equip/Merge/Drop. 둥근 사각(반경 5) + 흰 테두리, 상태별 알파. 좌우 패딩 10·MinWidth 100 — 한 줄에 2개가 들어가도록 맞춘 값(§5.3) |
| `BS_MenuItemCell` | `UCommonButtonStyle` | 인벤토리 셀. 반경 3, 선택·호버 시 테두리 2px |

⚠️ **밑줄은 `UCommonButtonStyle`로 만들 수 없다.** 버튼 브러시는 버튼 지오메트리 **전체**를 채우므로
"아래 몇 px만" 그릴 방법이 없다. `WBP_LNPMenuTabButton`의 트리에 `SelectionUnderline`(UImage)을 두고
`ULNPMenuTabButtonWidget::NativeOnSelected/NativeOnDeselected`가 가시성을 토글한다.
`Collapsed`가 아니라 **`Hidden`**을 쓰는 이유는 레이아웃 공간을 유지해 선택 전환 시 탭이 들썩이지 않게 하기 위함이다.

⚠️ 브러시의 둥근 모서리는 텍스처 없이 `DrawAs=RoundedBox` + `outlineSettings.roundingType=FixedRadius`로 만든다.

⚠️ **같은 함정을 힌트 바에서도 밟았다.** `ULNPMenuHintBarWidget`에 글리프/라벨 텍스트 스타일과 간격을
`EditDefaultsOnly` 프로퍼티로 뒀더니, `WBP_LNPMenuRoot`에 배치된 인스턴스가 CDO 값을 **상속하지 않고**
배치 시점 값을 직렬화해 들고 있었다(실측: CDO를 40으로 바꿔도 인스턴스는 18 유지). `EditDefaultsOnly`면
그 인스턴스가 디자이너 디테일 패널에 나오지도 않아 **에디터에서 고칠 방법이 아예 없다.**
지금은 그 프로퍼티들을 전부 없애고 시각 요소를 엔트리 WBP와 `UDynamicEntryBox`로 옮겨 문제를 소멸시켰다(§3.5).

⚠️ **`Style`은 CDO에 넣어도 이미 배치된 인스턴스에는 안 먹는다.** `UCommonButtonBase::Style`은 `EditAnywhere`라,
위젯을 다른 BP 트리에 배치하는 순간 그 시점 값(대개 `None`)이 **인스턴스에 직렬화**되어 CDO를 덮어쓴다.
그래서 버튼을 먼저 배치하고 나중에 CDO 스타일을 지정하면 그 인스턴스만 CommonUI 기본 스타일(밝은 회색)로 남는다.
배치형 버튼(`WBP_ItemDetailPanel`의 Equip/Merge/Drop)은 **인스턴스에도 직접** `Style`을 지정해야 한다.
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
- `UCommonBoundActionBar` 기반 하단 액션 바 — `WBP_LNPMenuRoot`의 `ActionBar` 위젯과
  `WBP_LNPActionBarButton` 에셋 (사유는 §3.3). 대체 = `ULNPMenuHintBarWidget`
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

✅ **게임패드 조작 확인 (사용자 실측)** — 탭 L1/R1, Grid 셀 네비게이션, ✕ 디테일 진입,
○ Grid 복귀·메뉴 닫기 전부 정상. **단 하단 액션 바만 비어 있었다** → §3.3의 원인 진단과 힌트 바 교체로 이어졌다.

✅ **힌트 바 렌더링 확인 (PIE, 키보드)** — 탭에 따라 힌트가 정확히 갈린다:

```
캐릭터 스탯 탭   : ESC Close                                Q/E Tab
인벤토리 탭(Grid): ESC Close   ENTER Details   WASD Move    Q/E Tab
```

기획 §8 표와 일치한다(스탯 탭은 ✕·방향 이동 행이 "—"). 탭 전환 시 즉시 갱신됨.

✅ **키·포커스 조작 확인 (사용자 실측)** — `TriggerClick` 적용 후 Enter/✕로 셀→디테일 진입 정상.
포커스 링이 키보드·게임패드 양쪽에서 즉시 붙고, Grid↔디테일 전환도 자연스럽다.
메뉴 첫 오픈에서 첫 셀에 링이 붙는 것, 마우스 클릭 시에는 링이 안 뜨는 것(의도) 확인.
빈 인벤토리에서 힌트가 `ESC Close · Q/E Tab`으로 줄고 그리드 전체 링이 없는 것,
버프 만료로 목록이 비는 순간 힌트가 실시간 갱신되는 것까지 확인.

✅ **폰트 폴백 경로 확인** — `/Engine/EngineFonts/Roboto`의 `fallbackTypeface`가
`Faces/DroidSansFallback`이고 문자 범위 제한이 없다. DroidSansFallback의 cmap에 `○ □ △ ✕ ↑←`가 모두 있다.
⚠️ 단 폴백 타입페이스에는 **`Regular` 페이스 하나뿐**이라, 글리프 스타일을 `Bold`로 두면
`○ □ △`(폴백)만 Regular로 나와 `L1 R1`(Roboto Bold)과 굵기가 섞인다.
그래서 `TS_MenuHintGlyph`는 **Regular 16pt + 흰색**으로 두고 라벨(Regular 14pt, 명도 0.72)과 대비시킨다.

## 12. 다국어 (로컬라이제이션)

원본은 영문이고, 번역이 준비되면 `Content/Localization/Game/<culture>/Game.po`만 채우면 되도록 기반을 깔았다.

- 타깃 설정: **`Config/Localization/Game.ini`** (`NativeCulture=en`, `CulturesToGenerate=en,ko`,
  소스 `Source/*` + 에셋 `Content/*` 수집)
- 런타임 경로 등록은 **불필요** — `Engine/Config/BaseGame.ini`가 이미
  `+LocalizationPaths=%GAMEDIR%Content/Localization/Game`을 갖고 있다
- 수집: 에디터 `Window > Localization Dashboard > Gather Text`, 또는
  `UnrealEditor-Cmd.exe LootNPop.uproject -run=GatherText -config="Config/Localization/Game.ini"`
- `Content/Localization/Game/` 이하는 **생성물**이다. 단 `<culture>/Game.archive`(및 추후 `Game.po`)에는
  번역 결과가 담기므로 커밋 대상이다

✅ **수집 검증 (2026-08-15)** — 34개 항목 수집. 힌트 바 라벨 6종(`Close`/`Back`/`Details`/`Confirm`/`Move`/`Tab`)과
기존 메뉴 문자열(`CHARACTER`/`INVENTORY`/`SETTINGS`/`Equip`/`Drop`/`Equipped`/`No item`/`Lv. {0}`/`Remaining {0}s`),
프롬프트 폴백 `F`가 모두 들어왔고 **키 글리프 심볼은 하나도 수집되지 않았다**(의도대로).
`Content/Localization/Game/{en,ko}/Game.archive`·`Game.locres` 생성 확인.

⚠️ 사용자에게 보이는 문자열은 `NSLOCTEXT("LNPMenu", ...)`로 쓴다. `FText::FromString`은 수집되지 않는다.
반대로 키 글리프는 번역 대상이 아니므로 **일부러** `FText::FromString`을 쓴다 (§3.4).

⚠️ **`GatherTextFromSource` 스텝은 `IncludePathFilters`가 아니라 `SearchDirectoryPaths`를 읽는다.**
(실측) 잘못 쓰면 `LogGatherTextFromSourceCommandlet: Warning: No search directory paths in section GatherTextStep0.`
한 줄만 남기고 소스 문자열을 하나도 수집하지 않은 채 **성공 종료한다.** 에셋 스텝(`GatherTextFromAssets`)은
`IncludePathFilters`가 맞는 키라 정상적으로 돌기 때문에 매니페스트에 항목이 생기고, 그래서 눈치채기 어렵다.
`Prepass`에서 `*.cpp = 0 files`가 찍히는지로 확인한다.

⚠️ 아직 `FText::FromString`으로 남아 있어 번역이 안 되는 곳 2군데:
`LNPItemDetailPanelWidget.cpp`의 아이템 이름(`Definition->GetName()` 폴백)과
`LNPStatsViewModel.cpp`의 조립된 리치 텍스트. 아이템 이름은 `ULNPItemDefinition`에 `FText` 표시명을
추가해야 하고, 스탯 리드아웃은 포맷 문자열 분해가 필요하다.

## 13. 잔여

- 하단 힌트 바 실기기 검증 — 키보드/게임패드 전환 시 글리프가 즉시 바뀌는지, `○ □ △`가 두부 박스로
  뜨지 않는지, 인벤토리 Grid↔디테일 전환 시 라벨이 따라오는지
- 인벤토리 실사용 검증 — 아이템 획득 후 Grid 표시, 장착 배지, 버프 잔여시간 카운트다운, Equip/Drop
- 2인 PIE에서 일시정지가 걸리지 않는지 확인 (스탠드얼론 전용 규칙)
- 환경설정 탭 내용
- 스탯 리드아웃 `C (A × B)` PIE 실측 — 곱연산 버프 중복 시 B가 140→180→220%로 합산되는지
