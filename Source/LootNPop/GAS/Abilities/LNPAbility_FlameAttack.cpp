// Copyright (c) 2026 LootNPop. All rights reserved.

#include "GAS/Abilities/LNPAbility_FlameAttack.h"
#include "GAS/Effects/LNPGameplayEffect_FlameFiring.h"
#include "GAS/Abilities/LNPAttackInputTargetData.h"
#include "HitDetection/LNPFlameConeMassTypes.h"
#include "HitDetection/LNPFireGeometry.h"
#include "HitDetection/LNPProjectileMotion.h"
#include "HitDetection/LNPPositionHistoryFragment.h"   // MaxPingRewindSeconds
#include "SurfaceNavigation/LNPMassWorldCollision.h"
#include "Character/LNPCharacterBase.h"
#include "Item/LNPWeaponData.h"
#include "Player/LNPTeam.h"
#include "LNPGameplayTags.h"
#include "AbilitySystemComponent.h"

#include "MassEntitySubsystem.h"
#include "MassEntityManager.h"
#include "MassAgentComponent.h"
#include "MassCommands.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/PlayerState.h"

namespace LNPFlameAttack
{
	static TAutoConsoleVariable<int32> CVarDebugDraw(
		TEXT("LNP.Flame.DebugDraw"), 0,
		TEXT("Draw the server-side flame cone (clipped range) for each tick. 0: off, 1: on"),
		ECVF_Cheat);
}

ULNPAbility_FlameAttack::ULNPAbility_FlameAttack()
{
	// 임시 수치 — 틱 0.1초라 넉백은 없애고 경직·계수는 1발 기준보다 작게 둔다.
	KnockbackStrength     = 0.f;
	PoiseDamage           = 3.f;
	BaseDamageCoefficient = 0.6f;
}

