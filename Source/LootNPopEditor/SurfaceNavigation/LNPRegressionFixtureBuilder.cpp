// Copyright (c) 2026 LootNPop. All rights reserved.

// 정적 회귀 fixture LVI를 결정론적으로 생성하는 에디터 명령(`design/RegressionMap.md` §3).
//
// 지각은 노이즈 없는 완전 구면이라 기대값이 "반지름 R, 법선은 중심 방향"으로 정해진다. 부유섬(하나·둘·가장자리),
// 동굴 키트(공동 + 경사 통로, 지각 입구), 정적 프랍, 이음매·꼭짓점 probe를 옥탄트 내부에 둔다.
// 패널·기둥·파괴 바닥 같은 동적 사례는 LVI에 둘 수 없으므로(D-026) `L_SurfaceRegression`에 남는다.
//
// 예: LNP.SurfaceNav.BuildRegressionFixture
//     동굴 키트 메시(LNP.SurfaceNav.BuildCaveKit)가 먼저 있어야 한다.
//     기존 LVI가 있으면 거부한다. 다시 만들려면 LVI를 지운 뒤 실행한다(fixture 메시는 제자리 갱신).

#include "Config/LNPSettings.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Misc/PackageName.h"
#include "SurfaceNavigation/LNPCaveKit.h"
#include "SurfaceNavigation/LNPFixtureMesh.h"
#include "SurfaceNavigation/LNPRegressionFixture.h"

DEFINE_LOG_CATEGORY_STATIC(LogLNPRegressionFixture, Log, All);

namespace LNPRegressionFixture
{
	namespace
	{
		const FName SupportTag(TEXT("LNP.Surface.Support"));
		const FName BlockerTag(TEXT("LNP.Surface.Blocker"));
		const FName StaticTag(TEXT("LNP.Surface.Static"));
		const FName DecorationTag(TEXT("LNP.Surface.Decoration"));
		const FName TerrainProfile(TEXT("LNPStaticTerrain"));
		const FName BlockerProfile(TEXT("LNPStaticBlocker"));
		const FName DecorationProfile(TEXT("LNPDecoration"));

		struct FBuildContext
		{
			UWorld* World = nullptr;
			UStaticMesh* BoxTop = nullptr;
			UStaticMesh* BoxBody = nullptr;
			UStaticMesh* Sphere = nullptr;
			UStaticMesh* Cylinder = nullptr;
			UStaticMesh* Cube = nullptr;

			void Spawn(UStaticMesh& Mesh, const FString& Label, const FTransform& Transform, bool bSupport)
			{
				TArray<FName> Tags = bSupport ? TArray<FName>{SupportTag, BlockerTag, StaticTag} : TArray<FName>{BlockerTag, StaticTag};
				LNPFixtureMesh::SpawnMeshActor(*World, Mesh, Label, Transform, MoveTemp(Tags), bSupport ? TerrainProfile : BlockerProfile);
			}

			void Decoration(UStaticMesh& Mesh, const FString& Label, const FTransform& Transform)
			{
				LNPFixtureMesh::SpawnMeshActor(*World, Mesh, Label, Transform, {DecorationTag}, DecorationProfile);
			}

			void Probe(const FString& Name, const FVector& Location)
			{
				Decoration(*Sphere, TEXT("Probe_") + Name, FTransform(FQuat::Identity, Location, FVector(0.3)));
			}

			/** 윗면(Support)과 몸체(측벽·밑면, Blocker)로 나눈 섬 판. 윗면이 반지름 Top에 온다. */
			void Island(const FString& Label, const FCaseFrame& Frame, double Top, double Length, double Width)
			{
				const FTransform Transform(Frame.UpRotation(), Frame.Radial * (Top + IslandThickness * 0.5),
					FVector(Length / 100.0, Width / 100.0, IslandThickness / 100.0));
				Spawn(*BoxTop, Label + TEXT("_Top"), Transform, true);
				Spawn(*BoxBody, Label + TEXT("_Body"), Transform, false);
			}
		};

