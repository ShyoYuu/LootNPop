// Copyright (c) 2026 LootNPop. All rights reserved.

#include "SurfaceNavigation/LNPSurfaceDataSubsystem.h"

#include "DataAsset/LNPOctantSurfaceData.h"
#include "GameLogic/LNPOctantSpawnSubsystem.h"
#include "LootNPop.h"
#include "SurfaceNavigation/LNPCrustAtlas.h"
#include "SurfaceNavigation/LNPHitIdentityRegistry.h"
#include "SurfaceNavigation/LNPSpawnData.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/Level.h"
#include "Engine/StreamableManager.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMemory.h"
#include "HAL/PlatformTime.h"
#include "Misc/PackageName.h"

namespace
{
	constexpr int32 SlotCount = 8;

	uint64 GetSupportAllocatedBytes(const FLNPSupportAtlas& Atlas)
	{
		uint64 Bytes = sizeof(FLNPSupportAtlas) + Atlas.Layers.GetAllocatedSize() + Atlas.Sources.GetAllocatedSize();
		for (const FLNPSupportAtlasLayer& Layer : Atlas.Layers)
		{
			Bytes += Layer.Layout.Rows.GetAllocatedSize();
			Bytes += Layer.Layout.RowOffsets.GetAllocatedSize();
			Bytes += Layer.RadiusQ.GetAllocatedSize();
			Bytes += Layer.NormalQ.GetAllocatedSize();
			Bytes += Layer.Flags.GetAllocatedSize();
		}
		for (const FLNPSupportAtlasSource& Source : Atlas.Sources)
		{
			Bytes += Source.Key.GetAllocatedSize();
			Bytes += Source.FaceMap.LayerByExternalFace.GetAllocatedSize();
		}
		return Bytes;
	}

	uint64 GetSpawnAllocatedBytes(const FLNPSpawnData& Spawn)
	{
		return sizeof(FLNPSpawnData)
			+ Spawn.AuthoredAnchors.GetAllocatedSize()
			+ Spawn.RandomCandidates.GetAllocatedSize();
	}

	uint64 GetDecodedResidentBytes(const FLNPSurfaceDataSnapshot& Snapshot)
	{
		uint64 Bytes = sizeof(FLNPSurfaceDataSnapshot) + Snapshot.Slots.GetAllocatedSize();
		TSet<const FLNPSupportAtlas*> CountedSupport;
		TSet<const FLNPSpawnData*> CountedSpawn;
		for (const FLNPSurfaceDataSlotSnapshot& Slot : Snapshot.Slots)
		{
			if (const FLNPSupportAtlas* Support = Slot.Support.Get(); Support != nullptr && !CountedSupport.Contains(Support))
			{
				CountedSupport.Add(Support);
				Bytes += GetSupportAllocatedBytes(*Support);
			}
			if (const FLNPSpawnData* Spawn = Slot.Spawn.Get(); Spawn != nullptr && !CountedSpawn.Contains(Spawn))
			{
				CountedSpawn.Add(Spawn);
				Bytes += GetSpawnAllocatedBytes(*Spawn);
			}
		}
		return Bytes;
	}

	uint64 GetSerializedPayloadBytes(TConstArrayView<TObjectPtr<ULNPOctantSurfaceData>> Assets)
	{
		uint64 Bytes = 0;
		for (const ULNPOctantSurfaceData* Asset : Assets)
		{
			if (Asset != nullptr)
			{
				Bytes += Asset->SupportPayload.GetAllocatedSize();
				Bytes += Asset->NavigationPayload.GetAllocatedSize();
				Bytes += Asset->TraversalPayload.GetAllocatedSize();
				Bytes += Asset->SpawnPayload.GetAllocatedSize();
			}
		}
		return Bytes;
	}

	bool ValidatePayload(
		const TCHAR* Name,
		const FLNPSurfacePayloadDescriptor& Descriptor,
		const TArray<uint8>& Payload,
		const bool bRequired,
		FString& OutError)
	{
		if (Payload.IsEmpty())
		{
			if (bRequired)
			{
				OutError = FString::Printf(TEXT("%s payload is empty."), Name);
				return false;
			}
			if (Descriptor.ElementCount != 0 || Descriptor.UncompressedSize != 0 || !Descriptor.ContentHash.IsZero())
			{
				OutError = FString::Printf(TEXT("%s descriptor is non-empty while its payload is empty."), Name);
				return false;
			}
			return true;
		}

		if (Descriptor.UncompressedSize != static_cast<uint64>(Payload.Num()))
		{
			OutError = FString::Printf(TEXT("%s payload size is %d but descriptor says %llu."),
				Name, Payload.Num(), Descriptor.UncompressedSize);
			return false;
		}
		const FLNPContentHash ActualHash(FIoHash::HashBuffer(Payload.GetData(), Payload.Num()));
		if (Descriptor.ContentHash != ActualHash)
		{
			OutError = FString::Printf(TEXT("%s payload hash mismatch (stored=%s actual=%s)."),
				Name, *Descriptor.ContentHash.ToString(), *ActualHash.ToString());
			return false;
		}
		return true;
	}

