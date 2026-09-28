// Copyright (c) 2026 LootNPop. All rights reserved.

#include "SurfaceNavigation/LNPMassSpawnPlan.h"

#include "Math/RotationMatrix.h"

namespace
{
	constexpr float MinEnemySpacing = 200.0f;

	struct FWorldAuthoredAnchor
	{
		int32 Slot = INDEX_NONE;
		FGuid SpawnPointId;
		FName TargetSpawnSetId;
		FTransform Transform = FTransform::Identity;
		uint16 LocalLayerId = LNPSupportLayers::NoLayer;
		bool bUsed = false;
	};

	struct FWorldRandomCandidate
	{
		int32 Slot = INDEX_NONE;
		uint32 CandidateIndex = 0;
		FVector Position = FVector::ZeroVector;
		FVector Normal = FVector::ZeroVector;
		uint16 LocalLayerId = LNPSupportLayers::NoLayer;
		ELNPSpawnCandidateFlags Allowed = ELNPSpawnCandidateFlags::None;
		bool bUsed = false;
	};

	template<typename T>
	void Shuffle(TArray<T>& Values, FRandomStream& Random)
	{
		for (int32 Index = Values.Num() - 1; Index > 0; --Index)
		{
			Values.Swap(Index, Random.RandRange(0, Index));
		}
	}

	bool GuidLess(const FGuid& A, const FGuid& B)
	{
		if (A.A != B.A) return A.A < B.A;
		if (A.B != B.B) return A.B < B.B;
		if (A.C != B.C) return A.C < B.C;
		return A.D < B.D;
	}

	FQuat ToDoubleQuat(const FQuat4f& Value)
	{
		return FQuat(Value.X, Value.Y, Value.Z, Value.W);
	}

	FVector ToDoubleVector(const FVector3f& Value)
	{
		return FVector(Value.X, Value.Y, Value.Z);
	}

	bool IsFarEnough(const FVector& Position, TConstArrayView<FVector> Occupied, const float MinimumDistance)
	{
		if (MinimumDistance <= 0.0f)
		{
			return true;
		}
		const double MinimumDistanceSquared = FMath::Square(static_cast<double>(MinimumDistance));
		for (const FVector& Other : Occupied)
		{
			if (FVector::DistSquared(Position, Other) < MinimumDistanceSquared)
			{
				return false;
			}
		}
		return true;
	}

	FLNPSurfaceHandle MakeSurfaceHandle(const FLNPSurfaceDataSnapshot& Snapshot, const int32 Slot, const uint16 Layer)
	{
		FLNPSurfaceHandle Handle;
		Handle.OctantSlot = static_cast<uint16>(Slot);
		Handle.LocalLayerId = Layer;
		Handle.Generation = Snapshot.Generation;
		return Handle;
	}

	void AddAuthoredPod(
		const FLNPSurfaceDataSnapshot& Snapshot,
		const FLNPMassSpawnSetPlanInput& Set,
		const int32 SetInputIndex,
		FWorldAuthoredAnchor& Anchor,
		const ELNPPodPlacementSource Source,
		FLNPMassSpawnPlan& Plan,
		TArray<FVector>& OccupiedPods)
	{
		Anchor.bUsed = true;
		FLNPMassSpawnPlannedPod& Pod = Plan.Pods.AddDefaulted_GetRef();
		Pod.SetInputIndex = SetInputIndex;
		Pod.PodAssetIndex = Set.PodAssetIndex;
		Pod.Transform = Anchor.Transform;
		Pod.Surface = MakeSurfaceHandle(Snapshot, Anchor.Slot, Anchor.LocalLayerId);
		Pod.PlacementSource = Source;
		OccupiedPods.Add(Anchor.Transform.GetLocation());
	}

