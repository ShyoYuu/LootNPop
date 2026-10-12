// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Config/LNPSettings.h"
#include "DataAsset/LNPMassSpawnConfig.h"
#include "DataAsset/LNPOctantPoolData.h"
#include "DataAsset/LNPOctantSurfaceData.h"
#include "Enemy/LNPEnemyConfig.h"
#include "Enemy/LNPEnemyMassTypes.h"
#include "MassEntityConfigAsset.h"
#include "GameLogic/LNPOctantSpawnSubsystem.h"
#include "SurfaceNavigation/LNPMassSpawnPlan.h"
#include "SurfaceNavigation/LNPSpawnData.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLNPProductionSpawnCompositionTest,
	"LootNPop.SurfaceNavigation.Runtime.ProductionSpawnComposition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
	FString DefinitionName(const FLNPOctantDefinition& Definition)
	{
		return Definition.LevelAsset.ToSoftObjectPath().GetAssetName();
	}
}

/**
 * 실제 풀 배치와 정식 DA_MassSpawnConfig로 Spawn 계획을 세워 모든 Pod의 적 편성이 전량 배치되는지 검사한다.
 * 부족한 Pod는 slot·definition·Layer·배치 출처·적 종류·적격 후보 수로 분류해 원인을 남긴다.
 */
