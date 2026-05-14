// Fill out your copyright notice in the Description page of Project Settings.

#include "Enemy/LNPEnemyStateTreeProcessors.h"
#include "Enemy/LNPEnemyMassTypes.h"
#include "Enemy/LNPEnemyConfig.h"
#include "Enemy/LNPEnemyCharacter.h"
#include "GameLogic/LNPSurfaceCacheSubsystem.h"
#include "LootNPop.h"

#include "MassStateTreeTypes.h"
#include "MassCommonFragments.h"
#include "MassMovementFragments.h"
#include "MassNavigationFragments.h"
#include "MassStateTreeExecutionContext.h"
#include "MassStateTreeDependency.h"
#include "StateTreeLinker.h"
#include "MassStateTreeSubsystem.h"
#include "MassStateTreeFragments.h"
#include "MassActorSubsystem.h"

// --- State Evaluator ---

bool FLNPEnemyStateEvaluator::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(SharedConfigHandle);
	Linker.LinkExternalData(TargetingHandle);
	Linker.LinkExternalData(CandidateHandle);
	return true;
}

void FLNPEnemyStateEvaluator::GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const
{
	Builder.AddReadOnly(SharedConfigHandle);
	Builder.AddReadOnly(TargetingHandle);
	Builder.AddReadOnly(CandidateHandle);
}

void FLNPEnemyStateEvaluator::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	const FLNPEnemyTargetingFragment& Targeting = Context.GetExternalData(TargetingHandle);
	const FLNPEnemyTargetingCandidateFragment& CandidateData = Context.GetExternalData(CandidateHandle);
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	InstanceData.TargetingState = Targeting.State;
	InstanceData.DistanceToTarget = FMath::Sqrt(Targeting.DistanceToTargetSq);
	
	const FLNPEnemySharedFragment& SharedConfig = Context.GetExternalData(SharedConfigHandle);
	if (SharedConfig.Config)
	{
		InstanceData.AttackRange = SharedConfig.Config->MovementConfig.AttackRange;
	}
}

// --- LookAt Task ---

bool FLNPEnemyLookAtTask::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(SharedConfigHandle);
	Linker.LinkExternalData(TransformHandle);
	Linker.LinkExternalData(TargetingHandle);
	Linker.LinkExternalData(MoveTargetHandle);
	Linker.LinkExternalData(ActorHandle);
	return true;
}

void FLNPEnemyLookAtTask::GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const
{
	Builder.AddReadOnly(SharedConfigHandle);
	Builder.AddReadOnly(TransformHandle);
	Builder.AddReadOnly(TargetingHandle);
	Builder.AddReadWrite(MoveTargetHandle);
	Builder.AddReadOnly(ActorHandle);
}

EStateTreeRunStatus FLNPEnemyLookAtTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	//UE_LOG(LogLootNPop, Log, TEXT("Entering LookAtTask"));
	return EStateTreeRunStatus::Running;
}

void FLNPEnemyLookAtTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	//UE_LOG(LogLootNPop, Log, TEXT("Exiting LookAtTask"));
}

EStateTreeRunStatus FLNPEnemyLookAtTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	const FLNPEnemyTargetingFragment& Targeting = Context.GetExternalData(TargetingHandle);
	FMassMoveTargetFragment& MoveTarget = Context.GetExternalData(MoveTargetHandle);

	// Immediate Interruption: If no longer in Alert state (or state changed to Confirmed/None), finish task
	if (Targeting.State != ELNPTargetingState::Alert)
	{
		return EStateTreeRunStatus::Failed;
	}

	if (Targeting.TargetPlayer.IsValid())
	{
		MoveTarget.Center = Targeting.LastKnownTargetLocation;
		MoveTarget.DesiredSpeed = FMassInt16Real(0.0f); // Face target without moving
		
		// This task stays Running while in Alert state to keep the orientation intent updated
		return EStateTreeRunStatus::Running;
	}
	
	return EStateTreeRunStatus::Failed;
}

// --- Steering Task ---

bool FLNPEnemySteeringTask::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(SharedConfigHandle);
	Linker.LinkExternalData(TargetingHandle);
	Linker.LinkExternalData(MoveTargetHandle);
	return true;
}