void ULNPAbility_FlameAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
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

	// 분사 상태 갱신은 서버만 — 스택 갱신형 GE는 예측 적용이 막혀 있고, 이 태그를 읽는 회복도 서버에서만 돈다.
	const ULNPWeaponData* WeaponDef = GetEquippedWeaponDef();
	if (WeaponDef && HasAuthority(&ActivationInfo))
	{
		FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(
			Handle, ActorInfo, ActivationInfo, ULNPGameplayEffect_FlameFiring::StaticClass(), GetAbilityLevel(Handle, ActorInfo));
		if (SpecHandle.IsValid())
		{
			SpecHandle.Data->SetDuration(WeaponDef->MagazineRegenDelay, true);
			// 예측 키 없이 적용한다 — 키를 실으면 소유 클라가 복제본을 "자기가 예측한 것"으로 취급해 큐 이벤트를 건너뛴다.
			ActorInfo->AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
		}

		SpawnConeJudge(ActivationInfo, TriggerEventData);
	}
	else if (WeaponDef && IsLocallyControlled())
	{
		// 게스트 소유자 화면의 분사 VFX. 위 GE의 큐는 복제를 기다려 RTT만큼 늦게 켜지고 늦게 꺼지므로,
		// 이 머신에서만 Executed 펄스를 쏘고 큐 액터가 펄스가 끊긴 뒤 RegenDelay(= GE 지속시간)만큼 유지한다.
		// 복제되지 않는 로컬 호출이라 대역폭 비용은 없다.
		FGameplayCueParameters CueParams;
		CueParams.RawMagnitude = WeaponDef->MagazineRegenDelay;
		ActorInfo->AbilitySystemComponent->InvokeGameplayCueEvent(TAG_GameplayCue_Weapon_Flame, EGameplayCueEvent::Executed, CueParams);
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void ULNPAbility_FlameAttack::SpawnConeJudge(const FGameplayAbilityActivationInfo& ActivationInfo, const FGameplayEventData* TriggerEventData) const
{
	ALNPCharacterBase* Character = GetOwningCharacter();
	const ULNPWeaponData* WeaponDef = GetEquippedWeaponDef();
	UWorld* World = Character ? Character->GetWorld() : nullptr;
	UMassEntitySubsystem* MassSubsystem = World ? World->GetSubsystem<UMassEntitySubsystem>() : nullptr;
	if (!Character || !WeaponDef || !MassSubsystem)
		return;

	// 원점·축은 원거리 발사와 같은 단일 정의(LNPFireGeometry)를 쓴다. 축은 발동 RPC에 실린 그 틱의 조준 스냅샷이다.
	const FVector Origin = LNPFireGeometry::ResolveMuzzleLocation(*Character, *WeaponDef);
	const FLNPFireAimTargetData* AimInput = LNPAttackInput::Find<FLNPFireAimTargetData>(TriggerEventData);
	FVector Axis;
	if (AimInput && !AimInput->ViewDirection.IsNearlyZero())
	{
		const FVector AimTarget = AimInput->AimTargetLocation;
		Axis = LNPFireGeometry::ResolveAimDirection(Origin, AimInput->ViewDirection.GetSafeNormal(), AimTarget.IsZero() ? nullptr : &AimTarget);
	}
	else
	{
		Axis = LNPFireGeometry::ResolveAimDirection(Origin, Character->GetBaseAimRotation().Vector(), nullptr);
	}

	// 정면 벽에서 사거리를 자른다 — 불길은 벽을 넘지 않는다.
	const FVector FullEnd = Origin + Axis * ConeRange;
	FLNPWorldHit WallHit;
	const FVector End = LNPProjectileMotion::ClipSegmentToWorld(World->GetSubsystem<ULNPMassWorldCollisionSubsystem>(), Origin, FullEnd, WallHit);

	FLNPFlameConeFragment Cone;
	Cone.Origin            = Origin;
	Cone.Axis              = Axis;
	Cone.ConeRange         = ConeRange;
	Cone.Range             = static_cast<float>(FVector::Dist(Origin, End));
	Cone.NearRadius        = NearRadius;
	Cone.FarRadius         = FarRadius;
	Cone.Damage            = ComputeDamage();
	Cone.PoiseDamage       = PoiseDamage;
	Cone.BurnChance         = BurnChance;
	Cone.MaxBurnStacks      = static_cast<uint8>(FMath::Clamp(MaxBurnStacks, 0, MAX_uint8));
	Cone.BurnDuration       = BurnDuration;
	Cone.BurnTickInterval   = BurnTickInterval;
	// 화상 계수는 원뿔 계수와 같은 축(AttackPower × 계수 × 무기 레벨 배율)이다 — 원뿔 피해에서 계수만 갈아 끼운다.
	Cone.BurnDamagePerStack = BaseDamageCoefficient > 0.f ? Cone.Damage * (BurnDamageCoefficient / BaseDamageCoefficient) : 0.f;
	Cone.DamageEffectClass = WeaponDef->ProjectileDamageEffect;
	Cone.InstigatorTeam    = LNPTeam::GetTeam(Character);
	if (const UMassAgentComponent* AgentComp = Character->FindComponentByClass<UMassAgentComponent>())
		Cone.InstigatorEntity = AgentComp->GetEntityHandle();
	if (const APlayerState* AttackerPS = Character->GetPlayerState<APlayerState>())
		Cone.CachedRewindSeconds = FMath::Clamp(AttackerPS->GetPingInMilliseconds() * 0.0005f, 0.f, LNPHitDetection::MaxPingRewindSeconds);

	FMassEntityManager& EntityManager = MassSubsystem->GetMutableEntityManager();
	EntityManager.Defer().PushCommand<FMassCommandBuildEntity<FLNPFlameConeFragment>>(EntityManager.ReserveEntity(), Cone);

#if ENABLE_DRAW_DEBUG
	if (LNPFlameAttack::CVarDebugDraw.GetValueOnGameThread() != 0)
	{
		// 0.1초 틱 간격만큼 남겨 홀드 중 끊기지 않게 보인다. 잘린 끝 단면은 빨강.
		const float Life = FMath::Max(WeaponDef->FireCooldown, 0.05f);
		const float EndRadius = FMath::Lerp(NearRadius, FarRadius, ConeRange > 0.f ? Cone.Range / ConeRange : 0.f);
		FVector Y, Z;
		Axis.FindBestAxisVectors(Y, Z);
		DrawDebugCircle(World, Origin, NearRadius, 16, FColor::Orange, false, Life, 0, 1.f, Y, Z, false);
		DrawDebugCircle(World, End, EndRadius, 24, Cone.Range < ConeRange - 1.f ? FColor::Red : FColor::Orange, false, Life, 0, 1.f, Y, Z, false);
		for (const FVector& Side : { Y, -Y, Z, -Z })
			DrawDebugLine(World, Origin + Side * NearRadius, End + Side * EndRadius, FColor::Orange, false, Life, 0, 1.f);
	}
#endif
}
