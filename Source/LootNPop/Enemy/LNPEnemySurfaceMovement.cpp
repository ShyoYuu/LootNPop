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
		std::atomic<uint64> GBlockerRejects = 0;
		std::atomic<uint64> GBlockerDetours = 0;

		/** Blocker 거부 프레임의 옆걸음 후보 각도(도). 작은 각도부터 보고, 각 각도는 좌(+)·우(−) 순서다. */
		constexpr float BlockerDetourAngles[] = {45.f, 90.f};

		/** 같은 각도의 좌우 후보 진척(cm) 차이가 이 안이면 같다고 보고 좌(+)를 고른다. 부동소수 잡음으로 좌우가 번갈지 않게 한다. */
		constexpr double BlockerDetourTieTolerance = 0.5;

		/** 후보가 실제로 움직인 거리가 이번 걸음의 이 비율보다 작으면(벽에 막힘) 통과로 치지 않는다. */
		constexpr double BlockerDetourMinMoveRatio = 0.25;

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
		GBlockerDetours.store(0, std::memory_order_relaxed);
		UE_LOG(LogLootNPop, Display, TEXT("EnemyGround counters reset."));
	}

	void ReportGroundStats()
	{
		const uint64 Steps = GGroundSteps.load(std::memory_order_relaxed);
		const uint64 CacheHits = GCacheHits.load(std::memory_order_relaxed);
		const uint64 Fallbacks = GExactFallbacks.load(std::memory_order_relaxed);
		const uint64 ExactOnly = GExactOnlySteps.load(std::memory_order_relaxed);
		const uint64 BlockerRejects = GBlockerRejects.load(std::memory_order_relaxed);
		const uint64 BlockerDetours = GBlockerDetours.load(std::memory_order_relaxed);
		const double CacheRate = Steps > 0 ? 100.0 * static_cast<double>(CacheHits) / static_cast<double>(Steps) : 0.0;
		UE_LOG(LogLootNPop, Display,
			TEXT("EnemyGround: mode=%s steps=%llu cache_hits=%llu exact_fallbacks=%llu exact_only=%llu cache_rate=%.2f%% blocker_rejects=%llu blocker_detours=%llu"),
			GEnemySupportCache != 0 ? TEXT("cache-first") : TEXT("exact-only"),
			Steps, CacheHits, Fallbacks, ExactOnly, CacheRate, BlockerRejects, BlockerDetours);
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

	namespace
	{
		/**
		 * 캐시 접지가 Support 없는 Blocker 겹침으로 거부된 프레임에만 부른다(D-087). 이번 이동을 접평면에서 ±45°, ±90° 돌린
		 * 후보를 작은 각도부터 기존과 같은 올린 sweep → 캐시 접지 → Blocker 겹침 검사로 시험한다. 같은 각도에서 좌우가 모두
		 * 통과하면 원래 이동 방향 진척이 큰 쪽을 고른다. 좌우 후보는 대칭이라 진척은 대개 같다. 그때는 원래 목표 지점에서 각 쪽으로
		 * 캡슐 반지름만큼 비킨 자리가 Blocker와 겹치지 않는 쪽(Blocker가 먼저 끝나는 쪽)을, 그래도 가릴 수 없으면 좌(+)를 고른다.
		 * 상태를 남기지 않으므로 다음 프레임은 원래 방향부터 다시 본다.
		 */
		bool TryBlockerDetour(
			const FLNPSurfaceDataSnapshot& Snapshot,
			const ULNPMassWorldCollisionSubsystem& Collision,
			const LNPEnemyExactMovement::FParams& Params,
			const FVector& Location,
			const FVector& Velocity,
			const float DeltaTime,
			const FLNPSurfaceHandle& Surface,
			FLNPSurfaceHandle& OutSurface,
			FVector& OutLocation)
		{
			struct FCandidate
			{
				bool bPassed = false;
				double Progress = 0.0;
				FVector Location = FVector::ZeroVector;
				FLNPSurfaceHandle Surface;
			};

			const FVector Up = (Params.GravityOrigin - Location).GetSafeNormal();
			const FVector Planar = FVector::VectorPlaneProject(Velocity, Up);
			const FVector Heading = Planar.GetSafeNormal();
			const double MinMoveSq = FMath::Square(Planar.Size() * DeltaTime * BlockerDetourMinMoveRatio);
			constexpr float Signs[2] = {1.f, -1.f};
			for (const float Angle : BlockerDetourAngles)
			{
				FCandidate Candidates[2];
				for (int32 Side = 0; Side < 2; ++Side)
				{
					const FVector CandidateVelocity = FQuat(Up, FMath::DegreesToRadians(Signs[Side] * Angle)).RotateVector(Planar);
					const FVector Target = LNPEnemyExactMovement::MoveGroundedLaterally(Collision, Params, Location, CandidateVelocity, DeltaTime);
					FCandidate& Candidate = Candidates[Side];
					Candidate.Surface = Surface;
					if (!TryResolveCachedGround(Snapshot, Params, Target, Candidate.Surface, Candidate.Location)
						|| FVector::VectorPlaneProject(Candidate.Location - Location, Up).SizeSquared() < MinMoveSq
						|| LNPEnemyExactMovement::OverlapsSupportlessBlocker(Collision, Params, Candidate.Location))
					{
						continue;
					}
					Candidate.bPassed = true;
					Candidate.Progress = FVector::DotProduct(Candidate.Location - Location, Heading);
				}
				if (!Candidates[0].bPassed && !Candidates[1].bPassed)
				{
					continue;
				}

				int32 Pick = Candidates[0].bPassed ? 0 : 1;
				if (Candidates[0].bPassed && Candidates[1].bPassed)
				{
					const double ProgressDiff = Candidates[1].Progress - Candidates[0].Progress;
					if (ProgressDiff > BlockerDetourTieTolerance)
					{
						Pick = 1;
					}
					else if (ProgressDiff >= -BlockerDetourTieTolerance)
					{
						const FVector Ahead = Location + Planar * DeltaTime;
						const FVector Aside = FQuat(Up, UE_HALF_PI).RotateVector(Heading) * Params.CapsuleRadius;
						const bool bLeftClear = !LNPEnemyExactMovement::OverlapsSupportlessBlocker(Collision, Params, Ahead + Aside);
						const bool bRightClear = !LNPEnemyExactMovement::OverlapsSupportlessBlocker(Collision, Params, Ahead - Aside);
						Pick = (bRightClear && !bLeftClear) ? 1 : 0;
					}
				}
				OutLocation = Candidates[Pick].Location;
				OutSurface = Candidates[Pick].Surface;
				return true;
			}
			return false;
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
				// 접지시켜 캡슐이 Blocker 안에 놓인다. exact probe라면 Rejected였을 자리이므로 이 이동은 거부한다. 올린 sweep의
				// slide가 이 Blocker를 보지 못하므로 옆걸음 후보를 대신 시험하고(D-087), 모두 실패하면 제자리에 선다.
				// 이미 겹친 채 출발했다면(스폰·상태형 지형이 덮음) 빠져나갈 수 있게 막지 않는다.
				if (Target != Location
					&& LNPEnemyExactMovement::OverlapsSupportlessBlocker(Collision, Params, OutLocation)
					&& !LNPEnemyExactMovement::OverlapsSupportlessBlocker(Collision, Params, Location))
				{
					if (TryBlockerDetour(*SurfaceSnapshot, Collision, Params, Location, Velocity, DeltaTime,
						PreviousSurface, InOutSurface, OutLocation))
					{
						GBlockerDetours.fetch_add(1, std::memory_order_relaxed);
						return LNPEnemyExactMovement::EGroundResult::Grounded;
					}
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
