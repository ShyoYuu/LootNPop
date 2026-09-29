// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "SurfaceNavigation/LNPCrustAtlas.h"

/** 옥탄트 삼각 격자의 6방향. 반대 방향은 값에 1을 xor한 쌍이다. */
enum class ELNPNavNeighbor : uint8
{
	IPositive = 0,
	INegative = 1,
	JPositive = 2,
	JNegative = 3,
	IPositiveJNegative = 4,
	INegativeJPositive = 5,
};

enum class ELNPNavCellFlags : uint8
{
	None = 0,
	Walkable = 1 << 0,
	NearStaticBlocker = 1 << 1,
	NeedsExact = 1 << 2,
};
ENUM_CLASS_FLAGS(ELNPNavCellFlags);

enum class ELNPNavPortalFlags : uint8
{
	None = 0,
	Bidirectional = 1 << 0,
};
ENUM_CLASS_FLAGS(ELNPNavPortalFlags);

/** Phase 7a의 단일 지상 agent bake profile. */
struct LOOTNPOP_API FLNPNavAgentProfile
{
	float Radius = 50.0f;
	float HalfHeight = 88.0f;
	float MaxStepUp = 45.0f;
	float MaxStepDown = 60.0f;
	float WalkableMinDot = 0.71f;
};

/** codec 안에서 쓰는 asset-local node 주소. */
struct LOOTNPOP_API FLNPLocalNavNodeRef
{
	uint16 LocalNavLayerId = MAX_uint16;
	uint16 TileId = MAX_uint16;
	uint16 LocalCellIndex = MAX_uint16;

	bool IsValid() const
	{
		return LocalNavLayerId != MAX_uint16 && TileId != MAX_uint16 && LocalCellIndex != MAX_uint16;
	}

	bool operator==(const FLNPLocalNavNodeRef& Other) const
	{
		return LocalNavLayerId == Other.LocalNavLayerId && TileId == Other.TileId
			&& LocalCellIndex == Other.LocalCellIndex;
	}
};

/** snapshot 게시 뒤의 runtime node 주소. generation은 Surface snapshot의 uint64 값을 그대로 쓴다. */
struct LOOTNPOP_API FLNPNavNodeRef
{
	uint16 RuntimeNavLayerId = MAX_uint16;
	uint16 TileId = MAX_uint16;
	uint16 LocalCellIndex = MAX_uint16;
	uint64 SnapshotGeneration = 0;

	bool IsValid() const
	{
		return RuntimeNavLayerId != MAX_uint16 && TileId != MAX_uint16
			&& LocalCellIndex != MAX_uint16 && SnapshotGeneration != 0;
	}
};

struct LOOTNPOP_API FLNPNavCell
{
	uint8 LocalCellIndex = MAX_uint8;
	/** ELNPNavNeighbor 비트. 상위 두 비트는 항상 0이다. */
	uint8 EdgeMask = 0;
	uint8 ClearanceClass = 0;
	ELNPNavCellFlags Flags = ELNPNavCellFlags::None;
	uint16 LocalStaticComponentId = MAX_uint16;
};

struct LOOTNPOP_API FLNPNavTile
{
	uint16 TileId = MAX_uint16;
	uint16 TileX = 0;
	uint16 TileY = 0;
	uint32 InitialRevision = 0;
	TArray<FLNPNavCell> Cells;
};

struct LOOTNPOP_API FLNPNavLayer
{
	uint16 LocalNavLayerId = MAX_uint16;
	uint16 LocalSupportLayerId = MAX_uint16;
	int32 Subdivisions = 0;
	TArray<FLNPNavTile> Tiles;
};

/** NavigationPayload codec v1의 decoded read model. */
struct LOOTNPOP_API FLNPNavData
{
	FLNPNavAgentProfile Agent;
	uint16 LocalStaticComponentCount = 0;
	TArray<FLNPNavLayer> Layers;
};

struct LOOTNPOP_API FLNPNavStaticComponent
{
	uint16 LocalStaticComponentId = MAX_uint16;
	uint32 NodeCount = 0;
};

