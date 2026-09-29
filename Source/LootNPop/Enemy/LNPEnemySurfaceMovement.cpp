// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Enemy/LNPEnemySurfaceMovement.h"

#include "DynamicTerrain/LNPDynamicTerrainSubsystem.h"
#include "Enemy/LNPEnemyMassTypes.h"
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

		FAutoConsoleCommand CmdReportStats(
			TEXT("LNP.SurfaceNav.EnemyGround.Report"),
			TEXT("Reports grounded enemy Support cache hits and exact fallback counts."),
			FConsoleCommandDelegate::CreateStatic(&ReportGroundStats));

		FAutoConsoleCommand CmdResetStats(
			TEXT("LNP.SurfaceNav.EnemyGround.Reset"),
			TEXT("Resets grounded enemy Support cache and exact fallback counters."),
			FConsoleCommandDelegate::CreateStatic(&ResetGroundStats));
	}

	bool IsSupportCacheEnabled()
	{
		return GEnemySupportCache != 0;
	}

	void ResetGroundStats()
	{
		GGroundSteps.store(0, std::memory_order_relaxed);
		GCacheHits.store(0, std::memory_order_relaxed);
		GExactFallbacks.store(0, std::memory_order_relaxed);
		GExactOnlySteps.store(0, std::memory_order_relaxed);
		UE_LOG(LogLootNPop, Display, TEXT("EnemyGround counters reset."));
	}

	void ReportGroundStats()
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

	bool MakeDynamicSupportContact(
		const FLNPExactHitIdentity& Identity,
		const FLNPDynamicSupportFrame& DynamicFrame,
		const FVector& CapsuleCenter,
		FLNPEnemyDynamicSupportContact& OutContact)
	{
		OutContact.Reset();
		if (Identity.Lifetime != ELNPExactSourceLifetime::Dynamic
			|| (Identity.Roles & ELNPExactSourceRole::Support) == 0
			|| !Identity.MarkerId.IsValid())
		{
			return false;
		}

		FLNPPlacementId Id;
		Id.Slot = Identity.Slot;
		Id.MarkerId = Identity.MarkerId;
		const FLNPDynamicSupportSnapshot* Support = DynamicFrame.Find(Id);
		if (Support == nullptr || !Support->bWalkable)
		{
			return false;
		}

		OutContact.SupportId = Id;
		OutContact.LocalCapsuleCenter = Support->CurrentTransform.InverseTransformPositionNoScale(CapsuleCenter);
		OutContact.LastLinearVelocity = Support->LinearVelocity;
		return true;
	}

	bool ApplyDynamicSupportDelta(
		const FLNPDynamicSupportFrame& DynamicFrame,
		FLNPEnemyDynamicSupportContact& InOutContact,
		FTransform& InOutTransform)
	{
		if (!InOutContact.IsValid())
		{
			return false;
		}

		const FLNPDynamicSupportSnapshot* Support = DynamicFrame.Find(InOutContact.SupportId);
		if (Support == nullptr || !Support->bWalkable)
		{
			return false;
		}

		InOutTransform.SetLocation(Support->CurrentTransform.TransformPositionNoScale(InOutContact.LocalCapsuleCenter));
		const FQuat RotationDelta = Support->CurrentTransform.GetRotation() * Support->PreviousTransform.GetRotation().Inverse();
		InOutTransform.SetRotation((RotationDelta * InOutTransform.GetRotation()).GetNormalized());
		InOutContact.LastLinearVelocity = Support->LinearVelocity;
		return true;
	}

	void UpdateActorHandoff(
		const bool bAirborne,
		const FVector& ActorVelocity,
		const FLNPExactHitIdentity* FloorIdentity,
		const FLNPDynamicSupportFrame& DynamicFrame,
		const FVector& CapsuleCenter,
		FVector& OutVelocity,
		FLNPSurfaceHandle& InOutSurface,
		FLNPEnemyDynamicSupportContact& InOutDynamicContact)
	{
		if (bAirborne)
		{
			OutVelocity = ActorVelocity;
			InOutSurface = FLNPSurfaceHandle();
			InOutDynamicContact.Reset();
			return;
		}

		OutVelocity = FVector::ZeroVector;
		// Mover 활성화 첫 프레임처럼 floor blackboard가 아직 비어 있으면 Entity에서 가져온
		// 마지막 정체성을 보존한다. 유효 hit을 받았을 때만 정확한 현재 바닥으로 교체한다.
		if (FloorIdentity == nullptr)
		{
			return;
		}

		FLNPSurfaceHandle ResolvedSurface;
		if (MakeSurfaceHandle(*FloorIdentity, ResolvedSurface))
		{
			InOutSurface = ResolvedSurface;
			InOutDynamicContact.Reset();
			return;
		}

		if (MakeDynamicSupportContact(*FloorIdentity, DynamicFrame, CapsuleCenter, InOutDynamicContact))
		{
			InOutSurface = FLNPSurfaceHandle();
		}
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

	bool ProjectWanderTarget(
		const FLNPSurfaceDataSnapshot* SurfaceSnapshot,
		const ULNPMassWorldCollisionSubsystem& Collision,
		const LNPEnemyExactMovement::FParams& Params,
		const FVector& ReferenceLocation,
		const FVector& Direction,
		const float Reach,
		const FLNPSurfaceHandle& CurrentSurface,
		FVector& OutCapsuleCenter)
	{
		const FVector Dir = Direction.GetSafeNormal();
		if (Dir.IsNearlyZero())
		{
			return false;
		}

		if (IsSupportCacheEnabled() && SurfaceSnapshot != nullptr
			&& CurrentSurface.IsValid() && CurrentSurface.Generation == SurfaceSnapshot->Generation)
		{
			const FVector ReferenceUp = (Params.GravityOrigin - ReferenceLocation).GetSafeNormal();
			const FVector ReferenceFoot = ReferenceLocation - ReferenceUp * Params.CapsuleHalfHeight;
			const double FootRadius = FVector::Dist(Params.GravityOrigin, ReferenceFoot);

			FLNPSurfaceQuery Query;
			Query.WorldPosition = FVector3d(Params.GravityOrigin + Dir * FootRadius);
			Query.PreferredSurface = CurrentSurface;
			Query.MaxStepUp = Reach;
			Query.MaxDrop = Reach;

			FLNPSurfaceQueryResult Result;
			const ELNPSurfaceQueryStatus Status = LNPSurfaceDataLoading::QuerySupport(*SurfaceSnapshot, Query, Result);
			if (Status == ELNPSurfaceQueryStatus::HighConfidence)
			{
				if (Result.Surface.OctantSlot != CurrentSurface.OctantSlot
					|| Result.Surface.LocalLayerId != CurrentSurface.LocalLayerId)
				{
					return false;
				}
				OutCapsuleCenter = FVector(Result.Point) + (-Dir) * Params.CapsuleHalfHeight;
				return true;
			}
		}

		FLNPExactHitIdentity Identity;
		if (!LNPEnemyExactMovement::ProjectToSameLayer(
			Collision, Params, ReferenceLocation, Dir, Reach, OutCapsuleCenter, &Identity))
		{
			return false;
		}

		if (CurrentSurface.IsValid())
		{
			FLNPSurfaceHandle Resolved;
			if (!MakeSurfaceHandle(Identity, Resolved)
				|| Resolved.Generation != CurrentSurface.Generation
				|| Resolved.OctantSlot != CurrentSurface.OctantSlot
				|| Resolved.LocalLayerId != CurrentSurface.LocalLayerId)
			{
				return false;
			}
		}
		return true;
	}

	void UpdateSurfaceHandleAfterAirborne(
		const bool bLanded,
		const FLNPExactHitIdentity& Identity,
		FLNPSurfaceHandle& InOutSurface)
	{
		InOutSurface = FLNPSurfaceHandle();
		if (bLanded)
		{
			MakeSurfaceHandle(Identity, InOutSurface);
		}
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
		FLNPSurfaceHandle& InOutSurface,
		FLNPExactHitIdentity* OutSurfaceIdentity)
	{
		if (OutSurfaceIdentity != nullptr)
		{
			*OutSurfaceIdentity = FLNPExactHitIdentity();
		}
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
		if (OutSurfaceIdentity != nullptr)
		{
			*OutSurfaceIdentity = Identity;
		}
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
		FLNPSurfaceHandle& InOutSurface,
		FLNPExactHitIdentity* OutSurfaceIdentity)
	{
		FLNPExactHitIdentity Identity;
		const bool bLanded = LNPEnemyExactMovement::StepAirborne(
			Collision, Params, Location, InOutVelocity, DeltaTime, OutLocation, &Identity);
		UpdateSurfaceHandleAfterAirborne(bLanded, Identity, InOutSurface);
		if (OutSurfaceIdentity != nullptr)
		{
			*OutSurfaceIdentity = Identity;
		}
		return bLanded;
	}
}
