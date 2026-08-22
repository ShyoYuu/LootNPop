// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "MovementMode.h"
#include "LNPDeadMode.generated.h"

/**
 * 사망(랙돌) 연출 동안 캐릭터 이동을 완전히 정지시키는 모드.
 *
 * ⚠ 엔진의 `UNullMovementMode`를 쓰면 안 된다. 그쪽 `SimulationTick_Implementation`은 본문이 비어 있는데,
 * NP 백엔드는 매 틱 `FMoverTickEndData`를 **기본 생성**해서 넘기고
 * (`MoverNetworkPredictionLiaison.cpp:166` — `FMoverTickEndData EndData;`),
 * `UMovementModeStateMachine::OnSimulationTick`이 `FindOrAddMutableDataByType<FMoverDefaultSyncState>()`로
 * 기본값 구조체를 만들어 둔다. 아무도 채우지 않으면 위치가 `FVector::ZeroVector`인 채로
 * `UMoverComponent::FinalizeFrame` → `SetFrameStateFromContext`에 실려 **폰이 월드 원점으로 순간이동한다.**
 *
 * 그래서 이 모드는 "아무것도 안 하는" 대신 **시작 상태를 그대로 되울린다** — 위치·회전은 유지하고
 * 속도만 0으로 만든다. 입력에 의존하지 않으므로 결정론적이고 NP 리컨사일도 유발하지 않는다.
 */
UCLASS()
class LOOTNPOP_API ULNPDeadMode : public UBaseMovementMode
{
	GENERATED_BODY()

public:
	ULNPDeadMode(const FObjectInitializer& ObjectInitializer);

	virtual void SimulationTick_Implementation(const FSimulationTickParams& Params, FMoverTickEndData& OutputState) override;
};
