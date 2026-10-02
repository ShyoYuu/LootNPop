// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "SurfaceNavigation/LNPSupportLayers.h"
#include "SurfaceNavigation/LNPSurfaceBakeGeometry.h"

namespace LNPSupportLayersTest
{
	constexpr double Radius = 30000.0;
	constexpr int32 Steps = 4;

	FVector3d Direction(double LatDeg, double AzDeg)
	{
		const double Lat = FMath::DegreesToRadians(LatDeg);
		const double Az = FMath::DegreesToRadians(AzDeg);
		return FVector3d(FMath::Cos(Lat) * FMath::Cos(Az), FMath::Cos(Lat) * FMath::Sin(Az), FMath::Sin(Lat));
	}

	/** 삼각형 하나를 더한다. bInward면 앞면이 구 중심(지역 Up)을, 아니면 바깥을 향하도록 winding을 고른다. */
	void AddTriangle(FLNPBakeTriangleMesh& Mesh, const FVector3d& A, const FVector3d& B, const FVector3d& C, bool bInward, int32 External)
	{
		const FVector3d Normal = FVector3d::CrossProduct(B - A, C - A);
		const bool bFacesCenter = FVector3d::DotProduct(Normal, -(A + B + C)) > 0.0;
		const int32 Base = Mesh.Vertices.Num();
		Mesh.Vertices.Add(A);
		if (bFacesCenter == bInward)
		{
			Mesh.Vertices.Add(B);
			Mesh.Vertices.Add(C);
		}
		else
		{
			Mesh.Vertices.Add(C);
			Mesh.Vertices.Add(B);
		}
		Mesh.Triangles.Emplace(Base, Base + 1, Base + 2);
		Mesh.ExternalFaceIndices.Add(External);
	}

	/**
	 * 반지름 R의 위도·방위 사각 구면 패치. 정점은 삼각형마다 따로 만든다(용접 전 삼각형 soup).
	 * External 번호는 FirstExternal부터 차례로 붙인다.
	 */
	void AppendPatch(FLNPBakeTriangleMesh& Mesh, double R, double Lat0, double Lat1, double Az0, double Az1, bool bInward, int32 FirstExternal)
	{
		int32 External = FirstExternal;
		auto Point = [&](int32 I, int32 J)
		{
			return Direction(Lat0 + (Lat1 - Lat0) * J / Steps, Az0 + (Az1 - Az0) * I / Steps) * R;
		};
		for (int32 J = 0; J < Steps; ++J)
		{
			for (int32 I = 0; I < Steps; ++I)
			{
				AddTriangle(Mesh, Point(I, J), Point(I + 1, J), Point(I + 1, J + 1), bInward, External++);
				AddTriangle(Mesh, Point(I, J), Point(I + 1, J + 1), Point(I, J + 1), bInward, External++);
			}
		}
	}

	/** 위도 Lat의 등위도선을 따라 반지름 R0에서 R1까지 서 있는 벽(non-walkable). */
	void AppendRiser(FLNPBakeTriangleMesh& Mesh, double Lat, double Az0, double Az1, double R0, double R1, int32 FirstExternal)
	{
		int32 External = FirstExternal;
		for (int32 I = 0; I < Steps; ++I)
		{
			const FVector3d D0 = Direction(Lat, Az0 + (Az1 - Az0) * I / Steps);
			const FVector3d D1 = Direction(Lat, Az0 + (Az1 - Az0) * (I + 1) / Steps);
			AddTriangle(Mesh, D0 * R0, D1 * R0, D1 * R1, true, External++);
			AddTriangle(Mesh, D0 * R0, D1 * R1, D0 * R1, true, External++);
		}
	}

	/** 세 이음매 평면에 닿는 작은 지각 대용. 테스트는 지각 인덱스를 직접 넘기므로 식별 규칙은 보지 않는다. */
	FLNPBakeSupportSource MakeCrust()
	{
		FLNPBakeSupportSource Source;
		Source.Name = TEXT("Crust");
		Source.Key = TEXT("Crust.Mesh");
		AppendPatch(Source.Mesh, Radius, 20.0, 40.0, 20.0, 40.0, true, 0);
		// 절벽 한 장. 지각은 non-walkable face도 Layer 0이다.
		AppendRiser(Source.Mesh, 30.0, 20.0, 40.0, Radius, Radius - 500.0, 1000);
		return Source;
	}