	bool ValidateLevelPair(
		const FLNPOctantDefinition& Definition,
		const ULNPOctantSurfaceData& SurfaceData,
		FString& OutError)
	{
		const FString ExpectedPackage = FPackageName::ObjectPathToPackageName(Definition.LevelAsset.ToString());
		int32 SourceLevelCount = 0;
		bool bFoundExpectedLevel = false;
		for (const FLNPOctantSourcePackage& Entry : SurfaceData.Header.SourceManifest)
		{
			if (Entry.Kind != ELNPOctantSourcePackageKind::SourceLevel)
			{
				continue;
			}
			++SourceLevelCount;
			bFoundExpectedLevel |= Entry.PackageName == FName(*ExpectedPackage);
		}
		if (SourceLevelCount != 1 || !bFoundExpectedLevel)
		{
			OutError = FString::Printf(
				TEXT("SurfaceData %s has %d SourceLevel manifest entries and does not uniquely match %s."),
				*SurfaceData.GetPathName(), SourceLevelCount, *ExpectedPackage);
			return false;
		}
		return true;
	}

	bool ValidateAtlasHeader(const FLNPSupportAtlas& Atlas, FString& OutError)
	{
		if (Atlas.Layers.IsEmpty() || !Atlas.Layers[0].Layout.IsFull())
		{
			OutError = TEXT("Support atlas has no full crust Layer 0.");
			return false;
		}

		FIoHash ComputedHashes[3];
		LNPCrustAtlas::ComputeSeamHashes(Atlas.Layers[0], ComputedHashes);
		for (int32 Edge = 0; Edge < 3; ++Edge)
		{
			if (Atlas.SeamHashes[Edge] != ComputedHashes[Edge])
			{
				OutError = FString::Printf(TEXT("Support atlas seam hash %d does not match decoded Layer 0."), Edge);
				return false;
			}
		}

		TSet<FString> SourceKeys;
		for (const FLNPSupportAtlasSource& Source : Atlas.Sources)
		{
			if (Source.Key.IsEmpty() || SourceKeys.Contains(Source.Key))
			{
				OutError = FString::Printf(TEXT("Support atlas contains an empty or duplicate source key '%s'."), *Source.Key);
				return false;
			}
			SourceKeys.Add(Source.Key);
		}
		return true;
	}

	bool ValidateSeams(const TArray<TSharedPtr<const FLNPSupportAtlas, ESPMode::ThreadSafe>>& Atlases, FString& OutError)
	{
		TArray<FLNPCrustSeamPair> Pairs;
		if (!LNPCrustAtlas::ComputeSeamPairs(
			MakeArrayView(ULNPOctantSpawnSubsystem::OctantRotations), Pairs, OutError))
		{
			return false;
		}

		for (const FLNPCrustSeamPair& Pair : Pairs)
		{
			const FLNPSupportAtlasLayer& A = Atlases[Pair.A.Slot]->Layers[0];
			const FLNPSupportAtlasLayer& B = Atlases[Pair.B.Slot]->Layers[0];
			const int32 N = A.Layout.Subdivisions;
			if (N <= 0 || B.Layout.Subdivisions != N
				|| A.RadiusStep != B.RadiusStep || A.BaseRadius != B.BaseRadius)
			{
				OutError = FString::Printf(TEXT("Seam pair %d:%d-%d:%d uses incompatible crust layouts or quantization."),
					Pair.A.Slot, static_cast<int32>(Pair.A.Edge), Pair.B.Slot, static_cast<int32>(Pair.B.Edge));
				return false;
			}

			for (int32 Step = 0; Step <= N; ++Step)
			{
				const FIntPoint CoordA = LNPCrustAtlas::GetSeamSampleCoord(N, Pair.A.Edge, Step);
				const FIntPoint CoordB = LNPCrustAtlas::GetSeamSampleCoord(
					N, Pair.B.Edge, Pair.bReversed ? N - Step : Step);
				const int32 IndexA = LNPSupportAtlas::GetSampleIndex(N, CoordA.X, CoordA.Y);
				const int32 IndexB = LNPSupportAtlas::GetSampleIndex(N, CoordB.X, CoordB.Y);
				const uint8 ValidMask = static_cast<uint8>(ELNPSupportSampleFlags::Valid);
				if (A.RadiusQ[IndexA] != B.RadiusQ[IndexB]
					|| (A.Flags[IndexA] & ValidMask) != (B.Flags[IndexB] & ValidMask))
				{
					OutError = FString::Printf(TEXT("Seam pair %d:%d-%d:%d differs at step %d/%d."),
						Pair.A.Slot, static_cast<int32>(Pair.A.Edge), Pair.B.Slot, static_cast<int32>(Pair.B.Edge), Step, N);
					return false;
				}
			}
		}
		return true;
	}

