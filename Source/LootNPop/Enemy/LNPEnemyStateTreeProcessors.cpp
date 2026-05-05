// Fill out your copyright notice in the Description page of Project Settings.

#include "Enemy/LNPEnemyStateTreeProcessors.h"
#include "Enemy/LNPEnemyMassTypes.h"
#include "Enemy/LNPEnemyConfig.h"
#include "Enemy/LNPEnemyCharacter.h"
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
	UE_LOG(LogTemp, Log, TEXT("Entering LookAtTask"));
	return EStateTreeRunStatus::Running;
}

void FLNPEnemyLookAtTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	UE_LOG(LogTemp, Log, TEXT("Exiting LookAtTask"));
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
	Linker.LinkExternalData(ActorHandle);
	return true;
}

void FLNPEnemySteeringTask::GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const
{
	Builder.AddReadOnly(SharedConfigHandle);
	Builder.AddReadOnly(TargetingHandle);
	Builder.AddReadWrite(MoveTargetHandle);
	Builder.AddReadOnly(ActorHandle);
}

EStateTreeRunStatus FLNPEnemySteeringTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transitions) const
{
	UE_LOG(LogTemp, Log, TEXT("Entering SteeringTask"));
	return EStateTreeRunStatus::Running;
}

void FLNPEnemySteeringTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	UE_LOG(LogTemp, Log, TEXT("Exiting SteeringTask"));
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
	Linker.LinkExternalData(EnemyHandle);
	Linker.LinkExternalData(TransformHandle);
	Linker.LinkExternalData(TargetingHandle);
	Linker.LinkExternalData(MoveTargetHandle);
	return true;
}

void FLNPEnemyIdleTask::GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const
{
	Builder.AddReadOnly(EnemyHandle);
	Builder.AddReadOnly(TransformHandle);
	Builder.AddReadOnly(TargetingHandle);
	Builder.AddReadWrite(MoveTargetHandle);
}

EStateTreeRunStatus FLNPEnemyIdleTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transitions) const
{
	//FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	//InstanceData.NextUpdate = 0.0; // Force immediate update on enter
	UE_LOG(LogTemp, Log, TEXT("Entering IdleTask"));
	return EStateTreeRunStatus::Running;
}

void FLNPEnemyIdleTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	UE_LOG(LogTemp, Log, TEXT("Exiting IdleTask"));
}

EStateTreeRunStatus FLNPEnemyIdleTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	const FLNPEnemyFragment& Enemy = Context.GetExternalData(EnemyHandle);
	const FTransformFragment& Transform = Context.GetExternalData(TransformHandle);
	const FLNPEnemyTargetingFragment& Targeting = Context.GetExternalData(TargetingHandle);
	FMassMoveTargetFragment& MoveTarget = Context.GetExternalData(MoveTargetHandle);
	//FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// 1. Immediate Interruption: If targeting state changed from None, bail out!
	if (Targeting.State != ELNPTargetingState::None)
	{
		return EStateTreeRunStatus::Failed;
	}

	const double CurrentTime = Context.GetWorld()->GetTimeSeconds();

	// 2. Completion Check: If we reached the current wander target, succeed
	const float DistSq = FVector::DistSquared(MoveTarget.Center, Transform.GetTransform().GetLocation());
	if (DistSq < FMath::Square(150.0f))
	{
		return EStateTreeRunStatus::Succeeded;
	}

	// 3. Waiting Logic: If we are still in the random wait period, keep running
	//if (CurrentTime < InstanceData.NextUpdate)
	//{
	//	return EStateTreeRunStatus::Running;
	//}

	// 4. Update Target: Pick a new random point near the pod
	const float WanderRadius = 500.0f;
	const FVector Directions[] = { FVector::ForwardVector, FVector::RightVector, -FVector::ForwardVector, -FVector::RightVector };
	const FVector RandomOffset = Directions[FMath::RandHelper(4)] * FMath::FRandRange(200.0f, WanderRadius);
	
	MoveTarget.Center = Enemy.ParentPodLocation + RandomOffset;
	MoveTarget.DistanceToGoal = 50.0f; 
	MoveTarget.DesiredSpeed = FMassInt16Real(0.0f); // Use Idle speed

	//InstanceData.NextUpdate = CurrentTime + FMath::FRandRange(5.0f, 10.0f);

	UE_LOG(LogTemp, Log, TEXT("IdleTask: New wander target set at %s"), *MoveTarget.Center.ToString());
	return EStateTreeRunStatus::Running;
}