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

ALNPPlayerController::OnPossess(Pawn)
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
| `ALNPPlayerController` | `Player/LNPPlayerController.h/.cpp` | 위젯 생성(BeginPlay), ViewModel 초기화(OnPossess) |

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

## 7. 설계 원칙

- **GAS ↔ Widget 직접 결합 없음.** ViewModel이 유일한 중개자.
- **틱 없는 이벤트 기반.** `UE_MVVM_SET_PROPERTY_VALUE`의 변경 감지로 동일 값 재전달 시 알림 생략.
- **ASC는 PlayerState에서 직접 접근.** `OnPossess`에서 `GetPlayerState<ALNPPlayerState>()`로 획득 — Pawn의 PlayerState 캐싱 타이밍에 무관.