	void AddRandomPod(
		const FLNPSurfaceDataSnapshot& Snapshot,
		const FLNPMassSpawnSetPlanInput& Set,
		const int32 SetInputIndex,
		FWorldRandomCandidate& Candidate,
		FLNPMassSpawnPlan& Plan,
		TArray<FVector>& OccupiedPods)
	{
		Candidate.bUsed = true;
		FLNPMassSpawnPlannedPod& Pod = Plan.Pods.AddDefaulted_GetRef();
		Pod.SetInputIndex = SetInputIndex;
		Pod.PodAssetIndex = Set.PodAssetIndex;
		Pod.Transform = FTransform(FRotationMatrix::MakeFromZ(Candidate.Normal).ToQuat(), Candidate.Position);
		Pod.Surface = MakeSurfaceHandle(Snapshot, Candidate.Slot, Candidate.LocalLayerId);
		Pod.PlacementSource = ELNPPodPlacementSource::RandomCandidate;
		OccupiedPods.Add(Candidate.Position);
	}
}

bool LNPMassSpawnPlanning::BuildPlan(
	const FLNPSurfaceDataSnapshot& Snapshot,
	const TConstArrayView<FLNPMassSpawnSetPlanInput> Sets,
	const int32 WorldSeed,
	const float MinDistanceBetweenPods,
	const float EnemySpawnRadiusAroundPod,
	FLNPMassSpawnPlan& OutPlan,
	FString& OutError)
{
	OutPlan = FLNPMassSpawnPlan();
	OutError.Reset();
	if (Snapshot.Generation == 0 || Snapshot.Slots.Num() != 8)
	{
		OutError = TEXT("Mass spawn planning requires a published 8-slot SurfaceData snapshot.");
		return false;
	}
	if (!FMath::IsFinite(MinDistanceBetweenPods) || MinDistanceBetweenPods < 0.0f
		|| !FMath::IsFinite(EnemySpawnRadiusAroundPod) || EnemySpawnRadiusAroundPod < 0.0f)
	{
		OutError = TEXT("Mass spawn planning received invalid distance constraints.");
		return false;
	}

	TSet<FName> KnownSetIds;
	TArray<int32> SetOrder;
	OutPlan.SetStats.SetNum(Sets.Num());
	for (int32 SetIndex = 0; SetIndex < Sets.Num(); ++SetIndex)
	{
		const FLNPMassSpawnSetPlanInput& Set = Sets[SetIndex];
		if (Set.SpawnSetId.IsNone() || KnownSetIds.Contains(Set.SpawnSetId) || Set.RequestedPods < 0)
		{
			OutError = FString::Printf(TEXT("Mass spawn set %d has an empty/duplicate ID or negative request count."), SetIndex);
			return false;
		}
		KnownSetIds.Add(Set.SpawnSetId);
		SetOrder.Add(SetIndex);
		FLNPMassSpawnSetStats& Stats = OutPlan.SetStats[SetIndex];
		Stats.SpawnSetId = Set.SpawnSetId;
		Stats.Requested = Set.RequestedPods;
		for (const FLNPMassSpawnEnemyPlanInput& Enemy : Set.Enemies)
		{
			if (Enemy.RequestedCount < 0)
			{
				OutError = FString::Printf(TEXT("Mass spawn set '%s' has a negative enemy request count."), *Set.SpawnSetId.ToString());
				return false;
			}
		}
	}

	TArray<FWorldAuthoredAnchor> Authored;
	TArray<FWorldRandomCandidate> RandomCandidates;
	for (int32 SlotIndex = 0; SlotIndex < Snapshot.Slots.Num(); ++SlotIndex)
	{
		const FLNPSurfaceDataSlotSnapshot& Slot = Snapshot.Slots[SlotIndex];
		if (!Slot.Spawn.IsValid())
		{
			OutError = FString::Printf(TEXT("SurfaceData slot %d has no decoded Spawn stream."), SlotIndex);
			return false;
		}
		for (const FLNPSpawnAuthoredAnchor& Local : Slot.Spawn->AuthoredAnchors)
		{
			if (!Local.TargetSpawnSetId.IsNone() && !KnownSetIds.Contains(Local.TargetSpawnSetId))
			{
				OutError = FString::Printf(TEXT("Authored anchor %s references unknown SpawnSetId '%s'."),
					*Local.SpawnPointId.ToString(EGuidFormats::Short), *Local.TargetSpawnSetId.ToString());
				return false;
			}
			FWorldAuthoredAnchor& World = Authored.AddDefaulted_GetRef();
			World.Slot = SlotIndex;
			World.SpawnPointId = Local.SpawnPointId;
			World.TargetSpawnSetId = Local.TargetSpawnSetId;
			const FVector WorldLocation = FVector(Slot.SlotRotation.RotateVector(FVector3d(ToDoubleVector(Local.LocalTransform.GetLocation()))));
			const FQuat WorldRotation = FQuat(Slot.SlotRotation) * ToDoubleQuat(Local.LocalTransform.GetRotation());
			World.Transform = FTransform(WorldRotation, WorldLocation);
			World.LocalLayerId = Local.LocalLayerId;
		}
		for (const FLNPSpawnRandomCandidate& Local : Slot.Spawn->RandomCandidates)
		{
			FWorldRandomCandidate& World = RandomCandidates.AddDefaulted_GetRef();
			World.Slot = SlotIndex;
			World.CandidateIndex = Local.CandidateIndex;
			World.Position = FVector(Slot.SlotRotation.RotateVector(FVector3d(ToDoubleVector(Local.LocalPosition))));
			World.Normal = FVector(Slot.SlotRotation.RotateVector(FVector3d(ToDoubleVector(Local.LocalNormal)))).GetSafeNormal();
			World.LocalLayerId = Local.LocalLayerId;
			World.Allowed = Local.Allowed;
		}
	}

	Authored.Sort([](const FWorldAuthoredAnchor& A, const FWorldAuthoredAnchor& B)
	{
		return A.Slot != B.Slot ? A.Slot < B.Slot : GuidLess(A.SpawnPointId, B.SpawnPointId);
	});
	RandomCandidates.Sort([](const FWorldRandomCandidate& A, const FWorldRandomCandidate& B)
	{
		return A.Slot != B.Slot ? A.Slot < B.Slot : A.CandidateIndex < B.CandidateIndex;
	});
	SetOrder.Sort([&Sets](const int32 A, const int32 B)
	{
		return Sets[A].SpawnSetId.LexicalLess(Sets[B].SpawnSetId);
	});

	// authored anchor는 runtime 거리 검사로 버리지 않는다. 조립 뒤 위반은 명시적 실패다.
	for (int32 A = 0; A < Authored.Num(); ++A)
	{
		for (int32 B = A + 1; B < Authored.Num(); ++B)
		{
			if (FVector::DistSquared(Authored[A].Transform.GetLocation(), Authored[B].Transform.GetLocation())
				< FMath::Square(static_cast<double>(MinDistanceBetweenPods)))
			{
				OutError = FString::Printf(TEXT("World authored anchors %s and %s violate MinDistanceBetweenPods."),
					*Authored[A].SpawnPointId.ToString(EGuidFormats::Short), *Authored[B].SpawnPointId.ToString(EGuidFormats::Short));
				return false;
			}
		}
	}

	FRandomStream Random(WorldSeed ^ 0x51A7F00D);
	Shuffle(SetOrder, Random);
	Shuffle(Authored, Random);
	Shuffle(RandomCandidates, Random);

	TArray<FVector> OccupiedPods;
	for (const int32 SetInputIndex : SetOrder)
	{
		const FLNPMassSpawnSetPlanInput& Set = Sets[SetInputIndex];
		FLNPMassSpawnSetStats& Stats = OutPlan.SetStats[SetInputIndex];
		int32 Remaining = Set.RequestedPods;

		for (FWorldAuthoredAnchor& Anchor : Authored)
		{
			if (Remaining == 0 || Anchor.bUsed || Anchor.TargetSpawnSetId != Set.SpawnSetId)
			{
				continue;
			}
			AddAuthoredPod(Snapshot, Set, SetInputIndex, Anchor, ELNPPodPlacementSource::AuthoredForSet, OutPlan, OccupiedPods);
			++Stats.Authored;
			--Remaining;
		}
		for (FWorldAuthoredAnchor& Anchor : Authored)
		{
			if (Remaining == 0 || Anchor.bUsed || !Anchor.TargetSpawnSetId.IsNone())
			{
				continue;
			}
			AddAuthoredPod(Snapshot, Set, SetInputIndex, Anchor, ELNPPodPlacementSource::AuthoredGeneric, OutPlan, OccupiedPods);
			++Stats.Generic;
			--Remaining;
		}
		for (FWorldRandomCandidate& Candidate : RandomCandidates)
		{
			if (Remaining == 0)
			{
				break;
			}
			if (Candidate.bUsed || !EnumHasAnyFlags(Candidate.Allowed, ELNPSpawnCandidateFlags::Pod)
				|| !IsFarEnough(Candidate.Position, OccupiedPods, MinDistanceBetweenPods))
			{
				continue;
			}
			AddRandomPod(Snapshot, Set, SetInputIndex, Candidate, OutPlan, OccupiedPods);
			++Stats.Random;
			--Remaining;
		}
		Stats.Placed = Stats.Authored + Stats.Generic + Stats.Random;
		Stats.Shortfall = Set.RequestedPods - Stats.Placed;
	}

	for (FWorldAuthoredAnchor& Anchor : Authored)
	{
		if (Anchor.bUsed)
		{
			continue;
		}
		if (Anchor.TargetSpawnSetId.IsNone())
		{
			++OutPlan.UnusedGenericAnchors;
		}
		else
		{
			for (FLNPMassSpawnSetStats& Stats : OutPlan.SetStats)
			{
				if (Stats.SpawnSetId == Anchor.TargetSpawnSetId)
				{
					++Stats.UnusedAuthored;
					break;
				}
			}
		}
	}

	const double EnemyRadiusSquared = FMath::Square(static_cast<double>(EnemySpawnRadiusAroundPod));
	for (FLNPMassSpawnPlannedPod& Pod : OutPlan.Pods)
	{
		const FLNPMassSpawnSetPlanInput& Set = Sets[Pod.SetInputIndex];
		FLNPMassSpawnSetStats& Stats = OutPlan.SetStats[Pod.SetInputIndex];
		TArray<FVector> OccupiedEnemies;
		for (const FLNPMassSpawnEnemyPlanInput& EnemyInput : Set.Enemies)
		{
			Stats.EnemyRequested += EnemyInput.RequestedCount;
			FLNPMassSpawnPlannedEnemyGroup Group;
			Group.AssetIndex = EnemyInput.AssetIndex;
			for (FWorldRandomCandidate& Candidate : RandomCandidates)
			{
				if (Group.Transforms.Num() >= EnemyInput.RequestedCount)
				{
					break;
				}
				if (Candidate.bUsed || !EnumHasAnyFlags(Candidate.Allowed, ELNPSpawnCandidateFlags::Enemy)
					|| Candidate.Slot != Pod.Surface.OctantSlot || Candidate.LocalLayerId != Pod.Surface.LocalLayerId
					|| FVector::DistSquared(Candidate.Position, Pod.Transform.GetLocation()) > EnemyRadiusSquared
					|| !IsFarEnough(Candidate.Position, OccupiedEnemies, MinEnemySpacing))
				{
					continue;
				}
				Candidate.bUsed = true;
				Group.Transforms.Add(FTransform(FRotationMatrix::MakeFromZ(Candidate.Normal).ToQuat(), Candidate.Position));
				OccupiedEnemies.Add(Candidate.Position);
			}
			Stats.EnemyPlaced += Group.Transforms.Num();
			if (!Group.Transforms.IsEmpty())
			{
				Pod.Enemies.Add(MoveTemp(Group));
			}
		}
	}
	return true;
}
