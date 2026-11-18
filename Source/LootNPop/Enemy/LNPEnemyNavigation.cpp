// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Enemy/LNPEnemyNavigation.h"
#include "Enemy/LNPEnemyMassTypes.h"
#include "Enemy/LNPEnemySurfaceMovement.h"
#include "SurfaceNavigation/LNPHitIdentityRegistry.h"
#include "SurfaceNavigation/LNPSurfaceDataSubsystem.h"

bool LNPEnemyNavigation::BorrowBlockerFloorSurface(const FLNPExactHitIdentity& Identity, const FLNPSurfaceHandle& Previous,
	const uint64 SnapshotGeneration, FLNPSurfaceHandle& OutSurface)
{
	const bool bStaticBlocker = Identity.Lifetime == ELNPExactSourceLifetime::Static
		&& (Identity.Roles & ELNPExactSourceRole::Blocker) != 0
		&& (Identity.Roles & ELNPExactSourceRole::Support) == 0
		&& Identity.Slot >= 0;
	if (!bStaticBlocker || !Previous.IsValid() || Previous.OctantSlot != static_cast<uint16>(Identity.Slot)
		|| Previous.Generation != SnapshotGeneration)
	{
		return false;
	}
	OutSurface = Previous;
	return true;
}

bool LNPEnemyNavigation::UpdateMeleeSlot(const FLNPNavSnapshot& Nav, const ELNPNavEndpointStatus Status,
	const FMassEntityHandle Player, const FLNPNavGroupRef& GroundGroup, const FLNPSurfaceHandle& GroundSurface,
	const bool bGrounded, const bool bOccupied, const double Now, FLNPEnemySlotReachability& State)
{
	// 땅 위인데 투영 node도 보행 anchor도 없는 현재 물리 바닥(바위 주머니·절벽 아래)은 판정 불가가 아니라 도달 불가다(D-091).
	// 판정 불가로 두면 이미 슬롯을 가진 근접 적이 기한 없이 슬롯을 쥐고 입구에서 막혀 선다.
	// 공중이면 Nav 소속이 없어 아래에서 슬롯만 보존하고 유예 시계는 되돌리지 않는다(주머니 안에서 뛰어 슬롯을 쥐지 못한다).
	const bool bOffNav = bGrounded && !GroundGroup.IsValid() && GroundSurface.IsValid()
		&& GroundSurface.Generation == Nav.SnapshotGeneration;
	if (Status == ELNPNavEndpointStatus::Stale || (!bOffNav && (!GroundGroup.IsValid()
		|| LNPNavQuery::TestReachability(Nav, GroundGroup, GroundGroup) == ELNPNavReachability::Stale)))
	{
		return bOccupied;
	}
	if (!bGrounded)
	{
		State.UnreachableSince = -1.0;
		return bOccupied;
	}
	if (State.Player != Player || State.GroundGroup.Group != GroundGroup.Group
		|| State.GroundGroup.SnapshotGeneration != GroundGroup.SnapshotGeneration
		|| State.GroundGroup.ConnectivityGraphVersion != GroundGroup.ConnectivityGraphVersion)
	{
		State.Player = Player;
		State.GroundGroup = GroundGroup;
		State.UnreachableSince = -1.0;
	}
	if (!bOffNav && Status == ELNPNavEndpointStatus::Reachable)
	{
		State.UnreachableSince = -1.0;
		return true;
	}
	if (!bOccupied)
	{
		return false;
	}
	if (State.UnreachableSince < 0.0)
	{
		State.UnreachableSince = Now;
	}
	return Now - State.UnreachableSince < 1.5;
}

