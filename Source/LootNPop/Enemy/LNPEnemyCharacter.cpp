// Fill out your copyright notice in the Description page of Project Settings.

#include "Enemy/LNPEnemyCharacter.h"
#include "Enemy/LNPEnemyConfig.h"
#include "Enemy/LNPEnemyMassTypes.h"
#include "AbilitySystemComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "MassAgentComponent.h"

ALNPEnemyCharacter::ALNPEnemyCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Default settings for enemies
	PrimaryActorTick.bCanEverTick = true;
}

void ALNPEnemyCharacter::BeginPlay()
{
	Super::BeginPlay();
}

void ALNPEnemyCharacter::InitializeFromConfig(ULNPEnemyConfig* InConfig)
{
	if (nullptr == InConfig)
		return;

	EnemyConfig = InConfig;

	// 1. Setup Visuals
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		if (InConfig->SkeletalMesh)
		{
			MeshComp->SetSkeletalMesh(InConfig->SkeletalMesh);
		}

		if (InConfig->AnimBlueprint)
		{
			MeshComp->SetAnimInstanceClass(InConfig->AnimBlueprint);
		}
	}

	// 2. Setup GAS (Abilities & Attributes)
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		// Grant Default Abilities
		for (const TSubclassOf<UGameplayAbility>& AbilityClass : InConfig->DefaultAbilities)
		{
			if (AbilityClass)
			{
				ASC->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1, INDEX_NONE, this));
			}
		}

		// Apply Initial Attributes
		for (auto& AttributePair : InConfig->InitialAttributeValues)
		{
			// Note: In a real project, you would map GameplayTags to actual Attribute accessors.
			// For now, we assume a generic way to set them or handle via Effect.
		}
	}
}

void ALNPEnemyCharacter::SyncFromEntity(FMassEntityHandle InEntityHandle, float InHealth, ELNPTargetingState InTargetingState)
{
	// Link the actor to the Mass Entity via the Agent Component
	if (UMassAgentComponent* AgentComp = FindComponentByClass<UMassAgentComponent>())
	{
		// Force set the entity handle to ensure the actor knows its owner
		// AgentComp->SetEntityHandle(InEntityHandle);
	}

	// TODO: Set Health to ASC or local variables
	// TODO: Handle AI activation based on TargetingState
}

void ALNPEnemyCharacter::SyncToEntity(float& OutHealth)
{
	// TODO: Get Health from ASC or local variables
	OutHealth = 100.0f; // Placeholder
}
