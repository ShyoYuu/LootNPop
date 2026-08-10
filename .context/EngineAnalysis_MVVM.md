# Engine Analysis - Unreal Engine 5.8 ModelViewViewModel (MVVM)

## 1. 개요 (Overview)

`ModelViewViewModel`은 널리 알려진 **MVVM 패턴을 언리얼 UI에 구현한 플러그인**입니다.
게임 데이터와 위젯 사이에 **ViewModel**이라는 중간층을 두고, 그 사이 배선을 **에디터에서 선언적으로** 긋습니다.

언리얼 구현의 특징 두 가지:

1. **컴파일 타임 바인딩** — 위젯 블루프린트를 컴파일할 때 바인딩이 코드로 굳습니다.
   런타임 리플렉션 조회가 아니라 빠릅니다.
2. **FieldNotification** — 변경 통지 메커니즘이 MVVM 전용이 아니라
   **엔진 공용 인터페이스**(`INotifyFieldValueChanged`, FieldNotification 모듈)로 분리돼 있습니다.
   따라서 ViewModel이 아닌 임의의 `UObject`도 통지 소스가 될 수 있습니다.

WPF/Xamarin의 MVVM을 안다면 `INotifyPropertyChanged` ↔ `INotifyFieldValueChanged`가 정확히 대응합니다.

### 해결하려는 문제

UI에 데이터를 꽂는 기존 방법은 둘이었고, 각각 결함이 있었습니다.

| 방법 | 문제 |
|:---|:---|
| **UMG 속성 바인딩** (`Bind ▾` 드롭다운) | **매 프레임 평가**되고 위젯을 volatile로 만들어 캐싱을 무력화 |
| **이벤트 → 세터 직접 호출** | 효율은 좋지만 **배선 코드가 위젯마다 흩어짐**. 구독/해제·null 체크가 반복 |

MVVM은 **두 번째의 효율을 유지하면서 배선을 선언적으로** 만든 것입니다.

```
[Model]                [ViewModel]                     [View]
게임 데이터        →   UI가 쓰기 좋게 가공한 상태   →   위젯
(어트리뷰트, 인벤토리)   + 변경 통지                     (표시만 담당)
```

> ⚠️ **은탄환이 아니라 배선 정리 도구입니다.** 위젯이 서넛뿐인 화면에 도입하면 파일 수만 늘어납니다.
> 적용 판단 기준은 §7.

---

## 2. 아키텍처 큰 그림

```
[게임 데이터]  ──(델리게이트/폴링)──▶  [ViewModel]
                                        │  UMVVMViewModelBase : UObject, INotifyFieldValueChanged
                                        │  FieldNotify 프로퍼티 + 세터 매크로
                                        │
                                        │  BroadcastFieldValueChanged(FieldId)
                                        ▼
                                    [UMVVMView]   ← UUserWidget의 Extension
                                        │  컴파일된 바인딩 목록(UMVVMViewClass)을 보유
                                        │  통지를 받은 필드의 바인딩만 실행
                                        ▼
                                     [위젯 프로퍼티]  (Text, Percent, ColorAndOpacity …)
```

| 구성 요소 | 역할 |
|:---|:---|
| `UMVVMViewModelBase` | ViewModel 베이스. `UObject` + `INotifyFieldValueChanged` |
| `INotifyFieldValueChanged` | 변경 통지 인터페이스 (FieldNotification 모듈, MVVM 비의존) |
| `UMVVMView` | `UUserWidget`에 붙는 Extension. ViewModel 보유 + 바인딩 실행 |
| `UMVVMViewClass` | 컴파일된 바인딩 정보 (위젯 BP 컴파일 산출물) |
| `UMVVMSubsystem` | 엔진 서브시스템. 바인딩 조회·검증 유틸 |
| `UMVVMGameSubsystem` | GameInstance 서브시스템. **전역 ViewModel 컬렉션** 보유 |
| `UMVVMViewModelCollectionObject` | 이름/클래스로 조회 가능한 ViewModel 보관소 |

