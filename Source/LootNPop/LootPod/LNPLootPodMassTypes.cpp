// Copyright (c) 2026 LootNPop. All rights reserved.

#include "LootPod/LNPLootPodMassTypes.h"
#include "Replication/LNPMassReplication.h"
#include "Replication/LNPMassReplicator.h"

#include "MassEntityTemplateRegistry.h"
#include "MassCommonFragments.h"
#include "MassActorSubsystem.h"
#include "MassReplicationTrait.h"

ULNPLootPodTrait::ULNPLootPodTrait()
{
	ReplicationTrait = CreateDefaultSubobject<UMassReplicationTrait>(TEXT("ReplicationTrait"));
	ReplicationTrait->Params.BubbleInfoClass = ALNPMassClientBubbleInfo::StaticClass();
	ReplicationTrait->Params.ReplicatorClass = ULNPMassReplicator::StaticClass();
}

void ULNPLootPodTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	// 1. 데이터 Fragment 추가 — 루팅 존 반경·게이지 총량은 config(트레잇 프로퍼티)에서 주입
	FLNPLootPodFragment& PodFragment = BuildContext.AddFragment_GetRef<FLNPLootPodFragment>();
	PodFragment.LootableDistSquared = FMath::Square(LootingZoneRadius);
	PodFragment.MaxGauge = MaxGauge;

	// 2. 식별 Tag 추가
	BuildContext.AddTag<FLNPLootPodTag>();
	BuildContext.AddTag<FLNPLootPodIdleTag>(); // Idle 상태로 시작

	// 3. Transform Fragment 추가 (Mass Representation 시스템에서 사용)
	BuildContext.AddFragment<FTransformFragment>();

	BuildContext.AddFragment<FMassActorFragment>();

	// 4. LootPod MassReplication (Phase 7) — NM_Standalone이면 UMassReplicationTrait::BuildTemplate이 자체적으로 조기 반환한다.
	ReplicationTrait->BuildTemplate(BuildContext, World);
}