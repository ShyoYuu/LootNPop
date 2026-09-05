// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Enemy/LNPEntityAttackProcessor.h"
#include "Enemy/LNPEnemyMassTypes.h"
#include "Enemy/LNPEnemyConfig.h"
#include "GAS/LNPPoiseTypes.h"
#include "HitDetection/LNPProjectileMassTypes.h"
#include "HitDetection/LNPWeaponTraceMassTypes.h"
#include "HitDetection/LNPGhostProjectileSubsystem.h"
#include "HitDetection/LNPSpreadPattern.h"
#include "Item/LNPWeaponData.h"
#include "LNPMassUtils.h"

#include "MassCommonFragments.h"
#include "MassCommonTypes.h"
#include "MassCommands.h"
#include "MassExecutionContext.h"


namespace
{
	/**
	 * 구면 규약의 로컬 기저. 기준점은 **캡슐 중심**이고(발밑이 아니다),
	 * 구 내벽이므로 머리 방향(Up)은 구 중심을 향한다 — Mass 판정 경로 전체가 쓰는 관행과 같다.
	 */
	struct FLNPEntityBasis
	{
		FVector Center = FVector::ZeroVector;
		FVector Up      = FVector::UpVector;
		FVector Forward = FVector::ForwardVector;
		FVector Right   = FVector::RightVector;
	};

	FLNPEntityBasis MakeEntityBasis(const FTransform& Transform)
	{
		FLNPEntityBasis Basis;
		Basis.Center = Transform.GetLocation();
		Basis.Up     = (-Basis.Center).GetSafeNormal();

		// 전방은 접평면에 투영해야 한다 — 구면에서 액터 전방은 표면과 미세하게 어긋나 있다.
		const FVector RawForward = Transform.GetUnitAxis(EAxis::X);
		Basis.Forward = (RawForward - Basis.Up * FVector::DotProduct(RawForward, Basis.Up)).GetSafeNormal();
		Basis.Right   = FVector::CrossProduct(Basis.Up, Basis.Forward);
		return Basis;
	}

	/**
	 * 접평면 위 방향을 Yaw로 돌리고 그만큼 위아래로 기울인다.
	 * 기울임 축을 고정 Right로 두면 Yaw가 ±90°에 가까울 때 축과 방향이 겹쳐 회전이 사라지므로,
	 * 접평면 성분을 만든 뒤 Up 쪽으로 들어 올리는 방식으로 계산한다.
	 */
	FVector MakeTangentDirection(const FLNPEntityBasis& Basis, const float YawDeg, const float PitchDeg)
	{
		const float YawRad   = FMath::DegreesToRadians(YawDeg);
		const float PitchRad = FMath::DegreesToRadians(PitchDeg);

		const FVector Tangent = Basis.Forward * FMath::Cos(YawRad) + Basis.Right * FMath::Sin(YawRad);
		return (Tangent * FMath::Cos(PitchRad) + Basis.Up * FMath::Sin(PitchRad)).GetSafeNormal();
	}

	/** 칼밑·칼끝 한 쌍. 2패스 사이를 건너는 유일한 데이터다. */
	struct FLNPBladePoints
	{
		FVector Root = FVector::ZeroVector;
		FVector Tip  = FVector::ZeroVector;
	};

	/** 가상 칼날의 현재 4점 중 Curr 두 점. t는 Active 구간의 진행률(0~1). */
	FLNPBladePoints ComputeBladePoints(const FLNPEntityBasis& Basis, const FLNPEntityAttackConfig& Config, const float T)
	{
		const FVector Pivot = Basis.Center
			+ Basis.Forward * Config.PivotForward
			+ Basis.Up      * Config.PivotUp;

		const float   YawDeg = FMath::Lerp(Config.ArcStartDeg, Config.ArcEndDeg, T);
		const FVector Dir    = MakeTangentDirection(Basis, YawDeg, Config.ArcPitchDeg);

		FLNPBladePoints Points;
		Points.Root = Pivot + Dir * Config.BladeInner;
		Points.Tip  = Pivot + Dir * Config.BladeOuter;
		return Points;
	}
}

