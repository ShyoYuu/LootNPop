// Fill out your copyright notice in the Description page of Project Settings.


#include "LootPod/LNPLootPod.h"
#include "LootPod/LNPLootPodMassTypes.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "SmartObjectComponent.h"
#include "NiagaraComponent.h"
#include "MassAgentComponent.h"
#include "MassEntityManager.h"
#include "MassEntityUtils.h"

// Sets default values
ALNPLootPod::ALNPLootPod()
{
	// Set this actor to call Tick() every frame.
	PrimaryActorTick.bCanEverTick = true;

	// 1. Static Mesh (Root)
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	SetRootComponent(MeshComponent);

	// 2. Smart Object Component
	SmartObjectComponent = CreateDefaultSubobject<USmartObjectComponent>(TEXT("SmartObjectComponent"));

	// 3. Looting Zone Sphere
	LootingZoneSphere = CreateDefaultSubobject<USphereComponent>(TEXT("LootingZoneSphere"));
	LootingZoneSphere->SetupAttachment(RootComponent);
	LootingZoneSphere->SetSphereRadius(500.0f); 
	LootingZoneSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	LootingZoneSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	// 4. Niagara VFX (Loot Pillar)
	LootPillarVFX = CreateDefaultSubobject<UNiagaraComponent>(TEXT("LootPillarVFX"));
	LootPillarVFX->SetupAttachment(RootComponent);

	// 5. Mass Agent Component (Link to MassEntity)
	MassAgentComponent = CreateDefaultSubobject<UMassAgentComponent>(TEXT("MassAgentComponent"));
}

void ALNPLootPod::BeginPlay()
{
	Super::BeginPlay();
	
	// Set initial visual state
	UpdateVisuals(ELNPLootPodState::Idle);
}

void ALNPLootPod::StartLooting()
{
	// Update Local State and Visuals
	UpdateVisuals(ELNPLootPodState::Looting);
}

void ALNPLootPod::UpdateVisuals(ELNPLootPodState NewState)
{
	CurrentState = NewState;
	if (!LootPillarVFX) return;

	FLinearColor TargetColor = IdleColor;

	switch (NewState)
	{
	case ELNPLootPodState::Looting:
		TargetColor = LootingColor;
		break;
	case ELNPLootPodState::Popped:
		TargetColor = PoppedColor;
		break;
	case ELNPLootPodState::Idle:
	default:
		TargetColor = IdleColor;
		break;
	}

	LootPillarVFX->SetVariableLinearColor(ColorParameterName, TargetColor);
}

bool ALNPLootPod::CanInteract_Implementation(const APawn* Interactor) const
{
	if (!Interactor) return false;

	// Only allow interaction if Idle or already Looting (multiple players)
	if (CurrentState == ELNPLootPodState::Popped || CurrentState == ELNPLootPodState::Interrupted)
	{
		return false;
	}

	// 1. Distance Check
	const float DistSq = FVector::DistSquared(GetActorLocation(), Interactor->GetActorLocation());
	const float MaxDist = LootingZoneSphere->GetScaledSphereRadius();
	
	if (DistSq > FMath::Square(MaxDist))
	{
		return false;
	}

	// 2. Angle Check
	const FVector DirToInteractor = (Interactor->GetActorLocation() - GetActorLocation()).GetSafeNormal();
	const float DotProduct = FVector::DotProduct(GetActorForwardVector(), DirToInteractor);
	const float AngleDeg = FMath::RadiansToDegrees(FMath::Acos(DotProduct));

	if (AngleDeg > MaxInteractionAngle)
	{
		return false;
	}

	return true;
}
