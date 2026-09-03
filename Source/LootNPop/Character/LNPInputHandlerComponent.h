// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MoverSimulationTypes.h"
#include "LNPInputHandlerComponent.generated.h"

class UInputAction;
class UInputMappingContext;
struct FInputActionValue;
class ULNPCharacterMoverComponent;
class ULNPPawnGravityComponent;
class ULNPControlRotationComponent;
class ULNPInteractionComponent;
class UAbilitySystemComponent;
class UMassAgentComponent;
class ULNPLockOnComponent;
struct FLNPParryStateFragment;

/**
 * 플레이어·AI의 입력을 Mover InputCmd로 변환하는 컴포넌트.
 *
 * - Enhanced Input 액션을 바인딩해 이동/시점/점프/대시/공격/가드/락온 입력을 캐싱한다.
 * - IMoverInputProducerInterface::ProduceInput에서 캐싱된 입력 + 구형 중력 기준 방향 보정을
 *   FCharacterDefaultInputs·FLNPModifierInputs로 채워 Mover 시뮬레이션에 전달한다.
 * - 컨트롤러가 없는 폰(AI)은 StateTree가 설정한 AIMoveInput/AIOrientationIntent를 대신 사용한다.
 * - 가드/패링 상태는 Mass FLNPParryStateFragment에 로컬 예측 + 서버 RTT 보정으로 반영한다.
 */
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class LOOTNPOP_API ULNPInputHandlerComponent : public UActorComponent, public IMoverInputProducerInterface
{
	GENERATED_BODY()

public:
	ULNPInputHandlerComponent();

	void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent);
	void CacheASC(UAbilitySystemComponent* InASC);

	/** 상호작용 프롬프트가 현재 입력 타입의 키 글리프를 뽑는 데 쓴다. */
	const UInputAction* GetInteractAction() const { return InteractAction; }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/**
	 * AI 이동 의도. **반드시 단위 벡터이거나 영벡터**여야 한다 — 크기로 속도를 표현하지 말 것.
	 *
	 * Mover의 UMovementUtils::ComputeVelocity는 방향 전환 항에서
	 *   Velocity += (ControlAcceleration * |Velocity| - Velocity) * min(dt * TurningBoost * Friction, 1)
	 * 를 쓰는데, ControlAcceleration이 **정규화되지 않은** 의도 벡터다(UE CMC는 여기서 GetSafeNormal()을 쓴다).
	 * 따라서 크기 s(<1)를 지속적으로 넣으면 매 프레임 속도가 s배로 깎여
	 * 평형 속도가 `Acceleration * s * dt / (1 - s)`까지 주저앉는다.
	 * 실측: s=0.3, Accel=4000, dt≈0.016 → 약 27cm/s (기대 180cm/s).
	 * 속도는 SetAIDesiredSpeed로 지정한다.
	 */
	void SetAIMoveInput(FVector InMoveInput) { AIMoveInput = InMoveInput; }
	void SetAIOrientationIntent(FVector InOrientationIntent) { AIOrientationIntent = InOrientationIntent; }

	/** AI가 원하는 이동 속도(cm/s). 0 이하면 미지정 — FLNPMoveSpeedModifier가 MaxSpeed로 반영한다. */
	void SetAIDesiredSpeed(float InSpeed) { AIDesiredSpeed = InSpeed; }
	float GetAIDesiredSpeed() const { return AIDesiredSpeed; }


	bool GetFaceMoveDirection() const { return bFaceMoveDirection; }
	void SetFaceMoveDirection(bool bValue) { bFaceMoveDirection = bValue; }

	bool HasMovementInput() const { return !CachedMoveInputIntent.IsNearlyZero() || !AIMoveInput.IsNearlyZero(); }

	/**
	 * 근접 공격 보정 회전. 월드 공간 단위 벡터이며 영벡터면 비활성이다.
	 * OnProduceInput이 플레이어 분기 끝에서 OrientationIntent를 이 값으로 덮어쓴다 —
	 * MoveInput은 건드리지 않으므로 "이동 인풋이 들어오면 무조건 우선"이 그대로 지켜진다.
	 *
	 * ⚠ 회전 보정을 루트모션 워프(bWarpRotation)로 하지 않는 이유가 여기 있다. 레이어드 무브의
	 * AngularVelocityDegrees는 MovementMixer에서 이동 모드 제안에 더해질 뿐인데, 이동 모드는 매 프레임
	 * OrientationIntent를 향해 TurningRate(엔진 기본 500도/초)로 되감으므로 워프 회전이 즉시 상쇄된다.
	 * OrientationIntent를 직접 덮어쓰면 이동 모드가 스스로 돌아주고, InputCmd 필드라 복제·롤백도 따라온다.
	 */
	void SetMeleeAssistOrientation(const FVector& WorldDir) { MeleeAssistOrientation = WorldDir.GetSafeNormal(); }
	void ClearMeleeAssistOrientation() { MeleeAssistOrientation = FVector::ZeroVector; }

	/** 장착 무기가 원거리(FreeAim)인가. 무기 데이터의 DefaultAimMode가 부여한 ASC 태그로 판정한다. */
	bool IsFreeAimMode() const;

	/**
	 * 로컬 제어 폰의 크로스헤어가 가리키는 월드 좌표. 카메라에서 시선 방향으로 트레이스해
	 * 첫 충돌점을, 아무것도 없으면 최대 거리 지점을 돌려준다. 로컬 제어가 아니면 false.
	 *
	 * 이 값은 소유 클라이언트만 계산할 수 있으므로(카메라가 로컬 상태다) OnProduceInput이
	 * InputCmd에 실어 서버로 보낸다 — 원거리 발사 방향의 단일 원본이다
	 * (FLNPModifierInputs::AimTargetLocation 주석 참조).
	 */
	static bool ComputeCrosshairAimPoint(const APawn* Pawn, FVector& OutAimPoint);

	/**
	 * ADS(정조준) 유효 상태.
	 *
	 * 별도 상태 변수를 두지 않고 bIsADSPressed와 조준 모드를 함께 본다 — 키를 누른 채
	 * 근접 무기로 교체하면 태그가 바뀌어 자동으로 false가 되므로 해제 배선이 따로 필요 없다.
	 */
	bool IsADSActive() const;

	/**
	 * Guard 유효 상태. `IsADSActive()`와 정확히 반대 조건이며, 같은 이유로 파생값이다 —
	 * 원시 `bIsGuardPressed`를 그대로 쓰면 가드 중 총으로 교체했을 때 총을 든 채 가드가 유지된다.
	 */
	bool IsGuardActive() const;

	/**
	 * 조준 모드가 바뀌었을 때 `ALNPCharacterBase::ApplyWeaponVisuals`가 호출한다.
	 *
	 * 이 키가 의미하는 행동 자체가 바뀌므로 눌린 상태를 **뗀 것으로 처리**하고 새 입력을 요구한다.
	 * 조용히 재개시키지 않는 이유: 가드의 ASC 태그·패링 프래그먼트는 입력 순간에 명령형으로 세팅되는데,
	 * 이동 모디파이어만 폴링으로 되살아나면 둘이 어긋난다.
	 */
	void NotifyAimModeChanged();

	/**
	 * 게임플레이 입력(이동·시점·공격·상호작용)을 켜고 끈다 — 인게임 메뉴가 열릴 때 사용한다.
	 * DefaultMappingContext를 통째로 제거/복원하므로 액션 바인딩은 그대로 두고 입력만 끊긴다.
	 * 끌 때는 눌린 상태가 남지 않도록 캐시된 입력을 초기화한다.
	 */
	void SetGameplayInputEnabled(bool bEnabled);

	/**
	 * **시선(Look)만 남기고** 나머지 게임플레이 입력을 막는다. 사망 연출 중 카메라는 돌리되
	 * 이동·공격·상호작용은 막고 싶을 때 쓴다.
	 *
	 * `SetGameplayInputEnabled(false)`와 다른 점: 매핑 컨텍스트를 떼지 않는다 —
	 * 그쪽은 Look까지 함께 죽으므로 사망 중 카메라 조작이 불가능해진다.
	 */
	void SetGameplayInputBlocked(bool bBlocked);

	bool IsGameplayInputBlocked() const { return bGameplayInputBlocked; }

	/**
	 * 서버 권위로 가드를 강제 해제한다 (가드 브레이크). 소유 클라에서 ReleaseGuardState를 실행하므로
	 * 눌린 상태(bIsGuardPressed)까지 내려가고, Enhanced Input의 Started는 전이에서만 발화하므로
	 * 키를 계속 누르고 있어도 떼었다 다시 누르기 전까지 재가드되지 않는다.
	 */
	UFUNCTION(Client, Reliable)
	void Client_ForceReleaseGuard();

