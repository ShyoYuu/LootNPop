// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Enemy/LNPEntityAttackProcessor.h"
#include "Enemy/LNPEnemyMassTypes.h"
#include "Enemy/LNPEnemyConfig.h"
#include "Enemy/LNPEntityAttackShared.h"
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
#include "MassReplicationFragments.h"
#include "MassExecutionContext.h"


namespace
{
	/** 칼밑·칼끝 한 쌍. 2패스 사이를 건너는 유일한 데이터다. */
	struct FLNPBladePoints
	{
		FVector Root = FVector::ZeroVector;
		FVector Tip  = FVector::ZeroVector;
	};

	/** 가상 칼날의 현재 4점 중 Curr 두 점. t는 Active 구간의 진행률(0~1). */
	FLNPBladePoints ComputeBladePoints(const LNPEntityAttack::FBasis& Basis, const FLNPEntityAttackConfig& Config, const float T)
	{
		const FVector Pivot = Basis.Center
			+ Basis.Forward * Config.PivotForward
			+ Basis.Up      * Config.PivotUp;

		const float   YawDeg = FMath::Lerp(Config.ArcStartDeg, Config.ArcEndDeg, T);
		const FVector Dir    = LNPEntityAttack::MakeTangentDirection(Basis, YawDeg, Config.ArcPitchDeg);

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
	// 결정론적 Ghost 키의 재료. 둘 다 Optional이다 — NetID 프래그먼트는 복제 트레이트가 붙이므로
	// Standalone(UMassReplicationTrait::BuildTemplate이 조기 반환)에는 아예 없다.
	AttackQuery.AddRequirement<FMassNetworkIDFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	// ReadWrite인 이유: **발사하는 그 순간에** 전이 카운터를 한 번 더 올리고 조준각을 확정한다.
	// 공격 상태 진입(선딜 시작)은 ULNPEnemyActionProcessor가 잡지만, 그 시점에는 아직 조준각이 없다.
	AttackQuery.AddRequirement<FLNPEnemyActionFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
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
			// 게스트의 관전용 Ghost도 **같은 함수**로 이 상수를 만든다 (LNPEntityAttackShared.h).
			ProjectileSharedValues.Add(EntityManager.GetOrCreateConstSharedFragment(
				LNPEntityAttack::MakeProjectileSharedData(*WeaponDef, AttackConfig)));
		}

		const TArrayView<FLNPEntityAttackFragment> Attacks              = Ctx.GetMutableFragmentView<FLNPEntityAttackFragment>();
		const TConstArrayView<FTransformFragment> Transforms            = Ctx.GetFragmentView<FTransformFragment>();
		const TConstArrayView<FLNPEnemyTargetingFragment> TargetingFrags = Ctx.GetFragmentView<FLNPEnemyTargetingFragment>();
		const TConstArrayView<FLNPPoiseFragment> PoiseFrags             = Ctx.GetFragmentView<FLNPPoiseFragment>();
		const TConstArrayView<FMassNetworkIDFragment> NetIDFrags        = Ctx.GetFragmentView<FMassNetworkIDFragment>();
		const TArrayView<FLNPEnemyActionFragment> ActionFrags           = Ctx.GetMutableFragmentView<FLNPEnemyActionFragment>();

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

			const LNPEntityAttack::FBasis Basis = LNPEntityAttack::MakeBasis(Transforms[i].GetTransform());

