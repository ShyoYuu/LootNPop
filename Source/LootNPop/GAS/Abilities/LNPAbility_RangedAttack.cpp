// Copyright (c) 2026 LootNPop. All rights reserved.

#include "GAS/Abilities/LNPAbility_RangedAttack.h"
#include "GAS/Effects/LNPGameplayEffect_Cooldown.h"
#include "Item/LNPWeaponData.h"
#include "HitDetection/LNPProjectileMassTypes.h"
#include "Character/LNPCharacterBase.h"
#include "Enemy/LNPEnemyCharacter.h"
#include "LNPGameplayTags.h"
#include "LootNPop.h"

#include "MassEntitySubsystem.h"
#include "MassEntityManager.h"
#include "MassAgentComponent.h"
#include "MassCommonFragments.h"
#include "MassCommands.h"
#include "GameFramework/PlayerController.h"


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

	const ALNPCharacterBase* Character = GetOwningCharacter();
	if (nullptr == Character)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	SpawnProjectile();
	Character->PlayMontage(TAG_Montage_Situation_Attack);

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

	SpecHandle.Data->SetDuration(WeaponDef->FireCooldown, true);
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

	// --- Shared Fragment (무기 타입 상수, 동일 무기의 모든 Projectile가 공유) ---
	FLNPProjectileSharedFragment SharedData;
	SharedData.VFXData                 = WeaponDef->ProjectileVFXData;
	SharedData.DamageEffectClass       = WeaponDef->ProjectileDamageEffect;
	SharedData.Type                    = WeaponDef->ProjectileType;
	SharedData.Damage                  = ComputeDamage();
	SharedData.HitRadius               = WeaponDef->HitRadius;
	SharedData.ParryRadius             = ParryRadius;
	SharedData.ExplosionRadius         = WeaponDef->ExplosionRadius;
	SharedData.KnockbackStrength       = KnockbackStrength;
	SharedData.SplashKnockbackStrength = SplashKnockbackStrength;

	FConstSharedStruct SharedStruct = EntityManager.GetOrCreateConstSharedFragment(SharedData);
	FMassArchetypeSharedFragmentValues SharedValues;
	SharedValues.Add(SharedStruct);

	// --- 스폰 위치: Muzzle 소켓 (없으면 ActorLocation 폴백) ---
	static const FName MuzzleSocket(TEXT("Muzzle"));
	const USkeletalMeshComponent* WeaponMesh = Character->GetWeaponMesh();
	const FVector BasePos = (WeaponMesh && WeaponMesh->DoesSocketExist(MuzzleSocket))
		? WeaponMesh->GetSocketLocation(MuzzleSocket)
		: Character->GetActorLocation();

	const FVector SpawnPos = BasePos + Character->GetActorTransform().TransformVector(WeaponDef->MuzzleOffset);

	// --- Instigator ---
	FMassEntityHandle InstigatorHandle;
	if (const UMassAgentComponent* AgentComp = Character->FindComponentByClass<UMassAgentComponent>())
		InstigatorHandle = AgentComp->GetEntityHandle();

	const ELNPInstigatorTeam Team = Cast<ALNPEnemyCharacter>(Character)
		? ELNPInstigatorTeam::Enemy
		: ELNPInstigatorTeam::Player;

	// --- 파생 클래스가 발사 방향 배열을 제공 (단일 / 방사형 등) ---
	const TArray<FVector> Directions = GetFireDirections(SpawnPos);

	for (const FVector& Dir : Directions)
	{
		const FMassEntityHandle Entity = EntityManager.ReserveEntity();

		FLNPProjectileFragment FragData;
		FragData.PreviousPos       = SpawnPos;
		FragData.SpawnLocation     = SpawnPos;
		FragData.Velocity          = Dir * WeaponDef->ProjectileSpeed;
		FragData.LifetimeRemaining = WeaponDef->ProjectileLifetime;
		FragData.Instigator        = InstigatorHandle;
		FragData.InstigatorTeam    = Team;

		FLNPProjectileVisualFragment VisualFrag;
		FTransformFragment TransFrag;
		TransFrag.GetMutableTransform().SetLocation(SpawnPos);

		// Add()는 &&만 받으므로 루프마다 복사본을 생성해 MoveTemp로 전달
		FMassArchetypeSharedFragmentValues SharedValuesCopy = SharedValues;
		EntityManager.Defer().PushCommand<FMassCommandBuildEntityWithSharedFragments<
			FMassArchetypeSharedFragmentValues,
			FLNPProjectileFragment,
			FLNPProjectileVisualFragment,
			FTransformFragment>>(
			Entity,
			MoveTemp(SharedValuesCopy),
			FragData,
			VisualFrag,
			TransFrag);
	}
}

TArray<FVector> ULNPAbility_RangedAttack::GetFireDirections(const FVector& SpawnPos) const
{
	const ALNPCharacterBase* Character = GetOwningCharacter();
	if (nullptr == Character)
		return {};

	static constexpr float MinAimDistanceSq = 150.f * 150.f;
	static constexpr float AimTraceDistance = 50000.f;

	FVector Direction = Character->GetActorForwardVector();

	if (const APlayerController* PC = Cast<APlayerController>(Character->GetController()))
	{
		UWorld* World = Character->GetWorld();
		FVector CamPos;
		FRotator CamRot;
		PC->GetPlayerViewPoint(CamPos, CamRot);

		const FVector TraceEnd = CamPos + CamRot.Vector() * AimTraceDistance;

		FHitResult Hit;
		FCollisionQueryParams QueryParams;
		QueryParams.AddIgnoredActor(Character);

		const FVector AimTarget = World->LineTraceSingleByChannel(Hit, CamPos, TraceEnd, ECC_Visibility, QueryParams)
			? Hit.ImpactPoint
			: TraceEnd;

		if (MinAimDistanceSq <= FVector::DistSquared(SpawnPos, AimTarget))
		{
			Direction = (AimTarget - SpawnPos).GetSafeNormal();
		}
	}

	return { Direction };
}
