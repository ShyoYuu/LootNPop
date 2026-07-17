// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Enemy/LNPEnemyMassTypes.h"
#include "Enemy/LNPEnemyConfig.h"
#include "Replication/LNPMassReplication.h"
#include "Replication/LNPMassReplicator.h"
#include "HitDetection/LNPPositionHistoryFragment.h"

#include "MassEntityTemplateRegistry.h"
#include "MassCommonFragments.h"
#include "MassRepresentationFragments.h"
#include "MassActorSubsystem.h"
#include "MassMovementFragments.h"
#include "MassNavigationFragments.h"
#include "MassDistanceLODProcessor.h"
#include "MassStateTreeFragments.h"
#include "MassReplicationTrait.h"
#include "StateTree.h"


ULNPEnemyTrait::ULNPEnemyTrait()
{
	ReplicationTrait = CreateDefaultSubobject<UMassReplicationTrait>(TEXT("ReplicationTrait"));
	ReplicationTrait->Params.BubbleInfoClass = ALNPMassClientBubbleInfo::StaticClass();
	ReplicationTrait->Params.ReplicatorClass = ULNPMassReplicator::StaticClass();
}

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
	BuildContext.AddFragment<FLNPPositionHistoryFragment>(); // Lag Compensation용 위치 히스토리 (서버 전용 기록)

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

	// 5. Enemy MassReplication (Phase 6) — NM_Standalone이면 UMassReplicationTrait::BuildTemplate이 자체적으로 조기 반환한다.
	ReplicationTrait->BuildTemplate(BuildContext, World);
}
