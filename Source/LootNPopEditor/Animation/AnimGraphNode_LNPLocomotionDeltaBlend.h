// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "AnimGraphNode_Base.h"
#include "Animation/AnimNode_LNPLocomotionDeltaBlend.h"
#include "AnimGraphNode_LNPLocomotionDeltaBlend.generated.h"

/** FAnimNode_LNPLocomotionDeltaBlend의 애님 그래프 노드. 무기 서브 ABP의 ApplyWeaponStyleOverlay에서 Layered Blend per Bone 자리에 둔다. */
UCLASS()
class UAnimGraphNode_LNPLocomotionDeltaBlend : public UAnimGraphNode_Base
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Settings)
	FAnimNode_LNPLocomotionDeltaBlend Node;

public:
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual FString GetNodeCategory() const override;
};
