// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "DataAsset/LNPOctantDefinition.h"
#include "Mass/ExternalSubsystemTraits.h"
#include "SurfaceNavigation/LNPSupportAtlas.h"
#include "SurfaceNavigation/LNPSpawnData.h"
#include "Subsystems/WorldSubsystem.h"
#include <atomic>
#include "LNPSurfaceDataSubsystem.generated.h"

struct FStreamableHandle;
class ULNPOctantSurfaceData;
class UPrimitiveComponent;

UENUM(BlueprintType)
enum class ELNPSurfaceDataLoadState : uint8
{
	NotStarted,
	Loading,
	Ready,
	Failed,
};

/** Immutable payload view for one runtime octant slot. UObject lifetime is owned by the subsystem. */
struct FLNPSurfaceDataSlotSnapshot
{
	FSoftObjectPath LevelAsset;
	FSoftObjectPath SurfaceDataAsset;
	FQuat4d SlotRotation = FQuat4d::Identity;
	FQuat4d WorldToSlotRotation = FQuat4d::Identity;
	TSharedPtr<const FLNPSupportAtlas, ESPMode::ThreadSafe> Support;
	TSharedPtr<const FLNPSpawnData, ESPMode::ThreadSafe> Spawn;
};

/** One fully validated 8-slot Support/Spawn generation. Later phases add Nav views without changing publication semantics. */
struct FLNPSurfaceDataSnapshot
{
	uint64 Generation = 0;
	TArray<FLNPSurfaceDataSlotSnapshot> Slots;
};

/** snapshot generation 안에서 Support Layer를 가리키는 안정 handle. */
struct FLNPSurfaceHandle
{
	uint16 OctantSlot = MAX_uint16;
	uint16 LocalLayerId = LNPSupportLayers::NoLayer;
	uint64 Generation = 0;

	bool IsValid() const
	{
		return OctantSlot != MAX_uint16 && LocalLayerId != LNPSupportLayers::NoLayer && Generation != 0;
	}
};

enum class ELNPSurfaceQueryStatus : uint8
{
	NotReady,
	HighConfidence,
	NeedsExact,
	NoSupport,
	OutsideCoverage,
};

struct FLNPSurfaceQuery
{
	FVector3d WorldPosition = FVector3d::ZeroVector;
	FLNPSurfaceHandle PreferredSurface;
	double MaxStepUp = 0.0;
	double MaxDrop = 0.0;
};

struct FLNPSurfaceQueryResult
{
	ELNPSurfaceQueryStatus Status = ELNPSurfaceQueryStatus::NotReady;
	FVector3d Point = FVector3d::ZeroVector;
	FVector3f Normal = FVector3f::ZeroVector;
	FLNPSurfaceHandle Surface;
	double RadialDelta = 0.0;
};

/** runtime Level Instance에서 수집한 source key와 component. */
struct FLNPRuntimeSupportSource
{
	FString Key;
	TWeakObjectPtr<UPrimitiveComponent> Component;
};

/** hit identity registry에 설치할 검증 완료 face map binding. */
struct FLNPSurfaceSourceBinding
{
	TWeakObjectPtr<UPrimitiveComponent> Component;
	int8 Slot = INDEX_NONE;
	TSharedPtr<const FLNPSupportAtlas, ESPMode::ThreadSafe> Support;
	int32 SourceIndex = INDEX_NONE;
};

namespace LNPSurfaceDataLoading
{
	/** Pure validation/assembly boundary shared by the runtime loader and automation tests. */
	LOOTNPOP_API bool ValidateAndBuildSnapshot(
		const TArray<FLNPOctantDefinition>& Definitions,
		const TArray<ULNPOctantSurfaceData*>& LoadedData,
		uint64 Generation,
		FLNPSurfaceDataSnapshot& OutSnapshot,
		FString& OutError);

