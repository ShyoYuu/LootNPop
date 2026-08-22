// Copyright LootNPop. All Rights Reserved.

#include "Movement/LNPCharacterMoverComponent.h"
#include "Movement/LNPCharacterMovementSettings.h"
#include "Movement/LNPAsyncWalkingMode.h"
#include "Movement/LNPDeadMode.h"
#include "Movement/LNPModifierInputs.h"
#include "Movement/LNPDashCooldownModifier.h"
#include "Character/LNPCharacterBase.h"
#include "GAS/Attributes/LNPBaseAttributeSet.h"
#include "LNPGameplayTags.h"
#include "LootNPop.h"

#include "DefaultMovementSet/Settings/CommonLegacyMovementSettings.h"
#include "DefaultMovementSet/Modes/AsyncFallingMode.h"
#include "DefaultMovementSet/LayeredMoves/BasicLayeredMoves.h"
#include "DefaultMovementSet/LayeredMoves/LaunchMove.h"
#include "DefaultMovementSet/LayeredMoves/AnimRootMotionLayeredMove.h"
#include "DefaultMovementSet/InstantMovementEffects/BasicInstantMovementEffects.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

UE_DEFINE_GAMEPLAY_TAG_COMMENT(LNP_Mover_IsSprinting, "LNP.Mover.IsSprinting", "Character is sprinting");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(LNP_Mover_IsGuarding, "LNP.Mover.IsGuarding",  "Character is guarding");

// UCommonLegacyMovementSettings 못 가져왔을 때 fallback용
const FName DefaultWalkingMode = TEXT("LNPAsyncWalking");
const FName DefaultFallingMode = TEXT("AsyncFalling");
const FName LNPDeadModeName    = TEXT("LNPDead");

ULNPCharacterMoverComponent::ULNPCharacterMoverComponent()
{
	bHandleSprintChanges = 1;
	bHandleGuardChanges = 1;

	// 기본 이동 모드
	MovementModes.Add(DefaultWalkingMode, CreateDefaultSubobject<ULNPAsyncWalkingMode>(TEXT("LNPAsyncWalkingMode")));
	MovementModes.Add(DefaultFallingMode, CreateDefaultSubobject<UAsyncFallingMode>(TEXT("AsyncFallingMode")));
	// 사망 정지 모드 — 엔진 UNullMovementMode는 원점 텔레포트 버그가 있어 쓸 수 없다 (ULNPDeadMode 주석 참조).
	MovementModes.Add(LNPDeadModeName, CreateDefaultSubobject<ULNPDeadMode>(TEXT("LNPDeadMode")));

	StartingMovementMode = DefaultFallingMode;
}

bool ULNPCharacterMoverComponent::IsSprinting() const
{
	return HasGameplayTag(LNP_Mover_IsSprinting, true);
}

bool ULNPCharacterMoverComponent::CanSprint() const
{
	return IsOnGround() && !IsGuarding();
}

bool ULNPCharacterMoverComponent::IsGuarding() const
{
	return HasGameplayTag(LNP_Mover_IsGuarding, true);
}

bool ULNPCharacterMoverComponent::CanGuard()
{
	return true;
}

bool ULNPCharacterMoverComponent::CanDash() const
{
	// 쿨다운은 월드 시간이 아니라 SyncState에 실리는 Modifier의 존재로 판정한다 —
	// 월드 시간 기준은 서버의 지연 시뮬레이션·리시뮬레이션에서 클라이언트와 어긋나 리컨사일 루프를 만든다.
	return IsOnGround() && !bIsAiming && FindMovementModifierByType<FLNPDashCooldownModifier>() == nullptr;
}

