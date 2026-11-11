// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Animation/AnimGraphNode_LNPLocomotionDeltaBlend.h"

#define LOCTEXT_NAMESPACE "AnimGraphNode_LNPLocomotionDeltaBlend"

FText UAnimGraphNode_LNPLocomotionDeltaBlend::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("Title", "LNP Locomotion Delta Blend");
}

FText UAnimGraphNode_LNPLocomotionDeltaBlend::GetTooltipText() const
{
	return LOCTEXT("Tooltip", "Layered per-bone blend that adds the locomotion delta (locomotion minus a neutral reference pose) to the overlay pose before blending, so the upper body keeps the weapon pose but still reacts to landings and steps.");
}

FString UAnimGraphNode_LNPLocomotionDeltaBlend::GetNodeCategory() const
{
	return TEXT("LNP");
}

#undef LOCTEXT_NAMESPACE