ELNPNavEndpointStatus LNPEnemyNavigation::SelectHomePod(const FLNPNavSnapshot& Nav,
	const FVector3d& Feet, const FLNPSurfaceHandle& Surface, const FMassEntityHandle Parent,
	const TConstArrayView<FLNPEnemyHomePod> Pods, int32& OutPod)
{
	OutPod = INDEX_NONE;
	if (!Surface.IsValid() || Surface.Generation != Nav.SnapshotGeneration)
	{
		return ELNPNavEndpointStatus::Stale;
	}
	double BestDistance = DBL_MAX;
	for (int32 Index = 0; Index < Pods.Num(); ++Index)
	{
		const FLNPEnemyHomePod& Pod = Pods[Index];
		if (LNPNavQuery::TestReachability(Nav, Pod.Group, Pod.Group) == ELNPNavReachability::Stale)
		{
			return ELNPNavEndpointStatus::Stale;
		}
		FLNPNavEndpointQuery Query;
		Query.StartPosition = Feet;
		Query.StartSurface = &Surface;
		Query.GoalPosition = Pod.Position;
		Query.GoalSurface = &Pod.Surface;
		Query.ApproachRadius = 0.0;
		const FLNPNavEndpoints Ends = LNPNavGraph::ResolveEndpoints(Nav, Query);
		if (Ends.Status == ELNPNavEndpointStatus::Stale)
		{
			return Ends.Status;
		}
		if (Ends.Status != ELNPNavEndpointStatus::Reachable)
		{
			continue;
		}
		if (Pod.Entity == Parent)
		{
			OutPod = Index;
			return ELNPNavEndpointStatus::Reachable;
		}
		const double Distance = FVector3d::DistSquared(Feet, Pod.Position);
		if (Distance < BestDistance || (Distance == BestDistance && OutPod != INDEX_NONE
			&& Pod.Entity.Index < Pods[OutPod].Entity.Index))
		{
			BestDistance = Distance;
			OutPod = Index;
		}
	}
	return OutPod != INDEX_NONE ? ELNPNavEndpointStatus::Reachable : ELNPNavEndpointStatus::Unreachable;
}

bool LNPEnemyNavigation::WalkReaches(const FLNPSurfaceDataSnapshot& Snapshot, const ULNPMassWorldCollisionSubsystem& Collision,
	const LNPEnemyExactMovement::FParams& Params, const FVector& StartCenter, const FLNPSurfaceHandle& Surface,
	const FVector& GoalFeet, const double AcceptRadius, FWalkProbe* Probe)
{
	using EEnd = FWalkProbe::EEnd;
	auto Finish = [Probe](const EEnd End, const bool bResult)
	{
		if (Probe) { Probe->End = End; }
		return bResult;
	};
	if (!Surface.IsValid())
	{
		return Finish(EEnd::NoSurface, false);
	}
	// 한 걸음은 캡슐 반지름이다. 접지 sweep은 연속 판정이라 걸음 사이의 벽을 건너뛰지 않는다.
	const bool bProbeStride = Probe && Probe->StepLength > 0.0 && Probe->StepSeconds > 0.f;
	const float StepSeconds = bProbeStride ? Probe->StepSeconds : 1.f / 30.f;
	const double StepLength = bProbeStride ? Probe->StepLength : FMath::Max(static_cast<double>(Params.CapsuleRadius), 10.0);
	auto FeetAt = [&Params](const FVector& Center)
	{
		return Center - (Params.GravityOrigin - Center).GetSafeNormal() * Params.CapsuleHalfHeight;
	};
	const int32 MaxSteps = FMath::CeilToInt(FVector::Distance(FeetAt(StartCenter), GoalFeet) * 1.5 / StepLength) + 2;
	FVector Location = StartCenter;
	FLNPSurfaceHandle Walking = Surface;
	for (int32 Step = 0; Step <= MaxSteps; ++Step)
	{
		const FVector Up = (Params.GravityOrigin - Location).GetSafeNormal();
		const FVector Delta = GoalFeet - FeetAt(Location);
		const FVector Planar = FVector::VectorPlaneProject(Delta, Up);
		const double PlanarDistance = Planar.Size();
		if (Probe && Probe->FeetPath)
		{
			Probe->FeetPath->Add(FeetAt(Location));
		}
		if (Probe)
		{
			Probe->Steps = Step;
			Probe->EndFeet = FeetAt(Location);
			if (Delta.Size() < Probe->Best3D)
			{
				Probe->Best3D = Delta.Size();
				Probe->BestPlanar = PlanarDistance;
				Probe->BestHeight = FVector::DotProduct(Delta, Up);
			}
		}
		// 접평면으로 닿아도 높이가 크게 다르면(다른 층·절벽 위아래) 닿은 것이 아니다.
		if (PlanarDistance <= AcceptRadius
			&& FMath::Abs(FVector::DotProduct(Delta, Up)) <= AcceptRadius + Params.MaxStepUp)
		{
			return Finish(EEnd::Reached, true);
		}
		if (Step == MaxSteps)
		{
			break;
		}
		const FVector Velocity = Planar.GetSafeNormal() * (FMath::Min(StepLength, PlanarDistance) / StepSeconds);
		FVector Next;
		FVector PhysVelocity;
		const LNPEnemyExactMovement::EGroundResult Result = LNPEnemySurfaceMovement::StepGrounded(
			&Snapshot, Collision, Params, Location, Velocity, StepSeconds, Next, PhysVelocity, Walking);
		if (Probe) { Probe->LastGround = Result; }
		if (Result != LNPEnemyExactMovement::EGroundResult::Grounded)
		{
			return Finish(EEnd::NotGrounded, false);
		}
		if (!Walking.IsValid() || Walking.OctantSlot != Surface.OctantSlot || Walking.LocalLayerId != Surface.LocalLayerId)
		{
			return Finish(EEnd::LayerChanged, false);
		}
		// 벽에 막혀 제자리걸음이면 실패다.
		if (FVector::DistSquared(Next, Location) < 1.0)
		{
			return Finish(EEnd::Stuck, false);
		}
		Location = Next;
	}
	return Finish(EEnd::MaxSteps, false);
}

