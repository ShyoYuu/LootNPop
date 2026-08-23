// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Player/LNPPlayerController.h"
#include "UI/LNPDeathScreenWidget.h"
#include "UI/LNPHudWidget.h"
#include "UI/Menu/LNPMenuRootWidget.h"
#include "UI/Menu/LNPUILayoutWidget.h"
#include "Character/LNPCharacterBase.h"
#include "Character/LNPInputHandlerComponent.h"
#include "GameLogic/LNPSurfaceCacheSubsystem.h"
#include "GameMode/LNPGameMode.h"
#include "GameMode/LNPGameState.h"
#include "Player/LNPPlayerState.h"
#include "LootNPop.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Framework/Application/NavigationConfig.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"
#include "HAL/IConsoleManager.h"
#include "Widgets/SViewport.h"
#include "Widgets/SWindow.h"

#if WITH_EDITOR
/**
 * PIE에서 메뉴를 닫을 때, 메뉴를 열기 전에 포커스를 쥐고 있던 PIE 창으로 Slate 포커스를 되돌릴지.
 * 끄면 엔진 기본 동작 — 즉 메뉴를 닫은 창이 포커스를 가져가고 게임패드 라우팅이 뒤집힌다.
 */
static bool GLNPRestorePIEGamepadFocus = true;
static FAutoConsoleVariableRef CVarLNPRestorePIEGamepadFocus(
	TEXT("LNP.PIE.RestoreGamepadFocusOnMenuClose"),
	GLNPRestorePIEGamepadFocus,
	TEXT("PIE only: when the in-game menu closes, restore Slate focus to the PIE window that had it when the menu opened, so 'Route Gamepad to Second Window' keeps targeting the same client."),
	ECVF_Default);
#endif

namespace
{
	/**
	 * 메뉴 전용 Slate 네비게이션 — 엔진 기본(화살표·D-Pad·좌스틱, Enter/Space=Accept, Esc=Back) 위에
	 * WASD를 얹기만 한다. 기본 생성자가 표준 규칙을 모두 깔아 주므로 추가분만 Emplace한다.
	 */
	class FLNPMenuNavigationConfig : public FNavigationConfig
	{
	public:
		FLNPMenuNavigationConfig()
		{
			KeyEventRules.Emplace(EKeys::W, EUINavigation::Up);
			KeyEventRules.Emplace(EKeys::A, EUINavigation::Left);
			KeyEventRules.Emplace(EKeys::S, EUINavigation::Down);
			KeyEventRules.Emplace(EKeys::D, EUINavigation::Right);
		}
	};
}

bool ALNPPlayerController::IsLoadingComplete() const
{
	if (HasAuthority())
	{
		const ALNPGameState* GS = GetWorld()->GetGameState<ALNPGameState>();
		return GS && GS->ServerPhase == ELNPInitPhase::Complete;
	}
	return bLoadingComplete;
}

namespace
{
	/** HUD 대시 쿨다운 표시에 쓸 Mover를 폰에서 꺼낸다. LNP 캐릭터가 아니면 null. */
	ULNPCharacterMoverComponent* GetMoverComponentFromPawn(APawn* InPawn)
	{
		ALNPCharacterBase* Character = Cast<ALNPCharacterBase>(InPawn);
		return Character ? Character->GetMoverComponent() : nullptr;
	}
}

void ALNPPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (!IsLocalController())
		return;

	// 리스폰으로 새 폰에 빙의했다 — 사망 오버레이를 걷는다.
	HideDeathScreen();

	if (ALNPPlayerState* PS = GetPlayerState<ALNPPlayerState>())
	{
		if (HudWidget)
			HudWidget->InitViewModel(PS->GetAbilitySystemComponent(), GetMoverComponentFromPawn(InPawn));
	}
}

void ALNPPlayerController::AcknowledgePossession(APawn* P)
{
	Super::AcknowledgePossession(P);

	// 원격 클라이언트의 리스폰 경로 — OnPossess는 서버에서만 돈다.
	HideDeathScreen();

	if (ALNPPlayerState* PS = GetPlayerState<ALNPPlayerState>())
	{
		if (HudWidget)
			HudWidget->InitViewModel(PS->GetAbilitySystemComponent(), GetMoverComponentFromPawn(P));
	}
}

