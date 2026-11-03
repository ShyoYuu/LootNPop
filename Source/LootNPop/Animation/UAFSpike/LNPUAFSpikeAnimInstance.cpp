// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Animation/UAFSpike/LNPUAFSpikeAnimInstance.h"

#include "Animation/AnimMontage.h"
#include "Animation/UAFSpike/LNPUAFRootFeedback.h"
#include "Animation/UAFSpike/LNPUAFMontageIKTrait.h"
#include "Animation/UAFSpike/LNPUAFSpikeDriver.h"
#include "Character/LNPCharacterBase.h"
#include "Character/LNPInputHandlerComponent.h"
#include "ChooserFunctionLibrary.h"
#include "Component/AnimNextComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Graph/AnimNextAnimationGraph.h"
#include "HAL/IConsoleManager.h"
#include "Item/LNPWeaponVisualSet.h"
#include "Item/LNPWeaponData.h"
#include "LNPGameplayTags.h"
#include "LootNPop.h"
#include "Module/AnimNextModule.h"
#include "Movement/LNPCharacterMoverComponent.h"
#include "MoveLibrary/BasedMovementUtils.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "SurfaceNavigation/LNPMassWorldCollision.h"
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
			UAFComponent->AddComponentPrerequisite(GetOwningActor()->FindComponentByClass<ULNPUAFSpikeInputComponent>(), TEXT("PrePhysics"));
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
	const ULNPWeaponVisualSet* VisualSet = OwningMoverCharacter ? OwningMoverCharacter->GetAppliedVisualSet() : nullptr;
	Inputs.RifleOverlayWeight = VisualSet && VisualSet->AnimSetTag == TAG_VisualSet_Rifle ? 1.0f : 0.0f;
	// 기존 공급층이 완료된 UAF 루트를 기준으로 계산한 접평면 조준값을 그대로 전달한다.
	Inputs.AimYaw = AimYaw;
	Inputs.AimPitch = AimPitch;
	if (Inputs.RifleOverlayWeight > 0.0f && OwningMoverCharacter->IsLocallyControlled())
	{
		const auto* Handler = OwningMoverCharacter->FindComponentByClass<ULNPInputHandlerComponent>();
		const auto* Weapon = OwningMoverCharacter->GetWeaponMesh();
		const auto* Parent = Weapon ? Cast<USkeletalMeshComponent>(Weapon->GetAttachParent()) : nullptr;
		const ULNPWeaponData* Definition = OwningMoverCharacter->GetActiveWeaponDef();
		if (Handler && Parent && Definition && Weapon->DoesSocketExist(TEXT("Muzzle"))
			&& Weapon->GetAttachSocketName() == TEXT("weapon_r")
			&& Parent->GetSkeletalMeshAsset() == GetSkelMeshComponent()->GetSkeletalMeshAsset()
			&& Handler->GetCrosshairAimPoint(Inputs.RifleAim.AimTarget))
		{
			Inputs.RifleAim.bEnabled = true;
			Inputs.RifleAim.ViewDirection = OwningMoverCharacter->GetControlRotation().Vector();
			Inputs.RifleAim.MeshWorld = Parent->GetComponentTransform();
			Inputs.RifleAim.MuzzleInWeaponBone = Weapon->GetSocketTransform(TEXT("Muzzle"), RTS_Component) * Weapon->GetRelativeTransform();
			Inputs.RifleAim.MuzzleOffsetWorld = OwningMoverCharacter->GetActorTransform().TransformVector(Definition->MuzzleOffset);
			Inputs.RifleAim.ActorLocation = OwningMoverCharacter->GetActorLocation();
			Inputs.RifleAim.WorldCollision = GetWorld()->GetSubsystem<ULNPMassWorldCollisionSubsystem>();
		}
	}

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
	bSucceeded &= SetInput(GET_MEMBER_NAME_CHECKED(FLNPUAFSpikeInputs, RifleOverlayWeight), Inputs.RifleOverlayWeight);
	bSucceeded &= SetInput(GET_MEMBER_NAME_CHECKED(FLNPUAFSpikeInputs, AimYaw), Inputs.AimYaw);
	bSucceeded &= SetInput(GET_MEMBER_NAME_CHECKED(FLNPUAFSpikeInputs, AimPitch), Inputs.AimPitch);
	bSucceeded &= SetInput(GET_MEMBER_NAME_CHECKED(FLNPUAFSpikeInputs, RifleAim), Inputs.RifleAim);
	Inputs.Physics = MakePhysicsInputs();
	bSucceeded &= SetInput(GET_MEMBER_NAME_CHECKED(FLNPUAFSpikeInputs, Physics), Inputs.Physics);
	Inputs.bWriteDisplayPose = !(OwningMoverCharacter && OwningMoverCharacter->IsRagdollActive());
	bSucceeded &= SetInput(GET_MEMBER_NAME_CHECKED(FLNPUAFSpikeInputs, bWriteDisplayPose), Inputs.bWriteDisplayPose);
	bWasFootPlacementEnabled = Inputs.FootPlacement.bEnabled;

	if (!bHasReportedInputs || bSucceeded != bLastInputsSucceeded)
	{
		UE_LOG(LogLootNPop, Log, TEXT("[UAFSpike] %s inputs=%s databases=%d trajectorySamples=%d"),
			*GetNameSafe(GetOwningActor()), bSucceeded ? TEXT("ready") : TEXT("unbound"),
			Inputs.Databases.Num(), Inputs.Trajectory.Samples.Num());
		bHasReportedInputs = true;
		bLastInputsSucceeded = bSucceeded;
	}
	const IConsoleVariable* Debug = IConsoleManager::Get().FindConsoleVariable(TEXT("LNP.Anim.DebugLocomotion"));
	const uint8 RootModes = static_cast<uint8>(Inputs.OffsetRoot.TranslationMode) << 4 | static_cast<uint8>(Inputs.OffsetRoot.RotationMode);
	if (Debug && Debug->GetInt() >= 1 && RootModes != LastLoggedRootModes)
	{
		LastLoggedRootModes = RootModes;
		const UAnimMontage* Montage = GetCurrentActiveMontage();
		UE_LOG(LogLootNPop, Log, TEXT("[UAFRootMode] owner=%s frame=%llu translation=%s rotation=%s montage=%s position=%.4f"),
			*GetNameSafe(GetOwningActor()), GFrameCounter,
			*StaticEnum<EOffsetRootBoneMode>()->GetNameStringByValue(static_cast<int64>(Inputs.OffsetRoot.TranslationMode)),
			*StaticEnum<EOffsetRootBoneMode>()->GetNameStringByValue(static_cast<int64>(Inputs.OffsetRoot.RotationMode)),
			*GetNameSafe(Montage), Montage ? Montage_GetPosition(Montage) : -1.0f);
	}
	if (Debug && Debug->GetInt() >= 2 && GFrameCounter % 60 == 0)
		UE_LOG(LogLootNPop, Log, TEXT("[UAFWeapon] owner=%s serial=%lld inputs=%d rifle=%.1f aim=(%.2f,%.2f)"),
			*GetNameSafe(GetOwningActor()), RootInputSerial, bSucceeded, Inputs.RifleOverlayWeight, Inputs.AimYaw, Inputs.AimPitch);
}

