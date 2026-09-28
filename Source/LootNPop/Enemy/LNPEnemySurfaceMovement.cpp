// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Enemy/LNPEnemySurfaceMovement.h"

#include "LootNPop.h"
#include "SurfaceNavigation/LNPHitIdentityRegistry.h"
#include "SurfaceNavigation/LNPMassWorldCollision.h"
#include "SurfaceNavigation/LNPSurfaceDataSubsystem.h"

#include "HAL/IConsoleManager.h"
#include <atomic>

namespace LNPEnemySurfaceMovement
{
	namespace
	{
		int32 GEnemySupportCache = 1;
		FAutoConsoleVariableRef CVarEnemySupportCache(
			TEXT("LNP.SurfaceNav.EnemySupportCache"), GEnemySupportCache,
			TEXT("Server-only grounded movement. 0 = Phase 3b exact-only baseline, 1 = Support cache high-confidence with exact fallback."));

		std::atomic<uint64> GGroundSteps = 0;
		std::atomic<uint64> GCacheHits = 0;
		std::atomic<uint64> GExactFallbacks = 0;
		std::atomic<uint64> GExactOnlySteps = 0;

		void ReportStats()
		{
			const uint64 Steps = GGroundSteps.load(std::memory_order_relaxed);
			const uint64 CacheHits = GCacheHits.load(std::memory_order_relaxed);
			const uint64 Fallbacks = GExactFallbacks.load(std::memory_order_relaxed);
			const uint64 ExactOnly = GExactOnlySteps.load(std::memory_order_relaxed);
			const double CacheRate = Steps > 0 ? 100.0 * static_cast<double>(CacheHits) / static_cast<double>(Steps) : 0.0;
			UE_LOG(LogLootNPop, Display,
				TEXT("EnemyGround: mode=%s steps=%llu cache_hits=%llu exact_fallbacks=%llu exact_only=%llu cache_rate=%.2f%%"),
				GEnemySupportCache != 0 ? TEXT("cache-first") : TEXT("exact-only"),
				Steps, CacheHits, Fallbacks, ExactOnly, CacheRate);
		}

		void ResetStats()
		{
			GGroundSteps.store(0, std::memory_order_relaxed);
			GCacheHits.store(0, std::memory_order_relaxed);
			GExactFallbacks.store(0, std::memory_order_relaxed);
			GExactOnlySteps.store(0, std::memory_order_relaxed);
			UE_LOG(LogLootNPop, Display, TEXT("EnemyGround counters reset."));
		}

		FAutoConsoleCommand CmdReportStats(
			TEXT("LNP.SurfaceNav.EnemyGround.Report"),
			TEXT("Reports grounded enemy Support cache hits and exact fallback counts."),
			FConsoleCommandDelegate::CreateStatic(&ReportStats));

		FAutoConsoleCommand CmdResetStats(
			TEXT("LNP.SurfaceNav.EnemyGround.Reset"),
			TEXT("Resets grounded enemy Support cache and exact fallback counters."),
			FConsoleCommandDelegate::CreateStatic(&ResetStats));
	}

	bool IsSupportCacheEnabled()
	{
		return GEnemySupportCache != 0;
	}

	bool MakeSurfaceHandle(const FLNPExactHitIdentity& Identity, FLNPSurfaceHandle& OutHandle)
	{
		OutHandle = FLNPSurfaceHandle();
		if (!Identity.IsKnown()
			|| (Identity.Roles & ELNPExactSourceRole::Support) == 0
			|| Identity.Slot < 0
			|| Identity.LocalLayerId == LNPSupportLayers::NoLayer
			|| Identity.SurfaceDataGeneration == 0)
		{
			return false;
		}

		OutHandle.OctantSlot = static_cast<uint16>(Identity.Slot);
		OutHandle.LocalLayerId = Identity.LocalLayerId;
		OutHandle.Generation = Identity.SurfaceDataGeneration;
		return true;
	}

