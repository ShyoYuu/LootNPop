# Engine Analysis - Unreal Engine 5.8 Slate & UMG

## 1. 개요 (Overview)

언리얼의 UI는 **두 층**으로 되어 있습니다.

- **Slate** — 실제로 레이아웃을 계산하고, 그리고, 입력을 받는 C++ 위젯 시스템. **UObject가 아닙니다.**
  GC도 리플렉션도 없고 `TSharedPtr`로 수명을 관리합니다. 원래 **에디터를 만들기 위해** 설계됐습니다.
- **UMG** — Slate를 `UObject`로 감싼 층. 에디터에서 배치하고, 블루프린트에서 다루고, 애니메이션을 붙이기 위해 존재합니다.

핵심 명제:

> **UMG는 Slate를 대체하지 않습니다. 화면에 실제로 그려지는 것은 언제나 Slate입니다.**
> UMG 위젯은 대응하는 Slate 위젯을 생성하고, 자기 프로퍼티 값을 그쪽으로 밀어 넣는 껍데기입니다.

이 사실을 놓치면 "에디터에서 값을 바꿨는데 왜 런타임에 안 변하지?", "왜 이 위젯 크기가 내 뜻대로 안 되지?" 같은
문제의 원인을 엉뚱한 층에서 찾게 됩니다. 문제가 생기면 **몇 층 문제인지부터** 가르는 것이 이 문서의 목표입니다.

CommonUI를 쓰는 프로젝트라면 그 층이 하나 더 얹히지만, **아래 두 층의 규칙은 그대로 적용**됩니다.

---

## 2. 아키텍처 큰 그림

```
[UMG]   UImage (UObject)  ──RebuildWidget()──▶  SImage (SWidget)   [Slate]
          │  에디터에서 편집                        │  실제 렌더링·입력
          └──SynchronizeProperties()───────────────┘  값 밀어넣기

[UMG]   UUserWidget ── WidgetTree(자식 위젯 템플릿) ──▶ SObjectWidget으로 감싸져 Slate 트리에 편입
```

| 개념 | Slate | UMG |
|:---|:---|:---|
| 위젯 | `SWidget` | `UWidget` |
| 자식 여럿 담는 패널 | `SHorizontalBox`, `SOverlay` … | `UHorizontalBox`, `UOverlay` … (`UPanelWidget`) |
| 자리(배치 규칙) | `FSlot` (패널 내부 타입) | `UPanelSlot` 파생 |
| 조립 단위 | 직접 `SNew` 조합 | `UUserWidget` + 위젯 블루프린트 |
| 그리기 데이터 | `FSlateBrush` | 동일 (`FSlateBrush`) |

---

## 3. Slate 핵심

### 3.1 선언적 조립 문법

```cpp
TSharedRef<SWidget> W =
    SNew(SHorizontalBox)
    + SHorizontalBox::Slot()        // ← 슬롯 생성
      .FillWidth(1.5f)              // ← 슬롯 규칙
      .Padding(0, 0, 16, 0)
      .VAlign(VAlign_Center)
      [
          SNew(SImage)              // ← 자식 위젯
          .ColorAndOpacity(FLinearColor::White)   // ← 위젯 자신의 속성
      ];
```

**`[ ]` 바깥의 점 문법은 슬롯, 안쪽은 자식 위젯**입니다. 이 경계가 §4의 개념 경계와 정확히 일치합니다.
`SNew`는 `TSharedRef<SWidget>`을 반환하며 수명은 셰어드 포인터가 관리합니다(GC 아님).

위젯은 자식이 없는 **Leaf**(`SImage`, `STextBlock`)와 자식을 배치하는 **Panel**(`SHorizontalBox`, `SOverlay`)로 나뉩니다.

### 3.2 레이아웃은 2패스다 ★

Slate 레이아웃의 전부입니다.

```
① ComputeDesiredSize()   ▲ 자식 → 부모   "나는 이만큼이 필요합니다"
② OnArrangeChildren()    ▼ 부모 → 자식   "너에게 이만큼을 준다"
```

```cpp
virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const;
virtual void OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const;
```

**②는 ①을 무시할 수 있습니다.** 부모의 배분 정책이 최종 결정권을 갖습니다.

| 슬롯 크기 규칙 | 자식이 받는 공간 |
|:---|:---|
| `Auto` (AutoWidth/AutoHeight) | 자식의 **희망 크기만** |
| `Fill` | 남은 공간을 비율로 나눠서 |

> **진단 원칙**: "크기가 이상하다"의 대부분은 자식이 아니라 **부모 슬롯** 문제입니다.
> 예를 들어 리스트/타일 위젯이 세로로 한 줄만 늘어선다면, 대개 부모 박스 슬롯이 `Auto`라
> 위젯이 자기 희망 너비(≈셀 1개)만 받은 것입니다. 열 수는 `할당 너비 ÷ 셀 너비`로 정해지므로 1열이 됩니다.

### 3.3 FGeometry