void ULNPCharacterMoverComponent::ExecuteDash(const FMoverTimeStep& TimeStep, const FVector& MoveInputIntent, const FRotator& ControlRotation)
{
	APawn* Pawn = CastChecked<APawn>(GetOwner());
	ALNPCharacterBase* Character = Cast<ALNPCharacterBase>(Pawn);
	if (!Character)
		return;

	// 물리 대시 방향: 이동 입력이 있으면 컨트롤 회전 기준 입력 방향, 없으면 후방 회피
	const bool bHasMoveInput = !MoveInputIntent.IsNearlyZero();
	const FVector DashDirection = bHasMoveInput
		? ControlRotation.RotateVector(MoveInputIntent).GetSafeNormal()
		: -Pawn->GetActorForwardVector();

	// 몽타주 방향 태그: Strafe 모드(FreeAim/LockOn)는 캐릭터가 시선 방향을 유지하므로 4방향 몽타주가 필요하고,
	// 일반 모드는 캐릭터가 이동 방향을 바라보므로 앞/뒤 2방향으로 충분하다. 태그는 ChooserTable 평가에 쓰인다.
	const UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent();
	const bool bIsStrafe = ASC &&
		(ASC->HasMatchingGameplayTag(TAG_AimMode_FreeAim) || ASC->HasMatchingGameplayTag(TAG_AimMode_LockOn));

	FGameplayTag DirTag = TAG_Montage_Value_Direction_Back;
	if (bHasMoveInput)
	{
		if (bIsStrafe)
		{
			// MoveInputIntent는 카메라 로컬 공간 (X=Forward, Y=Right) — 입력 각도로 4방향 분류
			const float Angle = FMath::RadiansToDegrees(FMath::Atan2(MoveInputIntent.Y, MoveInputIntent.X));
			if      (Angle >= -45.f && Angle <   45.f) DirTag = TAG_Montage_Value_Direction_Front;
			else if (Angle >=  45.f && Angle <  135.f) DirTag = TAG_Montage_Value_Direction_Right;
			else if (Angle >= -135.f && Angle < -45.f) DirTag = TAG_Montage_Value_Direction_Left;
			else                                       DirTag = TAG_Montage_Value_Direction_Back;
		}
		else
		{
			DirTag = TAG_Montage_Value_Direction_Front;
		}
	}

	const float DashDurationMs = DashDuration * 1000.0f;

	// 쿨다운을 SyncState에 남긴다 — 롤백 시 함께 복원되어 클라이언트와 서버의 CanDash 판정이 일치한다.
	TSharedPtr<FLNPDashCooldownModifier> CooldownModifier = MakeShared<FLNPDashCooldownModifier>();
	CooldownModifier->DurationMs = DashCooldown * 1000.0f;
	QueueMovementModifier(CooldownModifier);

	TSharedPtr<FLayeredMove_LinearVelocity> DashMove = MakeShared<FLayeredMove_LinearVelocity>();
	DashMove->Velocity = DashDirection * DashImpulseMagnitude;
	DashMove->DurationMs = DashDurationMs;
	DashMove->MixMode = EMoveMixMode::OverrideVelocity;
	DashMove->FinishVelocitySettings.FinishVelocityMode = ELayeredMoveFinishVelocityMode::MaintainLastRootMotionVelocity;
	QueueLayeredMove(DashMove);

	// 몽타주는 연출이다. 평가에 실패해도 물리 대시는 그대로 실행한다
	// (기존 구현은 여기서 조기 return이라 몽타주가 없으면 대시 자체가 취소됐다).
	UAnimMontage* SelectedMontage = Character->EvaluateMontage(TAG_Montage_Situation_Dash, DirTag);
	if (SelectedMontage)
	{
		// 시작 위치를 재생 중인 몽타주 인스턴스에서 되읽지 않고 0으로 고정한다 —
		// 서버와 리시뮬레이션에는 몽타주가 재생되지 않아 되읽은 값이 서로 갈린다.
		TSharedPtr<FLayeredMove_AnimRootMotion> AnimSyncMove = MakeShared<FLayeredMove_AnimRootMotion>();
		AnimSyncMove->MontageState.Montage = SelectedMontage;
		AnimSyncMove->MontageState.PlayRate = 1.0f;
		AnimSyncMove->MontageState.StartingMontagePosition = 0.0f;
		AnimSyncMove->MontageState.CurrentPosition = 0.0f;
		AnimSyncMove->DurationMs = DashDurationMs;
		QueueLayeredMove(AnimSyncMove);
	}

	// 여기부터는 시뮬레이션 상태가 아닌 연출·HUD다. 리시뮬레이션마다 반복되면
	// 몽타주가 다시 재생되고 HUD 쿨다운 파이가 계속 리셋되므로 첫 시뮬레이션에서만 실행한다.
	if (TimeStep.bIsResimulating)
		return;

	OnDashExecuted.Broadcast();

	if (SelectedMontage)
	{
		if (USkeletalMeshComponent* Mesh = Pawn->FindComponentByClass<USkeletalMeshComponent>())
		{
			if (UAnimInstance* AnimInstance = Mesh->GetAnimInstance())
				AnimInstance->Montage_Play(SelectedMontage, 1.0f);
		}
	}
}

