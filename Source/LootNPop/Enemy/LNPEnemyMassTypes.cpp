// Fill out your copyright notice in the Description page of Project Settings.

#include "Enemy/LNPEnemyMassTypes.h"
#include "Enemy/LNPEnemyConfig.h"
#include "MassEntityTemplateRegistry.h"
#include "MassCommonFragments.h"
#include "MassRepresentationFragments.h"
#include "MassActorSubsystem.h"
#include "MassMovementFragments.h"
#include "MassNavigationFragments.h"
#include "MassDistanceLODProcessor.h"
#include "MassStateTreeFragments.h"
#include "StateTree.h"


void ULNPEnemyTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	// 1. Core Data Fragments
	BuildContext.AddFragment<FLNPEnemyFragment>();
	BuildContext.AddFragment<FLNPEnemyIdleFragment>();
	BuildContext.AddFragment<FLNPEnemyTargetingFragment>();
	BuildContext.AddFragment<FLNPEnemyTargetingCandidateFragment>();
	BuildContext.AddFragment<FMassMoveTargetFragment>();
	//BuildContext.AddFragment<FMassVelocityFragment>();

	// 2. Shared Config Fragments
	if (EnemyConfig != nullptr)
	{
		FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);
		
		// Enemy Config Fragment
		FLNPEnemySharedFragment EnemySharedFragment;
		EnemySharedFragment.Config = EnemyConfig;
		FConstSharedStruct EnemySharedStruct = EntityManager.GetOrCreateConstSharedFragment(EnemySharedFragment);
		BuildContext.AddConstSharedFragment(EnemySharedStruct);
	}

	// 3. Identification Tags
	BuildContext.AddTag<FLNPEnemyTag>();

	// 4. Required Mass System Fragments
	BuildContext.AddFragment<FMassActorFragment>();
}
