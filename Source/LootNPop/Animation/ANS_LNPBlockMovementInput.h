// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "ANS_LNPBlockMovementInput.generated.h"

/**
 * 이동 입력 잠금 구간을 정의하는 AnimNotifyState.
 *
 * NotifyBegin : ASC에 TAG_Block_MovementInput 추가
 * NotifyEnd   : ASC에서 TAG_Block_MovementInput 제거
 */
UCLASS(meta = (DisplayName = "LNP Block Movement Input"))
class LOOTNPOP_API UANS_LNPBlockMovementInput : public UAnimNotifyState
{
	GENERATED_BODY()
public:
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		float TotalDuration, const FAnimNotifyEventReference& EventReference) override;

	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override { return TEXT("Block Movement Input"); }
};
