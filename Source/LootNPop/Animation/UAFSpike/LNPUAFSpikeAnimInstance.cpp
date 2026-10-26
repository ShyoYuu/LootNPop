// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Animation/UAFSpike/LNPUAFSpikeAnimInstance.h"

#include "ChooserFunctionLibrary.h"
#include "Component/AnimNextComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Graph/AnimNextAnimationGraph.h"
#include "LootNPop.h"
#include "Module/AnimNextModule.h"
#include "Movement/LNPCharacterMoverComponent.h"
#include "MoveLibrary/BasedMovementUtils.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "Variables/AnimNextVariableReference.h"

void ULNPUAFSpikeAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (!IsLocomotionSource() || !SpikeSystem || !SpikeGraph)
		return;

	if (!CachedUAFComponent.IsValid())
	{
		AActor* Owner = GetOwningActor();
		CachedUAFComponent = Owner ? Owner->FindComponentByClass<UUAFComponent>() : nullptr;
		bUAFDependencyAdded = false;
	}

	UUAFComponent* UAFComponent = CachedUAFComponent.Get();
	if (!UAFComponent || !UAFComponent->GetSystemReference().IsValid())
		return;

	// 숨김 메시의 상태 갱신·포즈 평가가 끝난 뒤 UAF를 실행한다.
	if (!bUAFDependencyAdded)
	{
		UAFComponent->AddComponentPrerequisite(GetSkelMeshComponent(), TEXT("PrePhysics"));
		bUAFDependencyAdded = true;
	}

	FLNPUAFSpikeInputs Inputs;
	if (LocomotionDatabaseChooser)
	{
		for (UObject* Result : UChooserFunctionLibrary::EvaluateChooserMulti(this, LocomotionDatabaseChooser, UPoseSearchDatabase::StaticClass()))
		{
			if (UPoseSearchDatabase* Database = Cast<UPoseSearchDatabase>(Result))
				Inputs.Databases.Add(Database);
		}
	}
	Inputs.Trajectory = Trajectory;
	Inputs.InterruptMode = GetMMInterruptMode();
	Inputs.BlendArguments.BlendTime = GetMMBlendTime();
	TInlineComponentArray<USkeletalMeshComponent*> Meshes(GetOwningActor());
	for (USkeletalMeshComponent* Mesh : Meshes)
	{
		if (Mesh->GetFName() == TEXT("VisualMesh"))
		{
			Inputs.OffsetRoot.bValid = true;
			Inputs.OffsetRoot.MeshComponentTransform = Mesh->GetComponentTransform();
			break;
		}
	}
	Inputs.OffsetRoot.UpdateSerial = ++RootInputSerial;
	Inputs.OffsetRoot.BasedMovementDelta = BasedMovementDelta;
	Inputs.OffsetRoot.TranslationMode = GetOffsetRootTranslationMode();
	Inputs.OffsetRoot.RotationMode = GetOffsetRootRotationMode();
	Inputs.OffsetRoot.TranslationHalfLife = GetOffsetRootTranslationHalfLife();
	Inputs.OffsetRoot.bOnGround = MovementMode == ELNPMovementMode::OnGround;
	Inputs.OffsetRoot.bReset = bJustTeleported;
	Inputs.OffsetRoot.bEnableMovementSteering = bIsMoving || MovementMode == ELNPMovementMode::InAir;
	Inputs.OffsetRoot.Trajectory = Trajectory;
	Inputs.OffsetRoot.LastNonZeroVelocity = LastNonZeroVelocity;
	if (MoverComponent)
	{
		const UPrimitiveComponent* Base = MoverComponent->GetMovementBase();
		Inputs.OffsetRoot.MovementBase = Base ? FName(*Base->GetPathName()) : NAME_None;
		Inputs.OffsetRoot.bOnMovingBase = UBasedMovementUtils::IsADynamicBase(Base);
	}

	Inputs.FootPlacement.GroundNormal = GroundNormal;
	Inputs.FootPlacement.bEnabled = ShouldUseFootPlacementRig();
	// 알파 0에서 멈춘 CR의 핀·바닥 평활은 착지와 CVar 재활성화 때 버린다.
	Inputs.FootPlacement.bForceReset = ShouldResetFootPlacement()
		|| (Inputs.FootPlacement.bEnabled && !bWasFootPlacementEnabled);
	Inputs.FootPlacement.bAllowFootPinning = IsOnStaticGround();
	Inputs.FootPlacement.bAllowSlopeWarping = Inputs.OffsetRoot.bOnGround;
	Inputs.FootPlacement.bDebugDraw = IsFootPlacementDebugEnabled();

	const auto SetInput = [UAFComponent](FName Name, const auto& Value)
	{
		return UAFComponent->SetVariable(FAnimNextVariableReference::FromName(Name, FLNPUAFSpikeInputs::StaticStruct()), Value);
	};

	// 단일 그래프 시스템 템플릿의 Graph 변수는 UObject 참조다.
	bool bSucceeded = UAFComponent->SetVariable(FAnimNextVariableReference::FromName(TEXT("Graph"), SpikeSystem), TObjectPtr<UObject>(SpikeGraph));
	bSucceeded &= SetInput(GET_MEMBER_NAME_CHECKED(FLNPUAFSpikeInputs, Databases), Inputs.Databases);
	bSucceeded &= SetInput(GET_MEMBER_NAME_CHECKED(FLNPUAFSpikeInputs, Trajectory), Inputs.Trajectory);
	bSucceeded &= SetInput(GET_MEMBER_NAME_CHECKED(FLNPUAFSpikeInputs, InterruptMode), Inputs.InterruptMode);
	bSucceeded &= SetInput(GET_MEMBER_NAME_CHECKED(FLNPUAFSpikeInputs, BlendArguments), Inputs.BlendArguments);
	bSucceeded &= SetInput(GET_MEMBER_NAME_CHECKED(FLNPUAFSpikeInputs, OffsetRoot), Inputs.OffsetRoot);
	bSucceeded &= SetInput(GET_MEMBER_NAME_CHECKED(FLNPUAFSpikeInputs, FootPlacement), Inputs.FootPlacement);
	bWasFootPlacementEnabled = Inputs.FootPlacement.bEnabled;

	if (!bHasReportedInputs || bSucceeded != bLastInputsSucceeded)
	{
		UE_LOG(LogLootNPop, Log, TEXT("[UAFSpike] %s inputs=%s databases=%d trajectorySamples=%d"),
			*GetNameSafe(GetOwningActor()), bSucceeded ? TEXT("ready") : TEXT("unbound"),
			Inputs.Databases.Num(), Inputs.Trajectory.Samples.Num());
		bHasReportedInputs = true;
		bLastInputsSucceeded = bSucceeded;
	}
}
