// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Enemy/LNPEnemySurfaceMovement.h"

#include "DynamicTerrain/LNPDynamicTerrainSubsystem.h"
#include "Enemy/LNPEnemyMassTypes.h"
#include "LootNPop.h"
#include "SurfaceNavigation/LNPHitIdentityRegistry.h"
#include "SurfaceNavigation/LNPMassWorldCollision.h"
#include "SurfaceNavigation/LNPNavQuery.h"
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
		std::atomic<uint64> GBlockerRejects = 0;
		std::atomic<uint64> GNavSeparationRetries = 0;

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
		GBlockerRejects.store(0, std::memory_order_relaxed);
		GNavSeparationRetries.store(0, std::memory_order_relaxed);
		UE_LOG(LogLootNPop, Display, TEXT("EnemyGround counters reset."));
	}

	void ReportGroundStats()
	{
		const uint64 Steps = GGroundSteps.load(std::memory_order_relaxed);
		const uint64 CacheHits = GCacheHits.load(std::memory_order_relaxed);
		const uint64 Fallbacks = GExactFallbacks.load(std::memory_order_relaxed);
		const uint64 ExactOnly = GExactOnlySteps.load(std::memory_order_relaxed);
		const uint64 BlockerRejects = GBlockerRejects.load(std::memory_order_relaxed);
		const uint64 NavSeparationRetries = GNavSeparationRetries.load(std::memory_order_relaxed);
		const double CacheRate = Steps > 0 ? 100.0 * static_cast<double>(CacheHits) / static_cast<double>(Steps) : 0.0;
		UE_LOG(LogLootNPop, Display,
			TEXT("EnemyGround: mode=%s steps=%llu cache_hits=%llu exact_fallbacks=%llu exact_only=%llu cache_rate=%.2f%% blocker_rejects=%llu nav_separation_retries=%llu"),
			GEnemySupportCache != 0 ? TEXT("cache-first") : TEXT("exact-only"),
			Steps, CacheHits, Fallbacks, ExactOnly, CacheRate, BlockerRejects, NavSeparationRetries);
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
			|| (Identity.LocalLayerId != LNPSupportLayers::NoLayer && Identity.SurfaceDataGeneration != 0)
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
		OutContact.LastLinearVelocity = Support->LinearVelocity + FVector::CrossProduct(Support->AngularVelocity,
			CapsuleCenter - Support->CurrentTransform.GetLocation());
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
		InOutContact.LastLinearVelocity = Support->LinearVelocity + FVector::CrossProduct(Support->AngularVelocity,
			InOutTransform.GetLocation() - Support->CurrentTransform.GetLocation());
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
		else
		{
			InOutSurface = FLNPSurfaceHandle();
			InOutDynamicContact.Reset();
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
			const FLNPSurfaceHandle PreviousSurface = InOutSurface;
			if (SurfaceSnapshot != nullptr
				&& TryResolveCachedGround(*SurfaceSnapshot, Params, Target, InOutSurface, OutLocation))
			{
				OutVelocity = FVector::ZeroVector;
				GCacheHits.fetch_add(1, std::memory_order_relaxed);
				// Support Atlas는 Blocker를 모른다. 측면 sweep이 MaxStepUp 아래 Blocker를 턱으로 넘기면 캐시는 그 밑의 지지면에
				// 접지시켜 캡슐이 Blocker 안에 놓인다. exact probe라면 Rejected였을 자리이므로 같은 규칙으로 제자리에 선다.
				// 이미 겹친 채 출발했다면(스폰·상태형 지형이 덮음) 빠져나갈 수 있게 막지 않는다.
				if (Target != Location
					&& LNPEnemyExactMovement::OverlapsSupportlessBlocker(Collision, Params, OutLocation)
					&& !LNPEnemyExactMovement::OverlapsSupportlessBlocker(Collision, Params, Location))
				{
					GBlockerRejects.fetch_add(1, std::memory_order_relaxed);
					InOutSurface = PreviousSurface;
					OutLocation = Location;
					return LNPEnemyExactMovement::EGroundResult::Rejected;
				}
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

	LNPEnemyExactMovement::EGroundResult StepGroundedWithSeparation(
		const FLNPSurfaceDataSnapshot* SurfaceSnapshot,
		const ULNPMassWorldCollisionSubsystem& Collision,
		const LNPEnemyExactMovement::FParams& Params,
		const FVector& Location,
		const FVector& IntentVelocity,
		const FVector& SeparationVelocity,
		const float DeltaTime,
		FVector& OutLocation,
		FVector& OutVelocity,
		FLNPSurfaceHandle& InOutSurface,
		const int32 NavHintNode,
		FLNPExactHitIdentity* OutSurfaceIdentity)
	{
		const FLNPSurfaceHandle PreviousSurface = InOutSurface;
		LNPEnemyExactMovement::EGroundResult Result = StepGrounded(SurfaceSnapshot, Collision, Params, Location, IntentVelocity + SeparationVelocity,
			DeltaTime, OutLocation, OutVelocity, InOutSurface, OutSurfaceIdentity);
		if (Result != LNPEnemyExactMovement::EGroundResult::Grounded || SeparationVelocity.IsNearlyZero()
			|| SurfaceSnapshot == nullptr || !SurfaceSnapshot->Nav.Graph.IsValid() || !InOutSurface.IsValid())
			return Result;

		const FLNPNavSnapshot& Nav = SurfaceSnapshot->Nav;
		auto HasNavStart = [&](const FVector& Center, const FLNPSurfaceHandle& Surface)
		{
			if (!Surface.IsValid() || Surface.Generation != Nav.SnapshotGeneration)
				return false;
			const FVector3d Feet(Center - (Params.GravityOrigin - Center).GetSafeNormal() * Params.CapsuleHalfHeight);
			const FLNPNavGraph& Graph = Nav.Graph;
			const int32 HintSlot = Graph.GetSlot(NavHintNode);
			// 경로 소비자가 이미 찾은 node의 300cm 안이면 node 존재가 증명되므로 최근접을 다시 찾지 않는다.
			if (HintSlot == Surface.OctantSlot && !Graph.BlockedNodes[NavHintNode])
			{
				const FLNPNavGraphNode& Hint = Graph.GetNode(HintSlot, NavHintNode);
				if (Graph.SlotGraphs[HintSlot]->Layers[Hint.LayerOrdinal].LocalNavLayerId == Surface.LocalLayerId
					&& FVector3d::DistSquared(Graph.GetWorldPoint(HintSlot, NavHintNode), Feet)
						<= FMath::Square(LNPNavQuery::DefaultProjectionRadius))
					return true;
			}
			return LNPNavGraph::FindNearestNode(Nav, Surface.OctantSlot, Surface.LocalLayerId, Feet,
				LNPNavQuery::DefaultProjectionRadius, nullptr, NavHintNode).Node != INDEX_NONE;
		};
		if (HasNavStart(OutLocation, InOutSurface) || !HasNavStart(Location, PreviousSurface))
			return Result;

		// Support가 있는 Nav 공백으로 분리력이 밀어내면 다음 프레임 NoNode가 되어 추격도 배회도 복구하지 못한다.
		// 원래 의도로 다시 걸어 경계 방향 추격을 살린다. 이미 Nav 밖에서 시작한 개체의 탈출은 막지 않는다.
		GNavSeparationRetries.fetch_add(1, std::memory_order_relaxed);
		InOutSurface = PreviousSurface;
		Result = StepGrounded(SurfaceSnapshot, Collision, Params, Location, IntentVelocity,
			DeltaTime, OutLocation, OutVelocity, InOutSurface, OutSurfaceIdentity);
		if (Result == LNPEnemyExactMovement::EGroundResult::Grounded && InOutSurface.IsValid()
			&& !HasNavStart(OutLocation, InOutSurface))
		{
			OutLocation = Location;
			OutVelocity = FVector::ZeroVector;
			InOutSurface = PreviousSurface;
			if (OutSurfaceIdentity != nullptr)
				*OutSurfaceIdentity = FLNPExactHitIdentity();
			return LNPEnemyExactMovement::EGroundResult::Rejected;
		}
		return Result;
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
