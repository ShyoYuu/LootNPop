// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "DefaultMovementSet/Modes/AsyncWalkingMode.h"
#include "LNPAsyncWalkingMode.generated.h"

/**
 * LootNPop 기본 걷기 모드.
 * 충돌이 제거된 이동 기반은 정지 중에도 해제한다. ULNPCharacterMovementSettings를
 * Shared Settings로 등록하여 Sprint/Guard/ADS/MoveSpeed Modifier가 설정 값을 조회할 수 있게 한다.
 */
UCLASS()
class LOOTNPOP_API ULNPAsyncWalkingMode : public UAsyncWalkingMode
{
	GENERATED_BODY()
	
public:
	ULNPAsyncWalkingMode(const FObjectInitializer& ObjectInitializer);
	virtual void SimulationTick_Implementation(const FSimulationTickParams& Params, FMoverTickEndData& OutputState) override;
private:
	friend class FLNPDestroyedMovementBaseTest;
};