	/** world 방향을 slot local 방향으로 바꿔 immutable Support snapshot을 조회한다. */
	LOOTNPOP_API ELNPSurfaceQueryStatus QuerySupport(
		const FLNPSurfaceDataSnapshot& Snapshot,
		const FLNPSurfaceQuery& Query,
		FLNPSurfaceQueryResult& OutResult);

	/** Phase 6까지 유지하는 지각 Layer 0 전용 호환 조회. */
	LOOTNPOP_API bool QueryLayerZero(
		const FLNPSurfaceDataSnapshot& Snapshot,
		const FVector3d& WorldDirection,
		FVector3d& OutPoint);

	/** slot마다 runtime key가 저장된 source 표와 정확히 일치하는지 검사하고 face map binding을 만든다. */
	LOOTNPOP_API bool BuildSourceBindings(
		const FLNPSurfaceDataSnapshot& Snapshot,
		const TArray<TArray<FLNPRuntimeSupportSource>>& RuntimeSources,
		TArray<FLNPSurfaceSourceBinding>& OutBindings,
		FString& OutError);
}

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FLNPOnSurfaceDataReady);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLNPOnSurfaceDataFailed, const FString&, Error);

/**
 * Loads the SurfaceData selected by ULNPOctantSpawnSubsystem and publishes one immutable 8-slot snapshot.
 * BeginLoading is game-thread-only. Readers may call TakeSnapshot from Mass workers after IsReady is true.
 */
UCLASS()
class LOOTNPOP_API ULNPSurfaceDataSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	/** Starts async loading after all eight Level Instances are visible. Repeated calls are no-ops. */
	UFUNCTION(BlueprintCallable, Category = "LNP|Surface Navigation")
	void BeginLoading();

	UFUNCTION(BlueprintPure, Category = "LNP|Surface Navigation")
	ELNPSurfaceDataLoadState GetLoadState() const { return LoadState; }

	UFUNCTION(BlueprintPure, Category = "LNP|Surface Navigation")
	float GetLoadProgress() const;

	const FString& GetLastError() const { return LastError; }
	bool IsReady() const { return bSnapshotReady.load(std::memory_order_acquire); }

	/** Returns null before successful publication. The returned generation is immutable. */
	TSharedPtr<const FLNPSurfaceDataSnapshot, ESPMode::ThreadSafe> TakeSnapshot() const;

	/** 다층 Support query. 게시 전에는 NotReady를 반환한다. */
	ELNPSurfaceQueryStatus QuerySupport(const FLNPSurfaceQuery& Query, FLNPSurfaceQueryResult& OutResult) const;

	/** Phase 6까지의 지각 Layer 0 호환 adapter. 게시 전 또는 risk 구간이면 false다. */
	bool GetSurfacePoint(const FVector& WorldDirection, FVector& OutPoint) const;

	UPROPERTY(BlueprintAssignable, Category = "LNP|Surface Navigation")
	FLNPOnSurfaceDataReady OnSurfaceDataReady;

	UPROPERTY(BlueprintAssignable, Category = "LNP|Surface Navigation")
	FLNPOnSurfaceDataFailed OnSurfaceDataFailed;

private:
	void OnAssetsLoaded();
	void Fail(FString Error);

	ELNPSurfaceDataLoadState LoadState = ELNPSurfaceDataLoadState::NotStarted;
	FString LastError;
	uint64 NextGeneration = 1;

	TSharedPtr<FStreamableHandle> LoadHandle;
	TSharedPtr<const FLNPSurfaceDataSnapshot, ESPMode::ThreadSafe> PublishedSnapshot;
	std::atomic<bool> bSnapshotReady = false;
	mutable std::atomic<bool> bLegacyMultiLayerWarningEmitted = false;

	/** Strong UObject references backing PublishedSnapshot's decoded POD views. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<ULNPOctantSurfaceData>> LoadedAssets;
};

/** Worker access is read-only after release publication. Loading and publication stay on the game thread. */
template<>
struct TMassExternalSubsystemTraits<ULNPSurfaceDataSubsystem> final
{
	enum
	{
		GameThreadOnly = false,
		ThreadSafeWrite = false,
	};
};