bool FLNPProductionSpawnCompositionTest::RunTest(const FString& Parameters)
{
	const ULNPSettings* Settings = GetDefault<ULNPSettings>();
	ULNPOctantPoolData* Pool = Settings ? Settings->OctantPool.LoadSynchronous() : nullptr;
	ULNPMassSpawnConfig* Config = LoadObject<ULNPMassSpawnConfig>(nullptr,
		TEXT("/Game/SpawnData/DA_MassSpawnConfig.DA_MassSpawnConfig"));
	if (!TestNotNull(TEXT("Production pool loads"), Pool) || !TestNotNull(TEXT("Spawn config loads"), Config)) return false;

	TArray<FLNPOctantDefinition> EffectiveDefinitions;
	Pool->BuildEffectiveDefinitions(EffectiveDefinitions);

	TArray<FLNPMassSpawnSetPlanInput> Sets;
	TArray<FString> AssetNames;
	for (const auto& Entry : Config->LootPodSpawnSets)
	{
		auto& Set = Sets.AddDefaulted_GetRef();
		Set.SpawnSetId = Entry.SpawnSetId;
		Set.RequestedPods = Entry.PodSetCount;
		Set.PodAssetIndex = AssetNames.Add(GetNameSafe(Entry.LootPodEntityConfig));
		for (const auto& Enemy : Entry.AssociatedEnemies)
		{
			const ULNPEnemyTrait* Trait = nullptr;
			for (const UMassEntityConfigAsset* Asset = Enemy.EnemyEntityConfig; Asset && !Trait; Asset = Asset->GetConfig().GetParent())
			{
				Trait = Cast<ULNPEnemyTrait>(Asset->FindTrait(ULNPEnemyTrait::StaticClass()));
			}
			const float Required = Enemy.Count > 0 && Trait && Trait->EnemyConfig ? Trait->EnemyConfig->GetSpawnRequiredHeadroom() : 0.0f;
			Set.Enemies.Add({Enemy.Count, AssetNames.Add(GetNameSafe(Enemy.EnemyEntityConfig)), Required, FName(*Enemy.EnemyEntityConfig->GetPathName())});
			Set.RequiredHeadroom = FMath::Max(Set.RequiredHeadroom, Required);
		}
	}

	// 실제 패키지1P(V008)의 배치: Meadow 0/3, Bastion 1/5/7, Hollow 2/4/6, WorldSeed 1.
	struct FArrangement
	{
		FString Label;
		int32 WorldSeed = 0;
		TArray<FLNPOctantDefinition> Definitions;
	};
	TArray<FArrangement> Arrangements;
	{
		FArrangement& Package = Arrangements.AddDefaulted_GetRef();
		Package.Label = TEXT("Package1P");
		Package.WorldSeed = 1;
		const TCHAR* Order[] = {TEXT("Meadow"), TEXT("Bastion"), TEXT("Hollow"), TEXT("Meadow"), TEXT("Hollow"), TEXT("Bastion"), TEXT("Hollow"), TEXT("Bastion")};
		for (const TCHAR* Name : Order)
		{
			const FLNPOctantDefinition* Match = EffectiveDefinitions.FindByPredicate([Name](const FLNPOctantDefinition& Definition)
			{
				return DefinitionName(Definition).Contains(Name);
			});
			if (!TestNotNull(FString::Printf(TEXT("Pool has %s definition"), Name), Match)) return false;
			Package.Definitions.Add(*Match);
		}
	}
	TArray<int32> Seeds = {0, -1, MIN_int32, MAX_int32, 135792468};
	FRandomStream SeedStream(9042026);
	while (Seeds.Num() < 32) { Seeds.AddUnique(SeedStream.GetCurrentSeed()); SeedStream.GetUnsignedInt(); }
	for (const int32 Seed : Seeds)
	{
		FArrangement& Arrangement = Arrangements.AddDefaulted_GetRef();
		Arrangement.Label = FString::Printf(TEXT("Seed %d"), Seed);
		Arrangement.WorldSeed = Seed;
		FString Error;
		if (!ULNPOctantSpawnSubsystem::SelectOctantDefinitions(EffectiveDefinitions, Seed, Arrangement.Definitions, nullptr, &Error))
		{
			AddError(Arrangement.Label + TEXT(": ") + Error);
			return false;
		}
	}

	const double RadiusSquared = FMath::Square(static_cast<double>(Config->EnemySpawnRadiusAroundPod));
	int32 TotalRequested = 0, TotalPlaced = 0, ShortArrangements = 0;
	for (int32 ArrangementIndex = 0; ArrangementIndex < Arrangements.Num(); ++ArrangementIndex)
	{
		const FArrangement& Arrangement = Arrangements[ArrangementIndex];
		TArray<ULNPOctantSurfaceData*> LoadedData;
		for (const FLNPOctantDefinition& Definition : Arrangement.Definitions)
		{
			LoadedData.Add(Definition.SurfaceData.LoadSynchronous());
		}
		FLNPSurfaceDataSnapshot Snapshot;
		FLNPMassSpawnPlan Plan;
		FString Error;
		if (!LNPSurfaceDataLoading::ValidateAndBuildSnapshot(Arrangement.Definitions, LoadedData, ArrangementIndex + 1, Snapshot, Error)
			|| !LNPMassSpawnPlanning::BuildPlan(Snapshot, Sets, Arrangement.WorldSeed, Config->MinDistanceBetweenPods,
				Config->EnemySpawnRadiusAroundPod, Plan, Error))
		{
			AddError(Arrangement.Label + TEXT(": ") + Error);
			return false;
		}

		int32 Requested = 0, Placed = 0, PodShortfall = 0;
		for (const FLNPMassSpawnSetStats& Stats : Plan.SetStats)
		{
			Requested += Stats.EnemyRequested;
			Placed += Stats.EnemyPlaced;
			PodShortfall += Stats.Shortfall;
		}
		TotalRequested += Requested;
		TotalPlaced += Placed;
		ShortArrangements += Placed < Requested ? 1 : 0;
		TestEqual(Arrangement.Label + TEXT(": every Pod is placed"), PodShortfall, 0);
		TestEqual(Arrangement.Label + TEXT(": every requested enemy is placed"), Placed, Requested);
		if (Placed == Requested && ArrangementIndex > 0)
		{
			continue;
		}

		AddInfo(FString::Printf(TEXT("%s: enemies placed=%d/%d"), *Arrangement.Label, Placed, Requested));
		for (const FLNPMassSpawnPlannedPod& Pod : Plan.Pods)
		{
			const FLNPMassSpawnSetPlanInput& Set = Sets[Pod.SetInputIndex];
			const TArray<FLNPMassSpawnEnemyPlanInput>& Inputs = Pod.AuthoredEnemies.IsEmpty() ? Set.Enemies : Pod.AuthoredEnemies;
			const int32 Slot = Pod.Surface.OctantSlot;
			const FLNPSpawnData& Spawn = *Snapshot.Slots[Slot].Spawn;
			const FQuat4d SlotRotation = Snapshot.Slots[Slot].SlotRotation;
			for (const FLNPMassSpawnEnemyPlanInput& Input : Inputs)
			{
				int32 GroupPlaced = 0;
				for (const FLNPMassSpawnPlannedEnemyGroup& Group : Pod.Enemies)
				{
					GroupPlaced += Group.AssetIndex == Input.AssetIndex ? Group.Transforms.Num() : 0;
				}
				if (GroupPlaced >= Input.RequestedCount)
				{
					continue;
				}
				// 같은 slot·Layer에서 반경 안 Enemy 후보, 그중 headroom 충족 후보 수. 다른 Pod 선점·200cm 간격은 세지 않는다.
				int32 InRadius = 0, HeadroomOk = 0, SameSlotLayer = 0;
				for (const FLNPSpawnRandomCandidate& Candidate : Spawn.RandomCandidates)
				{
					if (!EnumHasAnyFlags(Candidate.Allowed, ELNPSpawnCandidateFlags::Enemy) || Candidate.LocalLayerId != Pod.Surface.LocalLayerId)
					{
						continue;
					}
					++SameSlotLayer;
					const FVector Position(SlotRotation.RotateVector(FVector3d(Candidate.LocalPosition.X, Candidate.LocalPosition.Y, Candidate.LocalPosition.Z)));
					if (FVector::DistSquared(Position, Pod.Transform.GetLocation()) > RadiusSquared)
					{
						continue;
					}
					++InRadius;
					HeadroomOk += Candidate.Headroom >= Input.RequiredHeadroom ? 1 : 0;
				}
				AddInfo(FString::Printf(
					TEXT("%s short: set=%s slot=%d def=%s layer=%u source=%d enemy=%s placed=%d/%d requiredHeadroom=%.0f layerCandidates=%d inRadius=%d headroomOk=%d pod=%s"),
					*Arrangement.Label, *Set.SpawnSetId.ToString(), Slot, *DefinitionName(Arrangement.Definitions[Slot]),
					Pod.Surface.LocalLayerId, static_cast<int32>(Pod.PlacementSource), *AssetNames[Input.AssetIndex],
					GroupPlaced, Input.RequestedCount, Input.RequiredHeadroom, SameSlotLayer, InRadius, HeadroomOk,
					*Pod.Transform.GetLocation().ToCompactString()));
			}
		}
	}
	AddInfo(FString::Printf(TEXT("Production spawn composition: arrangements=%d shortArrangements=%d enemies=%d/%d"),
		Arrangements.Num(), ShortArrangements, TotalPlaced, TotalRequested));
	return !HasAnyErrors();
}

#endif