int32 LNPEnemyNavigation::FindWalkAnchor(const FLNPSurfaceDataSnapshot& Snapshot, const ULNPMassWorldCollisionSubsystem& Collision,
	const LNPEnemyExactMovement::FParams& Params, const FVector& Center, const FLNPSurfaceHandle& Surface,
	const FLNPNavOverlay* Overlay, const bool bWalkFromNode, const double AcceptRadius, TArray<FVector>* OutFeetPath)
{
	const FLNPNavSnapshot& Nav = Snapshot.Nav;
	if (!Nav.Graph.IsValid() || !Surface.IsValid() || Surface.Generation != Nav.SnapshotGeneration)
	{
		return INDEX_NONE;
	}
	const FVector Feet = Center - (Params.GravityOrigin - Center).GetSafeNormal() * Params.CapsuleHalfHeight;
	TArray<FLNPNavGraphCandidate> Candidates;
	LNPNavGraph::CollectNodesNear(Nav, Surface.OctantSlot, Surface.LocalLayerId, FVector3d(Feet), WalkAnchorSearchRadius,
		Candidates, Overlay);
	const int32 Count = FMath::Min(Candidates.Num(), WalkAnchorMaxCandidates);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const int32 Node = Candidates[Index].Node;
		const FVector Point(Nav.Graph.GetWorldPoint(Nav.Graph.GetSlot(Node), Node));
		FWalkProbe Probe;
		Probe.FeetPath = OutFeetPath;
		if (OutFeetPath)
		{
			OutFeetPath->Reset();
		}
		const bool bWalkable = bWalkFromNode
			? WalkReaches(Snapshot, Collision, Params,
				Point + (Params.GravityOrigin - Point).GetSafeNormal() * Params.CapsuleHalfHeight, Surface, Feet, AcceptRadius, &Probe)
			: WalkReaches(Snapshot, Collision, Params, Center, Surface, Point, AcceptRadius, &Probe);
		if (bWalkable)
		{
			return Node;
		}
	}
	return INDEX_NONE;
}

LNPEnemyNavigation::EPathShortcut LNPEnemyNavigation::CheckPathShortcut(const FLNPSurfaceDataSnapshot& Snapshot,
	const ULNPMassWorldCollisionSubsystem& Collision, const LNPEnemyExactMovement::FParams& Params, const FVector& StartCenter,
	const FLNPSurfaceHandle& Surface, const FVector& CurrentWaypoint, const FVector& Candidate)
{
	const FVector Feet = StartCenter - (Params.GravityOrigin - StartCenter).GetSafeNormal() * Params.CapsuleHalfHeight;
	const double Direct = FVector::Distance(Feet, Candidate);
	if (Direct > PathShortcutMaxDistance)
	{
		return EPathShortcut::TooFar;
	}
	if (FVector::Distance(Feet, CurrentWaypoint) + FVector::Distance(CurrentWaypoint, Candidate) - Direct < PathShortcutMinSaving)
	{
		return EPathShortcut::LowGain;
	}
	return WalkReaches(Snapshot, Collision, Params, StartCenter, Surface, Candidate, PathShortcutAccept)
		? EPathShortcut::Walkable : EPathShortcut::Blocked;
}

