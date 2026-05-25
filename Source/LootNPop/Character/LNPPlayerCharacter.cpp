// Fill out your copyright notice in the Description page of Project Settings.

#include "Character/LNPPlayerCharacter.h"
#include "Player/LNPPlayerState.h"
#include "Item/LNPEquipmentComponent.h"
#include "Item/LNPItemInstance.h"
#include "Item/LNPWeaponData.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/GameplayCameraComponent.h"
#include "Interaction/LNPInteractionComponent.h"

ALNPPlayerCharacter::ALNPPlayerCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	GameplayCamera = CreateDefaultSubobject<UGameplayCameraComponent>(TEXT("GameplayCamera"));
	GameplayCamera->SetupAttachment(AnimSourceMesh);

	InteractionComponent = CreateDefaultSubobject<ULNPInteractionComponent>(TEXT("InteractionComponent"));
}

void ALNPPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();
}

void ALNPPlayerCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}

void ALNPPlayerCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	if (ALNPPlayerState* PS = GetPlayerState<ALNPPlayerState>())
		PS->GetAbilitySystemComponent()->InitAbilityActorInfo(PS, this);

	if (GameplayCamera)
		GameplayCamera->ActivateCameraForPlayerController(Cast<APlayerController>(NewController));
}

void ALNPPlayerCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	if (ALNPPlayerState* PS = GetPlayerState<ALNPPlayerState>())
		PS->GetAbilitySystemComponent()->InitAbilityActorInfo(PS, this);
}

const ULNPWeaponData* ALNPPlayerCharacter::GetActiveWeaponDef() const
{
	const ALNPPlayerState* PS = GetPlayerState<ALNPPlayerState>();
	if (!PS)
		return nullptr;

	const ULNPEquipmentComponent* EqComp = PS->GetEquipmentComponent();
	if (!EqComp)
		return nullptr;

	return EqComp->GetWeaponSlot().Definition;
}

bool ALNPPlayerCharacter::TryActivateAttack()
{
	const ALNPPlayerState* PS = GetPlayerState<ALNPPlayerState>();
	if (!PS)
		return false;

	UAbilitySystemComponent* ASC = PS->GetAbilitySystemComponent();
	if (!ASC)
		return false;

	const ULNPEquipmentComponent* EqComp = PS->GetEquipmentComponent();
	if (!EqComp)
		return false;

	const FLNPWeaponInstance& WeaponSlot = EqComp->GetWeaponSlot();
	if (!WeaponSlot.IsValid() || !WeaponSlot.GrantedAbilities.IsValidIndex(0))
		return false;

	return ASC->TryActivateAbility(WeaponSlot.GrantedAbilities[0]);
}

TArray<AActor*> ALNPPlayerCharacter::GetInteractionCandidates() const
{
	return InteractionComponent ? InteractionComponent->GetInteractionCandidates() : TArray<AActor*>();
}

AActor* ALNPPlayerCharacter::GetInteractionCandidate() const
{
	return InteractionComponent ? InteractionComponent->GetFirstInteractionCandidate() : nullptr;
}