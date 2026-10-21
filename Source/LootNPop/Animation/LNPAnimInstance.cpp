// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Animation/LNPAnimInstance.h"
#include "Character/LNPCharacterBase.h"
#include "Character/LNPPlayerCharacter.h"
#include "Item/LNPWeaponVisualSet.h"
#include "Movement/LNPCharacterMoverComponent.h"
#include "Movement/LNPModifierInputs.h"
#include "LootNPop.h"

#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "DefaultMovementSet/Settings/CommonLegacyMovementSettings.h"
#include "MoverPoseSearchTrajectoryPredictor.h"
#include "MoveLibrary/BasedMovementUtils.h"
#include "Chooser.h"
#include "ChooserFunctionLibrary.h"
#include "PoseSearch/MotionMatchingAnimNodeLibrary.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchTrajectoryLibrary.h"
#include "BoneControllers/AnimNode_OffsetRootBone.h"
#include "AnimationWarpingLibrary.h"
#include "BlendStack/BlendStackAnimNodeLibrary.h"
#include "Animation/AnimClassInterface.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/AnimSubsystem_Tag.h"
#include "StructUtils/InstancedStruct.h"
#include "Kismet/KismetMathLibrary.h"
#include "Animation/Skeleton.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/SkeletalMesh.h"
#include "HAL/IConsoleManager.h"

namespace
{
	TAutoConsoleVariable<int32> CVarDebugLocomotion(
		TEXT("LNP.Anim.DebugLocomotion"), 0,
		TEXT("Show the locomotion values ULNPAnimInstance supplies to the anim graph (states, predicates, trajectory).\n")
		TEXT("  0: off\n")
		TEXT("  1: text above every character with a main anim instance (player and promoted enemies)\n")
		TEXT("  2: text + one log line per state change and predicate edge (compare host and guest logs)"),
		ECVF_Cheat);

	TAutoConsoleVariable<int32> CVarFootPlacement(
		TEXT("LNP.Anim.FootPlacement"), 1,
		TEXT("Procedural_Feet foot placement Control Rig (slope warping, foot pinning, root damper) on the ground.\n")
		TEXT("  0: off (pose passes through)\n")
		TEXT("  1: on"),
		ECVF_Cheat);

	TAutoConsoleVariable<int32> CVarFootPlacementOnMovingBase(
		TEXT("LNP.Anim.FootPlacementOnMovingBase"), 0,
		TEXT("Allow foot pinning and the root damper of the foot placement Control Rig while standing on a moving movement base. 0: off (default), 1: on"),
		ECVF_Cheat);

	TAutoConsoleVariable<int32> CVarAdditiveLeans(
		TEXT("LNP.Anim.AdditiveLeans"), 1,
		TEXT("Additive lean poses while moving (body lean from lateral acceleration, plus head lean when not free-aiming or locked on).\n")
		TEXT("  0: off\n")
		TEXT("  1: on"),
		ECVF_Cheat);

	TAutoConsoleVariable<int32> CVarDebugFootPlacement(
		TEXT("LNP.Anim.DebugFootPlacement"), 0,
		TEXT("Drive the DebugDraw input of the foot placement Control Rig (legs, floor collision, pins). 0: off, 1: on"),
		ECVF_Cheat);

	/*
	 * B0 계산 상수 — GASP 5.8 SandboxCharacter_Mover_ABP·SandboxCharacter_Mover 값(AnimationOverhaul design/GASPReference.md §2.8).
	 * LootNPop에서 바꾼 것은 주석에 적는다.
	 */
	constexpr float TrajectoryHistoryInterval     = 0.033f;
	constexpr int32 TrajectoryHistoryCount        = 15;
	constexpr float TrajectoryPredictionInterval  = 0.1f;
	constexpr int32 TrajectoryPredictionCount     = 15;
	constexpr float HasVelocitySpeed              = 5.0f;
	constexpr float IsMovingSpeed                 = 10.0f;   // GASP: 벡터 성분별 허용 10. 여기서는 접평면 속력으로 잰다
	constexpr float StartingSpeedMargin           = 100.0f;
	constexpr float PivotTurnAngle                = 75.0f;
	constexpr float TurnInPlaceFacingDelta        = 50.0f;
	constexpr float TurnInPlaceMaxSpeed           = 50.0f;
	constexpr float SpinFacingDelta               = 130.0f;
	constexpr float SpinMinSpeed                  = 150.0f;
	constexpr float CirclingYawRate               = 200.0f;
	constexpr float CirclingTurnAngle             = 50.0f;
	constexpr float FacingDeltaJump               = 200.0f;
	constexpr float HeavyLandSpeedThreshold       = -500.0f;
	constexpr float TeleportThreshold             = 50.0f;   // GASP: 프레임 이동 거리. 여기서는 속도로 갈 수 있는 거리를 넘는 몫만 잰다(런처·활공·착지 오탐 방지)
	constexpr float GroundNormalInterpSpeed       = 5.0f;
	constexpr float MovementModeRecentTime        = 0.2f;
	constexpr float StateRecentTime               = 0.1f;
	constexpr float WalkGaitMaxSpeed              = 450.0f;  // LootNPop: 걷기 입력이 없어 현재 최대 속력으로 가른다. CHT_PSD_UEFN의 Walk/Run 경계와 같다

	// MM 블렌드 시간(GASP Get_MMBlendTime)
	constexpr float MMBlendTimeOnGround           = 0.5f;
	constexpr float MMBlendTimeLanding            = 0.2f;
	constexpr float MMBlendTimeInAirRising        = 0.15f;
	constexpr float MMBlendTimeInAir              = 0.5f;
	constexpr float MMRisingUpSpeed               = 100.0f;

	// 이동 방향 히스테리시스(GASP Get_MovementDirectionThresholds, DDCvar.StrafeStyle 0). 현재 방향이 F·B면 ±60/±120, 좌·우면 ±40/±140
	constexpr float DirFrontLimitFromFrontBack    = 60.0f;
	constexpr float DirBackLimitFromFrontBack     = 120.0f;
	constexpr float DirFrontLimitFromSide         = 40.0f;
	constexpr float DirBackLimitFromSide          = 140.0f;

	// Offset Root Bone 반감기(GASP Get_OffsetRootTranslationHalfLife)
	constexpr float OffsetRootHalfLifeIdle        = 0.1f;
	constexpr float OffsetRootHalfLifeMoving      = 0.3f;
	// 가상 루트 Yaw 오차 상한(도). GASP는 제한 없음(-1). 정지 중 룩 회전이 TurnInPlace보다 빠를 때 허리가 180°까지 꺾이는 것을 막는다(2-3)
	constexpr float OffsetRootMaxRotationError    = 90.0f;

	// Steering 목표 시간(GASP Get_DesiredFacing·Get_ProceduralTargetTime). 곡선 SteeringTargetTime 0~1을 이 범위로 옮긴다
	constexpr float SteeringTargetTimeMin         = 0.1f;
	constexpr float SteeringTargetTimeMax         = 1.5f;
	constexpr float SteeringProceduralTimeMin     = 0.1f;
	constexpr float SteeringProceduralTimeMax     = 0.3f;

	const FName OffsetRootNodeTag(TEXT("OffsetRoot"));
	const FName SteeringTargetTimeCurve(TEXT("SteeringTargetTime"));
	const FName EnableWarpingCurve(TEXT("Enable_Warping"));
	const FName TurnInPlaceDatabaseTag(TEXT("TurnInPlace"));

	/** Offset Root를 Release하는 전신 몽타주 슬롯. UpperBody(재장전)·가산 슬롯(사격·피격)은 로코모션 하체를 그대로 두므로 넣지 않는다 */
	const FName FullBodyMontageSlots[] = { TEXT("DefaultSlot"), TEXT("FullBody") };

	/** Rotation을 Up 쪽으로 최소 회전(스윙)해 Z축을 Up에 맞춘다. 접평면 안의 방향(Yaw)은 그대로 남는다 */
	FQuat AlignAxisZToUp(const FQuat& Rotation, const FVector& Up)
	{
		return (FQuat::FindBetweenNormals(Rotation.GetAxisZ(), Up) * Rotation).GetNormalized();
	}