플러그인 모듈 구성: `ModelViewViewModel`(런타임), `ModelViewViewModelBlueprint`(BP 데이터),
`ModelViewViewModelEditor`(에디터 UI), `ModelViewViewModelDebugger`(+Editor), `ModelViewViewModelAssetSearch`.

---

## 3. 핵심 개념 (Core Concepts)

### 3.1 ViewModel — FieldNotify와 세터 매크로 ★

```cpp
UCLASS(BlueprintType)
class UMyViewModel : public UMVVMViewModelBase
{
    GENERATED_BODY()

private:
    UPROPERTY(BlueprintReadOnly, FieldNotify, meta = (AllowPrivateAccess = "true"))
    float HealthPercent = 1.f;

    UPROPERTY(BlueprintReadOnly, FieldNotify, meta = (AllowPrivateAccess = "true"))
    FText StatusText;

public:
    void SetHealthPercent(float In) { UE_MVVM_SET_PROPERTY_VALUE(HealthPercent, In); }
    void SetStatusText(const FText& In) { UE_MVVM_SET_PROPERTY_VALUE(StatusText, In); }
};
```

**`FieldNotify` 메타**가 핵심입니다. 이게 붙은 프로퍼티는 "변경을 통지할 수 있는 필드"로 등록되고,
바인딩 대상 후보가 됩니다.

**`UE_MVVM_SET_PROPERTY_VALUE`가 하는 일:**

```cpp
if (Value == NewValue) { return false; }          // ★ 같으면 조기 종료 — 통지 없음
Value = NewValue;
BroadcastFieldValueChanged(FieldId);              // 이 필드를 보는 바인딩만 실행
return true;
```

> **여기가 성능의 핵심입니다.** 매 프레임 세터를 호출해도 **값이 그대로면 위젯을 한 번도 건드리지 않습니다.**
> 속성 바인딩은 값이 그대로여도 매 프레임 평가하므로, 이 차이가 곧 캐싱 유지 여부로 이어집니다.

`FText`처럼 `operator==`가 없는 타입도 베이스가 전용 오버로드를 제공합니다
(`IdenticalTo` 비교). 별도 처리가 필요 없습니다.

`UE_MVVM_SET_PROPERTY_VALUE_INLINE`은 같은 동작을 람다로 인라인 수행하는 변형입니다.

⚠️ **세터를 거치지 않으면 통지가 발생하지 않습니다.**

```cpp
HealthPercent = 0.5f;                              // ❌ 통지 없음 — UI가 안 바뀜
UE_MVVM_SET_PROPERTY_VALUE(HealthPercent, 0.5f);   // ✅
```

ViewModel **내부에서도** 멤버를 직접 대입하지 말고 항상 세터/매크로를 거치는 습관이 필요합니다.

### 3.2 FieldNotification — MVVM과 분리된 통지 계층

통지 자체는 MVVM 플러그인이 아니라 **FieldNotification 모듈**의 것입니다.

```cpp
class INotifyFieldValueChanged : public IInterface
{
    virtual FDelegateHandle AddFieldValueChangedDelegate(FFieldId InFieldId, FFieldValueChangedDelegate InNewDelegate) = 0;
    virtual bool RemoveFieldValueChangedDelegate(FFieldId InFieldId, FDelegateHandle InHandle) = 0;
    virtual const IClassDescriptor& GetFieldNotificationDescriptor() const = 0;
    virtual void BroadcastFieldValueChanged(FFieldId InFieldId) = 0;
};
```

의미하는 바:

- ViewModel은 `UMVVMViewModelBase`를 상속하는 게 **가장 편한 경로일 뿐 유일한 경로는 아닙니다.**
  임의의 `UObject`가 이 인터페이스를 구현하면 바인딩 소스가 될 수 있습니다.
