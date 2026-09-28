// Copyright (c) 2026 LootNPop. All rights reserved.

#include "SurfaceNavigation/LNPMassSpawnPoint.h"

#include "Components/SceneComponent.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

ALNPMassSpawnPoint::ALNPMassSpawnPoint()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	SetCanBeDamaged(false);
	Anchor = CreateDefaultSubobject<USceneComponent>(TEXT("Anchor"));
	SetRootComponent(Anchor);
#if WITH_EDITORONLY_DATA
	bIsEditorOnlyActor = true;
#endif
}

void ALNPMassSpawnPoint::PostActorCreated()
{
	Super::PostActorCreated();
	if (!SpawnPointId.IsValid())
	{
		SpawnPointId = FGuid::NewGuid();
	}
}

#if WITH_EDITOR
void ALNPMassSpawnPoint::PostDuplicate(const EDuplicateMode::Type DuplicateMode)
{
	Super::PostDuplicate(DuplicateMode);
	if (DuplicateMode == EDuplicateMode::Normal)
	{
		SpawnPointId = FGuid::NewGuid();
	}
}

void ALNPMassSpawnPoint::PostEditImport()
{
	Super::PostEditImport();
	SpawnPointId = FGuid::NewGuid();
}

EDataValidationResult ALNPMassSpawnPoint::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (!SpawnPointId.IsValid())
	{
		Context.AddError(NSLOCTEXT("LNPMassSpawnPoint", "InvalidSpawnPointId", "SpawnPointId is invalid."));
		Result = EDataValidationResult::Invalid;
	}
	return Result == EDataValidationResult::NotValidated ? EDataValidationResult::Valid : Result;
}
#endif