	/** 블렌드 스택 입력 노드가 재생 중인 애니의 곡선 값(GASP: GetCurrentBlendStackAnimAsset → GetCurveValueFromAnimation) */
	float GetBlendStackCurveValue(const FAnimNodeReference& Node, FName CurveName)
	{
		float Value = 0.0f;
		if (const UAnimSequenceBase* Sequence = Cast<UAnimSequenceBase>(UBlendStackAnimNodeLibrary::GetCurrentBlendStackAnimAsset(Node)))
			UAnimationWarpingLibrary::GetCurveValueFromAnimation(Sequence, CurveName, UBlendStackAnimNodeLibrary::GetCurrentBlendStackAnimAssetTime(Node), Value);
		return Value;
	}

	/** From → To 회전각(도). Axis 둘레 부호각, UE 좌표에서 +는 오른쪽(Yaw 증가). 둘 중 하나라도 접평면 성분이 없으면 0 */
	float SignedAngleAroundAxis(const FVector& From, const FVector& To, const FVector& Axis)
	{
		const FVector A = FVector::VectorPlaneProject(From, Axis);
		const FVector B = FVector::VectorPlaneProject(To, Axis);
		if (A.IsNearlyZero() || B.IsNearlyZero())
			return 0.0f;
		return FMath::RadiansToDegrees(FMath::Atan2(FVector::DotProduct(FVector::CrossProduct(A, B), Axis), FVector::DotProduct(A, B)));
	}

	/** 두 facing 사이의 Up 축 회전각. 메시 facing은 Yaw만 다르므로 X축 방향을 비교한다 */
	float FacingDeltaAroundUp(const FQuat& From, const FQuat& To, const FVector& Up)
	{
		return SignedAngleAroundAxis(From.GetAxisX(), To.GetAxisX(), Up);
	}

	FVector TrajectoryVelocity(const FTransformTrajectory& InTrajectory, float Time1, float Time2, bool bExtrapolate = false)
	{
		FVector Out;
		UPoseSearchTrajectoryLibrary::GetTransformTrajectoryVelocity(InTrajectory, Time1, Time2, Out, bExtrapolate);
		return Out;
	}

	/** GASP UpdateStateValues 매크로. 값이 바뀌면 유지 시간을 0으로, 유지 시간이 RecentTimeLimit 이상이면 Recent를 갱신한다 */
	template <typename TEnum>
	void UpdateStateValues(TEnum NewState, TEnum& Current, TEnum& LastFrame, TEnum& Recent, float& TimeInState, float DeltaSeconds, float RecentTimeLimit)
	{
		LastFrame = Current;
		Current   = NewState;
		TimeInState = (Current != LastFrame) ? 0.0f : TimeInState + DeltaSeconds;
		if (TimeInState >= RecentTimeLimit)
			Recent = Current;
	}

	template <typename TEnum>
	FString EnumName(TEnum Value)
	{
		return StaticEnum<TEnum>()->GetNameStringByValue(static_cast<int64>(Value));
	}

	/**
	 * Lyra SK_Mannequin과 같은 몽타주 슬롯 → 그룹 표(AN-020).
	 * GASP SK_UEFN_Mannequin에는 DefaultGroup{DefaultSlot}·Partials{UpperBody}뿐이라, 그대로 두면
	 * 피격(AdditiveHitReact)이 공격과 같은 그룹이 되어 공격 몽타주를 끊고, 재장전(UpperBody + UpperBodyAdditive)은
	 * 그룹이 갈려 HasValidSlotSetup()에 걸려 재생되지 않는다.
	 */
	struct FLNPSlotGroupEntry
	{
		const TCHAR* SlotName;
		const TCHAR* GroupName;
	};

	constexpr FLNPSlotGroupEntry LNPLyraSlotGroups[] =
	{
		{ TEXT("DefaultSlot"),            TEXT("DefaultGroup") },
		{ TEXT("FullBody"),               TEXT("DefaultGroup") },
		{ TEXT("FullBodyAdditivePreAim"), TEXT("DefaultGroup") },
		{ TEXT("UpperBody"),              TEXT("DefaultGroup") },
		{ TEXT("UpperBodyAdditive"),      TEXT("DefaultGroup") },
		{ TEXT("AdditiveHitReact"),       TEXT("AdditiveGroup") },
	};

	/**
	 * 스켈레톤의 런타임 슬롯 그룹을 Lyra 표에 맞춘다. 이미 맞으면 아무것도 바꾸지 않는다(멱등).
	 * USkeleton::SetSlotGroupName은 패키지를 dirty로 만들지 않는다. 스켈레톤 에셋은 저장하지 않는다(AN-018) —
	 * 에디터에서 이 스켈레톤을 열어 저장하면 이 그룹이 디스크에 들어가므로 저장하지 말 것.
	 */
	void RegisterLyraSlotGroups(USkeleton* Skeleton)
	{
		if (Skeleton == nullptr)
			return;

		for (const FLNPSlotGroupEntry& Entry : LNPLyraSlotGroups)
		{
			const FName SlotName(Entry.SlotName);
			const FName GroupName(Entry.GroupName);
			if (Skeleton->ContainsSlotName(SlotName) && Skeleton->GetSlotGroupName(SlotName) == GroupName)
				continue;

			Skeleton->AddSlotGroupName(GroupName);
			Skeleton->SetSlotGroupName(SlotName, GroupName);
		}
	}
}

void ULNPAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	// 몽타주 그룹은 재생 시점에 스켈레톤에서 정해지므로 첫 Montage_Play 전에 맞춘다.
	if (const USkeletalMeshComponent* SkelMesh = GetSkelMeshComponent())
	{
		if (USkeletalMesh* MeshAsset = SkelMesh->GetSkeletalMeshAsset())
			RegisterLyraSlotGroups(MeshAsset->GetSkeleton());
	}

	// 성능 향상을 위한 참조 Cache
	OwningMoverCharacter = Cast<ALNPCharacterBase>(TryGetPawnOwner());
	if (OwningMoverCharacter != nullptr)
	{
		MoverComponent  = OwningMoverCharacter->GetMoverComponent();
		WeaponMeshComp  = OwningMoverCharacter->GetWeaponMesh();
	}
}

void ULNPAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	// 아직 Cache되지 않은 경우 캐릭터 Cache 시도 (지연 초기화)
	if (OwningMoverCharacter == nullptr)
	{
		OwningMoverCharacter = Cast<ALNPCharacterBase>(TryGetPawnOwner());

		if (OwningMoverCharacter != nullptr)
		{
			if (MoverComponent == nullptr)
				MoverComponent = OwningMoverCharacter->GetMoverComponent();
			if (WeaponMeshComp == nullptr)
				WeaponMeshComp = OwningMoverCharacter->GetWeaponMesh();
		}
	}

	if (OwningMoverCharacter == nullptr || MoverComponent == nullptr)
	{
		return;
	}

	// 1. 속도 및 지상 속력 업데이트
	Velocity = MoverComponent->GetVelocity();
	
	/**
	 * 구형 세계 보정:
	 * Size2D()는 World XY 평면에서만 동작한다.
	 * 캐릭터의 로컬 Up 벡터에 수직인 평면으로 속도를 투영해야 한다.
	 */
	FVector CharacterUp = OwningMoverCharacter->GetActorUpVector();
	FVector GroundVelocity = FVector::VectorPlaneProject(Velocity, CharacterUp);
	GroundSpeed = GroundVelocity.Size();

	// 2. 캐릭터가 이동해야 하는지 판단
	// 로직: 충분한 수평 속력과 의도적인 이동 입력이 있는 경우
	bShouldMove = (3.0f < GroundSpeed) && (!MoverComponent->GetMovementIntent().IsZero()); // 

	// 3. MoverComponent를 통한 공중 및 낙하 상태
	bIsOnGround = MoverComponent->IsOnGround();
	bIsAirborne = MoverComponent->IsAirborne();
	bIsFalling = MoverComponent->IsFalling();
	bIsSwimming = MoverComponent->IsSwimming();
	bIsCrouching  = MoverComponent->IsCrouching();
	bIsSprinting  = MoverComponent->IsSprinting();
	bIsGuarding   = MoverComponent->IsGuarding();
	const FLNPModifierInputs* ModifierInputs = MoverComponent->GetLastInputCmd().InputCollection.FindDataByType<FLNPModifierInputs>();
	bIsLockOn     = ModifierInputs && ModifierInputs->bIsLockOn;

	// 4. 이동 방향 계산 — 캐릭터 Up 기준 부호각(+ 오른쪽).
	// UKismetAnimationLibrary::CalculateDirection은 속도를 GetSafeNormal2D(월드 Z 제거)로 정규화해 적도에서 틀린 방향을 낸다.
	Direction = SignedAngleAroundAxis(OwningMoverCharacter->GetActorForwardVector(), Velocity, CharacterUp);

	// 5. 캐릭터가 스트레이핑해야 하는지 판단
	// 이동 방향으로 회전하지 않으면 스트레이핑 중으로 간주 (예: 타겟을 보며 이동).
	bShouldStrafe = !OwningMoverCharacter->GetFaceMoveDirection();

	// 6. Aim Offset용 Yaw/Pitch 계산
	// GetBaseAimRotation()은 플레이어의 경우 Control Rotation, AI는 별도 로직을 따름.
	// 구형 세계 보정: 월드 공간 Euler 각도 뺄셈은 캐릭터 Up이 World Up과 벌어질 때 오차가 생긴다.
	// 에임 방향 벡터를 캐릭터 로컬 좌표계로 변환한 뒤 Pitch/Yaw를 추출해 항상 올바른 로컬 에임 각도를 얻는다.
	// 기준은 메시가 실제로 향한 방향 — Offset Root Bone(B3)이 메시를 캡슐보다 늦게 돌리므로 그만큼 더한다(GASP도 오프셋 루트 기준).
	FRotator BaseAimRotation = OwningMoverCharacter->GetBaseAimRotation();
	const FQuat VisualFacing = GetOffsetRootRotationDelta() * OwningMoverCharacter->GetActorQuat();
	FVector LocalAimDir = VisualFacing.UnrotateVector(BaseAimRotation.Vector());
	FRotator LocalAimRot = LocalAimDir.Rotation();
	AimPitch = FMath::ClampAngle(LocalAimRot.Pitch, -90.0f, 90.0f);
	AimYaw   = FRotator::NormalizeAxis(LocalAimRot.Yaw);

	// 7. 왼손 IK 타겟 위치 업데이트 — 소켓이 우선, 없으면 표현 세트의 그립 오프셋(Lyra 총기 메시에는 소켓이 없다)
	static const FName LeftHandGripSocketName(TEXT("LeftHandGrip"));
	const ULNPWeaponVisualSet* VisualSet = OwningMoverCharacter->GetAppliedVisualSet();
	if (WeaponMeshComp && WeaponMeshComp->DoesSocketExist(LeftHandGripSocketName))
	{
		bHasLeftHandGrip     = true;
		LeftHandGripLocation = WeaponMeshComp->GetSocketLocation(LeftHandGripSocketName);
		LeftHandIKAlpha      = 1.0f;
	}
	else if (WeaponMeshComp && WeaponMeshComp->GetSkeletalMeshAsset() && VisualSet && VisualSet->bUseLeftHandGripOffset)
	{
		bHasLeftHandGrip     = true;
		LeftHandGripLocation = WeaponMeshComp->GetComponentTransform().TransformPosition(VisualSet->LeftHandGripOffset);
		LeftHandIKAlpha      = 1.0f - ComputeLeftHandOffDemand();
	}
	else
	{
		bHasLeftHandGrip = false;
		LeftHandIKAlpha  = 0.0f;
	}

	// 8. 견착 보정 — 무기 공간 보정량을 무기 부착 본 공간으로 옮긴다. 손·무기가 강체로 붙어 있어 값이 프레임마다 같다.
	RightHandIKOffset = FVector::ZeroVector;
	RightHandIKAlpha  = 0.0f;
	const USkeletalMeshComponent* WeaponParent = WeaponMeshComp ? Cast<USkeletalMeshComponent>(WeaponMeshComp->GetAttachParent()) : nullptr;
	if (WeaponParent && WeaponMeshComp->GetSkeletalMeshAsset() && VisualSet && !VisualSet->RightHandCorrection.IsNearlyZero())
	{
		const FName HandBone = WeaponParent->GetSocketBoneName(WeaponMeshComp->GetAttachSocketName());
		const FTransform HandWorld = WeaponParent->GetBoneTransform(WeaponParent->GetBoneIndex(HandBone));
		const FVector CorrectionWorld = WeaponMeshComp->GetComponentTransform().TransformVectorNoScale(VisualSet->RightHandCorrection);
		RightHandIKOffset = HandWorld.InverseTransformVectorNoScale(CorrectionWorld);
		RightHandIKAlpha  = 1.0f - ComputeLeftHandOffDemand();
	}

	// 9. 로코모션 공급층(B0) — 메인 인스턴스만
	if (IsLocomotionSource())
	{
		if (!bLocomotionSourceInitialized)
			InitializeLocomotionSource();
		UpdateLocomotionValues(DeltaSeconds);
	}
}

void ULNPAnimInstance::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeThreadSafeUpdateAnimation(DeltaSeconds);

	// 그래프 갱신 직전(워커). 메인 인스턴스의 OffsetRootBone 노드에 base 이동량과 Up 정렬을 넣는다 — 노드 Update가 이 값에서 시작한다.
	FAnimNode_OffsetRootBone* OffsetRootNode = nullptr;
	if (bLocomotionSourceInitialized)
	{
		if (const IAnimClassInterface* AnimClass = IAnimClassInterface::GetFromClass(GetClass()))
		{
			if (const FAnimSubsystem_Tag* TagSubsystem = AnimClass->FindSubsystem<FAnimSubsystem_Tag>())
				OffsetRootNode = TagSubsystem->FindNodeByTag<FAnimNode_OffsetRootBone>(OffsetRootNodeTag, this);
		}
	}
	bHasOffsetRoot = OffsetRootNode != nullptr;
	if (OffsetRootNode == nullptr)
		return;

	// GASP OnUpdate_OffsetRootBone: 움직이는 base가 캐릭터를 옮긴 만큼 가상 루트도 옮긴다(안 하면 base 위에서 메시가 뒤처진다)
	FVector Translation = OffsetRootNode->GetOffsetRootTranslation() + BasedMovementDelta.GetTranslation();
	FQuat Rotation = BasedMovementDelta.GetRotation() * OffsetRootNode->GetOffsetRootRotation();

	// 구면: Accumulate·Interpolate 모드는 캡슐 회전 변화를 가상 루트에 더하지 않아, 구면을 따라 이동하면 캡슐 Up만 돌고 메시가 기운다.
	// 매 프레임 가상 루트 Up을 컴포넌트 Up으로 스윙만 맞춘다(Yaw 오프셋은 노드 몫으로 남긴다).
	const FTransform& ComponentTransform = GetProxyOnAnyThread<FAnimInstanceProxy>().GetComponentTransform();
	Rotation = AlignAxisZToUp(Rotation, ComponentTransform.GetUnitAxis(EAxis::Z));

	// 회전 오차 제한(정지 중 룩 회전 안전망, 2-3). 노드의 MaxRotationError는 끈다(-1): 엔진이 오차를 컴포넌트 로컬 축으로 구하고
	// RotationOffset * ComponentRotation(월드 쪽)으로 다시 곱해, 적도에서는 로컬 Yaw가 월드 Z 회전으로 들어가 가상 루트가 최대 90° 기운다(2-5).
	// Up 정렬 뒤라 남은 오차는 컴포넌트 Up 둘레 회전뿐이다 → 로컬 Z 각으로 자른다.
	const FQuat ComponentRotation = ComponentTransform.GetRotation();
	FVector OffsetAxis;
	float OffsetAngle;
	(ComponentRotation.Inverse() * Rotation).GetNormalized().ToAxisAndAngle(OffsetAxis, OffsetAngle);
	const float SignedYaw = FMath::UnwindRadians(OffsetAxis.Z >= 0.0f ? OffsetAngle : -OffsetAngle);
	const float MaxYaw = FMath::DegreesToRadians(OffsetRootMaxRotationError);
	if (FMath::Abs(SignedYaw) > MaxYaw)
		Rotation = (ComponentRotation * FQuat(FVector::UpVector, FMath::Sign(SignedYaw) * MaxYaw)).GetNormalized();

	OffsetRootNode->SetOffsetRootTranslation(Translation);
	OffsetRootNode->SetOffsetRootRotation(Rotation);
	OffsetRootTransform = FTransform(Rotation, Translation);
}