	bool TryResolveCachedGround(
		const FLNPSurfaceDataSnapshot& Snapshot,
		const LNPEnemyExactMovement::FParams& Params,
		const FVector& TargetLocation,
		FLNPSurfaceHandle& InOutSurface,
		FVector& OutLocation)
	{
		// 현재 Layer 정체성이 없는 개체는 캐시가 임의의 가장 위 Layer를 고르게 두지 않는다.
		// 초기 스폰·정적 착지는 유효 handle을 주며, DynamicSupport는 handle이 없어서 계속 exact를 탄다.
		if (!InOutSurface.IsValid() || InOutSurface.Generation != Snapshot.Generation)
		{
			return false;
		}

		const FVector TargetUp = (Params.GravityOrigin - TargetLocation).GetSafeNormal();
		FLNPSurfaceQuery Query;
		Query.WorldPosition = FVector3d(TargetLocation - TargetUp * Params.CapsuleHalfHeight);
		Query.PreferredSurface = InOutSurface;
		Query.MaxStepUp = Params.MaxStepUp;
		Query.MaxDrop = Params.MaxStepDown;

		FLNPSurfaceQueryResult Result;
		if (LNPSurfaceDataLoading::QuerySupport(Snapshot, Query, Result) != ELNPSurfaceQueryStatus::HighConfidence)
		{
			return false;
		}

		OutLocation = FVector(Result.Point) + TargetUp * Params.CapsuleHalfHeight;
		InOutSurface = Result.Surface;
		return true;
	}

	LNPEnemyExactMovement::EGroundResult StepGrounded(
		const FLNPSurfaceDataSnapshot* SurfaceSnapshot,
		const ULNPMassWorldCollisionSubsystem& Collision,
		const LNPEnemyExactMovement::FParams& Params,
		const FVector& Location,
		const FVector& Velocity,
		const float DeltaTime,
		FVector& OutLocation,
		FVector& OutVelocity,
		FLNPSurfaceHandle& InOutSurface)
	{
		GGroundSteps.fetch_add(1, std::memory_order_relaxed);
		const FVector Target = LNPEnemyExactMovement::MoveGroundedLaterally(Collision, Params, Location, Velocity, DeltaTime);

		if (IsSupportCacheEnabled())
		{
			if (SurfaceSnapshot != nullptr
				&& TryResolveCachedGround(*SurfaceSnapshot, Params, Target, InOutSurface, OutLocation))
			{
				OutVelocity = FVector::ZeroVector;
				GCacheHits.fetch_add(1, std::memory_order_relaxed);
				return LNPEnemyExactMovement::EGroundResult::Grounded;
			}
			GExactFallbacks.fetch_add(1, std::memory_order_relaxed);
		}
		else
		{
			GExactOnlySteps.fetch_add(1, std::memory_order_relaxed);
		}

		FLNPExactHitIdentity Identity;
		const LNPEnemyExactMovement::EGroundResult GroundResult = LNPEnemyExactMovement::ProbeGroundedAt(
			Collision, Params, Location, Target, DeltaTime, OutLocation, OutVelocity, &Identity);
		if (GroundResult == LNPEnemyExactMovement::EGroundResult::Grounded)
		{
			FLNPSurfaceHandle Resolved;
			if (MakeSurfaceHandle(Identity, Resolved))
			{
				InOutSurface = Resolved;
			}
			else
			{
				InOutSurface = FLNPSurfaceHandle();
			}
		}
		else if (GroundResult == LNPEnemyExactMovement::EGroundResult::LostSupport)
		{
			InOutSurface = FLNPSurfaceHandle();
		}
		return GroundResult;
	}

	bool StepAirborne(
		const ULNPMassWorldCollisionSubsystem& Collision,
		const LNPEnemyExactMovement::FParams& Params,
		const FVector& Location,
		FVector& InOutVelocity,
		const float DeltaTime,
		FVector& OutLocation,
		FLNPSurfaceHandle& InOutSurface)
	{
		FLNPExactHitIdentity Identity;
		const bool bLanded = LNPEnemyExactMovement::StepAirborne(
			Collision, Params, Location, InOutVelocity, DeltaTime, OutLocation, &Identity);
		InOutSurface = FLNPSurfaceHandle();
		if (bLanded)
		{
			MakeSurfaceHandle(Identity, InOutSurface);
		}
		return bLanded;
	}
}
