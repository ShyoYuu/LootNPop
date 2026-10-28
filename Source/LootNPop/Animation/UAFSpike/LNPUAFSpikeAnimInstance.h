// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "Animation/LNPAnimInstance.h"
#include "AlphaBlend.h"
#include "Animation/UAFSpike/LNPUAFSpikeRootInputs.h"
#include "LNPUAFSpikeAnimInstance.generated.h"

class UUAFAnimGraph;
class UUAFComponent;
class UUAFSystem;

/** 기존 발 배치 CR과 Passthrough Blend에 전달하는 스파이크 전용 입력. */
USTRUCT(BlueprintType)
struct FLNPUAFSpikeFootPlacementInputs
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foot Placement")
	FVector GroundNormal = FVector::UpVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foot Placement")
	bool bEnabled = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foot Placement")
	bool bForceReset = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foot Placement")
	bool bAllowFootPinning = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foot Placement")
	bool bAllowSlopeWarping = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foot Placement")
	bool bDebugDraw = false;
};

/** UAF 시스템과 그래프가 함께 가져오는 스파이크 전용 입력. */
USTRUCT(BlueprintType)
struct LOOTNPOP_API FLNPUAFSpikeInputs
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion Matching")
	TArray<TObjectPtr<UPoseSearchDatabase>> Databases;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion Matching")
	FTransformTrajectory Trajectory;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion Matching")
	EPoseSearchInterruptMode InterruptMode = EPoseSearchInterruptMode::DoNotInterrupt;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion Matching")
	FAlphaBlendArgs BlendArguments;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Offset Root")
	FLNPUAFSpikeRootInputs OffsetRoot;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foot Placement")
	FLNPUAFSpikeFootPlacementInputs FootPlacement;
};

/** 기존 공급값을 UAF 표시 메시 시험에 전달한다. 명시적 드라이버에서는 숨김 그래프를 평가하지 않는다. */
UCLASS()
class LOOTNPOP_API ULNPUAFSpikeAnimInstance : public ULNPAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	void InitializeExplicitDriver();
	void UpdateFromDriver(float DeltaSeconds);

protected:
	UPROPERTY(EditDefaultsOnly, Category = "LNP|UAFSpike")
	TObjectPtr<UUAFSystem> SpikeSystem;

	UPROPERTY(EditDefaultsOnly, Category = "LNP|UAFSpike")
	TObjectPtr<UUAFAnimGraph> SpikeGraph;

private:
	void UpdateInputs(float DeltaSeconds);

	TWeakObjectPtr<UUAFComponent> CachedUAFComponent;
	bool bUAFDependencyAdded = false;
	bool bHasReportedInputs = false;
	bool bLastInputsSucceeded = false;
	bool bWasFootPlacementEnabled = false;
	uint64 LastDrivenFrame = MAX_uint64;

	UPROPERTY(VisibleAnywhere, Category = "LNP|UAFSpike")
	bool bExplicitDriver = false;

	UPROPERTY(VisibleAnywhere, Category = "LNP|UAFSpike")
	int64 RootInputSerial = 0;

	UPROPERTY(VisibleAnywhere, Category = "LNP|UAFSpike")
	int64 RootFeedbackSerial = 0;
};
