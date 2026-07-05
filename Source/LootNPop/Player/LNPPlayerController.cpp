// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Player/LNPPlayerController.h"
#include "UI/LNPHudWidget.h"
#include "GameLogic/LNPSurfaceCacheSubsystem.h"
#include "GameMode/LNPGameMode.h"
#include "GameMode/LNPGameState.h"
#include "Player/LNPPlayerState.h"

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

	if (!IsLocalController() || !HudWidget)
		return;

	if (ALNPPlayerState* PS = GetPlayerState<ALNPPlayerState>())
		HudWidget->InitViewModel(PS->GetAbilitySystemComponent());
}

void ALNPPlayerController::AcknowledgePossession(APawn* P)
{
	Super::AcknowledgePossession(P);

	if (!HudWidget)
		return;

	if (ALNPPlayerState* PS = GetPlayerState<ALNPPlayerState>())
		HudWidget->InitViewModel(PS->GetAbilitySystemComponent());
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

	ShowLoadingScreen();

	if (HudWidgetClass)
	{
		HudWidget = CreateWidget<ULNPHudWidget>(this, HudWidgetClass);
		if (HudWidget)
			HudWidget->AddToViewport();
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