void FLNPEnemySteeringTask::GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const
{
	Builder.AddReadOnly(SharedConfigHandle);
	Builder.AddReadOnly(TargetingHandle);
	Builder.AddReadWrite(MoveTargetHandle);
}

EStateTreeRunStatus FLNPEnemySteeringTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transitions) const
{
	//UE_LOG(LogLootNPop, Log, TEXT("Entering SteeringTask"));
	return EStateTreeRunStatus::Running;
}

void FLNPEnemySteeringTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	//UE_LOG(LogLootNPop, Log, TEXT("Exiting SteeringTask"));
}

EStateTreeRunStatus FLNPEnemySteeringTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	const FLNPEnemySharedFragment& SharedConfig = Context.GetExternalData(SharedConfigHandle);
	const FLNPEnemyTargetingFragment& Targeting = Context.GetExternalData(TargetingHandle);
	FMassMoveTargetFragment& MoveTarget = Context.GetExternalData(MoveTargetHandle);

	// 1. Immediate Interruption: If slot is lost or state changed
	if (Targeting.State != ELNPTargetingState::Confirmed)
	{
		return EStateTreeRunStatus::Failed;
	}

	if (Targeting.TargetPlayer.IsValid())
	{
		const float DistanceToTarget = FMath::Sqrt(Targeting.DistanceToTargetSq);
		const float AttackRange = SharedConfig.Config ? SharedConfig.Config->MovementConfig.AttackRange : 200.0f;

		// 2. Completion Check: If reached attack range
		if (DistanceToTarget <= AttackRange)
		{
			return EStateTreeRunStatus::Succeeded;
		}

		float DesiredSpeed = 600.0f;
		if (SharedConfig.Config)
		{
			DesiredSpeed = SharedConfig.Config->MovementConfig.MoveSpeed;
		}

		// 3. Keep Moving
		MoveTarget.Center = Targeting.LastKnownTargetLocation;
		MoveTarget.IntentAtGoal = EMassMovementAction::Stand;
		MoveTarget.DistanceToGoal = DistanceToTarget;
		MoveTarget.DesiredSpeed = FMassInt16Real(DesiredSpeed);

		return EStateTreeRunStatus::Running;
	}

	return EStateTreeRunStatus::Failed;
}

// --- Idle Task ---

bool FLNPEnemyIdleTask::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(SharedConfigHandle);
	Linker.LinkExternalData(EnemyHandle);
	Linker.LinkExternalData(IdleFragmentHandle);
	Linker.LinkExternalData(TransformHandle);
	Linker.LinkExternalData(TargetingHandle);
	Linker.LinkExternalData(MoveTargetHandle);
	return true;
}

void FLNPEnemyIdleTask::GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const
{
	Builder.AddReadOnly(SharedConfigHandle);
	Builder.AddReadOnly(EnemyHandle);
	Builder.AddReadWrite(IdleFragmentHandle);
	Builder.AddReadOnly(TransformHandle);
	Builder.AddReadOnly(TargetingHandle);
	Builder.AddReadWrite(MoveTargetHandle);
}

EStateTreeRunStatus FLNPEnemyIdleTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transitions) const
{
	//UE_LOG(LogLootNPop, Log, TEXT("Entering IdleTask"));

	FLNPEnemyIdleFragment& IdleData = Context.GetExternalData(IdleFragmentHandle);
	FMassMoveTargetFragment& MoveTarget = Context.GetExternalData(MoveTargetHandle);
	const FTransformFragment& Transform = Context.GetExternalData(TransformHandle);

	// Reset wander state so next Tick immediately picks a new target
	IdleData.bNeedNewWanderTarget = true;
	IdleData.LastWanderTime = 0.0;

	// Set destination to current position so MovementProcessor detects "arrival" next frame,
	// triggering a StateTreeActivate signal and calling Tick() within 1-2 frames.
	MoveTarget.Center = Transform.GetTransform().GetLocation();

	return EStateTreeRunStatus::Running;
}

void FLNPEnemyIdleTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	//UE_LOG(LogLootNPop, Log, TEXT("Exiting IdleTask"));
}

