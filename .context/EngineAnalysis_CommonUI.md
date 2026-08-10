# Engine Analysis - Unreal Engine 5.8 CommonUI

## 1. 개요 (Overview)

CommonUI는 **게임용 UI 프레임워크** 플러그인입니다. UMG는 "위젯을 만들고 배치하는 수단"까지만 제공하고,
게임 UI가 실제로 필요로 하는 것들 — 게임패드 조작, 화면 스택, 입력 소유권, 버튼 룩의 일관성 —
은 프로젝트가 매번 직접 짜야 했습니다. CommonUI는 그 반복을 표준화한 층입니다.

핵심 철학:

- **위젯이 "화면(screen)" 단위를 안다.** 활성/비활성 개념을 도입해, 어떤 화면이 지금 입력과 포커스를 갖는지를 프레임워크가 관리합니다.
- **입력은 키가 아니라 "액션"으로 다룬다.** `Back`·`Confirm` 같은 의미 단위를 DataTable에 정의하고, 키보드/게임패드/터치 매핑과 표시 아이콘을 거기에 묶습니다.
- **룩은 데이터다.** 버튼·텍스트 스타일을 에셋으로 분리해 수십 개 위젯이 한 곳을 바라보게 합니다.
- **게임패드가 1급 시민.** 포커스 이동, 액션 바 아이콘 자동 전환, 아날로그 커서를 기본 제공합니다.

전형적인 용도: 인게임 메뉴/인벤토리/상점, 타이틀·로비 화면, 콘솔 대응이 필요한 모든 UI.
반대로 HUD처럼 **입력을 받지 않고 정보만 표시하는 UI**에는 이득이 적습니다 — 순수 UMG로 충분합니다.

> ⚠️ CommonUI는 UMG를 **대체하지 않습니다.** UMG 위에 얹히는 층이며, 아래 두 층의 규칙(Slate 레이아웃 2패스,
> UMG 위젯 수명주기, `BindWidget` 해석 범위)은 그대로 적용됩니다. 문제가 생기면 "몇 층 문제인가"를 먼저 갈라야 합니다.
> 아래 두 층은 [EngineAnalysis_SlateUMG.md](EngineAnalysis_SlateUMG.md)에 정리돼 있습니다.

---

## 2. 아키텍처 큰 그림

```
┌─ CommonUI ── 화면 활성화 · 입력 액션 라우팅 · 스타일 에셋 · 게임패드
│      ↑ 얹힘
├─ UMG ─────── UObject 래퍼 (에디터 편집 · 블루프린트 · 리플렉션)
│      ↑ 감쌈
└─ Slate ───── 실제 레이아웃 · 렌더링 · 입력 수신
```

런타임 구성:

```
[UGameViewportClient = UCommonGameViewportClient]   ← 입력을 라우터로 먼저 흘려보냄
        │
        ▼
[UCommonUIActionRouterBase]  (LocalPlayer 서브시스템)
        │  · 활성 위젯 트리를 노드로 관리
        │  · 액션 바인딩 매칭 → 핸들러 호출
        │  · 활성 화면의 FUIInputConfig를 적용 (게임/UI 입력 소유권)
        ▼
[UCommonActivatableWidgetStack]      ← 뷰포트에 상주하는 화면 컨테이너
        │  push/pop, 위젯 인스턴스 풀링
        ▼
[UCommonActivatableWidget]           ← "화면" 하나
        ├── UCommonTabListWidgetBase  → UCommonAnimatedSwitcher (탭 컨텐츠)
        ├── UCommonButtonBase 파생     → UCommonButtonStyle 참조
        ├── UCommonListView / TileView → 엔트리 위젯 풀링
        └── UCommonBoundActionBar      → 현재 바인딩된 액션을 아이콘으로 표시
```

