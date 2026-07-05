// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "GameplayTagContainer.h"
#include "Mass/EntityHandle.h"
#include "ANS_LNPMeleeHitWindow.generated.h"

/**
 * 근거리 공격 애니메이션의 피격 활성 구간을 정의하는 AnimNotifyState.
 *
 * NotifyBegin : Mass 엔티티 생성 + FLNPWeaponTraceFragment 초기화
 * NotifyTick  : 매 프레임 칼날 본(Bone) 위치를 Fragment에 기록 (Swept Volume 스윕 선분 생성)
 * NotifyEnd   : 엔티티 파괴
 */
UCLASS(meta = (DisplayName = "LNP Melee Hit Window"))
class LOOTNPOP_API UANS_LNPMeleeHitWindow : public UAnimNotifyState
{
	GENERATED_BODY()
public:
	/** 칼끝 본 이름. */
	UPROPERTY(EditAnywhere, Category = "LNP|Melee")
	FName BoneTipName = TEXT("sword_tip");

	/** 칼밑(손잡이 쪽) 본 이름. */
	UPROPERTY(EditAnywhere, Category = "LNP|Melee")
	FName BoneRootName = TEXT("sword_root");

	/** 0이면 무기 DataAsset의 HitRadius를 사용한다. */
	UPROPERTY(EditAnywhere, Category = "LNP|Melee", meta = (ClampMin = "0"))
	float HitRadiusOverride = 0.f;

	/** 넉백 강도를 제공할 어빌리티를 식별하는 태그 (LNP.Ability.HitEffect.Knockback).
	 *  비워두면 TAG_Ability_HitEffect_Knockback으로 자동 선택한다. */
	UPROPERTY(EditAnywhere, Category = "LNP|Melee")
	FGameplayTag KnockbackAbilityTag;

	/** 패링 판정 반경을 제공할 어빌리티를 식별하는 태그 (LNP.Ability.HitEffect.ParryRadius).
	 *  비워두면 TAG_Ability_HitEffect_Parry로 자동 선택한다. */
	UPROPERTY(EditAnywhere, Category = "LNP|Melee")
	FGameplayTag ParryAbilityTag;

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		float TotalDuration, const FAnimNotifyEventReference& EventReference) override;

	virtual void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		float FrameDeltaTime, const FAnimNotifyEventReference& EventReference) override;

	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override { return TEXT("LNP Melee Hit Window"); }

private:
	/**
	 * UAnimNotifyState는 이 노티파이를 배치한 몽타주를 재생하는 모든 AnimInstance(서버·클라이언트,
	 * 여러 캐릭터 포함)가 공유하는 단일 오브젝트다. 따라서 진행 중인 스윙 상태를 이 클래스의 평범한
	 * 멤버 변수로 저장하면 안 되며, 반드시 호출마다 전달되는 MeshComp로 구분해야 한다.
	 */
	struct FActiveSwing
	{
		FMassEntityHandle Entity;
	};
	TMap<TWeakObjectPtr<USkeletalMeshComponent>, FActiveSwing> ActiveSwings;
};
