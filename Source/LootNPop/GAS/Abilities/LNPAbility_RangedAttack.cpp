// Copyright (c) 2026 LootNPop. All rights reserved.

#include "GAS/Abilities/LNPAbility_RangedAttack.h"
#include "Item/LNPWeaponData.h"
#include "HitDetection/LNPProjectileMassTypes.h"
#include "HitDetection/LNPGhostProjectileSubsystem.h"
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
#include "GameFramework/PlayerState.h"
#include "GameplayPrediction.h"


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

	SpawnProjectile(ActivationInfo);
	Character->PlayMontage(TAG_Montage_Situation_Attack);

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void ULNPAbility_RangedAttack::SpawnProjectile(const FGameplayAbilityActivationInfo& ActivationInfo) const
{
	ALNPCharacterBase* Character = GetOwningCharacter();
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
	SharedData.PoiseDamage             = PoiseDamage;

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

	// --- 네트워크 예측 식별자 (Ghost Projectile 재조정용, 섹션 5.2 참조) ---
	const bool  bIsLocalInstigator = Character->IsLocallyControlled();
	const int32 InstigatorPlayerID = Character->GetPlayerState<APlayerState>()
		? Character->GetPlayerState<APlayerState>()->GetPlayerId() : INDEX_NONE;

	// 예측 발사는 FPredictionKey를 그대로 쓴다 (공격자 클라·서버가 같은 값을 공유).
	// 예측 키가 없는 발사(리슨 호스트·NPC — 키가 0)는 서버 발급 SalvoID로 대체해 전역 고유성을 확보한다.
	int32 KeyOrSalvo = static_cast<int32>(ActivationInfo.GetActivationPredictionKey().Current);
	if (KeyOrSalvo == 0 && Character->HasAuthority())
		KeyOrSalvo = ULNPGhostProjectileSubsystem::IssueServerSalvoID();

	// 발사 시점의 공격자 RTT/2 — 서버에서는 Lag Compensation 캐싱과 관전자 Dead Reckoning 업스트림 지연에 공용.
	float AttackerHalfRTT = 0.f;
	if (Character->HasAuthority())
	{
		if (const APlayerState* AttackerPS = Character->GetPlayerState<APlayerState>())
			AttackerHalfRTT = FMath::Clamp(AttackerPS->GetPingInMilliseconds() * 0.0005f, 0.f, 0.2f);
	}

	// 서버 거부 판정을 예측 중인 원격 클라이언트에서만 Ghost를 등록한다.
	// Standalone/리슨서버 호스트는 IsPredictingClient()==false — 엔티티가 단 하나뿐이라 Ghost가 불필요하다.
	const bool bShouldRegisterGhost = bIsLocalInstigator && IsPredictingClient();
	ULNPGhostProjectileSubsystem* GhostSub = bShouldRegisterGhost ? World->GetSubsystem<ULNPGhostProjectileSubsystem>() : nullptr;
	if (GhostSub)
	{
		// FPredictionKeyDelegates::NewRejectedDelegate(정적 오버로드)는 모듈 밖에 노출되지 않아(UE_API 누락),
		// 로컬 사본을 만들어 멤버 버전(FPredictionKey::NewRejectedDelegate, 노출됨)을 사용한다.
		FPredictionKey RejectKey = ActivationInfo.GetActivationPredictionKey();
		RejectKey.NewRejectedDelegate().BindUObject(GhostSub, &ULNPGhostProjectileSubsystem::DestroyAllGhostsForKey, InstigatorPlayerID, KeyOrSalvo);
	}

	// --- 파생 클래스가 발사 방향 배열을 제공 (단일 / 방사형 등) ---
	const TArray<FVector> Directions = GetFireDirections(SpawnPos);

	// --- 시뮬레이티드 프록시(구경꾼) 가시성 — 서버가 전 클라이언트에 발사 시점 1회 방송 (섹션 5.2 "제3자 가시성") ---
	if (Character->HasAuthority())
	{
		TArray<FVector> Velocities;
		Velocities.Reserve(Directions.Num());
		for (const FVector& Dir : Directions)
			Velocities.Add(Dir * WeaponDef->ProjectileSpeed);

		Character->Multicast_SpawnGhostProjectiles(SharedData, SpawnPos, Velocities, WeaponDef->ProjectileLifetime,
			Team, KeyOrSalvo, InstigatorPlayerID, AttackerHalfRTT);
	}

	uint8 SpawnIndex = 0;
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
		FragData.bIsLocalInstigator = bIsLocalInstigator;
		FragData.InstigatorPlayerID = InstigatorPlayerID;
		FragData.PredictionKeyID    = KeyOrSalvo;
		FragData.SpawnIndex         = SpawnIndex;
		FragData.CachedRewindSeconds = AttackerHalfRTT; // 서버 전용 — 발사 시점 1회 캐싱 (섹션 5.0)

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

		if (GhostSub)
			GhostSub->RegisterGhost(Entity, { InstigatorPlayerID, KeyOrSalvo, SpawnIndex }, WeaponDef->ProjectileLifetime);

		++SpawnIndex;
	}
}

TArray<FVector> ULNPAbility_RangedAttack::GetFireDirections(const FVector& SpawnPos) const
{
	const ALNPCharacterBase* Character = GetOwningCharacter();
	if (nullptr == Character)
		return {};

	static constexpr float MinAimDistanceSq = 150.f * 150.f;

	// 컨트롤러가 없는 사수(적 NPC)의 기본 조준선. GetBaseAimRotation은 액터 전방에
	// 상하 조준 Pitch를 얹어 돌려주므로, 예전의 GetActorForwardVector()를 그대로 일반화한 값이다.
	const FVector FallbackDirection = Character->GetBaseAimRotation().Vector();
	FVector       Direction         = FallbackDirection;

	// 플레이어 폰: 조준점은 **모든 머신이 같은 InputCmd 값**을 읽는다. 로컬/원격으로 분기해
	// 각자 계산하면 서버 판정과 클라이언트 예측이 그 시차만큼 갈라지고, 총구와 카메라가
	// 떨어져 있으므로 그 오차는 거리와 무관한 상수로 남는다 — 게스트가 조준선대로 맞혀도
	// 서버 판정이 나지 않던 원인이 정확히 이것이었다.
	FVector AimTarget;
	if (Character->GetAimTargetLocation(AimTarget)
		&& MinAimDistanceSq <= FVector::DistSquared(SpawnPos, AimTarget))
	{
		const FVector Converged = (AimTarget - SpawnPos).GetSafeNormal();

		// 조준점은 소유 클라이언트가 만든 값이다. 다만 시선 회전(ControlRotation)도 이미 같은
		// 클라이언트가 보내는 값이라 새로 생기는 권위는 없고, 여기서 메우는 것은 총구-카메라
		// 시차뿐이므로 몇 도면 충분하다. 그 범위를 넘으면 조작으로 보고 시선 회전으로 되돌린다.
		static constexpr float MaxAimCorrectionCos = 0.9659258f; // cos(15°)
		if (MaxAimCorrectionCos <= FVector::DotProduct(Converged, FallbackDirection))
		{
			Direction = Converged;
		}
	}

	return { Direction };
}
