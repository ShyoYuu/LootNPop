// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "ANS_LNPAttackInputBlock.generated.h"

/**
 * 공격 입력 차단 구간을 정의하는 AnimNotifyState.
 *
 * NotifyBegin : ASC에 TAG_Block_AttackInput 추가 — 공격 선딜 중 연타 입력이 새 어빌리티를 발동하지 못하게 차단
 * NotifyEnd   : TAG_Block_AttackInput 제거
 *
 * 콤보 연결 자체는 ANS_LNPComboWindow가 여는 TAG_State_ComboWindow 구간에서
 * ALNPCharacterBase::TryActivateAttack()이 태그를 소비하며 처리한다.
 */
UCLASS(meta = (DisplayName = "LNP Block Attack Input"))
class LOOTNPOP_API UANS_LNPAttackInputBlock : public UAnimNotifyState
{
	GENERATED_BODY()
public:
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		float TotalDuration, const FAnimNotifyEventReference& EventReference) override;

	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override { return TEXT("Block Attack Input"); }
};
