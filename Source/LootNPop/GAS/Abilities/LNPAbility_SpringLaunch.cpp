// Copyright (c) 2026 LootNPop. All rights reserved.

#include "GAS/Abilities/LNPAbility_SpringLaunch.h"
#include "GAS/Abilities/LNPAttackInputTargetData.h"
#include "DataAsset/LNPWorldDeviceConfig.h"
#include "Character/LNPCharacterBase.h"
#include "Character/LNPInputHandlerComponent.h"
#include "Interaction/LNPInteractable.h"
#include "Movement/LNPCharacterMoverComponent.h"
#include "WorldDevice/LNPSpringLauncher.h"
#include "LNPGameplayTags.h"
#include "LootNPop.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Animation/AnimMontage.h"
#include "DefaultMovementSet/LayeredMoves/BasicLayeredMoves.h"

ULNPAbility_SpringLaunch::ULNPAbility_SpringLaunch()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	FGameplayTagContainer Tags;
	Tags.AddTag(TAG_Ability_SpringLaunch);
	SetAssetTags(Tags);

	// 정렬 중에는 이동 입력을 죽인다 — 대시가 그랬듯 LayeredMove는 이 태그에 막히지 않으므로
	// 정렬 이동만 남고 플레이어가 슬롯 밖으로 걸어 나가지 못한다.
	ActivationOwnedTags.AddTag(TAG_Block_MovementInput);
	ActivationBlockedTags.AddTag(TAG_State_Staggered);
	ActivationBlockedTags.AddTag(TAG_Ability_SpringLaunch);
}

