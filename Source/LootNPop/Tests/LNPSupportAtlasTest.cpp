// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Algo/Reverse.h"

#include "SurfaceNavigation/LNPCrustAtlas.h"
#include "SurfaceNavigation/LNPSupportAtlas.h"
#include "SurfaceNavigation/LNPSupportLayers.h"
#include "SurfaceNavigation/LNPSurfaceBakeGeometry.h"
#include "SurfaceNavigation/LNPNavBaking.h"
#include "SurfaceNavigation/LNPNavRuntime.h"
#include "SurfaceNavigation/LNPNavPathfinding.h"

namespace LNPSupportAtlasTest
{
	constexpr double Radius = 30000.0;
	/** 지각 N=200(옥탄트 중심 간격 약 367cm), Layer m=2(약 184cm, 0.35°). */
	constexpr int32 CrustSubdivisions = 200;
	constexpr int32 LayerMultiplier = 2;
	constexpr int32 PatchSteps = 12;
	/** 지각 패치는 넓어서 잘게 나눈다(방위 약 1°, sagitta 약 1cm). */
	constexpr int32 CrustPatchSteps = 48;

	/** 합성 장면 Layer ID(source Key 오름차순, 같은 source 안은 최소 external face 순). */
	constexpr uint16 CrustLayer = 0;
	constexpr uint16 LowLayer = 1;
	constexpr uint16 MidLayer = 2;
	constexpr uint16 TopLayer = 3;
	constexpr uint16 StairBaseLayer = 6;
	constexpr uint16 StairStepLayer = 7;

	/** 3층 겹침 중심과 캡 반폭(위도·방위 도). 위 캡일수록 작다. */
	constexpr double StackLat = 30.0;
	constexpr double StackAz = 45.0;
	constexpr double TopHalf = 3.0;
	constexpr double MidHalf = 5.0;
	constexpr double LowHalf = 7.0;

	/** 섬 윗면 위 30cm 계단. */
	constexpr double StairLat = 25.0;
	constexpr double StairAz = 27.0;
	constexpr double StairBaseHalf = 3.0;
	constexpr double StairStepHalf = 1.2;
	constexpr double StairHeight = 30.0;

	FVector3d Direction(double LatDeg, double AzDeg)
	{
		const double Lat = FMath::DegreesToRadians(LatDeg);
		const double Az = FMath::DegreesToRadians(AzDeg);
		return FVector3d(FMath::Cos(Lat) * FMath::Cos(Az), FMath::Cos(Lat) * FMath::Sin(Az), FMath::Sin(Lat));
	}

	/** 삼각형 하나를 더한다. 앞면이 구 중심(지역 Up)을 향하도록 winding을 고른다. */
	void AddInwardTriangle(FLNPBakeTriangleMesh& Mesh, const FVector3d& A, const FVector3d& B, const FVector3d& C)
	{
		const FVector3d Normal = FVector3d::CrossProduct(B - A, C - A);
		const bool bFacesCenter = FVector3d::DotProduct(Normal, -(A + B + C)) > 0.0;
		const int32 Base = Mesh.Vertices.Add(A);
		Mesh.Vertices.Add(bFacesCenter ? B : C);
		Mesh.Vertices.Add(bFacesCenter ? C : B);
		Mesh.Triangles.Emplace(Base, Base + 1, Base + 2);
		Mesh.ExternalFaceIndices.Add(Mesh.ExternalFaceIndices.Num());
	}

	/** 반지름 R의 위도·방위 사각 구면 패치. 삼각형은 평면이라 가운데가 구면보다 sagitta만큼 안쪽이다. */
	void AppendPatch(FLNPBakeTriangleMesh& Mesh, double R, double Lat, double Az, double HalfLat, double HalfAz, int32 Steps = PatchSteps)
	{
		auto Point = [&](int32 I, int32 J)
		{
			return Direction(Lat - HalfLat + 2.0 * HalfLat * J / Steps, Az - HalfAz + 2.0 * HalfAz * I / Steps) * R;
		};
		for (int32 J = 0; J < Steps; ++J)
		{
			for (int32 I = 0; I < Steps; ++I)
			{
				AddInwardTriangle(Mesh, Point(I, J), Point(I + 1, J), Point(I + 1, J + 1));
				AddInwardTriangle(Mesh, Point(I, J), Point(I + 1, J + 1), Point(I, J + 1));
			}
		}
	}

	FLNPBakeSupportSource MakeSource(const TCHAR* Key)
	{
		FLNPBakeSupportSource Source;
		Source.Name = Key;
		Source.Key = Key;
		return Source;
	}

	/** 지각 패치와 3층 캡, 둘로 나뉜 sheet, 섬 위 계단. 배열은 Key 오름차순이고 지각이 0번이다. */
	TArray<FLNPBakeSupportSource> MakeScene()
	{
		TArray<FLNPBakeSupportSource> Sources;
		FLNPBakeSupportSource& Crust = Sources.Add_GetRef(MakeSource(TEXT("Crust")));
		AppendPatch(Crust.Mesh, Radius, 30.0, 45.0, 15.0, 25.0, CrustPatchSteps);
		AppendPatch(Sources.Add_GetRef(MakeSource(TEXT("IslandLow"))).Mesh, Radius - 1000.0, StackLat, StackAz, LowHalf, LowHalf);
		AppendPatch(Sources.Add_GetRef(MakeSource(TEXT("IslandMid"))).Mesh, Radius - 2000.0, StackLat, StackAz, MidHalf, MidHalf);
		AppendPatch(Sources.Add_GetRef(MakeSource(TEXT("IslandTop"))).Mesh, Radius - 3000.0, StackLat, StackAz, TopHalf, TopHalf);
		FLNPBakeSupportSource& Split = Sources.Add_GetRef(MakeSource(TEXT("Split")));
		AppendPatch(Split.Mesh, Radius - 1500.0, 40.0, 59.5, 1.5, 1.5);
		AppendPatch(Split.Mesh, Radius - 1500.0, 40.0, 65.5, 1.5, 1.5);
		AppendPatch(Sources.Add_GetRef(MakeSource(TEXT("StairBase"))).Mesh, Radius - 2000.0, StairLat, StairAz, StairBaseHalf, StairBaseHalf);
		AppendPatch(Sources.Add_GetRef(MakeSource(TEXT("StairStep"))).Mesh,
			Radius - 2000.0 - StairHeight, StairLat, StairAz, StairStepHalf, StairStepHalf);
		return Sources;
	}

