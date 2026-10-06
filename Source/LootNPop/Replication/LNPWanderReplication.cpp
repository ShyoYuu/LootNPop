// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Replication/LNPWanderReplication.h"
#include "Replication/LNPMassReplication.h"
#include "Enemy/LNPEnemySurfaceMovement.h"
#include "SurfaceNavigation/LNPSurfaceDataSubsystem.h"

FLNPReplicatedWanderData FLNPReplicatedWanderData::Encode(const FLNPEnemyWanderSegment& Segment,
	const bool bFlying, const FLNPSurfaceHandle& Surface, const float LocalYaw)
{
	FLNPReplicatedWanderData Data;
	const double Travel = (Segment.ArrivalTime - Segment.StartTime) * 1000.0;
	const double Hold = (Segment.StopUntil - Segment.ArrivalTime) * 1000.0;
	if (!Segment.bValid || Segment.Start.ContainsNaN() || Segment.Goal.ContainsNaN()
		|| !FMath::IsFinite(Segment.StartTime) || Segment.StartTime < 0.0
		|| !FMath::IsFinite(Travel) || !FMath::IsFinite(Hold) || Travel < 0.0 || Hold < 0.0
		|| Travel > MAX_uint16 || Hold > MAX_uint16
		|| Segment.Start.GetAbsMax() > MAX_int16 || Segment.Goal.GetAbsMax() > MAX_int16
		|| (!bFlying && !Segment.bCrawl && (!Surface.IsValid() || Surface.OctantSlot < 0 || Surface.OctantSlot >= 8)))
		return Data;
	Data.Flags = 1 | (bFlying ? 2 : Segment.bCrawl ? 32 : static_cast<uint8>(Surface.OctantSlot << 2));
	Data.StartX = FMath::RoundToInt32(Segment.Start.X);
	Data.StartY = FMath::RoundToInt32(Segment.Start.Y);
	Data.StartZ = FMath::RoundToInt32(Segment.Start.Z);
	Data.GoalX = FMath::RoundToInt32(Segment.Goal.X);
	Data.GoalY = FMath::RoundToInt32(Segment.Goal.Y);
	Data.GoalZ = FMath::RoundToInt32(Segment.Goal.Z);
	Data.StartMs = static_cast<uint32>(static_cast<uint64>(FMath::RoundToInt64(Segment.StartTime * 1000.0)) & MAX_uint32);
	Data.TravelMs = FMath::RoundToInt32(Travel);
	Data.HoldMs = FMath::RoundToInt32(Hold);
	Data.Layer = bFlying || Segment.bCrawl ? 0 : Surface.LocalLayerId;
	Data.Yaw = static_cast<int16>(FMath::Clamp(FMath::RoundToInt32(FMath::UnwindRadians(LocalYaw) * MAX_int16 / UE_PI), -MAX_int16, MAX_int16));
	return Data;
}

double FLNPReplicatedWanderData::DecodeStartTime(const double ServerNow) const
{
	const uint64 NowMs = static_cast<uint64>(FMath::Max(0.0, ServerNow) * 1000.0);
	const uint32 Delta = StartMs - static_cast<uint32>(NowMs);
	const int64 SignedDelta = Delta <= MAX_int32 ? static_cast<int64>(Delta) : static_cast<int64>(Delta) - (1LL << 32);
	return (static_cast<double>(NowMs) + SignedDelta) / 1000.0;
}

void FLNPReplicatedWanderData::ApplyTo(FLNPEnemyWanderSegment& Segment, const double ServerNow) const
{
	Segment.Start = FVector(StartX, StartY, StartZ);
	Segment.Goal = FVector(GoalX, GoalY, GoalZ);
	Segment.StartTime = DecodeStartTime(ServerNow);
	Segment.ArrivalTime = Segment.StartTime + TravelMs / 1000.0;
	Segment.StopUntil = Segment.ArrivalTime + HoldMs / 1000.0;
	Segment.LocalYaw = static_cast<float>(Yaw) * UE_PI / MAX_int16;
	Segment.bValid = IsEnabled();
	Segment.bCrawl = IsCrawling();
}

bool FLNPReplicatedWanderData::IsMoving(const double ServerNow) const
{
	return IsEnabled() && TravelMs > 0 && (StartX != GoalX || StartY != GoalY || StartZ != GoalZ)
		&& ServerNow >= DecodeStartTime(ServerNow) && ServerNow < DecodeStartTime(ServerNow) + TravelMs / 1000.0;
}

bool FLNPReplicatedWanderData::Evaluate(const double ServerNow, const FLNPSurfaceDataSnapshot* Snapshot,
	const LNPEnemyExactMovement::FParams& Params, FTransform& OutTransform, const uint16 SurfaceNormal) const
{
	if (!IsEnabled())
		return false;
	FLNPEnemyWanderSegment Segment;
	ApplyTo(Segment, ServerNow);
	FVector Position = Segment.Evaluate(ServerNow);
	if (!IsFlying() && !IsCrawling())
	{
		if (!Snapshot)
			return false;
		FLNPSurfaceHandle Surface{ static_cast<int8>((Flags >> 2) & 7), Layer, Snapshot->Generation };
		if (!LNPEnemySurfaceMovement::TryResolveCachedGround(*Snapshot, Params, Position, Surface, Position))
			return false;
	}
	// 벽보행은 구간 내내 붙은 면의 법선이 Up이다. 법선이 없으면(0) 구 중심 방향으로 둔다.
	const uint16 Normal = IsCrawling() ? SurfaceNormal : 0;
	const FVector Up = Normal != 0 ? LNP::Replication::DecodeOctNormal(Normal) : (Params.GravityOrigin - Position).GetSafeNormal();
	FVector Forward = FVector::VectorPlaneProject(Segment.Goal - Segment.Start, Up).GetSafeNormal();
	const FQuat Rotation = Forward.IsNearlyZero()
		? LNP::Replication::DecodeReplicatedRotation(Position, Normal, static_cast<float>(Yaw) * UE_PI / MAX_int16)
		: FRotationMatrix::MakeFromXZ(Forward, Up).ToQuat();
	OutTransform.SetLocation(Position);
	OutTransform.SetRotation(Rotation);
	return true;
}