bool ULNPAnimInstance::IsLocomotionSource() const
{
	return OwningMoverCharacter != nullptr && OwningMoverCharacter->GetAnimInstance() == this;
}

void ULNPAnimInstance::InitializeLocomotionSource()
{
	bLocomotionSourceInitialized = true;

	TrajectoryPredictor = NewObject<UMoverTrajectoryPredictor>(this);
	TrajectoryPredictor->Setup(MoverComponent);

	MoverComponent->OnBasedMovementApplied.AddUniqueDynamic(this, &ULNPAnimInstance::HandleBasedMovementApplied);
}

void ULNPAnimInstance::HandleBasedMovementApplied(const FTransform& TransformDelta, const FMoverTimeStep& TimeStep)
{
	// 애니 갱신 사이에 시뮬 틱이 여러 번 돌 수 있어 덮어쓰지 않고 누적한다(GASP는 마지막 값만 보관)
	PendingBasedMovementDelta = PendingBasedMovementDelta * TransformDelta;
}

void ULNPAnimInstance::UpdateLocomotionValues(float DeltaSeconds)
{
	const FTransform ActorTransform = OwningMoverCharacter->GetActorTransform();
	const FVector Up       = ActorTransform.GetUnitAxis(EAxis::Z);
	LocomotionUp = Up;
	// 텔레포트 판정은 메시 위치로 한다. 시뮬 프록시는 Actor가 네트워크 값으로 스냅하고 메시만 평활되므로 Actor로 재면 스냅마다 오탐한다
	const FVector Location = GetSkelMeshComponent()->GetComponentLocation();
	const float SafeDeltaSeconds = FMath::Max(DeltaSeconds, 0.001f);

	// --- 궤적 생성 (GASP Update_Trajectory 앞부분) ---
	if (TrajectoryPredictor)
	{
		FTransformTrajectory NewTrajectory;
		UPoseSearchTrajectoryLibrary::PoseSearchGenerateTransformTrajectoryWithPredictor(
			TrajectoryPredictor.Get(), DeltaSeconds, Trajectory, PreviousDesiredControllerYaw, NewTrajectory,
			TrajectoryHistoryInterval, TrajectoryHistoryCount, TrajectoryPredictionInterval, TrajectoryPredictionCount);
		Trajectory = MoveTemp(NewTrajectory);
	}

	// --- 필수값 (GASP Update_EssentialValues) ---
	Velocity_LastFrame = bHasLocomotionHistory ? PreviousVelocity : Velocity;

	// 두 프레임 속력 중 큰 쪽으로 갈 수 있는 거리 + 50cm를 넘게 움직였을 때만. 고속 낙하 착지 프레임(직전 −3000cm/s → 0)을 오탐하지 않는다.
	// 움직이는 base가 옮긴 몫은 뺀다 — Mover 속도는 base 기준이라 발판 위에 서 있으면 0인데 메시는 발판과 함께 움직인다(2-5: 발 배치 CR이 매 프레임 리셋됐다)
	const float ReachableDistance = TeleportThreshold + FMath::Max(Velocity.Size(), Velocity_LastFrame.Size()) * DeltaSeconds;
	const FVector OwnDisplacement = Location - Location_LastFrame - PendingBasedMovementDelta.GetTranslation();
	bJustTeleported = bHasLocomotionHistory && OwnDisplacement.SizeSquared() > FMath::Square(ReachableDistance);

	const FVector PlaneVelocity          = FVector::VectorPlaneProject(Velocity, Up);
	const FVector PlaneVelocityLastFrame = FVector::VectorPlaneProject(Velocity_LastFrame, Up);
	bHasVelocity = GroundSpeed > HasVelocitySpeed;
	if (bHasVelocity)
		LastNonZeroVelocity = PlaneVelocity;

	VelocityAcceleration = (bHasLocomotionHistory && !bJustTeleported)
		? (PlaneVelocity - PlaneVelocityLastFrame) / SafeDeltaSeconds
		: FVector::ZeroVector;
	RelativeAcceleration = ActorTransform.InverseTransformVectorNoScale(VelocityAcceleration);

	const UCommonLegacyMovementSettings* MoveSettings = MoverComponent->FindSharedSettings<UCommonLegacyMovementSettings>();
	const float MaxAcceleration = MoveSettings ? MoveSettings->Acceleration : 0.0f;
	const FVector PlaneIntent = FVector::VectorPlaneProject(MoverComponent->GetMovementIntent(), Up).GetClampedToMaxSize(1.0f);
	Acceleration       = PlaneIntent * MaxAcceleration;
	AccelerationAmount = MaxAcceleration > 0.0f ? Acceleration.Size() / MaxAcceleration : 0.0f;
	bHasAcceleration   = AccelerationAmount > 0.0f;

	// 지면 — Mover가 마지막 시뮬에서 찾은 바닥. 공중이면 캐릭터 Up(GASP는 월드 Z)
	FHitResult FloorHit;
	if (bIsOnGround && MoverComponent->TryGetFloorCheckHitResult(FloorHit) && FloorHit.bBlockingHit)
	{
		GroundNormal   = FloorHit.ImpactNormal;
		GroundLocation = FloorHit.ImpactPoint;
	}
	else
	{
		GroundNormal   = Up;
		GroundLocation = GetSkelMeshComponent()->GetComponentLocation();
	}
	bOnMovingBase = UBasedMovementUtils::IsADynamicBase(MoverComponent->GetMovementBase());

	SmoothedGroundNormal = bHasLocomotionHistory
		? FMath::VInterpTo(SmoothedGroundNormal, GroundNormal, DeltaSeconds, GroundNormalInterpSpeed).GetSafeNormal(UE_SMALL_NUMBER, Up)
		: GroundNormal;
	UKismetMathLibrary::GetSlopeDegreeAngles(ActorTransform.GetUnitAxis(EAxis::Y), SmoothedGroundNormal, Up, SlopeAngle_Y, SlopeAngle_X);

	BasedMovementDelta = PendingBasedMovementDelta;
	PendingBasedMovementDelta = FTransform::Identity;

	UpdateTrajectoryValues(DeltaSeconds);
	UpdateStates(DeltaSeconds);

	// --- Additive Lean 입력 (GASP Update_AdditiveLean) ---
	const FVector VelocityRight = FVector::CrossProduct(Up, PlaneVelocity.GetSafeNormal());
	const float LateralAccelerationAmount = FMath::Clamp(
		FVector::DotProduct(VelocityAcceleration, VelocityRight)
			/ FMath::GetMappedRangeValueClamped(FVector2f(200.0f, 320.0f), FVector2f(500.0f, 800.0f), GroundSpeed),
		-1.0f, 1.0f);
	switch (MovementDirection)
	{
	case ELNPMovementDirection::F: LeanAmount = FVector2D(LateralAccelerationAmount, 0.0f);  break;
	case ELNPMovementDirection::B: LeanAmount = FVector2D(-LateralAccelerationAmount, 0.0f); break;
	case ELNPMovementDirection::L: LeanAmount = FVector2D(0.0f, LateralAccelerationAmount);  break;
	case ELNPMovementDirection::R: LeanAmount = FVector2D(0.0f, -LateralAccelerationAmount); break;
	}

	PreviousVelocity      = Velocity;
	Location_LastFrame    = Location;
	bHasLocomotionHistory = true;

	DrawLocomotionDebug();
}