	FLNPSupportRasterSettings MakeSettings(int32 Subdivisions)
	{
		FLNPSupportRasterSettings Settings;
		Settings.Subdivisions = Subdivisions;
		return Settings;
	}

	FLNPSupportCodecSettings MakeCodec()
	{
		FLNPSupportCodecSettings Codec;
		Codec.BaseRadius = Radius;
		return Codec;
	}

	/** 베이커와 같은 순서로 Layer 분리 → 지각·Layer rasterize. */
	bool RasterizeScene(
		TConstArrayView<FLNPBakeSupportSource> Sources,
		FLNPSupportLayerSet& OutSet,
		TArray<FLNPSupportLayerRaster>& OutRasters,
		FString& OutError)
	{
		OutRasters.Reset();
		FLNPSupportLayerSettings LayerSettings;
		LayerSettings.CrustSubdivisions = CrustSubdivisions;
		LayerSettings.LayerSubdivisionMultiplier = LayerMultiplier;
		if (!LNPSupportLayers::BuildLayers(Sources, 0, LayerSettings, OutSet, OutError))
		{
			return false;
		}
		OutRasters.SetNum(OutSet.Layers.Num());
		if (!LNPCrustAtlas::Rasterize(Sources[0].Mesh, MakeSettings(CrustSubdivisions), OutRasters[0], OutError))
		{
			return false;
		}
		OutRasters[0].SourceIndex = 0;
		for (int32 LayerId = 1; LayerId < OutSet.Layers.Num(); ++LayerId)
		{
			const FLNPSupportLayer& Layer = OutSet.Layers[LayerId];
			const FLNPBakeTriangleMesh LayerMesh = LNPSupportLayers::MakeLayerMesh(Sources[Layer.SourceIndex].Mesh, Layer);
			const int32 N = Layer.Subdivisions;
			if (!LNPSupportAtlas::Rasterize(
				LayerMesh, MakeSettings(N), LNPSupportAtlas::ComputeFootprint(LayerMesh, N), OutRasters[LayerId], OutError))
			{
				OutError = FString::Printf(TEXT("Layer %d: %s"), LayerId, *OutError);
				return false;
			}
			OutRasters[LayerId].SourceIndex = Layer.SourceIndex;
		}
		return true;
	}

	TArray<FLNPSupportAtlasSource> MakeAtlasSources(TConstArrayView<FLNPBakeSupportSource> Sources, const FLNPSupportLayerSet& Set)
	{
		TArray<FLNPSupportAtlasSource> AtlasSources;
		for (int32 SourceIndex = 0; SourceIndex < Sources.Num(); ++SourceIndex)
		{
			AtlasSources.Add({Sources[SourceIndex].Key, Set.FaceMaps[SourceIndex]});
		}
		return AtlasSources;
	}

	bool BuildSceneAtlas(FLNPSupportAtlas& OutAtlas, FString& OutError)
	{
		const TArray<FLNPBakeSupportSource> Sources = MakeScene();
		FLNPSupportLayerSet Set;
		TArray<FLNPSupportLayerRaster> Rasters;
		TArray<uint8> Payload;
		return RasterizeScene(Sources, Set, Rasters, OutError)
			&& LNPSupportAtlas::Encode(Rasters, MakeAtlasSources(Sources, Set), MakeCodec(), Payload, OutError)
			&& LNPSupportAtlas::Decode(Payload, OutAtlas, OutError);
	}

