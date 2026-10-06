// Copyright (c) 2026 LootNPop. All rights reserved.

// 벽보행 NPC 섬 검증 옥탄트 LVI를 결정론적으로 생성하는 에디터 명령(SurfaceSupportNavigation Phase 14 단위 4 통합 확인).
//
// 완전 구면 지각 위 옥탄트 중앙에 부유섬 하나(밑면 40m)를 띄우고, 섬 위와 지각에 Pod anchor를 나눠 둔다.
// 섬 위 Pod 소속 개체를 섬 밑면에서 넉백으로 떨어뜨리면 Home까지 직선거리가 재귀속 계측 거리를 넘는다(단위 4d).
// 지각 Pod는 섬 중심에서 70~90m라 플레이어가 섬 위에 있으면 배회 대역에 든다(단위 4e·4f). 그중 하나는 탑 곁이다.
// 섬 밑으로는 그냥 떨어지고(낙하 피해 없음), 지각에서 섬 가장자리로 이어진 30° 경사로로 걸어 돌아온다.
// 회귀 fixture와 달리 oracle이 없는 PIE 확인용이다.
//
// 예: LNP.SurfaceNav.BuildIslandFixture
//     회귀 fixture의 상자 메시(SM_FixtureBoxTop·Body)가 먼저 있어야 한다.
//     기존 LVI가 있으면 거부한다. 다시 만들려면 LVI를 지운 뒤 실행한다(지각 메시는 제자리 갱신).
//     이어서 LNP.SurfaceNav.BakeOctant /Game/Maps/SurfaceNavigation/Fixtures/LVI_Octant_Fixture_Island

#include "Config/LNPSettings.h"
#include "DynamicTerrain/LNPPlacementMarker.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Misc/PackageName.h"
#include "SurfaceNavigation/LNPFixtureMesh.h"
#include "SurfaceNavigation/LNPMassSpawnPoint.h"
#include "SurfaceNavigation/LNPRegressionFixture.h"

DEFINE_LOG_CATEGORY_STATIC(LogLNPIslandFixture, Log, All);

namespace LNPIslandFixture
{
	namespace
	{
		using LNPRegressionFixture::CrustRadius;
		using LNPRegressionFixture::FixtureFolder;

		const TCHAR* const LevelName = TEXT("LVI_Octant_Fixture_Island");
		const TCHAR* const CrustMeshName = TEXT("SM_FixtureIslandCrust_R30000");

		/** 구조물을 지각 아래로 묻는 깊이(cm). 곡률로 바닥 가장자리가 뜨지 않게 한다. */
		constexpr double Bury = 50.0;

		/** 부유섬: 밑면 높이·두께(cm). 윗면은 지각 위 43m다. */
		constexpr double IslandBottom = 4000.0;
		constexpr double IslandThickness = 300.0;
		constexpr double IslandTop = IslandBottom + IslandThickness;
		constexpr double IslandHalfSize = 3000.0;

		/**
		 * 옥탄트 중앙(위도 35.26°, 방위 45°) 기준 로컬 좌표. X는 위도 증가, Y는 방위 증가, Height는 지각에서 구 중심 쪽 높이다.
		 * 각 지점의 Up은 그 지점의 구 중심 방향이라 구조물마다 지면에 수직으로 선다.
		 */
		const LNPRegressionFixture::FCaseFrame& Center()
		{
			static const LNPRegressionFixture::FCaseFrame Frame(35.264389682754654, 45.0);
			return Frame;
		}

		FVector Radial(double X, double Y)
		{
			return (Center().Radial * CrustRadius + Center().Bitangent * X + Center().Tangent * Y).GetSafeNormal();
		}

		FVector Point(double X, double Y, double Height)
		{
			return Radial(X, Y) * (CrustRadius - Height);
		}

		/** 로컬 +Z = 그 지점의 Up, +Y = 방위 증가 방향. */
		FQuat Rotation(double X, double Y)
		{
			return FRotationMatrix::MakeFromZY(-Radial(X, Y), Center().Tangent).ToQuat();
		}