`OnPaint`·`OnArrangeChildren`에 넘어오는 `FGeometry`는 **"내가 화면에서 차지한 사각형"**입니다.
로컬 ↔ 절대 좌표 변환을 담고 있어 히트 테스트와 자식 배치가 전부 이것을 통합니다.

### 3.4 그리기 — OnPaint와 FSlateBrush

```cpp
virtual int32 OnPaint(const FPaintArgs&, const FGeometry& AllottedGeometry,
                      const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
                      int32 LayerId, const FWidgetStyle&, bool bParentEnabled) const;
```

Slate는 즉시 렌더링이 아니라 **드로우 엘리먼트를 리스트에 쌓아** 나중에 일괄 렌더링합니다.
`LayerId`가 그리기 순서(클수록 위)이며, 자식을 그린 뒤 **증가한 LayerId를 반환**하는 것이 규칙입니다.

`FSlateBrush`는 "무엇을 어떻게 그릴지"의 데이터입니다.

| 필드 | 의미 |
|:---|:---|
| `ResourceObject` | 텍스처/머티리얼 (**없어도 됨** — 색만으로 그릴 수 있음) |
| `DrawAs` | `Image` / `Box`(9-슬라이스) / `Border` / `RoundedBox` |
| `TintColor` | 색·알파 |
| `ImageSize` | **희망 크기** — 레이아웃에 영향을 준다 |
| `Margin` | 9-슬라이스 분할 비율 |
| `OutlineSettings` | 테두리 색·두께, `RoundedBox`의 모서리 반경 |

> `DrawAs = RoundedBox` + `OutlineSettings.RoundingType = FixedRadius` + `CornerRadii` 조합으로
> **이미지 에셋 없이** 둥근 모서리 버튼/패널을 만들 수 있습니다.

⚠️ **크기는 브러시와 슬롯이 함께 만듭니다.** 이미지가 한쪽으로만 늘어나 납작해 보인다면,
슬롯이 그 축만 `Fill`이고 다른 축은 브러시 `ImageSize`에 묶여 있는 경우가 많습니다.

### 3.5 입력 — FReply와 버블링

```cpp
virtual FReply OnMouseButtonDown(const FGeometry&, const FPointerEvent&);
virtual FReply OnKeyDown(const FGeometry&, const FKeyEvent&);
```

반환값이 전부입니다.

- `FReply::Handled()` — 내가 처리했다. **전파 중단.**
- `FReply::Unhandled()` — 안 썼다. **부모로 전파(버블링).**

전파는 **자식 → 부모** 방향입니다. 히트 테스트로 가장 안쪽 위젯을 찾고 위로 올라가며
`Handled`가 나올 때까지 물어봅니다. 반대 방향(터널링)은 `OnPreview*` 계열이 담당합니다.

`FReply`는 부가 요청도 함께 실어 보냅니다: `.SetUserFocus(Widget)`, `.CaptureMouse(Widget)`, `.DetectDrag(...)`.

### 3.6 포커스와 네비게이션

Slate는 **유저별 포커스**를 관리합니다(로컬 멀티플레이 대비).
키보드·게임패드 입력은 히트 테스트가 아니라 **포커스된 위젯**으로 갑니다.

방향 이동은 `FNavigationConfig`가 키 → `EUINavigation`(Up/Down/Left/Right) 매핑을 갖고,
Slate가 그 방향의 다음 포커스 후보를 찾습니다. 기본 생성자가 화살표·D-Pad·좌스틱과
`Enter`/`Space`(Accept), `Esc`(Back)를 이미 등록합니다.

추가 키(WASD 등)가 필요하면 파생 클래스에서 규칙만 더합니다.

```cpp
class FMyNavigationConfig : public FNavigationConfig
{
public:
    FMyNavigationConfig()
    {
        KeyEventRules.Emplace(EKeys::W, EUINavigation::Up);
        KeyEventRules.Emplace(EKeys::A, EUINavigation::Left);
        KeyEventRules.Emplace(EKeys::S, EUINavigation::Down);
        KeyEventRules.Emplace(EKeys::D, EUINavigation::Right);
    }
};
FSlateApplication::Get().SetNavigationConfig(MakeShared<FMyNavigationConfig>());
```

⚠️ **네비게이션 설정은 `FSlateApplication` 전역입니다.** 그리고 **PIE는 에디터와 Slate를 공유**합니다.
상시 등록하면 에디터 패널까지 영향을 받고, 교체한 채 PIE를 끝내면 그 상태가 에디터에 남습니다.
특정 UI 수명에만 걸고 **종료 시 반드시 원복**해야 합니다.

### 3.7 Visibility 5종 — 레이아웃과 히트테스트는 별개

| 값 | 보이나 | 레이아웃 공간 | 입력 받나 |
|:---|:---:|:---:|:---:|
| `Visible` | O | 차지 | O |
| `HitTestInvisible` | O | 차지 | ✗ (자식도 ✗) |
| `SelfHitTestInvisible` | O | 차지 | ✗ (자식은 O) |
| `Hidden` | ✗ | **차지함** | ✗ |
| `Collapsed` | ✗ | **차지 안 함** | ✗ |

