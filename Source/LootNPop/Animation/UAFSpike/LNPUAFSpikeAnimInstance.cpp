// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Animation/UAFSpike/LNPUAFSpikeAnimInstance.h"

#include "Animation/UAFSpike/LNPUAFRootFeedback.h"
#include "Animation/UAFSpike/LNPUAFMontageIKTrait.h"
#include "ChooserFunctionLibrary.h"
#include "Component/AnimNextComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Graph/AnimNextAnimationGraph.h"
#include "HAL/IConsoleManager.h"
#include "LootNPop.h"
#include "Module/AnimNextModule.h"
#include "Movement/LNPCharacterMoverComponent.h"
#include "MoveLibrary/BasedMovementUtils.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "Variables/AnimNextVariableReference.h"

void ULNPUAFSpikeAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	if (!bExplicitDriver)
		UpdateInputs(DeltaSeconds);
}

void ULNPUAFSpikeAnimInstance::InitializeExplicitDriver()
{
	if (bExplicitDriver)
		return;
	bExplicitDriver = true;
	InitializeMontageOnly();
	// MontageOnly의 초기화는 프로젝트 슬롯 그룹 등록·캐시 초기화를 호출하지 않는다.
	NativeInitializeAnimation();
	UE_LOG(LogLootNPop, Log, TEXT("[UAFSpikeDriver] %s source=%s generalAnimation=0"),
		*GetNameSafe(GetOwningActor()), *GetNameSafe(GetSkelMeshComponent()));
}

void ULNPUAFSpikeAnimInstance::UpdateFromDriver(float DeltaSeconds)
{
	check(IsInGameThread());
	if (LastDrivenFrame == GFrameCounter)
		return;
	LastDrivenFrame = GFrameCounter;
	UpdateInputs(DeltaSeconds);
}

void ULNPUAFSpikeAnimInstance::UpdateInputs(float DeltaSeconds)
{
	if (bExplicitDriver)
	{
		bHasOffsetRoot = false;
		if (UUAFComponent* UAF = GetOwningActor()->FindComponentByClass<UUAFComponent>())
		{
			const FUAFWeakSystemReference Reference = UAF->GetSystemReference();
			if (Reference.IsValid())
			{
				Reference.ReadComponent<FLNPUAFRootFeedback>([this](TConstStructView<FLNPUAFRootFeedback> Feedback)
				{
					bHasOffsetRoot = Feedback.Get().Read(OffsetRootTransform, RootFeedbackSerial);
				});
			}
		}
	}
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

	// 일반 경로는 숨김 메시 뒤, 명시적 드라이버는 폰의 입력 공급 틱 뒤에 평가한다.
	if (!bUAFDependencyAdded)
	{
		if (bExplicitDriver)
		{
			UAFComponent->AddPrerequisite(GetOwningActor(), GetOwningActor()->PrimaryActorTick, TEXT("PrePhysics"));
			// 표시 ABP가 이번 UAF 출력과 완료된 슬롯 가중치를 함께 읽게 한다.
			TInlineComponentArray<USkeletalMeshComponent*> Meshes(GetOwningActor());
			for (USkeletalMeshComponent* Mesh : Meshes)
				if (Mesh->GetFName() == TEXT("VisualMesh"))
				{
					UAFComponent->AddComponentSubsequent(Mesh, TEXT("PrePhysics"));
					Mesh->AddTickPrerequisiteComponent(GetSkelMeshComponent());
				}
		}
		else
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
	if (const USkeletalMeshComponent* Mesh = GetSkelMeshComponent())
	{
		Inputs.OffsetRoot.bValid = true;
		Inputs.OffsetRoot.MeshComponentTransform = Mesh->GetComponentTransform();
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

float ULNPUAFSpikeAnimInstance::ComputeLeftHandOffDemand() const
{
	if (!bExplicitDriver)
		return Super::ComputeLeftHandOffDemand();
	check(IsInGameThread());
	FLNPUAFHandIKSnapshot Snapshot;
	if (const UUAFComponent* UAF = GetOwningActor()->FindComponentByClass<UUAFComponent>())
	{
		const FUAFWeakSystemReference Reference = UAF->GetSystemReference();
		if (Reference.IsValid())
			Reference.ReadComponent<FLNPUAFHandIKFeedback>([&Snapshot](TConstStructView<FLNPUAFHandIKFeedback> Feedback)
			{
				Snapshot = Feedback.Get().Read();
			});
	}
	// 일반 평가가 꺼진 원본은 AnimInstanceProxy가 아니라 컴포넌트에 UAF 커브가 있다.
	const float Curve = GetSkelMeshComponent()->GetAnimCurves().Get(TEXT("DisableLHandIK"));
	const float Demand = Snapshot.GetDemand(Curve);
	const IConsoleVariable* Debug = IConsoleManager::Get().FindConsoleVariable(TEXT("LNP.Anim.DebugLocomotion"));
	if (Debug && Debug->GetInt() >= 2 && GFrameCounter % 60 == 0 && LastHandIKLogFrame != GFrameCounter)
	{
		LastHandIKLogFrame = GFrameCounter;
		UE_LOG(LogLootNPop, Log, TEXT("[UAFHandIK] owner=%s serial=%lld valid=%d slots=(%.3f,%.3f,%.3f) curve=%.3f demand=%.3f"),
			*GetNameSafe(GetOwningActor()), Snapshot.Serial, Snapshot.bValid,
			Snapshot.Weights[0], Snapshot.Weights[1], Snapshot.Weights[2], Curve, Demand);
	}
	return Demand;
}
