# 커스텀 Slate 위젯 제작 가이드

요구사항에 맞는 Slate 위젯을 직접 구현해 **재사용 가능한 모듈**로 만드는 절차.
완성된 위젯은 C++에서 `SNew`로, 디자이너에서는 UMG 팔레트로 쓸 수 있다.

---

## 사전 지식

Slate는 실제 레이아웃·렌더링·입력을 담당하는 C++ 위젯 시스템(`S` 접두사)이고, UMG는 그것을 UObject로 감싼 층.
커스텀 위젯은 **Slate 쪽 본체 + UMG 쪽 래퍼** 두 벌을 만드는 것이 기본 구성.

> 레이아웃 2패스·슬롯·무효화 등 기반 개념: [EngineAnalysis_SlateUMG.md](EngineAnalysis_SlateUMG.md)

**직접 만들기 전에 확인할 것** — 기존 위젯 조합으로 해결되면 그게 낫다.
`SBorder`+`SOverlay`+`SBox` 조합으로 되는 것을 새로 그리는 건 낭비다.
직접 만들 가치가 있는 경우는 다음 정도다.

- 기존 위젯으로 **그릴 수 없는 시각 표현** (방사형 게이지, 차트, 커스텀 그래프)
- 기존 패널에 없는 **배치 규칙** (방사형 배치, 나선 배치)
- 성능상 **드로우 콜을 직접 통제**해야 하는 경우

---

## 절차

### Step 1 — 베이스 클래스 선택

| 베이스 | 자식 | 구현할 것 | 쓰는 경우 |
|:---|:---|:---|:---|
| `SLeafWidget` | 없음 | `ComputeDesiredSize`, `OnPaint` | 직접 그리는 시각 요소 |
| `SCompoundWidget` | 1개(내부 조합) | `Construct`에서 `ChildSlot[...]` | 기존 위젯 조합 |
| `SPanel` | N개 | `OnArrangeChildren`, `GetChildren`, `ComputeDesiredSize` | 새 배치 알고리즘 |

`SPanel`은 슬롯 타입까지 직접 정의해야 하므로 난이도가 크게 올라간다. 정말 새 배치 규칙이 필요한지 먼저 따진다.

### Step 2 — 모듈 배치

재사용이 목적이면 게임플레이 모듈이 아니라 **전용 Runtime 모듈 또는 플러그인**에 둔다.

```csharp
// MyUI.Build.cs
PublicDependencyModuleNames.AddRange(new[] {
    "Core", "CoreUObject", "Engine",
    "Slate", "SlateCore", "InputCore",
    "UMG",              // UMG 래퍼를 함께 제공할 때
});
```

클래스에는 모듈 export 매크로를 붙인다.

```cpp
class MYUI_API SSegmentedBar : public SLeafWidget
```

`SLATE_DECLARE_WIDGET`(Step 4)을 쓰면서 export가 필요하면 API 인자를 받는 변형을 쓴다.

```cpp
SLATE_DECLARE_WIDGET_API(SSegmentedBar, SLeafWidget, MYUI_API)
```

### Step 3 — 스타일 구조체 분리

브러시·색·간격을 위젯에 하드코딩하면 다른 프로젝트에서 못 쓴다. `FSlateWidgetStyle` 파생으로 뺀다.

```cpp
USTRUCT(BlueprintType)
struct MYUI_API FSegmentedBarStyle : public FSlateWidgetStyle
{
    GENERATED_BODY()

    static const FName TypeName;
    virtual const FName GetTypeName() const override { return TypeName; }
    static const FSegmentedBarStyle& GetDefault();

    /** 브러시를 참조로 노출 — 누락 시 쿠킹·GC에서 문제가 생긴다 */
    virtual void GetResources(TArray<const FSlateBrush*>& OutBrushes) const override
    {
        OutBrushes.Add(&FillBrush);
        OutBrushes.Add(&EmptyBrush);
    }

    UPROPERTY(EditAnywhere, Category = Appearance)
    FSlateBrush FillBrush;

    UPROPERTY(EditAnywhere, Category = Appearance)
    FSlateBrush EmptyBrush;

    UPROPERTY(EditAnywhere, Category = Appearance)
    float Spacing = 2.f;
};
```

> `GetResources` 구현을 빠뜨리면 브러시가 참조되지 않아 패키징에서 텍스처가 빠질 수 있다.

### Step 4 — Slate 위젯 선언

#### 4-1. 인자 매크로

