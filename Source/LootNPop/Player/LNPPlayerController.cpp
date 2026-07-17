// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Player/LNPPlayerController.h"
#include "UI/LNPHudWidget.h"
#include "UI/LNPInventoryWidget.h"
#include "Item/LNPInventoryComponent.h"
#include "GameLogic/LNPSurfaceCacheSubsystem.h"
#include "GameMode/LNPGameMode.h"
#include "GameMode/LNPGameState.h"
#include "Player/LNPPlayerState.h"
#include "LootNPop.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "HAL/IConsoleManager.h"

bool ALNPPlayerController::IsLoadingComplete() const
{
	if (HasAuthority())
	{
		const ALNPGameState* GS = GetWorld()->GetGameState<ALNPGameState>();
		return GS && GS->ServerPhase == ELNPInitPhase::Complete;
	}
	return bLoadingComplete;
}

void ALNPPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (!IsLocalController())
		return;

	if (ALNPPlayerState* PS = GetPlayerState<ALNPPlayerState>())
	{
		if (HudWidget)
			HudWidget->InitViewModel(PS->GetAbilitySystemComponent());
	}
	InitInventoryViewModel();
}

void ALNPPlayerController::AcknowledgePossession(APawn* P)
{
	Super::AcknowledgePossession(P);

	if (ALNPPlayerState* PS = GetPlayerState<ALNPPlayerState>())
	{
		if (HudWidget)
			HudWidget->InitViewModel(PS->GetAbilitySystemComponent());
	}
	InitInventoryViewModel();
}

void ALNPPlayerController::OnUnPossess()
{
	if (HudWidget)
		HudWidget->DeinitViewModel();
	if (InventoryWidget)
		InventoryWidget->DeinitViewModel();

	Super::OnUnPossess();
}

void ALNPPlayerController::InitInventoryViewModel()
{
	if (!InventoryWidget)
		return;

	if (const ALNPPlayerState* PS = GetPlayerState<ALNPPlayerState>())
	{
		if (ULNPInventoryComponent* Inventory = PS->GetInventoryComponent())
			InventoryWidget->InitViewModel(Inventory);
	}
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

	// 인벤토리 패널은 뷰포트에 올려두되 기본 숨김 — ToggleInventory로 표시/숨김.
	if (InventoryWidgetClass)
	{
		InventoryWidget = CreateWidget<ULNPInventoryWidget>(this, InventoryWidgetClass);
		if (InventoryWidget)
		{
			InventoryWidget->AddToViewport(10);
			InventoryWidget->SetVisibility(ESlateVisibility::Collapsed);
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

	// 인벤토리 토글은 UI 관심사이므로 폰 Enhanced Input(DefaultMappingContext)과 분리된
	// 컨트롤러 상시 매핑 컨텍스트(PlayerMappingContext)로 처리한다 — UI 입력 수명이 빙의와 무관하게 유지된다.
	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent))
	{
		if (ToggleInventoryAction)
			EIC->BindAction(ToggleInventoryAction, ETriggerEvent::Started, this, &ALNPPlayerController::ToggleInventory);
	}
}

void ALNPPlayerController::ToggleInventory()
{
	if (!IsLocalController() || InventoryWidget == nullptr)
		return;

	bInventoryOpen = !bInventoryOpen;
	InventoryWidget->SetVisibility(bInventoryOpen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

	if (bInventoryOpen)
	{
		FInputModeGameAndUI Mode;
		Mode.SetWidgetToFocus(InventoryWidget->TakeWidget());
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(Mode);
		bShowMouseCursor = true;
	}
	else
	{
		SetInputMode(FInputModeGameOnly());
		bShowMouseCursor = false;
	}
}

namespace
{
	/** 디버그: LNP.Debug.ToggleInventory — 키 바인딩 없이도 인벤토리 패널을 토글 (검증용) */
	FAutoConsoleCommandWithWorld GLNPDebugToggleInventory(
		TEXT("LNP.Debug.ToggleInventory"),
		TEXT("Toggle the inventory panel for the first local player controller."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (World == nullptr)
				return;
			if (ALNPPlayerController* PC = Cast<ALNPPlayerController>(World->GetFirstPlayerController()))
				PC->ToggleInventory();
		}));
}
