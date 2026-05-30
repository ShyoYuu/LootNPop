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

// --- State Evaluator (상태 평가) ---

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

// --- LookAt Task (방향 전환) ---

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

	// 즉시 중단: Alert 상태가 아니면 (Confirmed/None으로 변경 포함) 태스크 종료
	if (Targeting.State != ELNPTargetingState::Alert)
	{
		return EStateTreeRunStatus::Failed;
	}

	if (Targeting.TargetPlayer.IsValid())
	{
		MoveTarget.Center = Targeting.TargetLocation;
		MoveTarget.DesiredSpeed = FMassInt16Real(0.0f); // 이동 없이 타겟 방향 전환
		
		// Alert 상태 동안 방향 Intent 유지를 위해 Running 상태 지속
		return EStateTreeRunStatus::Running;
	}
	
	return EStateTreeRunStatus::Failed;
}

// --- Steering Task (조향) ---

bool FLNPEnemySteeringTask::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(SharedConfigHandle);
	Linker.LinkExternalData(TargetingHandle);
	Linker.LinkExternalData(TransformHandle);
	Linker.LinkExternalData(MoveTargetHandle);
	return true;
}

void FLNPEnemySteeringTask::GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const
{
	Builder.AddReadOnly(SharedConfigHandle);
	Builder.AddReadOnly(TargetingHandle);
	Builder.AddReadOnly(TransformHandle);
	Builder.AddReadWrite(MoveTargetHandle);
}

EStateTreeRunStatus FLNPEnemySteeringTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transitions) const
{
	UE_LOG(LogLootNPop, Log, TEXT("Entering SteeringTask"));
	return EStateTreeRunStatus::Running;
}

void FLNPEnemySteeringTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	UE_LOG(LogLootNPop, Log, TEXT("Exiting SteeringTask"));
}

EStateTreeRunStatus FLNPEnemySteeringTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	const FLNPEnemySharedFragment& SharedConfig = Context.GetExternalData(SharedConfigHandle);
	const FLNPEnemyTargetingFragment& Targeting = Context.GetExternalData(TargetingHandle);
	FMassMoveTargetFragment& MoveTarget = Context.GetExternalData(MoveTargetHandle);

	// 1. 즉시 중단: 슬롯이 소실되거나 상태가 변경된 경우
	if (Targeting.State != ELNPTargetingState::Confirmed)
	{
		return EStateTreeRunStatus::Failed;
	}

	if (Targeting.TargetPlayer.IsValid())
	{
		const float DistanceToTarget = FMath::Sqrt(Targeting.DistanceToTargetSq);
		const float AttackRange = SharedConfig.Config ? SharedConfig.Config->MovementConfig.AttackRange : 200.0f;

		// 2. 완료 체크: 공격 범위 도달 시
		if (DistanceToTarget <= AttackRange)
		{
			return EStateTreeRunStatus::Succeeded;
		}

		float DesiredSpeed = 600.0f;
		if (SharedConfig.Config)
		{
			DesiredSpeed = SharedConfig.Config->MovementConfig.MoveSpeed;
		}

		// 3. 계속 이동 — AttackRange 내 StopBuffer 앞에서 정지하여 도착 신호 확실히 발생
		// StopBuffer >= 30 (ArrivalBuffer) 보장.
		const FVector EntityLocation = Context.GetExternalData(TransformHandle).GetTransform().GetLocation();
		const FVector DirToTarget = (Targeting.TargetLocation - EntityLocation).GetSafeNormal();
		const float StopBuffer = FMath::Max(30.f, FMath::Min(AttackRange * 0.1f, 100.f));
		const float StopDist = FMath::Max(0.f, AttackRange - StopBuffer);
		MoveTarget.Center = Targeting.TargetLocation - DirToTarget * StopDist;
		MoveTarget.IntentAtGoal = EMassMovementAction::Stand;
		MoveTarget.DistanceToGoal = FMath::Max(0.f, DistanceToTarget - StopDist);
		MoveTarget.DesiredSpeed = FMassInt16Real(DesiredSpeed);

		return EStateTreeRunStatus::Running;
	}

	return EStateTreeRunStatus::Failed;
}

// --- Attack Task (공격) ---

