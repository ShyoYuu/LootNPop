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
class ULNPInteractionComponent;

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class LOOTNPOP_API ULNPInputHandlerComponent : public UActorComponent, public IMoverInputProducerInterface
{
	GENERATED_BODY()

public:
	ULNPInputHandlerComponent();

	void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void SetAIMoveInput(FVector InMoveInput) { AIMoveInput = InMoveInput; }
	void SetAIOrientationIntent(FVector InOrientationIntent) { AIOrientationIntent = InOrientationIntent; }

	bool GetFaceMoveDirection() const { return bFaceMoveDirection; }

protected:
	virtual void BeginPlay() override;

	virtual void ProduceInput_Implementation(int32 SimTimeMs, FMoverInputCmdContext& InputCmdResult) override;
	virtual void OnProduceInput(float DeltaMs, FMoverInputCmdContext& OutInputCmd);

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
	TObjectPtr<ULNPInteractionComponent> InteractionComponent;

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
	void OnAttackStarted(const FInputActionValue& Value);
	void OnAttackReleased(const FInputActionValue& Value);
	void OnGuardStarted(const FInputActionValue& Value);
	void OnGuardReleased(const FInputActionValue& Value);
	void OnLockOnStarted(const FInputActionValue& Value);
	void OnLockOnReleased(const FInputActionValue& Value);
	void OnActiveSkillStarted(const FInputActionValue& Value, int32 SlotIndex);
	void OnActiveSkillReleased(const FInputActionValue& Value, int32 SlotIndex);
};