	bool FindSlotForDirection(
		const FLNPSurfaceDataSnapshot& Snapshot,
		const FVector3d& WorldDirection,
		int32& OutSlot,
		FVector3d& OutLocalDirection)
	{
		if (Snapshot.Generation == 0 || Snapshot.Slots.Num() != SlotCount || WorldDirection.IsNearlyZero())
		{
			return false;
		}

		const FVector3d Direction = WorldDirection.GetSafeNormal();
		constexpr double ComponentTolerance = 1.e-8;
		for (int32 Slot = 0; Slot < Snapshot.Slots.Num(); ++Slot)
		{
			const FVector3d Local = Snapshot.Slots[Slot].WorldToSlotRotation.RotateVector(Direction);
			if (Local.X >= -ComponentTolerance && Local.Y >= -ComponentTolerance && Local.Z >= -ComponentTolerance)
			{
				OutSlot = Slot;
				OutLocalDirection = FVector3d(
					FMath::Max(0.0, Local.X),
					FMath::Max(0.0, Local.Y),
					FMath::Max(0.0, Local.Z)).GetSafeNormal();
				return true;
			}
		}
		return false;
	}

	bool CollectRuntimeSupportSources(
		const ULNPOctantSpawnSubsystem& Octants,
		TArray<TArray<FLNPRuntimeSupportSource>>& OutSources,
		FString& OutError)
	{
		static const FName SupportTag(TEXT("LNP.Surface.Support"));
		OutSources.Reset(SlotCount);
		OutSources.SetNum(SlotCount);
		for (int32 Slot = 0; Slot < SlotCount; ++Slot)
		{
			const ULevel* Level = Octants.GetSlotLevel(Slot);
			if (Level == nullptr)
			{
				OutError = FString::Printf(TEXT("Slot %d has no loaded runtime Level."), Slot);
				return false;
			}

			for (const AActor* Actor : Level->Actors)
			{
				if (!IsValid(Actor))
				{
					continue;
				}
				TInlineComponentArray<UPrimitiveComponent*> Components(Actor);
				for (UPrimitiveComponent* Component : Components)
				{
					if (!IsValid(Component) || !Component->ComponentHasTag(SupportTag))
					{
						continue;
					}
					FLNPRuntimeSupportSource& Source = OutSources[Slot].AddDefaulted_GetRef();
					Source.Key = FString::Printf(TEXT("%s.%s"),
						*Actor->GetFName().ToString(), *Component->GetFName().ToString());
					Source.Component = Component;
				}
			}
		}
		return true;
	}
}