| 구성 요소 | 역할 |
|:---|:---|
| `UCommonUserWidget` | CommonUI 기본 위젯. UI 액션 바인딩 등록(`RegisterUIActionBinding`) 지원 |
| `UCommonActivatableWidget` | "화면" 단위. 활성/비활성 수명주기, Back 처리, 포커스 대상, 입력 설정 |
| `UCommonActivatableWidgetContainerBase` | 화면 컨테이너 베이스. 파생: `...Stack`(후입선출), `...Queue`(선입선출) |
| `UCommonUIActionRouterBase` | LocalPlayer 서브시스템. 입력 → 액션 매칭, 입력 모드 적용 |
| `UCommonInputSubsystem` | 현재 입력 방식(KBM/게임패드/터치) 추적·통지 |
| `UCommonUIInputData` | 프로젝트 기본 Click/Back 액션 지정 (`UObject` 파생 BP) |
| `CommonInputActionDataBase` | 액션 1종의 DataTable 행 구조체 (키·아이콘·표시명) |
| `UCommonButtonBase` | 버튼 베이스 (**Abstract**). 상태별 브러시·텍스트 스타일을 스타일 에셋에서 가져옴 |
| `UCommonButtonStyle` / `UCommonTextStyle` | 룩 데이터 에셋 (`UObject` 파생 BP) |
| `UCommonTabListWidgetBase` | 탭 목록. 스위처와 연동, 탭 이동 입력 액션 처리 |
| `UCommonBoundActionBar` | 현재 활성 액션들을 버튼 프롬프트로 나열 |
| `UCommonActionWidget` | 액션 1종의 입력 아이콘 표시 (입력 방식 따라 자동 교체) |

---

## 3. 핵심 개념 (Core Concepts)

### 3.1 UCommonActivatableWidget — "화면"

일반 `UUserWidget`에 **활성화(Activation)** 축을 하나 더한 것입니다. 뷰포트에 존재하는 것과
"지금 이 화면이 주인공인가"는 별개라는 발상입니다.

```cpp
ActivateWidget();      // → NativeOnActivated()
DeactivateWidget();    // → NativeOnDeactivated()
IsActivated();
OnActivated() / OnDeactivated()   // 네이티브 멀티캐스트 델리게이트
```

활성화 시 프레임워크가 하는 일:

1. `GetDesiredFocusTarget()`이 가리키는 위젯으로 포커스를 옮깁니다 (`bSupportsActivationFocus`, `bAutoRestoreFocus`).
2. `bIsBackHandler`가 켜져 있으면 **Back 액션 바인딩을 등록**합니다.
3. `GetDesiredInputConfig()`가 값을 돌려주면 그 입력 모드를 적용합니다.

주요 오버라이드:

| 함수 | 용도 |
|:---|:---|
| `NativeOnActivated` / `NativeOnDeactivated` | 화면이 주인공이 될 때/물러날 때. **열 때마다 실행됨** |
| `NativeGetDesiredFocusTarget` | 활성화 시 포커스를 받을 위젯 |
| `NativeOnHandleBackAction` | Back 입력 처리. `true`면 소비, `false`면 전파 |
| `GetDesiredInputConfig` | 이 화면의 입력 모드/마우스 캡처 설정 (`TOptional`) |

### 3.2 Back 액션의 전파 모델 — 통과(passthrough)가 없다

`bIsBackHandler = true`인 위젯은 Back 키를 **바인딩**하고, 그 바인딩이 걸리면 **그 위젯이 소비**합니다.
기본 구현은 이렇습니다 (`CommonActivatableWidget.cpp:347`):

```cpp
bool UCommonActivatableWidget::NativeOnHandleBackAction()
{
    if (bIsBackHandler)
    {
        if (!BP_OnHandleBackAction())
            DeactivateWidget();   // 기본 동작: 무조건 비활성화
        return true;
    }
    return false;
}
```

⚠️ **여기서 흔히 설계를 잘못 잡습니다.** "안쪽 패널이 Back을 받고, 처리할 게 없으면 바깥으로 넘긴다"를
기대하고 안쪽 위젯에도 `bIsBackHandler`를 켜면, 안쪽이 **항상** 소비해 버려 바깥 화면이 Back을 영영 못 받습니다.
액션 바인딩 스택에는 "안 쓰면 다음으로 넘김" 개념이 없습니다.

✅ **권장 패턴**: Back 핸들러는 **화면(스택에 올라가는 단위) 하나만** 켜고, 그 화면이 내부 상태를
직접 물어봐 분기합니다.

