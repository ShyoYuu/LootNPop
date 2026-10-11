// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "DataAsset/LNPMassSpawnConfig.h"
#include "DataAsset/LNPOctantSurfaceData.h"
#include "Enemy/LNPEnemyConfig.h"
#include "Enemy/LNPEnemyMassTypes.h"
#include "MassEntityConfigAsset.h"
#include "SurfaceNavigation/LNPMassSpawnPlan.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLNPBastionEncounterTest,
	"LootNPop.SurfaceNavigation.Bake.BastionEncounters",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLNPBastionEncounterTest::RunTest(const FString& Parameters)
{
	ULNPOctantSurfaceData* Data = LoadObject<ULNPOctantSurfaceData>(nullptr,
		TEXT("/Game/Maps/Bastion_00/DA_OctantSurface_Bastion_00.DA_OctantSurface_Bastion_00"));
	ULNPMassSpawnConfig* Config = LoadObject<ULNPMassSpawnConfig>(nullptr,
		TEXT("/Game/SpawnData/DA_MassSpawnConfig.DA_MassSpawnConfig"));
	if (!TestNotNull(TEXT("Bastion bake loads"), Data) || !TestNotNull(TEXT("Spawn config loads"), Config)) return false;
	ULNPOctantSurfaceData* Meadow = LoadObject<ULNPOctantSurfaceData>(nullptr,
		TEXT("/Game/Maps/Meadow_00/DA_OctantSurface_Meadow_00.DA_OctantSurface_Meadow_00"));
	FLNPSpawnData Legacy;
	FString LegacyError;
	if (!TestNotNull(TEXT("Existing Meadow bake loads"), Meadow)
		|| !TestTrue(TEXT("Existing v2 Spawn stream remains readable"), LNPSpawnData::Decode(Meadow->SpawnPayload, Legacy, LegacyError))) return false;
	for (const auto& Anchor : Legacy.AuthoredAnchors) TestTrue(TEXT("Existing map has no encounter override"), Anchor.EnemyFormations.IsEmpty());

	FLNPSurfaceDataSnapshot Snapshot;
	Snapshot.Generation = 1;
	Snapshot.Slots.SetNum(8);
	FString Error;
	for (int32 Slot = 0; Slot < 8; ++Slot)
	{
		auto Spawn = MakeShared<FLNPSpawnData, ESPMode::ThreadSafe>();
		if (Slot == 0 && !LNPSpawnData::Decode(Data->SpawnPayload, *Spawn, Error)) { AddError(Error); return false; }
		Snapshot.Slots[Slot].Spawn = Spawn;
	}
	TArray<FLNPMassSpawnSetPlanInput> Sets;
	int32 AssetIndex = 0;
	for (const auto& Entry : Config->LootPodSpawnSets)
	{
		auto& Set = Sets.AddDefaulted_GetRef();
		Set.SpawnSetId = Entry.SpawnSetId;
		Set.RequestedPods = Entry.PodSetCount;
		Set.PodAssetIndex = AssetIndex++;
		for (const auto& Enemy : Entry.AssociatedEnemies)
		{
			const ULNPEnemyTrait* Trait = nullptr;
			for (const UMassEntityConfigAsset* Asset = Enemy.EnemyEntityConfig; Asset && !Trait; Asset = Asset->GetConfig().GetParent())
			{
				Trait = Cast<ULNPEnemyTrait>(Asset->FindTrait(ULNPEnemyTrait::StaticClass()));
			}
			const float Required = Trait && Trait->EnemyConfig ? Trait->EnemyConfig->GetSpawnRequiredHeadroom() : 0.0f;
			Set.Enemies.Add({Enemy.Count, AssetIndex++, Required, FName(*Enemy.EnemyEntityConfig->GetPathName())});
			Set.RequiredHeadroom = FMath::Max(Set.RequiredHeadroom, Required);
		}
	}
	TestEqual(TEXT("Bastion has twelve authored encounters"), Snapshot.Slots[0].Spawn->AuthoredAnchors.Num(), 12);
	// 여러 seed에서 랜덤 Pod와 후보 선점까지 포함한 실제 편성을 검사한다.
	for (int32 Seed = 0; Seed < 16; ++Seed)
	{
		FLNPMassSpawnPlan Plan;
		if (!LNPMassSpawnPlanning::BuildPlan(Snapshot, Sets, Seed, Config->MinDistanceBetweenPods,
			Config->EnemySpawnRadiusAroundPod, Plan, Error)) { AddError(Error); return false; }
		int32 Encounters = 0;
		for (const auto& Pod : Plan.Pods)
		{
			if (Pod.AuthoredEnemies.IsEmpty()) continue;
			++Encounters;
			int32 Requested = 0, Placed = 0;
			for (const auto& Enemy : Pod.AuthoredEnemies) Requested += Enemy.RequestedCount;
			for (const auto& Group : Pod.Enemies)
			{
				Placed += Group.Transforms.Num();
				for (const FTransform& Transform : Group.Transforms)
				{
					TestTrue(TEXT("Encounter leaves four meters around Pod"), FVector::Distance(Transform.GetLocation(), Pod.Transform.GetLocation()) >= 400.0);
					TestTrue(TEXT("Spawn stays inside its authored approach region"), Pod.AuthoredEnemies.ContainsByPredicate([&](const auto& Enemy)
					{
						return Enemy.AssetIndex == Group.AssetIndex && FVector::Distance(Transform.GetLocation(),
							Pod.Transform.TransformPosition(FVector(Enemy.Offset.X, Enemy.Offset.Y, 0.0))) <= Enemy.FormationRadius;
					}));
				}
			}
			if (Seed == 0) AddInfo(FString::Printf(TEXT("Bastion Pod=%s requested=%d placed=%d"), *Pod.Transform.GetLocation().ToString(), Requested, Placed));
			TestEqual(FString::Printf(TEXT("Seed %d encounter %s retains its complete composition"), Seed, *Pod.Transform.GetLocation().ToString()), Placed, Requested);
		}
		TestEqual(TEXT("Every authored Pod uses its encounter, including the cave"), Encounters, 12);
	}
	return !HasAnyErrors();
}

#endif