bool LNPSurfaceDataLoading::ValidateAndBuildSnapshot(
	const TArray<FLNPOctantDefinition>& Definitions,
	const TArray<ULNPOctantSurfaceData*>& LoadedData,
	const uint64 Generation,
	FLNPSurfaceDataSnapshot& OutSnapshot,
	FString& OutError)
{
	OutSnapshot = FLNPSurfaceDataSnapshot();
	OutError.Reset();

	if (Definitions.Num() != SlotCount || LoadedData.Num() != SlotCount)
	{
		OutError = FString::Printf(TEXT("Expected 8 selected definitions and loaded SurfaceData assets, got %d and %d."),
			Definitions.Num(), LoadedData.Num());
		return false;
	}

	const FName SeamSignature = Definitions[0].SeamSignature;
	if (SeamSignature.IsNone())
	{
		OutError = TEXT("Selected octant definitions have no SeamSignature.");
		return false;
	}

	TArray<TSharedPtr<const FLNPSupportAtlas, ESPMode::ThreadSafe>> Atlases;
	Atlases.Reserve(SlotCount);
	TMap<const ULNPOctantSurfaceData*, TSharedPtr<const FLNPSupportAtlas, ESPMode::ThreadSafe>> DecodedByAsset;
	TMap<const ULNPOctantSurfaceData*, TSharedPtr<const FLNPSpawnData, ESPMode::ThreadSafe>> DecodedSpawnByAsset;
	OutSnapshot.Generation = Generation;
	OutSnapshot.Slots.Reserve(SlotCount);

	for (int32 Slot = 0; Slot < SlotCount; ++Slot)
	{
		const FLNPOctantDefinition& Definition = Definitions[Slot];
		ULNPOctantSurfaceData* SurfaceData = LoadedData[Slot];
		if (Definition.LevelAsset.IsNull() || Definition.SurfaceData.IsNull() || SurfaceData == nullptr)
		{
			OutError = FString::Printf(TEXT("Slot %d has a missing Level, SurfaceData reference, or loaded asset."), Slot);
			return false;
		}
		if (Definition.SeamSignature != SeamSignature)
		{
			OutError = FString::Printf(TEXT("Slot %d SeamSignature '%s' differs from '%s'."),
				Slot, *Definition.SeamSignature.ToString(), *SeamSignature.ToString());
			return false;
		}
		if (SurfaceData->Header.DataVersion != FLNPSurfaceBakeHeader::CurrentDataVersion)
		{
			OutError = FString::Printf(TEXT("Slot %d SurfaceData %s has DataVersion %u; expected %u."),
				Slot, *SurfaceData->GetPathName(), SurfaceData->Header.DataVersion, FLNPSurfaceBakeHeader::CurrentDataVersion);
			return false;
		}
		if (!ValidateLevelPair(Definition, *SurfaceData, OutError)
			|| !ValidatePayload(TEXT("Support"), SurfaceData->Header.Support, SurfaceData->SupportPayload, true, OutError)
			|| !ValidatePayload(TEXT("Navigation"), SurfaceData->Header.Navigation, SurfaceData->NavigationPayload, false, OutError)
			|| !ValidatePayload(TEXT("Traversal"), SurfaceData->Header.Traversal, SurfaceData->TraversalPayload, false, OutError)
			|| !ValidatePayload(TEXT("Spawn"), SurfaceData->Header.Spawn, SurfaceData->SpawnPayload, true, OutError))
		{
			OutError = FString::Printf(TEXT("Slot %d: %s"), Slot, *OutError);
			return false;
		}

		TSharedPtr<const FLNPSupportAtlas, ESPMode::ThreadSafe> Atlas = DecodedByAsset.FindRef(SurfaceData);
		if (!Atlas.IsValid())
		{
			TSharedPtr<FLNPSupportAtlas, ESPMode::ThreadSafe> Decoded = MakeShared<FLNPSupportAtlas, ESPMode::ThreadSafe>();
			if (!LNPSupportAtlas::Decode(SurfaceData->SupportPayload, *Decoded, OutError)
				|| !ValidateAtlasHeader(*Decoded, OutError))
			{
				OutError = FString::Printf(TEXT("Slot %d Support decode/validation failed: %s"), Slot, *OutError);
				return false;
			}
			uint64 SampleCount = 0;
			for (const FLNPSupportAtlasLayer& Layer : Decoded->Layers)
			{
				SampleCount += Layer.Num();
			}
			if (SampleCount != SurfaceData->Header.Support.ElementCount)
			{
				OutError = FString::Printf(TEXT("Slot %d Support sample count is %llu but descriptor says %u."),
					Slot, SampleCount, SurfaceData->Header.Support.ElementCount);
				return false;
			}
			Atlas = MoveTemp(Decoded);
			DecodedByAsset.Add(SurfaceData, Atlas);
		}

		TSharedPtr<const FLNPSpawnData, ESPMode::ThreadSafe> Spawn = DecodedSpawnByAsset.FindRef(SurfaceData);
		if (!Spawn.IsValid())
		{
			TSharedPtr<FLNPSpawnData, ESPMode::ThreadSafe> Decoded = MakeShared<FLNPSpawnData, ESPMode::ThreadSafe>();
			if (!LNPSpawnData::Decode(SurfaceData->SpawnPayload, *Decoded, OutError))
			{
				OutError = FString::Printf(TEXT("Slot %d Spawn decode/validation failed: %s"), Slot, *OutError);
				return false;
			}
			const uint64 ElementCount = Decoded->AuthoredAnchors.Num() + Decoded->RandomCandidates.Num();
			if (ElementCount != SurfaceData->Header.Spawn.ElementCount)
			{
				OutError = FString::Printf(TEXT("Slot %d Spawn record count is %llu but descriptor says %u."),
					Slot, ElementCount, SurfaceData->Header.Spawn.ElementCount);
				return false;
			}
			Spawn = MoveTemp(Decoded);
			DecodedSpawnByAsset.Add(SurfaceData, Spawn);
		}

		Atlases.Add(Atlas);
		FLNPSurfaceDataSlotSnapshot& SlotSnapshot = OutSnapshot.Slots.AddDefaulted_GetRef();
		SlotSnapshot.LevelAsset = Definition.LevelAsset.ToSoftObjectPath();
		SlotSnapshot.SurfaceDataAsset = Definition.SurfaceData.ToSoftObjectPath();
		SlotSnapshot.SlotRotation = FQuat4d(ULNPOctantSpawnSubsystem::OctantRotations[Slot].Quaternion());
		SlotSnapshot.WorldToSlotRotation = SlotSnapshot.SlotRotation.Inverse();
		SlotSnapshot.Support = Atlas;
		SlotSnapshot.Spawn = Spawn;
	}

	if (!ValidateSeams(Atlases, OutError))
	{
		OutSnapshot = FLNPSurfaceDataSnapshot();
		return false;
	}
	return true;
}