`Hidden` ↔ `Collapsed`의 차이가 실무에서 큽니다.

- 켜고 끌 때 **주변 레이아웃이 움직이면 안 되는 요소**(선택 표시선, 배지 자리)는 `Hidden`.
- 공간까지 회수해야 하는 요소는 `Collapsed`.
- 성능 관점에서는 `Collapsed`가 유리합니다 — `Hidden`은 레이아웃 계산이 계속 돕니다(§6.4).

---

## 4. 슬롯(Slot) — 가장 많이 헷갈리는 개념

### 4.1 정의

> **슬롯 = 부모가 자식에게 내주는 "자리"이자, 그 자리에 관한 규칙 묶음.**

위젯이 아니고, 그려지지도 않습니다. **부모-자식 사이의 계약**을 담은 객체입니다.

### 4.2 왜 위젯 속성이 아닌가

**같은 위젯이라도 어떤 부모 안에 있느냐에 따라 필요한 배치 정보가 완전히 달라지기 때문**입니다.

| 부모 | 자식에게 필요한 정보 |
|:---|:---|
| `HorizontalBox` / `VerticalBox` | 크기 규칙(Auto/Fill), HAlign, VAlign, Padding |
| `Overlay` | HAlign, VAlign, Padding (**크기 규칙 무의미** — 겹쳐 쌓기만 함) |
| `CanvasPanel` | Anchors, Offsets, Alignment, ZOrder (박스 개념 자체가 없음) |
| `GridPanel` | Row, Column, RowSpan, ColumnSpan |

이 정보들은 서로 호환되지 않습니다. 그렇다고 모든 위젯에 모든 부모 종류의 필드를 넣어둘 수는 없으므로,
**부모 종류마다 다른 슬롯 타입**을 만들어 거기에 담습니다. **소유자는 부모**입니다.

```
UVerticalBox
  ├─ Slot[0] ─ { 자식: A, Size: Auto,     HAlign: Left, Padding: (0,0,0,10) }
  ├─ Slot[1] ─ { 자식: B, Size: Auto,     HAlign: Fill, Padding: 0 }
  └─ Slot[2] ─ { 자식: C, Size: Fill 1.0, HAlign: Fill, Padding: 0 }
```

> 위젯이 *사람*이라면 슬롯은 *좌석*입니다. 좌석은 극장(부모)의 소유물이고, 같은 사람이 다른 극장에 가면
> **다른 종류의 좌석**에 앉습니다. 사람이 좌석을 들고 다니지 않습니다.

### 4.3 에디터에서 확인하는 법

박스 안의 자식 위젯을 선택하면 Details 패널이 두 구역으로 갈립니다.

```
▼ Slot (as Vertical Box Slot)   ← 부모가 주는 규칙 (= 슬롯)
    Padding / Size / Horizontal Alignment / Vertical Alignment
▼ Appearance, Behavior …        ← 위젯 자신의 속성
```

괄호 안 이름이 부모 종류에 따라 바뀝니다. 같은 위젯을 CanvasPanel로 옮기면
`Slot (as Canvas Panel Slot)`이 되고 항목이 통째로 달라집니다.

### 4.4 코드에서 접근

```cpp
// UMG — AddChild 계열이 슬롯을 반환한다
UVerticalBoxSlot* S = VerticalBox->AddChildToVerticalBox(MyImage);
S->SetSize(ESlateSizeRule::Fill);
S->SetPadding(FMargin(0, 0, 16, 0));
S->SetHorizontalAlignment(HAlign_Center);

// 모든 UWidget은 자기가 앉아 있는 좌석 포인터를 갖는다
if (UVerticalBoxSlot* MySlot = Cast<UVerticalBoxSlot>(MyImage->Slot))
    MySlot->SetPadding(...);
```

### 4.5 유의점

- **부모를 바꾸면 슬롯 설정이 날아갑니다** — 슬롯 타입 자체가 바뀌므로 옮겨 담을 곳이 없습니다.
- **슬롯은 그려지지 않습니다** — 배경색·테두리가 필요하면 `Border` 같은 위젯을 하나 끼워야 합니다.
- **자식 1개만 받는 위젯**(`Border`, `SizeBox`, `ScaleBox`, `Button`)과 **여럿 받는 패널**을 구분해 두면 좋습니다.

---

## 5. UMG 핵심

### 5.1 UWidget ↔ SWidget 다리

| 함수 | 역할 |
|:---|:---|
| `RebuildWidget()` | 대응하는 Slate 위젯을 **생성**해 반환 |
| `TakeWidget()` | 생성·캐시(`MyWidget`) 후 반환. 이미 있으면 재사용 |
| `SynchronizeProperties()` | UObject 쪽 값을 Slate 쪽으로 **밀어넣음** |
| `ReleaseSlateResources()` | Slate 참조 해제 |

