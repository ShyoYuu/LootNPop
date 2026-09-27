// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "SurfaceNavigation/LNPSurfaceBakeGeometry.h"

namespace LNPSurfaceBakeGeometryTest
{
	constexpr double Radius = 30000.0;

	/** 반지름 Radius의 (+X,+Y,+Z) 옥탄트 구면 패치. 변 위 정점이 이음매 평면 위에 정확히 놓인다. */
	FLNPBakeSupportSource MakeCrust(const TCHAR* Name)
	{
		constexpr int32 N = 8;
		FLNPBakeSupportSource Source;
		Source.Name = Name;
		TArray<int32> Index;
		Index.SetNumUninitialized((N + 1) * (N + 1));
		for (int32 J = 0; J <= N; ++J)
		{
			for (int32 I = 0; I + J <= N; ++I)
			{
				Index[J * (N + 1) + I] = Source.Mesh.Vertices.Add(FVector3d(I, J, N - I - J).GetSafeNormal() * Radius);
			}
		}
		for (int32 J = 0; J < N; ++J)
		{
			for (int32 I = 0; I + J < N; ++I)
			{
				Source.Mesh.Triangles.Emplace(
					Index[J * (N + 1) + I], Index[J * (N + 1) + I + 1], Index[(J + 1) * (N + 1) + I]);
			}
		}
		return Source;
	}

