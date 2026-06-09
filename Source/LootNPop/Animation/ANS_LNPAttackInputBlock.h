// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "ANS_LNPAttackInputBlock.generated.h"

/**
 * 공격 입력 차단 구간을 정의하는 AnimNotifyState.
 *
 * NotifyBegin : ASC에 TAG_Block_AttackInput 추가
 * NotifyEnd   : TAG_Block_AttackInput 제거 후 콤보 버퍼 소비
 *               - 버퍼 있음 → IncrementComboIndex + TryActivateAttack (연속 공격)
 *               - 버퍼 없음 → ResetCombo (콤보 종료)
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