```cpp
bool UMyScreen::NativeOnHandleBackAction()
{
    if (InnerPanelHasFocus())      // 내부에서 한 단계 되돌릴 게 있으면
    {
        ReturnFocusToOuterPanel();
        return true;               // 소비 — 화면은 닫히지 않음
    }
    return Super::NativeOnHandleBackAction();  // 기본 = Deactivate → 스택이 pop
}
```

### 3.3 화면 컨테이너 — Stack / Queue, 그리고 인스턴스 풀링

```cpp
MyStack->AddWidget<UMyScreen>(ScreenClass);   // push (+ 자동 활성화)
MyStack->RemoveWidget(*Screen);               // pop
MyStack->GetActiveWidget();
```

컨테이너는 자식 화면의 `OnDeactivated`를 구독하고 있어서, **화면이 스스로 비활성화하면 컨테이너가
알아서 제거**합니다 (`CommonActivatableWidgetContainer.cpp`). 그래서 Back → `DeactivateWidget()` → pop이
별도 코드 없이 성립합니다.

⚠️ **가장 중요한 특성: 컨테이너는 위젯 인스턴스를 클래스별로 재사용합니다.**

```cpp
UCommonActivatableWidget* UCommonActivatableWidgetContainerBase::AddWidgetInternal(...)
{
    if (UCommonActivatableWidget* WidgetInstance = GeneratedWidgetsPool.GetOrCreateInstance(ActivatableWidgetClass))
    { ... }
}
```

같은 클래스를 두 번 push하면 **같은 객체가 돌아옵니다.** 이 사실이 §6.1 함정의 원인입니다.

### 3.4 입력 액션 — 키가 아니라 의미로 다룬다

액션 1종 = `CommonInputActionDataBase`를 행 구조체로 쓰는 **DataTable 한 행**입니다.

| 행 필드 | 내용 |
|:---|:---|
| `DisplayName` | 액션 바에 표시할 이름 |
| `KeyboardInputTypeInfo` | 키보드 키 + `AdditionalKeys`(보조 키 배열) + 표시 브러시 |
| `DefaultGamepadInputTypeInfo` | 게임패드 키 + 아이콘 |
| `GamepadInputOverrides` | 패드 종류별 예외 |
| `NavBarPriority` | 액션 바 정렬 순서 |

`AdditionalKeys`는 실제로 함께 등록됩니다 (`UIActionRouterTypes.cpp`의 `RegisterAdditionalKeysForTypeInfo`).
"주 키 + 보조 키" 구성을 만들 때 별도 행을 팔 필요가 없습니다.

프로젝트 전역 기본 액션은 `UCommonUIInputData` 파생 블루프린트에 지정합니다:

```
DefaultClickAction  → (DataTable, RowName)
DefaultBackAction   → (DataTable, RowName)
```

위젯이 액션을 직접 바인딩할 때는 `UCommonUserWidget::RegisterUIActionBinding(FBindUIActionArgs{...})`를 씁니다.

### 3.5 입력 소유권 — ECommonInputMode / FUIInputConfig

활성 화면의 `GetDesiredInputConfig()`가 **게임과 UI 중 누가 입력을 받는지**를 정합니다.

```cpp
enum class ECommonInputMode : uint8
{
    Menu,   // UI만 입력을 받음
    Game,   // 게임만 입력을 받음
    All,    // 둘 다
};

return FUIInputConfig(ECommonInputMode::All,
                      EMouseCaptureMode::NoCapture,
                      EMouseLockMode::DoNotLock);
```

⚠️ **`Menu`의 차단 범위가 넓습니다.** 엔진 주석(`CommonInputSettings.h`)이 명시하듯,
> *"When the active input mode is `ECommonInputMode::Menu`, ALL input components with lower priority than this will be fully blocked."*

즉 **PlayerController에 바인딩된 Enhanced Input 액션까지 전부 죽습니다.** 자세한 결과는 §6.3.

### 3.6 스타일 — UObject 블루프린트로서의 룩

`UCommonButtonStyle`은 상태별 브러시와 텍스트 스타일을 담은 **데이터 전용 UObject**입니다.
블루프린트로 파생 클래스를 만들어 값을 채우고, 버튼의 `Style` 프로퍼티에 **클래스**를 지정합니다.

