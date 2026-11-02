// Copyright LootNPop. All Rights Reserved.

#include "Movement/LNPLayeredMove_DashMontageSync.h"

bool FLNPLayeredMove_DashMontageSync::GenerateMove(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep,
	const UMoverComponent* MoverComp, UMoverBlackboard* SimBlackboard, FProposedMove& OutProposedMove)
{
	// 부모의 "메시에 몽타주가 없으면 끝낸다" 검사만 건너뛰려고 재시뮬레이션 플래그를 세운 사본을 넘긴다.
	// 부모(UE 5.8)가 이 플래그를 읽는 곳은 그 검사와 디버그 로그뿐이다.
	FMoverTimeStep StepWithoutMeshCheck = TimeStep;
	StepWithoutMeshCheck.bIsResimulating = true;
	return Super::GenerateMove(StartState, StepWithoutMeshCheck, MoverComp, SimBlackboard, OutProposedMove);
}

FLayeredMoveBase* FLNPLayeredMove_DashMontageSync::Clone() const
{
	// 부모 Clone을 쓰면 롤백 복사에서 부모 타입으로 잘려 메시 검사가 되살아난다.
	return new FLNPLayeredMove_DashMontageSync(*this);
}

UScriptStruct* FLNPLayeredMove_DashMontageSync::GetScriptStruct() const
{
	return FLNPLayeredMove_DashMontageSync::StaticStruct();
}

FString FLNPLayeredMove_DashMontageSync::ToSimpleString() const
{
	return TEXT("LNP DashMontageSync");
}