		void Run()
		{
			if (!FMath::IsNearlyEqual(GetDefault<ULNPSettings>()->SphereRadius, CrustRadius))
			{
				UE_LOG(LogLNPRegressionFixture, Error, TEXT("[RegressionFixture] SphereRadius %.0f differs from the fixture radius %.0f"),
					GetDefault<ULNPSettings>()->SphereRadius, CrustRadius);
				return;
			}
			const FString LevelPackageName = FString::Printf(TEXT("%s/%s"), FixtureFolder, LevelName);
			if (FPackageName::DoesPackageExist(LevelPackageName))
			{
				UE_LOG(LogLNPRegressionFixture, Error,
					TEXT("[RegressionFixture] %s already exists. Delete it first to rebuild."), *LevelPackageName);
				return;
			}

			auto LoadKit = [](const TCHAR* Name)
			{
				return LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("%s/%s.%s"), LNPCaveKit::KitFolder, Name, Name));
			};
			UStaticMesh* RoomFloor = LoadKit(LNPCaveKit::RoomFloorName);
			UStaticMesh* RoomShell = LoadKit(LNPCaveKit::RoomShellName);
			UStaticMesh* CorridorFloor = LoadKit(LNPCaveKit::CorridorFloorName);
			UStaticMesh* CorridorShell = LoadKit(LNPCaveKit::CorridorShellName);
			if (!RoomFloor || !RoomShell || !CorridorFloor || !CorridorShell)
			{
				UE_LOG(LogLNPRegressionFixture, Error, TEXT("[RegressionFixture] Cave kit meshes are missing. Run LNP.SurfaceNav.BuildCaveKit first."));
				return;
			}

			// 지각에서 통로 내부와 겹치는 부분만 잘라 입구를 낸다. 통로 벽 평면이 곧 구멍 경계라 틈이 없다.
			const FCaseFrame CaveFrame = Cave();
			const LNPCaveKit::FPlacement Placement = LNPCaveKit::PlaceUnderSphere(CaveFrame.Radial, CaveFrame.Bitangent, CrustRadius);
			TArray<FPlane> EntrancePlanes;
			for (const FPlane& Local : LNPCaveKit::GetCorridorInteriorPlanes())
			{
				const FVector Normal = Placement.Corridor.TransformVectorNoScale(Local.GetNormal());
				const FVector Base = Placement.Corridor.TransformPosition(Local.GetNormal() * Local.W);
				EntrancePlanes.Emplace(Base, Normal);
			}
			FLNPFixtureMesh Crust = LNPFixtureMesh::BuildSphereOctant(CrustRadius, CrustSubdivisions);
			const int32 CutTriangles = Crust.RemoveInsideConvex(EntrancePlanes);

			FBuildContext Context;
			UStaticMesh* CrustMesh = LNPFixtureMesh::WriteStaticMesh(FixtureFolder, CrustMeshName, Crust);
			Context.BoxTop = LNPFixtureMesh::WriteStaticMesh(FixtureFolder, BoxTopMeshName,
				LNPFixtureMesh::BuildUnitBox(LNPFixtureMesh::EBoxFace::PosZ));
			Context.BoxBody = LNPFixtureMesh::WriteStaticMesh(FixtureFolder, BoxBodyMeshName,
				LNPFixtureMesh::BuildUnitBox(LNPFixtureMesh::EBoxFace::All & ~LNPFixtureMesh::EBoxFace::PosZ));
			Context.Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
			Context.Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
			Context.Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
			if (!CrustMesh || !Context.BoxTop || !Context.BoxBody || !Context.Sphere || !Context.Cylinder || !Context.Cube)
			{
				UE_LOG(LogLNPRegressionFixture, Error, TEXT("[RegressionFixture] Mesh creation failed"));
				return;
			}

			Context.World = LNPFixtureMesh::CreateLevelWorld(LevelPackageName);
			Context.Spawn(*CrustMesh, TEXT("RF_00_Crust"), FTransform::Identity, true);

			const FCaseFrame Basic = BasicCrust();
			Context.Probe(TEXT("BasicCrust"), Basic.At(CrustRadius - 500.0));

			const FCaseFrame One = IslandOne();
			Context.Island(TEXT("RF_01_IslandOne"), One, IslandOneTop, IslandOneSize, IslandOneSize);
			Context.Probe(TEXT("IslandOne"), One.At(IslandOneTop - 500.0));

			const FCaseFrame Two = IslandTwo();
			Context.Island(TEXT("RF_02_IslandTwoOuter"), Two, IslandTwoOuterTop, IslandTwoOuterSize, IslandTwoOuterSize);
			Context.Island(TEXT("RF_02_IslandTwoInner"), Two, IslandTwoInnerTop, IslandTwoInnerSize, IslandTwoInnerSize);
			Context.Probe(TEXT("IslandTwo"), Two.At(IslandTwoInnerTop - 600.0));