| 그룹 | 필드 |
|:---|:---|
| Normal | `NormalBase`, `NormalHovered`, `NormalPressed` |
| Selected | `SelectedBase`, `SelectedHovered`, `SelectedPressed` |
| 기타 | `Disabled`, `ButtonPadding`, `CustomPadding`, `MinWidth/MinHeight`, 사운드 |
| 텍스트 | `NormalTextStyle`, `NormalHoveredTextStyle`, `SelectedTextStyle`, `SelectedHoveredTextStyle`, `DisabledTextStyle` (각각 `UCommonTextStyle` 파생 클래스) |

브러시는 `FSlateBrush`이므로 **텍스처 없이도** 만들 수 있습니다. `DrawAs = RoundedBox` +
`OutlineSettings.RoundingType = FixedRadius` + `CornerRadii`로 이미지 에셋 없는 둥근 버튼이 가능합니다.

> `UCommonButtonBase`는 `UCLASS(Abstract)`입니다. 위젯 트리에 직접 배치할 수 없고,
> 반드시 블루프린트 파생을 만들어 그 클래스를 써야 합니다.
> 그리고 **텍스트 개념이 없습니다** — 라벨이 필요하면 §6.5를 보십시오.

### 3.7 탭 — UCommonTabListWidgetBase

탭 목록과 컨텐츠 스위처를 연결해 주는 위젯입니다.

```cpp
TabList->SetLinkedSwitcher(ContentSwitcher);                      // UCommonAnimatedSwitcher 계열
TabList->RegisterTab(TabId, TabButtonClass, ContentWidget);       // 등록 순서 = 화면상 순서
TabList->SelectTabByID(TabId);
```

- 탭 컨텐츠를 활성/비활성 전환까지 시키려면 스위처를 `UCommonActivatableWidgetSwitcher`로 씁니다
  (`UCommonAnimatedSwitcher` 파생이라 `SetLinkedSwitcher`에 그대로 들어갑니다).
- 탭 이동 입력은 `NextTabInputActionData` / `PreviousTabInputActionData`(DataTable 행 핸들)로 지정합니다.
- ⚠️ 탭 버튼을 화면에 붙이는 일과 입력 수신은 **자동이 아닙니다.** §6.2 참조.

### 3.8 리스트 — CommonListView / CommonTileView

UMG의 `UListView`/`UTileView`에 CommonUI 스타일과 게임패드 네비게이션을 얹은 것입니다.
엔트리 위젯은 **풀링·재사용**되며, 항목이 위젯에 배정되는 순간이 `NativeOnListItemObjectSet`입니다.
엔트리 클래스를 `UCommonButtonBase` 파생으로 만들면 셀 포커스·선택이 CommonUI 규칙을 따릅니다.

---

## 4. 필수 프로젝트 설정

CommonUI는 배선이 빠지면 **에러 없이 조용히 절반만 동작**합니다. 다음 4가지가 전제입니다.

**① 모듈 의존성** — `Build.cs`의 `PublicDependencyModuleNames`에 `"CommonUI"`, `"CommonInput"`.

**② 뷰포트 클라이언트 교체** — 입력이 액션 라우터를 거치게 하려면 필수입니다.

```ini
; DefaultEngine.ini
[/Script/Engine.Engine]
GameViewportClientClassName=/Script/CommonUI.CommonGameViewportClient
```

**③ 입력 데이터 지정** — Click/Back 기본 액션.

```ini
; DefaultGame.ini  ⚠️ Engine.ini 아님
[/Script/CommonInput.CommonInputSettings]
InputData=/Game/UI/DA_MyCommonInputData.DA_MyCommonInputData_C
```

⚠️ `UCommonInputSettings`는 `UCLASS(config = Game)`입니다. `DefaultEngine.ini`에 써 두면
**조용히 무시**되고 `InputData`가 `None`으로 남습니다 (§6.4).

**④ 에셋** — 액션 DataTable(`CommonInputActionDataBase` 행) + `UCommonUIInputData` 파생 BP.

---

## 5. 위젯 수명주기 — 어떤 훅에 무엇을 넣을 것인가

CommonUI를 쓰면 훅이 네 종류로 늘어나고, **호출 횟수가 서로 다릅니다.** 이 표가 §6.1 함정의 예방책입니다.

