// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "ANS_LNPComboWindow.generated.h"

/**
 * 콤보 입력 유효 구간을 정의하는 AnimNotifyState.
 *
 * NotifyBegin : ASC에 TAG_State_ComboWindow 추가
 *               이 태그가 살아 있는 동안 공격 입력이 들어오면 ALNPCharacterBase::TryActivateAttack()이 태그를
 *               즉시 소비하고, 현재 공격 어빌리티를 취소한 뒤 다음 콤보 섹션으로 재발동한다.
 * NotifyEnd   : ASC에서 TAG_State_ComboWindow 제거
 */
UCLASS(meta = (DisplayName = "LNP Combo Window"))
class LOOTNPOP_API UANS_LNPComboWindow : public UAnimNotifyState
{
	GENERATED_BODY()
public:
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		float TotalDuration, const FAnimNotifyEventReference& EventReference) override;

	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override { return TEXT("Combo Window"); }
};
