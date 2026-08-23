// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Camera/LNPControlRotationComponent.h"
#include "Gravity/LNPPawnGravityComponent.h"
#include "Character/LNPCharacterBase.h"

#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

ULNPControlRotationComponent::ULNPControlRotationComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void ULNPControlRotationComponent::BeginPlay()
{
	Super::BeginPlay();

	GravityComponent = GetOwner()->FindComponentByClass<ULNPPawnGravityComponent>();

	// Actor Tick 이후에 실행 (Tick 의존 컴포넌트들과 일관성 유지)
	AddTickPrerequisiteActor(GetOwner());
}

void ULNPControlRotationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn || !OwnerPawn->GetController() || !GravityComponent)
		return;

	const FVector TargetUpDir = GravityComponent->GetUpDirection();
	UpdateControllerOrientation(TargetUpDir);
	LastUpDir = TargetUpDir;
}

void ULNPControlRotationComponent::InputLook(const FRotator& LookDelta)
{
	PendingLookInput += LookDelta;
}

void ULNPControlRotationComponent::InputLockOnCorrectionDeg(float YawDeg, float PitchDeg)
{
	PendingLockOnYawDeg += YawDeg;
	PendingLockOnPitchDeg += PitchDeg;
}

void ULNPControlRotationComponent::SetLockOnClamp(const FVector& ToTargetDir, float MaxDeviationDeg)
{
	PendingLockOnClampDir = ToTargetDir;
	PendingLockOnMaxDeviationDeg = MaxDeviationDeg;
}

void ULNPControlRotationComponent::UpdateControllerOrientation(const FVector& TargetUpDir)
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController());
	if (!PC)
		return;

	FQuat CurrentControlQuat = PC->GetControlRotation().Quaternion();

	// 1. 곡률 보정
	if (!LastUpDir.Equals(TargetUpDir, 0.0001f))
	{
		const FQuat CurvatureDelta = FQuat::FindBetweenNormals(LastUpDir, TargetUpDir);
		CurrentControlQuat = CurvatureDelta * CurrentControlQuat;
	}

	// 2. 시선 입력 적용 (Yaw, Pitch)
	if (!PendingLookInput.IsNearlyZero())
	{
		// ADS 중에는 감도를 낮춘다. 락온 보정(3·5단계)에는 곱하지 않는다 —
		// 그건 시스템이 만든 델타지 플레이어 입력이 아니다.
		const ALNPCharacterBase* Character = Cast<ALNPCharacterBase>(OwnerPawn);
		const double LookScale = (Character && Character->IsADSActive()) ? ADSLookSensitivityScale : 1.0;

		const FQuat YawQuat(TargetUpDir, FMath::DegreesToRadians(PendingLookInput.Yaw * LookYawSensitivity * LookScale));
		CurrentControlQuat = YawQuat * CurrentControlQuat;

		const FVector CurrentRight = FVector::CrossProduct(TargetUpDir, CurrentControlQuat.GetForwardVector()).GetSafeNormal();
		if (!CurrentRight.IsNearlyZero())
		{
			const FQuat PitchQuat(CurrentRight, FMath::DegreesToRadians(-PendingLookInput.Pitch * LookPitchSensitivity * LookScale));
			CurrentControlQuat = PitchQuat * CurrentControlQuat;
		}

		PendingLookInput = FRotator::ZeroRotator;
	}

	// 3. 락온 보정 적용
	if (FMath::Abs(PendingLockOnYawDeg) > KINDA_SMALL_NUMBER)
	{
		const FQuat LockOnYawQuat(TargetUpDir, FMath::DegreesToRadians(PendingLockOnYawDeg));
		CurrentControlQuat = LockOnYawQuat * CurrentControlQuat;
		PendingLockOnYawDeg = 0.f;
	}

	if (FMath::Abs(PendingLockOnPitchDeg) > KINDA_SMALL_NUMBER)
	{
		const FVector LockOnRight = FVector::CrossProduct(TargetUpDir, CurrentControlQuat.GetForwardVector()).GetSafeNormal();
		if (!LockOnRight.IsNearlyZero())
		{
			const FQuat LockOnPitchQuat(LockOnRight, FMath::DegreesToRadians(-PendingLockOnPitchDeg));
			CurrentControlQuat = LockOnPitchQuat * CurrentControlQuat;
		}
		PendingLockOnPitchDeg = 0.f;
	}

	// 4. Pitch 클램핑
	FVector ViewForward = CurrentControlQuat.GetForwardVector();
	const float CosAngleFromUp = FVector::DotProduct(ViewForward, TargetUpDir);

	const float MaxCosLimit = 0.996f;  // 약 85도
	const float MinCosLimit = -0.996f;

	if (CosAngleFromUp > MaxCosLimit || CosAngleFromUp < MinCosLimit)
	{
		const float ClampedCos = FMath::Clamp(CosAngleFromUp, MinCosLimit, MaxCosLimit);
		const float ClampedSin = FMath::Sqrt(1.0f - (ClampedCos * ClampedCos));
		const FVector HorizonForward = FVector::VectorPlaneProject(ViewForward, TargetUpDir).GetSafeNormal();
		ViewForward = (HorizonForward * ClampedSin) + (TargetUpDir * ClampedCos);
	}

	// 5. 락온 하드 클램프: 모든 입력 후에도 타겟과의 각도가 상한을 초과하면 강제 보정
	if (!PendingLockOnClampDir.IsNearlyZero())
	{
		const float AngleDeg = FMath::RadiansToDegrees(
			FMath::Acos(FMath::Clamp(FVector::DotProduct(ViewForward, PendingLockOnClampDir), -1.f, 1.f)));

		if (AngleDeg > PendingLockOnMaxDeviationDeg)
		{
			// ViewForward를 타겟 방향으로 초과분만큼 회전시켜 정확히 MaxDeviationDeg로 맞춘다
			const FQuat TowardTarget = FQuat::FindBetweenNormals(ViewForward, PendingLockOnClampDir);
			const float Alpha = (AngleDeg - PendingLockOnMaxDeviationDeg) / AngleDeg;
			ViewForward = FQuat::Slerp(FQuat::Identity, TowardTarget, Alpha).RotateVector(ViewForward).GetSafeNormal();
		}

		PendingLockOnClampDir = FVector::ZeroVector;
		PendingLockOnMaxDeviationDeg = 0.f;
	}

	// 6. Roll-free ControlRotation 설정
	// 카메라 Roll 보정은 LNPGravityRollCorrectionCameraNode가 담당한다.
	FVector FinalRight = FVector::CrossProduct(TargetUpDir, ViewForward).GetSafeNormal();
	FVector FinalUp = FVector::CrossProduct(ViewForward, FinalRight).GetSafeNormal();
	PC->SetControlRotation(FMatrix(ViewForward, FinalRight, FinalUp, FVector::ZeroVector).Rotator());
}