| 훅 | 호출 시점 | 횟수 |
|:---|:---|:---|
| `NativeOnInitialized` | 위젯 객체 초기화 | **인스턴스당 1회** |
| `NativeConstruct` | Slate 위젯 생성(뷰포트 진입) | 진입할 때마다 |
| `NativeOnActivated` | 화면이 활성화 | **활성화할 때마다** |
| `NativeOnDeactivated` / `NativeDestruct` | 비활성화 / 해제 | 그때마다 |

원칙:

- **1회성 배선**(델리게이트 구독 대상이 불변인 것, 하위 위젯 생성) → `NativeOnInitialized`
- **열 때마다 필요한 상태 구성**(데이터 구독, 목록 채우기, 탭 등록) → `NativeOnActivated`
- **해제** → `NativeOnDeactivated` (활성화와 짝을 맞춘다)

⚠️ 컨테이너가 인스턴스를 재사용(§3.3)하므로, `NativeOnInitialized`는 **두 번째 열기부터 실행되지 않습니다.**

---

## 6. 작업 시 유의점 (Pitfalls)

### 6.1 ⚠️ 컨테이너의 위젯 재사용 + `NativeDestruct`의 정리 = 두 번째 열기부터 빈 화면 (가장 흔함)

증상: 처음 열 때는 정상인데 **닫았다 다시 열면 일부 UI가 사라지고**, 남은 것도 반응하지 않습니다.

원인 체인:

1. 컨테이너는 클래스별로 위젯 인스턴스를 **풀에서 재사용**합니다 (§3.3).
2. 상당수 CommonUI 위젯은 `NativeDestruct`에서 **자기 내용을 정리**합니다.
   대표적으로 `UCommonTabListWidgetBase::NativeDestruct`는 `RemoveAllTabs()`를 호출해 등록된 탭을 전부 지웁니다.
3. 그런데 `NativeOnInitialized`는 인스턴스당 1회뿐입니다.

```
1회차: OnInitialized(등록) → Construct → 정상
닫기 : Destruct → 내용 삭제
2회차: Construct 만 (OnInitialized 없음) → 내용 0개
```

✅ **해결**: 매번 필요한 구성은 `NativeOnActivated`에서 하고, 중복만 막습니다.

```cpp
void UMyScreen::NativeOnActivated()
{
    Super::NativeOnActivated();
    if (TabList && TabList->GetTabCount() == 0)   // 재진입 시에만 다시 등록
    {
        TabList->SetLinkedSwitcher(ContentSwitcher);   // 같은 값이면 내부에서 조기 반환됨
        TabList->RegisterTab(...);
    }
}
```

> 같은 이유로, 컨테이너에 push되는 화면은 **재사용된다고 가정하고** 상태를 짜야 합니다.
> 멤버 변수가 지난번 값을 그대로 들고 있습니다.

### 6.2 ⚠️ 탭은 기본값이 "아무것도 안 함"이다 — 세 가지를 직접 켜야 한다

`UCommonTabListWidgetBase`는 셋 다 조용히 실패합니다. 로그도 경고도 없습니다.

**① `HandleTabCreation_Implementation`이 빈 구현**

`RegisterTab`은 버튼 인스턴스를 만들기만 하고 **어떤 패널에도 붙이지 않습니다.**
파생 클래스가 이 `BlueprintNativeEvent`를 구현해 직접 컨테이너에 넣어야 탭 바가 보입니다.

```cpp
void UMyTabList::HandleTabCreation_Implementation(FName TabId, UCommonButtonBase* TabButton)
{
    if (TabButtonContainer && TabButton)
        TabButtonContainer->AddChild(TabButton);
}
// 짝이 되는 HandleTabRemoval_Implementation에서 RemoveChild도 함께 구현
```

**② `bAutoListenForInput`의 기본값이 `false`**

`NativeConstruct`가 이 플래그를 보고서야 `SetListeningForInput(true)`를 호출하고, 그래야
`Next/PreviousTabInputActionData`가 바인딩됩니다. 켜지 않으면 **탭 이동 입력이 전부 무반응**입니다.
생성자에서 `bAutoListenForInput = true;` 하거나 위젯 인스턴스에서 체크합니다.

