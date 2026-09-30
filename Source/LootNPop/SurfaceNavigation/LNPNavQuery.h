// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "SurfaceNavigation/LNPSurfaceDataSubsystem.h"

/** ReachabilityGroup 조회 결과. group 숫자만으로는 유효성을 판단하지 않고 generation·version을 함께 비교한다. */
struct LOOTNPOP_API FLNPNavGroupRef
{
	uint32 Group = MAX_uint32;
	uint32 ConnectivityGraphVersion = 0;
	uint64 SnapshotGeneration = 0;

	bool IsValid() const { return Group != MAX_uint32 && ConnectivityGraphVersion != 0 && SnapshotGeneration != 0; }
};

enum class ELNPNavReachability : uint8
{
	Reachable,
	Unreachable,
	/** 어느 한쪽이 현재 snapshot generation 또는 ConnectivityGraphVersion과 다르다. 다시 조회해야 한다. */
	Stale,
};

struct LOOTNPOP_API FLNPNavProjection
{
	FLNPNavNodeRef Node;
	/** node의 world Support 지면점과 법선. */
	FVector3d Point = FVector3d::ZeroVector;
	FVector3f Normal = FVector3f::ZeroVector;
	double Distance = 0.0;
};

/**
 * 게시된 Surface snapshot 위의 Nav node 조회와 도달성 판정. 모든 함수는 immutable snapshot만 읽으므로 Mass worker에서 호출할 수 있다.
 * node 위치·법선은 codec에 저장하지 않으므로 대응 Support Layer를 node 방향에서 보간해 복원한다(`phases/Phase07a_NavDataFoundation.md` §3.3).
 */
namespace LNPNavQuery
{
	/** 지각 Nav 간격(200cm) 격자의 가장 먼 점까지 거리(약 115cm)와 dilation 가장자리 여유를 합한 기본 projection 반경. */
	constexpr double DefaultProjectionRadius = 300.0;

	/**
	 * Surface handle이 가리키는 slot·Layer에서 WorldPosition에 가장 가까운 walkable node를 MaxDistance 안에서 찾는다.
	 * 다른 Layer나 다른 slot으로 스냅하지 않으며 D-060으로 막힌 이음매 node는 제외한다. 거리는 node 지면점까지의 3D 거리다.
	 */
	LOOTNPOP_API bool ProjectToNode(
		const FLNPSurfaceDataSnapshot& Snapshot,
		const FVector3d& WorldPosition,
		const FLNPSurfaceHandle& Surface,
		double MaxDistance,
		FLNPNavProjection& OutProjection);

	/** node의 world Support 지면점과 법선. generation이 다르거나 node가 없으면 false다. */
	LOOTNPOP_API bool GetNodeSupport(
		const FLNPSurfaceDataSnapshot& Snapshot, const FLNPNavNodeRef& Node, FVector3d& OutPoint, FVector3f& OutNormal);

	/** runtime StaticNavComponent. 막힌 이음매 node와 stale ref는 false다. */
	LOOTNPOP_API bool GetStaticComponent(
		const FLNPSurfaceDataSnapshot& Snapshot, const FLNPNavNodeRef& Node, uint32& OutComponent);

	LOOTNPOP_API bool GetReachabilityGroup(
		const FLNPSurfaceDataSnapshot& Snapshot, const FLNPNavNodeRef& Node, FLNPNavGroupRef& OutGroup);

	/** 두 group이 모두 현재 generation·ConnectivityGraphVersion일 때만 Reachable/Unreachable을 판정한다. */
	LOOTNPOP_API ELNPNavReachability TestReachability(
		const FLNPNavSnapshot& Nav, const FLNPNavGroupRef& From, const FLNPNavGroupRef& To);
}