ELNPSurfaceQueryStatus LNPSurfaceDataLoading::QuerySupport(
	const FLNPSurfaceDataSnapshot& Snapshot,
	const FLNPSurfaceQuery& Query,
	FLNPSurfaceQueryResult& OutResult)
{
	OutResult = FLNPSurfaceQueryResult();
	if (Snapshot.Generation == 0 || Snapshot.Slots.Num() != SlotCount)
	{
		return OutResult.Status;
	}

	const double FeetRadius = Query.WorldPosition.Length();
	int32 Slot = INDEX_NONE;
	FVector3d LocalDirection;
	if (FeetRadius <= UE_DOUBLE_SMALL_NUMBER
		|| !FindSlotForDirection(Snapshot, Query.WorldPosition / FeetRadius, Slot, LocalDirection))
	{
		OutResult.Status = ELNPSurfaceQueryStatus::OutsideCoverage;
		return OutResult.Status;
	}

	const FLNPSurfaceDataSlotSnapshot& SlotSnapshot = Snapshot.Slots[Slot];
	if (!SlotSnapshot.Support.IsValid())
	{
		OutResult.Status = ELNPSurfaceQueryStatus::NotReady;
		return OutResult.Status;
	}

	const uint16 PreferredLayer = Query.PreferredSurface.Generation == Snapshot.Generation
		&& Query.PreferredSurface.OctantSlot == Slot
		? Query.PreferredSurface.LocalLayerId
		: LNPSupportLayers::NoLayer;
	FLNPSupportLayerHit Hit;
	const ELNPSupportQueryResult Result = LNPSupportAtlas::QueryLayers(
		*SlotSnapshot.Support,
		LocalDirection,
		FeetRadius,
		Query.MaxStepUp,
		Query.MaxDrop,
		PreferredLayer,
		Hit);
	if (Result == ELNPSupportQueryResult::NeedsExact)
	{
		OutResult.Status = ELNPSurfaceQueryStatus::NeedsExact;
		return OutResult.Status;
	}
	if (Result == ELNPSupportQueryResult::NoSupport)
	{
		OutResult.Status = ELNPSurfaceQueryStatus::NoSupport;
		return OutResult.Status;
	}

	const FVector3d WorldDirection = Query.WorldPosition / FeetRadius;
	OutResult.Status = ELNPSurfaceQueryStatus::HighConfidence;
	OutResult.Point = WorldDirection * Hit.Radius;
	OutResult.Normal = FVector3f(SlotSnapshot.SlotRotation.RotateVector(FVector3d(Hit.Normal)).GetSafeNormal());
	OutResult.Surface.OctantSlot = static_cast<uint16>(Slot);
	OutResult.Surface.LocalLayerId = Hit.Layer;
	OutResult.Surface.Generation = Snapshot.Generation;
	OutResult.RadialDelta = Hit.Radius - FeetRadius;
	return OutResult.Status;
}

bool LNPSurfaceDataLoading::QueryLayerZero(
	const FLNPSurfaceDataSnapshot& Snapshot,
	const FVector3d& WorldDirection,
	FVector3d& OutPoint)
{
	int32 Slot = INDEX_NONE;
	FVector3d LocalDirection;
	if (!FindSlotForDirection(Snapshot, WorldDirection, Slot, LocalDirection))
	{
		return false;
	}

	const FLNPSurfaceDataSlotSnapshot& SlotSnapshot = Snapshot.Slots[Slot];
	if (!SlotSnapshot.Support.IsValid() || SlotSnapshot.Support->Layers.IsEmpty())
	{
		return false;
	}
	FLNPSupportLayerQuery LayerQuery;
	if (!LNPSupportAtlas::QueryLayer(SlotSnapshot.Support->Layers[0], LocalDirection, LayerQuery))
	{
		return false;
	}
	OutPoint = WorldDirection.GetSafeNormal() * LayerQuery.Radius;
	return true;
}

bool LNPSurfaceDataLoading::BuildSourceBindings(
	const FLNPSurfaceDataSnapshot& Snapshot,
	const TArray<TArray<FLNPRuntimeSupportSource>>& RuntimeSources,
	TArray<FLNPSurfaceSourceBinding>& OutBindings,
	FString& OutError)
{
	OutBindings.Reset();
	OutError.Reset();
	if (Snapshot.Generation == 0 || Snapshot.Slots.Num() != SlotCount || RuntimeSources.Num() != SlotCount)
	{
		OutError = TEXT("Source binding requires one ready snapshot and runtime source list for all 8 slots.");
		return false;
	}

	for (int32 Slot = 0; Slot < SlotCount; ++Slot)
	{
		const TSharedPtr<const FLNPSupportAtlas, ESPMode::ThreadSafe>& Atlas = Snapshot.Slots[Slot].Support;
		if (!Atlas.IsValid())
		{
			OutError = FString::Printf(TEXT("Slot %d has no decoded Support atlas."), Slot);
			return false;
		}

		TMap<FString, UPrimitiveComponent*> ComponentsByKey;
		for (const FLNPRuntimeSupportSource& RuntimeSource : RuntimeSources[Slot])
		{
			UPrimitiveComponent* Component = RuntimeSource.Component.Get();
			if (RuntimeSource.Key.IsEmpty() || Component == nullptr)
			{
				OutError = FString::Printf(TEXT("Slot %d contains an empty source key or invalid component."), Slot);
				return false;
			}
			if (ComponentsByKey.Contains(RuntimeSource.Key))
			{
				OutError = FString::Printf(TEXT("Slot %d runtime source key '%s' is duplicated."), Slot, *RuntimeSource.Key);
				return false;
			}
			ComponentsByKey.Add(RuntimeSource.Key, Component);
		}

		for (int32 SourceIndex = 0; SourceIndex < Atlas->Sources.Num(); ++SourceIndex)
		{
			const FLNPSupportAtlasSource& SavedSource = Atlas->Sources[SourceIndex];
			UPrimitiveComponent* const* Component = ComponentsByKey.Find(SavedSource.Key);
			if (Component == nullptr)
			{
				OutError = FString::Printf(TEXT("Slot %d is missing runtime Support source '%s'."), Slot, *SavedSource.Key);
				return false;
			}
			FLNPSurfaceSourceBinding& Binding = OutBindings.AddDefaulted_GetRef();
			Binding.Component = *Component;
			Binding.Slot = static_cast<int8>(Slot);
			Binding.Support = Atlas;
			Binding.SourceIndex = SourceIndex;
		}

		if (ComponentsByKey.Num() != Atlas->Sources.Num())
		{
			for (const TPair<FString, UPrimitiveComponent*>& Pair : ComponentsByKey)
			{
				if (!Atlas->Sources.ContainsByPredicate([&Pair](const FLNPSupportAtlasSource& Source)
				{
					return Source.Key.Equals(Pair.Key, ESearchCase::CaseSensitive);
				}))
				{
					OutError = FString::Printf(TEXT("Slot %d has unexpected runtime Support source '%s'."), Slot, *Pair.Key);
					return false;
				}
			}
		}
	}
	return true;
}