		struct FBuildContext
		{
			UWorld* World = nullptr;
			UStaticMesh* BoxTop = nullptr;
			UStaticMesh* BoxBody = nullptr;
			UClass* AnchorClass = nullptr;
			UClass* LauncherClass = nullptr;
			int32 Boxes = 0;
			int32 Markers = 0;
			int32 SpawnPoints = 0;

			/**
			 * 윗면(Support)과 나머지 면(Blocker)으로 나눈 상자. 중심 (X, Y), 크기 SizeX × SizeY × Height.
			 * Elevation이 0이면 지각에 묻고, 0보다 크면 밑면이 그 높이에 뜬다.
			 */
			void Box(const FString& Label, double X, double Y, double SizeX, double SizeY, double Height, double Elevation = 0.0)
			{
				const double Bottom = Elevation > 0.0 ? Elevation : -Bury;
				const double Top = Elevation > 0.0 ? Elevation + Height : Height;
				const FTransform Transform(Rotation(X, Y), Point(X, Y, (Bottom + Top) * 0.5),
					FVector(SizeX / 100.0, SizeY / 100.0, (Top - Bottom) / 100.0));
				LNPFixtureMesh::SpawnMeshActor(*World, *BoxTop, Label + TEXT("_Top"), Transform,
					{FName(TEXT("LNP.Surface.Support")), FName(TEXT("LNP.Surface.Blocker")), FName(TEXT("LNP.Surface.Static"))},
					FName(TEXT("LNPStaticTerrain")));
				LNPFixtureMesh::SpawnMeshActor(*World, *BoxBody, Label + TEXT("_Body"), Transform,
					{FName(TEXT("LNP.Surface.Blocker")), FName(TEXT("LNP.Surface.Static"))}, FName(TEXT("LNPStaticBlocker")));
				++Boxes;
			}

			/**
			 * From에서 To까지 윗면이 지나는 곧은 경사로(윗면 Support, 나머지 Blocker). (X, Y, Height)는 Box와 같은 로컬 좌표다.
			 * 아래쪽 끝은 지각에 2m 묻고 위쪽 끝은 섬 윗면 위로 1m 겹쳐 틈 없이 잇는다.
			 */
			void Ramp(const FString& Label, const FVector& From, const FVector& To, double Width, double Thickness)
			{
				const FVector Low = Point(From.X, From.Y, From.Z);
				const FVector High = Point(To.X, To.Y, To.Z);
				const FVector Along = (High - Low).GetSafeNormal();
				const FVector Start = Low - Along * 200.0;
				const FVector End = High + Along * 100.0;
				const FVector MidUp = -Radial((From.X + To.X) * 0.5, (From.Y + To.Y) * 0.5);
				const FQuat RampRotation = FRotationMatrix::MakeFromXZ(Along, MidUp).ToQuat();
				const FVector Up = RampRotation.GetAxisZ();
				const FTransform Transform(RampRotation, (Start + End) * 0.5 - Up * (Thickness * 0.5),
					FVector(FVector::Distance(Start, End) / 100.0, Width / 100.0, Thickness / 100.0));
				LNPFixtureMesh::SpawnMeshActor(*World, *BoxTop, Label + TEXT("_Top"), Transform,
					{FName(TEXT("LNP.Surface.Support")), FName(TEXT("LNP.Surface.Blocker")), FName(TEXT("LNP.Surface.Static"))},
					FName(TEXT("LNPStaticTerrain")));
				LNPFixtureMesh::SpawnMeshActor(*World, *BoxBody, Label + TEXT("_Body"), Transform,
					{FName(TEXT("LNP.Surface.Blocker")), FName(TEXT("LNP.Surface.Static"))}, FName(TEXT("LNPStaticBlocker")));
				++Boxes;
			}

			void Marker(UClass* ElementClass, const TCHAR* Prefix, const FVector& Location, const FQuat& MarkerRotation)
			{
				ALNPPlacementMarker* PlacementMarker = World->SpawnActor<ALNPPlacementMarker>(Location, MarkerRotation.Rotator());
				PlacementMarker->SetActorLabel(FString::Printf(TEXT("%s_%02d"), Prefix, Markers));
				PlacementMarker->MarkerId = FGuid(0x0E140003, 0, 0, Markers + 1);
				PlacementMarker->ElementClass = ElementClass;
				++Markers;
			}

