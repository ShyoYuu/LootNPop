// Copyright (c) 2026 LootNPop. All rights reserved.

#include "GAS/LNPPoiseTypes.h"

#include "Character/LNPCharacterBase.h"
#include "Character/LNPInputHandlerComponent.h"
#include "GAS/Abilities/LNPAbility_Stagger.h"
#include "HitDetection/LNPGuardParryTypes.h"
#include "LNPGameplayTags.h"
#include "LootNPop.h"

#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "MassActorSubsystem.h"

FGameplayTag LNPPoise::GetStaggerEventTag(ELNPStaggerTier Tier)
{
	switch (Tier)
	{
	case ELNPStaggerTier::Down:   return TAG_GameplayEvent_Stagger_Heavy;
	case ELNPStaggerTier::Groggy: return TAG_GameplayEvent_Stagger_Light;
	default:                      return FGameplayTag();
	}
}

FGameplayTag LNPPoise::GetStaggerMontageValueTag(ELNPStaggerTier Tier)
{
	return (Tier == ELNPStaggerTier::Down) ? TAG_Montage_Value_Stagger_Heavy : TAG_Montage_Value_Stagger_Light;
}

namespace
{
	/** 경직 진입 시 기존 몽타주를 끊는 블렌드 아웃 시간 (초). */
	constexpr float MontageStopBlendTime = 0.1f;

	/**
	 * 실행 중인 경직 GA를 전부 끝낸다.
	 *
	 * 그로기 해제와 다운 진입 양쪽에 필요하다 — 특히 다운은 GA의
	 * `ActivationBlockedTags = {State.Staggered}`에 자기가 막히므로, 그로기 GA를 먼저 내려야만 뜬다.
	 * (취소 → EndAbility → ActivationOwnedTags 해제가 동기 처리되므로 같은 호출 안에서 재발동이 성립한다.)
	 */
	void CancelActiveStaggerAbilities(UAbilitySystemComponent& ASC)
	{
		TArray<FGameplayAbilitySpecHandle> ToCancel;
		for (const FGameplayAbilitySpec& Spec : ASC.GetActivatableAbilities())
		{
			if (Spec.Ability && Spec.Ability->IsA<ULNPAbility_Stagger>())
				ToCancel.Add(Spec.Handle);
		}

		for (const FGameplayAbilitySpecHandle& Handle : ToCancel)
			ASC.CancelAbilityHandle(Handle);
	}
}

void FLNPStaggerCommand::Run(FMassEntityManager& EntityManager)
{
	UWorld* World = EntityManager.GetWorld();
	UMassActorSubsystem* ActorSub = World ? World->GetSubsystem<UMassActorSubsystem>() : nullptr;
	if (!ActorSub)
		return;

	for (const FEntry& Entry : Entries)
	{
		if (!Entry.Entity.IsSet() || !EntityManager.IsEntityActive(Entry.Entity))
			continue;

		// Actor가 없는 Low LOD 적은 연출도 어빌리티도 없다 — 이동 정지는
		// ULNPEnemyMovementProcessor가 FLNPPoiseFragment::bIsGroggy를 보고 처리한다.
		ALNPCharacterBase* Victim = Cast<ALNPCharacterBase>(ActorSub->GetActorFromHandle(Entry.Entity));
		if (!IsValid(Victim))
			continue;

		UAbilitySystemComponent* ASC = Victim->GetAbilitySystemComponent();
		if (!IsValid(ASC))
			continue;

		// 랙돌 중(사망 연출·적 시체)에는 경직을 걸지 않는다 — 물리에 몽타주를 얹게 된다.
		// 해제는 랙돌 여부와 무관하게 처리해야 태그가 남지 않는다.
		if (Entry.Tier != ELNPStaggerTier::None && Victim->IsRagdollActive())
			continue;

		// 그로기 해제 — 경직도가 자연회복으로 T1 아래까지 내려왔다.
		if (Entry.Tier == ELNPStaggerTier::None)
		{
			CancelActiveStaggerAbilities(*ASC);
			continue;
		}

		// 진행 중인 공격을 끊는다. 몽타주만 덮어써서는 GAS 상태가 남아 콤보·쿨다운이 어긋난다.
		Victim->CancelCurrentAttackAbility();

		// 재생 중인 몽타주를 **명시적으로** 끊는다. 근접은 PlayMontageAndWait이 어빌리티 취소에 딸려
		// 멈추지만, 원거리(ULNPAbility_RangedAttack)는 몽타주를 Montage_Play로 흘려보내고 어빌리티가
		// 즉시 끝나므로 취소할 대상 자체가 없다 — 그대로 두면 굳은 채로 공격 모션이 이어진다.
		// 아래 GameplayCue가 경직 몽타주를 얹어 덮어쓰기는 하지만, Chooser 행이 없으면 그마저 안 된다.
		if (UAnimInstance* Anim = Victim->GetAnimInstance())
			Anim->Montage_Stop(MontageStopBlendTime);

		// 가드 브레이크 — 서버 판정용 미러를 먼저 내리고, 소유 클라의 눌림 상태도 함께 턴다.
		if (FLNPParryStateFragment* ParryState = EntityManager.GetFragmentDataPtr<FLNPParryStateFragment>(Entry.Entity))
		{
			ParryState->bIsGuarding           = false;
			ParryState->bIsParrying           = false;
			ParryState->ParryWindowExpiryTime = -1.0;
		}
		if (ULNPInputHandlerComponent* InputHandler = Victim->FindComponentByClass<ULNPInputHandlerComponent>())
		{
			InputHandler->Client_ForceReleaseGuard();
		}

		// 다운은 그로기 위에 덮어쓰는 전이다 — 먼저 그로기 GA를 내려야 차단 태그가 풀린다.
		if (Entry.Tier == ELNPStaggerTier::Down)
			CancelActiveStaggerAbilities(*ASC);

		// 입력 차단은 GA가 소유한다 — 어빌리티 수명이 곧 차단 구간이라 태그 페어가 깨질 수 없고,
		// 소유 클라에는 GAS 활성화 복제로 그대로 전달된다.
		FGameplayEventData EventData;
		EventData.Target = Victim;
		ASC->HandleGameplayEvent(LNPPoise::GetStaggerEventTag(Entry.Tier), &EventData);

		// 몽타주는 GameplayCue가 전 머신에 나른다 — 적 ASC는 Minimal 복제라
		// 어빌리티 활성화가 시뮬 프록시(게스트 화면)에 도달하지 않는다.
		// 티어가 정하는 기본 밸류 태그를 엔트리가 덮어쓸 수 있다 — 패링 그로기만 연출을 갈라 쓴다.
		const FGameplayTag MontageValueTag = Entry.MontageValueTag.IsValid()
			? Entry.MontageValueTag
			: LNPPoise::GetStaggerMontageValueTag(Entry.Tier);

		FGameplayCueParameters CueParams;
		CueParams.AggregatedSourceTags.AddTag(MontageValueTag);
		ASC->ExecuteGameplayCue(TAG_GameplayCue_Character_Stagger, CueParams);

		UE_LOG(LogLootNPop, Log, TEXT("[Poise] %s -> %s"),
			*GetNameSafe(Victim), (Entry.Tier == ELNPStaggerTier::Down) ? TEXT("DOWN") : TEXT("GROGGY"));
	}
}