```cpp
SLATE_BEGIN_ARGS(SSegmentedBar)
    : _Percent(0.f)
    , _SegmentCount(10)
    , _Style(&FSegmentedBarStyle::GetDefault())
    {}

    SLATE_ATTRIBUTE(float, Percent)                    // 런타임에 계속 변할 수 있음
    SLATE_ARGUMENT(int32, SegmentCount)                // 생성 시 고정
    SLATE_STYLE_ARGUMENT(FSegmentedBarStyle, Style)    // 스타일 포인터
    SLATE_EVENT(FSimpleDelegate, OnFilled)             // 콜백
SLATE_END_ARGS()
```

| 매크로 | 의미 | 판단 기준 |
|:---|:---|:---|
| `SLATE_ARGUMENT` | 생성 후 안 바뀌는 값 | 개수·방향·모드 |
| `SLATE_ATTRIBUTE` | 값 **또는** 값을 돌려주는 델리게이트 | 진행률·색·텍스트 |
| `SLATE_EVENT` | 콜백 델리게이트 | OnClicked, OnValueChanged |
| `SLATE_STYLE_ARGUMENT` | 스타일 구조체 포인터 | 룩 전체 |
| `SLATE_DEFAULT_SLOT` | 자식 슬롯 | 자식을 받는 위젯 |

⚠️ **`ATTRIBUTE`를 남발하지 않는다.** 바인딩된 어트리뷰트는 매 프레임 평가되고 위젯을 volatile로 만든다.
"정말 매 프레임 달라질 수 있는 것"만 `ATTRIBUTE`로 열고, 나머지는 `ARGUMENT` + 세터로 제공한다.

인자는 `_` 접두사가 붙으므로 `InArgs._Percent`로 접근한다.

#### 4-2. TSlateAttribute와 무효화 사유 등록

UE5 권장 방식. 값이 캐시되고, 변경 시 **지정한 사유로 자동 무효화**된다.

```cpp
// ── SSegmentedBar.h ──
class MYUI_API SSegmentedBar : public SLeafWidget
{
    SLATE_DECLARE_WIDGET_API(SSegmentedBar, SLeafWidget, MYUI_API)

public:
    SLATE_BEGIN_ARGS(SSegmentedBar) /* ... */ SLATE_END_ARGS()

    SSegmentedBar()
        : Percent(*this, 0.f)     // ★ 반드시 this 포인터
    {
        SetCanTick(false);        // ★ Tick 불필요 시 반드시 끈다 (기본이 켜짐)
    }

    void Construct(const FArguments& InArgs);
    void SetPercent(float In) { Percent.Set(*this, In); }

private:
    TSlateAttribute<float> Percent;
    int32 SegmentCount = 10;
    const FSegmentedBarStyle* Style = nullptr;
    FSimpleDelegate OnFilled;
};

// ── SSegmentedBar.cpp ──
SLATE_IMPLEMENT_WIDGET(SSegmentedBar)
void SSegmentedBar::PrivateRegisterAttributes(FSlateAttributeInitializer& Init)
{
    SLATE_ADD_MEMBER_ATTRIBUTE_DEFINITION(Init, Percent, EInvalidateWidgetReason::Paint);
    //                                                    ↑ 크기가 아니라 그림만 변함
}

void SSegmentedBar::Construct(const FArguments& InArgs)
{
    Percent.Assign(*this, InArgs._Percent);   // ★ 여기서도 this
    SegmentCount = FMath::Max(1, InArgs._SegmentCount);
    Style        = InArgs._Style;
    OnFilled     = InArgs._OnFilled;
}
```

⚠️ **무효화 사유 선택이 성능을 가른다.** 색·진행률처럼 크기에 영향이 없으면 `Paint`.
희망 크기가 달라지는 변경만 `Layout`. 습관적으로 `Layout`을 쓰면 부모까지 재계산이 전파된다.

> 템플릿 인자로 사유를 직접 주는 방식도 있다: `TSlateAttribute<float, EInvalidationReason::Paint> Percent;`
> 이 경우 `SLATE_DECLARE_WIDGET`·`PrivateRegisterAttributes` 없이 쓴다.
> 엔진 주석 기준 — 템플릿 방식은 실수가 적고, 등록 방식은 디버깅·재정의에 유리하다.

### Step 5 — 레이아웃과 그리기 구현

#### 5-1. ComputeDesiredSize

```cpp
virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override
{
    const float W = SegmentCount * MinSegmentWidth + (SegmentCount - 1) * Style->Spacing;
    return FVector2D(W, MinBarHeight);
}
```

부모에게 올려보내는 희망 크기. **레이아웃마다 호출되므로 무거운 계산 금지.**
텍스트 측정처럼 비싼 것은 캐시한다.

#### 5-2. OnPaint