			void Anchor(double X, double Y, double Height)
			{
				Marker(AnchorClass, TEXT("IF_Anchor"), Point(X, Y, Height), Rotation(X, Y));
			}

			/** 지각 위 런처. 발사 방향은 마커 전방의 접평면 성분이므로 (ToX, ToY) 쪽을 보게 한다. */
			void Launcher(double X, double Y, double ToX, double ToY)
			{
				const FVector Up = -Radial(X, Y);
				const FVector Forward = FVector::VectorPlaneProject(Point(ToX, ToY, 0.0) - Point(X, Y, 0.0), Up).GetSafeNormal();
				Marker(LauncherClass, TEXT("IF_Launcher"), Point(X, Y, 0.0), FRotationMatrix::MakeFromZX(Up, Forward).ToQuat());
			}

			/** Height는 anchor가 놓일 면의 높이다(지각 0, 섬 윗면 IslandTop). 베이커가 반지름이 가장 가까운 Layer로 투영한다. */
			void Spawn(double X, double Y, double Height, FName SpawnSet)
			{
				ALNPMassSpawnPoint* SpawnPoint = World->SpawnActor<ALNPMassSpawnPoint>(Point(X, Y, Height), Rotation(X, Y).Rotator());
				SpawnPoint->SetActorLabel(FString::Printf(TEXT("IF_Spawn_%02d_%s"), SpawnPoints, *SpawnSet.ToString()));
				SpawnPoint->SpawnPointId = FGuid(0x0E140003, 1, 0, SpawnPoints + 1);
				SpawnPoint->TargetSpawnSetId = SpawnSet;
				++SpawnPoints;
			}
		};

		void Run()
		{
			if (!FMath::IsNearlyEqual(GetDefault<ULNPSettings>()->SphereRadius, CrustRadius))
			{
				UE_LOG(LogLNPIslandFixture, Error, TEXT("[IslandFixture] SphereRadius %.0f differs from the fixture radius %.0f"),
					GetDefault<ULNPSettings>()->SphereRadius, CrustRadius);
				return;
			}
			const FString LevelPackageName = FString::Printf(TEXT("%s/%s"), FixtureFolder, LevelName);
			if (FPackageName::DoesPackageExist(LevelPackageName))
			{
				UE_LOG(LogLNPIslandFixture, Error, TEXT("[IslandFixture] %s already exists. Delete it first to rebuild."), *LevelPackageName);
				return;
			}

			auto LoadBox = [](const TCHAR* Name)
			{
				return LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("%s/%s.%s"), FixtureFolder, Name, Name));
			};
			FBuildContext Context;
			Context.BoxTop = LoadBox(LNPRegressionFixture::BoxTopMeshName);
			Context.BoxBody = LoadBox(LNPRegressionFixture::BoxBodyMeshName);
			Context.AnchorClass = LoadClass<AActor>(nullptr, TEXT("/Game/WorldDevice/BP_GrappleAnchor.BP_GrappleAnchor_C"));
			Context.LauncherClass = LoadClass<AActor>(nullptr, TEXT("/Game/WorldDevice/BP_SpringLauncher.BP_SpringLauncher_C"));
			const FLNPFixtureMesh Crust = LNPFixtureMesh::BuildSphereOctant(CrustRadius, LNPRegressionFixture::CrustSubdivisions);
			UStaticMesh* CrustMesh = LNPFixtureMesh::WriteStaticMesh(FixtureFolder, CrustMeshName, Crust);
			if (!Context.BoxTop || !Context.BoxBody || !Context.AnchorClass || !Context.LauncherClass || !CrustMesh)
			{
				UE_LOG(LogLNPIslandFixture, Error,
					TEXT("[IslandFixture] Box meshes (run LNP.SurfaceNav.BuildRegressionFixture), world device classes or crust mesh is missing"));
				return;
			}

