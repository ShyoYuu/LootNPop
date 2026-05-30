// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassStateTreeTypes.h"
#include "LNPEnemyStateTreeTypes.h"
#include "MassProcessor.h"
#include "LNPEnemyStateTreeProcessors.generated.h"

namespace UE::MassBehavior
{
	struct FStateTreeDependencyBuilder;
}

/**
 * Enemy 상태(인식, 타게팅, 거리)를 위한 통합 Evaluator.
 */
USTRUCT(meta = (DisplayName = "LNP State Eval"))
struct LOOTNPOP_API FLNPEnemyStateEvaluator : public FMassStateTreeEvaluatorBase
{
	GENERATED_BODY()

	using FInstanceDataType = FLNPEnemyStateEvaluatorInstanceData;

protected:
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual void Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const override;

	TStateTreeExternalDataHandle<struct FLNPEnemySharedFragment> SharedConfigHandle;
	TStateTreeExternalDataHandle<struct FLNPEnemyTargetingFragment> TargetingHandle;
	TStateTreeExternalDataHandle<struct FLNPEnemyTargetingCandidateFragment> CandidateHandle;
};

/**
 * Entity를 Player 타겟 방향으로 회전시킨다.
 */
USTRUCT(meta = (DisplayName = "LNP LookAt Task"))
struct LOOTNPOP_API FLNPEnemyLookAtTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FLNPEnemyTaskInstanceData;

protected:
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const override;

	TStateTreeExternalDataHandle<struct FLNPEnemySharedFragment> SharedConfigHandle;
	TStateTreeExternalDataHandle<struct FTransformFragment> TransformHandle;
	TStateTreeExternalDataHandle<struct FLNPEnemyTargetingFragment> TargetingHandle;
	TStateTreeExternalDataHandle<struct FMassMoveTargetFragment> MoveTargetHandle;
	TStateTreeExternalDataHandle<struct FMassActorFragment> ActorHandle;
};

/**
 * 단순 Steering으로 Entity를 Player 방향으로 이동시킨다.
 */
USTRUCT(meta = (DisplayName = "LNP Steering Task"))
struct LOOTNPOP_API FLNPEnemySteeringTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FLNPEnemyTaskInstanceData;

protected:
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transitions) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const override;

	TStateTreeExternalDataHandle<struct FLNPEnemySharedFragment> SharedConfigHandle;
	TStateTreeExternalDataHandle<struct FLNPEnemyTargetingFragment> TargetingHandle;
	TStateTreeExternalDataHandle<struct FTransformFragment> TransformHandle;
	TStateTreeExternalDataHandle<struct FMassMoveTargetFragment> MoveTargetHandle;
};

/**
 * Enemy이 공격 범위 내에 있는 동안 GAS를 통해 설정된 공격 Ability를 발동한다.
 * 타게팅이 소실되거나 타겟이 범위를 벗어나면 Failed를 반환한다 (재추격 트리거).
 */
USTRUCT(meta = (DisplayName = "LNP Attack Task"))
struct LOOTNPOP_API FLNPEnemyAttackTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FLNPEnemyTaskInstanceData;

protected:
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const override;

	TStateTreeExternalDataHandle<struct FLNPEnemySharedFragment> SharedConfigHandle;
	TStateTreeExternalDataHandle<struct FLNPEnemyTargetingFragment> TargetingHandle;
	TStateTreeExternalDataHandle<struct FTransformFragment> TransformHandle;
	TStateTreeExternalDataHandle<struct FMassMoveTargetFragment> MoveTargetHandle;
	TStateTreeExternalDataHandle<struct FMassActorFragment> ActorHandle;
};

/**
 * Idle 상태일 때 부모 LootPod 주변을 배회한다.
 */
USTRUCT(meta = (DisplayName = "LNP Idle Task"))
struct LOOTNPOP_API FLNPEnemyIdleTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FLNPEnemyTaskInstanceData;

protected:
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transitions) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const override;

	TStateTreeExternalDataHandle<struct FLNPEnemySharedFragment> SharedConfigHandle;
	TStateTreeExternalDataHandle<struct FLNPEnemyFragment> EnemyHandle;
	TStateTreeExternalDataHandle<struct FLNPEnemyIdleFragment> IdleFragmentHandle;
	TStateTreeExternalDataHandle<struct FTransformFragment> TransformHandle;
	TStateTreeExternalDataHandle<struct FLNPEnemyTargetingFragment> TargetingHandle;
	TStateTreeExternalDataHandle<struct FMassMoveTargetFragment> MoveTargetHandle;
};