void ULNPCharacterMoverComponent::ApplyKnockback(const FVector HitFromDirection, const float Strength)
{
	if (HitFromDirection.IsNearlyZero() || Strength <= 0.f)
		return;

	if (TSharedPtr<FApplyVelocityEffect> KnockbackEffect = MakeShared<FApplyVelocityEffect>())
	{
		KnockbackEffect->VelocityToApply = HitFromDirection.GetSafeNormal() * Strength;
		KnockbackEffect->bAdditiveVelocity = true;

		// GroundMovementMode에서는 매 틱 속도를 MaxWalkSpeed로 클램핑하고 위치를 지면에 스냅해버려서 의도한 넉백 느낌이 안남. 따라서 AirMovementMode로 적용.
		if (const UCommonLegacyMovementSettings* CommonSettings = FindSharedSettings<UCommonLegacyMovementSettings>())
			KnockbackEffect->ForceMovementMode = CommonSettings->AirMovementModeName;
		else
			KnockbackEffect->ForceMovementMode = GetMovementModeName();
		QueueInstantMovementEffect(KnockbackEffect);
	}
}

void ULNPCharacterMoverComponent::LaunchWithVelocity(FVector InVelocity)
{
	if (InVelocity.IsNearlyZero())
		return;

	TSharedPtr<FLayeredMove_Launch> LaunchMove = MakeShared<FLayeredMove_Launch>();
	LaunchMove->LaunchVelocity = InVelocity;
	LaunchMove->DurationMs = 0.f;
	LaunchMove->MixMode = EMoveMixMode::OverrideVelocity;
	if (const UCommonLegacyMovementSettings* CommonSettings = FindSharedSettings<UCommonLegacyMovementSettings>())
		LaunchMove->ForceMovementMode = CommonSettings->AirMovementModeName;
	else
		LaunchMove->ForceMovementMode = DefaultFallingMode;
	QueueLayeredMove(LaunchMove);
}

void ULNPCharacterMoverComponent::EnterDeadMode()
{
	if (GetMovementModeName() == LNPDeadModeName)
		return;

	QueueNextMode(LNPDeadModeName);
}

void ULNPCharacterMoverComponent::ExitDeadMode()
{
	if (GetMovementModeName() != LNPDeadModeName)
		return;

	// 낙하 모드로 되돌린다 — 접지 판정은 AsyncFallingMode의 전이가 알아서 한다.
	if (const UCommonLegacyMovementSettings* CommonSettings = FindSharedSettings<UCommonLegacyMovementSettings>())
		QueueNextMode(CommonSettings->AirMovementModeName);
	else
		QueueNextMode(DefaultFallingMode);
}

bool ULNPCharacterMoverComponent::IsInDeadMode() const
{
	return GetMovementModeName() == LNPDeadModeName;
}