	const TCHAR* LexResult(ELNPSupportQueryResult Result)
	{
		switch (Result)
		{
		case ELNPSupportQueryResult::Supported:
			return TEXT("Supported");
		case ELNPSupportQueryResult::NeedsExact:
			return TEXT("NeedsExact");
		default:
			return TEXT("NoSupport");
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPSupportAtlasFootprintTest,
	"LootNPop.SurfaceNavigation.Bake.SupportAtlasFootprint",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLNPSupportAtlasFootprintTest::RunTest(const FString& Parameters)
{
	using namespace LNPSupportAtlasTest;
	const TArray<FLNPBakeSupportSource> Sources = MakeScene();
	FLNPSupportLayerSet Set;
	TArray<FLNPSupportLayerRaster> Rasters;
	FString Error;
	if (!TestTrue(TEXT("Scene rasterizes"), RasterizeScene(Sources, Set, Rasters, Error)))
	{
		AddError(Error);
		return false;
	}
	TestEqual(TEXT("Crust + 3 stacked caps + 2 split sheets + stair base and step"), Set.Layers.Num(), 8);
	TestTrue(TEXT("Split source has a per-face map"), !Set.FaceMaps[4].IsUniform());
	TestTrue(TEXT("Crust layer is the full octant layout"), Rasters[0].Layout.IsFull());

	// row span은 옥탄트 전체 배치로 구운 결과의 Valid 샘플을 하나도 빠뜨리지 않아야 한다.
	const int32 N = CrustSubdivisions * LayerMultiplier;
	for (int32 LayerId = 1; LayerId < Set.Layers.Num(); ++LayerId)
	{
		const FLNPSupportLayer& Layer = Set.Layers[LayerId];
		const FLNPBakeTriangleMesh LayerMesh = LNPSupportLayers::MakeLayerMesh(Sources[Layer.SourceIndex].Mesh, Layer);
		FLNPSupportLayerRaster Full;
		if (!LNPSupportAtlas::Rasterize(LayerMesh, MakeSettings(N), FLNPSupportLayout::MakeFull(N), Full, Error))
		{
			AddError(Error);
			continue;
		}
		const FLNPSupportLayerRaster& Sparse = Rasters[LayerId];
		int32 FullValid = 0;
		int32 Missing = 0;
		int32 FlagMismatch = 0;
		for (int32 J = 0; J <= N; ++J)
		{
			for (int32 I = 0; I + J <= N; ++I)
			{
				const FLNPSupportSample& Sample = Full.Samples[LNPSupportAtlas::GetSampleIndex(N, I, J)];
				if (!EnumHasAnyFlags(Sample.Flags, ELNPSupportSampleFlags::Valid))
				{
					continue;
				}
				++FullValid;
				const int32 SparseIndex = Sparse.Layout.Find(I, J);
				if (SparseIndex == INDEX_NONE)
				{
					++Missing;
					continue;
				}
				FlagMismatch += Sparse.Samples[SparseIndex].Flags == Sample.Flags ? 0 : 1;
			}
		}
		int32 SparseValid = 0;
		for (const FLNPSupportSample& Sample : Sparse.Samples)
		{
			SparseValid += EnumHasAnyFlags(Sample.Flags, ELNPSupportSampleFlags::Valid) ? 1 : 0;
		}
		AddInfo(FString::Printf(TEXT("Layer %d: rows=%d samples=%d valid=%d (full layout valid %d)"),
			LayerId, Sparse.Layout.Rows.Num(), Sparse.Samples.Num(), SparseValid, FullValid));
		TestTrue(FString::Printf(TEXT("Layer %d has valid samples"), LayerId), FullValid > 0);
		TestEqual(FString::Printf(TEXT("Layer %d footprint keeps every valid sample"), LayerId), Missing, 0);
		TestEqual(FString::Printf(TEXT("Layer %d valid count matches the full layout"), LayerId), SparseValid, FullValid);
		// 구간 밖 이웃을 invalid로 보므로 full 배치와 플래그가 같아야 한다.
		TestEqual(FString::Printf(TEXT("Layer %d flags match the full layout"), LayerId), FlagMismatch, 0);
		TestTrue(FString::Printf(TEXT("Layer %d stores far fewer samples than the octant"), LayerId),
			Sparse.Samples.Num() * 20 < Full.Samples.Num());
	}
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPSupportAtlasLayerQueryTest,
	"LootNPop.SurfaceNavigation.Bake.SupportAtlasLayerQuery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLNPSupportAtlasLayerQueryTest::RunTest(const FString& Parameters)
{
	using namespace LNPSupportAtlasTest;
	FLNPSupportAtlas Atlas;
	FString Error;
	if (!TestTrue(TEXT("Scene atlas builds"), BuildSceneAtlas(Atlas, Error)))
	{
		AddError(Error);
		return false;
	}

	auto Expect = [this, &Atlas](const TCHAR* Label, const FVector3d& Dir, double Feet, double StepUp, double Drop, uint16 Preferred,
		ELNPSupportQueryResult ExpectedResult, uint16 ExpectedLayer, double ExpectedRadius)
	{
		FLNPSupportLayerHit Hit;
		const ELNPSupportQueryResult Result = LNPSupportAtlas::QueryLayers(Atlas, Dir, Feet, StepUp, Drop, Preferred, Hit);
		TestEqual(FString::Printf(TEXT("%s: result %s"), Label, LexResult(Result)), Result, ExpectedResult);
		if (ExpectedResult == ELNPSupportQueryResult::Supported)
		{
			TestEqual(FString::Printf(TEXT("%s: Layer"), Label), Hit.Layer, ExpectedLayer);
			TestTrue(FString::Printf(TEXT("%s: radius %.2f near %.2f"), Label, Hit.Radius, ExpectedRadius),
				FMath::Abs(Hit.Radius - ExpectedRadius) <= 3.0);
		}
	};

	// 같은 방향 3층 캡: 창에 따라 각 층을 고른다.
	const FVector3d Stack = Direction(StackLat, StackAz);
	constexpr ELNPSupportQueryResult Supported = ELNPSupportQueryResult::Supported;
	constexpr ELNPSupportQueryResult NeedsExact = ELNPSupportQueryResult::NeedsExact;
	constexpr ELNPSupportQueryResult NoSupport = ELNPSupportQueryResult::NoSupport;
	Expect(TEXT("Feet on top cap"), Stack, Radius - 3000.0, 50.0, 500.0, LNPSupportLayers::NoLayer, Supported, TopLayer, Radius - 3000.0);
	Expect(TEXT("Feet on middle cap"), Stack, Radius - 2000.0, 50.0, 500.0, LNPSupportLayers::NoLayer, Supported, MidLayer, Radius - 2000.0);
	Expect(TEXT("Feet on low cap"), Stack, Radius - 1000.0, 50.0, 500.0, LNPSupportLayers::NoLayer, Supported, LowLayer, Radius - 1000.0);
	Expect(TEXT("Feet on crust"), Stack, Radius, 50.0, 500.0, LNPSupportLayers::NoLayer, Supported, CrustLayer, Radius);
	Expect(TEXT("Feet in the air above the top cap"), Stack, Radius - 3700.0, 50.0, 500.0, LNPSupportLayers::NoLayer, NoSupport, 0, 0.0);
	Expect(TEXT("Wide window picks the top-most Layer"), Stack, Radius - 3000.0, 50.0, 5000.0, LNPSupportLayers::NoLayer, Supported, TopLayer, Radius - 3000.0);
	Expect(TEXT("Preferred middle Layer wins in a wide window"), Stack, Radius - 3000.0, 50.0, 5000.0, MidLayer, Supported, MidLayer, Radius - 2000.0);
	Expect(TEXT("Preferred crust wins in a wide window"), Stack, Radius - 3000.0, 50.0, 5000.0, CrustLayer, Supported, CrustLayer, Radius);
	Expect(TEXT("Preferred Layer outside the window is ignored"), Stack, Radius - 3000.0, 50.0, 500.0, LowLayer, Supported, TopLayer, Radius - 3000.0);

	// 섬 위 30cm 계단: StepUp 안이면 위 칸, 아니면 섬 윗면.
	const FVector3d Stair = Direction(StairLat, StairAz);
	const double StairBase = Radius - 2000.0;
	Expect(TEXT("Step within StepUp is chosen"), Stair, StairBase, 50.0, 500.0, LNPSupportLayers::NoLayer, Supported, StairStepLayer, StairBase - StairHeight);
	Expect(TEXT("Step above StepUp is skipped"), Stair, StairBase, 20.0, 500.0, LNPSupportLayers::NoLayer, Supported, StairBaseLayer, StairBase);
	Expect(TEXT("Feet on the step stay on the step"), Stair, StairBase - StairHeight, 50.0, 100.0, LNPSupportLayers::NoLayer, Supported, StairStepLayer, StairBase - StairHeight);
	Expect(TEXT("Preferred base under the step"), Stair, StairBase - StairHeight, 50.0, 100.0, StairBaseLayer, Supported, StairBaseLayer, StairBase);
	Expect(TEXT("Beside the step is the island top"), Direction(StairLat, StairAz + StairStepHalf + 0.8), StairBase - StairHeight, 50.0, 100.0,
		LNPSupportLayers::NoLayer, Supported, StairBaseLayer, StairBase);

	// 위 캡 가장자리를 가로지른다. 위 캡이 후보(꼭짓점이 하나라도 Valid이거나 가장자리 띠)면 아래 Layer로 떨어지지 않고 NeedsExact여야 한다.
	int32 NeedsExactCount = 0;
	int32 FellThrough = 0;
	int32 GhostTop = 0;
	int32 MidBeyondEdge = 0;
	for (double Offset = TopHalf - 1.0; Offset <= TopHalf + 1.5; Offset += 0.005)
	{
		const FVector3d Dir = Direction(StackLat, StackAz + Offset);
		FLNPSupportLayerQuery Top;
		LNPSupportAtlas::QueryLayer(Atlas.Layers[TopLayer], Dir, Top);
		FLNPSupportLayerHit Hit;
		const ELNPSupportQueryResult Result = LNPSupportAtlas::QueryLayers(Atlas, Dir, Radius - 3000.0, 50.0, 1500.0, LNPSupportLayers::NoLayer, Hit);
		NeedsExactCount += Result == NeedsExact ? 1 : 0;
		const bool bSupportedTop = Result == Supported && Hit.Layer == TopLayer;
		GhostTop += bSupportedTop && Offset > TopHalf ? 1 : 0;
		FellThrough += Top.IsCandidate() && Result == Supported && Hit.Layer != TopLayer ? 1 : 0;
		MidBeyondEdge += !Top.IsCandidate() && Result == Supported && Hit.Layer == MidLayer ? 1 : 0;
	}
	AddInfo(FString::Printf(TEXT("Top cap edge scan: NeedsExact=%d fellThrough=%d ghostTop=%d midBeyondEdge=%d"),
		NeedsExactCount, FellThrough, GhostTop, MidBeyondEdge));
	TestTrue(TEXT("Top cap edge needs exact"), NeedsExactCount > 0);
	TestEqual(TEXT("Edge never falls through to a lower Layer"), FellThrough, 0);
	TestEqual(TEXT("No ghost floor beyond the top cap"), GhostTop, 0);
	TestTrue(TEXT("Clear of the top cap the middle cap is chosen"), MidBeyondEdge > 0);

	FLNPSupportLayerHit OtherOctantHit;
	TestEqual(TEXT("Direction in another octant needs exact"),
		LNPSupportAtlas::QueryLayers(Atlas, FVector3d(-0.1, 1.0, 1.0).GetSafeNormal(), Radius, 50.0, 500.0, LNPSupportLayers::NoLayer, OtherOctantHit),
		NeedsExact);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPSupportAtlasFoldedSheetTest,
	"LootNPop.SurfaceNavigation.Bake.SupportAtlasFoldedSheet",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLNPSupportAtlasFoldedSheetTest::RunTest(const FString& Parameters)
{
	using namespace LNPSupportAtlasTest;
	// 한 바퀴 넘게 도는 나선 경사로: 모든 삼각형이 walkable이고 한 연결 성분이지만 겹친 구간에서 광선이 두 번 맞는다.
	constexpr int32 SegmentsPerTurn = 48;
	constexpr double Turns = 1.3;
	constexpr double RisePerTurn = 300.0;
	constexpr double InnerRadius = 300.0;
	constexpr double OuterRadius = 700.0;
	const FVector3d Center = Direction(StackLat, StackAz);
	FVector3d TangentA, TangentB;
	Center.FindBestAxisVectors(TangentA, TangentB);
	auto Point = [&](int32 Segment, double LocalRadius)
	{
		const double Theta = UE_TWO_PI * Segment / SegmentsPerTurn;
		const double Height = RisePerTurn * Segment / SegmentsPerTurn;
		return Center * (Radius - 2000.0 - Height) + (TangentA * FMath::Cos(Theta) + TangentB * FMath::Sin(Theta)) * LocalRadius;
	};

	TArray<FLNPBakeSupportSource> Sources;
	AppendPatch(Sources.Add_GetRef(MakeSource(TEXT("Crust"))).Mesh, Radius, 30.0, 45.0, 15.0, 25.0, CrustPatchSteps);
	FLNPBakeSupportSource& Spiral = Sources.Add_GetRef(MakeSource(TEXT("Spiral")));
	for (int32 Segment = 0; Segment < FMath::RoundToInt32(Turns * SegmentsPerTurn); ++Segment)
	{
		AddInwardTriangle(Spiral.Mesh, Point(Segment, InnerRadius), Point(Segment, OuterRadius), Point(Segment + 1, OuterRadius));
		AddInwardTriangle(Spiral.Mesh, Point(Segment, InnerRadius), Point(Segment + 1, OuterRadius), Point(Segment + 1, InnerRadius));
	}

	FLNPSupportLayerSet Set;
	FString Error;
	FLNPSupportLayerSettings Settings;
	// Nav 연속성은 production과 같은 25cm Support에서 확인한다. 얇은 나선에 184cm 격자를 쓰면 경계가 사라진다.
	Settings.CrustSubdivisions = 735;
	Settings.LayerSubdivisionMultiplier = 4;
	if (!TestTrue(TEXT("Spiral Layer split succeeds"), LNPSupportLayers::BuildLayers(Sources, 0, Settings, Set, Error)))
	{
		AddError(Error);
		return false;
	}
	TestTrue(TEXT("Spiral ramp splits into multiple Layers"), Set.Layers.Num() > 2);
	TestEqual(TEXT("One folded original sheet"), Set.SplitSheetCount, 1);
	TestTrue(TEXT("Split has shared cut edges"), Set.CutEdgeCount > 0 && Set.CutBoundaryLength > 0.0);
	TSet<int32> SeenFaces;
	TArray<FLNPSupportLayerRaster> Rasters;
	Rasters.SetNum(Set.Layers.Num());
	if (!LNPCrustAtlas::Rasterize(Sources[0].Mesh, MakeSettings(Settings.CrustSubdivisions), Rasters[0], Error))
	{
		AddError(Error);
		return false;
	}
	Rasters[0].SourceIndex = 0;
	for (int32 LayerId = 1; LayerId < Set.Layers.Num(); ++LayerId)
	{
		const FLNPSupportLayer& Layer = Set.Layers[LayerId];
		const FLNPBakeTriangleMesh PartMesh = LNPSupportLayers::MakeLayerMesh(Spiral.Mesh, Layer);
		const int32 N = Layer.Subdivisions;
		if (!TestTrue(TEXT("Each sub-sheet has one floor per direction"), LNPSupportAtlas::Rasterize(
			PartMesh, MakeSettings(N), LNPSupportAtlas::ComputeFootprint(PartMesh, N), Rasters[LayerId], Error)))
		{
			AddError(Error);
			return false;
		}
		Rasters[LayerId].SourceIndex = 1;
		for (const int32 Face : PartMesh.ExternalFaceIndices)
		{
			TestFalse(TEXT("Face is assigned once"), SeenFaces.Contains(Face));
			SeenFaces.Add(Face);
			TestEqual(TEXT("External face resolves to sub-sheet"), Set.FaceMaps[1].Resolve(Face), static_cast<uint16>(LayerId));
		}
	}
	TestEqual(TEXT("All spiral faces survive"), SeenFaces.Num(), Spiral.Mesh.Triangles.Num());
	TArray<uint8> Payload;
	FLNPSupportAtlas Atlas;
	TestTrue(TEXT("Split Atlas encodes"), LNPSupportAtlas::Encode(Rasters, MakeAtlasSources(Sources, Set), MakeCodec(), Payload, Error));
	TestTrue(TEXT("Split Atlas decodes"), LNPSupportAtlas::Decode(Payload, Atlas, Error));
	int32 MultiLayerDirections = 0;
	for (int32 Segment = 0; Segment < SegmentsPerTurn / 4; ++Segment)
	{
		const FVector3d DirectionAtOverlap = Point(Segment, 500.0).GetSafeNormal();
		int32 Candidates = 0;
		for (int32 LayerId = 1; LayerId < Atlas.Layers.Num(); ++LayerId)
		{
			FLNPSupportLayerQuery Query;
			LNPSupportAtlas::QueryLayer(Atlas.Layers[LayerId], DirectionAtOverlap, Query);
			Candidates += Query.IsCandidate() ? 1 : 0;
		}
		MultiLayerDirections += Candidates >= 2 ? 1 : 0;
	}
	TestTrue(TEXT("Overlapping directions retain both floors"), MultiLayerDirections > 0);
	// 순수 삼각형 oracle: 실제 나선 면에 50cm 간격으로 지지점이 존재해야 하며, 층 사이 빈 공간을 건너뛰지 않는다.
	auto SupportPath = [&](uint16, const FVector3d& From, const FVector3f&, uint16, const FVector3d& To, const FVector3f&)
	{
		const int32 Steps = FMath::Max(2, FMath::CeilToInt(FVector3d::Distance(From, To) / 50.0));
		FVector3d Previous = From;
		for (int32 Step = 1; Step < Steps; ++Step)
		{
			const FVector3d P = FMath::Lerp(From, To, static_cast<double>(Step) / Steps);
			const FVector3d D = P.GetSafeNormal();
			double Best = TNumericLimits<double>::Max();
			FVector3d Hit = FVector3d::ZeroVector;
			for (int32 Face = 0; Face < Spiral.Mesh.Triangles.Num(); ++Face)
			{
				const auto& T = Spiral.Mesh.Triangles[Face];
				const FVector3d A = Spiral.Mesh.Vertices[T.X], B = Spiral.Mesh.Vertices[T.Y], C = Spiral.Mesh.Vertices[T.Z];
				const FVector3d Normal = Spiral.Mesh.GetTriangleNormal(Face);
				const double Dot = FVector3d::DotProduct(Normal, D);
				if (Dot >= -0.71) { continue; }
				const double R = FVector3d::DotProduct(Normal, A) / Dot;
				if (R < P.Length() - 45.0 || R > P.Length() + 60.0) { continue; }
				const FVector3d Q = D * R;
				if (FVector3d::DotProduct(FVector3d::CrossProduct(B - A, Q - A), Normal) < -0.01
					|| FVector3d::DotProduct(FVector3d::CrossProduct(C - B, Q - B), Normal) < -0.01
					|| FVector3d::DotProduct(FVector3d::CrossProduct(A - C, Q - C), Normal) < -0.01) { continue; }
				if (R < Best) { Best = R; Hit = Q; }
			}
			if (Best == TNumericLimits<double>::Max()) { return false; }
			const FVector3d Delta = Hit - Previous;
			const double Rise = FVector3d::DotProduct(Delta, -Previous.GetSafeNormal());
			const double SlopeRise = FMath::Sqrt(FMath::Max(0.0, Delta.SizeSquared() - Rise * Rise));
			if (Rise > FMath::Max(45.0, SlopeRise) || Rise < -FMath::Max(60.0, SlopeRise)) { return false; }
			Previous = Hit;
		}
		return true;
	};
	FLNPNavData Navigation;
	FLNPNavTraversalData Traversal;
	FLNPNavBakeReport NavReport;
	if (!TestTrue(TEXT("Split spiral Nav bakes"), LNPNavBaking::Build(Atlas, {},
		[](uint16, const FVector3d&, const FVector3f&) { return true; }, SupportPath,
		Navigation, Traversal, NavReport, Error))) { AddError(Error); return false; }
	TestTrue(TEXT("Split boundaries retain Walk portals"), !Traversal.Portals.IsEmpty());
	FLNPNavSlotInput Input;
	Input.Support = MakeShared<FLNPSupportAtlas, ESPMode::ThreadSafe>(Atlas);
	Input.Navigation = MakeShared<FLNPNavData, ESPMode::ThreadSafe>(Navigation);
	Input.Traversal = MakeShared<FLNPNavTraversalData, ESPMode::ThreadSafe>(Traversal);
	TArray<FLNPNavSlotInput> Slots;
	Slots.Init(Input, 8);
	const TArray<FRotator> Rotations = {FRotator(0,0,0), FRotator(0,90,0), FRotator(0,180,0), FRotator(0,270,0),
		FRotator(180,0,0), FRotator(180,90,0), FRotator(180,180,0), FRotator(180,270,0)};
	FLNPNavSnapshot Nav;
	if (!TestTrue(TEXT("Split spiral graph assembles"), LNPNavRuntime::BuildSnapshot(Slots, Rotations, 1, Nav, Error)))
	{ AddError(Error); return false; }
	auto NearestSpiralNode = [&](const FVector3d& P)
	{
		int32 BestNode = INDEX_NONE;
		double BestDistance = TNumericLimits<double>::Max();
		for (int32 Node = 0; Node < Nav.Graph.SlotNodeBase[1]; ++Node)
		{
			if (Nav.Graph.GetNode(0, Node).LayerOrdinal == 0) { continue; }
			const double Distance = FVector3d::DistSquared(P, Nav.Graph.GetWorldPoint(0, Node));
			if (Distance < BestDistance) { BestDistance = Distance; BestNode = Node; }
		}
		return BestNode;
	};
	FLNPNavSearchScratch Scratch;
	FLNPNavSearch Search;
	LNPNavPathfinding::BeginSearch(Nav, NearestSpiralNode(Point(1, 500.0)), NearestSpiralNode(Point(60, 500.0)), {}, Scratch, Search);
	while (Search.Status == ELNPNavSearchStatus::Running) { LNPNavPathfinding::StepSearch(Nav, Scratch, Search, 4000); }
	TestTrue(TEXT("A* walks across the split spiral boundary"), Search.Status == ELNPNavSearchStatus::Found);
	TArray<int32> Path;
	LNPNavPathfinding::ExtractNodePath(Scratch, Search, Path);
	int32 LayerCrossings = 0;
	for (int32 Index = 1; Index < Path.Num(); ++Index)
	{
		LayerCrossings += Nav.Graph.GetNode(0, Path[Index - 1]).LayerOrdinal != Nav.Graph.GetNode(0, Path[Index]).LayerOrdinal ? 1 : 0;
	}
	TestTrue(TEXT("Spiral path uses a split-boundary portal"), LayerCrossings > 0);
	AddInfo(FString::Printf(TEXT("Spiral Nav: layers=%d portals=%d candidates=%d/%d/%d pathNodes=%d crossings=%d portalBake=%.3fms"),
		Atlas.Layers.Num(), NavReport.PortalCount, NavReport.PortalDistanceCandidateCount, NavReport.PortalStepCandidateCount,
		NavReport.PortalClearanceCandidateCount, Path.Num(), LayerCrossings, NavReport.PortalBakeSeconds * 1000.0));
	FLNPSupportLayerSet Again;
	TestTrue(TEXT("Repeated split succeeds"), LNPSupportLayers::BuildLayers(Sources, 0, Settings, Again, Error));
	TestTrue(TEXT("Repeated face assignments match"), Set.FaceMaps[1].LayerByExternalFace == Again.FaceMaps[1].LayerByExternalFace);
	// 추출 배열 순서가 달라도 external face 기준으로 같은 sub-sheet를 골라야 한다.
	Algo::Reverse(Spiral.Mesh.Triangles);
	Algo::Reverse(Spiral.Mesh.ExternalFaceIndices);
	TestTrue(TEXT("Reordered split succeeds"), LNPSupportLayers::BuildLayers(Sources, 0, Settings, Again, Error));
	TestTrue(TEXT("Reordered face assignments match"), Set.FaceMaps[1].LayerByExternalFace == Again.FaceMaps[1].LayerByExternalFace);
	// raw folded mesh는 여전히 오류다. 분할 안전망을 완화하지 않는다.
	FLNPSupportLayerRaster RawRaster;
	const int32 N = Settings.GetSourceSubdivisions(Spiral);
	TestFalse(TEXT("Raw folded mesh is rejected"), LNPSupportAtlas::Rasterize(Spiral.Mesh, MakeSettings(N),
		LNPSupportAtlas::ComputeFootprint(Spiral.Mesh, N), RawRaster, Error));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPSupportAtlasCodecTest,
	"LootNPop.SurfaceNavigation.Bake.SupportAtlasCodec",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPSupportAtlasSourceResolutionTest,
	"LootNPop.SurfaceNavigation.Bake.SupportAtlasSourceResolution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLNPSupportAtlasSourceResolutionTest::RunTest(const FString& Parameters)
{
	using namespace LNPSupportAtlasTest;
	TArray<FLNPBakeSupportSource> Sources = MakeScene();
	Sources[1].bCoarseSupport = true;
	FLNPSupportLayerSet Set;
	TArray<FLNPSupportLayerRaster> Rasters;
	FString Error;
	if (!RasterizeScene(Sources, Set, Rasters, Error))
	{
		AddError(Error);
		return false;
	}
	TestEqual(TEXT("Coarse source uses crust grid"), Set.Layers[LowLayer].Subdivisions, CrustSubdivisions);
	TestEqual(TEXT("Other source keeps fine grid"), Set.Layers[MidLayer].Subdivisions, CrustSubdivisions * LayerMultiplier);
	TArray<uint8> Payload;
	FLNPSupportAtlas Atlas;
	if (!LNPSupportAtlas::Encode(Rasters, MakeAtlasSources(Sources, Set), MakeCodec(), Payload, Error)
		|| !LNPSupportAtlas::Decode(Payload, Atlas, Error))
	{
		AddError(Error);
		return false;
	}
	TestEqual(TEXT("Codec preserves coarse grid"), Atlas.Layers[LowLayer].Layout.Subdivisions, CrustSubdivisions);
	TestEqual(TEXT("Codec preserves fine grid"), Atlas.Layers[MidLayer].Layout.Subdivisions, CrustSubdivisions * LayerMultiplier);
	FLNPSupportLayerHit Hit;
	TestEqual(TEXT("Coarse floor remains queryable"), LNPSupportAtlas::QueryLayers(Atlas, Direction(StackLat, StackAz),
		Radius - 1000.0, 45.0, 60.0, LowLayer, Hit), ELNPSupportQueryResult::Supported);
	TestEqual(TEXT("Query selects coarse floor"), Hit.Layer, LowLayer);
	TestTrue(TEXT("Coarse floor height matches geometry"), FMath::Abs(Hit.Radius - (Radius - 1000.0)) < 10.0);
	FLNPSupportLayerSet Rejected;
	TestFalse(TEXT("Missing grid is rejected"), LNPSupportLayers::BuildLayers(Sources, 0,
		FLNPSupportLayerSettings(), Rejected, Error));
	return !HasAnyErrors();
}

bool FLNPSupportAtlasCodecTest::RunTest(const FString& Parameters)
{
	using namespace LNPSupportAtlasTest;
	const TArray<FLNPBakeSupportSource> Sources = MakeScene();
	FLNPSupportLayerSet Set;
	TArray<FLNPSupportLayerRaster> Rasters;
	FString Error;
	if (!TestTrue(TEXT("Scene rasterizes"), RasterizeScene(Sources, Set, Rasters, Error)))
	{
		AddError(Error);
		return false;
	}
	const TArray<FLNPSupportAtlasSource> AtlasSources = MakeAtlasSources(Sources, Set);
	const FLNPSupportCodecSettings Codec = MakeCodec();
	TArray<uint8> Payload;
	FLNPSupportAtlas Atlas;
	if (!TestTrue(TEXT("Scene encodes"), LNPSupportAtlas::Encode(Rasters, AtlasSources, Codec, Payload, Error))
		|| !TestTrue(TEXT("Scene decodes"), LNPSupportAtlas::Decode(Payload, Atlas, Error)))
	{
		AddError(Error);
		return false;
	}

	TestEqual(TEXT("Layer count"), Atlas.Layers.Num(), Rasters.Num());
	TestEqual(TEXT("Source count"), Atlas.Sources.Num(), AtlasSources.Num());
	int64 SampleTotal = 0;
	for (int32 LayerId = 0; LayerId < FMath::Min(Atlas.Layers.Num(), Rasters.Num()); ++LayerId)
	{
		const FLNPSupportAtlasLayer& Layer = Atlas.Layers[LayerId];
		const FLNPSupportLayerRaster& Raster = Rasters[LayerId];
		SampleTotal += Layer.Num();
		bool bLayoutMatches = Layer.Layout.Subdivisions == Raster.Layout.Subdivisions && Layer.Layout.J0 == Raster.Layout.J0
			&& Layer.Layout.Rows.Num() == Raster.Layout.Rows.Num() && Layer.SourceIndex == Raster.SourceIndex;
		for (int32 Row = 0; bLayoutMatches && Row < Layer.Layout.Rows.Num(); ++Row)
		{
			bLayoutMatches = Layer.Layout.Rows[Row].IStart == Raster.Layout.Rows[Row].IStart
				&& Layer.Layout.Rows[Row].Count == Raster.Layout.Rows[Row].Count;
		}
		TestTrue(FString::Printf(TEXT("Layer %d layout survives the codec"), LayerId), bLayoutMatches);
		if (!bLayoutMatches)
		{
			continue;
		}

		double MaxRadiusError = 0.0;
		double MinValid = TNumericLimits<double>::Max();
		double MaxValid = TNumericLimits<double>::Lowest();
		int32 FlagMismatch = 0;
		int32 NormalMismatch = 0;
		for (int32 Index = 0; Index < Raster.Samples.Num(); ++Index)
		{
			const FLNPSupportSample& Sample = Raster.Samples[Index];
			FlagMismatch += Layer.GetFlags(Index) == Sample.Flags ? 0 : 1;
			if (EnumHasAnyFlags(Sample.Flags, ELNPSupportSampleFlags::Valid))
			{
				MaxRadiusError = FMath::Max(MaxRadiusError, FMath::Abs(Layer.GetRadius(Index) - Sample.Radius));
				NormalMismatch += FVector3f::DotProduct(Layer.GetNormal(Index), Sample.Normal) > 0.99999f ? 0 : 1;
				MinValid = FMath::Min(MinValid, Sample.Radius);
				MaxValid = FMath::Max(MaxValid, Sample.Radius);
			}
		}
		TestEqual(FString::Printf(TEXT("Layer %d flags survive"), LayerId), FlagMismatch, 0);
		TestEqual(FString::Printf(TEXT("Layer %d normals survive"), LayerId), NormalMismatch, 0);
		TestTrue(FString::Printf(TEXT("Layer %d radius error %.4f is at most half a step"), LayerId, MaxRadiusError),
			MaxRadiusError <= Codec.RadiusStep * 0.5 + 1e-9);
		if (LayerId == 0)
		{
			TestEqual(TEXT("Crust base radius is the codec base radius"), Layer.BaseRadius, Codec.BaseRadius);
		}
		else
		{
			TestTrue(FString::Printf(TEXT("Layer %d base radius %.2f sits inside its sample range"), LayerId, Layer.BaseRadius),
				Layer.BaseRadius >= MinValid - Codec.RadiusStep && Layer.BaseRadius <= MaxValid + Codec.RadiusStep);
			TestTrue(FString::Printf(TEXT("Layer %d base radius is a step multiple"), LayerId),
				FMath::IsNearlyZero(FMath::Fmod(Layer.BaseRadius, Codec.RadiusStep), 1e-6));
		}
	}

	bool bSourcesMatch = Atlas.Sources.Num() == AtlasSources.Num();
	for (int32 SourceIndex = 0; bSourcesMatch && SourceIndex < AtlasSources.Num(); ++SourceIndex)
	{
		const FLNPSupportFaceMap& Expected = AtlasSources[SourceIndex].FaceMap;
		const FLNPSupportFaceMap& Actual = Atlas.Sources[SourceIndex].FaceMap;
		bSourcesMatch = Atlas.Sources[SourceIndex].Key == AtlasSources[SourceIndex].Key
			&& Actual.IsUniform() == Expected.IsUniform()
			&& (Expected.IsUniform() ? Actual.UniformLayer == Expected.UniformLayer : Actual.LayerByExternalFace == Expected.LayerByExternalFace);
	}
	TestTrue(TEXT("Source keys and face maps survive"), bSourcesMatch);
	AddInfo(FString::Printf(TEXT("Codec v2: %d Layers, %lld samples, payload=%d bytes"), Atlas.Layers.Num(), SampleTotal, Payload.Num()));
	TestTrue(TEXT("Payload holds 7 bytes per sample plus tables"), Payload.Num() > SampleTotal * 7);

	FLNPSupportAtlas Rejected;
	TestFalse(TEXT("Truncated payload is rejected"), LNPSupportAtlas::Decode(MakeArrayView(Payload.GetData(), Payload.Num() - 1), Rejected, Error));
	TArray<uint8> Trailing = Payload;
	Trailing.Add(0);
	TestFalse(TEXT("Trailing bytes are rejected"), LNPSupportAtlas::Decode(Trailing, Rejected, Error));
	TArray<uint8> OldVersion = Payload;
	OldVersion[0] = 1;
	TestFalse(TEXT("Codec v1 payload is rejected"), LNPSupportAtlas::Decode(OldVersion, Rejected, Error));

	TArray<FLNPSupportAtlasSource> Unsorted = AtlasSources;
	Swap(Unsorted[1], Unsorted[2]);
	TArray<uint8> Unused;
	TestFalse(TEXT("Unsorted source keys are an encoding error"), LNPSupportAtlas::Encode(Rasters, Unsorted, Codec, Unused, Error));
	TArray<FLNPSupportLayerRaster> SparseCrust = Rasters;
	Swap(SparseCrust[0], SparseCrust[1]);
	TestFalse(TEXT("Layer 0 must be a full crust layout"), LNPSupportAtlas::Encode(SparseCrust, AtlasSources, Codec, Unused, Error));
	TArray<FLNPSupportLayerRaster> OddSubdivisions = Rasters;
	OddSubdivisions[1].Layout = LNPSupportAtlas::ComputeFootprint(FLNPBakeTriangleMesh(), CrustSubdivisions * 2 + 1);
	OddSubdivisions[1].Samples.Reset();
	TestFalse(TEXT("Layer subdivisions must be a crust multiple"), LNPSupportAtlas::Encode(OddSubdivisions, AtlasSources, Codec, Unused, Error));
	FLNPSupportCodecSettings FarBase = Codec;
	FarBase.BaseRadius = Radius + 10000.0;
	TestFalse(TEXT("Crust radius offset beyond the int16 range is an encoding error"),
		LNPSupportAtlas::Encode(Rasters, AtlasSources, FarBase, Unused, Error));
	return !HasAnyErrors();
}

#endif