void LNPEnemyNavigation::UpdatePlayerTrail(const FLNPSurfaceDataSnapshot& Snapshot, const ULNPMassWorldCollisionSubsystem& Collision,
	const LNPEnemyExactMovement::FParams& Params, const FVector& Feet, const FLNPSurfaceHandle& Surface, const bool bOnNav,
	const int32 OpenNode, const bool bLanded, FLNPPlayerTrail& Trail)
{
	const FLNPNavSnapshot& Nav = Snapshot.Nav;
	auto Break = [&Trail, &Feet](const FLNPPlayerTrail::EBreak Reason)
	{
		Trail.LastBreak = Reason;
		Trail.LastBreakFeet = Feet;
		++Trail.Breaks;
		Trail.bBroken = true;
		Trail.Count = 0;
		++Trail.Revision;
	};
	auto SameLayer = [](const FLNPSurfaceHandle& A, const FLNPSurfaceHandle& B)
	{
		return A.IsValid() && B.IsValid() && A.Generation == B.Generation && A.OctantSlot == B.OctantSlot && A.LocalLayerId == B.LocalLayerId;
	};
	if (bOnNav)
	{
		// Nav로 돌아오면 지난 발자취는 버린다. 다음에 벗어날 때 시작할 트인 자리를 기억한다.
		if (Trail.Count > 0 || Trail.bBroken)
		{
			Trail.Count = 0;
			Trail.bBroken = false;
			++Trail.Revision;
		}
		if (OpenNode != INDEX_NONE)
		{
			Trail.OpenNode = OpenNode;
			Trail.OpenFeet = Feet;
			Trail.OpenSurface = Surface;
		}
		return;
	}
	if (Trail.bBroken)
	{
		return;
	}
	if (Trail.Count == 0)
	{
		// 마지막으로 트인 삼각형의 발은 그 삼각형 꼭짓점 node에서 곧게 걸을 수 있다(D-090 통로 기준). 거기서부터 걸어 들어간 길만 잇는다.
		const int32 OpenSlot = Nav.Graph.GetSlot(Trail.OpenNode);
		if (OpenSlot == INDEX_NONE || !SameLayer(Trail.OpenSurface, Surface) || Surface.Generation != Nav.SnapshotGeneration
			|| FVector::DistSquared(Trail.OpenFeet, Feet) > FMath::Square(TrailStartMaxDistance))
		{
			Break(FLNPPlayerTrail::EBreak::Start);
			return;
		}
		Trail.Points[0] = FVector(Nav.Graph.GetWorldPoint(OpenSlot, Trail.OpenNode));
		Trail.Points[1] = Trail.OpenFeet;
		Trail.Count = 2;
		Trail.CheckedFeet = Trail.OpenFeet;
		Trail.EntryNode = Trail.OpenNode;
		Trail.Surface = Surface;
		++Trail.Revision;
	}
	if (!SameLayer(Trail.Surface, Surface))
	{
		Break(FLNPPlayerTrail::EBreak::Layer);
		return;
	}
	// 앞선 점 가까이로 돌아왔으면 그 뒤를 지운다. 주머니 안을 오가도 점이 상한까지 쌓이지 않고 길도 짧아진다.
	for (int32 Index = 1; Index < Trail.Count - 1; ++Index)
	{
		if (FVector::DistSquared(Trail.Points[Index], Feet) <= FMath::Square(TrailAccept))
		{
			Trail.Count = Index + 1;
			Trail.CheckedFeet = Trail.Points[Index];
			++Trail.Revision;
			return;
		}
	}
	// 마지막 점에서 지금 발까지의 직선을 TrailCheckStep마다 걸어 본다. 통과한 자리는 CheckedFeet로 기억한다.
	// 플레이어가 모서리를 곡선으로 돌면 100cm 직선이 벽에 막힌다(PIE: 걷기만 해도 21번 끊김). 그때는 마지막으로 통과한 자리를
	// 점으로 남기고 그 자리에서 다시 본다. 거기서도 막히면 적이 못 가는 길이다(턱·틈·점프).
	// 착지는 거리와 무관하게 본다. 절벽 낙하·그래플·런처·적이 못 오르는 단차가 이 구간에서 끊긴다.
	if (!bLanded && FVector::DistSquared(Trail.CheckedFeet, Feet) < FMath::Square(TrailCheckStep))
	{
		return;
	}
	auto Walks = [&](const FVector& From)
	{
		const FVector Up = (Params.GravityOrigin - From).GetSafeNormal();
		return WalkReaches(Snapshot, Collision, Params, From + Up * Params.CapsuleHalfHeight, Surface, Feet, TrailAccept);
	};
	auto Append = [&Trail](const FVector& Point)
	{
		if (Trail.Count >= FLNPPlayerTrail::MaxPoints)
		{
			return false;
		}
		Trail.Points[Trail.Count++] = Point;
		Trail.CheckedFeet = Point;
		return true;
	};
	const FVector Last = Trail.Points[Trail.Count - 1];
	if (!Walks(Last))
	{
		const bool bHasChecked = FVector::DistSquared(Trail.CheckedFeet, Last) >= FMath::Square(TrailAccept);
		if (!bHasChecked)
		{
			Break(FLNPPlayerTrail::EBreak::Walk);
			return;
		}
		if (!Append(Trail.CheckedFeet))
		{
			Break(FLNPPlayerTrail::EBreak::Cap);
			return;
		}
		if (!Walks(Trail.CheckedFeet))
		{
			Break(FLNPPlayerTrail::EBreak::Walk);
			return;
		}
	}
	Trail.CheckedFeet = Feet;
	if (FVector::DistSquared(Trail.Points[Trail.Count - 1], Feet) >= FMath::Square(TrailSpacing) && !Append(Feet))
	{
		Break(FLNPPlayerTrail::EBreak::Cap);
	}
}

