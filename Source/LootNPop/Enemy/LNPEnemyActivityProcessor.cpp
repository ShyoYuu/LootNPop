// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Enemy/LNPEnemyActivityProcessor.h"
#include "Enemy/LNPEnemyMassTypes.h"
#include "Enemy/LNPEnemyConfig.h"
#include "Enemy/LNPEnemyProcessors.h"
#include "LootPod/LNPLootPodMassTypes.h"
#include "GAS/LNPPoiseTypes.h"
#include "SurfaceNavigation/LNPSurfaceDataSubsystem.h"
#include "SurfaceNavigation/LNPNavPathSubsystem.h"
#include "SurfaceNavigation/LNPLoadBaseline.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "MassActorSubsystem.h"
#include "MassCommonFragments.h"
#include "MassExecutionContext.h"
#include "MassNavigationFragments.h"
#include "MassSignalSubsystem.h"
#include "MassStateTreeTypes.h"
#include "LNPMassUtils.h"
#include "LootNPop.h"
#include "GameFramework/Actor.h"

ULNPEnemyActivityProcessor::ULNPEnemyActivityProcessor()
	: EnemyQuery(*this), PodQuery(*this), PlayerQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Server | EProcessorExecutionFlags::Standalone);
	ProcessingPhase = EMassProcessingPhase::PostPhysics;
	ExecutionOrder.ExecuteAfter.Add(ULNPHealthProcessor::StaticClass()->GetFName());
	ExecutionOrder.ExecuteAfter.Add(ULNPEnemyActorSyncProcessor::StaticClass()->GetFName());
	ExecutionOrder.ExecuteBefore.Add(ULNPEnemyScoringProcessor::StaticClass()->GetFName());
}

void ULNPEnemyActivityProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EnemyQuery.AddTagRequirement<FLNPEnemyTag>(EMassFragmentPresence::All);
	EnemyQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EnemyQuery.AddRequirement<FLNPEnemyFragment>(EMassFragmentAccess::ReadWrite);
	EnemyQuery.AddRequirement<FLNPEnemyTargetingFragment>(EMassFragmentAccess::ReadOnly);
	EnemyQuery.AddRequirement<FLNPEnemyVelocityFragment>(EMassFragmentAccess::ReadOnly);
	EnemyQuery.AddRequirement<FLNPEntityAttackFragment>(EMassFragmentAccess::ReadOnly);
	EnemyQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadOnly);
	EnemyQuery.AddRequirement<FLNPPoiseFragment>(EMassFragmentAccess::ReadOnly);
	EnemyQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadWrite);
	EnemyQuery.AddRequirement<FLNPEnemySeparationFragment>(EMassFragmentAccess::ReadWrite);
	EnemyQuery.AddRequirement<FLNPEnemyIdleFragment>(EMassFragmentAccess::ReadWrite);
	EnemyQuery.AddRequirement<FLNPEnemyPathFragment>(EMassFragmentAccess::ReadWrite);
	EnemyQuery.AddConstSharedRequirement<FLNPEnemySharedFragment>();
	EnemyQuery.RegisterWithProcessor(*this);
	PodQuery.AddTagRequirement<FLNPLootPodTag>(EMassFragmentPresence::All);
	PodQuery.AddRequirement<FLNPLootPodFragment>(EMassFragmentAccess::ReadWrite);
	PodQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	PodQuery.RegisterWithProcessor(*this);
	PlayerQuery.AddTagRequirement<FLNPPlayerTag>(EMassFragmentPresence::All);
	PlayerQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	PlayerQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	PlayerQuery.RegisterWithProcessor(*this);
	ProcessorRequirements.AddSubsystemRequirement<ULNPSurfaceDataSubsystem>(EMassFragmentAccess::ReadOnly);
	ProcessorRequirements.AddSubsystemRequirement<ULNPNavPathSubsystem>(EMassFragmentAccess::ReadWrite);
	ProcessorRequirements.AddSubsystemRequirement<UMassSignalSubsystem>(EMassFragmentAccess::ReadWrite);
}

void ULNPEnemyActivityProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	if (LNPMass::IsClientWorld(EntityManager))
		return;
	const double Now = EntityManager.GetWorld()->GetTimeSeconds();
	// 같은 production 스폰의 항상 활성 대조군. 일반 플레이에는 적용하지 않는다.
	static const bool bAlwaysActive = LNPLoadBaseline::IsProduction()
		&& FParse::Param(FCommandLine::Get(), TEXT("LNPLoadBaselineAlwaysActive"));
	const bool bDistanceDue = Now >= NextDistanceTime;
	if (bDistanceDue)
		NextDistanceTime = Now + LNPEnemyActivity::DistanceInterval;
	const bool bDiagnostics = LNPLoadBaseline::IsActivityDiagnostics();
	const bool bDiagnosticSample = bDiagnostics && bDistanceDue && Now >= NextDiagnosticTime;
	if (bDiagnosticSample)
	{
		if (NextDiagnosticTime == 0.0)
			UE_LOG(LogLootNPop, Display, TEXT("[ActivityDiagnostic] Reason bits: 0=Wake 1=Dead 2=Target 3=Velocity 4=Attack 5=HitPoise 6=Home 7=Dynamic 8=Actor 9=Settings 10=Support"));
		NextDiagnosticTime = Now + 1.0;
	}
	const auto Snapshot = Context.GetSubsystemChecked<ULNPSurfaceDataSubsystem>().TakeSnapshot();
	ULNPNavPathSubsystem& NavPaths = Context.GetMutableSubsystemChecked<ULNPNavPathSubsystem>();
	UMassSignalSubsystem& Signals = Context.GetMutableSubsystemChecked<UMassSignalSubsystem>();
	// 파괴·길 변경으로 휴면 발밑이 사라질 수 있다. 새 immutable 게시를 한 번의 깨움으로 소비한다.
	const bool bTerrainChanged = LastSurfaceSnapshot.IsValid() && LastSurfaceSnapshot != Snapshot;
	LastSurfaceSnapshot = Snapshot;
	TArray<FVector> Players;
	TArray<FVector> DiagnosticActorPlayers;
	if (bDistanceDue)
	{
		PlayerQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& Ctx)
		{
			const auto Transforms = Ctx.GetFragmentView<FTransformFragment>();
			const auto Actors = Ctx.GetFragmentView<FMassActorFragment>();
			for (int32 I = 0; I < Ctx.GetNumEntities(); ++I)
			{
				const FVector Position = Transforms[I].GetTransform().GetLocation();
				Players.Add(Position);
				if (bDiagnosticSample)
				{
					const AActor* Actor = Actors.IsValidIndex(I) ? Actors[I].Get() : nullptr;
					if (Actor)
						DiagnosticActorPlayers.Add(Actor->GetActorLocation());
					UE_LOG(LogLootNPop, Display, TEXT("[ActivityDiagnostic] Player=%d Actor=%s Mass=%s ActorPosition=%s Delta=%.1f"),
						Ctx.GetEntity(I).Index, *GetNameSafe(Actor), *Position.ToString(),
						Actor ? *Actor->GetActorLocation().ToString() : TEXT("None"), Actor ? FVector::Dist(Position, Actor->GetActorLocation()) : -1.0);
				}
			}
		});
	}
	auto NearestPlayerSq = [&Players](const FVector& Position)
	{
		double DistanceSq = DBL_MAX;
		for (const FVector& Player : Players)
			DistanceSq = FMath::Min(DistanceSq, FVector::DistSquared(Position, Player));
		return DistanceSq;
	};
	struct FActivityPod
	{
		FLNPLootPodFragment* Fragment;
		FVector Position;
		float ActiveDistance = 0.f;
		float CullDistance = 0.f;
		bool bForceActive = false;
		bool bCanSleep = true;
		int32 DiagnosticCull[2] = {};
	};
	TMap<FMassEntityHandle, FActivityPod> Pods;
	PodQuery.ForEachEntityChunk(Context, [&Pods, bTerrainChanged](FMassExecutionContext& Ctx)
	{
		const auto Fragments = Ctx.GetMutableFragmentView<FLNPLootPodFragment>();
		const auto Transforms = Ctx.GetFragmentView<FTransformFragment>();
		for (int32 I = 0; I < Ctx.GetNumEntities(); ++I)
		{
			if (Fragments[I].State != ELNPLootPodState::Popped)
			{
				FActivityPod& Pod = Pods.Add(Ctx.GetEntity(I), { &Fragments[I], Transforms[I].GetTransform().GetLocation() });
				Pod.bForceActive = bAlwaysActive || bTerrainChanged || Fragments[I].State == ELNPLootPodState::Looting;
			}
		}
	});
	// 거리 판정은 4Hz지만 피격·물리/전투 상태는 매 프레임 읽어 무리 전체를 깨운다.
	EnemyQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& Ctx)
	{
		const auto Enemies = Ctx.GetMutableFragmentView<FLNPEnemyFragment>();
		const auto Transforms = Ctx.GetFragmentView<FTransformFragment>();
		const auto Targets = Ctx.GetFragmentView<FLNPEnemyTargetingFragment>();
		const auto Velocities = Ctx.GetFragmentView<FLNPEnemyVelocityFragment>();
		const auto Attacks = Ctx.GetFragmentView<FLNPEntityAttackFragment>();
		const auto Actors = Ctx.GetFragmentView<FMassActorFragment>();
		const auto Poise = Ctx.GetFragmentView<FLNPPoiseFragment>();
		const auto& Shared = Ctx.GetConstSharedFragment<FLNPEnemySharedFragment>();
		for (int32 I = 0; I < Ctx.GetNumEntities(); ++I)
		{
			FLNPEnemyFragment& Enemy = Enemies[I];
			if (FActivityPod* Pod = Pods.Find(Enemy.ParentLootPod))
			{
				if (bDiagnostics)
				{
					// 1Hz 표본 사이의 순간 깨움도 누적한다. 비트의 대응은 진단 로그 범례에 남긴다.
					DiagnosticWakeReasons.FindOrAdd(Enemy.ParentLootPod) |= uint32(Enemy.bActivityWakeRequested) << 0
						| uint32(Enemy.Health <= 0.f) << 1 | uint32(Targets[I].State != ELNPTargetingState::None) << 2
						| uint32(!Velocities[I].Velocity.IsNearlyZero()) << 3
						| uint32(Attacks[I].Phase != ELNPEntityAttackPhase::None || Attacks[I].bAttackRequested) << 4
						| uint32(Enemy.HitReactTimer > 0.f || Enemy.FlinchTimeRemaining > 0.f || Poise[I].Current > 0.f) << 5
						| uint32(Enemy.bNeedsHomeCheck) << 6 | uint32(Enemy.DynamicSupportContact.IsValid()) << 7
						| uint32(Actors[I].Get() != nullptr) << 8
						| uint32(Shared.ActivityActiveDistance <= 0.f || Shared.ActivityCullDistance < Shared.ActivityActiveDistance) << 9
						| uint32(Shared.Config && !Shared.Config->IsFlying() && !Shared.Config->IsCrawling() && !Enemy.SurfaceHandle.IsValid()) << 10;
				}
				if (bDiagnosticSample)
				{
					for (const FVector& Player : DiagnosticActorPlayers)
						if (FVector::DistSquared(Player, Transforms[I].GetTransform().GetLocation()) < FMath::Square(Shared.ActivityCullDistance))
						{
							++Pod->DiagnosticCull[Ctx.DoesArchetypeHaveTag<FLNPEnemyFlyingTag>() ? 1 : 0];
							break;
						}
				}
				Pod->bForceActive |= Enemy.bActivityWakeRequested || Enemy.Health <= 0.f
					|| Targets[I].State != ELNPTargetingState::None || !Velocities[I].Velocity.IsNearlyZero()
					|| Attacks[I].Phase != ELNPEntityAttackPhase::None || Attacks[I].bAttackRequested
					|| Enemy.HitReactTimer > 0.f || Enemy.FlinchTimeRemaining > 0.f || Poise[I].Current > 0.f
					|| Enemy.bNeedsHomeCheck || Enemy.DynamicSupportContact.IsValid() || Actors[I].Get() != nullptr;
				if (Shared.Config && Shared.Config->CombatMode == ELNPEnemyCombatMode::PureEntity)
				{
					Pod->ActiveDistance = FMath::Max(Pod->ActiveDistance, Shared.ActivityActiveDistance);
					Pod->CullDistance = FMath::Max(Pod->CullDistance, Shared.ActivityCullDistance);
					Pod->bForceActive |= Shared.ActivityActiveDistance <= 0.f
						|| Shared.ActivityCullDistance < Shared.ActivityActiveDistance
						|| (!Shared.Config->IsFlying() && !Shared.Config->IsCrawling() && !Enemy.SurfaceHandle.IsValid());
					if (bDistanceDue)
						Pod->bCanSleep &= NearestPlayerSq(Transforms[I].GetTransform().GetLocation())
							> FMath::Square(static_cast<double>(FMath::Max(
								Shared.ActivityCullDistance, Shared.ActivityVisibilityDistance) * 1.05f));
				}
			}
			Enemy.bActivityWakeRequested = false;
		}
	});
	for (auto& Pair : Pods)
	{
		FActivityPod& Pod = Pair.Value;
		if (Pod.bForceActive)
			Pod.Fragment->ActivityWakeUntil = Now + LNPEnemyActivity::WakeHoldSeconds;
		if (Pod.bForceActive || Now < Pod.Fragment->ActivityWakeUntil || bDistanceDue)
			Pod.Fragment->ActivityBand = LNPEnemyActivity::SelectBand(Pod.Fragment->ActivityBand,
				NearestPlayerSq(Pod.Position), Pod.ActiveDistance, Pod.CullDistance,
				Now < Pod.Fragment->ActivityWakeUntil, Pod.bCanSleep);
		if (bDiagnosticSample && Pod.DiagnosticCull[0] + Pod.DiagnosticCull[1] > 0)
		{
			double ActorDistanceSq = DBL_MAX;
			for (const FVector& Player : DiagnosticActorPlayers)
				ActorDistanceSq = FMath::Min(ActorDistanceSq, FVector::DistSquared(Pod.Position, Player));
			UE_LOG(LogLootNPop, Display, TEXT("[ActivityDiagnostic] Pod=%d Band=%d MassDistance=%.1f ActorDistance=%.1f Active=%.1f Cull=%.1f Ground=%d Flight=%d Force=%d Hold=%.1f Reasons=%u Looting=%d TerrainChanged=%d"),
				Pair.Key.Index, int32(Pod.Fragment->ActivityBand), FMath::Sqrt(NearestPlayerSq(Pod.Position)), FMath::Sqrt(ActorDistanceSq),
				Pod.ActiveDistance, Pod.CullDistance, Pod.DiagnosticCull[0], Pod.DiagnosticCull[1], Pod.bForceActive,
				FMath::Max(0.0, Pod.Fragment->ActivityWakeUntil - Now), DiagnosticWakeReasons.FindRef(Pair.Key),
				Pod.Fragment->State == ELNPLootPodState::Looting, bTerrainChanged);
		}
	}
	if (bDiagnosticSample)
		DiagnosticWakeReasons.Reset();
	TArray<FMassEntityHandle> Woken;
	EnemyQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& Ctx)
	{
		const auto Enemies = Ctx.GetMutableFragmentView<FLNPEnemyFragment>();
		const auto Transforms = Ctx.GetFragmentView<FTransformFragment>();
		const auto Moves = Ctx.GetMutableFragmentView<FMassMoveTargetFragment>();
		const auto Separation = Ctx.GetMutableFragmentView<FLNPEnemySeparationFragment>();
		const auto Idle = Ctx.GetMutableFragmentView<FLNPEnemyIdleFragment>();
		const auto Paths = Ctx.GetMutableFragmentView<FLNPEnemyPathFragment>();
		const auto* Config = Ctx.GetConstSharedFragment<FLNPEnemySharedFragment>().Config.Get();
		const bool bWasDormant = Ctx.DoesArchetypeHaveTag<FLNPEnemyDormantTag>();
		for (int32 I = 0; I < Ctx.GetNumEntities(); ++I)
		{
			FLNPEnemyFragment& Enemy = Enemies[I];
			const FActivityPod* Pod = Pods.Find(Enemy.ParentLootPod);
			const ELNPEnemyActivityBand PreviousBand = Enemy.ActivityBand;
			Enemy.ActivityBand = Pod && Config && Config->CombatMode == ELNPEnemyCombatMode::PureEntity
				? Pod->Fragment->ActivityBand : ELNPEnemyActivityBand::Active;
			if (PreviousBand != Enemy.ActivityBand)
			{
				Idle[I].WanderSegment = FLNPEnemyWanderSegment();
				Idle[I].bWanderHandled = false;
				Idle[I].bNeedNewWanderTarget = true;
				Idle[I].LastWanderTime = 0.0;
				Moves[I].Center = Transforms[I].GetTransform().GetLocation();
				Moves[I].DesiredSpeed = FMassInt16Real(0.f);
				if (Enemy.ActivityBand == ELNPEnemyActivityBand::Active && !bWasDormant)
					Woken.Add(Ctx.GetEntity(I));
				// 경로 요청 몰림과 대조할 깨움 기록(캡처 중에만, Phase 10 단위 1).
				if (Enemy.ActivityBand > PreviousBand)
					NavPaths.RecordWake(Ctx.GetEntity(I), static_cast<uint8>(Enemy.ActivityBand), bWasDormant);
			}
			const bool bDormant = Enemy.ActivityBand == ELNPEnemyActivityBand::Dormant;
			if (bDormant == bWasDormant)
				continue;
			if (bDormant)
			{
				Ctx.Defer().AddTag<FLNPEnemyDormantTag>(Ctx.GetEntity(I));
				Moves[I].Center = Transforms[I].GetTransform().GetLocation();
				Moves[I].DesiredSpeed = FMassInt16Real(0.f);
				Separation[I] = FLNPEnemySeparationFragment();
				NavPaths.Cancel(Ctx.GetEntity(I));
				Paths[I].Path.Reset();
				Paths[I].RequestSerial = 0;
				Paths[I].Status = ELNPNavPathStatus::None;
				Paths[I].LastRequestTime = -DBL_MAX;
				Paths[I].bHasSteeringPoint = false;
				Paths[I].bApproachingUnreachable = false;
			}
			else
			{
				Ctx.Defer().RemoveTag<FLNPEnemyDormantTag>(Ctx.GetEntity(I));
				Idle[I].bNeedNewWanderTarget = true;
				Idle[I].LastWanderTime = 0.0;
				Idle[I].TimeSinceWanderIssued = 0.f;
				Idle[I].bWanderTargetTimedOut = false;
				Woken.Add(Ctx.GetEntity(I));
			}
		}
	});
	if (!Woken.IsEmpty())
		Signals.SignalEntities(UE::Mass::Signals::StateTreeActivate, Woken);
}