			const FCaseFrame Edge = IslandEdge();
			Context.Island(TEXT("RF_03_IslandEdge"), Edge, IslandEdgeTop, IslandEdgeLength, IslandEdgeWidth);
			Context.Probe(TEXT("IslandEdgeInside"), Edge.At(IslandEdgeTop - 500.0, 550.0));
			Context.Probe(TEXT("IslandEdgeOutside"), Edge.At(IslandEdgeTop - 500.0, 750.0));

			Context.Spawn(*RoomFloor, TEXT("RF_04_CaveRoom_Floor"), Placement.Room, true);
			Context.Spawn(*RoomShell, TEXT("RF_04_CaveRoom_Shell"), Placement.Room, false);
			Context.Spawn(*CorridorFloor, TEXT("RF_04_CaveCorridor_Floor"), Placement.Corridor, true);
			Context.Spawn(*CorridorShell, TEXT("RF_04_CaveCorridor_Shell"), Placement.Corridor, false);
			Context.Probe(TEXT("Cave"), Placement.Room.TransformPosition(FVector(0.0, 0.0, 250.0)));

			// 엔진 원기둥·구는 단순 충돌이지만 Blocker 전용이라 베이커 입력이 아니다. 지각에 뿌리를 묻는다.
			const FCaseFrame Props = StaticProps();
			const FQuat PropRotation = Props.UpRotation();
			Context.Spawn(*Context.Cylinder, TEXT("RF_05_TreeBlocker"),
				FTransform(PropRotation, Props.At(CrustRadius - TreeHeight * 0.5, TreeTangent),
					FVector(TreeDiameter / 100.0, TreeDiameter / 100.0, TreeHeight / 100.0)), false);
			Context.Spawn(*Context.Sphere, TEXT("RF_05_RockBlocker"),
				FTransform(PropRotation, Props.At(CrustRadius - 200.0, RockTangent), FVector(4.5, 4.5, 4.0)), false);
			Context.Decoration(*Context.Cube, TEXT("RF_05_Decoration"),
				FTransform(PropRotation, Props.At(CrustRadius - 400.0, DecorationTangent), FVector(5.0, 2.5, 2.0)));
			Context.Probe(TEXT("StaticProps"), Props.At(CrustRadius - 600.0));

			const TArray<TPair<FString, FVector>> SeamProbes = {
				{TEXT("SeamMid_XY"), FVector(1.0, 1.0, 0.0).GetSafeNormal()},
				{TEXT("SeamMid_YZ"), FVector(0.0, 1.0, 1.0).GetSafeNormal()},
				{TEXT("SeamMid_ZX"), FVector(1.0, 0.0, 1.0).GetSafeNormal()},
				{TEXT("Corner_X"), FVector::XAxisVector},
				{TEXT("Corner_Y"), FVector::YAxisVector},
				{TEXT("Corner_Z"), FVector::ZAxisVector},
			};
			for (const TPair<FString, FVector>& Probe : SeamProbes)
			{
				Context.Probe(Probe.Key, Probe.Value * (CrustRadius - 100.0));
			}

			const bool bSaved = LNPFixtureMesh::SaveWorld(*Context.World);
			UE_LOG(LogLNPRegressionFixture, Display,
				TEXT("[RegressionFixture] %s %s | Radius=%.0f Subdivisions=%d CrustTriangles=%d EntranceCut=%d RoomFloorRadius=%.1f"),
				*LevelPackageName, bSaved ? TEXT("saved") : TEXT("FAILED to save"), CrustRadius, CrustSubdivisions,
				Crust.Triangles.Num(), CutTriangles, Placement.Room.GetLocation().Size());
		}

		FAutoConsoleCommand Command(
			TEXT("LNP.SurfaceNav.BuildRegressionFixture"),
			TEXT("Create the static regression fixture LVI (perfect-sphere crust with a cave entrance, floating islands, ")
			TEXT("island edge, cave kit room and ramp corridor, static props, seam and corner probes) under ")
			TEXT("/Game/Maps/SurfaceNavigation/Fixtures. Requires the cave kit meshes. Refuses to overwrite an existing LVI."),
			FConsoleCommandDelegate::CreateStatic(&Run));
	}
}
