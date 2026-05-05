// Fill out your copyright notice in the Description page of Project Settings.

#include "Character/LNPPlayerCharacter.h"
#include "Enemy/LNPEnemyMassTypes.h"
#include "MassAgentComponent.h"
#include "MassEntitySubsystem.h"
#include "MassLODSubsystem.h"

ALNPPlayerCharacter::ALNPPlayerCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void ALNPPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	// 1. Register as a Player for Targeting logic
	if (UMassAgentComponent* AgentComp = FindComponentByClass<UMassAgentComponent>())
	{
		// DA_PlayerEntityConfig로 옮김
	}

	// 2. Register this PAWN as a Viewer for LOD calculations
	// This ensures Mass uses the pawn's moving location instead of the static PlayerController location.
	if (UMassLODSubsystem* LODSubsystem = GetWorld()->GetSubsystem<UMassLODSubsystem>())
	{
		FMassViewerHandle ViewerHandle = LODSubsystem->GetViewerHandleFromActor(*this);
		if (ViewerHandle.IsValid() == false)
		{
			LODSubsystem->RegisterActorViewer(*this);
		}
	}
}

void ALNPPlayerCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 3. Clean up Viewer registration
	if (UMassLODSubsystem* LODSubsystem = GetWorld()->GetSubsystem<UMassLODSubsystem>())
	{
		LODSubsystem->UnregisterActorViewer(*this);
	}

	Super::EndPlay(EndPlayReason);
}