void ULNPAbility_SpringLaunch::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	ALNPCharacterBase* Character = GetOwningCharacter();
	ULNPCharacterMoverComponent* Mover = Character ? Character->GetMoverComponent() : nullptr;

	// 어느 런처인지는 발동 요청에만 실려 온다. 서버가 스스로 탐색하면 "원본이 둘"이 되어
	// 클라와 다른 런처를 고를 수 있다 (TechDesign_Networking.md §4.8).
	const FLNPSpringLaunchTargetData* Data = LNPAttackInput::Find<FLNPSpringLaunchTargetData>(TriggerEventData);
	ALNPSpringLauncher* Launcher = Data ? Data->Launcher.Get() : nullptr;

	// 서버 재검증 — 클라 판정 시점과 도착 시점 사이에 플레이어가 멀어졌을 수 있다.
	//
	// ⚠️ 여기서 조용히 EndAbility하면 증상이 **"한 번 깜박이고 제자리"**로만 나타난다. 소유 클라는
	// 예측으로 정렬·발사를 실행했는데 서버가 기각하면 다음 권위 상태가 그것을 통째로 롤백하기 때문이다.
	// 어느 조건이 걸렸는지 반드시 남긴다 — 발동 실패는 원인별로 대처가 완전히 다르다.
	const bool bCanInteract = (Character != nullptr && Launcher != nullptr)
		&& ILNPInteractable::Execute_CanInteract(Launcher, Character);
	if (Character == nullptr || Mover == nullptr || Launcher == nullptr || !bCanInteract
		|| !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		UE_LOG(LogLootNPop, Warning,
			TEXT("[SpringLaunch] Activation rejected — character=%d mover=%d launcher=%d canInteract=%d dist=%.0f authority=%d"),
			Character != nullptr, Mover != nullptr, Launcher != nullptr, bCanInteract,
			(Character && Launcher) ? FVector::Dist(Character->GetActorLocation(), Launcher->GetActorLocation()) : -1.f,
			Character ? Character->HasAuthority() : -1);

		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	PendingLauncher = Launcher;

	const ULNPWorldDeviceConfig* Config = ULNPWorldDeviceConfig::Get(Launcher);
	const float ConfiguredAlign = Config ? Config->AlignDuration : 1.0f;
	const float Duration = AlignMontage ? AlignMontage->GetPlayLength() : ConfiguredAlign;
	AlignSeconds = Duration;
	ActivationTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;

	/**
	 * ⚠️ **정렬 이동은 발사보다 반드시 먼저 끝나야 한다.**
	 *
	 * 레이어드 무브가 끝나는 틱에 엔진은 `FinishVelocitySettings`의 잔여 속도를 캐릭터에 적용하는데
	 * (`FLayeredMoveGroup::GatherResidualVelocitySettings`), 그 적용은 같은 패스에서 **큐잉된 새 무브를
	 * 활성화하기 전**에 일어난다. 정렬의 잔여 속도가 0인 채로 발사를 같은 시점에 큐잉하면
	 * 발사가 실린 그 틱이 통째로 0으로 덮인다 — 증상은 **"정렬까지는 되는데 그 뒤로 아무 일도 안 일어남"**이다.
	 *
	 * 그래서 정렬 이동을 앞쪽 구간에만 두고 뒤에 정지 구간(Settle)을 남긴다. 부수적으로 체감도 낫다 —
	 * 미끄러져 자리를 잡고, 잠깐 멈췄다가, 튀어 나간다.
	 */
	const float SettleSeconds = FMath::Clamp(Duration * 0.25f, 0.1f, 0.35f);
	const float AlignMoveSeconds = FMath::Max(Duration - SettleSeconds, 0.05f);

	// --- 정렬 ① 위치: 슬롯으로 끌어당긴다 ---
	// 접평면 성분만 쓴다. 반지름 방향까지 맞추려 들면 캐릭터를 지면에서 띄워 Falling으로 전환시킨다
	// (근접 보정이 캡슐 중심을 워프 타깃으로 넘겼다가 겪은 것과 같은 함정).
	if (const USceneComponent* Slot = Launcher->GetLaunchSlot())
	{
		const FVector SelfLoc = Character->GetActorLocation();
		const FVector Up = -SelfLoc.GetSafeNormal();
		const FVector Delta = FVector::VectorPlaneProject(Slot->GetComponentLocation() - SelfLoc, Up);

		if (!Delta.IsNearlyZero())
		{
			TSharedPtr<FLayeredMove_LinearVelocity> AlignMove = MakeShared<FLayeredMove_LinearVelocity>();
			AlignMove->Velocity = Delta / AlignMoveSeconds;
			AlignMove->DurationMs = AlignMoveSeconds * 1000.f;
			// 정렬은 "제안 위에 얹는 보정"이 아니라 "정해진 슬롯으로 데려가는 이동"이라 Override다.
			AlignMove->MixMode = EMoveMixMode::OverrideVelocity;
			// 슬롯을 지나쳐 미끄러지지 않도록 정지시킨다. 이 0이 위 주석의 함정을 만든 값이다.
			AlignMove->FinishVelocitySettings.FinishVelocityMode = ELayeredMoveFinishVelocityMode::SetVelocity;
			AlignMove->FinishVelocitySettings.SetVelocity = FVector::ZeroVector;
			Mover->QueueLayeredMove(AlignMove);
		}
	}

	// --- 정렬 ② 회전: 발사 방향을 바라보게 한다 ---
	if (ULNPInputHandlerComponent* InputHandler = Character->FindComponentByClass<ULNPInputHandlerComponent>())
	{
		const FVector Facing = Launcher->GetFacingDirection();
		if (!Facing.IsNearlyZero())
		{
			InputHandler->SetOrientationOverride(Facing);
		}
	}

	// 연출 — 몽타주가 없어도 정렬·발사는 그대로 동작한다.
	if (AlignMontage)
	{
		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
		{
			ASC->PlayMontage(this, ActivationInfo, AlignMontage, 1.f);
		}
	}

	UAbilityTask_WaitDelay* AlignTask = UAbilityTask_WaitDelay::WaitDelay(this, Duration);
	AlignTask->OnFinish.AddDynamic(this, &ULNPAbility_SpringLaunch::OnAlignFinished);
	AlignTask->ReadyForActivation();
}

