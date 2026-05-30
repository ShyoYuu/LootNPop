// Fill out your copyright notice in the Description page of Project Settings.

#include "LootPod/LNPLootPodMassTypes.h"

#include "MassEntityTemplateRegistry.h"
#include "MassCommonFragments.h"
#include "MassActorSubsystem.h"

void ULNPLootPodTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	// 1. 데이터 Fragment 추가
	BuildContext.AddFragment<FLNPLootPodFragment>();

	// 2. 식별 Tag 추가
	BuildContext.AddTag<FLNPLootPodTag>();
	BuildContext.AddTag<FLNPLootPodIdleTag>(); // Idle 상태로 시작

	// 3. Transform Fragment 추가 (Mass Representation 시스템에서 사용)
	BuildContext.AddFragment<FTransformFragment>();

	BuildContext.AddFragment<FMassActorFragment>();
}