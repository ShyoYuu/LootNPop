// Copyright (c) 2026 LootNPop. All rights reserved.

#include "GAS/Abilities/LNPAbility_RangedAttack.h"
#include "GAS/Effects/LNPGameplayEffect_Cooldown.h"
#include "Item/LNPWeaponData.h"
#include "HitDetection/LNPProjectileMassTypes.h"
#include "Character/LNPCharacterBase.h"
#include "MassEntitySubsystem.h"
#include "MassEntityManager.h"
#include "MassAgentComponent.h"
#include "MassCommonFragments.h"
#include "MassCommands.h"

void ULNPAbility_RangedAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (false == CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("ULNPAbility_RangedAttack: Activated by %s"), *GetOwningActorFromActorInfo()->GetName());

	SpawnProjectile();
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

UGameplayEffect* ULNPAbility_RangedAttack::GetCooldownGameplayEffect() const
{
	return ULNPGameplayEffect_Cooldown::StaticClass()->GetDefaultObject<UGameplayEffect>();
}

void ULNPAbility_RangedAttack::ApplyCooldown(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo) const
{
	const ULNPWeaponData* WeaponDef = GetEquippedWeaponDef();
	if (!WeaponDef || WeaponDef->FireCooldown <= 0.f)
		return;

	FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(
		Handle, ActorInfo, ActivationInfo,
		ULNPGameplayEffect_Cooldown::StaticClass(), GetAbilityLevel());

	if (!SpecHandle.IsValid())
		return;

	SpecHandle.Data->Duration = WeaponDef->FireCooldown;
	ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, SpecHandle);
}

void ULNPAbility_RangedAttack::SpawnProjectile() const
{
	const ALNPCharacterBase* Character = GetOwningCharacter();
	if (nullptr == Character)
		return;

	const ULNPWeaponData* WeaponDef = GetEquippedWeaponDef();
	if (nullptr == WeaponDef)
		return;

	UWorld* World = Character->GetWorld();
	if (nullptr == World)
		return;

	UMassEntitySubsystem* MassSubsystem = World->GetSubsystem<UMassEntitySubsystem>();
	if (nullptr == MassSubsystem)
		return;

	FMassEntityManager& EntityManager = MassSubsystem->GetMutableEntityManager();

	// --- Shared fragment (weapon-type constants, shared across all projectiles of this weapon) ---
	FLNPProjectileSharedFragment SharedData;
	SharedData.VFXData     = WeaponDef->ProjectileVFXData;
	SharedData.Type        = WeaponDef->ProjectileType;
	SharedData.Damage      = WeaponDef->ProjectileDamage;
	SharedData.HitRadiusSq = FMath::Square(WeaponDef->ProjectileHitRadius);

	FConstSharedStruct SharedStruct = EntityManager.GetOrCreateConstSharedFragment(SharedData);
	FMassArchetypeSharedFragmentValues SharedValues;
	SharedValues.Add(SharedStruct);

	// --- Per-entity state ---
	const FVector SpawnPos  = Character->GetActorLocation()
		+ Character->GetActorForwardVector() * WeaponDef->MuzzleOffset.X;
	const FVector Direction = Character->GetActorForwardVector();

	FMassEntityHandle InstigatorHandle;
	if (const UMassAgentComponent* AgentComp = Character->FindComponentByClass<UMassAgentComponent>())
		InstigatorHandle = AgentComp->GetEntityHandle();

	// ReserveEntity() is safe during Mass processing (just allocates an ID, no IsProcessing check).
	// The actual entity build is deferred so it executes after the current processing scope ends.
	const FMassEntityHandle Entity = EntityManager.ReserveEntity();

	FLNPProjectileFragment FragData;
	FragData.PreviousPos       = SpawnPos;
	FragData.SpawnLocation     = SpawnPos;
	FragData.Velocity          = Direction * WeaponDef->ProjectileSpeed;
	FragData.LifetimeRemaining = WeaponDef->ProjectileLifetime;
	FragData.Instigator        = InstigatorHandle;

	FLNPProjectileVisualFragment VisualFrag; // bInitialized = false by default

	FTransformFragment TransFrag;
	TransFrag.GetMutableTransform().SetLocation(SpawnPos);

	EntityManager.Defer().PushCommand<FMassCommandBuildEntityWithSharedFragments<
		FMassArchetypeSharedFragmentValues,
		FLNPProjectileFragment,
		FLNPProjectileVisualFragment,
		FTransformFragment>>(
		Entity,
		MoveTemp(SharedValues),
		FragData,
		VisualFrag,
		TransFrag);

	UE_LOG(LogTemp, Log, TEXT("ULNPAbility_RangedAttack: Spawned projectile entity %d at %s with velocity %s"),
		Entity.Index, *SpawnPos.ToString(), *FragData.Velocity.ToString());
}
