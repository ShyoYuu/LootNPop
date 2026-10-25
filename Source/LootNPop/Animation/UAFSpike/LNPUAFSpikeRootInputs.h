// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "BoneControllers/AnimNode_OffsetRootBone.h"
#include "Animation/TrajectoryTypes.h"
#include "LNPUAFSpikeRootInputs.generated.h"

/** 표시 메시의 구면 루트 보정에 필요한 프레임별 사본. 숨김 ABP의 가상 루트는 전달하지 않는다. */
USTRUCT(BlueprintType)
struct FLNPUAFSpikeRootInputs
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Offset Root")
	bool bValid = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Offset Root")
	int64 UpdateSerial = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Offset Root")
	FTransform MeshComponentTransform = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Offset Root")
	FTransform BasedMovementDelta = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Offset Root")
	EOffsetRootBoneMode TranslationMode = EOffsetRootBoneMode::Release;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Offset Root")
	EOffsetRootBoneMode RotationMode = EOffsetRootBoneMode::Accumulate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Offset Root")
	float TranslationHalfLife = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Offset Root")
	bool bOnGround = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Offset Root")
	bool bReset = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steering")
	bool bEnableMovementSteering = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steering")
	FTransformTrajectory Trajectory;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Offset Root")
	FName MovementBase;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Offset Root")
	bool bOnMovingBase = false;
};
