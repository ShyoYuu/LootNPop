// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "SurfaceNavigation/LNPSurfaceBakeGeometry.h"

class USceneComponent;
class UStaticMeshComponent;
class UWorld;

/**
 * 옥탄트 LVI의 Support 역할 component에서 베이커 입력 삼각형을 추출한다.
 * 삼각형 원본은 exact query가 맞히는 cooked Chaos triangle mesh다(`design/SurfaceBaking.md` "삼각형 원본").
 * Terrain Contract 검증(FLNPOctantSourceCollector)을 통과한 World를 입력으로 가정한다.
 */
class LOOTNPOPEDITOR_API FLNPOctantTriangleExtractor
{
public:
	/**
	 * component의 cooked Chaos trimesh를 component transform으로 옮긴다. 음수 scale이면 winding을 뒤집어
	 * 앞면 법선을 유지한다. exact query는 bTraceComplex=false라서 complex-as-simple이 아닌 mesh는
	 * exact가 다른 shape를 맞히므로 오류다.
	 */
	static bool ExtractComponent(
		const UStaticMeshComponent& Component,
		FLNPBakeTriangleMesh& OutMesh,
		FString& OutError);

	/** source Level 좌표계의 component transform. 등록되지 않은 component는 부착 체인의 relative transform을 합성한다. */
	static FTransform GetSourceTransform(const USceneComponent& Component);

	/** persistent Level의 Support 역할 component를 모두 추출해 component 경로 순으로 돌려준다. */
	static bool ExtractSupportSources(
		const UWorld& SourceWorld,
		TArray<FLNPBakeSupportSource>& OutSources,
		FString& OutError);
};