void ALNPPlayerController::ShowDeathScreen(float RespawnDelay)
{
	if (!IsLocalController() || DeathScreenWidgetClass == nullptr)
		return;

	// 사망 시점에 처음 만든다 — 죽지 않는 판에서는 위젯을 아예 만들지 않는다.
	if (DeathScreenWidget == nullptr)
	{
		DeathScreenWidget = CreateWidget<ULNPDeathScreenWidget>(this, DeathScreenWidgetClass);
		if (DeathScreenWidget == nullptr)
			return;

		// 메뉴(UILayoutWidget, ZOrder 10)보다 아래에 둔다 — 죽은 채로 메뉴를 열 수 있어야 한다.
		DeathScreenWidget->AddToViewport(5);
	}

	DeathScreenWidget->ShowCountdown(RespawnDelay);
}

void ALNPPlayerController::HideDeathScreen()
{
	if (DeathScreenWidget)
		DeathScreenWidget->HideCountdown();
}



void ALNPPlayerController::OnUnPossess()
{
	if (HudWidget)
		HudWidget->DeinitViewModel();

	Super::OnUnPossess();
}

void ALNPPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalController())
		return;

	// 컨트롤러 상시 매핑 컨텍스트 등록 — 폰의 DefaultMappingContext와 별개로 컨트롤러 수명 동안 유지된다.
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		if (PlayerMappingContext)
			Subsystem->AddMappingContext(PlayerMappingContext, 0);
	}

	ShowLoadingScreen();

	if (HudWidgetClass)
	{
		HudWidget = CreateWidget<ULNPHudWidget>(this, HudWidgetClass);
		if (HudWidget)
			HudWidget->AddToViewport();
	}

	// UI 레이아웃은 뷰포트에 상주한다 — 메뉴는 이 안의 스택에 push/pop된다.
	if (UILayoutWidgetClass)
	{
		UILayoutWidget = CreateWidget<ULNPUILayoutWidget>(this, UILayoutWidgetClass);
		if (UILayoutWidget)
		{
			UILayoutWidget->AddToViewport(10);
			UILayoutWidget->OnMenuClosed.AddUObject(this, &ALNPPlayerController::HandleMenuClosed);
		}
	}

	UWorld* World = GetWorld();
	check(World);

	if (ULNPSurfaceCacheSubsystem* SurfaceSub = World->GetSubsystem<ULNPSurfaceCacheSubsystem>())
	{
		if (SurfaceSub->GetBakingProgress() >= 1.0f)
		{
			// 베이킹 이미 완료됨 (예: 리슨 서버 로컬 Player)
			OnLocalBakingComplete();
		}
		else
		{
			SurfaceSub->OnBakingComplete.AddDynamic(this, &ALNPPlayerController::OnLocalBakingComplete);
		}
	}
}

void ALNPPlayerController::OnLocalBakingComplete()
{
	bLoadingComplete = true;
	HideLoadingScreen();
	ServerNotifyClientReady();
}

void ALNPPlayerController::ServerNotifyClientReady_Implementation()
{
	if (ALNPGameMode* GM = GetWorld()->GetAuthGameMode<ALNPGameMode>())
	{
		GM->OnClientReady(this);
	}
}

void ALNPPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// 메뉴 열기는 UI 관심사이므로 폰 Enhanced Input(DefaultMappingContext)과 분리된
	// 컨트롤러 상시 매핑 컨텍스트(PlayerMappingContext)로 처리한다 — 메뉴가 폰 입력을 꺼도 살아 있어야 한다.
	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent))
	{
		if (OpenMenuAction)
			EIC->BindAction(OpenMenuAction, ETriggerEvent::Started, this, &ALNPPlayerController::HandleOpenMenuInput);
		if (OpenSettingsAction)
			EIC->BindAction(OpenSettingsAction, ETriggerEvent::Started, this, &ALNPPlayerController::HandleOpenSettingsInput);
	}
}

void ALNPPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// ⚠️ 네비게이션 설정은 FSlateApplication 전역이라, 메뉴를 연 채 PIE를 끝내면
	// WASD 네비게이션이 에디터에 그대로 남는다. 반드시 원복하고 나간다.
	SetMenuNavigationEnabled(false);

	Super::EndPlay(EndPlayReason);
}