void ULNPSurfaceDataSubsystem::BeginLoading()
{
	check(IsInGameThread());
	if (LoadState != ELNPSurfaceDataLoadState::NotStarted)
	{
		return;
	}

	const ULNPOctantSpawnSubsystem* Octants = GetWorld() ? GetWorld()->GetSubsystem<ULNPOctantSpawnSubsystem>() : nullptr;
	if (Octants == nullptr || !Octants->bGenerationComplete)
	{
		Fail(TEXT("Octant world generation is not complete."));
		return;
	}
	const TArray<FLNPOctantDefinition>& Definitions = Octants->GetSelectedOctantDefinitions();
	if (Definitions.Num() != SlotCount)
	{
		Fail(FString::Printf(TEXT("Expected 8 selected octant definitions, got %d."), Definitions.Num()));
		return;
	}

	TArray<FSoftObjectPath> Paths;
	Paths.Reserve(SlotCount);
	for (int32 Slot = 0; Slot < Definitions.Num(); ++Slot)
	{
		if (Definitions[Slot].SurfaceData.IsNull())
		{
			Fail(FString::Printf(TEXT("Slot %d definition %s has no SurfaceData."),
				Slot, *Definitions[Slot].LevelAsset.ToString()));
			return;
		}
		Paths.AddUnique(Definitions[Slot].SurfaceData.ToSoftObjectPath());
	}

	LoadState = ELNPSurfaceDataLoadState::Loading;
	LoadStartTimeSeconds = FPlatformTime::Seconds();
	LoadStartPhysicalBytes = FPlatformMemory::GetStats().UsedPhysical;
	LoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		Paths,
		FStreamableDelegate::CreateUObject(this, &ULNPSurfaceDataSubsystem::OnAssetsLoaded),
		FStreamableManager::AsyncLoadHighPriority,
		false,
		false,
		TEXT("LNP SurfaceData"));
	if (!LoadHandle.IsValid())
	{
		Fail(TEXT("RequestAsyncLoad returned no handle for selected SurfaceData assets."));
	}
}

