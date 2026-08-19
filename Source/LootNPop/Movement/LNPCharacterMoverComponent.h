// Copyright LootNPop. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "NativeGameplayTags.h"
#include "LNPSprintModifier.h"
#include "LNPGuardModifier.h"
#include "LNPMoveSpeedModifier.h"
#include "LNPCharacterMoverComponent.generated.h"

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

	/** 캐릭터가 현재 Sprint 중이면 true를 반환한다 */
	UFUNCTION(BlueprintPure, Category = "LNP|Movement")
	bool IsSprinting() const;

	/** 캐릭터가 현재 Sprint 가능한지 확인한다 */
	UFUNCTION(BlueprintPure, Category = "LNP|Movement")
	bool CanSprint() const;

	/** 캐릭터가 현재 가드 Modifier가 활성화되어 있으면 true를 반환한다 */
	UFUNCTION(BlueprintPure, Category = "LNP|Movement")
	bool IsGuarding() const;

	bool CanGuard();

	void SetIsAiming(bool bInIsAiming) { bIsAiming = bInIsAiming; }
	bool GetIsAiming() const { return bIsAiming; }

	/** 현재 Dash 실행 가능 여부를 반환한다 (지면, 조준 아님, 쿨다운 Modifier 부재) */
	bool CanDash() const;

	/** Dash 쿨다운 길이 (초). HUD 쿨다운 표시가 읽는다. */
	float GetDashCooldown() const { return DashCooldown; }

	/** Dash가 실제로 실행된 순간 발송된다. 리시뮬레이션 중에는 발송되지 않는다. */
	FSimpleMulticastDelegate OnDashExecuted;

	/** HitFromDirection 방향으로 Strength 크기의 넉백 임펄스를 가한다. */
	void ApplyKnockback(const FVector HitFromDirection, const float Strength);

	/** InVelocity를 초기 속도로 설정하는 Launch 레이어 무브를 큐에 추가한다. */
	void LaunchWithVelocity(FVector InVelocity);

protected:
	/** simulation tick 직전 호출된다. 상태 기반 Modifier 변경을 적용하는 데 사용된다. */
	virtual void OnMoverPreSimulationTick(const FMoverTimeStep& TimeStep, const FMoverInputCmdContext& InputCmd) override;

	/**
	 * Dash를 실행한다. 반드시 시뮬레이션 틱(OnMoverPreSimulationTick) 안에서만 호출해야 한다 —
	 * 입력 콜백 등 바깥에서 호출하면 InputCmd를 타지 않아 서버와 리시뮬레이션이 재현할 수 없고,
	 * 로컬에서만 한 번 튀었다가 다음 권위 상태 도착 시 롤백된다.
	 *
	 * MoveInputIntent와 ControlRotation은 폰이 아니라 InputCmd에서 받는다 — 서버는 원격 폰을
	 * 버퍼된 입력으로 늦게 시뮬레이션하므로 폰의 현재 값은 해당 프레임의 값이 아니다.
	 */
	void ExecuteDash(const FMoverTimeStep& TimeStep, const FVector& MoveInputIntent, const FRotator& ControlRotation);

	/** 커스텀 시뮬레이션 로직이 항상 등록되도록 Override한다. */
	virtual void OnHandlerSettingChanged() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|Dash")
	float DashDuration = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|Dash")
	float DashImpulseMagnitude = 2000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|Dash")
	float DashCooldown = 1.0f;

private:
	/** 이 Component가 의도에 따라 Sprint 로직을 직접 처리할지 여부. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|Movement", meta = (AllowPrivateAccess = "true"))
	uint8 bHandleSprintChanges : 1 = 1;

	/** 활성 Sprint Modifier Handle */
	FMovementModifierHandle SprintModifierHandle;

	/** 이 Component가 의도에 따라 Guard 로직을 직접 처리할지 여부. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|Movement", meta = (AllowPrivateAccess = "true"))
	uint8 bHandleGuardChanges : 1 = 1;

	/** 활성 Guard Modifier Handle */
	FMovementModifierHandle GuardModifierHandle;

	bool bIsAiming = false;
};