void ULNPCharacterMoverComponent::OnMoverPreSimulationTick(const FMoverTimeStep& TimeStep, const FMoverInputCmdContext& InputCmd)
{
	// MoveSpeed Modifier는 상시 활성 — 한 번만 큐잉하고 이후 매 틱 OnPreMovement가 MaxSpeed를 재계산한다.
	// (리시뮬레이션·재빙의로 Modifier가 유실될 수 있어 타입 조회로 부재를 확인한다.)
	if (FindMovementModifierByType<FLNPMoveSpeedModifier>() == nullptr)
	{
		QueueMovementModifier(MakeShared<FLNPMoveSpeedModifier>());
	}

	// Guard/Sprint 의도는 컴포넌트 멤버 변수가 아닌 InputCmd에서 읽는다 — Jump가 FCharacterDefaultInputs::
	// bIsJumpJustPressed를 InputCmd로 전달받는 것과 동일한 방식. 평범한 컴포넌트 멤버는 Mover의
	// 예측·복제·리시뮬레이션 파이프라인을 타지 않아 원격 클라이언트에서 신뢰할 수 없었다.
	const FLNPModifierInputs* ModifierInputs = InputCmd.InputCollection.FindDataByType<FLNPModifierInputs>();

	// Guard Modifier 관리 (Sprint보다 먼저 처리 — Guard 중에는 Sprint 불가)
	if (bHandleGuardChanges)
	{
		const FLNPGuardModifier* ActiveModifier = static_cast<const FLNPGuardModifier*>(FindMovementModifier(GuardModifierHandle));
		if (ActiveModifier == nullptr)
		{
			ActiveModifier = FindMovementModifierByType<FLNPGuardModifier>();
		}

		const bool bIsGuarding = HasGameplayTag(LNP_Mover_IsGuarding, true);
		const bool bShouldGuard = ModifierInputs && ModifierInputs->bWantsToGuard;

		if (bIsGuarding && (!bShouldGuard || !CanGuard()))
		{
			if (ActiveModifier)
			{
				CancelModifierFromHandle(ActiveModifier->GetHandle());
				GuardModifierHandle.Invalidate();
				ActiveModifier = nullptr;
			}
		}
		else if (!bIsGuarding && bShouldGuard && CanGuard())
		{
			TSharedPtr<FLNPGuardModifier> NewModifier = MakeShared<FLNPGuardModifier>();
			GuardModifierHandle = QueueMovementModifier(NewModifier);
			ActiveModifier = NewModifier.Get();
		}
	}

	// Sprint Modifier 관리 (CanSprint 내부에서 IsGuarding() 체크)
	if (bHandleSprintChanges)
	{
		const FLNPSprintModifier* ActiveModifier = static_cast<const FLNPSprintModifier*>(FindMovementModifier(SprintModifierHandle));
		if (ActiveModifier == nullptr)
		{
			ActiveModifier = FindMovementModifierByType<FLNPSprintModifier>();
		}

		const bool bIsSprinting  = HasGameplayTag(LNP_Mover_IsSprinting, true);
		const bool bShouldSprint = ModifierInputs && ModifierInputs->bWantsToSprint;

		if (bIsSprinting && (!bShouldSprint || !CanSprint()))
		{
			if (ActiveModifier)
			{
				CancelModifierFromHandle(ActiveModifier->GetHandle());
				SprintModifierHandle.Invalidate();
				ActiveModifier = nullptr;
			}
		}
		else if (!bIsSprinting && bShouldSprint && CanSprint())
		{
			TSharedPtr<FLNPSprintModifier> NewModifier = MakeShared<FLNPSprintModifier>();
			SprintModifierHandle = QueueMovementModifier(NewModifier);
			ActiveModifier = NewModifier.Get();
		}
	}

	// Dash 실행 — 의도는 InputCmd로 전달받고 실행은 시뮬레이션 안에서 이루어진다.
	// 입력 콜백에서 직접 실행하던 기존 방식은 서버와 리시뮬레이션이 재현할 수 없어,
	// 클라이언트가 대시하면 로컬에서만 튀었다가 롤백되고 서버에는 아무 일도 일어나지 않았다.
	if (ModifierInputs && ModifierInputs->bWantsToDash && CanDash())
	{
		const FCharacterDefaultInputs* CharacterInputs = InputCmd.InputCollection.FindDataByType<FCharacterDefaultInputs>();
		ExecuteDash(TimeStep, ModifierInputs->DashInputIntent,
			CharacterInputs ? CharacterInputs->ControlRotation : FRotator::ZeroRotator);
	}

	// 기본 기능(점프, 앉기) 처리를 위해 Super 호출
	Super::OnMoverPreSimulationTick(TimeStep, InputCmd);
}

void ULNPCharacterMoverComponent::OnHandlerSettingChanged()
{
	// Super는 점프/자세 설정에 따라 OnMoverPreSimulationTick을 추가/제거한다.
	//Super::OnHandlerSettingChanged();

	const bool bIsHandlingAnySettings = bHandleSprintChanges || bHandleGuardChanges || bHandleJump || bHandleStanceChanges;

	if (bIsHandlingAnySettings)
	{
		OnPreSimulationTick.AddUniqueDynamic(this, &ULNPCharacterMoverComponent::OnMoverPreSimulationTick);
	}
	else
	{
		OnPreSimulationTick.RemoveDynamic(this, &ULNPCharacterMoverComponent::OnMoverPreSimulationTick);
	}
}

namespace
{
	/** 화면 메시지 슬롯 — 매 프레임 같은 키로 덮어써 각 한 줄만 유지한다. */
	constexpr uint64 GLNPShowSpeedMsgKey    = 0x4C4E'5053;
	constexpr uint64 GLNPShowSpeedCfgMsgKey = 0x4C4E'5054;

	FTSTicker::FDelegateHandle GLNPShowSpeedTicker;

	void LNPStopShowSpeed()
	{
		if (GLNPShowSpeedTicker.IsValid())
		{
			FTSTicker::GetCoreTicker().RemoveTicker(GLNPShowSpeedTicker);
			GLNPShowSpeedTicker.Reset();
		}
		if (GEngine != nullptr)
		{
			GEngine->RemoveOnScreenDebugMessage(GLNPShowSpeedMsgKey);
			GEngine->RemoveOnScreenDebugMessage(GLNPShowSpeedCfgMsgKey);
		}
	}