EStateTreeRunStatus FLNPEnemyIdleTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	const FLNPEnemySharedFragment& SharedConfig = Context.GetExternalData(SharedConfigHandle);
	const FLNPEnemyFragment& Enemy = Context.GetExternalData(EnemyHandle);
	FLNPEnemyIdleFragment& IdleData = Context.GetExternalData(IdleFragmentHandle);
	const FTransformFragment& Transform = Context.GetExternalData(TransformHandle);
	const FLNPEnemyTargetingFragment& Targeting = Context.GetExternalData(TargetingHandle);
	FMassMoveTargetFragment& MoveTarget = Context.GetExternalData(MoveTargetHandle);

	const double CurrentTime = Context.GetWorld()->GetTimeSeconds();
	const double TimeSinceLastWander = CurrentTime - IdleData.LastWanderTime;
	const double WANDER_INTERVAL = 3.0;

	// 1. Immediate Interruption: If targeting state changed from None, bail out!
	if (Targeting.State != ELNPTargetingState::None)
	{
		//UE_LOG(LogLootNPop, Log, TEXT("IdleTask: Targeting state changed to %d, exiting Idle state"), static_cast<int32>(Targeting.State));
		return EStateTreeRunStatus::Failed;
	}

	if (IdleData.bNeedNewWanderTarget && WANDER_INTERVAL < TimeSinceLastWander)
	{
		const float WanderMinDist = SharedConfig.Config ? SharedConfig.Config->MovementConfig.WanderMinDistance : 200.0f;
		const float WanderMaxDist = SharedConfig.Config ? SharedConfig.Config->MovementConfig.WanderMaxDistance : 800.0f;
		const FVector GravityOrigin = SharedConfig.Config ? SharedConfig.Config->MovementConfig.GravityOrigin : FVector::ZeroVector;
		const FVector PodOutDir = (Enemy.ParentPodLocation - GravityOrigin).GetSafeNormal();
		const FVector ArbitraryAxis = FMath::Abs(FVector::DotProduct(PodOutDir, FVector::ForwardVector)) < 0.9f ? FVector::ForwardVector : FVector::RightVector;
		const FVector Tangent1 = FVector::CrossProduct(PodOutDir, ArbitraryAxis).GetSafeNormal();
		const FVector Tangent2 = FVector::CrossProduct(PodOutDir, Tangent1).GetSafeNormal();

		const float Angle = FMath::FRandRange(0.0f, 2.0f * PI);
		const float Dist = FMath::FRandRange(WanderMinDist, WanderMaxDist);
		const FVector TangentOffset = (Tangent1 * FMath::Cos(Angle) + Tangent2 * FMath::Sin(Angle)) * Dist;

		const float PodRadius = (Enemy.ParentPodLocation - GravityOrigin).Size();
		const FVector QueryDir = (PodOutDir * PodRadius + TangentOffset).GetSafeNormal();

		ULNPSurfaceCacheSubsystem* SurfaceCache = Context.GetWorld()->GetSubsystem<ULNPSurfaceCacheSubsystem>();
		FVector WanderPoint;
		if (SurfaceCache && SurfaceCache->GetSurfacePoint(QueryDir, WanderPoint))
		{
			MoveTarget.Center = WanderPoint;
		}
		else
		{
			MoveTarget.Center = Enemy.ParentPodLocation + TangentOffset;
			//UE_LOG(LogLootNPop, Warning, TEXT("IdleTask: Surface cache unavailable, using fallback wander target at %s"), *MoveTarget.Center.ToString());
		}

		MoveTarget.DistanceToGoal = 50.0f;
		MoveTarget.DesiredSpeed = FMassInt16Real(0.0f);

		IdleData.bNeedNewWanderTarget = false;
		//UE_LOG(LogLootNPop, Log, TEXT("IdleTask: New wander target set at %s"), *MoveTarget.Center.ToString());
	}

	// 2. Completion Check: If we reached the current wander target, wait for next interval
	const float DistSq = FVector::DistSquared(MoveTarget.Center, Transform.GetTransform().GetLocation());
	if (DistSq < FMath::Square(100.0f))
	{
		if (IdleData.bNeedNewWanderTarget == false)
		{
			IdleData.LastWanderTime = CurrentTime;
			IdleData.bNeedNewWanderTarget = true;
		}
		//UE_LOG(LogLootNPop, Log, TEXT("IdleTask: Reached wander target at %s"), *MoveTarget.Center.ToString());
	}

	// Always return Running to keep InstanceData (and logic) persistent while in Idle state
	return EStateTreeRunStatus::Running;
}