- `UUserWidget` 자신도 `FieldNotify` 프로퍼티를 가질 수 있어, **위젯 → 위젯** 바인딩도 가능합니다.

### 3.3 View — 위젯 쪽 배선

위젯 블루프린트의 **View Model 패널**에서 ViewModel을 등록하고, 디자이너에서 바인딩을 긋습니다.

```
ViewModel.HealthPercent  ──OneWayToDestination──▶  ProgressBar.Percent
ViewModel.StatusText     ──OneWayToDestination──▶  TextBlock.Text
```

이 배선은 컴파일 시 `UMVVMViewClass`로 굳고, 런타임에는 `UMVVMView` Extension이 실행합니다.

```cpp
if (UMVVMView* View = GetExtension<UMVVMView>())
    View->SetViewModelByClass(MyViewModel);
```

`UMVVMView`는 소스·바인딩·이벤트를 각각 초기화/해제하는 API도 노출합니다
(`InitializeSources` / `InitializeBindings` / `InitializeEvents`와 각 `Uninitialize*`).
수동으로 수명을 통제해야 하는 특수 상황에 씁니다.

### 3.4 ViewModel 주입 방식 6종

에디터의 View Model 패널에서 각 ViewModel마다 **Creation Type**을 고릅니다.

```cpp
enum class EMVVMBlueprintViewModelContextCreationType : uint8
{
    Manual,                    // 나중에 코드가 넣어준다
    CreateInstance,            // 위젯 생성 시 새 인스턴스를 만든다
    GlobalViewModelCollection, // 전역 컬렉션에서 가져온다
    PropertyPath,              // 함수/프로퍼티 경로를 평가해 가져온다
    Resolver,                  // 리졸버 객체가 결정한다
    Context,                   // 로컬 컨텍스트 프로바이더에서 (수동 설정도 가능)
};
```

| 방식 | 적합한 경우 |
|:---|:---|
| `Manual` | 게임플레이 데이터(어트리뷰트·인벤토리 등)를 구독해야 해서 **초기화 타이밍을 코드가 통제**해야 할 때 |
| `CreateInstance` | ViewModel이 자기 완결적이고 위젯 수명과 같을 때 |
| `GlobalViewModelCollection` | 여러 화면이 **같은 ViewModel 인스턴스를 공유**할 때 (설정·플레이어 프로필 등) |
| `PropertyPath` | 소유 위젯이 이미 참조를 갖고 있을 때 |
| `Resolver` | 조회 로직이 복잡해 별도 객체로 빼야 할 때 |

**`Manual` 주입 정형:**

```cpp
void UMyWidget::InitViewModel(UAbilitySystemComponent* ASC)
{
    if (!ViewModel)
        ViewModel = NewObject<UMyViewModel>(this);

    ViewModel->Initialize(ASC);          // 게임 데이터 구독 시작

    if (UMVVMView* View = GetExtension<UMVVMView>())
        View->SetViewModelByClass(ViewModel);
}
```

> `SetViewModel(FName, ...)`은 View Model 패널에 등록한 **컨텍스트 이름이 정확히 일치**해야 합니다.
> `SetViewModelByClass`는 클래스로 매칭하므로 이름 오타·리네임에 안전합니다. 기본적으로 후자를 권장합니다.

전역 공유가 필요하면 `UMVVMGameSubsystem::GetViewModelCollection()`에 인스턴스를 등록해 두고
`GlobalViewModelCollection` 방식으로 받습니다.
(`UMVVMSubsystem::GetGlobalViewModelCollection`은 5.3에서 **deprecated** — GameInstance 서브시스템 쪽을 씁니다.)

### 3.5 바인딩 모드

```cpp
enum class EMVVMBindingMode : uint8
{
    OneTimeToDestination = 0,   // 한 번만 VM → View
    OneWayToDestination,        // VM → View   ← 대부분 이것
    TwoWay,                     // 양방향
    OneTimeToSource UMETA(Hidden),
    OneWayToSource,             // View → VM
};
```