			switch (Attack.Phase)
			{
			case ELNPEntityAttackPhase::Windup:
			{
				if (Attack.PhaseElapsed < AttackConfig.WindupTime)
					break;

				// 원거리는 선딜이 끝나는 순간 1회 발사한다. ActiveTime은 근접 전용이다.
				if (bIsRanged && WeaponDef)
				{
					const FVector Muzzle = LNPEntityAttack::ComputeMuzzle(Basis, AttackConfig);

					// 조준선은 Actor 경로의 GetBaseAimRotation()과 같은 규약이다 —
					// 수평은 몸이 향한 접평면 전방, 상하만 타겟에서 뽑아 가용 각도로 클램프한다.
					// 클램프 값이 조준 자세·발사 방향·피격 인지 게이트의 공용 원본이므로 여기서도 그 값을 읽는다.
					// 타겟 Transform은 좌표 규약상 **캡슐 중심**이다(플레이어·적 모두). 다만 캡슐 중심은
					// 골반 높이라, 가슴께를 겨누려면 Config의 상하 보정을 얹는다.
					const FVector AimPoint   = TargetingFrags[i].TargetLocation + Basis.Up * AttackConfig.AimTargetUpOffset;
					const FVector ToTarget   = AimPoint - Muzzle;
					const float   VerticalUp = FVector::DotProduct(ToTarget, Basis.Up);
					const float   Horizontal = (ToTarget - Basis.Up * VerticalUp).Size();
					const float   PitchDeg   = FMath::Clamp(
						FMath::RadiansToDegrees(FMath::Atan2(VerticalUp, Horizontal)),
						MoveConfig.AimPitchMinDeg, MoveConfig.AimPitchMaxDeg);

					// **발사 = 그 자체로 하나의 전이다.** 공격 상태 진입(선딜 시작)과 발사는 다른 순간이고,
					// 게스트가 고스트를 만들어야 하는 것은 후자다. 여기서 카운터를 한 번 더 올려
					// 두 순간을 구분해 주고, 같은 자리에서 조준각을 확정해 함께 실어 보낸다.
					// 조준각이 없으면 게스트 고스트가 수평으로 날아가 고저차 교전에서 서버 탄착과 어긋난다.
					if (ActionFrags.IsValidIndex(i))
					{
						++ActionFrags[i].Seq;
						ActionFrags[i].AimPitch = FLNPEnemyActionFragment::EncodeAimPitch(PitchDeg);
					}

					// 양자화를 거친 값으로 쏜다 — 서버와 게스트가 **같은 각도**를 쓰게 하려면
					// 서버도 와이어에 실리는 값을 그대로 써야 한다. 그러지 않으면 0.7도가 조용히 갈린다.
					const float FirePitchDeg = ActionFrags.IsValidIndex(i)
						? FLNPEnemyActionFragment::DecodeAimPitch(ActionFrags[i].AimPitch)
						: PitchDeg;
					const FVector FireDir = LNPEntityAttack::MakeTangentDirection(Basis, 0.f, FirePitchDeg);

					// 산탄 배치는 어빌리티 경로와 같은 공식을 쓴다 (LNPSpreadPattern.h).
					// HexRingCount가 0이면 중앙 1발만 나오므로 단발도 이 경로로 수렴한다.
					LNPSpread::BuildHexRingDirections(FireDir, Basis.Up, Basis.Forward,
						AttackConfig.HexRingCount, AttackConfig.HexStepDegrees, FireDirections);

					// SalvoID는 **한 번의 발사에 하나**다. 펠릿 구분은 SpawnIndex가 맡는다 —
					// Ghost 식별자가 {PlayerID, KeyOrSalvo, SpawnIndex} 조합이라 펠릿마다 키를 새로
					// 발급하면 같은 발사의 펠릿들이 서로 다른 발사로 보인다.
					//
					// ⚠️ 전역 카운터(IssueServerSalvoID)는 **복제되지 않으므로** 게스트가 같은 키를
					// 만들 수 없고, 그러면 서버 임팩트 큐가 게스트 Ghost를 못 찾아 관통해 날아간다.
					// NetID + 전이 카운터는 이미 양쪽이 갖고 있어 추가 대역폭 없이 같은 값이 나온다.
					// 복제가 없는 Standalone에는 NetID 프래그먼트 자체가 없으므로 그때만 전역 카운터로 돌아간다.
					const int32 SalvoID = (NetIDFrags.IsValidIndex(i) && ActionFrags.IsValidIndex(i))
						? LNPEntityAttack::MakeGhostSalvoKey(NetIDFrags[i].NetID.GetValue(), ActionFrags[i].Seq)
						: ULNPGhostProjectileSubsystem::IssueServerSalvoID();

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

// --- Ghost Projectile Processor (게스트 관전 가시성) ---

ULNPEntityGhostProjectileProcessor::ULNPEntityGhostProjectileProcessor()
	: GhostQuery(*this)
{
	// ⚠️ 기본값(Server | Standalone)이면 게스트에서 **아예 돌지 않는다** — 게스트가 유일한 대상이므로
	//    반드시 Client여야 한다. 같은 함정을 ULNPEnemyLODOverrideProcessor가 먼저 밟았다.
	//    리슨 호스트는 넷 모드가 Client | Server라 이 플래그에도 매칭되므로, 실제 발사체를 이미 갖고 있는
	//    서버가 Ghost를 겹쳐 만들지 않도록 Execute 첫 줄에서 다시 거른다.
	ExecutionFlags = (int32)EProcessorExecutionFlags::Client;
	bAutoRegisterWithProcessingPhases = true;
	// Ghost 스폰이 월드 서브시스템과 Mass 커맨드를 거치므로 게임 스레드에서 돈다 (서버 경로와 같은 이유).
	bRequiresGameThreadExecution = true;
	// 이번 프레임에 도착한 수신값을 같은 프레임에 소비한다 — 버블 핸들러는 넷 틱에서 이미 썼다.
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Tasks;
}

void ULNPEntityGhostProjectileProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	GhostQuery.AddRequirement<FLNPEnemyActionFragment>(EMassFragmentAccess::ReadWrite);
	GhostQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	GhostQuery.AddRequirement<FMassNetworkIDFragment>(EMassFragmentAccess::ReadOnly);
	GhostQuery.AddConstSharedRequirement<FLNPEnemySharedFragment>();
	GhostQuery.AddTagRequirement<FLNPEnemyTag>(EMassFragmentPresence::All);
	GhostQuery.RegisterWithProcessor(*this);
}

void ULNPEntityGhostProjectileProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	// 리슨 호스트는 실제 발사체를 갖고 있다 — 여기서 Ghost를 또 만들면 두 발로 보인다.
	if (!LNPMass::IsClientWorld(EntityManager))
		return;

	UWorld* World = EntityManager.GetWorld();
	ULNPGhostProjectileSubsystem* GhostSub = World ? World->GetSubsystem<ULNPGhostProjectileSubsystem>() : nullptr;
	if (GhostSub == nullptr)
		return;

	GhostQuery.ForEachEntityChunk(Context, [GhostSub](FMassExecutionContext& Ctx)
	{
		const ULNPEnemyConfig* Config = Ctx.GetConstSharedFragment<FLNPEnemySharedFragment>().Config;
		if (Config == nullptr
			|| Config->CombatMode != ELNPEnemyCombatMode::PureEntity
			|| Config->AttackType != ELNPEnemyAttackType::Ranged
			|| Config->WeaponData == nullptr)
		{
			return;
		}

		const FLNPEntityAttackConfig& AttackConfig = Config->EntityAttackConfig;
		const ULNPWeaponData& WeaponDef = *Config->WeaponData;

		// 무기 상수는 청크 공용이다 — 서버가 공유 프래그먼트를 청크당 1회만 만드는 것과 같은 이유.
		const FLNPProjectileSharedFragment SharedData = LNPEntityAttack::MakeProjectileSharedData(WeaponDef, AttackConfig);

		const TArrayView<FLNPEnemyActionFragment> Actions    = Ctx.GetMutableFragmentView<FLNPEnemyActionFragment>();
		const TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();
		const TConstArrayView<FMassNetworkIDFragment> NetIDs = Ctx.GetFragmentView<FMassNetworkIDFragment>();

		// 발사마다 새로 할당하지 않도록 청크 단위로 재사용한다 (BuildHexRingDirections가 Reset한다).
		TArray<FVector> FireDirections;

		for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
		{
			FLNPEnemyActionFragment& ActionFrag = Actions[i];

			// 카운터는 wrap하므로 **"같은가"로만** 비교한다 — 대소 비교는 경계에서 뒤집힌다.
			if (ActionFrag.Seq == ActionFrag.ConsumedSeq)
				continue;

			// 놓친 전이가 여럿이어도 소비는 최신 하나로 끝낸다. 이미 지난 발사를 몰아 쏘면
			// 없던 탄막이 생긴다 — 복제 주기가 공격 길이보다 길 때의 스킵은 그대로 스킵으로 둔다.
			const uint8 PendingSeq = ActionFrag.Seq;
			const ELNPEnemyAction PreviousAction = ActionFrag.ConsumedAction;
			ActionFrag.ConsumedSeq    = PendingSeq;
			ActionFrag.ConsumedAction = ActionFrag.Action;

			// 한 번의 공격은 전이를 **두 번** 만든다 — 선딜 시작(Move->Attack)과 발사(Attack->Attack).
			// 서버가 실제로 쏘는 것은 후자이고 조준각도 그때 확정되므로, **직전 소비 행동이 이미
			// Attack이었을 때만** 발사로 읽는다. 선딜 진입에서 쏘면 선딜 길이만큼 앞질러 날아가고
			// 각도도 아직 정해지지 않은 값이 된다.
			if (ActionFrag.Action != ELNPEnemyAction::Attack || PreviousAction != ELNPEnemyAction::Attack)
				continue;

			const LNPEntityAttack::FBasis Basis = LNPEntityAttack::MakeBasis(Transforms[i].GetTransform());
			const FVector Muzzle = LNPEntityAttack::ComputeMuzzle(Basis, AttackConfig);

			// 서버가 발사 순간에 확정해 실어 보낸 각도를 그대로 쓴다 — 양쪽이 같은 양자화 값을 본다.
			const FVector FireDir = LNPEntityAttack::MakeTangentDirection(
				Basis, 0.f, FLNPEnemyActionFragment::DecodeAimPitch(ActionFrag.AimPitch));

			// 산탄 배치는 서버·어빌리티와 같은 공식을 쓴다. HexRingCount가 0이면 중앙 1발만 나온다.
			LNPSpread::BuildHexRingDirections(FireDir, Basis.Up, Basis.Forward,
				AttackConfig.HexRingCount, AttackConfig.HexStepDegrees, FireDirections);

			TArray<FVector, TInlineAllocator<19>> Velocities;
			Velocities.Reserve(FireDirections.Num());
			for (const FVector& Dir : FireDirections)
				Velocities.Add(Dir * WeaponDef.ProjectileSpeed);

			// 서버가 실제 발사체에 넣은 것과 **같은 함수로 유도한** 키다. 이 값이 어긋나면
			// 서버 임팩트 큐가 이 Ghost를 못 찾아 관통해 날아간다.
			GhostSub->SpawnSpectatorGhosts(SharedData, Muzzle, Velocities, WeaponDef.ProjectileLifetime,
				ELNPInstigatorTeam::Enemy, INDEX_NONE,
				LNPEntityAttack::MakeGhostSalvoKey(NetIDs[i].NetID.GetValue(), PendingSeq),
				/*UpstreamDelaySeconds*/ 0.f);
		}
	});
}
