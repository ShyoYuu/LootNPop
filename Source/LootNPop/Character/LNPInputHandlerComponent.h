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

	void SetAIMoveInput(FVector InMoveInput) { AIMoveInput = InMoveInput; }
	void SetAIOrientationIntent(FVector InOrientationIntent) { AIOrientationIntent = InOrientationIntent; }

	bool GetFaceMoveDirection() const { return bFaceMoveDirection; }
	void SetFaceMoveDirection(bool bValue) { bFaceMoveDirection = bValue; }

	bool HasMovementInput() const { return !CachedMoveInputIntent.IsNearlyZero() || !AIMoveInput.IsNearlyZero(); }

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

	FVector CachedMoveInputIntent = FVector::ZeroVector;
	FRotator CachedLookInput = FRotator::ZeroRotator;
	FVector LastAffirmativeMoveInput = FVector::ZeroVector;

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
