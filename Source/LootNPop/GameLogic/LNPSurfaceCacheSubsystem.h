// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "WorldCollision.h"
#include "LNPSurfaceCacheSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FLNPOnBakingComplete);

/**
 * Thread-safe snapshot of the baked surface cache.
 * Created on the game thread after baking completes; safe to read from any thread.
 */
struct LOOTNPOP_API FLNPSurfaceCacheSnapshot
{
	struct FPoint
	{
		FVector Loc = FVector::ZeroVector;
		bool bValid = false;
	};

	/** Shared reference to the baked data. No copy — multiple snapshots share one allocation. */
	TSharedPtr<const TArray<FPoint>> Points;
	int32 LatRes = 0;
	int32 LonRes = 0;

	bool IsValid() const { return Points.IsValid(); }
	bool GetPoint(const FVector& WorldDirection, FVector& OutPoint) const;
};

/**
 * Precomputes a spherical equirectangular grid of surface hit points via async line traces.
 * All traces are fired in one batch on BeginBaking(); results are collected each tick.
 * The resulting cache is read-only after baking completes, making it safe for Mass worker threads.
 */
UCLASS()
class LOOTNPOP_API ULNPSurfaceCacheSubsystem : public UWorldSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	// FTickableGameObject interface
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override { return bIsBaking; }
	virtual TStatId GetStatId() const override;
	// End FTickableGameObject

	/** Fires all surface traces asynchronously. Called by GameMode after world gen completes. */
	void BeginBaking();

	/**
	 * Thread-safe surface point query.
	 * Returns the baked surface hit point closest to the given world direction.
	 * Only valid after OnBakingComplete fires.
	 */
	bool GetSurfacePoint(const FVector& WorldDirection, FVector& OutPoint) const;

	/** Returns baking progress in [0, 1]. Reaches 1.0 when OnBakingComplete fires. */
	float GetBakingProgress() const;

	/** Creates a thread-safe snapshot for use on background threads. Only valid after OnBakingComplete. */
	FLNPSurfaceCacheSnapshot TakeSnapshot() const;

	UPROPERTY(BlueprintAssignable, Category = "LNP|Surface Cache")
	FLNPOnBakingComplete OnBakingComplete;

private:
	using FPoint = FLNPSurfaceCacheSnapshot::FPoint;

	static FVector IndexToDirection(int32 LatIdx, int32 LonIdx, int32 LatRes, int32 LonRes);
	static void DirectionToIndex(const FVector& Dir, int32 LatRes, int32 LonRes, int32& OutLatIdx, int32& OutLonIdx);

	void OnAsyncTraceComplete(const FTraceHandle& Handle, FTraceDatum& Data);

	/** Owns the baked data. Replaced each BeginBaking() so re-baking never corrupts live snapshots. */
	TSharedPtr<TArray<FPoint>> CacheData;

	/** Single shared delegate — all async traces call back here, using UserData as the sample index. */
	FTraceDelegate TraceDelegate;

	int32 LatResolution = 64;
	int32 LonResolution = 128;
	int32 TotalSamples = 0;
	int32 CompletedCount = 0;
	float SphereRadius = 10000.0f;

	bool bIsBaking = false;
	bool bBakingComplete = false;
};
