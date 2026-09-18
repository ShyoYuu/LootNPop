// Copyright LootNPop. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "NativeGameplayTags.h"
#include "LNPSprintModifier.h"
#include "LNPGuardModifier.h"
#include "LNPADSModifier.h"
#include "LNPMoveSpeedModifier.h"
#include "LNPCharacterMoverComponent.generated.h"

LOOTNPOP_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(LNP_Mover_IsSprinting);
LOOTNPOP_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(LNP_Mover_IsGuarding);
LOOTNPOP_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(LNP_Mover_IsADS);

/**
 * LootNPop 캐릭터용 커스텀 Mover Component.
 * Sprint/Guard/ADS/MoveSpeed Modifier 갱신, Dash 실행, 넉백·Launch, 사망 정지 모드를 처리한다.
 * 의도는 전부 InputCmd(FLNPModifierInputs)로 받고 실행은 시뮬레이션 틱 안에서 한다.
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

	/**
	 * 이번 시뮬레이션 틱의 InputCmd가 실어 온 AI 이동 속도(cm/s). 0 이하면 미지정.
	 * `FLNPMoveSpeedModifier`가 MaxSpeed 산출에 쓴다 — 폰의 컴포넌트 멤버를 직접 읽으면
	 * 서버에만 값이 있어 클라이언트 재시뮬레이션이 갈라진다 (FLNPModifierInputs::AIDesiredSpeed 주석).
	 */
	float GetAIDesiredSpeedFromInput() const { return AIDesiredSpeedFromInput; }

	bool CanGuard();

	/** 캐릭터가 현재 ADS(정조준) Modifier가 활성화되어 있으면 true를 반환한다 */
	UFUNCTION(BlueprintPure, Category = "LNP|Movement")
	bool IsADS() const;

	/** 캐릭터가 현재 ADS 가능한지 확인한다 (가드 중이 아님) */
	UFUNCTION(BlueprintPure, Category = "LNP|Movement")
	bool CanADS() const;

	/** 현재 Dash 실행 가능 여부를 반환한다 (지면, ADS 아님, 쿨다운 Modifier 부재) */
	bool CanDash() const;

	/** Dash 쿨다운 길이 (초). HUD 쿨다운 표시가 읽는다. */
	float GetDashCooldown() const { return DashCooldown; }

	/** Dash가 실제로 실행된 순간 발송된다. 리시뮬레이션 중에는 발송되지 않는다. */
	FSimpleMulticastDelegate OnDashExecuted;

	/**
	 * 그래플 비행 중인지. 쿨다운이 아니라 **비행 시간과 같은 길이의 상태**다 — 도착하면 바로 풀린다.
	 * 재진입 방지가 유일한 목적이며, 대시는 이것 없이도 막힌다(CanDash가 접지를 요구하는데 비행 중엔 공중 모드다).
	 */
	bool IsGrappleFlying() const;

	/** 그래플이 실제로 실행된 순간 발송된다. 리시뮬레이션 중에는 발송되지 않는다 (OnDashExecuted와 같은 규약). */
	FSimpleMulticastDelegate OnGrappleExecuted;

	/** HitFromDirection 방향으로 Strength 크기의 넉백 임펄스를 가한다. */
	void ApplyKnockback(const FVector HitFromDirection, const float Strength);

	/** InVelocity를 초기 속도로 설정하는 Launch 레이어 무브를 큐에 추가한다. */
	void LaunchWithVelocity(FVector InVelocity);

	/**
	 * 사망 연출 동안 이동을 정지시킨다 (→ ULNPDeadMode).
	 * 각 머신이 로컬로 호출한다 — 랙돌 연출과 짝이므로 복제하지 않는다.
	 */
	void EnterDeadMode();

	/** 사망 정지를 해제하고 기본 낙하 모드로 되돌린다 (Mass 표현 풀 재사용용). */
	void ExitDeadMode();

	/** 현재 ULNPDeadMode 상태인지. */
	bool IsInDeadMode() const;

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
	void ExecuteDash(const FMoverTimeStep& TimeStep, const FVector& MoveInputIntent, const FRotator& ControlRotation,
		bool bLockOnActive);

	/**
	 * 그래플을 실행한다. ExecuteDash와 같은 제약 — 반드시 시뮬레이션 틱 안에서만 호출한다.
	 *
	 * 앵커 좌표를 InputCmd에서 받지 않고 **ID로 조회해 서버 자신의 앵커에서 읽는다.** 목적지의 권위를
	 * 서버에 남기기 위함이다 (FLNPModifierInputs::GrappleAnchorID 주석). 각도 재검증도 여기서 한다 —
	 * 서버의 원격 폰은 GetControlRotation()이 최신이 아니라 앵커 쪽에서는 각도를 볼 수 없다.
	 */
	void ExecuteGrapple(const FMoverTimeStep& TimeStep, int32 AnchorID, const FRotator& ControlRotation);

	/** 커스텀 시뮬레이션 로직이 항상 등록되도록 Override한다. */
	virtual void OnHandlerSettingChanged() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|Dash")
	float DashDuration = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|Dash")
	float DashImpulseMagnitude = 2000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|Dash")
	float DashCooldown = 1.0f;

	/** 그래플 비행 속력 (cm/s). 비행 시간 = 거리 / 이 값. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|Grapple")
	float GrappleSpeed = 2500.0f;

	/**
	 * 앵커 앞에서 멈추는 거리 (cm). 비행을 이만큼 짧게 끝내 앵커에 박히는 것을 막는다.
	 * 한 틱 오버슛(속력/60 ≈ 42cm)보다 크게 잡으면 관통이 구조적으로 불가능해진다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|Grapple")
	float GrappleStopShortDistance = 150.0f;

	/** 도착 시 남기는 잔여 속력 (cm/s). 대시처럼 마지막 속도를 유지하면 앵커를 지나쳐 날아간다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|Grapple")
	float GrappleExitSpeed = 600.0f;

	/**
	 * 시뮬레이션 쪽 각도 재검증의 허용 반각 (도). 로컬 프롬프트 판정(앵커의 AimHalfAngle, 8°)보다
	 * **훨씬 넓다** — 목적이 픽셀 단위 재현이 아니라 등 뒤 앵커로 순간이동하는 조작의 차단이기 때문이다.
	 * 원점도 다르다(카메라 vs 폰)이라 좁게 잡으면 정당한 그래플이 기각된다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|Grapple")
	float GrappleServerAimHalfAngle = 30.0f;

private:
	/** OnMoverPreSimulationTick이 InputCmd에서 옮겨 담는다. 리시뮬레이션 프레임마다 그 프레임의 값으로 갱신된다. */
	float AIDesiredSpeedFromInput = 0.f;

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

	/** 이 Component가 의도에 따라 ADS 로직을 직접 처리할지 여부. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|Movement", meta = (AllowPrivateAccess = "true"))
	uint8 bHandleADSChanges : 1 = 1;

	/** 활성 ADS Modifier Handle */
	FMovementModifierHandle ADSModifierHandle;

};
