// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Animation/AnimGraphNode_LNPRifleAim.h"

#define LOCTEXT_NAMESPACE "AnimGraphNode_LNPRifleAim"

FText UAnimGraphNode_LNPRifleAim::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return GetControllerDescription();
}

FText UAnimGraphNode_LNPRifleAim::GetTooltipText() const
{
	return LOCTEXT("Tooltip", "Rotates the pivot bone so the rifle barrel lines up with the actual fire direction.");
}

FText UAnimGraphNode_LNPRifleAim::GetControllerDescription() const
{
	return LOCTEXT("Title", "LNP Rifle Aim");
}

#undef LOCTEXT_NAMESPACE