void ALNPPlayerController::HandleOpenMenuInput()
{
	// 같은 키로 열고 닫는다. 메뉴 활성 중에도 이 액션이 살아 있도록
	// ULNPMenuRootWidget::GetDesiredInputConfig가 ECommonInputMode::All을 돌려준다.
	if (UILayoutWidget && UILayoutWidget->IsMenuOpen())
	{
		CloseMenu();
		return;
	}

	OpenMenu(NAME_None);
}

void ALNPPlayerController::HandleOpenSettingsInput()
{
	if (UILayoutWidget && UILayoutWidget->IsMenuOpen())
	{
		CloseMenu();
		return;
	}

	OpenMenu(ULNPMenuRootWidget::TabId_Settings());
}

void ALNPPlayerController::OpenMenu(FName TabId)
{
	if (!IsLocalController() || UILayoutWidget == nullptr || MenuWidgetClass == nullptr)
		return;

	// None이면 마지막으로 보던 탭 — 첫 진입이면 캐릭터 스탯 (기획 §2).
	if (TabId.IsNone())
		TabId = LastViewedTabId.IsNone() ? ULNPMenuRootWidget::TabId_Stats() : LastViewedTabId;

	const bool bWasOpen = UILayoutWidget->IsMenuOpen();
	if (UILayoutWidget->OpenMenu(MenuWidgetClass, TabId) == nullptr)
		return;

	if (bWasOpen)
		return;   // 이미 열려 있었으면 탭만 바뀐 것 — 입력·일시정지 상태는 그대로 둔다.

	SetPawnGameplayInputEnabled(false);
	SetMenuNavigationEnabled(true);

#if WITH_EDITOR
	// 아래 SetInputMode가 Slate 포커스를 이 창으로 끌어오기 전에, 지금 포커스를 쥔 창을 기억해 둔다.
	CapturePIEForeignFocus();
#endif

	FInputModeGameAndUI Mode;
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	// 포커스 대상은 지정하지 않는다 — CommonUI의 활성화/GetDesiredFocusTarget이 관리한다.
	SetInputMode(Mode);
	bShowMouseCursor = true;

	// 일시정지는 스탠드얼론에서만. 멀티에서는 월드를 멈출 수 없다 (기획 §2).
	if (GetNetMode() == NM_Standalone)
		SetPause(true);
}

void ALNPPlayerController::CloseMenu()
{
	if (UILayoutWidget)
		UILayoutWidget->CloseMenu();   // 뒷정리는 HandleMenuClosed가 한다.
}

void ALNPPlayerController::HandleMenuClosed()
{
	// 다음 진입에 쓸 탭을 기억해 둔다 (환경설정은 루트가 걸러 준다).
	if (const ULNPMenuRootWidget* Menu = UILayoutWidget ? UILayoutWidget->GetMenu() : nullptr)
		LastViewedTabId = Menu->GetRememberableTabId();

	if (GetNetMode() == NM_Standalone)
		SetPause(false);

	SetInputMode(FInputModeGameOnly());
	bShowMouseCursor = false;

#if WITH_EDITOR
	// 반드시 SetInputMode 뒤에서 — 포커스 지정을 덮어써야 하기 때문이다 (RestorePIEForeignFocus 주석 참고).
	RestorePIEForeignFocus();
#endif

	SetMenuNavigationEnabled(false);
	SetPawnGameplayInputEnabled(true);
}

void ALNPPlayerController::SetPawnGameplayInputEnabled(bool bEnabled)
{
	if (APawn* ControlledPawn = GetPawn())
	{
		if (ULNPInputHandlerComponent* InputHandler = ControlledPawn->FindComponentByClass<ULNPInputHandlerComponent>())
			InputHandler->SetGameplayInputEnabled(bEnabled);
	}
}

void ALNPPlayerController::SetMenuNavigationEnabled(bool bEnabled)
{
	if (!FSlateApplication::IsInitialized())
		return;

	FSlateApplication& SlateApp = FSlateApplication::Get();

	if (bEnabled)
	{
		// 중복 진입 방지 — 이미 걸려 있으면 원본을 덮어써 잃어버리지 않는다.
		if (PreviousNavigationConfig.IsValid())
			return;

		PreviousNavigationConfig = SlateApp.GetNavigationConfig();
		SlateApp.SetNavigationConfig(MakeShared<FLNPMenuNavigationConfig>());
	}
	else if (PreviousNavigationConfig.IsValid())
	{
		SlateApp.SetNavigationConfig(PreviousNavigationConfig.ToSharedRef());
		PreviousNavigationConfig.Reset();
	}
}

