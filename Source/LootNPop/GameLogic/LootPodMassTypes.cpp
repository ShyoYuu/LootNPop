// Fill out your copyright notice in the Description page of Project Settings.

#include "GameLogic/LootPodMassTypes.h"
#include "MassEntityTemplateRegistry.h"
#include "MassCommonFragments.h"

void ULNPLootPodTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	// 1. Add Data Fragments
	BuildContext.AddFragment<FLNPLootPodFragment>();

	// 2. Add Identification Tags
	BuildContext.AddTag<FLNPLootPodTag>();
	BuildContext.AddTag<FLNPLootPodIdleTag>(); // Start in Idle state

	// 3. Add Transform Fragment (Used by Mass Representation system)
	BuildContext.AddFragment<FTransformFragment>();
}