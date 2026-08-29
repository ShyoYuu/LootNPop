// Copyright (c) 2026 LootNPop. All rights reserved.

#include "GAS/GameplayCues/LNPGameplayCueNotify_Stagger.h"
#include "Character/LNPCharacterBase.h"
#include "LNPGameplayTags.h"
#include "LootNPop.h"

bool ULNPGameplayCueNotify_Stagger::OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	ALNPCharacterBase* VictimChar = Cast<ALNPCharacterBase>(MyTarget);
	if (!VictimChar)
	{
		UE_LOG(LogLootNPop, Warning, TEXT("[Poise] Stagger cue target is not ALNPCharacterBase: %s"), *GetNameSafe(MyTarget));
		return true;
	}

	// 어느 밸류 태그로 왔는지는 FLNPStaggerCommand가 정한다 — 여기서는 실려 온 것을 그대로 쓰고,
	// 아무것도 없으면 가장 흔한 Light로 폴백한다.
	//
	// AggregatedSourceTags는 FGameplayCueParameters::NetSerialize가 RepBits 마스크 **바깥에서 무조건**
	// 직렬화하므로 원격 클라이언트에도 그대로 도착한다 (빈 컨테이너가 1비트라 마스크 비트를 두지 않은 것).
	FGameplayTag ValueTag = TAG_Montage_Value_Stagger_Light;
	for (const FGameplayTag& Candidate : { FGameplayTag(TAG_Montage_Value_Stagger_Heavy),
	                                       FGameplayTag(TAG_Montage_Value_Stagger_Parried) })
	{
		if (Parameters.AggregatedSourceTags.HasTagExact(Candidate))
		{
			ValueTag = Candidate;
			break;
		}
	}

	VictimChar->PlayMontage(TAG_Montage_Situation_Stagger, ValueTag);
	return true;
}