void ULNPAnimInstance::UpdateTrajectoryValues(float DeltaSeconds)
{
	if (Trajectory.Samples.IsEmpty())
		return;

	const FVector Up = OwningMoverCharacter->GetActorUpVector();
	const FVector PlaneVelocity = FVector::VectorPlaneProject(Velocity, Up);

	Trj_PastVelocity           = FVector::VectorPlaneProject(TrajectoryVelocity(Trajectory, -0.3f, -0.2f), Up);
	Trj_NearFutureVelocity     = FVector::VectorPlaneProject(TrajectoryVelocity(Trajectory, 0.1f, 0.2f), Up);
	Trj_PreviousFutureVelocity = Trj_FutureVelocity;
	Trj_FutureVelocity         = FVector::VectorPlaneProject(TrajectoryVelocity(Trajectory, 0.4f, 0.5f), Up);
	Trj_TurnAngle              = SignedAngleAroundAxis(PlaneVelocity, Trj_FutureVelocity, Up);
	Trj_FutureFacing           = Trajectory.GetSampleAtTime(1.5f).Facing;

	// 지금 메시 facing에서 예측 0·0.25·0.75·1.5s facing까지 누적(GASP Get_TotalFacingDelta).
	// 첫 기준은 Offset Root Bone 가상 루트(메시가 실제로 향한 방향), 노드가 없으면 메시 컴포넌트.
	static constexpr float FacingTimes[] = { 0.0f, 0.25f, 0.75f, 1.5f };
	FQuat PreviousFacing = bHasOffsetRoot ? OffsetRootTransform.GetRotation() : GetSkelMeshComponent()->GetComponentQuat();
	float TotalFacingDelta = 0.0f;
	for (const float Time : FacingTimes)
	{
		const FQuat Facing = Trajectory.GetSampleAtTime(Time).Facing;
		TotalFacingDelta += FacingDeltaAroundUp(PreviousFacing, Facing, Up);
		PreviousFacing = Facing;
	}
	FutureFacingDelta_LastFrame = FutureFacingDelta;
	FutureFacingDelta = TotalFacingDelta;
	bForceMovementDirectionChange = FMath::Abs(FutureFacingDelta - FutureFacingDelta_LastFrame) > FacingDeltaJump;

	FVector PastAngularVelocity, CurrentAngularVelocity;
	UPoseSearchTrajectoryLibrary::GetTransformTrajectoryAngularVelocity(Trajectory, -0.4f, -0.3f, PastAngularVelocity);
	UPoseSearchTrajectoryLibrary::GetTransformTrajectoryAngularVelocity(Trajectory, 0.0f, 0.1f, CurrentAngularVelocity);
	Trj_PastYawRate    = FVector::DotProduct(PastAngularVelocity, Up);
	Trj_CurrentYawRate = FVector::DotProduct(CurrentAngularVelocity, Up);

	bTrj_IsCircling = ((Trj_PastYawRate < -CirclingYawRate && Trj_CurrentYawRate < -CirclingYawRate)
	                || (Trj_PastYawRate >  CirclingYawRate && Trj_CurrentYawRate >  CirclingYawRate))
	               && FMath::Abs(Trj_TurnAngle) > CirclingTurnAngle;
	Trj_CirclingTime = bTrj_IsCircling ? Trj_CirclingTime + DeltaSeconds : 0.0f;
}

ELNPMovementDirection ULNPAnimInstance::ComputeMovementDirection(ELNPRotationMode NewRotationMode, ELNPGait NewGait) const
{
	// GASP 폰 Get_MovementDirectionAndOffset: 이동 방향 회전이거나 질주면 항상 F, 속도가 없으면 F
	if (NewRotationMode == ELNPRotationMode::OrientToMovement || NewGait == ELNPGait::Sprint || !bHasVelocity)
		return ELNPMovementDirection::F;

	// Direction = 캐릭터 회전 기준 속도 방향(-180~180, 로컬 좌표로 계산해 구면에서도 유효)
	const bool bFromFrontBack = MovementDirection == ELNPMovementDirection::F || MovementDirection == ELNPMovementDirection::B;
	const float FrontLimit = bFromFrontBack ? DirFrontLimitFromFrontBack : DirFrontLimitFromSide;
	const float BackLimit  = bFromFrontBack ? DirBackLimitFromFrontBack  : DirBackLimitFromSide;
	const float AbsAngle = FMath::Abs(Direction);
	if (AbsAngle <= FrontLimit)
		return ELNPMovementDirection::F;
	if (AbsAngle >= BackLimit)
		return ELNPMovementDirection::B;
	return Direction < 0.0f ? ELNPMovementDirection::L : ELNPMovementDirection::R;
}

void ULNPAnimInstance::UpdateStates(float DeltaSeconds)
{
	const FVector Up = OwningMoverCharacter->GetActorUpVector();

	// GASP IsMoving: 궤적 0.9~1.0s 속도(외삽)가 0이 아니다
	bIsMoving = Trajectory.Samples.IsEmpty()
		? GroundSpeed > IsMovingSpeed
		: FVector::VectorPlaneProject(TrajectoryVelocity(Trajectory, 0.9f, 1.0f, true), Up).Size() > IsMovingSpeed;

	const UCommonLegacyMovementSettings* MoveSettings = MoverComponent->FindSharedSettings<UCommonLegacyMovementSettings>();
	ELNPGait NewGait = bIsSprinting ? ELNPGait::Sprint
		: (MoveSettings && MoveSettings->MaxSpeed < WalkGaitMaxSpeed) ? ELNPGait::Walk : ELNPGait::Run;
	if (NewGait == ELNPGait::Sprint && bTrj_IsCircling)
		NewGait = ELNPGait::Run;

	const ELNPRotationMode NewRotationMode = !bShouldStrafe ? ELNPRotationMode::OrientToMovement
		: MoverComponent->IsADS() ? ELNPRotationMode::Aim : ELNPRotationMode::Strafe;
	const ELNPMovementDirection NewDirection = ComputeMovementDirection(NewRotationMode, NewGait);

	if (bForceMovementDirectionChange)
	{
		MovementDirection        = ELNPMovementDirection::B;
		MovementDirection_Recent = ELNPMovementDirection::B;
	}

	const bool bLog = CVarDebugLocomotion.GetValueOnGameThread() >= 2;
	FString Changes;
	auto Track = [&Changes, bLog](const TCHAR* Label, const FString& From, const FString& To)
	{
		if (bLog && From != To)
			Changes += FString::Printf(TEXT(" %s %s->%s"), Label, *From, *To);
	};
	const ELNPMovementMode      PrevMode = MovementMode;
	const ELNPMovementState     PrevState = MovementState;
	const ELNPGait              PrevGait = Gait;
	const ELNPRotationMode      PrevRotation = RotationMode;
	const ELNPMovementDirection PrevDirection = MovementDirection;

	UpdateStateValues(bIsOnGround ? ELNPMovementMode::OnGround : ELNPMovementMode::InAir,
		MovementMode, MovementMode_LastFrame, MovementMode_Recent, MovementMode_Time, DeltaSeconds, MovementModeRecentTime);
	UpdateStateValues(NewRotationMode,
		RotationMode, RotationMode_LastFrame, RotationMode_Recent, RotationMode_Time, DeltaSeconds, StateRecentTime);
	UpdateStateValues(bIsMoving ? ELNPMovementState::Moving : ELNPMovementState::Idle,
		MovementState, MovementState_LastFrame, MovementState_Recent, MovementState_Time, DeltaSeconds, StateRecentTime);
	UpdateStateValues(NewGait,
		Gait, Gait_LastFrame, Gait_Recent, Gait_Time, DeltaSeconds, StateRecentTime);
	UpdateStateValues(bIsCrouching ? ELNPStance::Crouch : ELNPStance::Stand,
		Stance, Stance_LastFrame, Stance_Recent, Stance_Time, DeltaSeconds, StateRecentTime);
	UpdateStateValues(NewDirection,
		MovementDirection, MovementDirection_LastFrame, MovementDirection_Recent, MovementDirection_Time, DeltaSeconds, StateRecentTime);

	// --- 술어 ---
	const bool bWasPivoting = bIsPivoting, bWasTurnInPlace = bShouldTurnInPlace, bWasSpin = bShouldSpinTransition, bWasStarting = bIsStarting;

	bIsStarting           = bIsMoving && Trj_FutureVelocity.Size() >= GroundSpeed + StartingSpeedMargin;
	bIsPivoting           = MovementState == ELNPMovementState::Moving && FMath::Abs(Trj_TurnAngle) >= PivotTurnAngle && !bTrj_IsCircling;
	bShouldTurnInPlace    = FMath::Abs(FutureFacingDelta) >= TurnInPlaceFacingDelta && GroundSpeed < TurnInPlaceMaxSpeed;
	bShouldSpinTransition = FMath::Abs(FutureFacingDelta) >= SpinFacingDelta && GroundSpeed >= SpinMinSpeed;

	const bool bJustLanded = MovementMode == ELNPMovementMode::OnGround && MovementMode_LastFrame == ELNPMovementMode::InAir;
	const float LandUpSpeed = FVector::DotProduct(Velocity_LastFrame, Up);
	bJustLanded_Light = bJustLanded && LandUpSpeed >  HeavyLandSpeedThreshold;
	bJustLanded_Heavy = bJustLanded && LandUpSpeed <= HeavyLandSpeedThreshold;
	if (bJustLanded)
		LandVelocity = Velocity_LastFrame;

	if (!bLog)
		return;

	Track(TEXT("Mode"),      EnumName(PrevMode),      EnumName(MovementMode));
	Track(TEXT("State"),     EnumName(PrevState),     EnumName(MovementState));
	Track(TEXT("Gait"),      EnumName(PrevGait),      EnumName(Gait));
	Track(TEXT("Rotation"),  EnumName(PrevRotation),  EnumName(RotationMode));
	Track(TEXT("Direction"), EnumName(PrevDirection), EnumName(MovementDirection));
	if (bIsStarting && !bWasStarting)                Changes += TEXT(" +Starting");
	if (bIsPivoting && !bWasPivoting)                Changes += FString::Printf(TEXT(" +Pivot(turn %.0f)"), Trj_TurnAngle);
	if (bShouldTurnInPlace && !bWasTurnInPlace)      Changes += FString::Printf(TEXT(" +TurnInPlace(facing %.0f)"), FutureFacingDelta);
	if (bShouldSpinTransition && !bWasSpin)          Changes += FString::Printf(TEXT(" +Spin(facing %.0f)"), FutureFacingDelta);
	if (bJustLanded)                                 Changes += FString::Printf(TEXT(" +Landed_%s(up %.0f)"), bJustLanded_Heavy ? TEXT("Heavy") : TEXT("Light"), LandUpSpeed);
	if (bJustTeleported)                             Changes += TEXT(" +Teleported");
	if (bForceMovementDirectionChange)               Changes += TEXT(" +FacingJump");

	if (!Changes.IsEmpty())
	{
		const UWorld* World = GetWorld();
		UE_LOG(LogLootNPop, Log, TEXT("[AnimB0] %s %s/%s speed %.0f t=%.2f:%s"),
			*GetNameSafe(OwningMoverCharacter),
			World && World->GetNetMode() == NM_Client ? TEXT("Client") : TEXT("Server"),
			*StaticEnum<ENetRole>()->GetNameStringByValue(OwningMoverCharacter->GetLocalRole()),
			GroundSpeed, World ? World->GetTimeSeconds() : 0.0, *Changes);
	}
}