**③ 등록 시점에 `TabButtonGroup`이 있어야 함**

`RegisterTab`은 `TabButtonGroup == nullptr`이면 경고만 남기고 실패합니다. 이 그룹은 탭 리스트의
`NativeOnInitialized`에서 만들어지므로, **탭 등록은 그보다 나중**(권장: 소유 화면의 `NativeOnActivated`)이어야 합니다.

### 6.3 ⚠️ `ECommonInputMode::Menu`는 게임 입력을 전부 죽인다 — 같은 키로 UI를 닫을 수 없다

증상: 메뉴를 여는 키는 동작하는데, **열린 상태에서 같은 키를 눌러도 닫히지 않습니다.**

원인: 활성 화면의 기본 입력 모드가 `Menu`이고, 이 모드는 우선순위가 낮은 입력 컴포넌트를
**전면 차단**합니다(§3.5). 메뉴 토글 키를 PlayerController의 Enhanced Input에 바인딩해 뒀다면,
메뉴가 열린 순간 그 액션이 아예 들어오지 않습니다. 토글 코드를 넣어도 호출조차 되지 않습니다.

✅ **해결 A**: 화면이 `ECommonInputMode::All`을 요구하게 합니다. 게임 입력이 함께 살아나므로,
메뉴를 열 때 폰의 매핑 컨텍스트를 제거하는 등 **게임플레이 입력은 별도로 차단**하는 구성과 궁합이 좋습니다.

```cpp
TOptional<FUIInputConfig> UMyScreen::GetDesiredInputConfig() const
{
    return FUIInputConfig(ECommonInputMode::All, EMouseCaptureMode::NoCapture, EMouseLockMode::DoNotLock);
}
```

✅ **해결 B**: 닫기를 게임 입력이 아니라 **CommonUI 액션**(Back 등)으로만 처리합니다.

⚠️ **A와 B를 같은 키에 겹치지 마십시오.** 한 키가 CommonUI Back과 Enhanced Input 양쪽에 걸리면
"Back이 닫음 → 직후 게임 입력이 다시 엶"으로 **도로 열립니다.** 키를 나누거나, 게임 입력 핸들러가
"이미 열려 있으면 즉시 return"하도록 가드해야 합니다.

### 6.4 ⚠️ `CommonInputSettings`는 `config = Game`이고, 시작 시 1회만 로드된다

`UCommonInputSettings`는 `UCLASS(config = Game)`이므로 설정은 **`DefaultGame.ini`**에 있어야 합니다.
`DefaultEngine.ini`에 쓰면 조용히 무시되고 `InputData`가 `None`으로 남으며, 증상은 이렇게 나타납니다:

```
LogUIActionRouter: Error: Cannot create action binding for widget [...] - no action provided.
```

이 상태에서는 Back·Click·탭 이동이 **전부** 죽습니다.

또한 이 데이터는 **에디터/게임 시작 시 1회만 로드**되고 `bInputDataLoaded`에 캐시됩니다.
ini를 고쳤거나 입력 데이터 에셋을 새로 만든 직후에는 **에디터 재시작**이 필요합니다 —
실행 중 CDO 프로퍼티를 직접 써도 내부 `InputDataClass`는 갱신되지 않습니다.

### 6.5 ⚠️ `UCommonButtonBase`에는 텍스트가 없다

버튼 블루프린트 안에 TextBlock을 넣어 두기만 하면 **아무도 글자를 쓰지 않습니다.**
스타일의 배경 알파가 낮으면 "빈 상자"로 보여 원인을 찾기 어렵습니다.

✅ 라벨 setter를 가진 파생 클래스를 만들고, 소유 위젯이 명시적으로 값을 넣습니다.

```cpp
UCLASS()
class UMyLabeledButton : public UCommonButtonBase
{
    GENERATED_BODY()
public:
    void SetButtonLabel(const FText& InText) { if (Label) Label->SetText(InText); }
protected:
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> Label;   // UCommonTextBlock도 UTextBlock 파생이라 둘 다 바인딩됨
};
```

> 참고로 `UCommonBoundActionButton`(액션 바 버튼)은 반대로 `Text_ActionName`이라는
> **필수 `BindWidget`**을 요구합니다. 없으면 컴파일 에러가 납니다.

