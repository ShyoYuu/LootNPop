// Copyright (c) 2026 LootNPop. All rights reserved.

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
 * Alert 전용 — 제자리에서 Player 타겟을 주시한다.
 *
 * 직접 회전시키지는 않는다. `FMassMoveTargetFragment`에 타겟 위치와 `DesiredSpeed = 0`만 남기고,
 * 실제 회전은 `ULNPEnemyMovementProcessor`가 `RotationRate`로 수행한다.
 *
 * ⚠️ Tick이 `State != Alert`면 즉시 Failed를 돌려주므로 **Combat 상태군의 공통 Task로 달 수 없다.**
 * 추격·공격 중 주시는 Steering/Attack Task가 같은 필드를 갱신하는 것으로 이미 성립한다.
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
 * 공격 범위 안에 있는 동안 기본 공격을 구동한다. `CombatMode`로 경로가 갈린다 —
 * `ActorPromoted`는 조준 갱신 후 GAS 어빌리티를 발동하고,
 * `PureEntity`는 `FLNPEntityAttackFragment::bAttackRequested`만 세워
 * 위상 진행을 `ULNPEntityAttackProcessor`에 넘긴다(신호 구동인 Tick에 위상을 두면 스윙이 끊긴다).
 *
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
	TStateTreeExternalDataHandle<struct FLNPEntityAttackFragment> EntityAttackHandle;
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