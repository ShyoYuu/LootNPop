// Copyright LootNPop. All Rights Reserved.

#include "Movement/LNPGlideMode.h"
#include "Movement/LNPCharacterMoverComponent.h"

#include "MoverComponent.h"
#include "MoverDataModelTypes.h"
#include "MoveLibrary/AirMovementUtils.h"
#include "MoveLibrary/FloorQueryUtils.h"
#include "MoveLibrary/MoverBlackboard.h"
#include "MoveLibrary/MovementUtils.h"
#include "MoveLibrary/ConstrainedMoveUtils.h"
#include "DefaultMovementSet/Settings/CommonLegacyMovementSettings.h"

ULNPGlideMode::ULNPGlideMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 낙하 태그(Mover_IsInAir·Mover_IsFalling)는 부모가 이미 넣었다.
	GameplayTags.AddTag(LNP_Mover_IsGliding);
}

void ULNPGlideMode::GenerateMove_Implementation(const FMoverSimContext& SimContext, const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, FProposedMove& OutProposedMove) const
{
	// 엔진 UAsyncFallingMode::GenerateMove와 같은 구조다. 다른 점은 둘 — 수평 입력을 AirControlPercentage로 줄이지 않고
	// 활공 속력을 쓰는 것, 수직을 종단 속도 클램프 대신 목표 하강 속력으로 수렴시키는 것.
	const UMoverComponent* MoverComp = GetMoverComponent();
	const FCharacterDefaultInputs* CharacterInputs = StartState.InputCmd.InputCollection.FindDataByType<FCharacterDefaultInputs>();
	const FMoverDefaultSyncState* StartingSyncState = StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	check(StartingSyncState);

	if (!CommonLegacySettings.IsValid())
	{
		return;
	}

	const float DeltaSeconds = TimeStep.StepMs * 0.001f;
	const FVector UpDirection = MoverComp->GetUpDirection();

	const FVector StartVelocity = StartingSyncState->GetVelocity_WorldSpace();

	FFreeMoveParams Params;
	if (CharacterInputs)
	{
		Params.MoveInputType = CharacterInputs->GetMoveInputType();
		Params.MoveInput = UPlanarConstraintUtils::ConstrainDirectionToPlane(MoverComp->GetPlanarConstraint(), CharacterInputs->GetMoveInput_WorldSpace(), /*bMaintainInputMagnitude=*/true);
	}
	else
	{
		Params.MoveInputType = EMoveInputType::None;
		Params.MoveInput = FVector::ZeroVector;
	}
	Params.MoveInput = FVector::VectorPlaneProject(Params.MoveInput, UpDirection);

	FRotator IntendedOrientation_WorldSpace;
	if (!CharacterInputs || CharacterInputs->OrientationIntent.IsNearlyZero())
	{
		IntendedOrientation_WorldSpace = StartingSyncState->GetOrientation_WorldSpace();
	}
	else
	{
		IntendedOrientation_WorldSpace = CharacterInputs->GetOrientationIntentDir_WorldSpace().ToOrientationRotator();
	}
	IntendedOrientation_WorldSpace = UMovementUtils::ApplyGravityToOrientationIntent(IntendedOrientation_WorldSpace, MoverComp->GetWorldToGravityTransform(), CommonLegacySettings->bShouldRemainVertical);

	Params.OrientationIntent = IntendedOrientation_WorldSpace;
	Params.PriorVelocity = FVector::VectorPlaneProject(StartVelocity, UpDirection);
	Params.PriorOrientation = StartingSyncState->GetOrientation_WorldSpace();
	Params.DeltaSeconds = DeltaSeconds;
	Params.TurningRate = CommonLegacySettings->TurningRate;
	Params.TurningBoost = CommonLegacySettings->TurningBoost;
	Params.MaxSpeed = GlideSpeed;
	Params.Acceleration = GlideAcceleration;
	Params.Deceleration = GlideDeceleration;
	Params.WorldToGravityQuat = MoverComp->GetWorldToGravityTransform();
	Params.bUseAccelerationForVelocityMove = CommonLegacySettings->bUseAccelerationForVelocityMove;

	// 걸을 수 없는 벽에 붙어 있으면 벽 안쪽 입력을 버린다 — 엔진 낙하 모드와 같은 이유(벽을 타고 밀려 올라간다).
	if (const UMoverBlackboard* SimBlackboard = MoverComp->GetSimBlackboard())
	{
		FFloorCheckResult LastFloorResult;
		if (SimBlackboard->TryGet(CommonBlackboard::LastFloorResult, LastFloorResult)
			&& LastFloorResult.HitResult.IsValidBlockingHit()
			&& LastFloorResult.HitResult.Normal.Dot(UpDirection) > UE::MoverUtils::VERTICAL_SLOPE_NORMAL_MAX_DOT
			&& !LastFloorResult.IsWalkableFloor()
			&& FVector::DotProduct(Params.MoveInput, LastFloorResult.HitResult.Normal) < 0.f)
		{
			const FVector FallingHitNormal = FVector::VectorPlaneProject(LastFloorResult.HitResult.Normal, -UpDirection).GetSafeNormal();
			Params.MoveInput = FVector::VectorPlaneProject(Params.MoveInput, FallingHitNormal);
		}
	}

	OutProposedMove = UAirMovementUtils::ComputeControlledFreeMove(Params);

	// 수직: 목표 하강 속력으로 수렴. 위쪽(또는 느린 하강)에서는 중력으로, 빠른 하강에서는 GlideCatchDeceleration으로.
	const float TargetVerticalSpeed = -GlideDescentSpeed;
	const float StartVerticalSpeed = StartVelocity.Dot(UpDirection);
	float VerticalSpeed;
	if (StartVerticalSpeed > TargetVerticalSpeed)
	{
		const float GravityMagnitude = MoverComp->GetGravityAcceleration().Size();
		VerticalSpeed = FMath::Max(TargetVerticalSpeed, StartVerticalSpeed - GravityMagnitude * DeltaSeconds);
	}
	else
	{
		VerticalSpeed = FMath::Min(TargetVerticalSpeed, StartVerticalSpeed + GlideCatchDeceleration * DeltaSeconds);
	}
	UMovementUtils::SetGravityVerticalComponent(OutProposedMove.LinearVelocity, VerticalSpeed, UpDirection);
}