⚠️ **"값을 바꿨는데 반영이 안 된다"의 대부분이 이 다리를 안 건넌 경우입니다.**
커스텀 `UWidget`을 만들 때 세터가 멤버만 바꾸고 Slate 쪽에 전달하지 않으면 런타임에 아무 일도 일어나지 않습니다.

```cpp
void UMyWidget::SetValue(float In)
{
    Value = In;
    if (MySlateWidget.IsValid())      // ← 빠뜨리기 쉬운 부분
        MySlateWidget->SetValue(In);
}
```

### 5.2 UUserWidget과 위젯 블루프린트

```
WBP_Foo (에셋)
  └─ UWidgetBlueprintGeneratedClass   (컴파일 산출물)
       ├─ WidgetTree   ← 배치한 위젯들의 템플릿
       ├─ Animations
       └─ 바인딩 정보
            ↓ CreateWidget
       UUserWidget 인스턴스 ── SObjectWidget으로 감싸져 Slate 트리에 편입
```

`UUserWidget`은 자기 `WidgetTree`를 갖는 특별한 `UWidget`입니다. 다른 위젯 안에 넣으면 그 트리가 통째로 들어갑니다.

```cpp
UMyWidget* W = CreateWidget<UMyWidget>(PlayerController, WidgetClass);
W->AddToViewport(ZOrder);     // 화면 공간
// 월드 공간에 띄우려면 액터에 UWidgetComponent를 붙인다
```

### 5.3 수명주기 ★

| 훅 | 시점 | 횟수 | 용도 |
|:---|:---|:---|:---|
| `NativeOnInitialized` | 인스턴스 초기화 | **1회** | 변하지 않는 배선 |
| `NativePreConstruct` | 위젯 빌드 직전 (**에디터 디자이너에서도 실행**) | 여러 번 | 미리보기 외형 |
| `NativeConstruct` | Slate 위젯 생성(뷰포트 진입) | 진입마다 | 런타임 초기화 |
| `NativeTick` | 매 프레임 | — | 되도록 피할 것 (§6) |
| `NativeDestruct` | Slate 해제 | 이탈마다 | 정리 |

⚠️ **`PreConstruct`는 에디터 디자이너에서도 실행됩니다.** 여기서 게임 상태(`GetWorld()`, PlayerController,
게임 서브시스템)에 접근하면 디자이너에서 오동작하거나 크래시합니다. `IsDesignTime()`으로 가드하십시오.

⚠️ **1회 훅과 매번 훅을 혼동하면 위젯이 재사용될 때 깨집니다.** 위젯을 풀링·재사용하는 컨테이너
(CommonUI의 화면 스택 등)를 쓰면 `NativeOnInitialized`는 두 번째부터 실행되지 않습니다.
**"열 때마다 필요한 구성"은 반드시 매번 도는 훅에** 두어야 합니다.

### 5.4 BindWidget

```cpp
UPROPERTY(meta = (BindWidget))          // 없으면 컴파일 에러
TObjectPtr<UTextBlock> NameText;

UPROPERTY(meta = (BindWidgetOptional))  // 없으면 null
TObjectPtr<UImage> IconImage;
```

컴파일 시 **위젯 트리에서 이름이 같은 위젯**을 찾아 대입합니다. 타입도 맞아야 하며,
부모 타입으로 선언하면 파생도 받습니다(`UTextBlock`으로 선언하면 `UCommonTextBlock`도 바인딩됨).

⚠️ **해석 범위는 "그 위젯 블루프린트의 트리" 안입니다.** `BindWidget`을 가진 C++ 위젯 클래스를
블루프린트로 감싸지 않고 **다른 위젯 트리에 직접 배치**하면, 그 위젯의 트리가 비어 있으므로
바인딩이 영원히 `null`입니다. 반드시 파생 위젯 BP를 만들어 그 클래스를 배치하십시오.

⚠️ 위젯의 **`Is Variable` 체크가 꺼져 있으면 바인딩되지 않습니다.** 트리에 분명히 있는데 null이면 여기를 보십시오.

### 5.5 위젯 애니메이션

UMG의 **위젯 애니메이션**은 "움직이는 위젯" 일반이 아니라, 위젯 블루프린트의 `Animations` 패널에서
타임라인으로 저작하는 **시퀀서 기반 애니메이션**이라는 구체적인 기능을 가리킵니다.

```cpp
class UWidgetAnimation : public UMovieSceneSequence
```

**`UMovieSceneSequence`** — 레벨 시퀀스와 같은 뿌리입니다. 즉 "시퀀서를 위젯 프로퍼티에 붙인 것"입니다.
애니메이션 대상은 Render Transform(위치·회전·스케일), Opacity, Color and Opacity, Visibility 등
키프레임 가능한 프로퍼티와 사운드·이벤트 트랙입니다.