int32 LNPEnemyNavigation::BuildAnchorPath(const FVector& NodePoint, const TConstArrayView<FVector> WalkedFeet,
	const TArrayView<FVector> OutPoints)
{
	if (OutPoints.IsEmpty())
	{
		return 0;
	}
	int32 Count = 0;
	OutPoints[Count++] = NodePoint;
	for (const FVector& Step : WalkedFeet)
	{
		if (Count < OutPoints.Num() && FVector::DistSquared(Step, OutPoints[Count - 1]) >= FMath::Square(AnchorPathSpacing))
		{
			OutPoints[Count++] = Step;
		}
	}
	return Count;
}

int32 LNPEnemyNavigation::SyncTail(const TConstArrayView<FVector> Points, const FVector& Feet, const FVector& Up)
{
	for (int32 Index = Points.Num() - 1; Index >= 0; --Index)
	{
		if (FVector::VectorPlaneProject(Points[Index] - Feet, Up).SizeSquared() <= FMath::Square(TailSyncRadius))
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

bool LNPEnemyNavigation::FollowTail(const TConstArrayView<FVector> Points, const FVector& GoalFeet, const FVector& Feet,
	const FVector& Up, const double Now, int32& Index, double& BestDistance, double& ProgressTime, FVector& OutSteer)
{
	auto PointAt = [&](const int32 At) { return Points.IsValidIndex(At) ? Points[At] : GoalFeet; };
	auto Planar = [&](const FVector& Point) { return FVector::VectorPlaneProject(Point - Feet, Up).Size(); };
	Index = FMath::Clamp(Index, 0, Points.Num());
	for (int32 At = Points.Num() - 1; At >= Index; --At)
	{
		if (Planar(Points[At]) <= TailArrival)
		{
			Index = At + 1;
			BestDistance = DBL_MAX;
			ProgressTime = Now;
			break;
		}
	}
	const double Distance = Planar(PointAt(Index));
	if (Distance < BestDistance - TailStallProgress)
	{
		BestDistance = Distance;
		ProgressTime = Now;
	}
	else if (Now - ProgressTime >= TailStallSeconds)
	{
		// 검증된 선을 벗어나 막혔다(벽을 따라 미끄러짐·분리력). 지나온 점으로 돌아가 거기서 다시 따라간다.
		// 머리 node에서도 막혔으면 곧게 돌아갈 점이 없다(PIE: 옆 자리에서 node로 가는 첫 걸음부터 거부).
		if (Index == 0)
		{
			OutSteer = PointAt(0);
			return false;
		}
		--Index;
		BestDistance = DBL_MAX;
		ProgressTime = Now;
	}
	OutSteer = PointAt(Index);
	return true;
}
