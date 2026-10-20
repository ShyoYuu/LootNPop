// Copyright LootNPop. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MovementMode.h"
#include "LNPCharacterMovementSettings.generated.h"

/**
 * LootNPop 캐릭터용 커스텀 이동 설정.
 * Mover의 Shared Settings를 확장한다.
 */
UCLASS(BlueprintType)
class LOOTNPOP_API ULNPCharacterMovementSettings : public UObject, public IMovementSettingsInterface
{
	GENERATED_BODY()

public:
	virtual FString GetDisplayName() const override { return TEXT("LNP Character Movement Settings"); }

	/** Sprint 속도 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|SprintModifier", meta = (ClampMin = "0", UIMin = "0", ForceUnits = "cm/s"))
	float SprintSpeed = 1200.0f;

	/** Sprint 중 가속도 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|SprintModifier", meta = (ClampMin = "0", UIMin = "0", ForceUnits = "cm/s^2"))
	float SprintAcceleration = 6000.0f;

	/**
	 * 질주를 시작할 수 있는 최대 방향 각도. 이동 의도(MoveInput)와 바라볼 방향(OrientationIntent)의
	 * 접평면 사잇각이다. 기본값 50°는 키보드 대각(45°)을 허용하고 옆(90°)·뒤는 막는다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|SprintModifier", meta = (ClampMin = "0", ClampMax = "180", UIMin = "0", UIMax = "180", ForceUnits = "deg"))
	float SprintStartDirectionAngle = 50.0f;

	/**
	 * 질주 중 이 각도를 넘으면 질주가 풀린다. 시작 각도보다 크게 둬서(히스테리시스)
	 * 스틱 떨림·서버/클라 입력 양자화 차이로 경계에서 질주가 깜빡이지 않게 한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|SprintModifier", meta = (ClampMin = "0", ClampMax = "180", UIMin = "0", UIMax = "180", ForceUnits = "deg"))
	float SprintCancelDirectionAngle = 65.0f;

	/** 가드 중 이동 속도 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|GuardModifier", meta = (ClampMin = "0", UIMin = "0", ForceUnits = "cm/s"))
	float GuardWalkSpeed = 200.0f;

	/** 가드 중 가속도 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|GuardModifier", meta = (ClampMin = "0", UIMin = "0", ForceUnits = "cm/s^2"))
	float GuardAcceleration = 2000.0f;

	/** ADS(정조준) 중 이동 속도 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|ADSModifier", meta = (ClampMin = "0", UIMin = "0", ForceUnits = "cm/s"))
	float ADSWalkSpeed = 300.0f;

	/** ADS(정조준) 중 가속도 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|ADSModifier", meta = (ClampMin = "0", UIMin = "0", ForceUnits = "cm/s^2"))
	float ADSAcceleration = 2500.0f;
};