protected:
	virtual void BeginPlay() override;

	virtual void ProduceInput_Implementation(int32 SimTimeMs, FMoverInputCmdContext& InputCmdResult) override;
	virtual void OnProduceInput(float DeltaMs, FMoverInputCmdContext& OutInputCmd);
	FLNPParryStateFragment* GetParryFragment() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> DashAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> InteractAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> AttackAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> GuardAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> ADSAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> LockOnAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TArray<TObjectPtr<UInputAction>> ActiveSkillActions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|Movement")
	bool bUseBaseRelativeMovement = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|Movement")
	bool bFaceMoveDirection = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|Movement")
	bool bMaintainLastInputOrientation = false;

private:
	UPROPERTY()
	TObjectPtr<ULNPCharacterMoverComponent> MoverComponent;

	UPROPERTY()
	TObjectPtr<ULNPPawnGravityComponent> GravityComponent;

	UPROPERTY()
	TObjectPtr<ULNPControlRotationComponent> ControlRotationComponent;

	UPROPERTY()
	TObjectPtr<ULNPInteractionComponent> InteractionComponent;

	UPROPERTY()
	TObjectPtr<UAbilitySystemComponent> ASC;

	UPROPERTY()
	TObjectPtr<ULNPLockOnComponent> LockOnComponent;

	FVector AIMoveInput = FVector::ZeroVector;
	FVector AIOrientationIntent = FVector::ZeroVector;
	float   AIDesiredSpeed = 0.f;

	FVector CachedMoveInputIntent = FVector::ZeroVector;
	FRotator CachedLookInput = FRotator::ZeroRotator;
	FVector LastAffirmativeMoveInput = FVector::ZeroVector;

	/** SetMeleeAssistOrientation 참조. 월드 공간 단위 벡터, 영벡터면 비활성. */
	FVector MeleeAssistOrientation = FVector::ZeroVector;

	/** true면 Look을 제외한 모든 입력 콜백이 조기 반환한다 (SetGameplayInputBlocked). */
	bool bGameplayInputBlocked = false;

	bool bIsJumpPressed = false;
	bool bIsJumpJustPressed = false;
	bool bIsDashPressed = false;
	bool bIsDashJustPressed = false;
	bool bIsInteractPressed = false;
	bool bIsInteractJustPressed = false;
	bool bIsAttackPressed = false;
	bool bIsAttackJustPressed = false;
	bool bIsGuardPressed = false;
	bool bIsGuardJustPressed = false;
	bool bIsADSPressed = false;
	bool bIsADSJustPressed = false;
	bool bIsLockOnPressed = false;
	bool bIsLockOnJustPressed = false;
	TArray<bool> ActiveSkillPressed;
	TArray<bool> ActiveSkillJustPressed;

	bool bIsDashBuffered = false;
	float DashBufferTime = -1.0f;

	bool bIsAttackBuffered = false;
	float AttackBufferTime = -1.0f;

	FTimerHandle ParryWindowTimer;

	/** 서버 전용 패링 창 타이머. Server_SetGuardState가 RTT 역보정된 시간으로 별도 관리한다. */
	FTimerHandle ServerParryWindowTimer;

	UPROPERTY(EditAnywhere, Category = "LNP|Guard", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float ParryWindowDuration = 0.15f;

	/**
	 * Guard 상태를 서버에 복제한다. 리슨 서버 호스트가 자신을 호출하면 즉시 로컬 실행되므로
	 * HasAuthority 분기 없이 항상 호출한다.
	 * 서버는 수신 시각에서 방어자 RTT/2를 뺀 시각을 패링 창 시작점으로 사용해 지연을 보정한다 (섹션 5.1).
	 */
	UFUNCTION(Server, Reliable)
	void Server_SetGuardState(bool bGuarding);

	void OnMoveTriggered(const FInputActionValue& Value);
	void OnMoveCompleted(const FInputActionValue& Value);
	void OnLookTriggered(const FInputActionValue& Value);
	void OnLookCompleted(const FInputActionValue& Value);
	void OnJumpStarted(const FInputActionValue& Value);
	void OnJumpReleased(const FInputActionValue& Value);
	void OnDashStarted(const FInputActionValue& Value);
	void OnDashReleased(const FInputActionValue& Value);
	void OnInteractStarted(const FInputActionValue& Value);
	void OnInteractReleased(const FInputActionValue& Value);
	void OnAttackTriggered(const FInputActionValue& Value);
	void OnAttackReleased(const FInputActionValue& Value);
	void OnGuardStarted(const FInputActionValue& Value);
	void OnGuardReleased(const FInputActionValue& Value);

	/**
	 * 가드 해제 일체 — 눌림 플래그, ASC 루즈 태그, 패링 창 타이머, Mass 프래그먼트, 서버 RPC.
	 * 키를 뗐을 때와 무기 교체로 조준 모드가 바뀌었을 때 양쪽에서 부른다. 멱등하다.
	 */
	void ReleaseGuardState();
	void OnADSStarted(const FInputActionValue& Value);
	void OnADSReleased(const FInputActionValue& Value);
	void OnLockOnStarted(const FInputActionValue& Value);
	void OnLockOnReleased(const FInputActionValue& Value);
	void OnActiveSkillStarted(const FInputActionValue& Value, int32 SlotIndex);
	void OnActiveSkillReleased(const FInputActionValue& Value, int32 SlotIndex);

	/**
	 * PIE 멀티플레이 테스트 전용. 콘솔 변수(LNP.Debug.AuthorityAutoAction / LNP.Debug.ClientAutoAction)로
	 * 서버(HasAuthority)·클라이언트 캐릭터 중 하나가 자동으로 공격 또는 가드/패링을 반복하게 한다.
	 * Shift+F1로 PIE 창을 전환하지 않고 한쪽 캐릭터만 조작하며 상대 반응을 테스트할 수 있다.
	 */
	void TickDebugAutoAction(float DeltaTime);
	float DebugAutoActionTimer = 0.f;
	bool  bDebugGuardPulseActive = false;
};