```cpp
virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
                      const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
                      int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
{
    const float     Pct    = FMath::Clamp(Percent.Get(), 0.f, 1.f);   // 캐시된 값
    const FVector2D Size   = AllottedGeometry.GetLocalSize();
    const int32     Filled = FMath::RoundToInt(Pct * SegmentCount);
    const float     SegW   = (Size.X - Style->Spacing * (SegmentCount - 1)) / SegmentCount;

    for (int32 i = 0; i < SegmentCount; ++i)
    {
        const FSlateBrush* Brush = (i < Filled) ? &Style->FillBrush : &Style->EmptyBrush;
        const FVector2D    Offset(i * (SegW + Style->Spacing), 0.f);

        FSlateDrawElement::MakeBox(
            OutDrawElements,
            LayerId,
            AllottedGeometry.ToPaintGeometry(FVector2f(SegW, Size.Y), FSlateLayoutTransform(Offset)),
            Brush,
            ESlateDrawEffect::None,
            Brush->GetTint(InWidgetStyle) * InWidgetStyle.GetColorAndOpacityTint());
            //                              ↑ ★ 부모의 색·투명도 전파
    }

    return LayerId;
}
```

지켜야 할 것:

- **`const` 함수다.** 여기서 상태를 바꾸지 않는다.
- 자식 사각형은 `AllottedGeometry.ToPaintGeometry(LocalSize, FSlateLayoutTransform(Offset))`로 만든다.
- **`InWidgetStyle.GetColorAndOpacityTint()`를 곱한다.** 빠뜨리면 부모를 페이드아웃해도 이 위젯만 남는다.
- 자식을 그렸다면 **증가한 `LayerId`를 반환**한다. 안 올리면 자식이 부모에 가려진다.

그리기 함수: `MakeBox`(브러시), `MakeText`, `MakeLines`, `MakeRotatedBox`, `MakeCubicBezierSpline`.

#### 5-3. OnArrangeChildren — `SPanel`일 때만

```cpp
virtual void OnArrangeChildren(const FGeometry& AllottedGeometry,
                               FArrangedChildren& ArrangedChildren) const override
{
    for (int32 i = 0; i < Children.Num(); ++i)
        ArrangedChildren.AddWidget(AllottedGeometry.MakeChild(Children[i].GetWidget(), Offset_i, Size_i));
}

virtual FChildren* GetChildren() override { return &Children; }
```

### Step 6 — 입력 처리 (필요 시)

```cpp
SSegmentedBar()
{
    SetCanTick(false);
    bCanSupportFocus = true;     // 포커스를 받을 위젯이면
}

virtual FReply OnMouseButtonDown(const FGeometry& G, const FPointerEvent& E) override
{
    if (E.GetEffectingButton() != EKeys::LeftMouseButton)
        return FReply::Unhandled();      // 안 쓰면 부모로 전파

    OnFilled.ExecuteIfBound();
    return FReply::Handled();
}
```

### Step 7 — UMG 래퍼 제공

C++ 전용으로 두면 디자이너가 쓸 수 없다. `UWidget` 래퍼를 씌우면 팔레트에 등장한다.

```cpp
UCLASS()
class MYUI_API USegmentedBar : public UWidget
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
    float Percent = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
    int32 SegmentCount = 10;

    UPROPERTY(EditAnywhere, Category = Style, meta = (ShowOnlyInnerProperties))
    FSegmentedBarStyle Style;

    UFUNCTION(BlueprintCallable, Category = Appearance)
    void SetPercent(float In)
    {
        Percent = In;
        if (MyBar.IsValid())              // ★ Slate 쪽에 반영
            MyBar->SetPercent(In);
    }

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override
    {
        MyBar = SNew(SSegmentedBar)
                .Style(&Style)
                .SegmentCount(SegmentCount)
                .Percent(Percent);
        return MyBar.ToSharedRef();
    }

    virtual void SynchronizeProperties() override
    {
        Super::SynchronizeProperties();
        if (MyBar.IsValid())
            MyBar->SetPercent(Percent);   // 에디터 편집·재컴파일 시 반영
    }

    virtual void ReleaseSlateResources(bool bReleaseChildren) override
    {
        Super::ReleaseSlateResources(bReleaseChildren);
        MyBar.Reset();                    // ★ 누락 시 Slate 위젯 릭
    }

#if WITH_EDITOR
    virtual const FText GetPaletteCategory() override
    {
        return NSLOCTEXT("MyUI", "PaletteCategory", "My UI");
    }
#endif

private:
    TSharedPtr<SSegmentedBar> MyBar;
};
```

이 네 함수가 UMG 래퍼의 정형이다.

| 함수 | 빠뜨리면 |
|:---|:---|
| `RebuildWidget` | 위젯이 아예 안 만들어짐 |
| `SynchronizeProperties` | 에디터에서 값을 바꿔도 반영 안 됨 |
| `ReleaseSlateResources` | Slate 위젯 릭 |
| `GetPaletteCategory` | 팔레트에서 찾기 어려움 (선택) |