bool FLNPEnemyAttackTask::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(SharedConfigHandle);
	Linker.LinkExternalData(TargetingHandle);
	Linker.LinkExternalData(TransformHandle);
	Linker.LinkExternalData(MoveTargetHandle);
	Linker.LinkExternalData(ActorHandle);
	return true;
}

void FLNPEnemyAttackTask::GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const
{
	Builder.AddReadOnly(SharedConfigHandle);
	Builder.AddReadOnly(TargetingHandle);
	Builder.AddReadOnly(TransformHandle);
	Builder.AddReadWrite(MoveTargetHandle);
	Builder.AddReadOnly(ActorHandle);
}

EStateTreeRunStatus FLNPEnemyAttackTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	// 이동 정지 및 타겟 방향 전환 (TargetFollowProcessor가 ActualDistance <= StopDist인 동안
	// 매 프레임 StateTree에 신호를 보내 공격 루프를 구동한다).
	const FLNPEnemyTargetingFragment& Targeting = Context.GetExternalData(TargetingHandle);
	FMassMoveTargetFragment& MoveTarget = Context.GetExternalData(MoveTargetHandle);
	MoveTarget.Center       = Targeting.TargetLocation;
	MoveTarget.DesiredSpeed = FMassInt16Real(0.f);

	UE_LOG(LogLootNPop, Log, TEXT("Entering AttackTask"));
	return EStateTreeRunStatus::Running;
}

void FLNPEnemyAttackTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	UE_LOG(LogLootNPop, Log, TEXT("Exiting AttackTask"));
}

EStateTreeRunStatus FLNPEnemyAttackTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	const FLNPEnemyTargetingFragment& Targeting = Context.GetExternalData(TargetingHandle);

	if (Targeting.State != ELNPTargetingState::Confirmed)
	{
		UE_LOG(LogLootNPop, Log, TEXT("AttackTask: Targeting state changed to %d, exiting Attack state"), static_cast<int32>(Targeting.State));
		return EStateTreeRunStatus::Failed;
	}

	const FLNPEnemySharedFragment& SharedConfig = Context.GetExternalData(SharedConfigHandle);
	const float AttackRange = SharedConfig.Config ? SharedConfig.Config->MovementConfig.AttackRange : 200.f;

	if (Targeting.DistanceToTargetSq > FMath::Square(AttackRange))
	{
		UE_LOG(LogLootNPop, Log, TEXT("AttackTask: Target moved out of range, exiting Attack state"));
		return EStateTreeRunStatus::Failed;
	}

	// 타겟 방향 유지; 이동은 MovementProcessor가 정지시킴 (ActualDistance <= StopDist)
	FMassMoveTargetFragment& MoveTarget = Context.GetExternalData(MoveTargetHandle);
	MoveTarget.Center       = Targeting.TargetLocation;
	MoveTarget.DesiredSpeed = FMassInt16Real(0.f);

	const FMassActorFragment& ActorFrag = Context.GetExternalData(ActorHandle);
	if (ALNPEnemyCharacter* Enemy = Cast<ALNPEnemyCharacter>(const_cast<AActor*>(ActorFrag.Get())))
	{
		Enemy->TryActivateAttack();
	}
	else
	{
		UE_LOG(LogLootNPop, Warning, TEXT("AttackTask: Failed to cast actor to ALNPEnemyCharacter"));
	}

	return EStateTreeRunStatus::Running;
}

// --- Idle Task (대기) ---

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

	// 배회 상태 리셋: 다음 Tick에서 즉시 새 타겟 선택
	IdleData.bNeedNewWanderTarget = true;
	IdleData.LastWanderTime = 0.0;

	// 목적지를 현재 위치로 설정: 다음 프레임에 MovementProcessor가 "도착"을 감지하여
	// StateTreeActivate 신호를 발생시키고 1-2 프레임 내에 Tick()을 호출한다.
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

	// 1. 즉시 중단: 타게팅 상태가 None에서 변경된 경우 즉시 종료
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

	// 2. 완료 체크: 현재 배회 타겟 도달 시 다음 인터벌 대기
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

	// Idle 상태 동안 InstanceData(및 로직)를 유지하기 위해 항상 Running 반환
	return EStateTreeRunStatus::Running;
}