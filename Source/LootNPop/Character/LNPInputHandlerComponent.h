// Fill out your copyright notice in the Description page of Project Settings.

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

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class LOOTNPOP_API ULNPInputHandlerComponent : public UActorComponent, public IMoverInputProducerInterface
{
	GENERATED_BODY()

public:
	ULNPInputHandlerComponent();

	void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent);
	void CacheASC(UAbilitySystemComponent* InASC);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void SetAIMoveInput(FVector InMoveInput) { AIMoveInput = InMoveInput; }
	void SetAIOrientationIntent(FVector InOrientationIntent) { AIOrientationIntent = InOrientationIntent; }

	bool GetFaceMoveDirection() const { return bFaceMoveDirection; }
	void SetFaceMoveDirection(bool bValue) { bFaceMoveDirection = bValue; }

	bool HasMovementInput() const { return !CachedMoveInputIntent.IsNearlyZero() || !AIMoveInput.IsNearlyZero(); }

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

	FVector CachedMoveInputIntent = FVector::ZeroVector;
	FRotator CachedLookInput = FRotator::ZeroRotator;
	FVector LastAffirmativeMoveInput = FVector::ZeroVector;

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
