// Fill out your copyright notice in the Description page of Project Settings.

#include "Enemy/LNPEnemyCharacter.h"
#include "Enemy/LNPEnemyConfig.h"
#include "Enemy/LNPEnemyMassTypes.h"
#include "GAS/Abilities/LNPGameplayAbility.h"
#include "Movement/LNPCharacterMoverComponent.h"

#include "GAS/Attributes/LNPBaseAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "DefaultMovementSet/LayeredMoves/LaunchMove.h"
#include "Components/WidgetComponent.h"
#include "UI/LNPHpBarWidget.h"

ALNPEnemyCharacter::ALNPEnemyCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;

	ASC = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("ASC"));
	ASC->SetIsReplicated(false);

	AttributeSet = CreateDefaultSubobject<ULNPBaseAttributeSet>(TEXT("AttributeSet"));

	HpBarComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("HpBarComponent"));
	HpBarComponent->SetupAttachment(RootComponent);
	HpBarComponent->SetVisibility(false);
}

UAbilitySystemComponent* ALNPEnemyCharacter::GetAbilitySystemComponent() const
{
	return ASC;
}

void ALNPEnemyCharacter::BeginPlay()
{
	Super::BeginPlay();
	if (ASC)
	{
		ASC->InitAbilityActorInfo(this, this);

		if (HpBarWidgetClass)
		{
			HpBarComponent->SetWidgetClass(HpBarWidgetClass);
			ASC->GetGameplayAttributeValueChangeDelegate(ULNPBaseAttributeSet::GetHealthAttribute())
				.AddUObject(this, &ALNPEnemyCharacter::OnHpAttributeChanged);
		}
	}
}

void ALNPEnemyCharacter::InitializeFromConfig(ULNPEnemyConfig* InConfig)
{
	if (nullptr == InConfig)
		return;

	EnemyConfig = InConfig;

	// Setup GAS (Abilities & Attributes)
	if (UAbilitySystemComponent* EnemyASC = GetAbilitySystemComponent())
	{
		// Grant weapon abilities from WeaponData; first handle becomes the attack handle
		if (InConfig->WeaponData)
		{
			for (const TSubclassOf<ULNPGameplayAbility>& AbilityClass : InConfig->WeaponData->AbilitiesToGrant)
			{
				if (AbilityClass)
				{
					FGameplayAbilitySpecHandle Handle = EnemyASC->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1, INDEX_NONE, this));
					if (!WeaponAbilityHandle.IsValid())
						WeaponAbilityHandle = Handle;
				}
			}
		}

		// Grant additional non-weapon abilities (dodge, block, etc.)
		for (const TSubclassOf<UGameplayAbility>& AbilityClass : InConfig->DefaultAbilities)
		{
			if (AbilityClass)
				EnemyASC->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1, INDEX_NONE, this));
		}

		// Apply Initial Attributes
		for (auto& AttributePair : InConfig->InitialAttributeValues)
		{
			// Note: In a real project, you would map GameplayTags to actual Attribute accessors.
			// For now, we assume a generic way to set them or handle via Effect.
		}
	}
}

void ALNPEnemyCharacter::SyncFromEntity(FMassEntityHandle InEntityHandle, float InHealth, ELNPTargetingState InTargetingState, FVector InVelocity)
{
	if (AttributeSet)
		AttributeSet->SetHealth(InHealth);

	if (!InVelocity.IsNearlyZero() && MoverComponent)
	{
		TSharedPtr<FLayeredMove_Launch> LaunchMove = MakeShared<FLayeredMove_Launch>();
		LaunchMove->LaunchVelocity = InVelocity;
		LaunchMove->DurationMs = 0.f;
		LaunchMove->MixMode = EMoveMixMode::OverrideVelocity;
		LaunchMove->ForceMovementMode = FName(TEXT("AsyncFalling"));
		MoverComponent->QueueLayeredMove(LaunchMove);
	}

	RefreshHpBar(InHealth, AttributeSet ? AttributeSet->GetMaxHealth() : 0.f);
}

void ALNPEnemyCharacter::TriggerRagdoll()
{
	if (VisualMesh)
	{
		VisualMesh->SetAllBodiesSimulatePhysics(true);
		VisualMesh->SetCollisionProfileName(TEXT("Ragdoll"));
		VisualMesh->WakeAllRigidBodies();
	}
	if (CapsuleComponent)
		CapsuleComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

bool ALNPEnemyCharacter::TryActivateAttack()
{
	if (!WeaponAbilityHandle.IsValid() || !ASC)
		return false;

	return ASC->TryActivateAbility(WeaponAbilityHandle);
}

const ULNPWeaponData* ALNPEnemyCharacter::GetActiveWeaponDef() const
{
	return EnemyConfig ? EnemyConfig->WeaponData.Get() : nullptr;
}

void ALNPEnemyCharacter::SyncToEntity(float& OutHealth, FVector& OutVelocity) const
{
	OutHealth = AttributeSet ? AttributeSet->GetHealth() : 0.f;
	OutVelocity = (MoverComponent && MoverComponent->IsAirborne())
		? MoverComponent->GetVelocity()
		: FVector::ZeroVector;
}

void ALNPEnemyCharacter::OnHpAttributeChanged(const FOnAttributeChangeData& Data)
{
	RefreshHpBar(Data.NewValue, AttributeSet ? AttributeSet->GetMaxHealth() : 0.f);
}

void ALNPEnemyCharacter::RefreshHpBar(float Current, float Max)
{
	const bool bShouldShow = Current > 0.f && Max > 0.f && Current < Max;
	HpBarComponent->SetVisibility(bShouldShow);

	if (bShouldShow)
	{
		if (auto* Widget = Cast<ULNPHpBarWidget>(HpBarComponent->GetWidget()))
			Widget->UpdateHpBar(Current, Max);
	}
}