void ULNPAbility_SpringLaunch::OnAlignFinished()
{
	// 여기서 사출하지 않는다 — 사출은 EndAbility가 한다. 사유는 EndAbility 주석 참조.
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void ULNPAbility_SpringLaunch::ExecuteLaunch()
{
	ALNPCharacterBase* Character = GetOwningCharacter();
	ULNPCharacterMoverComponent* Mover = Character ? Character->GetMoverComponent() : nullptr;
	const ALNPSpringLauncher* Launcher = PendingLauncher.Get();
	if (Mover == nullptr || Launcher == nullptr)
	{
		return;
	}

	// 서버와 소유 클라가 같은 액터 트랜스폼에서 파생하므로 값이 갈리지 않는다.
	// FLayeredMove_Launch는 한 번만 속도를 주고 이후를 구면 중력에 맡긴다 = 큰 포물선.
	const FVector LaunchVelocity = Launcher->GetLaunchVelocity();
	if (LaunchVelocity.IsNearlyZero())
	{
		// LaunchWithVelocity가 영벡터를 조용히 무시하므로 여기서 잡지 않으면 "정렬만 하고 안 날아감"이 된다.
		UE_LOG(LogLootNPop, Warning, TEXT("[SpringLaunch] %s zero launch velocity — device config missing?"),
			*GetNameSafe(Launcher));
		return;
	}
	Mover->LaunchWithVelocity(LaunchVelocity);

	UE_LOG(LogLootNPop, Log, TEXT("[SpringLaunch] %s launched by %s — velocity=%s (%.0f cm/s), authority=%d"),
		*GetNameSafe(Character), *GetNameSafe(Launcher), *LaunchVelocity.ToCompactString(),
		LaunchVelocity.Size(), Character->HasAuthority() ? 1 : 0);
}

/**
 * **사출은 여기서 한다 — 정렬 타이머 콜백이 아니다.**
 *
 * 이 어빌리티는 LocalPredicted라 소유 클라와 서버가 각자 자기 시계로 정렬 타이머를 돌린다. 그런데
 * 서버는 **자기 타이머와 소유 클라의 종료 통지 중 먼저 온 쪽으로 끝나고, 클라 통지가 먼저 오면
 * 서버 타이머 콜백은 영영 불리지 않는다.** 타이머 콜백에서 사출하면 그 경로에서 사출이 통째로 사라진다 —
 * 게스트는 예측으로 날아갔다가 권위 상태에 롤백되어 **"한순간 깜박이고 제자리"**가 된다.
 * 리슨 서버 호스트는 자기가 서버라 이 경쟁이 없어 멀쩡히 동작하므로 증상이 게스트에서만 나타난다.
 * (`ULNPAbility_Reload`가 같은 이유로 탄을 EndAbility에서 채운다 — 같은 실패 모드다.)
 *
 * EndAbility는 양쪽 모두에서 반드시 불리므로 어느 쪽 타이머가 이기든 사출이 한 번 일어난다.
 */
void ULNPAbility_SpringLaunch::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	if (!IsActive())
	{
		Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
		return;
	}

	if (const ALNPCharacterBase* Character = GetOwningCharacter())
	{
		if (ULNPInputHandlerComponent* InputHandler = Character->FindComponentByClass<ULNPInputHandlerComponent>())
		{
			InputHandler->ClearOrientationOverride();
		}
	}

	// 경직 등으로 취소됐으면 사출하지 않는다. 경과 시간 하한은 조기 종료 통지가 즉시 사출로 둔갑하는 것을 막는다.
	const double Elapsed = GetWorld() ? GetWorld()->GetTimeSeconds() - ActivationTime : 0.0;
	if (!bWasCancelled && AlignSeconds > 0.f && Elapsed >= AlignSeconds * MinCompletionRatio)
	{
		ExecuteLaunch();
	}

	AlignSeconds = 0.f;
	PendingLauncher.Reset();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