```cpp
FWidgetAnimationHandle PlayAnimation(UWidgetAnimation* InAnimation,
    float StartAtTime = 0.f, int32 NumLoopsToPlay = 1,
    EUMGSequencePlayMode::Type PlayMode = EUMGSequencePlayMode::Forward,
    float PlaybackSpeed = 1.f, bool bRestoreState = false);

PlayAnimationForward / PlayAnimationReverse / PauseAnimation / StopAnimation
IsAnimationPlaying / SetPlaybackSpeed / BindToAnimationFinished
StopAnimationsAndLatentActions();   // 화면에서 내릴 때
```

C++에서 참조하려면 전용 메타를 씁니다.

```cpp
UPROPERTY(Transient, meta = (BindWidgetAnim))   // ⚠️ Transient 필수
TObjectPtr<UWidgetAnimation> FadeIn;
```

`bRestoreState = true`로 재생하면 종료 후 프로퍼티가 원래 값으로 복원됩니다.

⚠️ **위젯의 내장 동작과 혼동하지 마십시오.** 스크롤(`SScrollBox` + `FInertialScrollManager`),
슬라이더 드래그, 콤보박스 펼침 등은 **위젯 구현이 코드로 수행하는 동작**이며 `UWidgetAnimation`과 무관합니다.
구분 기준은 "움직이는가"가 아니라 **"누가 그 움직임을 저작했는가"**입니다.

⚠️ 재생 중인 애니메이션은 매 프레임 프로퍼티를 갱신하므로 그 위젯은 매 프레임 무효화됩니다.
이는 정상이지만, **끝나지 않는 루프**(`NumLoopsToPlay = 0`)를 화면 밖에서 방치하면 계속 비용을 냅니다.
위젯을 숨길 때 `StopAnimation`도 함께 호출하십시오.

### 5.6 드래그 앤 드롭

드래그는 **여러 프레임에 걸친 상태**이므로 한 프레임짜리 `FReply`만으로는 표현할 수 없습니다.
그래서 Slate는 **`FDragDropOperation`이라는 별도 객체**를 두고 `FSlateApplication`이 들고 있게 합니다.

```
[드래그 소스] ──만듦──▶ [DragDropOperation] ◀──질의── [드롭 타겟]
```

**소스와 타겟이 서로를 모르는 것**이 이 설계의 이점입니다. 둘 다 오퍼레이션만 압니다.

```
① OnMouseButtonDown → FReply::Handled().DetectDrag(Widget, EKeys::LeftMouseButton)
                        ↑ 임계값만큼 움직이면 알려달라고 "예약"
② OnDragDetected    → 오퍼레이션 생성 (UMG는 out 파라미터로 반환)
③ 드래그 중          → 타겟들의 OnDragEnter / OnDragOver / OnDragLeave
④ 놓음              → 타겟의 OnDrop이 true면 성공, 아무도 안 받으면 DragCancelled
```

UMG 쪽 오퍼레이션의 핵심 필드:

| 필드 | 역할 |
|:---|:---|
| `Payload` | **끌고 다니는 데이터**(아무 `UObject`). 인벤토리라면 아이템 인스턴스 |
| `DefaultDragVisual` | 커서를 따라다닐 위젯 |
| `Pivot` (`EDragPivot`) | 시각물이 커서 기준 어디에 붙는가 (`MouseDown` 등 10종) |
| `Tag` | 종류 구분용 문자열 |

소스 훅은 `NativeOnMouseButtonDown`(→ `DetectDragIfPressed`)과 `NativeOnDragDetected` 둘,
타겟 훅은 `NativeOnDragEnter/Leave/Over/Drop` 넷입니다.
`NativeOnDrop`의 **반환값이 곧 "받았다/안 받았다"**이며, `false`면 버블링으로 부모에게 기회가 넘어갑니다.

⚠️ 주의점:

- **Drag Visual은 새 위젯을 만들 것.** 화면에 있는 위젯을 그대로 넣으면 원래 자리에서 뽑혀 나갑니다.
- **드롭 타겟의 Visibility가 `Visible`이어야** 드래그 이벤트가 옵니다 (§3.7).
- `OnDrop`에는 **모든 종류의 드래그**가 들어옵니다. 타입·`Tag` 확인 필수.
- **`OnDragOver`는 매 프레임 호출**됩니다. 무거운 검사는 `OnDragEnter`에서 한 번만.
- **게임패드에는 드래그 앤 드롭이 없습니다.** 포인터 기반 기능이라 별도 조작 경로가 필요합니다.
- 멀티플레이에서는 **순수 로컬 UI 동작**입니다. `OnDrop`에서 데이터를 직접 바꾸지 말고
  서버 RPC로 의도를 보내고 복제 결과로 UI를 갱신하십시오.

---

## 6. 성능 — Invalidation과 Retainer

### 6.0 문제 정의

기본 Slate는 **매 프레임 전체 위젯 트리를 순회**합니다(Prepass → Paint).
그런데 대부분의 프레임에서 UI는 아무것도 변하지 않습니다. 위젯 수백 개가 떠 있고 아무 입력이 없어도
매 프레임 크기를 다시 재고 드로우 엘리먼트를 다시 쌓습니다. 이것이 UI가 **게임 스레드 시간을 먹는** 주된 이유입니다.