FLNPUAFSpikePhysicsInputs ULNPUAFSpikeAnimInstance::MakePhysicsInputs()
{
	FLNPUAFSpikePhysicsInputs Physics;
	Physics.Alpha = PhysicsAlpha;
	// RBWC 핀 값(실제 월드 공간)을 그대로 넘긴다. ControlRigPhysics 솔버가 리그 소유 컴포넌트(바인딩된 원본 메시)의
	// 월드 변환으로 힘·지점·OffsetInWorldSpace 목표를 시뮬 공간으로 옮긴다.
	// 힘 핀은 피격이 있던 업데이트 한 번만 채워진다. 기본 모드는 지점 임펄스 기록 하나다.
	if (!HitForces.ForcesAndTorques.IsEmpty())
	{
		if (HitForces.ForcesAndTorques.Num() > 1 && !bReportedExtraForces)
		{
			UE_LOG(LogLootNPop, Warning, TEXT("[UAFPhysics] %s has %d force records; the spike rig applies only the first (diagnostic modes 1/2 are not supported)."),
				*GetNameSafe(GetOwningActor()), HitForces.ForcesAndTorques.Num());
			bReportedExtraForces = true;
		}
		const auto It = HitForces.ForcesAndTorques.CreateConstIterator();
		Physics.ForceBody = It.Key();
		Physics.Force = It.Value();
		++PhysicsForceSerial;
	}
	Physics.ForceSerial = PhysicsForceSerial;
	if (const FRigidBodyKinematicTarget* Pelvis = HitKinematicTargets.Targets.Find(TEXT("pelvis")))
		Physics.PelvisOffset = FTransform(Pelvis->TargetOrientation, Pelvis->TargetPosition);
	// 척추 항목들은 같은 배율을 쓴다(모드 64). 온몸 세트(모드 8)도 같은 값으로 척추에만 건다.
	if (!HitControlParameters.ControlMultiplierParameters.IsEmpty())
		Physics.SpineAngularStrengthMultiplier = HitControlParameters.ControlMultiplierParameters[0].Data.AngularStrengthMultiplier;
	// 그립과 앵커 본은 같은 (지난 프레임) 표시 포즈에서 읽어 상대 위치만 넘긴다. 표시 메시는 원본과 같은 스켈레톤이다.
	// 그립이 없어도 유효한 본을 넘긴다. CR의 Get Transform은 없는 본이면 매 실행 경고를 남긴다(알파 0이라 포즈는 그대로).
	Physics.LeftHandAnchorBone = TEXT("hand_l");
	const USkeletalMeshComponent* WeaponParent = WeaponMeshComp ? Cast<USkeletalMeshComponent>(WeaponMeshComp->GetAttachParent()) : nullptr;
	if (bHasLeftHandGrip && WeaponParent)
	{
		const FName AnchorBone = WeaponParent->GetSocketBoneName(WeaponMeshComp->GetAttachSocketName());
		const int32 AnchorIndex = WeaponParent->GetBoneIndex(AnchorBone);
		if (AnchorIndex != INDEX_NONE)
		{
			Physics.LeftHandIKAlpha = LeftHandIKAlpha;
			Physics.LeftHandAnchorBone = AnchorBone;
			Physics.LeftHandGripInAnchor = WeaponParent->GetBoneTransform(AnchorIndex).InverseTransformPosition(LeftHandGripLocation);
		}
	}
	return Physics;
}

bool ULNPUAFSpikeAnimInstance::IsMontageSlotActive(FName SlotName) const
{
	if (!bExplicitDriver)
		return Super::IsMontageSlotActive(SlotName);
	// UAF의 TickMontageOnly는 몽타주 인스턴스만 전진시키고 Proxy 평가 사본(IsSlotActive가 읽는 곳)을 갱신하지 않는다.
	// 엔진 UpdateMontageEvaluationData와 같은 기준(활성·슬롯)을 인스턴스에서 직접 읽는다. 공급이 UAF 틱보다 앞이라
	// 이번 프레임에 재생된 몽타주는 가중치가 아직 0이므로 가중치 필터는 두지 않는다.
	check(IsInGameThread());
	for (const FAnimMontageInstance* Instance : MontageInstances)
	{
		if (Instance && Instance->Montage && Instance->IsActive() && Instance->Montage->IsValidSlot(SlotName))
			return true;
	}
	return false;
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
