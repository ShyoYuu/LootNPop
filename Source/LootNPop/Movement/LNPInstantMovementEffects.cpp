// Copyright LootNPop. All Rights Reserved.

#include "Movement/LNPInstantMovementEffects.h"

#include "MoverDataModelTypes.h"
#include "MoverSimulationTypes.h"

namespace LNPInstantMovementEffectsPrivate
{
	// 부모가 컴포넌트에서 읽은 위치·회전만 서브스텝 시작 상태로 되돌린다. 속도·각속도·기반은 부모 결과를 그대로 둔다.
	void RestoreStartTransform(const FApplyMovementEffectParams& ApplyEffectParams, FMoverSyncState& OutputState)
	{
		const FMoverDefaultSyncState* Start = ApplyEffectParams.StartState
			? ApplyEffectParams.StartState->SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>()
			: nullptr;
		FMoverDefaultSyncState* Out = OutputState.SyncStateCollection.FindMutableDataByType<FMoverDefaultSyncState>();
		if (Start == nullptr || Out == nullptr)
		{
			return;
		}

		Out->SetTransforms_WorldSpace(Start->GetLocation_WorldSpace(), Start->GetOrientation_WorldSpace(),
			Out->GetVelocity_WorldSpace(), Out->GetAngularVelocityDegrees_WorldSpace(),
			Out->GetMovementBase(), Out->GetMovementBaseBoneName());
	}
}

bool FLNPJumpImpulseEffect::ApplyMovementEffect(FApplyMovementEffectParams& ApplyEffectParams, FMoverSyncState& OutputState)
{
	if (!Super::ApplyMovementEffect(ApplyEffectParams, OutputState))
	{
		return false;
	}

	LNPInstantMovementEffectsPrivate::RestoreStartTransform(ApplyEffectParams, OutputState);
	return true;
}

FInstantMovementEffect* FLNPJumpImpulseEffect::Clone() const
{
	// 부모 Clone을 쓰면 큐 복사에서 부모 타입으로 잘려 이 우회가 사라진다.
	return new FLNPJumpImpulseEffect(*this);
}

UScriptStruct* FLNPJumpImpulseEffect::GetScriptStruct() const
{
	return FLNPJumpImpulseEffect::StaticStruct();
}

FString FLNPJumpImpulseEffect::ToSimpleString() const
{
	return TEXT("LNP JumpImpulse");
}

bool FLNPApplyVelocityEffect::ApplyMovementEffect(FApplyMovementEffectParams& ApplyEffectParams, FMoverSyncState& OutputState)
{
	if (!Super::ApplyMovementEffect(ApplyEffectParams, OutputState))
	{
		return false;
	}

	LNPInstantMovementEffectsPrivate::RestoreStartTransform(ApplyEffectParams, OutputState);
	return true;
}

FInstantMovementEffect* FLNPApplyVelocityEffect::Clone() const
{
	return new FLNPApplyVelocityEffect(*this);
}

UScriptStruct* FLNPApplyVelocityEffect::GetScriptStruct() const
{
	return FLNPApplyVelocityEffect::StaticStruct();
}

FString FLNPApplyVelocityEffect::ToSimpleString() const
{
	return TEXT("LNP ApplyVelocity");
}
