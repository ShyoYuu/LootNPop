#include "Camera/LNPLockOnComponent.h"
#include "Camera/LNPControlRotationComponent.h"
#include "Gravity/LNPPawnGravityComponent.h"

#include "Engine/EngineTypes.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

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

void ULNPLockOnComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (ULNPTargetQuerySubsystem* QuerySub = World->GetSubsystem<ULNPTargetQuerySubsystem>())
			QuerySub->UnregisterQuery(QueryHandle);
	}

	Super::EndPlay(EndPlayReason);
}

void ULNPLockOnComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdateQuery();

	if (!LockOnEntity.IsSet())
		return;

	// Track 질의의 결과가 사라지면 사망·소멸·거리 이탈 중 하나다 — 셋을 따로 확인할 필요가 없다.
	UWorld* World = GetWorld();
	ULNPTargetQuerySubsystem* QuerySub = World ? World->GetSubsystem<ULNPTargetQuerySubsystem>() : nullptr;
	FLNPTargetQueryResult Result;
	if (!QuerySub || !QuerySub->GetResult(QueryHandle, Result) || !Result.bHit)
	{
		ClearTarget();
		return;
	}
	LockOnTargetLocation = Result.Location;

	ApplySoftRotation(DeltaTime);

	// 하드 클램프: 소프트 보정과 무관하게 타겟이 항상 MaxDeviationDeg 이내에 있도록 강제
	if (ControlRotationComponent.IsValid())
	{
		if (const APawn* OwnerPawn = Cast<APawn>(GetOwner()))
		{
			// 구 내벽이라 대상의 Up은 위치에서 곧바로 나온다 — Actor가 없어도 성립한다.
			const FVector TargetUp  = (-LockOnTargetLocation).GetSafeNormal();
			const FVector TargetPos = LockOnTargetLocation + TargetUp * TargetAimHeightOffset;
			const FVector ToTarget  = (TargetPos - OwnerPawn->GetActorLocation()).GetSafeNormal();
			if (!ToTarget.IsNearlyZero())
				ControlRotationComponent->SetLockOnClamp(ToTarget, MaxDeviationDeg);
		}
	}
}

void ULNPLockOnComponent::UpdateQuery()
{
	UWorld* World = GetWorld();
	ULNPTargetQuerySubsystem* QuerySub = World ? World->GetSubsystem<ULNPTargetQuerySubsystem>() : nullptr;
	if (!QuerySub)
		return;

	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	// 락온은 로컬 상태다 — 서버의 원격 폰에서 돌면 서버 시점으로 후보를 고르는 무의미한 질의가 된다.
	// 결과는 InputCmd(LockOnTargetLocation)로 서버에 전달된다 (TechDesign_TargetQuery.md §8).
	if (!OwnerPawn || !OwnerPawn->IsLocallyControlled())
		return;

	if (!QueryHandle.IsValid())
		QueryHandle = QuerySub->RegisterQuery();

	const APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController());
	if (!PC)
		return;

	const FVector SelfLoc = OwnerPawn->GetActorLocation();

	// 락온 중이면 후보 탐색이 필요 없다(토글은 해제만 한다). 같은 슬롯을 추적으로 돌려쓴다.
	if (LockOnEntity.IsSet())
	{
		QuerySub->SetTrackQuery(QueryHandle, SelfLoc, LockOnEntity, AutoBreakRange);
		return;
	}

	// 후보 탐색: 카메라 축 기준 원뿔. "화면 밖 제외"를 ProjectWorldLocationToScreen으로 두지 않는 이유는
	// 뷰포트가 게임 스레드 전용이라 워커에서 부를 수 없기 때문이다 — 시야 반각으로 대신한다.
	FVector  CamLoc;
	FRotator CamRot;
	PC->GetPlayerViewPoint(CamLoc, CamRot);

	const FVector UpDir = GravityComponent.IsValid() ? GravityComponent->GetUpDirection() : FVector::UpVector;

	FVector CamForwardTangent;
	float   ForwardLen = 0.f;
	if (!LNPTargetQuery::ProjectToTangent(UpDir, CamRot.Vector(), CamForwardTangent, ForwardLen))
	{
		QuerySub->SetConeQuery(QueryHandle, SelfLoc, FVector::ForwardVector, UpDir, 0.f, 0.f, 0.f, 0.f);
		return;
	}

	// 점수는 각도만 본다 — 옛 구현이 "화면 중앙에서 가장 가까운 각도"였고 그 감각을 유지한다.
	QuerySub->SetConeQuery(QueryHandle, CamLoc, CamForwardTangent, UpDir,
		MaxLockOnRange, LockOnSearchAngleDeg, 1.f, 0.f);
}

void ULNPLockOnComponent::ToggleLockOn()
{
	if (LockOnEntity.IsSet())
	{
		ClearTarget();
		return;
	}

	UWorld* World = GetWorld();
	ULNPTargetQuerySubsystem* QuerySub = World ? World->GetSubsystem<ULNPTargetQuerySubsystem>() : nullptr;
	if (!QuerySub)
		return;

	// 상시 질의라 누른 순간 답이 이미 준비돼 있다 — 여기서 탐색하지 않는다.
	FLNPTargetQueryResult Result;
	if (QuerySub->GetResult(QueryHandle, Result) && Result.bHit)
		SetTarget(Result.Entity, Result.Location);
}

bool ULNPLockOnComponent::GetLockOnTargetLocation(FVector& OutLocation) const
{
	if (!LockOnEntity.IsSet())
		return false;

	OutLocation = LockOnTargetLocation;
	return true;
}

void ULNPLockOnComponent::SetTarget(FMassEntityHandle NewTarget, const FVector& TargetLocation)
{
	LockOnEntity         = NewTarget;
	LockOnTargetLocation = TargetLocation;
}

void ULNPLockOnComponent::ClearTarget()
{
	LockOnEntity.Reset();
	LockOnTargetLocation = FVector::ZeroVector;
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

	const FVector TargetUp  = (-LockOnTargetLocation).GetSafeNormal();
	const FVector TargetPos = LockOnTargetLocation + TargetUp * TargetAimHeightOffset;

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
