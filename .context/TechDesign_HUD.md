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

## 7. 어필 포인트 (설계 원칙)

- **GAS ↔ Widget 직접 결합 없음.** ViewModel이 유일한 중개자 — 위젯 레이아웃 변경이 전투 로직에 영향을 주지 않고, ViewModel은 위젯 없이 단위 검증 가능.
- **틱 없는 이벤트 기반.** ASC 델리게이트 → FieldNotify 체인. `UE_MVVM_SET_PROPERTY_VALUE`의 변경 감지로 동일 값 재전달 시 알림 생략.
- **ASC는 PlayerState에서 직접 접근.** `GetPlayerState<ALNPPlayerState>()`로 획득 — Pawn의 PlayerState 캐싱 타이밍에 무관.
- **멀티플레이 이중 진입점.** `OnPossess`는 서버에서만 호출되므로 원격 클라이언트는 `AcknowledgePossession`에서 동일 초기화를 수행 — 어느 경로든 `InitViewModel`이 멱등(ViewModel 1회 생성, Initialize가 기존 구독 해제 후 재구독)이라 중복 호출에 안전.
- **수명 관리 3중 안전망.** `OnUnPossess` / `NativeDestruct` / `Initialize` 선두의 `Deinitialize()` — 어떤 순서로 파괴·재빙의가 일어나도 델리게이트 누수가 없다.

---

## 8. 인벤토리 패널 — ✅ 구현 완료 (2026-07-12) / 인스턴스 모델 전환 (2026-07-17)

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
  DropButton → Character->DropItem(ItemId) / EquipButton → Character->EquipWeaponInstance(Instance)
```

**클래스/에셋:** `UI/LNPInventoryWidget`, `UI/LNPInventoryEntryWidget`. BP: `/Game/UI/WBP_Inventory`(+ RootVBox·StorageList·BuffList), `WBP_InventoryEntry`, `WBP_BuffEntry`. `BP_LNPPlayerController.InventoryWidgetClass` 지정. (구 `ULNPInventoryViewModel`/`ULNPBuffEntryObject`는 삭제됨.)

**PIE 검증(호스트, 2026-07-17):** 획득→표시, 장착 시 가방에서 즉시 사라짐, 드랍→Dice 스폰·재획득 정상. 이름 표시됨. ⚠️ **BindWidget 함정:** `NameText`가 트리에 있어도 **Is Variable이 꺼져 있으면 BindWidgetOptional이 null** — 위젯 BP에서 bIsVariable 켜야 한다.

## 9. 미구현 항목

- **HUD 추가 요소:** 미니맵, 점수/메달 카운터 등 (DevelopmentPlan Phase 6).
- **루팅 게이지 HUD 연동:** `ALNPLootPod::GetGaugePercent()`(복제 완료)를 읽는 월드 스페이스 또는 HUD 게이지 위젯.
- **쿨다운 표시:** Active Skill 슬롯·대시 쿨다운의 ViewModel 프로퍼티화.
- **인벤토리 폴리시:** 버프 잔여시간 라이브 카운트다운, 스킬 슬롯 장착 UI, 패널 배경·레이아웃 스타일. `WBP_InventoryEntry`에 `DetailText` TextBlock 추가(버프 잔여·레벨 표시 — C++는 이미 지원, 위젯만 없음).
