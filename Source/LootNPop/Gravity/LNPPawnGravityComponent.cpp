// Copyright Epic Games, Inc. All Rights Reserved.

#include "Gravity/LNPPawnGravityComponent.h"
#include "GameMode/LNPGameState.h"

#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "DefaultMovementSet/CharacterMoverComponent.h"

ULNPPawnGravityComponent::ULNPPawnGravityComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void ULNPPawnGravityComponent::BeginPlay()
{
	Super::BeginPlay();

	// 1. Owner Actor에서 MoverComponent Cache
	if (APawn* OwnerPawn = Cast<APawn>(GetOwner()))
	{
		CachedMoverComponent = OwnerPawn->FindComponentByClass<UCharacterMoverComponent>();
	}

	// 2. GameState 설정과 초기 동기화
	if (ALNPGameState* GS = GetWorld()->GetGameState<ALNPGameState>())
	{
		if (GS->bIsSphereWorld)
			SetGravity(ELNPGravityType::RadialOutward, FVector::ZeroVector);
		else
			SetGravity(ELNPGravityType::Fixed, FVector::DownVector);
	}

	AddTickPrerequisiteActor(GetOwner());
	LastGravityType = GravityType;
	LastUpDir = FVector::UpVector;
}

void ULNPPawnGravityComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (OwnerPawn == nullptr)
		return;

	FVector PawnUpDir = FVector::UpVector;
	FVector PawnDownDir = FVector::DownVector;

	// 현재 모드에 따라 목표 표면 법선과 중력 방향 계산
	switch (GravityType)
	{
	case ELNPGravityType::Fixed:
		PawnDownDir = FixedGravityDirection.GetSafeNormal();
		PawnUpDir = -PawnDownDir;
		break;

	case ELNPGravityType::RadialInward:
		PawnDownDir = (GravityOrigin - OwnerPawn->GetActorLocation()).GetSafeNormal();
		PawnUpDir = -PawnDownDir;
		break;

	case ELNPGravityType::RadialOutward:
		PawnDownDir = (OwnerPawn->GetActorLocation() - GravityOrigin).GetSafeNormal();
		PawnUpDir = -PawnDownDir;
		break;

	default:
		return;
	}

	// 1. 모드 전환 감지
	if (LastGravityType != GravityType)
	{
		// 전환 시 초기 물리 업데이트 강제
		UpdatePawnOrientation(PawnUpDir, PawnDownDir);
		LastGravityType = GravityType;
	}

	// 2. 필요 시 방향 업데이트 (방향 변경 또는 동적 중력 세계)
	// 동적 모드에서는 Pawn이 이동할 때 부드러운 전환을 위해 항상 실행
	const bool bDirectionChanged = !LastUpDir.Equals(PawnUpDir, 0.001f);

	if (GravityType != ELNPGravityType::Fixed || bDirectionChanged)
	{
		UpdatePawnOrientation(PawnUpDir, PawnDownDir);
		UpdateControllerOrientation(DeltaTime, PawnUpDir);
	}

	LastUpDir = PawnUpDir;
}

void ULNPPawnGravityComponent::SetGravity(const ELNPGravityType NewType, const FVector& NewDirectionOrOrigin)
{
	GravityType = NewType;
	if (NewType == ELNPGravityType::Fixed)
	{
		FixedGravityDirection = NewDirectionOrOrigin.GetSafeNormal();
	}
	else
	{
		GravityOrigin = NewDirectionOrOrigin;
	}
}

FVector ULNPPawnGravityComponent::GetUpDirection() const
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return FVector::UpVector;
	}

	switch (GravityType)
	{
	case ELNPGravityType::Fixed:
		return -FixedGravityDirection.GetSafeNormal();

	case ELNPGravityType::RadialInward:
		return (Owner->GetActorLocation() - GravityOrigin).GetSafeNormal();

	case ELNPGravityType::RadialOutward:
		return (GravityOrigin - Owner->GetActorLocation()).GetSafeNormal();
	}

	return FVector::UpVector;
}

void ULNPPawnGravityComponent::InputLook(const FRotator& LookDelta)
{
	PendingLookInput += LookDelta;
}

void ULNPPawnGravityComponent::UpdatePawnOrientation(const FVector& PawnUpDir, const FVector& PawnDownDir)
{
	if (CachedMoverComponent == nullptr)
		return;

	const FVector CustomGravityVector = PawnDownDir * GravityStrength;

	CachedMoverComponent->SetGravityOverride(true, CustomGravityVector);
	CachedMoverComponent->SetUpDirectionOverride(true, PawnUpDir);

	// 여기서 SetActorRotation은 더 이상 호출하지 않는다.
	// Mover Component가 UpDirectionOverride를 통해 Capsule 방향을 부드럽게 처리한다.
}

void ULNPPawnGravityComponent::UpdateControllerOrientation(float DeltaTime, const FVector& TargetUpDir)
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (OwnerPawn == nullptr || OwnerPawn->GetController() == nullptr)
		return;

	APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController());
	if (PC == nullptr)
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
		// Yaw: 로컬 Up 축을 중심으로 회전
		const FQuat YawQuat(TargetUpDir, FMath::DegreesToRadians(PendingLookInput.Yaw * 4.0));
		CurrentControlQuat = YawQuat * CurrentControlQuat;

		// Pitch: 로컬 Right 축을 중심으로 회전
		const FVector CurrentRight = FVector::CrossProduct(TargetUpDir, CurrentControlQuat.GetForwardVector()).GetSafeNormal();
		if (!CurrentRight.IsNearlyZero())
		{
			const FQuat PitchQuat(CurrentRight, FMath::DegreesToRadians(-PendingLookInput.Pitch * 2.0));
			CurrentControlQuat = PitchQuat * CurrentControlQuat;
		}

		PendingLookInput = FRotator::ZeroRotator;
	}

	// 3. 로컬 Up 기준 안정화 및 Pitch 클램핑
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

	// 4. 최종 재구성: ViewForward를 유지하면서 중력 기준 안정적인 Right 축 강제
	// Pitch를 유지하면서 카메라가 표면에 대해 롤하지 않도록 보장한다.
	FVector FinalRight = FVector::CrossProduct(TargetUpDir, ViewForward).GetSafeNormal();
	FVector FinalUp = FVector::CrossProduct(ViewForward, FinalRight).GetSafeNormal();
	
	const FRotator FinalControlRotation = FMatrix(ViewForward, FinalRight, FinalUp, FVector::ZeroVector).Rotator();
	PC->SetControlRotation(FinalControlRotation);
}