void ULNPSurfaceDataSubsystem::OnAssetsLoaded()
{
	check(IsInGameThread());
	const ULNPOctantSpawnSubsystem* Octants = GetWorld() ? GetWorld()->GetSubsystem<ULNPOctantSpawnSubsystem>() : nullptr;
	if (Octants == nullptr)
	{
		Fail(TEXT("Octant subsystem disappeared while SurfaceData was loading."));
		return;
	}

	const TArray<FLNPOctantDefinition>& Definitions = Octants->GetSelectedOctantDefinitions();
	TArray<ULNPOctantSurfaceData*> SlotData;
	SlotData.Reserve(Definitions.Num());
	LoadedAssets.Reset();
	for (const FLNPOctantDefinition& Definition : Definitions)
	{
		ULNPOctantSurfaceData* Data = Definition.SurfaceData.Get();
		SlotData.Add(Data);
		LoadedAssets.AddUnique(Data);
	}

	FLNPSurfaceDataSnapshot Snapshot;
	FString Error;
	if (!LNPSurfaceDataLoading::ValidateAndBuildSnapshot(Definitions, SlotData, NextGeneration, Snapshot, Error))
	{
		Fail(MoveTemp(Error));
		return;
	}

	TArray<TArray<FLNPRuntimeSupportSource>> RuntimeSources;
	TArray<FLNPSurfaceSourceBinding> SourceBindings;
	if (!CollectRuntimeSupportSources(*Octants, RuntimeSources, Error)
		|| !LNPSurfaceDataLoading::BuildSourceBindings(Snapshot, RuntimeSources, SourceBindings, Error))
	{
		Fail(MoveTemp(Error));
		return;
	}
	ULNPHitIdentitySubsystem* HitIdentity = GetWorld()->GetSubsystem<ULNPHitIdentitySubsystem>();
	if (HitIdentity == nullptr)
	{
		Fail(TEXT("Hit identity subsystem is unavailable."));
		return;
	}
	if (!HitIdentity->PublishSurfaceBindings(Snapshot.Generation, SourceBindings, Error))
	{
		Fail(MoveTemp(Error));
		return;
	}

	const uint64 DecodedResidentBytes = GetDecodedResidentBytes(Snapshot);
	PublishedSnapshot = MakeShared<FLNPSurfaceDataSnapshot, ESPMode::ThreadSafe>(MoveTemp(Snapshot));
	++NextGeneration;
	bSnapshotReady.store(true, std::memory_order_release);
	LoadState = ELNPSurfaceDataLoadState::Ready;
	LastError.Reset();
	LoadHandle.Reset();
	UE_LOG(LogLootNPop, Log,
		TEXT("LNPSurfaceDataSubsystem: Published generation %llu with 8 validated slots, %d source bindings, registry generation %u."),
		PublishedSnapshot->Generation, SourceBindings.Num(), HitIdentity->GetSnapshot()->Generation);
	const FPlatformMemoryStats MemoryStats = FPlatformMemory::GetStats();
	UE_LOG(LogLootNPop, Log,
		TEXT("LNPSurfaceDataSubsystem: Load metrics elapsed=%.2fms serialized=%.2fMiB decodedResident=%.2fMiB processPhysicalStart=%.2fMiB processPhysicalPublish=%.2fMiB processPhysicalPeak=%.2fMiB."),
		(FPlatformTime::Seconds() - LoadStartTimeSeconds) * 1000.0,
		GetSerializedPayloadBytes(LoadedAssets) / (1024.0 * 1024.0),
		DecodedResidentBytes / (1024.0 * 1024.0),
		LoadStartPhysicalBytes / (1024.0 * 1024.0),
		MemoryStats.UsedPhysical / (1024.0 * 1024.0),
		MemoryStats.PeakUsedPhysical / (1024.0 * 1024.0));
	OnSurfaceDataReady.Broadcast();
}

void ULNPSurfaceDataSubsystem::Fail(FString Error)
{
	check(IsInGameThread());
	bSnapshotReady.store(false, std::memory_order_release);
	PublishedSnapshot.Reset();
	LoadedAssets.Reset();
	LoadHandle.Reset();
	LoadState = ELNPSurfaceDataLoadState::Failed;
	LastError = MoveTemp(Error);
	UE_LOG(LogLootNPop, Error, TEXT("LNPSurfaceDataSubsystem: %s"), *LastError);
	OnSurfaceDataFailed.Broadcast(LastError);
}

float ULNPSurfaceDataSubsystem::GetLoadProgress() const
{
	if (LoadState == ELNPSurfaceDataLoadState::Ready)
	{
		return 1.0f;
	}
	return LoadHandle.IsValid() ? LoadHandle->GetProgress() : 0.0f;
}

TSharedPtr<const FLNPSurfaceDataSnapshot, ESPMode::ThreadSafe> ULNPSurfaceDataSubsystem::TakeSnapshot() const
{
	if (!bSnapshotReady.load(std::memory_order_acquire))
	{
		return nullptr;
	}
	return PublishedSnapshot;
}

ELNPSurfaceQueryStatus ULNPSurfaceDataSubsystem::QuerySupport(
	const FLNPSurfaceQuery& Query,
	FLNPSurfaceQueryResult& OutResult) const
{
	const TSharedPtr<const FLNPSurfaceDataSnapshot, ESPMode::ThreadSafe> Snapshot = TakeSnapshot();
	return Snapshot.IsValid()
		? LNPSurfaceDataLoading::QuerySupport(*Snapshot, Query, OutResult)
		: (OutResult = FLNPSurfaceQueryResult()).Status;
}

bool ULNPSurfaceDataSubsystem::GetSurfacePoint(const FVector& WorldDirection, FVector& OutPoint) const
{
	const TSharedPtr<const FLNPSurfaceDataSnapshot, ESPMode::ThreadSafe> Snapshot = TakeSnapshot();
	if (!Snapshot.IsValid())
	{
		return false;
	}

#if !UE_BUILD_SHIPPING
	if (!bLegacyMultiLayerWarningEmitted.load(std::memory_order_relaxed)
		&& Snapshot->Slots.ContainsByPredicate([](const FLNPSurfaceDataSlotSnapshot& Slot)
		{
			return Slot.Support.IsValid() && Slot.Support->Layers.Num() > 1;
		}))
	{
		bool bExpected = false;
		if (bLegacyMultiLayerWarningEmitted.compare_exchange_strong(bExpected, true, std::memory_order_relaxed))
		{
			UE_LOG(LogLootNPop, Warning, TEXT("LNPSurfaceDataSubsystem: Legacy GetSurfacePoint queries crust Layer 0 only in a multi-layer world."));
		}
	}
#endif

	FVector3d Point;
	if (!LNPSurfaceDataLoading::QueryLayerZero(*Snapshot, FVector3d(WorldDirection), Point))
	{
		return false;
	}
	OutPoint = FVector(Point);
	return true;
}

