#include "Camera/LNPLockOnComponent.h"
#include "Camera/LNPControlRotationComponent.h"
#include "Enemy/LNPEnemyCharacter.h"
#include "Gravity/LNPPawnGravityComponent.h"
#include "GAS/Attributes/LNPBaseAttributeSet.h"

#include "AbilitySystemComponent.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/KismetSystemLibrary.h"

ULNPLockOnComponent::ULNPLockOnComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void ULNPLockOnComponent::BeginPlay()
{
	Super::BeginPlay();

	GravityComponent = GetOwner()->FindComponentByClass<ULNPPawnGravityComponent>();
	ControlRotationComponent = GetOwner()->FindComponentByClass<ULNPControlRotationComponent>();

	// LockOn이 먼저 Tick하여 보정 델타를 계산 → ControlRotationComponent가 합산하여 SetControlRotation 한 번만 호출
	AddTickPrerequisiteActor(GetOwner());
	if (ControlRotationComponent.IsValid())
	{
		ControlRotationComponent->AddTickPrerequisiteComponent(this);
	}
}

void ULNPLockOnComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!LockOnTarget.IsValid())
		return;

	if (!IsTargetStillValid())
	{
		ClearTarget();
		return;
	}

	ApplySoftRotation(DeltaTime);

	// 하드 클램프: 소프트 보정과 무관하게 타겟이 항상 MaxDeviationDeg 이내에 있도록 강제
	if (ControlRotationComponent.IsValid())
	{
		if (const APawn* OwnerPawn = Cast<APawn>(GetOwner()))
		{
			const FVector TargetPos = LockOnTarget->GetActorLocation()
				+ LockOnTarget->GetActorUpVector() * TargetAimHeightOffset;
			const FVector ToTarget = (TargetPos - OwnerPawn->GetActorLocation()).GetSafeNormal();
			if (!ToTarget.IsNearlyZero())
				ControlRotationComponent->SetLockOnClamp(ToTarget, MaxDeviationDeg);
		}
	}
}

void ULNPLockOnComponent::ToggleLockOn()
{
	if (LockOnTarget.IsValid())
	{
		ClearTarget();
	}
	else
	{
		if (ALNPEnemyCharacter* Best = FindBestTarget())
		{
			SetTarget(Best);
		}
	}
}

ALNPEnemyCharacter* ULNPLockOnComponent::FindBestTarget() const
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn)
		return nullptr;

	APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController());
	if (!PC)
		return nullptr;

	// Physics Broadphase로 MaxLockOnRange 이내의 Pawn 캡슐만 추려낸다.
	// TActorIterator 전체 순회 대비 O(k) — 근거리 적만 고려하면 충분하다.
	TArray<AActor*> OverlappedActors;
	UKismetSystemLibrary::SphereOverlapActors(
		GetWorld(),
		OwnerPawn->GetActorLocation(),
		MaxLockOnRange,
		TArray<TEnumAsByte<EObjectTypeQuery>>{ UEngineTypes::ConvertToObjectType(ECC_Pawn) },
		ALNPEnemyCharacter::StaticClass(),
		TArray<AActor*>{ GetOwner() },
		OverlappedActors
	);

	int32 ViewportSizeX = 0, ViewportSizeY = 0;
	PC->GetViewportSize(ViewportSizeX, ViewportSizeY);

	FVector CamLoc;
	FRotator CamRot;
	PC->GetPlayerViewPoint(CamLoc, CamRot);
	const FVector CamForward = CamRot.Vector();

	ALNPEnemyCharacter* BestTarget = nullptr;
	float BestAngleDeg = MAX_FLT;

	for (AActor* Actor : OverlappedActors)
	{
		ALNPEnemyCharacter* Enemy = Cast<ALNPEnemyCharacter>(Actor);
		if (!IsValid(Enemy))
			continue;

		// 카메라 뒤쪽 제외
		const FVector ToEnemy = (Enemy->GetActorLocation() - CamLoc).GetSafeNormal();
		if (FVector::DotProduct(CamForward, ToEnemy) <= 0.f)
			continue;

		// 화면 밖 제외
		FVector2D ScreenPos;
		if (!PC->ProjectWorldLocationToScreen(Enemy->GetActorLocation(), ScreenPos, true))
			continue;

		if (ScreenPos.X < 0.f || ScreenPos.X > ViewportSizeX || ScreenPos.Y < 0.f || ScreenPos.Y > ViewportSizeY)
			continue;

		const float CosAngle = FMath::Clamp(FVector::DotProduct(CamForward, ToEnemy), -1.f, 1.f);
		const float AngleDeg = FMath::RadiansToDegrees(FMath::Acos(CosAngle));

		if (AngleDeg < BestAngleDeg)
		{
			BestAngleDeg = AngleDeg;
			BestTarget = Enemy;
		}
	}

	return BestTarget;
}

void ULNPLockOnComponent::SetTarget(ALNPEnemyCharacter* NewTarget)
{
	ClearTarget();
	LockOnTarget = NewTarget;
	NewTarget->SetLockOnMarkerVisible(true);
}

void ULNPLockOnComponent::ClearTarget()
{
	if (LockOnTarget.IsValid())
	{
		LockOnTarget->SetLockOnMarkerVisible(false);
	}
	LockOnTarget.Reset();
}

