// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Animation/LNPAnimInstance.h"
#include "Character/LNPCharacterBase.h"
#include "Character/LNPPlayerCharacter.h"
#include "Item/LNPWeaponVisualSet.h"
#include "Movement/LNPCharacterMoverComponent.h"
#include "LootNPop.h"

#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "DefaultMovementSet/Settings/CommonLegacyMovementSettings.h"
#include "MoverPoseSearchTrajectoryPredictor.h"
#include "PoseSearch/PoseSearchTrajectoryLibrary.h"
#include "KismetAnimationLibrary.h"
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

	// 이동 방향 히스테리시스(GASP Get_MovementDirectionThresholds, DDCvar.StrafeStyle 0). 현재 방향이 F·B면 ±60/±120, 좌·우면 ±40/±140
	constexpr float DirFrontLimitFromFrontBack    = 60.0f;
	constexpr float DirBackLimitFromFrontBack     = 120.0f;
	constexpr float DirFrontLimitFromSide         = 40.0f;
	constexpr float DirBackLimitFromSide          = 140.0f;

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

	// 4. 이동 방향 계산
	Direction = UKismetAnimationLibrary::CalculateDirection(Velocity, OwningMoverCharacter->GetActorRotation());

	// 5. 캐릭터가 스트레이핑해야 하는지 판단
	// 이동 방향으로 회전하지 않으면 스트레이핑 중으로 간주 (예: 타겟을 보며 이동).
	bShouldStrafe = !OwningMoverCharacter->GetFaceMoveDirection();

	// 6. Aim Offset용 Yaw/Pitch 계산
	// GetBaseAimRotation()은 플레이어의 경우 Control Rotation, AI는 별도 로직을 따름.
	// 구형 세계 보정: 월드 공간 Euler 각도 뺄셈은 캐릭터 Up이 World Up과 벌어질 때 오차가 생긴다.
	// 에임 방향 벡터를 캐릭터 로컬 좌표계로 변환한 뒤 Pitch/Yaw를 추출해 항상 올바른 로컬 에임 각도를 얻는다.
	FRotator BaseAimRotation = OwningMoverCharacter->GetBaseAimRotation();
	FVector LocalAimDir = OwningMoverCharacter->GetActorTransform().InverseTransformVectorNoScale(BaseAimRotation.Vector());
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

	// 두 프레임 속력 중 큰 쪽으로 갈 수 있는 거리 + 50cm를 넘게 움직였을 때만. 고속 낙하 착지 프레임(직전 −3000cm/s → 0)을 오탐하지 않는다
	const float ReachableDistance = TeleportThreshold + FMath::Max(Velocity.Size(), Velocity_LastFrame.Size()) * DeltaSeconds;
	bJustTeleported = bHasLocomotionHistory
		&& FVector::DistSquared(Location, Location_LastFrame) > FMath::Square(ReachableDistance);

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
	SmoothedGroundNormal = bHasLocomotionHistory
		? FMath::VInterpTo(SmoothedGroundNormal, GroundNormal, DeltaSeconds, GroundNormalInterpSpeed).GetSafeNormal(UE_SMALL_NUMBER, Up)
		: GroundNormal;
	UKismetMathLibrary::GetSlopeDegreeAngles(ActorTransform.GetUnitAxis(EAxis::Y), SmoothedGroundNormal, Up, SlopeAngle_Y, SlopeAngle_X);

	BasedMovementDelta = PendingBasedMovementDelta;
	PendingBasedMovementDelta = FTransform::Identity;

	UpdateTrajectoryValues(DeltaSeconds);
	UpdateStates(DeltaSeconds);

	// --- Additive Lean 입력 (GASP Update_AdditiveLean, 레이어는 2-6) ---
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
	// GASP 첫 기준은 Offset Root Bone 루트다. 그 노드는 2-3(B3)에서 들어오므로 지금은 메시 facing이 루트와 같다.
	static constexpr float FacingTimes[] = { 0.0f, 0.25f, 0.75f, 1.5f };
	FQuat PreviousFacing = GetSkelMeshComponent()->GetComponentQuat();
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

	const FString Text = FString::Printf(
		TEXT("%s %s %s %s %s (recent %s)\n")
		TEXT("spd %.0f acc %.2f velAcc %.0f lean %.2f,%.2f\n")
		TEXT("turn %.0f facing %.0f yawRate %.0f/%.0f\n")
		TEXT("slope %.1f,%.1f based %.1f%s"),
		*EnumName(MovementMode), *EnumName(MovementState), *EnumName(Gait), *EnumName(RotationMode), *EnumName(MovementDirection), *EnumName(MovementDirection_Recent),
		GroundSpeed, AccelerationAmount, VelocityAcceleration.Size(), LeanAmount.X, LeanAmount.Y,
		Trj_TurnAngle, FutureFacingDelta, Trj_PastYawRate, Trj_CurrentYawRate,
		SlopeAngle_X, SlopeAngle_Y, BasedMovementDelta.GetTranslation().Size(), *Flags);
	DrawDebugString(World, Location + Up * 130.0f, Text, nullptr, FColor::White, 0.0f, true, 1.0f);

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