### 6.6 ⚠️ `Style`은 CDO에 넣어도 이미 배치된 인스턴스에는 적용되지 않는다

`UCommonButtonBase::Style`은 `EditAnywhere`입니다. 버튼 위젯을 다른 블루프린트의 트리에
**배치하는 순간 그 시점 값(대개 `None`)이 인스턴스에 직렬화**되어 CDO를 덮어씁니다.
따라서 "버튼을 먼저 배치하고 나중에 CDO 스타일을 지정"하면 그 인스턴스만 기본 스타일로 남습니다.

- **런타임 생성형**(탭 버튼, 리스트 엔트리, 액션 바 버튼)은 클래스에서 생성되므로 CDO만으로 충분합니다.
- **배치형**(패널에 직접 놓은 버튼)은 **인스턴스에도** `Style`을 지정해야 합니다.

같은 함정이 `EditAnywhere` 계열 프로퍼티 전반에 적용됩니다 — 배치 순서가 값에 영향을 줍니다.

### 6.7 ⚠️ UI 액션에 배정한 키는 Slate 네비게이션보다 먼저 소비된다

방향키를 탭 이동·확인·취소 같은 액션에 배정하면, 그 키들이 액션 라우터에서 소비되어
**리스트/그리드의 방향 이동이 동작하지 않습니다.** 액션과 네비게이션은 같은 키를 나눠 쓸 수 없습니다.

✅ 방향키는 네비게이션 전용으로 비워 두고, 액션은 다른 키(`Q`/`E`, `Enter`, `Esc` 등)에 배정합니다.
추가 네비게이션 키(WASD 등)가 필요하면 `FNavigationConfig`를 파생해 규칙을 더합니다.

```cpp
class FMyNavigationConfig : public FNavigationConfig
{
public:
    FMyNavigationConfig()   // 기본 생성자가 화살표·D-Pad·Enter/Space·Esc를 이미 깔아 준다
    {
        KeyEventRules.Emplace(EKeys::W, EUINavigation::Up);
        KeyEventRules.Emplace(EKeys::A, EUINavigation::Left);
        KeyEventRules.Emplace(EKeys::S, EUINavigation::Down);
        KeyEventRules.Emplace(EKeys::D, EUINavigation::Right);
    }
};
FSlateApplication::Get().SetNavigationConfig(MakeShared<FMyNavigationConfig>());
```

⚠️ **네비게이션 설정은 `FSlateApplication` 전역이고, PIE는 에디터와 Slate를 공유합니다.**
상시 등록하면 에디터 패널까지 영향을 받고, UI를 연 채 PIE를 끝내면 그 상태가 에디터에 남습니다.
**UI 수명에만 걸고 `EndPlay`에서도 반드시 원복**하십시오. (CommonUI는 자체 `FNavigationConfig`를
설치하지 않으므로, 교체해도 프레임워크와 충돌하지는 않습니다.)

### 6.8 ⚠️ 리스트는 "항목 내부의 변화"를 모른다

`SetListItems`는 목록의 **추가·제거**만 반영합니다. 항목 객체는 그대로인데 그 안의 상태만 바뀌는 경우
(장착 여부, 잠금, 수량 등) 목록이 포인터·순서까지 동일하므로, SListView가 기존 행 위젯을 그대로 재사용하고
`NativeOnListItemObjectSet`을 다시 호출하지 않습니다 → **셀 표시가 갱신되지 않습니다.**

✅ `RegenerateAllEntries()`를 호출합니다. 엔진도 `ListViewBase.h`의 `RequestRefresh` 주석에서 이 경우를 명시합니다:
> *"it's also reasonable (though not ideal) to call RegenerateAllEntries when changes within N list items need to be reflected."*

항목 수가 많고 갱신이 잦다면, 엔트리 위젯이 자기 데이터의 변경 델리게이트를 직접 구독하는 편이 낫습니다.

### 6.9 UMG 기본 규칙은 그대로 적용된다

CommonUI를 쓴다고 아래 층 규칙이 사라지지 않습니다
(전체는 [EngineAnalysis_SlateUMG.md](EngineAnalysis_SlateUMG.md) 참조). 특히 자주 물리는 셋:

- **`BindWidget`은 위젯 블루프린트의 트리 안에서만 해석됩니다.** `BindWidget`을 가진 C++ 위젯 클래스를
  블루프린트로 감싸지 않고 다른 위젯 트리에 **직접 배치**하면, 그 위젯의 트리가 비어 있으므로
  바인딩이 영원히 `null`입니다. 반드시 파생 위젯 BP를 만들어 그 클래스를 배치하십시오.
- **슬롯의 크기 규칙(Auto/Fill)이 자식이 받는 공간을 결정합니다.** 예를 들어 박스 슬롯이 `Auto`면
  TileView는 자기 희망 너비(≈셀 1개)만 받아 **한 줄로 늘어섭니다.** 열 수는 할당 너비 ÷ EntryWidth로 정해집니다.
- **`UImage`의 브러시 `ImageSize`와 슬롯 정렬이 함께 크기를 만듭니다.** `HAlign_Fill`이면 가로만 늘어나고
  세로는 `ImageSize`에 묶여 납작해집니다.

---

## 7. 최소 구현 체크리스트

새 프로젝트에 CommonUI 기반 화면을 붙일 때:

1. **모듈**: `Build.cs`에 `"CommonUI"`, `"CommonInput"`
2. **Config**: `DefaultEngine.ini`에 `GameViewportClientClassName`, **`DefaultGame.ini`**에 `CommonInputSettings.InputData`
3. **입력 에셋**: 액션 DataTable(`CommonInputActionDataBase`) + `UCommonUIInputData` 파생 BP (Click/Back 지정)
4. **스타일 에셋**: `UCommonTextStyle` 파생 몇 종 → `UCommonButtonStyle` 파생에서 상태별 브러시·텍스트 스타일 구성
5. **레이아웃**: 뷰포트 상주 위젯에 `UCommonActivatableWidgetStack`을 두고, 화면은 여기에 push/pop
6. **화면 클래스**: `UCommonActivatableWidget` 파생.
   `bIsBackHandler`는 **화면 단위로 하나만**, 내부 단계 되돌리기는 `NativeOnHandleBackAction`에서 분기
7. **열 때마다 필요한 구성은 `NativeOnActivated`에** (컨테이너 재사용 때문)
8. **버튼**: `UCommonButtonBase`는 Abstract → BP 파생 필수. 라벨 setter를 가진 C++ 파생 준비.
   배치형 버튼은 인스턴스에도 `Style` 지정
9. **탭을 쓴다면**: `HandleTabCreation`/`HandleTabRemoval` 구현 + `bAutoListenForInput = true` +
   등록은 `TabButtonGroup` 생성 이후
10. **키 배정**: 방향키는 네비게이션 전용으로 남기고, 액션은 다른 키에. 게임 입력과 UI 액션에 **같은 키를 겹치지 말 것**

### 흔한 증상 → 원인 빠른 표

| 증상 | 먼저 확인할 것 |
|:---|:---|
| Back/Click/탭 이동이 전부 무반응 | `InputData`가 `DefaultGame.ini`에 있는가, 에디터 재시작했는가 (§6.4) |
| 탭 바가 안 보임 | `HandleTabCreation` 구현했는가 (§6.2①) |
| 탭 이동 키만 무반응 | `bAutoListenForInput` 켰는가 (§6.2②) |
| 두 번째 열기부터 UI가 빔 | 구성을 `NativeOnInitialized`에 두었는가 (§6.1) |
| 여는 키로 닫히지 않음 | 입력 모드가 `Menu`인가 (§6.3) |
| 버튼이 흰 상자로 보임 | 인스턴스 `Style`이 `None`인가 (§6.6), 라벨을 넣었는가 (§6.5) |
| 리스트/그리드 방향 이동 불가 | 방향키를 UI 액션에 배정했는가 (§6.7) |
| 셀 표시가 갱신 안 됨 | 항목 내부 변화인가 → `RegenerateAllEntries` (§6.8) |
| 리스트가 한 줄로 늘어섬 | 슬롯 크기 규칙이 `Auto`인가 (§6.9) |
| `BindWidget`이 null | C++ 위젯을 BP로 감싸지 않고 직접 배치했는가 (§6.9) |