두 가지 해법이 있고, **캐시하는 대상이 다릅니다.**

| | Invalidation | Retainer |
|:---|:---|:---|
| 캐시 대상 | **레이아웃 결과 + 드로우 엘리먼트** | **렌더 타겟(픽셀)** |
| 절약 | CPU(게임 스레드) | CPU + 리드로우 |
| 비용 | 메모리, 무효화 누락 리스크 | **VRAM**, 해상도 종속 |
| 성격 | "안 바뀐 건 다시 계산하지 마" | "한 장 찍어두고 그거 붙여" |

### 6.1 Invalidation — 무효화 사유를 구분하라 ★

각 위젯이 자기가 더러운지(dirty) 표시하고, 더러운 것만 다시 처리합니다.
`Invalidate()`를 호출하는 주체는 보통 세터입니다.

```cpp
enum class EInvalidateWidgetReason : uint8
{
    None                  = 0,
    Layout                = 1 << 0,  // 희망 크기가 변함 — 비쌈
    Paint                 = 1 << 1,  // 그림만 변함 — 쌈
    Volatility            = 1 << 2,
    ChildOrder            = 1 << 3,  // 자식 추가/제거 (Prepass·Layout 포함)
    RenderTransform       = 1 << 4,
    Visibility            = 1 << 5,  // (Layout 포함)
    AttributeRegistration = 1 << 6,
    Prepass               = 1 << 7,  // 자식 희망 크기 전체 재계산 (Layout 포함)
};
```

엔진 주석이 직접 경고합니다:

> *"Use Layout invalidation if your widget needs to change desired size. **This is an expensive invalidation**
> so do not use if all you need to do is redraw a widget."*

**왜 `Layout`이 비싼가**: 내 희망 크기가 변하면 부모의 배치가 달라지고, 형제들의 자리도 달라집니다.
**위로 전파**됩니다. 반면 `Paint`는 나만 다시 그리면 끝입니다.

> ✅ 커스텀 위젯의 세터를 작성할 때, **색·투명도처럼 크기에 영향이 없는 변경에는 `Paint`**를 쓰십시오.
> 습관적으로 `Layout`을 걸면 조용히 비용을 냅니다.

### 6.2 Volatile — 캐시를 포기한 위젯

**Volatile 위젯은 무효화 여부와 무관하게 매 프레임 다시 그려집니다.**

```cpp
inline bool IsVolatile() const { return bCachedVolatile; }
inline bool IsVolatileIndirectly() const { return bInheritedVolatility; }  // 부모가 volatile
virtual bool ComputeVolatility() const { return false; }
inline void ForceVolatile(bool bForce);
// bCachedVolatile = bForceVolatile || ComputeVolatility();
```

volatile이 되는 경우:

- **바인딩된 어트리뷰트**를 가진 위젯 (매 프레임 값이 바뀔 수 있으므로 캐시 불가)
- `ForceVolatile(true)`
- **부모가 volatile이면 자식도 간접 volatile** — 영향 범위가 넓습니다

⚠️ **UMG의 속성 바인딩이 여기에 직결됩니다.** 디자이너에서 Text 옆 `Bind` 드롭다운으로 함수를 연결하면:

```cpp
#define PROPERTY_BINDING(ReturnType, MemberName)          \
    ( MemberName##Delegate.IsBound() && !IsDesignTime() ) \
    ? BIND_UOBJECT_ATTRIBUTE(ReturnType, K2_Gate_##MemberName) \
    : TAttribute<ReturnType>(MemberName)
```

델리게이트가 붙으면 `TAttribute`가 바인딩 모드가 되어 **매 프레임 평가**됩니다.
**바인딩 하나가 그 위젯의 캐싱을 무력화**합니다.

> ✅ **권장**: 바인딩 대신 **값이 바뀌는 순간 세터를 호출**하는 구조로 가십시오.
> 그러면 변화가 없는 프레임에는 아무 일도 일어나지 않습니다.

배선이 여러 위젯에 흩어져 관리가 어렵다면 **ModelViewViewModel(MVVM) 플러그인**이 그 구조를 선언적으로 정리해 줍니다.
바인딩이 **컴파일 타임에 굳고**, `FieldNotify` 프로퍼티의 세터 매크로가
**값이 실제로 변할 때만** 통지를 발송합니다. (상세: [EngineAnalysis_MVVM.md](EngineAnalysis_MVVM.md))

```cpp
UPROPERTY(BlueprintReadOnly, FieldNotify, meta = (AllowPrivateAccess = "true"))
float HealthPercent = 1.f;

void SetHealthPercent(float In) { UE_MVVM_SET_PROPERTY_VALUE(HealthPercent, In); }
// 매크로 내부: 값이 같으면 조기 종료 → 통지 없음 → 바인딩도 안 돎
```

즉 **매 프레임 세터를 호출해도 값이 그대로면 위젯을 한 번도 건드리지 않습니다.**
다만 `UListView::ListItems` 같은 런타임 쓰기 불가 프로퍼티에는 바인딩할 수 없어
**컬렉션은 여전히 C++가 직접 채워야** 합니다.