- **`OneWayToDestination`** — 표시 전용. 압도적 다수가 이것입니다.
- **`OneTimeToDestination`** — 초기값만 필요하고 이후 변하지 않을 때. 통지 구독 비용조차 아낍니다.
- **`TwoWay` / `OneWayToSource`** — 텍스트 입력·슬라이더처럼 **사용자가 값을 되돌려주는** 위젯에만.

### 3.6 실행 모드 — 언제 반영할 것인가 ★

```cpp
enum class EMVVMExecutionMode : uint8
{
    Immediate = 0,  // 소스 값이 바뀌는 즉시
    Delayed   = 1,  // 값이 바뀌면, 프레임 끝(그리기 직전)에
    Tick      = 2,  // 항상 프레임 끝에
    DelayedWhenSharedElseImmediate = 3 UMETA(DisplayName = "Auto"),
                    // 여러 필드가 트리거하면 Delayed, 아니면 Immediate
};
```

**왜 `Delayed`가 필요한가**: 한 프레임에 소스가 여러 번 바뀌면 `Immediate`는 그때마다 위젯을 갱신합니다.
체력이 한 프레임에 3번 깎이면 3번 그립니다. `Delayed`는 **프레임당 한 번으로 합칩니다.**

- 기본값 **Auto**가 대개 옳은 선택입니다.
- 문자열 조립·서식화처럼 **갱신이 비싼 바인딩**은 `Delayed`를 명시하는 것도 좋습니다.
- ⚠️ **`Tick`은 매 프레임**이라 속성 바인딩과 다를 바 없습니다. 특별한 이유 없이 쓰지 마십시오.

### 3.7 변환 함수 (Conversion Function)

소스와 대상의 타입이 다르면 변환 함수를 끼웁니다. 예: `bool` → `ESlateVisibility`, `float` → `FText`.

에디터가 시그니처가 맞는 후보를 제시하며, 없으면 직접 만들어 붙입니다.
**순수(pure) 함수**여야 하고 부작용이 없어야 합니다.

> ✅ 이 덕분에 ViewModel은 **`bool bIsLowHealth` 같은 의미 단위**만 노출하면 되고,
> "그래서 위젯을 어떻게 보이게 할지"는 View 쪽에서 결정합니다. 관심사 분리가 유지됩니다.

---

## 4. 최소 구현 흐름

1. **플러그인 활성화** — `ModelViewViewModel` (에디터 미리보기가 필요하면 `ModelViewViewModelPreview`도)
2. **모듈 의존성** — `Build.cs`에 `"ModelViewViewModel"`
3. **ViewModel 작성** — `UMVVMViewModelBase` 파생, 프로퍼티에 `FieldNotify`, 세터는 `UE_MVVM_SET_PROPERTY_VALUE`
4. **데이터 구독** — `Initialize(...)` / `Deinitialize()` 쌍을 만들고 게임 데이터 델리게이트를 구독
5. **위젯 BP** — View Model 패널에 ViewModel 등록(Creation Type 선택) → 디자이너에서 바인딩
6. **주입** — `Manual`이면 `GetExtension<UMVVMView>()->SetViewModelByClass(...)`
7. **해제** — 위젯이 내려갈 때 `Deinitialize()`

---

## 5. 작업 시 유의점 (Pitfalls)

### 5.1 ⚠️ 리스트·컬렉션에는 바인딩할 수 없다 (가장 중요한 경계)

`UListView::ListItems`처럼 **런타임 쓰기 불가 프로퍼티**에는 바인딩을 걸 수 없습니다.
위젯 블루프린트 컴파일러가 거부합니다.

**리스트·타일·트리 계열은 C++가 직접 채워야 합니다.**

```cpp
ItemGrid->SetListItems(Items);      // MVVM 아님. 코드가 직접
```