struct LOOTNPOP_API FLNPNavPortal
{
	FLNPLocalNavNodeRef A;
	FLNPLocalNavNodeRef B;
	uint8 MinClearanceClass = 0;
	ELNPNavPortalFlags Flags = ELNPNavPortalFlags::Bidirectional;
};

struct LOOTNPOP_API FLNPNavSeamEndpoint
{
	FLNPLocalNavNodeRef Node;
	ELNPCrustSeamEdge Edge = ELNPCrustSeamEdge::X0;
	uint16 SeamStep = 0;
	uint8 MinClearanceClass = 0;
};

/** TraversalPayload codec v1의 decoded read model. */
struct LOOTNPOP_API FLNPNavTraversalData
{
	TArray<FLNPNavStaticComponent> StaticComponents;
	TArray<FLNPNavPortal> Portals;
	TArray<FLNPNavSeamEndpoint> SeamEndpoints;
};

namespace LNPNavData
{
	constexpr uint16 NavigationCodecVersion = 1;
	constexpr uint16 TraversalCodecVersion = 1;
	constexpr int32 TileSide = 16;
	constexpr int32 MaxCellsPerTile = TileSide * TileSide;
	constexpr uint8 NeighborMask = (1u << 6) - 1u;

	LOOTNPOP_API uint8 GetNeighborBit(ELNPNavNeighbor Neighbor);
	LOOTNPOP_API ELNPNavNeighbor GetOppositeNeighbor(ELNPNavNeighbor Neighbor);
	LOOTNPOP_API bool IsValidGridCoord(int32 Subdivisions, int32 I, int32 J);
	LOOTNPOP_API bool TryGetNeighborCoord(
		int32 Subdivisions, int32 I, int32 J, ELNPNavNeighbor Neighbor, FIntPoint& OutCoord);

	/** 격자 좌표를 16x16 Tile 주소로 바꾼다. uint16 Tile 좌표 범위를 넘으면 false다. */
	LOOTNPOP_API bool MakeTileAddress(int32 I, int32 J, uint16& OutTileX, uint16& OutTileY, uint8& OutLocalCellIndex);
	LOOTNPOP_API FIntPoint DecodeTileAddress(uint16 TileX, uint16 TileY, uint8 LocalCellIndex);

	LOOTNPOP_API const FLNPNavLayer* FindLayer(const FLNPNavData& Data, uint16 LocalNavLayerId);
	LOOTNPOP_API const FLNPNavCell* ResolveLocalNode(
		const FLNPNavData& Data, const FLNPLocalNavNodeRef& Node, FIntPoint* OutCoord = nullptr);
	LOOTNPOP_API bool MakeLocalNodeRef(
		const FLNPNavData& Data, uint16 LocalNavLayerId, int32 I, int32 J, FLNPLocalNavNodeRef& OutNode);
	LOOTNPOP_API bool IsCurrentNodeRef(const FLNPNavNodeRef& Node, uint64 SnapshotGeneration);

	/** 범위, canonical ID, tile 경계와 6방향 edge 대칭을 검사한다. */
	LOOTNPOP_API bool ValidateNavigation(const FLNPNavData& Data, FString& OutError);
	LOOTNPOP_API bool EncodeNavigation(const FLNPNavData& Data, TArray<uint8>& OutPayload, FString& OutError);
	LOOTNPOP_API bool DecodeNavigation(TConstArrayView<uint8> Payload, FLNPNavData& OutData, FString& OutError);

	/** component node 수, portal endpoint와 ordered seam 좌표를 Navigation data에 대조한다. */
	LOOTNPOP_API bool ValidateTraversal(
		const FLNPNavTraversalData& Traversal, const FLNPNavData& Navigation, FString& OutError);
	LOOTNPOP_API bool EncodeTraversal(
		const FLNPNavTraversalData& Traversal, const FLNPNavData& Navigation,
		TArray<uint8>& OutPayload, FString& OutError);
	LOOTNPOP_API bool DecodeTraversal(
		TConstArrayView<uint8> Payload, const FLNPNavData& Navigation,
		FLNPNavTraversalData& OutTraversal, FString& OutError);
}
