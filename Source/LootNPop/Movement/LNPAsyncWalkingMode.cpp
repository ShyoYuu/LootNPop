// Copyright (c) 2026 LootNPop. All rights reserved.


#include "Movement/LNPAsyncWalkingMode.h"
#include "Movement/LNPCharacterMovementSettings.h"
#include "Components/PrimitiveComponent.h"
#include "DefaultMovementSet/Settings/CommonLegacyMovementSettings.h"
#include "MoverComponent.h"
#include "MoverDataModelTypes.h"
#include "MoverSimulationTypes.h"
#include "MoveLibrary/MoverBlackboard.h"

ULNPAsyncWalkingMode::ULNPAsyncWalkingMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SharedSettingsClasses.Add(ULNPCharacterMovementSettings::StaticClass());
}

void ULNPAsyncWalkingMode::SimulationTick_Implementation(const FSimulationTickParams& Params, FMoverTickEndData& OutputState)
{
	const FMoverDefaultSyncState* Start =
		Params.StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	const UPrimitiveComponent* Base = Start ? Start->GetMovementBase() : nullptr;
	if (Base && !Base->IsQueryCollisionEnabled() && CommonLegacySettings.IsValid())
	{
		// 엔진 정지 분기는 바닥을 재검사하지 않는다. 파괴된 기반의 캐시를 버리고 같은 틱에 낙하한다.
		FMoverDefaultSyncState& Out = OutputState.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FMoverDefaultSyncState>();
		Out = *Start;
		Out.SetTransforms_WorldSpace(Start->GetLocation_WorldSpace(), Start->GetOrientation_WorldSpace(),
			Start->GetVelocity_WorldSpace(), Start->GetAngularVelocityDegrees_WorldSpace(), nullptr);
		if (UMoverBlackboard* Blackboard = GetMoverComponent()->GetSimBlackboard_Mutable())
		{
			Blackboard->Invalidate(CommonBlackboard::LastFloorResult);
			Blackboard->Invalidate(CommonBlackboard::LastFoundDynamicMovementBase);
		}
		OutputState.MovementEndState.NextModeName = CommonLegacySettings->AirMovementModeName;
		OutputState.MovementEndState.RemainingMs = Params.TimeStep.StepMs;
		return;
	}
	Super::SimulationTick_Implementation(Params, OutputState);
}
