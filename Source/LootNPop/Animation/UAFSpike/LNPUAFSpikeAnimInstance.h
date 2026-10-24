// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "Animation/LNPAnimInstance.h"
#include "AlphaBlend.h"
#include "Animation/UAFSpike/LNPUAFSpikeRootInputs.h"
#include "LNPUAFSpikeAnimInstance.generated.h"

class UUAFAnimGraph;
class UUAFComponent;
class UUAFSystem;

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
};

/** 숨김 ABP의 기존 공급값을 UAF 표시 메시 시험에 전달한다. 기본 플레이어에는 사용하지 않는다. */
UCLASS()
class LOOTNPOP_API ULNPUAFSpikeAnimInstance : public ULNPAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "LNP|UAFSpike")
	TObjectPtr<UUAFSystem> SpikeSystem;

	UPROPERTY(EditDefaultsOnly, Category = "LNP|UAFSpike")
	TObjectPtr<UUAFAnimGraph> SpikeGraph;

private:
	TWeakObjectPtr<UUAFComponent> CachedUAFComponent;
	bool bUAFDependencyAdded = false;
	bool bHasReportedInputs = false;
	bool bLastInputsSucceeded = false;
	int64 RootInputSerial = 0;
};