ULNPEntityAttackProcessor::ULNPEntityAttackProcessor()
	: AttackQuery(*this), SwingQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;
	// 발사체 스폰이 GetOrCreateConstSharedFragment를 타므로 게임 스레드에서 돈다 (헤더 주석).
	bRequiresGameThreadExecution = true;
	// StateTree Task가 세운 요청을 같은 프레임에 소비하려면 UMassStateTreeProcessor보다 뒤여야 한다.
	// 엔진이 그 프로세서를 Behavior 그룹에 두고 Tasks 그룹보다 앞서도록 못 박아 두었으므로,
	// Tasks 그룹에 들어가는 것으로 순서가 보장된다.
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Tasks;
}

void ULNPEntityAttackProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	AttackQuery.AddRequirement<FLNPEntityAttackFragment>(EMassFragmentAccess::ReadWrite);
	AttackQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	AttackQuery.AddRequirement<FLNPEnemyTargetingFragment>(EMassFragmentAccess::ReadOnly);
	AttackQuery.AddRequirement<FLNPPoiseFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional); // 경직 중 공격 중단
	AttackQuery.AddConstSharedRequirement<FLNPEnemySharedFragment>();
	AttackQuery.AddTagRequirement<FLNPEnemyTag>(EMassFragmentPresence::All);
	AttackQuery.AddTagRequirement<FLNPEnemyDyingTag>(EMassFragmentPresence::None);
	AttackQuery.RegisterWithProcessor(*this);

	SwingQuery.AddRequirement<FLNPWeaponTraceFragment>(EMassFragmentAccess::ReadWrite);
	SwingQuery.AddRequirement<FLNPEntitySwingFragment>(EMassFragmentAccess::ReadOnly);
	SwingQuery.RegisterWithProcessor(*this);
}

void ULNPEntityAttackProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	// 순수 엔티티 공격은 서버 권위다 — 예측 대상이 아니므로 클라이언트에서는 아무것도 하지 않는다.
	if (LNPMass::IsClientWorld(EntityManager))
		return;

	const float DeltaTime = Context.GetDeltaTimeSeconds();

	// Pass 1이 계산해 Pass 2가 반영한다. 칼날은 적과 다른 엔티티라 이 자리를 거치지 않고는
	// 서로의 프래그먼트에 닿을 수 없다 (헤더의 SwingQuery 주석).
	TMap<FMassEntityHandle, FLNPBladePoints> PendingBladePoints;

	AttackQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& Ctx)
	{
		const ULNPEnemyConfig* Config = Ctx.GetConstSharedFragment<FLNPEnemySharedFragment>().Config;
		if (Config == nullptr || Config->CombatMode != ELNPEnemyCombatMode::PureEntity)
			return;

		const FLNPEntityAttackConfig& AttackConfig = Config->EntityAttackConfig;
		const FLNPEnemyMovementConfig& MoveConfig  = Config->MovementConfig;
		const bool bIsRanged = Config->AttackType == ELNPEnemyAttackType::Ranged;
		const ULNPWeaponData* WeaponDef = Config->WeaponData;

		// 발사체 무기 상수는 chunk 공용이므로 공유 프래그먼트를 청크당 1회만 만든다.
		FMassArchetypeSharedFragmentValues ProjectileSharedValues;
		if (bIsRanged && WeaponDef)
		{
			FLNPProjectileSharedFragment SharedData;
			SharedData.VFXData           = WeaponDef->ProjectileVFXData;
			SharedData.DamageEffectClass = WeaponDef->ProjectileDamageEffect;
			SharedData.Type              = WeaponDef->ProjectileType;
			SharedData.HitRadius         = WeaponDef->HitRadius;
			SharedData.ExplosionRadius   = WeaponDef->ExplosionRadius;
			// 어빌리티 인스턴스가 공급하던 값들만 EntityAttackConfig로 대체된다.
			SharedData.Damage            = AttackConfig.Damage;
			SharedData.ParryRadius       = AttackConfig.ParryRadius;
			SharedData.KnockbackStrength = AttackConfig.KnockbackStrength;
			SharedData.PoiseDamage       = AttackConfig.PoiseDamage;

			ProjectileSharedValues.Add(EntityManager.GetOrCreateConstSharedFragment(SharedData));
		}

		const TArrayView<FLNPEntityAttackFragment> Attacks              = Ctx.GetMutableFragmentView<FLNPEntityAttackFragment>();
		const TConstArrayView<FTransformFragment> Transforms            = Ctx.GetFragmentView<FTransformFragment>();
		const TConstArrayView<FLNPEnemyTargetingFragment> TargetingFrags = Ctx.GetFragmentView<FLNPEnemyTargetingFragment>();
		const TConstArrayView<FLNPPoiseFragment> PoiseFrags             = Ctx.GetFragmentView<FLNPPoiseFragment>();

		// 발사마다 새로 할당하지 않도록 청크 단위로 재사용한다 (BuildHexRingDirections가 Reset한다).
		TArray<FVector> FireDirections;

		// Active 종료·경직·타겟 상실 어디서 끊기든 칼날을 반드시 회수한다.
		// TimeToLive는 이 경로를 놓쳤을 때의 그물이지 1차 방어선이 아니다.
		auto DestroySwing = [&Ctx](FLNPEntityAttackFragment& Attack)
		{
			if (Attack.SwingEntity.IsSet())
			{
				Ctx.Defer().PushCommand<FMassCommandDestroyEntities>(Attack.SwingEntity);
				Attack.SwingEntity = FMassEntityHandle();
			}
		};

		for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
		{
			FLNPEntityAttackFragment& Attack = Attacks[i];

			// 요청은 1프레임짜리다 — Task가 원하는 동안 매 프레임 다시 세운다.
			// 소비하지 않고 남겨 두면 Attack 상태를 벗어난 뒤 쿨다운이 풀리는 순간 헛스윙이 나간다.
			const bool bRequested = Attack.bAttackRequested != 0;
			Attack.bAttackRequested = 0;

			if (Attack.CooldownRemaining > 0.f)
				Attack.CooldownRemaining = FMath::Max(0.f, Attack.CooldownRemaining - DeltaTime);

			// 경직·다운 중에는 공격이 끊긴다. Actor 경로에서는 FLNPStaggerCommand::Run이
			// CancelCurrentAttackAbility()로 끊지만 그 함수는 Actor가 없으면 도달하지 못한다 —
			// 다운은 게이지를 0으로 리셋하므로 bIsGroggy만으로는 안 잡혀 면역 잔여도 함께 본다.
			const bool bDisabled = PoiseFrags.IsValidIndex(i)
				&& (PoiseFrags[i].bIsGroggy || PoiseFrags[i].ImmunityTimeRemaining > 0.f);

			if (bDisabled)
			{
				DestroySwing(Attack);
				Attack.Phase        = ELNPEntityAttackPhase::None;
				Attack.PhaseElapsed = 0.f;
				continue;
			}

			if (Attack.Phase == ELNPEntityAttackPhase::None)
			{
				if (bRequested && Attack.CooldownRemaining <= 0.f)
				{
					Attack.Phase        = ELNPEntityAttackPhase::Windup;
					Attack.PhaseElapsed = 0.f;
				}
				continue;
			}

			// 선딜 중에 타겟을 잃으면 취소한다. Active 이후는 끝까지 재생한다 — 헛스윙이 자연스럽다.
			if (Attack.Phase == ELNPEntityAttackPhase::Windup
				&& TargetingFrags[i].State != ELNPTargetingState::Confirmed)
			{
				Attack.Phase        = ELNPEntityAttackPhase::None;
				Attack.PhaseElapsed = 0.f;
				continue;
			}

			Attack.PhaseElapsed += DeltaTime;

			const FLNPEntityBasis Basis = MakeEntityBasis(Transforms[i].GetTransform());

			switch (Attack.Phase)
			{
			case ELNPEntityAttackPhase::Windup:
			{
				if (Attack.PhaseElapsed < AttackConfig.WindupTime)
					break;

				// 원거리는 선딜이 끝나는 순간 1회 발사한다. ActiveTime은 근접 전용이다.
				if (bIsRanged && WeaponDef)
				{
					const FVector Muzzle = Basis.Center
						+ Basis.Forward * AttackConfig.MuzzleLocalOffset.X
						+ Basis.Right   * AttackConfig.MuzzleLocalOffset.Y
						+ Basis.Up      * AttackConfig.MuzzleLocalOffset.Z;

					// 조준선은 Actor 경로의 GetBaseAimRotation()과 같은 규약이다 —
					// 수평은 몸이 향한 접평면 전방, 상하만 타겟에서 뽑아 가용 각도로 클램프한다.
					// 클램프 값이 조준 자세·발사 방향·피격 인지 게이트의 공용 원본이므로 여기서도 그 값을 읽는다.
					const FVector ToTarget   = TargetingFrags[i].TargetLocation - Muzzle;
					const float   VerticalUp = FVector::DotProduct(ToTarget, Basis.Up);
					const float   Horizontal = (ToTarget - Basis.Up * VerticalUp).Size();
					const float   PitchDeg   = FMath::Clamp(
						FMath::RadiansToDegrees(FMath::Atan2(VerticalUp, Horizontal)),
						MoveConfig.AimPitchMinDeg, MoveConfig.AimPitchMaxDeg);

					const FVector FireDir = MakeTangentDirection(Basis, 0.f, PitchDeg);

					// 산탄 배치는 어빌리티 경로와 같은 공식을 쓴다 (LNPSpreadPattern.h).
					// HexRingCount가 0이면 중앙 1발만 나오므로 단발도 이 경로로 수렴한다.
					LNPSpread::BuildHexRingDirections(FireDir, Basis.Up, Basis.Forward,
						AttackConfig.HexRingCount, AttackConfig.HexStepDegrees, FireDirections);

					// SalvoID는 **한 번의 발사에 하나**다. 펠릿 구분은 SpawnIndex가 맡는다 —
					// Ghost 식별자가 {PlayerID, KeyOrSalvo, SpawnIndex} 조합이라 펠릿마다 키를 새로
					// 발급하면 같은 발사의 펠릿들이 서로 다른 발사로 보인다.
					const int32 SalvoID = ULNPGhostProjectileSubsystem::IssueServerSalvoID();

					uint8 SpawnIndex = 0;
					for (const FVector& Dir : FireDirections)
					{
						FLNPProjectileFragment ProjFrag;
						ProjFrag.PreviousPos        = Muzzle;
						ProjFrag.SpawnLocation      = Muzzle;
						ProjFrag.Velocity           = Dir * WeaponDef->ProjectileSpeed;
						ProjFrag.LifetimeRemaining  = WeaponDef->ProjectileLifetime;
						ProjFrag.Instigator         = Ctx.GetEntity(i);
						ProjFrag.InstigatorTeam     = ELNPInstigatorTeam::Enemy;
						ProjFrag.bIsLocalInstigator = false;             // 엔티티 NPC는 예측 대상이 아니다
						ProjFrag.InstigatorPlayerID = INDEX_NONE;
						// 예측 키가 없는 발사는 서버 발급 SalvoID로 전역 고유성을 확보한다 (패링 반사 경로와 동일).
						ProjFrag.PredictionKeyID    = SalvoID;
						ProjFrag.SpawnIndex         = SpawnIndex++;
						ProjFrag.CachedRewindSeconds = 0.f;              // 공격자에 PlayerState가 없어 되감기가 없다

						FLNPProjectileVisualFragment VisualFrag;
						FTransformFragment TransFrag;
						TransFrag.GetMutableTransform().SetLocation(Muzzle);

						FMassArchetypeSharedFragmentValues SharedValuesCopy = ProjectileSharedValues;
						Ctx.Defer().PushCommand<FMassCommandBuildEntityWithSharedFragments<
							FMassArchetypeSharedFragmentValues,
							FLNPProjectileFragment,
							FLNPProjectileVisualFragment,
							FTransformFragment>>(
							EntityManager.ReserveEntity(),
							MoveTemp(SharedValuesCopy),
							ProjFrag,
							VisualFrag,
							TransFrag);
					}
				}

				// 근접은 Active 진입과 함께 칼날 엔티티를 만든다. 판정 파이프라인이 요구하는 것은
				// 좌표 4점 + 반경뿐이라, 본·소켓·몽타주 없이도 UANS_LNPMeleeHitWindow와 같은 입력이 된다.
				// ⚠️ 서버에서만 만든다 — ANS가 양쪽에서 만드는 것은 로컬 공격자 예측용이고,
				//    엔티티 NPC는 예측 대상이 아니다 (Execute 첫 줄의 클라이언트 가드).
				if (!bIsRanged)
				{
					const FLNPBladePoints Seed = ComputeBladePoints(Basis, AttackConfig, 0.f);

					FLNPWeaponTraceFragment Blade;
					Blade.HitRadius         = AttackConfig.HitRadius;
					Blade.ParryRadius       = AttackConfig.ParryRadius;
					Blade.Damage            = AttackConfig.Damage;
					Blade.KnockbackStrength = AttackConfig.KnockbackStrength;
					Blade.PoiseDamage       = AttackConfig.PoiseDamage;
					// 프로세서가 종료를 놓쳤을 때(사망으로 쿼리에서 빠지는 경우 등)의 그물.
					Blade.TimeToLive        = AttackConfig.ActiveTime + 0.2f;
					Blade.DamageEffectClass = WeaponDef ? WeaponDef->ProjectileDamageEffect.Get() : nullptr;
					Blade.InstigatorEntity  = Ctx.GetEntity(i);
					Blade.InstigatorActor   = nullptr;
					Blade.InstigatorTeam    = ELNPInstigatorTeam::Enemy;
					Blade.bIsLocalInstigator = false;
					// 첫 프레임은 Prev == Curr다. 판정 프로세서가 선분-선분으로 폴백한다.
					Blade.SwordRootPrev = Blade.SwordRootCurr = Seed.Root;
					Blade.SwordTipPrev  = Blade.SwordTipCurr  = Seed.Tip;

					FLNPEntitySwingFragment SwingFrag;
					SwingFrag.Owner = Ctx.GetEntity(i);

					FTransformFragment SwingTransform;
					SwingTransform.GetMutableTransform().SetLocation(Seed.Tip);

					const FMassEntityHandle SwingEntity = EntityManager.ReserveEntity();
					Ctx.Defer().PushCommand<FMassCommandBuildEntity<
						FLNPWeaponTraceFragment,
						FTransformFragment,
						FLNPEntitySwingFragment>>(
						SwingEntity, Blade, SwingTransform, SwingFrag);

					Attack.SwingEntity = SwingEntity;
				}

				Attack.Phase        = bIsRanged ? ELNPEntityAttackPhase::Recovery : ELNPEntityAttackPhase::Active;
				Attack.PhaseElapsed = 0.f;
				break;
			}

			case ELNPEntityAttackPhase::Active:
			{
				if (Attack.PhaseElapsed >= AttackConfig.ActiveTime)
				{
					DestroySwing(Attack);
					Attack.Phase        = ELNPEntityAttackPhase::Recovery;
					Attack.PhaseElapsed = 0.f;
					break;
				}

				if (Attack.SwingEntity.IsSet())
				{
					const float T = AttackConfig.ActiveTime > 0.f
						? FMath::Clamp(Attack.PhaseElapsed / AttackConfig.ActiveTime, 0.f, 1.f)
						: 1.f;
					PendingBladePoints.Add(Attack.SwingEntity, ComputeBladePoints(Basis, AttackConfig, T));
				}
				break;
			}

			case ELNPEntityAttackPhase::Recovery:
			{
				if (Attack.PhaseElapsed < AttackConfig.RecoveryTime)
					break;

				Attack.Phase             = ELNPEntityAttackPhase::None;
				Attack.PhaseElapsed      = 0.f;
				Attack.CooldownRemaining = MoveConfig.AttackInterval;
				break;
			}

			default:
				break;
			}
		}
	});

	if (PendingBladePoints.IsEmpty())
		return;

	// Pass 2 — 계산된 4점을 칼날 엔티티에 반영한다.
	SwingQuery.ForEachEntityChunk(Context, [&PendingBladePoints](FMassExecutionContext& Ctx)
	{
		const TArrayView<FLNPWeaponTraceFragment> Blades = Ctx.GetMutableFragmentView<FLNPWeaponTraceFragment>();

		for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
		{
			const FLNPBladePoints* Points = PendingBladePoints.Find(Ctx.GetEntity(i));
			if (Points == nullptr)
				continue;

			FLNPWeaponTraceFragment& Blade = Blades[i];
			Blade.SwordRootPrev = Blade.SwordRootCurr;
			Blade.SwordTipPrev  = Blade.SwordTipCurr;
			Blade.SwordRootCurr = Points->Root;
			Blade.SwordTipCurr  = Points->Tip;
		}
	});
}
