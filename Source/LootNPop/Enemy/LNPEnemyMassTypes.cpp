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
	// 1. 핵심 데이터 Fragment
	BuildContext.AddFragment<FLNPEnemyFragment>();
	BuildContext.AddFragment<FLNPEnemyIdleFragment>();
	BuildContext.AddFragment<FLNPEnemyTargetingFragment>();
	BuildContext.AddFragment<FLNPEnemyTargetingCandidateFragment>();
	BuildContext.AddFragment<FMassMoveTargetFragment>();
	BuildContext.AddFragment<FLNPEnemyVelocityFragment>();
	//BuildContext.AddFragment<FMassVelocityFragment>();

	// 2. Shared Config Fragment
	if (EnemyConfig != nullptr)
	{
		FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);
		
		// Enemy Config Fragment (Enemy 설정)
		FLNPEnemySharedFragment EnemySharedFragment;
		EnemySharedFragment.Config = EnemyConfig;
		FConstSharedStruct EnemySharedStruct = EntityManager.GetOrCreateConstSharedFragment(EnemySharedFragment);
		BuildContext.AddConstSharedFragment(EnemySharedStruct);
	}

	// 3. 식별 Tag
	BuildContext.AddTag<FLNPEnemyTag>();

	// 4. 필수 Mass 시스템 Fragment
	BuildContext.AddFragment<FMassActorFragment>();
}
