// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LNPPawnGravityComponent.generated.h"

class UCharacterMoverComponent;

/** 중력 동작 모드 */
UENUM(BlueprintType)
enum class ELNPGravityType : uint8
{
	None,
	Fixed,          // 고정 중력 (특정 고정 방향을 가리킴)
	RadialInward,   // 동적 중력: 중심점으로 끌어당김 (행성형)
	RadialOutward   // 동적 중력: 중심점에서 밀어냄 (반전 구형 세계)
};

/**
 * ULNPPawnGravityComponent
 * 폰의 커스텀 중력(MoverComponent를 통한 물리)을 관리하며
 * Player의 곡률 인식 카메라/컨트롤 회전을 처리한다.
 */
UCLASS( ClassGroup=(LNP), meta=(BlueprintSpawnableComponent) )
class LOOTNPOP_API ULNPPawnGravityComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	ULNPPawnGravityComponent();

protected:
	virtual void BeginPlay() override;

public:	
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** 중력 모드와 방향/원점을 업데이트한다 */
	UFUNCTION(BlueprintCallable, Category = "LNP|Gravity")
	void SetGravity(const ELNPGravityType NewType, const FVector& NewDirectionOrOrigin);

	/** 현재 중력 타입과 Owner 위치에 기반하여 계산된 Up 방향을 반환한다 */
	UFUNCTION(BlueprintPure, Category = "LNP|Gravity")
	FVector GetUpDirection() const;

	/** 다음 tick에 처리될 시선 입력 델타 */
	void InputLook(const FRotator& LookDelta);

protected:
	/** 현재 활성화된 중력 모드 */
	UPROPERTY(EditAnywhere, Category = "LNP|Gravity")
	ELNPGravityType GravityType = ELNPGravityType::None;

	/** 중력 원점 (Radial 모드에서 사용) */
	UPROPERTY(EditAnywhere, Category = "LNP|Gravity")
	FVector GravityOrigin = FVector::ZeroVector;

	/** Fixed 모드의 중력 방향 (기본값은 World 다운) */
	UPROPERTY(EditAnywhere, Category = "LNP|Gravity")
	FVector FixedGravityDirection = FVector::DownVector;

	/** 중력 가속도 크기 */
	UPROPERTY(EditAnywhere, Category = "LNP|Gravity")
	float GravityStrength = 2000.0f;

private:
	/** Owner의 MoverComponent Cache 참조 */
	UPROPERTY(Transient)
	TObjectPtr<UCharacterMoverComponent> CachedMoverComponent;

	/** 캐릭터의 누적 시선 입력 */
	FRotator PendingLookInput = FRotator::ZeroRotator;

	/** 전환 감지를 위한 이전 모드 추적 */
	ELNPGravityType LastGravityType = ELNPGravityType::None;

	/** 카메라 곡률 보정을 위한 Up 방향 Cache */
	FVector LastUpDir = FVector::UpVector;

	/** 새 방향에 따라 물리 중력과 Capsule 방향을 업데이트한다 */
	void UpdatePawnOrientation(const FVector& PawnUpDir, const FVector& PawnDownDir);

	/** World 곡률과 시선 입력에 맞게 Player 컨트롤러의 회전 행렬을 조정한다 */
	void UpdateControllerOrientation(float DeltaTime, const FVector& TargetUpDir);
};