			Context.World = LNPFixtureMesh::CreateLevelWorld(LevelPackageName);
			LNPFixtureMesh::SpawnMeshActor(*Context.World, *CrustMesh, TEXT("IF_00_Crust"), FTransform::Identity,
				{FName(TEXT("LNP.Surface.Support")), FName(TEXT("LNP.Surface.Blocker")), FName(TEXT("LNP.Surface.Static"))},
				FName(TEXT("LNPStaticTerrain")));

			// 부유섬: 60m × 60m, 밑면 40m. 윗면은 Support, 옆면·밑면은 Blocker라 벽보행만 밑면에 붙는다.
			Context.Box(TEXT("IF_01_Island"), 0.0, 0.0, IslandHalfSize * 2.0, IslandHalfSize * 2.0, IslandThickness, IslandBottom);

			// 경사로: -X 가장자리 중앙에서 지각까지 약 30°(수평 74.5m, 높이 43m), 폭 6m.
			Context.Ramp(TEXT("IF_03_Ramp"), FVector(-IslandHalfSize - 7450.0, 0.0, 0.0), FVector(-IslandHalfSize, 0.0, IslandTop), 600.0, 100.0);

			// 탑: 지각 Pod 곁의 25m 수직면. 추격하다 벽에 오른 개체가 배회 대역으로 넘어가는 장면용이다.
			Context.Box(TEXT("IF_02_Tower"), 7000.0, 1300.0, 800.0, 800.0, 2500.0);

			// Pod anchor: 세트마다 섬 위 하나와 지각 하나. 베이커가 anchor끼리 20m 이상 떨어지기를 요구한다.
			// 섬 위 anchor는 가장자리에서 15m 안쪽이라 주변 적도 섬 윗면에서 스폰된다.
			struct FSpawnPoint { double X; double Y; double Height; const TCHAR* SpawnSet; };
			const FSpawnPoint SpawnPoints[] = {
				{-1500.0, -1500.0, IslandTop, TEXT("Bench01")}, {1500.0, -1500.0, IslandTop, TEXT("Bench01_2")},
				{-1500.0, 1500.0, IslandTop, TEXT("Bench01_3")}, {1500.0, 1500.0, IslandTop, TEXT("Bench01_4")},
				{7000.0, 0.0, 0.0, TEXT("Bench01")}, {0.0, 7500.0, 0.0, TEXT("Bench01_2")},
				{-7000.0, -2000.0, 0.0, TEXT("Bench01_3")}, {2000.0, -8000.0, 0.0, TEXT("Bench01_4")},
			};
			for (const FSpawnPoint& SpawnPoint : SpawnPoints)
			{
				Context.Spawn(SpawnPoint.X, SpawnPoint.Y, SpawnPoint.Height, FName(SpawnPoint.SpawnSet));
			}

			// 그래플 앵커: 탑 꼭대기 위 하나. 섬 복귀는 경사로가 맡는다.
			Context.Anchor(7000.0, 1300.0, 3200.0);

			// 런처: 섬 밑 지각에서 섬 바깥으로 빠져나가는 두 개.
			Context.Launcher(1000.0, 1000.0, 6000.0, 6000.0);
			Context.Launcher(-1000.0, -1000.0, -6000.0, -6000.0);

			const bool bSaved = LNPFixtureMesh::SaveWorld(*Context.World);
			UE_LOG(LogLNPIslandFixture, Display,
				TEXT("[IslandFixture] %s %s | Radius=%.0f boxes=%d markers=%d spawnAnchors=%d islandTop=%.0f"),
				*LevelPackageName, bSaved ? TEXT("saved") : TEXT("FAILED to save"), CrustRadius,
				Context.Boxes, Context.Markers, Context.SpawnPoints, IslandTop);
		}

		FAutoConsoleCommand Command(
			TEXT("LNP.SurfaceNav.BuildIslandFixture"),
			TEXT("Create the wall-crawler island verification LVI (perfect-sphere crust with a 60 m floating island 40 m up, a tower, ")
			TEXT("a 30-degree ramp back up, Pod spawn anchors split between the island and the crust, a grapple anchor and spring launchers) under ")
			TEXT("/Game/Maps/SurfaceNavigation/Fixtures. Requires the regression box meshes. Refuses to overwrite an existing LVI."),
			FConsoleCommandDelegate::CreateStatic(&Run));
	}
}