void ULNPAnimInstance::UpdateMotionMatchingNode(const FAnimNodeReference& Node)
{
	EAnimNodeReferenceConversionResult ConversionResult;
	const FMotionMatchingAnimNodeReference MotionMatchingNode = UMotionMatchingAnimNodeLibrary::ConvertToMotionMatchingNode(Node, ConversionResult);
	if (ConversionResult != EAnimNodeReferenceConversionResult::Succeeded)
		return;

	// 직전 선택의 DB 태그(GASP Update_MotionMatching_PostSelection → CurrentDatabaseTags). 제자리 회전 Steering이 읽는다
	FPoseSearchBlueprintResult SearchResult;
	bool bSearchResultValid = false;
	UMotionMatchingAnimNodeLibrary::GetMotionMatchingSearchResult(MotionMatchingNode, SearchResult, bSearchResultValid);
	bTurnInPlaceDatabaseActive = bSearchResultValid && SearchResult.SelectedDatabase
		&& SearchResult.SelectedDatabase->Tags.Contains(TurnInPlaceDatabaseTag);

	// 디버그(2-4): MM이 고른 DB가 바뀔 때만 한 줄 — 전환 DB(Starts·Stops·Pivots…)가 실제로 뽑히는지 본다
	const UPoseSearchDatabase* SelectedDatabase = bSearchResultValid ? SearchResult.SelectedDatabase.Get() : nullptr;
	if (SelectedDatabase != LastSelectedDatabase.Get())
	{
		if (CVarDebugLocomotion.GetValueOnAnyThread() >= 2)
		{
			UE_LOG(LogLootNPop, Log, TEXT("[AnimB6] %s selected %s / %s t=%.2f cost %.3f (%s %s %s %s speed %.0f)"),
				*GetNameSafe(OwningMoverCharacter), *GetNameSafe(SelectedDatabase), *GetNameSafe(SearchResult.SelectedAnim),
				SearchResult.SelectedTime, SearchResult.SearchCost,
				*EnumName(MovementMode), *EnumName(MovementState), *EnumName(Gait), *EnumName(MovementDirection), GroundSpeed);
		}
		LastSelectedDatabase = SelectedDatabase;
	}

	TArray<UPoseSearchDatabase*> Databases;
	if (LocomotionDatabaseChooser)
	{
		for (UObject* Result : UChooserFunctionLibrary::EvaluateChooserMulti(this, LocomotionDatabaseChooser, UPoseSearchDatabase::StaticClass()))
		{
			if (UPoseSearchDatabase* Database = Cast<UPoseSearchDatabase>(Result))
				Databases.Add(Database);
		}
	}

	const EPoseSearchInterruptMode InterruptMode = GetMMInterruptMode();
	UMotionMatchingAnimNodeLibrary::SetDatabasesToSearch(MotionMatchingNode, Databases, InterruptMode);

	// GASP는 노드의 Blend Time 핀을 Get_MMBlendTime에 바인딩한다. 여기서는 노드 블렌드 설정의 시간만 덮어쓴다(프로필·곡선은 노드 값 유지).
	// FMotionMatchingBlueprintBlendSettings 생성자는 PoseSearch 모듈 밖으로 내보내지지 않아 리플렉션으로 인스턴스를 만든다.
	static const UScriptStruct* BlendSettingsStruct = FindObject<UScriptStruct>(nullptr, TEXT("/Script/PoseSearch.MotionMatchingBlueprintBlendSettings"));
	if (BlendSettingsStruct)
	{
		FInstancedStruct BlendSettingsMemory;
		BlendSettingsMemory.InitializeAs(BlendSettingsStruct);
		FMotionMatchingBlueprintBlendSettings& BlendSettings = *reinterpret_cast<FMotionMatchingBlueprintBlendSettings*>(BlendSettingsMemory.GetMutableMemory());
		bool bBlendSettingsValid = false;
		UMotionMatchingAnimNodeLibrary::GetMotionMatchingBlendSettings(MotionMatchingNode, BlendSettings, bBlendSettingsValid);
		if (bBlendSettingsValid)
		{
			BlendSettings.BlendTime = GetMMBlendTime();
			UMotionMatchingAnimNodeLibrary::OverrideMotionMatchingBlendSettings(MotionMatchingNode, BlendSettings);
		}
	}

	// 디버그: 고른 DB 목록이 바뀔 때만 한 줄(워커 스레드라 GetValueOnAnyThread)
	const TArray<const UPoseSearchDatabase*> SearchedDatabases(Databases);
	if (SearchedDatabases != LastSearchedDatabases)
	{
		if (CVarDebugLocomotion.GetValueOnAnyThread() >= 2)
		{
			TArray<FString> Names;
			for (const UPoseSearchDatabase* Database : SearchedDatabases)
				Names.Add(GetNameSafe(Database));
			UE_LOG(LogLootNPop, Log, TEXT("[AnimB1] %s DBs [%s] interrupt %s blend %.2f (%s %s %s %s)"),
				*GetNameSafe(OwningMoverCharacter), *FString::Join(Names, TEXT(", ")),
				*StaticEnum<EPoseSearchInterruptMode>()->GetNameStringByValue(static_cast<int64>(InterruptMode)), GetMMBlendTime(),
				*EnumName(MovementMode), *EnumName(MovementState), *EnumName(Gait), *EnumName(MovementDirection));
		}
		LastSearchedDatabases = SearchedDatabases;
	}
}