	/** Direction 방향에 놓인 작은 삼각형 하나(섬 윗면 대역). */
	FLNPBakeSupportSource MakeIsland(const TCHAR* Name, const FVector3d& Direction, double IslandRadius)
	{
		FVector3d TangentA, TangentB;
		Direction.FindBestAxisVectors(TangentA, TangentB);
		const FVector3d Center = Direction * IslandRadius;
		FLNPBakeSupportSource Source;
		Source.Name = Name;
		Source.Mesh.Vertices = {Center + TangentA * 500.0, Center + TangentB * 500.0, Center - TangentA * 500.0};
		Source.Mesh.Triangles.Emplace(0, 1, 2);
		return Source;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPSurfaceBakeCrustIdentificationTest,
	"LootNPop.SurfaceNavigation.Bake.CrustIdentification",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLNPSurfaceBakeCrustIdentificationTest::RunTest(const FString& Parameters)
{
	using namespace LNPSurfaceBakeGeometryTest;
	const FVector3d InteriorDirection = FVector3d(1.0, 2.0, 3.0).GetSafeNormal();

	FLNPBakeSupportSource Crust = MakeCrust(TEXT("Crust"));
	FLNPBakeSupportSource Island = MakeIsland(TEXT("Island"), InteriorDirection, Radius - 2000.0);
	TestEqual(TEXT("Crust touches all three seam planes"), LNPSurfaceBake::GetTouchedSeamPlanes(Crust.Mesh), 0b111);
	TestEqual(TEXT("Interior island touches no seam plane"), LNPSurfaceBake::GetTouchedSeamPlanes(Island.Mesh), 0);

	FString Error;
	int32 CrustIndex = INDEX_NONE;
	TestTrue(TEXT("Crust among islands is identified"),
		LNPSurfaceBake::IdentifyCrust({Island, Crust}, CrustIndex, Error));
	TestEqual(TEXT("Identified index points at the crust"), CrustIndex, 1);

	TestFalse(TEXT("Octant without a crust is a bake error"),
		LNPSurfaceBake::IdentifyCrust({Island}, CrustIndex, Error));
	TestEqual(TEXT("Missing crust yields no index"), CrustIndex, INDEX_NONE);

	FLNPBakeSupportSource SecondCrust = MakeCrust(TEXT("SecondCrust"));
	TestFalse(TEXT("Two crust candidates are a bake error"),
		LNPSurfaceBake::IdentifyCrust({Crust, Island, SecondCrust}, CrustIndex, Error));
	TestTrue(TEXT("Ambiguity error names every candidate"),
		Error.Contains(TEXT("SecondCrust")) && Error.Contains(TEXT("\nCrust")));

	// 두 평면에만 닿는 source(이음매 변 하나에 붙은 섬)는 지각이 아니다.
	FLNPBakeSupportSource EdgeIsland = MakeIsland(TEXT("EdgeIsland"), FVector3d(1.0, 1.0, 1.0).GetSafeNormal(), Radius);
	EdgeIsland.Mesh.Vertices.Add(FVector3d(0.0, 0.0, Radius));
	EdgeIsland.Mesh.Triangles.Emplace(0, 1, 3);
	TestEqual(TEXT("Axis vertex touches two seam planes"), LNPSurfaceBake::GetTouchedSeamPlanes(EdgeIsland.Mesh), 0b011);
	TestTrue(TEXT("Two-plane source does not compete with the crust"),
		LNPSurfaceBake::IdentifyCrust({EdgeIsland, Crust}, CrustIndex, Error) && CrustIndex == 1);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPSurfaceBakeSupportSourceValidationTest,
	"LootNPop.SurfaceNavigation.Bake.SupportSourceValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLNPSurfaceBakeSupportSourceValidationTest::RunTest(const FString& Parameters)
{
	using namespace LNPSurfaceBakeGeometryTest;
	FString Error;

	TestTrue(TEXT("30,000cm crust passes"), LNPSurfaceBake::ValidateSupportSource(MakeCrust(TEXT("Crust")), Error));

	// 옥탄트 중심 방향은 성분이 r/√3이라 반지름 50,000도 캡 안이다. 반지름이 아니라 성분이 기준임을 고정한다.
	TestTrue(TEXT("Large radius along the octant center stays under the per-component cap"),
		LNPSurfaceBake::ValidateSupportSource(
			MakeIsland(TEXT("CenterIsland"), FVector3d(1.0, 1.0, 1.0).GetSafeNormal(), 50000.0), Error));

	// 꼭짓점(좌표축) 부근은 반지름이 곧 성분이다.
	TestFalse(TEXT("Geometry beyond the int16 cap near an octant corner is rejected"),
		LNPSurfaceBake::ValidateSupportSource(
			MakeIsland(TEXT("CornerCave"), FVector3d(1.0, 0.03, 0.03).GetSafeNormal(), 33500.0), Error));
	TestTrue(TEXT("Cap error names the cap"), Error.Contains(TEXT("int16")));

	FLNPBakeSupportSource Crossing = MakeIsland(TEXT("Crossing"), FVector3d(1.0, 1.0, 1.0).GetSafeNormal(), Radius);
	Crossing.Mesh.Vertices[0].X = -5.0;
	TestFalse(TEXT("Support crossing the octant boundary is rejected"),
		LNPSurfaceBake::ValidateSupportSource(Crossing, Error));
	TestTrue(TEXT("Boundary error names the boundary"), Error.Contains(TEXT("boundary")));

	FLNPBakeSupportSource WithinTolerance = Crossing;
	WithinTolerance.Mesh.Vertices[0].X = -0.5;
	TestTrue(TEXT("Seam float error inside the tolerance passes"),
		LNPSurfaceBake::ValidateSupportSource(WithinTolerance, Error));

	FLNPBakeSupportSource NonFinite = MakeCrust(TEXT("NonFinite"));
	NonFinite.Mesh.Vertices[3].Y = std::numeric_limits<double>::quiet_NaN();
	TestFalse(TEXT("Non-finite vertex is rejected"), LNPSurfaceBake::ValidateSupportSource(NonFinite, Error));

	FLNPBakeSupportSource Empty;
	Empty.Name = TEXT("Empty");
	TestFalse(TEXT("Source without triangles is rejected"), LNPSurfaceBake::ValidateSupportSource(Empty, Error));
	return !HasAnyErrors();
}

#endif