void ULNPSurfaceDataSubsystem::Deinitialize()
{
	bSnapshotReady.store(false, std::memory_order_release);
	PublishedSnapshot.Reset();
	LoadedAssets.Reset();
	if (LoadHandle.IsValid())
	{
		LoadHandle->CancelHandle();
		LoadHandle.Reset();
	}
	LoadState = ELNPSurfaceDataLoadState::NotStarted;
	LastError.Reset();
	LoadStartTimeSeconds = 0.0;
	LoadStartPhysicalBytes = 0;
	bLegacyMultiLayerWarningEmitted.store(false, std::memory_order_relaxed);
	Super::Deinitialize();
}

#if !UE_BUILD_SHIPPING
namespace
{
	FAutoConsoleCommandWithWorld GLNPProbeSurfaceData(
		TEXT("LNP.SurfaceNav.ProbeSurfaceData"),
		TEXT("Validate the published 8-slot Support snapshot, inverse-rotation queries, and hit identity SurfaceData generation."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			const ULNPSurfaceDataSubsystem* SurfaceData = World
				? World->GetSubsystem<ULNPSurfaceDataSubsystem>()
				: nullptr;
			const ULNPHitIdentitySubsystem* HitIdentity = World
				? World->GetSubsystem<ULNPHitIdentitySubsystem>()
				: nullptr;
			const TSharedPtr<const FLNPSurfaceDataSnapshot, ESPMode::ThreadSafe> SurfaceSnapshot = SurfaceData
				? SurfaceData->TakeSnapshot()
				: nullptr;
			TSharedPtr<const FLNPHitIdentitySnapshot, ESPMode::ThreadSafe> IdentitySnapshot;
			if (HitIdentity != nullptr)
			{
				IdentitySnapshot = HitIdentity->GetSnapshot();
			}
			if (!SurfaceSnapshot.IsValid() || !IdentitySnapshot.IsValid())
			{
				UE_LOG(LogLootNPop, Warning, TEXT("[ProbeSurfaceData] %s -> NOT_READY"), *GetNameSafe(World));
				return;
			}

			int32 QueryPasses = 0;
			int32 ExpectedBindings = 0;
			for (int32 Slot = 0; Slot < SurfaceSnapshot->Slots.Num(); ++Slot)
			{
				const FLNPSurfaceDataSlotSnapshot& SlotSnapshot = SurfaceSnapshot->Slots[Slot];
				if (!SlotSnapshot.Support.IsValid())
				{
					continue;
				}
				ExpectedBindings += SlotSnapshot.Support->Sources.Num();
				bool bQueryPassed = false;
				constexpr int32 ProbeCount = 256;
				constexpr double GoldenRatioConjugate = 0.6180339887498948482;
				for (int32 Probe = 0; Probe < ProbeCount && !bQueryPassed; ++Probe)
				{
					const double Z = (Probe + 0.5) / ProbeCount;
					const double Phi = FMath::Frac(Probe * GoldenRatioConjugate) * UE_HALF_PI;
					const double RadiusXY = FMath::Sqrt(FMath::Max(0.0, 1.0 - Z * Z));
					const FVector3d LocalDirection(RadiusXY * FMath::Cos(Phi), RadiusXY * FMath::Sin(Phi), Z);
					const FVector3d WorldDirection = SlotSnapshot.SlotRotation.RotateVector(LocalDirection);
					FVector3d Point;
					bQueryPassed = LNPSurfaceDataLoading::QueryLayerZero(*SurfaceSnapshot, WorldDirection, Point)
						&& !Point.ContainsNaN() && Point.Length() > 0.0;
				}
				QueryPasses += bQueryPassed ? 1 : 0;
			}

			int32 BoundSources = 0;
			for (const TPair<TWeakObjectPtr<UPrimitiveComponent>, FLNPExactSourceEntry>& Pair : IdentitySnapshot->Sources)
			{
				BoundSources += Pair.Value.HasSupportFaceMap() ? 1 : 0;
			}
			const bool bPass = SurfaceSnapshot->Slots.Num() == SlotCount
				&& QueryPasses == SlotCount
				&& BoundSources == ExpectedBindings
				&& IdentitySnapshot->SurfaceDataGeneration == SurfaceSnapshot->Generation;
			UE_LOG(LogLootNPop, Display,
				TEXT("[ProbeSurfaceData] %s NetMode=%d surfaceGeneration=%llu registrySurfaceGeneration=%llu slots=%d queries=%d/%d bindings=%d/%d -> %s"),
				*GetNameSafe(World), static_cast<int32>(World->GetNetMode()), SurfaceSnapshot->Generation,
				IdentitySnapshot->SurfaceDataGeneration, SurfaceSnapshot->Slots.Num(), QueryPasses, SlotCount,
				BoundSources, ExpectedBindings, bPass ? TEXT("PASS") : TEXT("FAIL"));
		}));
}
#endif
