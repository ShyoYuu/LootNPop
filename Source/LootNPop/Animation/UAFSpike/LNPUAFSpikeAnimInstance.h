// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "Animation/LNPPhysicalHitReactAnimInstance.h"
#include "AlphaBlend.h"
#include "Animation/UAFSpike/LNPUAFSpikeRootInputs.h"
#include "Animation/UAFSpike/LNPUAFRifleAimTrait.h"
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

/**
 * 물리 히트리액트 CR(ControlRigPhysics)에 넘기는 값. 기존 RBWC 핀(힘·골반 목표·척추 배율·알파)을 바디 하나씩으로 편다.
 * 기본 모드 100(지점 임펄스·골반·척추 강도)만 담는다 — 힘 기록은 피격당 하나다.
 */
USTRUCT(BlueprintType)
struct FLNPUAFSpikePhysicsInputs
{
	GENERATED_BODY()

	/** Step Physics Solver 알파. 기존 RBWC와 같이 피격 구간에만 올라간다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	float Alpha = 0.0f;

	/** 새 피격마다 증가한다. CR은 값이 바뀐 실행에서만 힘을 한 번 더한다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	int32 ForceSerial = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	FName ForceBody = NAME_None;

	/** 기존 RBWC 핀과 같은 값(월드 공간·VelocityChange). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	FPhysicsControlForceAndTorqueData Force;

	/** 골반 키네마틱 목표 오프셋(월드 축). RBWC KinematicTargets의 pelvis 항목과 같다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	FTransform PelvisOffset = FTransform::Identity;

	/** 척추 컨트롤 각강도 배율. 피격 구간에만 1 아래로 내려간다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	float SpineAngularStrengthMultiplier = 1.0f;
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

	/** 라이플 표현 세트에서만 기본 자세·AO 오버레이를 활성화한다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rifle Overlay")
	float RifleOverlayWeight = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rifle Overlay")
	float AimYaw = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rifle Overlay")
	float AimPitch = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rifle Aim")
	FLNPUAFRifleAimInputs RifleAim;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	FLNPUAFSpikePhysicsInputs Physics;

	/** 사망 랙돌 중에는 시스템이 VisualMesh에 포즈를 쓰지 않는다. 컴포넌트 물리 블렌드가 표시 포즈를 소유한다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Display")
	bool bWriteDisplayPose = true;
};

/**
 * 기존 공급값을 UAF 표시 메시 시험에 전달한다. 명시적 드라이버에서는 숨김 그래프를 평가하지 않는다.
 * 물리 히트리액트 계산은 표시 ABP 부모와 같은 클래스를 상속해 그대로 재사용한다.
 */
UCLASS()
class LOOTNPOP_API ULNPUAFSpikeAnimInstance : public ULNPPhysicalHitReactAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	void InitializeExplicitDriver();
	void UpdateFromDriver(float DeltaSeconds);

protected:
	virtual float ComputeLeftHandOffDemand() const override;
	virtual bool IsMontageSlotActive(FName SlotName) const override;

	UPROPERTY(EditDefaultsOnly, Category = "LNP|UAFSpike")
	TObjectPtr<UUAFSystem> SpikeSystem;

	UPROPERTY(EditDefaultsOnly, Category = "LNP|UAFSpike")
	TObjectPtr<UUAFAnimGraph> SpikeGraph;

private:
	void UpdateInputs(float DeltaSeconds);
	/** 이번 업데이트의 RBWC 핀 값을 물리 CR 입력으로 편다. */
	FLNPUAFSpikePhysicsInputs MakePhysicsInputs();

	int32 PhysicsForceSerial = 0;
	bool bReportedExtraForces = false;

	TWeakObjectPtr<UUAFComponent> CachedUAFComponent;
	bool bUAFDependencyAdded = false;
	bool bHasReportedInputs = false;
	bool bLastInputsSucceeded = false;
	bool bWasFootPlacementEnabled = false;
	uint64 LastDrivenFrame = MAX_uint64;
	mutable uint64 LastHandIKLogFrame = MAX_uint64;
	uint8 LastLoggedRootModes = MAX_uint8;

	UPROPERTY(VisibleAnywhere, Category = "LNP|UAFSpike")
	bool bExplicitDriver = false;

	UPROPERTY(VisibleAnywhere, Category = "LNP|UAFSpike")
	int64 RootInputSerial = 0;

	UPROPERTY(VisibleAnywhere, Category = "LNP|UAFSpike")
	int64 RootFeedbackSerial = 0;
};
