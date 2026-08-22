// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Movement/LNPDeadMode.h"

#include "MoverDataModelTypes.h"
#include "MoverSimulationTypes.h"

ULNPDeadMode::ULNPDeadMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 씬 컴포넌트를 건드리지 않고 상태만 복사하므로 워커 스레드에서 돌아도 안전하다.
	bSupportsAsync = true;
}

void ULNPDeadMode::SimulationTick_Implementation(const FSimulationTickParams& Params, FMoverTickEndData& OutputState)
{
	const FMoverDefaultSyncState* StartState =
		Params.StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();

	FMoverDefaultSyncState& OutState =
		OutputState.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FMoverDefaultSyncState>();

	if (StartState != nullptr)
	{
		// 시작 상태 되울림 — 위치·회전은 그대로, 속도는 0. 이걸 빼면 원점 텔레포트가 된다 (클래스 주석 참조).
		OutState.SetTransforms_WorldSpace(
			StartState->GetLocation_WorldSpace(),
			StartState->GetOrientation_WorldSpace(),
			FVector::ZeroVector,
			FVector::ZeroVector,
			StartState->GetMovementBase(),
			StartState->GetMovementBaseBoneName());
	}

	// 남은 시간을 전부 소비했다고 보고해 서브스텝 루프를 즉시 끝낸다.
	OutputState.MovementEndState.RemainingMs = 0.f;
}