| | 속성 바인딩 | 이벤트 → 세터 | MVVM |
|:---|:---|:---|:---|
| 실행 시점 | 매 프레임 | 값 변경 시 | 값 변경 시 |
| 캐싱 | **무력화** | 유지 | 유지 |
| 배선 관리 | 간단 | **코드에 흩어짐** | 디자이너에서 한눈에 |

### 6.3 매 프레임 일하게 만드는 플래그

```cpp
enum class EWidgetUpdateFlags : uint8
{
    NeedsTick              = 1 << 2,  // Tick 함수가 있음
    NeedsActiveTimerUpdate = 1 << 3,  // 액티브 타이머
    NeedsRepaint           = 1 << 4,  // 더러움
    NeedsVolatilePaint     = 1 << 6,  // volatile이라 매번 그림
    NeedsVolatilePrepass   = 1 << 7,  // volatile이라 매번 크기 계산
};
```

UI 성능을 진단할 때는 **이 플래그가 켜진 위젯이 몇 개인지**를 봅니다.
`NeedsTick`과 `NeedsVolatilePaint`가 특히 비쌉니다.

### 6.4 Invalidation Root

무효화 캐시는 **Invalidation Root**(`FSlateInvalidationRoot`) 안에서만 동작합니다.
루트가 하위 트리의 위젯 목록·정렬 순서·드로우 엘리먼트를 관리합니다. 루트를 만드는 방법이 둘입니다.

**① 부분 적용 — `SInvalidationPanel` / UMG의 `Invalidation Box`**

```cpp
UMG_API bool GetCanCache() const;
UMG_API void SetCanCache(bool CanCache);
```

**② 전역 적용 — `Slate.EnableGlobalInvalidation`**

```cpp
bool GSlateEnableGlobalInvalidation = false;   // 기본 꺼짐
```

⚠️ **Invalidation Box를 아무데나 두면 오히려 느려집니다.** 루트 자체가 관리 비용(위젯 인덱스·정렬 순서 유지)을
갖기 때문에, 자주 변하는 내용을 감싸면 캐시 이득 없이 관리 비용만 냅니다.
원칙은 **"거의 안 변하고 위젯 수가 많은 덩어리"**입니다.

⚠️ **Global Invalidation을 켜는 것은 프로젝트 차원의 결정입니다.** 켜는 순간 그동안 숨어 있던
**무효화 누락 버그가 드러납니다**(바뀌어야 하는데 화면이 안 바뀌는 UI). 프로젝트 초기에 켜고
충분히 테스트하는 편이 안전하며, 후반에 켜면 회귀 검증 부담이 큽니다.

### 6.5 Retainer — 픽셀 캐싱

서브트리를 **렌더 타겟에 한 번 그려놓고** 이후 프레임엔 그 텍스처만 붙입니다.
위젯 수백 개가 **쿼드 1장**이 됩니다. UMG에서는 `Retainer Box`입니다.

```cpp
bool bRetainRender;         // 리테이너 사용
bool RenderOnInvalidation;  // 무효화될 때만 다시 그림
bool RenderOnPhase;         // 위상에 따라 주기적으로
int32 Phase;
int32 PhaseCount;
void SetRenderingPhase(int32 RenderPhase, int32 TotalPhases);
```

`Phase`/`PhaseCount`는 엔진 주석 그대로입니다:

> *"If the Phase is 0, and the PhaseCount is 1, the widget will be drawn fresh every frame.
> If the Phase were 0, and the PhaseCount were 2, this retainer would draw a fresh frame every other frame."*

> ✅ **실전 기법**: 리테이너가 여럿이면 `Phase`를 0, 1, 2…로 흩뿌려 갱신 부하를 프레임마다 분산시킵니다.
> 같은 프레임에 몰려 스파이크가 나는 것을 막습니다.
> 다만 대개는 `RenderOnInvalidation = true`가 가장 무난합니다 — 안 변하면 안 그립니다.

**덤: 머티리얼 이펙트.** 결과가 텍스처이므로 머티리얼을 씌울 수 있습니다.

```cpp
void SetEffectMaterial(UMaterialInterface* EffectMaterial);
void SetTextureParameter(FName TextureParameter);
```

UI 전체에 블러·색수차·흑백 같은 연출을 걸 때 씁니다.

⚠️ **리테이너는 공짜가 아닙니다.**

- **VRAM**: 서브트리 크기만큼 렌더 타겟이 필요합니다. 화면 전체를 감싸면 풀스크린 텍스처 하나입니다.
- **해상도 종속**: 렌더 타겟 해상도로 굳으므로 스케일업하면 뭉개집니다.
- **중첩 금지**: 리테이너 안의 리테이너는 렌더 타겟을 겹겹이 쌓습니다.
- 매 프레임 갱신되면 **이득 0에 렌더 타겟 비용만** 남습니다.

### 6.6 최적화 순서

