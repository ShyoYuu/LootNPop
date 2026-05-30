// Copyright LootNPop. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "NativeGameplayTags.h"
#include "LNPSprintModifier.h"
#include "LNPCharacterMoverComponent.generated.h"

class UAnimMontage;

LOOTNPOP_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(LNPTAG_Mover_IsSprinting);

/**
 * LootNPop 캐릭터용 커스텀 Mover Component.
 * Sprint, Dash 실행, 동적 이동 Modifier 업데이트를 처리한다.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class LOOTNPOP_API ULNPCharacterMoverComponent : public UCharacterMoverComponent
{
	GENERATED_BODY()

public:
	ULNPCharacterMoverComponent();

	/** 캐릭터가 달리기(Sprint)를 원하는지 여부를 설정한다 */
	UFUNCTION(BlueprintCallable, Category = "LNP|Movement")
	void SetWantsToRun(bool bInWantsToRun) { bWantsToRun = bInWantsToRun; }

	/** 캐릭터가 현재 Sprint 중이면 true를 반환한다 */
	UFUNCTION(BlueprintPure, Category = "LNP|Movement")
	bool IsSprinting() const;

	/** 캐릭터가 현재 Sprint 가능한지 확인한다 */
	UFUNCTION(BlueprintPure, Category = "LNP|Movement")
	bool CanSprint() const;

	void SetIsAiming(bool bInIsAiming) { bIsAiming = bInIsAiming; }
	bool GetIsAiming() const { return bIsAiming; }

	/** 현재 Dash 실행 가능 여부를 반환한다 (지면, 조준 아님, 쿨다운 경과) */
	bool CanDash() const;

	/** 주어진 이동 Input Intent로 Dash를 실행한다 */
	void ExecuteDash(FVector MoveInputIntent);

protected:
	/** simulation tick 직전 호출된다. 상태 기반 Modifier 변경을 적용하는 데 사용된다. */
	virtual void OnMoverPreSimulationTick(const FMoverTimeStep& TimeStep, const FMoverInputCmdContext& InputCmd) override;

	/** 커스텀 시뮬레이션 로직이 항상 등록되도록 Override한다. */
	virtual void OnHandlerSettingChanged() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|Dash")
	float DashDuration = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|Dash")
	float DashImpulseMagnitude = 2000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|Dash")
	float DashCooldown = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|Dash")
	TObjectPtr<UAnimMontage> DashForwardMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|Dash")
	TObjectPtr<UAnimMontage> DashBackwardMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|Dash")
	TObjectPtr<UAnimMontage> DashLeftMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|Dash")
	TObjectPtr<UAnimMontage> DashRightMontage;

private:
	/** 이 Component가 의도에 따라 Sprint 로직을 직접 처리할지 여부. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|Movement", meta = (AllowPrivateAccess = "true"))
	uint8 bHandleSprintChanges : 1 = 1;

	/** true이면 캐릭터가 다음 simulation tick에서 Sprint를 의도하고 있다 */
	UPROPERTY(BlueprintReadOnly, Category = "LNP|Movement", meta = (AllowPrivateAccess = "true"))
	uint8 bWantsToRun : 1 = 0;

	/** 활성 Sprint Modifier Handle */
	FMovementModifierHandle SprintModifierHandle;

	bool bIsAiming = false;
	float LastDashTime = -1.0f;
};