### Step 8 — 검증

1. **`WidgetReflector`** (에디터 Tools 메뉴)로 실제 크기·계층·볼라틸 여부 확인
2. 부모를 반투명하게 만들었을 때 **함께 투명해지는지** (틴트 전파 확인)
3. **무효화 사유 확인** — 값을 여러 번 갱신하며 의도한 사유로만 무효화되는지
   ```
   SlateDebugger.Invalidate.bLogInvalidatedWidget 1
   SlateDebugger.Invalidate.bShowLegend 1
   SlateDebugger.Invalidate.Start          ← 확인 후 .Stop
   ```
   화면 테두리 색이 사유다: **Paint=노랑 / Layout=자홍 / Prepass=주황 / Volatility=회색 /
   ChildOrder=청록 / RenderTransform=검정 / Visibility=흰색**.
   ⚠️ `Slate.InvalidationDebugging`은 UE 5.8에서 **deprecated**이며, 포워딩 대상 이름이 어긋나 동작하지 않는다.
   ⚠️ **디버거는 무효화 루트(fast path) 안에 있는 위젯만 기록한다** — `ConsoleSlateDebuggerInvalidate.cpp:465`가
   `GetProxyHandle().IsValid()`가 아니면 입구에서 버린다 (2026-08-17 실측). 뷰포트에 그냥 올린 UMG는
   무효화 루트 밖이라 **로그에도 화면에도 아무것도 안 나온다.** 확인하려면 `Slate.EnableGlobalInvalidation 1`을
   먼저 켜거나 대상 위젯을 Invalidation Box 안에 넣어야 한다. `SlateDebugger.Update`도 같은 제약을 받는다.
4. **Tick 여부 확인** — 필요할 때만 Tick하는지 (`SetCanTick` 제어가 실제로 먹는지)
   ```
   SlateDebugger.Update.OnlyProjectContent 1
   SlateDebugger.Update.SetWidgetUpdateFlagsFilter Tick
   SlateDebugger.Update.ToggleWidgetNameList
   SlateDebugger.Update.Start              ← 확인 후 .Stop
   ```
   유휴 상태에서 위젯 이름이 **목록에 안 뜨면** Tick이 꺼진 것이다.
   `stat Slate`는 전체 집계라 위젯별 Tick 판별에는 쓸 수 없다.

---

## 재사용성 체크리스트

| 항목 | 확인 |
|:---|:---|
| 게임 타입(`UMyItem*` 등)에 의존하지 않는가 | 어트리뷰트로 받고 델리게이트로 알린다 |
| `GetWorld()`·서브시스템을 직접 찾지 않는가 | 값은 밖에서 주입받는다 |
| 색·크기가 하드코딩돼 있지 않은가 | `FSlateWidgetStyle`로 분리 |
| `GetResources`를 구현했는가 | 브러시 참조 노출 |
| 모듈 export 매크로가 붙어 있는가 | `MYUI_API` |
| `SetCanTick(false)` 했는가 | 불필요한 매 프레임 비용 제거 |
| 무효화 사유가 최소인가 | 색 변경에 `Layout`을 쓰고 있지 않은가 |
| UMG 래퍼가 있는가 | 디자이너 사용 가능 여부 |

**핵심 원칙: 위젯은 "어떻게 보일지"만 알고, "무슨 데이터인지"는 몰라야 한다.**

> **적용 사례:** `SLNPRadialCooldown`(방사형 쿨다운 파이, `Source/LNPUI/`) —
> 모듈 분리·`MakeCustomVerts` 부채꼴·Tick 자체 제어의 실제 구현은
> [TechDesign_HUD.md](TechDesign_HUD.md) §9 참조.

---

## 흔한 실수

| 실수 | 결과 |
|:---|:---|
| `TSlateAttribute` 생성자에 `this` 미전달 | 런타임 체크 실패 (엔진이 "예외 없이 this" 명시) |
| 무효화 사유를 `Layout`으로 남발 | 부모까지 재계산 전파 |
| `SetCanTick(false)` 누락 | 매 프레임 Tick |
| `OnPaint`에서 상태 변경 | `const` 위반, 예측 불가 동작 |
| `InWidgetStyle` 틴트 미적용 | 부모 페이드가 안 먹음 |
| 반환 `LayerId` 미증가 | 자식이 가려짐 |
| `ReleaseSlateResources`에서 `Reset()` 누락 | Slate 위젯 릭 |
| `ComputeDesiredSize`에서 무거운 계산 | 레이아웃마다 비용 |
| 스타일 하드코딩 | 재사용 불가 |