**캐싱은 마지막 수단입니다.** 순서를 지키면 대개 캐싱까지 가기 전에 해결됩니다.

```
UI가 느리다
   │
   1) 속성 바인딩을 걷어내고 이벤트 기반(세터·MVVM)으로   ← 효과가 가장 큼
   2) NativeTick 제거 — 주기적 갱신은 타이머로
   3) 안 보이는 것은 Collapsed로 (Hidden은 레이아웃 계산이 계속 돎)
   4) 그래도 무겁다 →
        ├─ 위젯 많고 대부분 정적        → Invalidation
        └─ 서브트리 통째로 정적 + 이펙트 → Retainer
```

> 예: 초 단위로만 바뀌는 표시(잔여 시간 등)를 매 프레임 `Tick`으로 갱신하면
> 프레임마다 문자열을 새로 만듭니다. **1초 주기 타이머**로 바꾸는 것만으로 (1)과 (2)를 동시에 해결합니다.

---

## 7. 작업 시 유의점 (Pitfalls) 요약

| 증상 | 실제 원인 |
|:---|:---|
| 크기가 이상하다 | **부모 슬롯**의 크기 규칙·정렬 (자식 속성이 아님, §3.2 / §4) |
| 이미지가 한쪽으로만 늘어나 납작하다 | 슬롯이 그 축만 `Fill` + 브러시 `ImageSize`가 작음 (§3.4) |
| 값을 바꿨는데 런타임에 반영 안 됨 | 커스텀 위젯 세터의 `SynchronizeProperties` 누락 (§5.1) |
| 디자이너에서만 오동작/크래시 | `PreConstruct`에서 게임 상태 접근 (§5.3) |
| 위젯이 재사용될 때 깨진다 | 매번 필요한 구성을 1회 훅에 둠 (§5.3) |
| `BindWidget`이 null | BP로 안 감쌈 / 이름 불일치 / `Is Variable` 꺼짐 (§5.4) |
| 껐는데 레이아웃이 들썩인다 | `Collapsed` 대신 `Hidden`이 필요 (§3.7) |
| 입력이 안 먹는다 | Visibility가 `HitTestInvisible`, 또는 상위가 `Handled`로 소비 (§3.5 / §3.7) |
| 부모를 바꿨더니 배치가 다 풀렸다 | 슬롯 타입이 바뀌어 설정이 유실 (§4.5) |
| UI가 게임 스레드를 먹는다 | 속성 바인딩·Tick으로 인한 매 프레임 작업 (§6.2 / §6.6) |
| 캐싱을 켰는데 더 느려졌다 | 자주 변하는 곳에 Invalidation Root를 둠 (§6.4) |
| 캐싱을 켰더니 화면이 안 바뀐다 | 무효화 누락 — 세터가 `Invalidate`를 호출하지 않음 (§6.1) |
| 에디터 조작이 이상해졌다 | 전역 `FNavigationConfig`를 원복하지 않음 (§3.6) |
| 드롭이 안 받아진다 | 타겟 Visibility가 `HitTestInvisible`, 또는 `OnDrop`이 `false` 반환 (§5.6) |
| 드래그하면 원본 위젯이 뽑혀 나간다 | `DefaultDragVisual`에 화면의 기존 위젯을 넣음 (§5.6) |
| 위젯을 숨겼는데도 계속 무겁다 | 루프 애니메이션이 재생 중 — `StopAnimation` 누락 (§5.5) |

---

## 8. 측정 도구

추측하지 말고 재야 합니다.

| 도구 | 용도 |
|:---|:---|
| `WidgetReflector` (에디터 Tools 메뉴) | 실행 중 위젯 트리·계층·크기·볼라틸 여부를 집어서 확인. **입문에 가장 유용** |
| `stat Slate` | Slate 전반 통계 (Paint/Prepass/Tick 시간) |
| `stat SlateVerbose` | 위젯 단위 상세 |
| `SlateDebugger.Start` / `.Stop` | 무효화 이벤트 실시간 추적 |
| `Slate.InvalidationDebugging 1` | 무효화되는 위젯을 화면에 표시 |
| Unreal Insights | 게임 스레드에서 Slate가 차지하는 실제 비중 |

---

## 9. 정리

- **Slate가 진짜 UI다.** UMG는 UObject 껍데기이고, 값을 Slate로 밀어넣는 다리를 건너야 반영된다.
- **레이아웃은 2패스.** 자식이 희망을 올리고 부모가 실제를 내린다. 크기 문제는 **부모 슬롯부터** 본다.
- **슬롯은 부모의 소유물**이며 부모 종류마다 타입이 다르다. 위젯 속성과 혼동하지 않는다.
- **수명주기 훅의 호출 횟수가 다르다.** 1회 훅과 매번 훅을 구분하지 않으면 재사용 시 깨진다.
- **성능의 8할은 "매 프레임 뭘 하고 있는가"**다. 속성 바인딩과 Tick을 먼저 걷어내고,
  캐싱(Invalidation/Retainer)은 그 다음에 고려한다.