	FLNPBakeSupportSource MakeSource(const TCHAR* Key)
	{
		FLNPBakeSupportSource Source;
		Source.Name = Key;
		Source.Key = Key;
		return Source;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPSupportLayersSplitTest,
	"LootNPop.SurfaceNavigation.Bake.SupportLayersSplit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLNPSupportLayersSplitTest::RunTest(const FString& Parameters)
{
	using namespace LNPSupportLayersTest;
	FLNPSupportLayerSettings Settings;
	Settings.CrustSubdivisions = 200;

	// 한 컴포넌트의 떨어진 판 2장(분리 sheet). 뒤쪽 판의 external 번호를 작게 줘서 순서가 external을 따르는지 본다.
	FLNPBakeSupportSource Split = MakeSource(TEXT("B.Split"));
	AppendPatch(Split.Mesh, Radius - 800.0, 30.0, 31.0, 10.0, 11.0, true, 500);
	AppendPatch(Split.Mesh, Radius - 800.0, 30.0, 31.0, 14.0, 15.0, true, 100);

	// 양면 판: 같은 자리에 반대 winding. 바깥을 향한 면은 walkable이 아니라 Layer가 없다.
	FLNPBakeSupportSource DoubleSided = MakeSource(TEXT("A.DoubleSided"));
	AppendPatch(DoubleSided.Mesh, Radius - 800.0, 25.0, 26.0, 70.0, 71.0, true, 0);
	AppendPatch(DoubleSided.Mesh, Radius - 800.0, 25.0, 26.0, 70.0, 71.0, false, 32);

	// 계단: 아래 판과 위 판이 선 벽으로만 이어진다. 벽은 non-walkable이라 두 Layer로 나뉜다.
	FLNPBakeSupportSource Step = MakeSource(TEXT("C.Step"));
	AppendPatch(Step.Mesh, Radius - 800.0, 34.0, 35.0, 50.0, 51.0, true, 0);
	AppendRiser(Step.Mesh, 35.0, 50.0, 51.0, Radius - 800.0, Radius - 900.0, 32);
	AppendPatch(Step.Mesh, Radius - 900.0, 35.0, 36.0, 50.0, 51.0, true, 40);

	// 용접: 삼각형 soup 판 하나는 정점이 모두 따로여도 한 Layer다.
	FLNPBakeSupportSource Soup = MakeSource(TEXT("D.Soup"));
	AppendPatch(Soup.Mesh, Radius - 800.0, 36.0, 37.0, 60.0, 61.0, true, 0);

	const TArray<FLNPBakeSupportSource> Sources = {Step, MakeCrust(), Split, Soup, DoubleSided};
	constexpr int32 CrustIndex = 1;
	FLNPSupportLayerSet Set;
	FString Error;
	if (!TestTrue(TEXT("Layers build"), LNPSupportLayers::BuildLayers(Sources, CrustIndex, Settings, Set, Error)))
	{
		AddError(Error);
		return false;
	}

	// 기대 순서: 지각, A.DoubleSided, B.Split(external 100 판, 500 판), C.Step(아래 판, 위 판), D.Soup.
	const TArray<TPair<int32, int32>> Expected = {
		{1, 0}, {4, 0}, {2, 100}, {2, 500}, {0, 0}, {0, 40}, {3, 0}};
	if (TestEqual(TEXT("Layer count"), Set.Layers.Num(), Expected.Num()))
	{
		for (int32 LayerId = 0; LayerId < Expected.Num(); ++LayerId)
		{
			TestEqual(FString::Printf(TEXT("Layer %d source"), LayerId), Set.Layers[LayerId].SourceIndex, Expected[LayerId].Key);
			TestEqual(FString::Printf(TEXT("Layer %d min external face"), LayerId),
				Set.Layers[LayerId].MinExternalFace, Expected[LayerId].Value);
		}
		TestEqual(TEXT("Crust Layer keeps its cliff triangles"), Set.Layers[0].Triangles.Num(), Sources[CrustIndex].Mesh.Triangles.Num());
		TestEqual(TEXT("Each patch Layer has all 32 patch triangles"), Set.Layers[6].Triangles.Num(), 2 * Steps * Steps);
	}

	const FLNPSupportFaceMap& CrustMap = Set.FaceMaps[CrustIndex];
	TestTrue(TEXT("Crust face map is uniform Layer 0"), CrustMap.IsUniform() && CrustMap.UniformLayer == 0);
	TestEqual(TEXT("Crust cliff face resolves to Layer 0"), CrustMap.Resolve(1000), uint16(0));
	TestTrue(TEXT("Single-sheet source is uniform"), Set.FaceMaps[3].IsUniform() && Set.FaceMaps[3].UniformLayer == 6);

	const FLNPSupportFaceMap& SplitMap = Set.FaceMaps[2];
	TestFalse(TEXT("Split source needs a per-face map"), SplitMap.IsUniform());
	TestEqual(TEXT("Split face 100 -> Layer 2"), SplitMap.Resolve(100), uint16(2));
	TestEqual(TEXT("Split face 531 -> Layer 3"), SplitMap.Resolve(531), uint16(3));
	TestEqual(TEXT("Split unused external number -> NoLayer"), SplitMap.Resolve(300), LNPSupportLayers::NoLayer);

	const FLNPSupportFaceMap& DoubleMap = Set.FaceMaps[4];
	TestEqual(TEXT("Double-sided inward face -> Layer 1"), DoubleMap.Resolve(0), uint16(1));
	TestEqual(TEXT("Double-sided outward face -> NoLayer"), DoubleMap.Resolve(32), LNPSupportLayers::NoLayer);

	const FLNPSupportFaceMap& StepMap = Set.FaceMaps[0];
	TestEqual(TEXT("Step riser -> NoLayer"), StepMap.Resolve(32), LNPSupportLayers::NoLayer);
	TestEqual(TEXT("Step upper floor -> Layer 5"), StepMap.Resolve(40), uint16(5));

	// 결정론: 같은 입력은 같은 결과.
	FLNPSupportLayerSet Again;
	LNPSupportLayers::BuildLayers(Sources, CrustIndex, Settings, Again, Error);
	bool bSame = Again.Layers.Num() == Set.Layers.Num();
	for (int32 LayerId = 0; bSame && LayerId < Set.Layers.Num(); ++LayerId)
	{
		bSame = Again.Layers[LayerId].Triangles == Set.Layers[LayerId].Triangles;
	}
	TestTrue(TEXT("Rebuilding gives identical Layers"), bSame);

	// 오류: external 번호 누락, key 중복.
	TArray<FLNPBakeSupportSource> Broken = Sources;
	Broken[3].Mesh.ExternalFaceIndices.Pop();
	TestFalse(TEXT("Missing external face index is rejected"), LNPSupportLayers::BuildLayers(Broken, CrustIndex, Settings, Again, Error));
	Broken = Sources;
	Broken[3].Key = Broken[2].Key;
	TestFalse(TEXT("Duplicate source key is rejected"), LNPSupportLayers::BuildLayers(Broken, CrustIndex, Settings, Again, Error));
	return !HasAnyErrors();
}

#endif