EPoseSearchInterruptMode ULNPAnimInstance::GetMMInterruptMode() const
{
	const bool bMovementModeChanged = MovementMode != MovementMode_LastFrame;
	const bool bDirectionChangedWhileMoving = MovementDirection != MovementDirection_LastFrame && MovementState == ELNPMovementState::Moving;
	// LootNPop 추가 두 가지. 진행 포즈는 자기 DB 스키마로 평가되는데 Idle 스키마는 궤적 가중치가 0이라,
	// 궤적이 바뀌어도 Idle 포즈 비용이 오르지 않아 Starts·TurnInPlace 후보를 계속 이긴다.
	// ① 출발 프레임(2-2). 2-4에서 Starts DB가 생긴 뒤에도 빼면 출발 때 Idle 포즈로 미끄러졌다.
	// ② 정지 중 제자리 회전 요청인데 아직 회전 DB가 아니다(2-4). 끊지 않으면 회전 클립이 안 뽑히고 가상 루트만 벌어졌다가 뚝 돈다.
	// ③ 정지 프레임(2-5). 아주 느리게 걷다 서면 궤적 차이가 작아 진행 중인 걷기·출발 포즈가 Idle·Stops 후보를 계속 이겨 제자리 걸음이 남았다.
	const bool bStartedMoving = MovementState == ELNPMovementState::Moving && MovementState_LastFrame == ELNPMovementState::Idle;
	const bool bStoppedMoving = MovementState == ELNPMovementState::Idle && MovementState_LastFrame == ELNPMovementState::Moving;
	const bool bTurnInPlaceRequested = MovementState == ELNPMovementState::Idle && bShouldTurnInPlace && !bTurnInPlaceDatabaseActive;
	return (bMovementModeChanged || bDirectionChangedWhileMoving || bStartedMoving || bStoppedMoving || bTurnInPlaceRequested)
		? EPoseSearchInterruptMode::InterruptOnDatabaseChange
		: EPoseSearchInterruptMode::DoNotInterrupt;
}

float ULNPAnimInstance::GetMMBlendTime() const
{
	if (MovementMode == ELNPMovementMode::OnGround)
		return MovementMode_LastFrame == ELNPMovementMode::OnGround ? MMBlendTimeOnGround : MMBlendTimeLanding;
	return FVector::DotProduct(Velocity, LocomotionUp) > MMRisingUpSpeed ? MMBlendTimeInAirRising : MMBlendTimeInAir;
}

bool ULNPAnimInstance::ShouldEnableSteering(const FAnimNodeReference& Node) const
{
	// GASP는 SM 경로의 루프 플래그도 OR하지만 MM 경로에서는 늘 거짓이다
	return (UBlendStackAnimNodeLibrary::GetCurrentBlendStackAnimIsActive(Node) && bIsMoving)
		|| MovementMode == ELNPMovementMode::InAir;
}

FQuat ULNPAnimInstance::GetSteeringTargetFacing(const FAnimNodeReference& Node) const
{
	const FTransform& ComponentTransform = GetProxyOnAnyThread<FAnimInstanceProxy>().GetComponentTransform();
	if (Trajectory.Samples.IsEmpty())
		return ComponentTransform.GetRotation();

	const float TargetTime = FMath::GetMappedRangeValueClamped(FVector2f(0.0f, 1.0f), FVector2f(SteeringTargetTimeMin, SteeringTargetTimeMax),
		GetBlendStackCurveValue(Node, SteeringTargetTimeCurve));
	// 궤적 facing은 Mover 예측(표시 컴포넌트 회전)이다. Steering은 가상 루트와의 상대 회전 전체를 루트모션에 더하므로
	// 예측 facing의 Up이 지금 컴포넌트 Up과 다르면(구면에서 앞쪽 샘플) 기울기까지 보정하려 든다 → Up을 맞춰 Yaw만 남긴다.
	return AlignAxisZToUp(Trajectory.GetSampleAtTime(TargetTime).Facing, ComponentTransform.GetUnitAxis(EAxis::Z));
}

float ULNPAnimInstance::GetSteeringProceduralTargetTime(const FAnimNodeReference& Node) const
{
	return FMath::GetMappedRangeValueClamped(FVector2f(0.0f, 1.0f), FVector2f(SteeringProceduralTimeMin, SteeringProceduralTimeMax),
		GetBlendStackCurveValue(Node, SteeringTargetTimeCurve));
}

bool ULNPAnimInstance::IsTurnInPlaceDatabaseActive() const
{
	return bTurnInPlaceDatabaseActive;
}

float ULNPAnimInstance::GetOrientationWarpingAlpha(const FAnimNodeReference& Node) const
{
	return GetBlendStackCurveValue(Node, EnableWarpingCurve);
}

bool ULNPAnimInstance::IsFullBodyMontageActive() const
{
	for (const FName& Slot : FullBodyMontageSlots)
	{
		if (IsSlotActive(Slot))
			return true;
	}
	return false;
}

EOffsetRootBoneMode ULNPAnimInstance::GetOffsetRootTranslationMode() const
{
	// GASP Get_OffsetRootTranslationMode. 몽타주 루트모션(근접 보정 워핑 포함)은 캡슐이 따라가므로 메시를 캡슐로 되돌린다
	if (IsFullBodyMontageActive() || MovementMode == ELNPMovementMode::InAir)
		return EOffsetRootBoneMode::Release;
	return bIsMoving ? EOffsetRootBoneMode::Interpolate : EOffsetRootBoneMode::Release;
}

EOffsetRootBoneMode ULNPAnimInstance::GetOffsetRootRotationMode() const
{
	// GASP Get_OffsetRootRotationMode
	return IsFullBodyMontageActive() ? EOffsetRootBoneMode::Release : EOffsetRootBoneMode::Accumulate;
}

float ULNPAnimInstance::GetOffsetRootTranslationHalfLife() const
{
	return MovementState == ELNPMovementState::Moving ? OffsetRootHalfLifeMoving : OffsetRootHalfLifeIdle;
}

FVector ULNPAnimInstance::GetOffsetRootGroundNormal() const
{
	return GetProxyOnAnyThread<FAnimInstanceProxy>().GetComponentTransform().GetUnitAxis(EAxis::Z);
}

bool ULNPAnimInstance::ShouldUseFootPlacementRig() const
{
	// GASP: OnGround → CR, InAir → LegIK(ik_foot 목표). LootNPop 공중 포즈(Fall_Loop, Lyra 리타깃)는 ik_foot을 믿을 수 없어 IK 없이 통과한다
	return MovementMode == ELNPMovementMode::OnGround && CVarFootPlacement.GetValueOnAnyThread() > 0;
}

bool ULNPAnimInstance::IsOnStaticGround() const
{
	return MovementMode == ELNPMovementMode::OnGround && (!bOnMovingBase || CVarFootPlacementOnMovingBase.GetValueOnAnyThread() > 0);
}

bool ULNPAnimInstance::ShouldResetFootPlacement() const
{
	return MovementMode == ELNPMovementMode::OnGround && MovementMode_LastFrame != ELNPMovementMode::OnGround;
}

bool ULNPAnimInstance::IsFootPlacementDebugEnabled() const
{
	return CVarDebugFootPlacement.GetValueOnAnyThread() > 0;
}

bool ULNPAnimInstance::ShouldApplyAdditiveLeans() const
{
	return MovementState == ELNPMovementState::Moving && CVarAdditiveLeans.GetValueOnAnyThread() > 0;
}

bool ULNPAnimInstance::ShouldApplyHeadLean() const
{
	// bShouldStrafe = FreeAim 무기(이동 방향으로 돌지 않는다). 락온은 무기와 무관하다
	return !bShouldStrafe && !bIsLockOn;
}