	/**
	 * 디버그: LNP.Debug.ShowSpeed [0|1] — 로컬 폰의 실측 이동 속도를 화면에 표시한다.
	 *
	 * Mover의 Velocity나 MaxSpeed 설정을 일절 읽지 않고, **액터 월드 위치의 프레임 간 변화량**만
	 * 나눠서 구한다. 이동 로직이 의도한 속도를 실제로 내고 있는지 바깥에서 교차 검증하는 용도다.
	 * 코어 티커에 붙으므로 Mover 시뮬레이션 파이프라인과도 무관하다.
	 */
	FAutoConsoleCommandWithWorldAndArgs GLNPDebugShowSpeed(
		TEXT("LNP.Debug.ShowSpeed"),
		TEXT("Show the local pawn's measured speed, derived purely from world position deltas. Args: [0|1]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			LNPStopShowSpeed();

			const bool bEnable = (Args.Num() == 0) || (Args[0] != TEXT("0"));
			if (!bEnable || World == nullptr)
			{
				UE_LOG(LogLootNPop, Log, TEXT("[Debug] ShowSpeed off"));
				return;
			}

			struct FLNPSpeedSample
			{
				FVector LastLocation = FVector::ZeroVector;
				bool    bHasLast     = false;
				float   Smoothed     = 0.0f;
				float   Peak         = 0.0f;
			};

			TSharedRef<FLNPSpeedSample> State = MakeShared<FLNPSpeedSample>();
			TWeakObjectPtr<UWorld>      WeakWorld = World;

			GLNPShowSpeedTicker = FTSTicker::GetCoreTicker().AddTicker(
				FTickerDelegate::CreateLambda([State, WeakWorld](float DeltaTime) -> bool
				{
					UWorld* TickWorld = WeakWorld.Get();
					if (TickWorld == nullptr)
					{
						return false; // 월드가 사라지면(PIE 종료) 티커를 스스로 정리한다
					}

					const APlayerController* PC = TickWorld->GetFirstPlayerController();
					const APawn* Pawn = (PC != nullptr) ? PC->GetPawn() : nullptr;
					if (Pawn == nullptr || DeltaTime <= 0.0f)
					{
						return true;
					}

					const FVector Location = Pawn->GetActorLocation();
					if (State->bHasLast)
					{
						const float Speed = static_cast<float>((Location - State->LastLocation).Size()) / DeltaTime;

						// 프레임 단위 델타는 튀므로 EMA로 다듬은 값을 함께 보여준다.
						State->Smoothed = FMath::Lerp(State->Smoothed, Speed, 0.15f);
						State->Peak     = FMath::Max(State->Peak, State->Smoothed);

						if (GEngine != nullptr)
						{
							GEngine->AddOnScreenDebugMessage(GLNPShowSpeedMsgKey, 0.2f, FColor::Green,
								FString::Printf(TEXT("[Measured] now %7.1f   avg %7.1f   peak %7.1f  cm/s"),
									Speed, State->Smoothed, State->Peak));
						}
					}

					// 대조용 설정값 — 실측이 이 목표치에 도달하는지 나란히 본다.
					// 위 실측 블록과 분리해 두어야 "외부 관측"과 "우리 로직이 주장하는 값"이 섞이지 않는다.
					if (GEngine != nullptr)
					{
						if (const ULNPCharacterMoverComponent* Mover = Pawn->FindComponentByClass<ULNPCharacterMoverComponent>())
						{
							const UCommonLegacyMovementSettings* Common = Mover->FindSharedSettings<UCommonLegacyMovementSettings>();

							const TCHAR* StateName = TEXT("Walk");
							if (Mover->HasGameplayTag(LNP_Mover_IsSprinting, /*bExactMatch=*/true))
								StateName = TEXT("Sprint");
							else if (Mover->HasGameplayTag(LNP_Mover_IsGuarding, /*bExactMatch=*/true))
								StateName = TEXT("Guard");

							float Multiplier = 1.0f;
							if (const IAbilitySystemInterface* ASCInterface = Cast<IAbilitySystemInterface>(Pawn))
							{
								if (const UAbilitySystemComponent* ASC = ASCInterface->GetAbilitySystemComponent())
									Multiplier = ASC->GetNumericAttribute(ULNPBaseAttributeSet::GetMoveSpeedAttribute());
							}

							GEngine->AddOnScreenDebugMessage(GLNPShowSpeedCfgMsgKey, 0.2f, FColor::Cyan,
								FString::Printf(TEXT("[Settings] MaxSpeed %7.1f  cm/s   state %-6s   MoveSpeed x%.2f"),
									Common ? Common->MaxSpeed : 0.0f, StateName, Multiplier));
						}
					}

					State->LastLocation = Location;
					State->bHasLast     = true;
					return true;
				}));

			UE_LOG(LogLootNPop, Log, TEXT("[Debug] ShowSpeed on"));
		}));
}