#if WITH_EDITOR
void ALNPPlayerController::CapturePIEForeignFocus()
{
	PIEForeignFocusWidget.Reset();

	const UWorld* World = GetWorld();
	if (!GLNPRestorePIEGamepadFocus || World == nullptr || World->WorldType != EWorldType::PIE || !FSlateApplication::IsInitialized())
		return;

	// PIE 클라이언트들의 첫 LocalPlayer는 모두 ControllerId 0 — 즉 FSlateUser(0) 하나를 공유한다.
	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	const TSharedPtr<FSlateUser> SlateUser = LocalPlayer ? LocalPlayer->GetSlateUser() : nullptr;
	const TSharedPtr<SWidget> FocusedWidget = SlateUser ? SlateUser->GetFocusedWidget() : nullptr;

	const UGameViewportClient* ViewportClient = World->GetGameViewport();
	const TSharedPtr<SViewport> MyViewportWidget = ViewportClient ? ViewportClient->GetGameViewportWidget() : nullptr;
	if (!FocusedWidget.IsValid() || !MyViewportWidget.IsValid())
		return;

	FSlateApplication& SlateApp = FSlateApplication::Get();
	const TSharedPtr<SWindow> FocusedWindow = SlateApp.FindWidgetWindow(FocusedWidget.ToSharedRef());
	const TSharedPtr<SWindow> MyWindow = SlateApp.FindWidgetWindow(MyViewportWidget.ToSharedRef());

	// 내 창이 이미 포커스를 쥐고 있으면 라우팅이 뒤집힐 일이 없다 — 기억할 필요도 없다.
	if (!FocusedWindow.IsValid() || FocusedWindow == MyWindow)
		return;

	PIEForeignFocusWidget = FocusedWidget;
}

void ALNPPlayerController::RestorePIEForeignFocus()
{
	const TSharedPtr<SWidget> Target = PIEForeignFocusWidget.Pin();
	PIEForeignFocusWidget.Reset();

	if (!GLNPRestorePIEGamepadFocus || !Target.IsValid())
		return;

	/**
	 * SetInputMode(FInputModeGameOnly)가 LocalPlayer의 지연 FReply에 심어 둔
	 * SetUserFocus(자기 뷰포트 위젯)를 덮어쓴다 — FReply는 포커스 수신자를 필드 하나로 들고 있어
	 * 마지막 지정이 이긴다.
	 *
	 * ⚠️ FSlateApplication::SetUserFocus를 직접 부르면 안 된다. 그 FReply는 엔진 틱 말미의
	 * ProcessLocalPlayerSlateOperations(LaunchEngineLoop.cpp)에서 뒤늦게 적용되므로
	 * 직접 옮긴 포커스를 도로 빼앗아 간다.
	 */
	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
		LocalPlayer->GetSlateOperations().SetUserFocus(Target.ToSharedRef(), EFocusCause::SetDirectly);
}
#endif

namespace
{
	/** 디버그: LNP.Debug.OpenMenu [TabId] — 키 바인딩 없이 인게임 메뉴를 연다 (검증용) */
	FAutoConsoleCommandWithWorldAndArgs GLNPDebugOpenMenu(
		TEXT("LNP.Debug.OpenMenu"),
		TEXT("Open the in-game menu for the first local player controller. Optional arg: Stats | Inventory | Settings."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (World == nullptr)
				return;
			if (ALNPPlayerController* PC = Cast<ALNPPlayerController>(World->GetFirstPlayerController()))
				PC->OpenMenu(Args.Num() > 0 ? FName(*Args[0]) : NAME_None);
		}));

	/** 디버그: LNP.Debug.CloseMenu — 인게임 메뉴를 닫는다 (검증용) */
	FAutoConsoleCommandWithWorld GLNPDebugCloseMenu(
		TEXT("LNP.Debug.CloseMenu"),
		TEXT("Close the in-game menu for the first local player controller."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (World == nullptr)
				return;
			if (ALNPPlayerController* PC = Cast<ALNPPlayerController>(World->GetFirstPlayerController()))
				PC->CloseMenu();
		}));
}
