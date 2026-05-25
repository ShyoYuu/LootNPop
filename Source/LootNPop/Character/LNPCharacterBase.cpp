// Copyright Epic Games, Inc. All Rights Reserved.

#include "LNPCharacterBase.h"
#include "Movement/LNPCharacterMoverComponent.h"
#include "Character/LNPInputHandlerComponent.h"
#include "Gravity/LNPPawnGravityComponent.h"
#include "Player/LNPPlayerState.h"

#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "AbilitySystemComponent.h"
#include "MassAgentComponent.h"


ALNPCharacterBase::ALNPCharacterBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;

	SetReplicatingMovement(false);

	CapsuleComponent = CreateDefaultSubobject<UCapsuleComponent>(TEXT("CapsuleComponent"));
	CapsuleComponent->InitCapsuleSize(42.f, 96.0f);
	SetRootComponent(CapsuleComponent);

	AnimSourceMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("AnimSourceMesh"));
	AnimSourceMesh->SetupAttachment(CapsuleComponent);
	VisualMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("VisualMesh"));
	VisualMesh->SetupAttachment(AnimSourceMesh);

	MassAgentComponent = CreateDefaultSubobject<UMassAgentComponent>(TEXT("MassAgentComponent"));

	MoverComponent = CreateDefaultSubobject<ULNPCharacterMoverComponent>(TEXT("MoverComponent"));
	InputHandlerComponent = CreateDefaultSubobject<ULNPInputHandlerComponent>(TEXT("InputHandlerComponent"));
	GravityComponent = CreateDefaultSubobject<ULNPPawnGravityComponent>(TEXT("GravityComponent"));

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;
}

bool ALNPCharacterBase::GetOrientRotationToMovement() const
{
	return InputHandlerComponent ? InputHandlerComponent->GetOrientRotationToMovement() : false;
}

UAbilitySystemComponent* ALNPCharacterBase::GetAbilitySystemComponent() const
{
	if (const ALNPPlayerState* PS = GetPlayerState<ALNPPlayerState>())
		return PS->GetAbilitySystemComponent();
	return nullptr;
}

void ALNPCharacterBase::SetAIMoveInput(FVector InMoveInput)
{
	if (InputHandlerComponent)
		InputHandlerComponent->SetAIMoveInput(InMoveInput);
}

void ALNPCharacterBase::SetAIOrientationIntent(FVector InOrientationIntent)
{
	if (InputHandlerComponent)
		InputHandlerComponent->SetAIOrientationIntent(InOrientationIntent);
}

bool ALNPCharacterBase::TryActivateAttack()
{
	return false;
}

void ALNPCharacterBase::PostInitializeComponents()
{
	Super::PostInitializeComponents();
}

void ALNPCharacterBase::BeginPlay()
{
	Super::BeginPlay();
}

void ALNPCharacterBase::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (InputHandlerComponent)
		InputHandlerComponent->SetupPlayerInputComponent(PlayerInputComponent);
}