bool ULNPLockOnComponent::IsTargetStillValid() const
{
	if (!LockOnTarget.IsValid())
		return false;

	// 거리 이탈 체크
	const float DistSq = FVector::DistSquared(GetOwner()->GetActorLocation(), LockOnTarget->GetActorLocation());
	if (DistSq > AutoBreakRange * AutoBreakRange)
		return false;

	// 사망 체크
	if (UAbilitySystemComponent* ASC = LockOnTarget->GetAbilitySystemComponent())
	{
		bool bFound = false;
		const float Health = ASC->GetGameplayAttributeValue(ULNPBaseAttributeSet::GetHealthAttribute(), bFound);
		if (bFound && Health <= 0.f)
			return false;
	}

	return true;
}

void ULNPLockOnComponent::ApplySoftRotation(float DeltaTime)
{
	if (!GravityComponent.IsValid())
		return;

	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn)
		return;

	APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController());
	if (!PC)
		return;

	const FVector TargetPos = LockOnTarget->GetActorLocation()
		+ LockOnTarget->GetActorUpVector() * TargetAimHeightOffset;

	const FVector CurrentForward = PC->GetControlRotation().Quaternion().GetForwardVector();
	const FVector UpDir = GravityComponent->GetUpDirection();
	const FVector ToTarget = (TargetPos - OwnerPawn->GetActorLocation()).GetSafeNormal();
	if (ToTarget.IsNearlyZero())
		return;

	int32 ViewportSizeX = 0, ViewportSizeY = 0;
	PC->GetViewportSize(ViewportSizeX, ViewportSizeY);
	const float HalfW = ViewportSizeX * 0.5f;
	const float HalfH = ViewportSizeY * 0.5f;

	// 데드존 초과 비율: 0=데드존 내, 1=화면 끝, >1=화면 밖
	// 보정 속도 = MaxCorrectionDegPerSec * Excess — 멀어질수록 강하게 당겨진다.
	float YawExcess   = 0.f;
	float PitchExcess = 0.f;

	FVector2D ScreenPos;
	const bool bInFront = FVector::DotProduct(CurrentForward, ToTarget) > 0.f;

	if (bInFront && PC->ProjectWorldLocationToScreen(TargetPos, ScreenPos, true))
	{
		// 앞쪽이면 화면 안팎 모두 NX/NY로 연속 처리 (화면 밖에서 NX>1로 자연스럽게 이어짐)
		const float NX = (ScreenPos.X - HalfW) / HalfW;
		const float NY = (ScreenPos.Y - HalfH) / HalfH;
		if (FMath::Abs(NX) > DeadzoneRatioX)
			YawExcess   = (FMath::Abs(NX) - DeadzoneRatioX) / FMath::Max(1.f - DeadzoneRatioX, KINDA_SMALL_NUMBER);
		if (FMath::Abs(NY) > DeadzoneRatioY)
			PitchExcess = (FMath::Abs(NY) - DeadzoneRatioY) / FMath::Max(1.f - DeadzoneRatioY, KINDA_SMALL_NUMBER);
	}
	else
	{
		// 완전히 등진 경우: 데드존 무시하고 강한 고정 보정
		YawExcess   = 3.f;
		PitchExcess = 3.f;
	}

	if (YawExcess <= 0.f && PitchExcess <= 0.f)
		return;

	float YawCorrectionDeg   = 0.f;
	float PitchCorrectionDeg = 0.f;

	// Yaw 보정량 계산 (부호 포함 각도)
	if (YawExcess > 0.f)
	{
		const FVector HorizFwd = FVector::VectorPlaneProject(CurrentForward, UpDir).GetSafeNormal();
		const FVector HorizTgt = FVector::VectorPlaneProject(ToTarget, UpDir).GetSafeNormal();
		if (!HorizFwd.IsNearlyZero() && !HorizTgt.IsNearlyZero())
		{
			const float HorizAngleDeg = FMath::RadiansToDegrees(
				FMath::Acos(FMath::Clamp(FVector::DotProduct(HorizFwd, HorizTgt), -1.f, 1.f)));
			if (HorizAngleDeg > KINDA_SMALL_NUMBER)
			{
				const float Magnitude = FMath::Min(MaxCorrectionDegPerSec * YawExcess * DeltaTime, HorizAngleDeg);
				const float Sign = FVector::DotProduct(FVector::CrossProduct(HorizFwd, HorizTgt), UpDir) >= 0.f ? 1.f : -1.f;
				YawCorrectionDeg = Sign * Magnitude;
			}
		}
	}

	// Pitch 보정량 계산 (양수=위로 보정 필요)
	if (PitchExcess > 0.f)
	{
		const float CPitch = FMath::RadiansToDegrees(
			FMath::Asin(FMath::Clamp(FVector::DotProduct(CurrentForward, UpDir), -1.f, 1.f)));
		const float TPitch = FMath::RadiansToDegrees(
			FMath::Asin(FMath::Clamp(FVector::DotProduct(ToTarget, UpDir), -1.f, 1.f)));
		const float PitchError = TPitch - CPitch;
		if (FMath::Abs(PitchError) > KINDA_SMALL_NUMBER)
		{
			const float MaxCorr = MaxCorrectionDegPerSec * PitchExcess * DeltaTime;
			PitchCorrectionDeg = FMath::Clamp(PitchError, -MaxCorr, MaxCorr);
		}
	}

	// ControlRotationComponent에 델타 적립 — SetControlRotation은 ControlRotationComponent Tick에서 한 번만 호출된다.
	if ((FMath::Abs(YawCorrectionDeg) > KINDA_SMALL_NUMBER || FMath::Abs(PitchCorrectionDeg) > KINDA_SMALL_NUMBER)
		&& ControlRotationComponent.IsValid())
	{
		ControlRotationComponent->InputLockOnCorrectionDeg(YawCorrectionDeg, PitchCorrectionDeg);
	}
}