MVVM은 **스칼라 값**(텍스트, 퍼센트, 색, 가시성, bool)에 강하고 **컬렉션에는 약합니다.**
설계 초기에 이 경계를 정해 두는 것이 좋습니다 — "리스트는 코드, 나머지는 MVVM".

### 5.2 ⚠️ `FieldNotify`를 빠뜨리면 조용히 갱신되지 않는다

`FieldNotify`가 없는 프로퍼티에도 바인딩은 걸립니다. 다만 **아무도 통지하지 않아**
초기값 이후로 영원히 갱신되지 않습니다. 컴파일러가 경고를 주지만 놓치기 쉽습니다.

✅ **ViewModel이 노출하는 프로퍼티에는 예외 없이 `FieldNotify`를 붙입니다.**

### 5.3 ⚠️ 세터를 우회하면 통지가 없다

§3.1 참조. 멤버 직접 대입은 UI에 아무 영향을 주지 않습니다.
ViewModel 내부 로직에서도 반드시 세터를 경유하십시오.

### 5.4 ⚠️ 구독 해제를 잊으면 수명 문제가 생긴다

ViewModel이 게임 데이터(어트리뷰트 델리게이트 등)를 구독한다면, 위젯이 사라질 때 반드시 해제해야 합니다.

✅ **정형**: `Initialize` / `Deinitialize` 쌍을 만들고, **`Initialize` 선두에서 먼저 `Deinitialize`를 호출**합니다.
그러면 재초기화(재빙의, 위젯 재사용)에도 중복 구독이 생기지 않습니다.

```cpp
void UMyViewModel::Initialize(UAbilitySystemComponent* InASC)
{
    Deinitialize();          // ★ 재초기화 안전망
    BoundASC = InASC;
    // ... 구독
}
```

구독 핸들을 배열로 보관한다면 **구독할 때와 같은 순서·같은 목록**으로 해제해야 합니다.

### 5.5 ⚠️ ViewModel에 뷰 로직을 넣으면 이점이 사라진다

ViewModel은 "UI가 쓰기 좋은 **데이터**"까지입니다.
위젯을 직접 참조하거나 `SetVisibility`를 호출하기 시작하면 분리·테스트 가능성이라는 이점이 무너집니다.

| ❌ | ✅ |
|:---|:---|
| ViewModel이 `TextBlock`을 들고 `SetText` | ViewModel은 `FText`를 노출, 바인딩이 옮김 |
| ViewModel이 `SetVisibility(Collapsed)` | ViewModel은 `bool`을 노출, 변환 함수가 처리 |

### 5.6 ⚠️ `Tick` 실행 모드는 속성 바인딩과 다를 바 없다

MVVM을 도입한 이유가 매 프레임 평가를 없애는 것인데, 실행 모드를 `Tick`으로 두면 원점입니다.
기본값 **Auto**를 유지하고, 필요할 때만 `Delayed`를 명시하십시오.

### 5.7 ⚠️ 컨텍스트 이름 기반 주입은 리네임에 취약하다

`SetViewModel(FName("MyVM"), ...)`은 View Model 패널의 등록 이름과 문자열이 일치해야 합니다.
이름을 바꾸면 **컴파일 에러 없이 런타임에 조용히 실패**합니다.
가능하면 `SetViewModelByClass`를 쓰십시오.

### 5.8 위젯 재사용 환경에서의 초기화 지점

위젯을 풀링·재사용하는 컨테이너(CommonUI의 화면 스택 등) 아래에서는
`NativeOnInitialized`가 **인스턴스당 한 번만** 실행됩니다.
ViewModel 생성은 1회로 충분하지만, **데이터 구독은 열 때마다 다시 해야** 합니다.

