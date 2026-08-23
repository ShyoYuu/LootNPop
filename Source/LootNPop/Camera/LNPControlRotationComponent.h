// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LNPControlRotationComponent.generated.h"

class ULNPPawnGravityComponent;

/**
 * ULNPControlRotationComponent
 * PlayerController의 SetControlRotation을 전담한다.
 * - GravityComponent에서 Up 방향을 읽어 곡률 보정 적용
 * - InputHandlerComponent, LockOnComponent가 적립한 look/보정 델타를 소비
 * - 두 컴포넌트보다 늦게 Tick하여 프레임당 SetControlRotation 한 번만 호출
 */
UCLASS(ClassGroup=(LNP), meta=(BlueprintSpawnableComponent))
class LOOTNPOP_API ULNPControlRotationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	ULNPControlRotationComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** InputHandlerComponent가 Tick에서 호출 — 시선 입력 델타 적립 */
	void InputLook(const FRotator& LookDelta);

	/** LockOnComponent가 Tick에서 호출 — 소프트 보정 델타 적립 */
	void InputLockOnCorrectionDeg(float YawDeg, float PitchDeg);

	/** LockOnComponent가 Tick에서 호출 — 타겟 방향 하드 클램프 설정.
	 *  UpdateControllerOrientation 말미에서 ViewForward가 이 방향과 MaxDeviationDeg 이내로 강제된다. */
	void SetLockOnClamp(const FVector& ToTargetDir, float MaxDeviationDeg);

protected:
	/** 시선 입력 Yaw 배율. 기존 하드코딩 상수를 그대로 옮긴 값이다. */
	UPROPERTY(EditAnywhere, Category = "LNP|Look")
	double LookYawSensitivity = 8.0;

	/** 시선 입력 Pitch 배율. 기존 하드코딩 상수를 그대로 옮긴 값이다. */
	UPROPERTY(EditAnywhere, Category = "LNP|Look")
	double LookPitchSensitivity = 4.0;

	/**
	 * ADS(정조준) 중 시선 입력 배율.
	 *
	 * FOV를 좁히면 같은 마우스 이동이 화면상 더 크게 돌아 조준이 과민해진다.
	 * 기준값은 tan(ADS_FOV/2) / tan(허리사격_FOV/2) — 리그의 FOV를 바꾸면 이 값도 함께 본다.
	 */
	UPROPERTY(EditAnywhere, Category = "LNP|Look", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	double ADSLookSensitivityScale = 0.65;

private:
	UPROPERTY(Transient)
	TObjectPtr<ULNPPawnGravityComponent> GravityComponent;

	FRotator PendingLookInput = FRotator::ZeroRotator;
	float PendingLockOnYawDeg = 0.f;
	float PendingLockOnPitchDeg = 0.f;

	/** 락온 하드 클램프. LockOnComponent가 매 Tick 설정, UpdateControllerOrientation에서 소비 */
	FVector PendingLockOnClampDir = FVector::ZeroVector;
	float PendingLockOnMaxDeviationDeg = 0.f;

	FVector LastUpDir = FVector::UpVector;

	void UpdateControllerOrientation(const FVector& TargetUpDir);
};
