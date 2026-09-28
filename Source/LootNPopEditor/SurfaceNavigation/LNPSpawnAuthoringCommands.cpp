// Copyright (c) 2026 LootNPop. All rights reserved.

// Phase 5 Spawn stream의 일회성 production config 마이그레이션과 회귀 LVI 앵커 배치 명령.

#include "Config/LNPSettings.h"
#include "DataAsset/LNPMassSpawnConfig.h"
#include "Editor.h"
#include "FileHelpers.h"
#include "HAL/IConsoleManager.h"
#include "MassEntityConfigAsset.h"
#include "Misc/PackageName.h"
#include "SurfaceNavigation/LNPCaveKit.h"
#include "SurfaceNavigation/LNPMassSpawnPoint.h"
#include "SurfaceNavigation/LNPRegressionFixture.h"
#include "UObject/SavePackage.h"

DEFINE_LOG_CATEGORY_STATIC(LogLNPSpawnAuthoring, Log, All);

namespace
{
	bool SaveAsset(UObject& Asset)
	{
		UPackage* Package = Asset.GetOutermost();
		Package->MarkPackageDirty();
		const FString Filename = FPackageName::LongPackageNameToFilename(
			Package->GetName(), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		return UPackage::SavePackage(Package, &Asset, *Filename, Args);
	}

	void MigrateMassSpawnConfig()
	{
		const ULNPSettings* Settings = GetDefault<ULNPSettings>();
		ULNPMassSpawnConfig* Config = Settings ? Settings->MassSpawnConfig.LoadSynchronous() : nullptr;
		if (!Config)
		{
			UE_LOG(LogLNPSpawnAuthoring, Error, TEXT("[SpawnAuthoring] LNPSettings has no loadable MassSpawnConfig."));
			return;
		}

		Config->Modify();
		TSet<FName> Used;
		for (int32 Index = 0; Index < Config->LootPodSpawnSets.Num(); ++Index)
		{
			FLNPLootPodSpawnEntry& Entry = Config->LootPodSpawnSets[Index];
			if (!Entry.SpawnSetId.IsNone() && !Used.Contains(Entry.SpawnSetId))
			{
				Used.Add(Entry.SpawnSetId);
				continue;
			}

			FString Base = Entry.LootPodEntityConfig ? Entry.LootPodEntityConfig->GetName() : TEXT("SpawnSet");
			Base.RemoveFromStart(TEXT("DA_LootPodEntityConfig_"));
			Base.RemoveFromStart(TEXT("DA_"));
			FName Candidate(*Base);
			for (int32 Suffix = 2; Candidate.IsNone() || Used.Contains(Candidate); ++Suffix)
			{
				Candidate = FName(*FString::Printf(TEXT("%s_%d"), *Base, Suffix));
			}
			Entry.SpawnSetId = Candidate;
			Used.Add(Candidate);
			UE_LOG(LogLNPSpawnAuthoring, Display, TEXT("[SpawnAuthoring] LootPodSpawnSets[%d] -> SpawnSetId '%s'."),
				Index, *Candidate.ToString());
		}
		UE_LOG(LogLNPSpawnAuthoring, Display, TEXT("[SpawnAuthoring] MassSpawnConfig %s."),
			SaveAsset(*Config) ? TEXT("saved") : TEXT("FAILED to save"));
	}

	ALNPMassSpawnPoint* SpawnAnchor(UWorld& World, const TCHAR* Label, const FTransform& Transform, FName TargetSpawnSetId = NAME_None)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ALNPMassSpawnPoint* Point = World.SpawnActor<ALNPMassSpawnPoint>(ALNPMassSpawnPoint::StaticClass(), Transform, Params);
		if (Point)
		{
			Point->SetActorLabel(Label);
			Point->TargetSpawnSetId = TargetSpawnSetId;
		}
		return Point;
	}

	void PlaceRegressionAnchors()
	{
		const FString PackageName = FPackageName::ObjectPathToPackageName(FString(LNPRegressionFixture::LevelPath));
		const FString Filename = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetMapPackageExtension());
		if (!FEditorFileUtils::LoadMap(Filename, false, false))
		{
			UE_LOG(LogLNPSpawnAuthoring, Error, TEXT("[SpawnAuthoring] Failed to load %s."), *Filename);
			return;
		}
		UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
		if (!World || !World->PersistentLevel)
		{
			UE_LOG(LogLNPSpawnAuthoring, Error, TEXT("[SpawnAuthoring] Editor World is unavailable."));
			return;
		}
		for (const AActor* Actor : World->PersistentLevel->Actors)
		{
			if (IsValid(Cast<ALNPMassSpawnPoint>(Actor)))
			{
				UE_LOG(LogLNPSpawnAuthoring, Error, TEXT("[SpawnAuthoring] %s already contains Mass spawn points; refusing to add another set."),
					*PackageName);
				return;
			}
		}

		const LNPRegressionFixture::FCaseFrame Crust = LNPRegressionFixture::BasicCrust();
		const LNPRegressionFixture::FCaseFrame Island = LNPRegressionFixture::IslandOne();
		const LNPRegressionFixture::FCaseFrame Cave = LNPRegressionFixture::Cave();
		const LNPCaveKit::FPlacement CavePlacement = LNPCaveKit::PlaceUnderSphere(
			Cave.Radial, Cave.Bitangent, LNPRegressionFixture::CrustRadius);
		const bool bSpawned = SpawnAnchor(*World, TEXT("SpawnPoint_Crust"),
			FTransform(Crust.UpRotation(), Crust.At(LNPRegressionFixture::CrustRadius)))
			&& SpawnAnchor(*World, TEXT("SpawnPoint_Island"),
				FTransform(Island.UpRotation(), Island.At(LNPRegressionFixture::IslandOneTop)))
			&& SpawnAnchor(*World, TEXT("SpawnPoint_Cave"), CavePlacement.Room);
		if (!bSpawned)
		{
			UE_LOG(LogLNPSpawnAuthoring, Error, TEXT("[SpawnAuthoring] Failed to spawn one or more regression anchors."));
			return;
		}
		World->PersistentLevel->MarkPackageDirty();
		UE_LOG(LogLNPSpawnAuthoring, Display, TEXT("[SpawnAuthoring] Regression anchors %s."),
			FEditorFileUtils::SaveLevel(World->PersistentLevel) ? TEXT("saved") : TEXT("FAILED to save"));
	}

	FAutoConsoleCommand MigrateConfigCommand(
		TEXT("LNP.SurfaceNav.MigrateMassSpawnConfig"),
		TEXT("Assign stable non-empty SpawnSetId values to DA_MassSpawnConfig entries and save it."),
		FConsoleCommandDelegate::CreateStatic(&MigrateMassSpawnConfig));

	FAutoConsoleCommand PlaceFixtureCommand(
		TEXT("LNP.SurfaceNav.PlaceRegressionSpawnAnchors"),
		TEXT("Place and save crust, floating-island, and cave Mass spawn anchors in the regression fixture LVI."),
		FConsoleCommandDelegate::CreateStatic(&PlaceRegressionAnchors));
}
