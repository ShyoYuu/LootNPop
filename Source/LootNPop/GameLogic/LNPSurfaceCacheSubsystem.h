// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "LNPSurfaceCacheSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FLNPOnBakingComplete);

/**
 * Precomputes a spherical equirectangular grid of surface hit points via line traces.
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

	/** Begin the frame-budgeted surface baking process. Called by GameMode after world gen completes. */
	void BeginBaking();

	/**
	 * Thread-safe surface point query.
	 * Returns the baked surface hit point closest to the given world direction.
	 * Only valid after OnBakingComplete fires.
	 */
	bool GetSurfacePoint(const FVector& WorldDirection, FVector& OutPoint) const;

	/** Returns baking progress in [0, 1]. Reaches 1.0 when OnBakingComplete fires. */
	float GetBakingProgress() const;

	UPROPERTY(BlueprintAssignable, Category = "LNP|Surface Cache")
	FLNPOnBakingComplete OnBakingComplete;

private:
	struct FCacheEntry
	{
		FVector SurfacePoint = FVector::ZeroVector;
		bool bValid = false;
	};

	static FVector IndexToDirection(int32 LatIdx, int32 LonIdx, int32 LatRes, int32 LonRes);
	static void DirectionToIndex(const FVector& Dir, int32 LatRes, int32 LonRes, int32& OutLatIdx, int32& OutLonIdx);

	TArray<FCacheEntry> Cache;
	int32 LatResolution = 64;
	int32 LonResolution = 128;
	int32 TotalSamples = 0;
	int32 NextSampleIndex = 0;
	int32 TracesPerFrame = 64;
	float SphereRadius = 10000.0f;

	bool bIsBaking = false;
	bool bBakingComplete = false;
};