FQuat ULNPAnimInstance::GetOffsetRootRotationDelta() const
{
	// 서브 ABP(링크 레이어)도 자기 AimYaw를 계산하므로 메인 인스턴스의 가상 루트를 읽는다
	const ULNPAnimInstance* Main = OwningMoverCharacter ? Cast<ULNPAnimInstance>(OwningMoverCharacter->GetAnimInstance()) : nullptr;
	if (Main == nullptr || !Main->bHasOffsetRoot)
		return FQuat::Identity;
	return Main->OffsetRootTransform.GetRotation() * Main->GetSkelMeshComponent()->GetComponentQuat().Inverse();
}

void ULNPAnimInstance::DrawLocomotionDebug() const
{
#if ENABLE_DRAW_DEBUG
	if (CVarDebugLocomotion.GetValueOnGameThread() <= 0)
		return;

	const FVector Up       = OwningMoverCharacter->GetActorUpVector();
	const FVector Location = OwningMoverCharacter->GetActorLocation();
	const UWorld* World    = GetWorld();

	FString Flags;
	if (bIsStarting)           Flags += TEXT(" Start");
	if (bIsPivoting)           Flags += TEXT(" Pivot");
	if (bShouldTurnInPlace)    Flags += TEXT(" TIP");
	if (bShouldSpinTransition) Flags += TEXT(" Spin");
	if (bTrj_IsCircling)       Flags += TEXT(" Circle");
	if (bJustLanded_Light)     Flags += TEXT(" LandL");
	if (bJustLanded_Heavy)     Flags += TEXT(" LandH");
	if (bJustTeleported)       Flags += TEXT(" Tele");
	if (bIsLockOn)             Flags += TEXT(" LockOn");

	// 2-3 구면 확인값: 가상 루트(B3)가 메시 컴포넌트에서 벌어진 거리·Yaw·기울기, 궤적 예측 facing(Steering 목표)의 Up이 캐릭터 Up에서 기운 최대각.
	// 거리는 직전 평가의 가상 루트와 이번 프레임 메시를 비교하므로 한 프레임 이동량(속도 × dt)이 더해진다 — 실제 벌어짐은 노드가 MaxTranslationError로 자른다.
	const FTransform MeshTransform = GetSkelMeshComponent()->GetComponentTransform();
	const float RootDistance = bHasOffsetRoot ? FVector::Dist(OffsetRootTransform.GetLocation(), MeshTransform.GetLocation()) : 0.0f;
	const float RootYaw      = bHasOffsetRoot ? FacingDeltaAroundUp(MeshTransform.GetRotation(), OffsetRootTransform.GetRotation(), Up) : 0.0f;
	const float RootTilt     = bHasOffsetRoot ? FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(OffsetRootTransform.GetUnitAxis(EAxis::Z), Up), -1.0f, 1.0f))) : 0.0f;
	float TrajectoryFacingTilt = 0.0f;
	if (!Trajectory.Samples.IsEmpty())
	{
		for (const float Time : { SteeringTargetTimeMin, 0.5f, 1.0f, SteeringTargetTimeMax })
		{
			const float Cos = FMath::Clamp(FVector::DotProduct(Trajectory.GetSampleAtTime(Time).Facing.GetAxisZ(), Up), -1.0f, 1.0f);
			TrajectoryFacingTilt = FMath::Max(TrajectoryFacingTilt, FMath::RadiansToDegrees(FMath::Acos(Cos)));
		}
	}
	const EOffsetRootBoneMode RootTranslationMode = GetOffsetRootTranslationMode();
	const EOffsetRootBoneMode RootRotationMode    = GetOffsetRootRotationMode();

	const FString Text = FString::Printf(
		TEXT("%s %s %s %s %s (recent %s)\n")
		TEXT("spd %.0f acc %.2f velAcc %.0f lean %.2f,%.2f %s%s\n")
		TEXT("turn %.0f facing %.0f yawRate %.0f/%.0f\n")
		TEXT("slope %.1f,%.1f based %.1f%s\n")
		TEXT("root %s/%s %.1fcm yaw %.1f tilt %.2f trjTilt %.2f"),
		*EnumName(MovementMode), *EnumName(MovementState), *EnumName(Gait), *EnumName(RotationMode), *EnumName(MovementDirection), *EnumName(MovementDirection_Recent),
		GroundSpeed, AccelerationAmount, VelocityAcceleration.Size(), LeanAmount.X, LeanAmount.Y,
		ShouldApplyAdditiveLeans() ? TEXT("on") : TEXT("off"), ShouldApplyHeadLean() ? TEXT("+head") : TEXT(""),
		Trj_TurnAngle, FutureFacingDelta, Trj_PastYawRate, Trj_CurrentYawRate,
		SlopeAngle_X, SlopeAngle_Y, BasedMovementDelta.GetTranslation().Size(), *Flags,
		*EnumName(RootTranslationMode), *EnumName(RootRotationMode), RootDistance, RootYaw, RootTilt, TrajectoryFacingTilt);
	DrawDebugString(World, Location + Up * 130.0f, Text, nullptr, FColor::White, 0.0f, true, 1.0f);

	// 2: 1초마다 한 줄(호스트·게스트 로그 대조용)
	const double Now = World ? World->GetTimeSeconds() : 0.0;
	if (CVarDebugLocomotion.GetValueOnGameThread() >= 2 && Now - LastOffsetRootLogTime >= 1.0)
	{
		LastOffsetRootLogTime = Now;
		UE_LOG(LogLootNPop, Log, TEXT("[AnimB3] %s %s/%s %s speed %.0f root %s/%s dist %.1f yaw %.1f tilt %.2f trjTilt %.2f aimYaw %.1f lean %.2f,%.2f %s%s"),
			*GetNameSafe(OwningMoverCharacter),
			World && World->GetNetMode() == NM_Client ? TEXT("Client") : TEXT("Server"),
			*StaticEnum<ENetRole>()->GetNameStringByValue(OwningMoverCharacter->GetLocalRole()),
			*EnumName(MovementState), GroundSpeed, *EnumName(RootTranslationMode), *EnumName(RootRotationMode),
			RootDistance, RootYaw, RootTilt, TrajectoryFacingTilt, AimYaw, LeanAmount.X, LeanAmount.Y,
			ShouldApplyAdditiveLeans() ? TEXT("on") : TEXT("off"), ShouldApplyHeadLean() ? TEXT("+head") : TEXT(""));
	}

	// 초록 = 접평면 속도, 노랑 = 0.4~0.5s 예측 속도, 하늘 = 평활 지면 법선
	DrawDebugLine(World, Location, Location + FVector::VectorPlaneProject(Velocity, Up) * 0.2f, FColor::Green, false, 0.0f, 0, 2.0f);
	DrawDebugLine(World, Location, Location + Trj_FutureVelocity * 0.2f, FColor::Yellow, false, 0.0f, 0, 2.0f);
	DrawDebugLine(World, GroundLocation, GroundLocation + SmoothedGroundNormal * 60.0f, FColor::Cyan, false, 0.0f, 0, 2.0f);
#endif
}

float ULNPAnimInstance::ComputeLeftHandOffDemand() const
{
	// 몽타주·커브는 포즈 원본(AnimSourceMesh) 인스턴스에서 읽는다. 표시 메시 ABP도 이 클래스를 부모로 쓰기 때문이다.
	const UAnimInstance* SourceAnim = OwningMoverCharacter ? OwningMoverCharacter->GetAnimInstance() : nullptr;
	if (SourceAnim == nullptr)
		return 0.0f;

	static const FName DisableLHandIKCurve(TEXT("DisableLHandIK"));
	static const FName HandOffSlots[] = { TEXT("DefaultSlot"), TEXT("FullBody"), TEXT("UpperBody") };

	float Demand = SourceAnim->GetCurveValue(DisableLHandIKCurve);
	for (const FName& Slot : HandOffSlots)
		Demand = FMath::Max(Demand, SourceAnim->GetSlotMontageGlobalWeight(Slot));
	return FMath::Clamp(Demand, 0.0f, 1.0f);
}
