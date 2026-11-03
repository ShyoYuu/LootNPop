// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "AnimGraphNode_SkeletalControlBase.h"
#include "Animation/AnimNode_LNPRifleAim.h"
#include "AnimGraphNode_LNPRifleAim.generated.h"

/** FAnimNode_LNPRifleAim의 애님 그래프 노드. 플레이어 표시 ABP(ABP_Retarget)의 오른팔 견착 IK 뒤·왼손 IK 앞에 둔다. */
UCLASS()
class UAnimGraphNode_LNPRifleAim : public UAnimGraphNode_SkeletalControlBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Settings)
	FAnimNode_LNPRifleAim Node;

public:
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;

protected:
	virtual FText GetControllerDescription() const override;
	virtual const FAnimNode_SkeletalControlBase* GetNode() const override { return &Node; }
};