```cpp
void UMyTab::NativeOnActivated()      // 열 때마다 실행되는 훅
{
    Super::NativeOnActivated();
    if (!ViewModel) ViewModel = NewObject<UMyViewModel>(this);   // 생성은 1회
    ViewModel->Initialize(GetDataSource());                      // 구독은 매번
    if (UMVVMView* View = GetExtension<UMVVMView>())
        View->SetViewModelByClass(ViewModel);
}

void UMyTab::NativeOnDeactivated()
{
    if (ViewModel) ViewModel->Deinitialize();                    // 짝을 맞춘다
    Super::NativeOnDeactivated();
}
```

---

## 6. 속성 바인딩 대비 — 왜 MVVM인가

| | UMG 속성 바인딩 | MVVM |
|:---|:---|:---|
| 실행 시점 | **매 프레임** | 값이 **실제로 바뀔 때만** |
| 값이 그대로면 | 그래도 호출 | **조기 종료 — 아무 일 없음** |
| Slate 캐싱(Invalidation) | **무력화** (위젯이 volatile) | 유지 |
| 바인딩 해석 | 런타임 | **컴파일 타임** |
| 배선 위치 | 위젯마다 코드에 흩어짐 | 디자이너에서 한눈에 |
| 테스트 | 위젯이 있어야 함 | **ViewModel만 단독 검증 가능** |
| 컬렉션(리스트) | 불가 | **불가** (동일 한계) |

> volatile·Invalidation의 배경은 [EngineAnalysis_SlateUMG.md](EngineAnalysis_SlateUMG.md) §6 참조.

---

## 7. 도입 판단

**적합**

- 값이 **가끔** 바뀌고 그것을 보는 **위젯이 여럿**인 경우 (스탯 표시, 체력·자원 게이지, 상태 텍스트)
- 같은 데이터를 **여러 화면**이 표시하는 경우
- ViewModel 단위로 **테스트**하고 싶은 경우

**부적합**

- 값이 하나뿐이고 갱신 지점도 하나 → 세터 직접 호출이 더 간단
- **리스트·그리드** → 어차피 C++가 채운다 (§5.1)
- **매 프레임 변하는 값** → MVVM이 절약해 줄 것이 없다
- 위젯 서넛짜리 소규모 HUD → 파일 수만 늘어난다

---

## 8. 체크리스트

1. 플러그인 활성화 + `Build.cs`에 `"ModelViewViewModel"`
2. ViewModel의 모든 노출 프로퍼티에 **`FieldNotify`**
3. 값 변경은 **예외 없이 `UE_MVVM_SET_PROPERTY_VALUE`** 경유
4. `Initialize` / `Deinitialize` 쌍 + `Initialize` 선두의 `Deinitialize` 안전망
5. 주입은 **`SetViewModelByClass`** 우선
6. 실행 모드는 **Auto** 유지, 비싼 바인딩만 `Delayed`
7. **리스트는 MVVM 대상 아님** — C++가 직접 채운다
8. ViewModel에 **위젯 참조·뷰 로직 금지** (bool을 노출하고 변환 함수에 맡긴다)
9. 위젯 재사용 환경이면 **구독은 "열 때마다" 훅에서**

### 흔한 증상 → 원인

| 증상 | 먼저 확인할 것 |
|:---|:---|
| 초기값만 보이고 갱신이 안 됨 | 프로퍼티에 `FieldNotify`가 있는가 (§5.2) |
| 값을 바꿨는데 UI 무반응 | 멤버 직접 대입이 아닌 세터를 거쳤는가 (§5.3) |
| 바인딩을 못 긋겠음 (컴파일러 거부) | 대상이 리스트류 등 런타임 쓰기 불가 프로퍼티인가 (§5.1) |
| 리네임 후 조용히 동작 안 함 | 이름 기반 `SetViewModel`을 쓰고 있는가 (§5.7) |
| 두 번째 열기부터 갱신 안 됨 | 구독을 1회성 훅에 두었는가 (§5.8) |
| 도입했는데 여전히 매 프레임 비용 | 실행 모드가 `Tick`인가 (§5.6) |
| 위젯 파괴 후 크래시·경고 | `Deinitialize` 누락 (§5.4) |
