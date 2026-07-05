// Copyright LootNPop. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "NativeGameplayTags.h"
#include "LNPSprintModifier.h"
#include "LNPGuardModifier.h"
#include "LNPCharacterMoverComponent.generated.h"

class UAnimMontage;

LOOTNPOP_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(LNP_Mover_IsSprinting);
LOOTNPOP_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(LNP_Mover_IsGuarding);

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

	/** 캐릭터가 가드를 원하는지 여부를 설정한다 */
	UFUNCTION(BlueprintCallable, Category = "LNP|Movement")
	void SetWantsToGuard(bool bInWantsToGuard) { bWantsToGuard = bInWantsToGuard; }

	/** 캐릭터가 현재 가드 Modifier가 활성화되어 있으면 true를 반환한다 */
	UFUNCTION(BlueprintPure, Category = "LNP|Movement")
	bool IsGuarding() const;

	bool CanGuard();

	void SetIsAiming(bool bInIsAiming) { bIsAiming = bInIsAiming; }
	bool GetIsAiming() const { return bIsAiming; }

	/** 현재 Dash 실행 가능 여부를 반환한다 (지면, 조준 아님, 쿨다운 경과) */
	bool CanDash() const;

	/** 주어진 이동 Input Intent로 Dash를 실행한다 */
	void ExecuteDash(FVector MoveInputIntent);

	/** HitFromDirection 방향으로 Strength 크기의 넉백 임펄스를 가한다. */
	void ApplyKnockback(const FVector HitFromDirection, const float Strength);

	/** InVelocity를 초기 속도로 설정하는 Launch 레이어 무브를 큐에 추가한다. */
	void LaunchWithVelocity(FVector InVelocity);

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|Movement", meta = (AllowPrivateAccess = "true"))
	uint8 bHandleGuardChanges : 1 = 1;

	/** true이면 캐릭터가 다음 simulation tick에서 가드를 의도하고 있다 */
	UPROPERTY(BlueprintReadOnly, Category = "LNP|Movement", meta = (AllowPrivateAccess = "true"))
	uint8 bWantsToGuard : 1 = 0;

	/** 활성 Guard Modifier Handle */
	FMovementModifierHandle GuardModifierHandle;

	bool bIsAiming = false;
	float LastDashTime = -1.0f;

	// 임시 디버그 로그용 — 원격 클라이언트 가드 이동속도 미적용 이슈 진단 후 제거 예정.
	float DebugLastLoggedMaxSpeed = -1.0f;
};
