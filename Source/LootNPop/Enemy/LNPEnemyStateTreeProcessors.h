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
 * Unified evaluator for Enemy state (Perception, Targeting, Distances).
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
 * Rotates the entity to face the player target.
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
 * Moves the entity towards the player using simple steering.
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
	TStateTreeExternalDataHandle<struct FMassMoveTargetFragment> MoveTargetHandle;
	TStateTreeExternalDataHandle<struct FMassActorFragment> ActorHandle;
};

/**
 * Wanders around the Parent LootPod when in Idle state.
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

	TStateTreeExternalDataHandle<struct FLNPEnemyFragment> EnemyHandle;
	TStateTreeExternalDataHandle<struct FTransformFragment> TransformHandle;
	TStateTreeExternalDataHandle<struct FLNPEnemyTargetingFragment> TargetingHandle;
	TStateTreeExternalDataHandle<struct FMassMoveTargetFragment> MoveTargetHandle;
};

