// Copyright (c) 2026 LootNPop. All rights reserved.

// 벽보행 NPC 검증 옥탄트 LVI를 결정론적으로 생성하는 에디터 명령(SurfaceSupportNavigation Phase 14 단위 2d).
//
// 완전 구면 지각 위 옥탄트 중앙에 수직 구조물을 모은다: 긴 절벽과 그 옆면의 돌출부(천장), 높은 탑, 네 기둥 위 판(천장),
// 높이가 다른 턱 줄, 가는 기둥. 그래플 앵커를 구조물 위와 공중에 뿌려 플레이어가 수직으로 도망칠 수 있게 하고,
// 네 편성 세트의 Pod anchor를 구조물 곁에 둔다. 회귀 fixture와 달리 oracle이 없는 PIE 확인용이다.
//
// 예: LNP.SurfaceNav.BuildCrawlFixture
//     회귀 fixture의 상자 메시(SM_FixtureBoxTop·Body)가 먼저 있어야 한다.
//     기존 LVI가 있으면 거부한다. 다시 만들려면 LVI를 지운 뒤 실행한다(지각 메시는 제자리 갱신).
//     이어서 LNP.SurfaceNav.BakeOctant /Game/Maps/SurfaceNavigation/Fixtures/LVI_Octant_Fixture_Crawl

#include "Config/LNPSettings.h"
#include "DynamicTerrain/LNPPlacementMarker.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Misc/PackageName.h"
#include "SurfaceNavigation/LNPFixtureMesh.h"
#include "SurfaceNavigation/LNPMassSpawnPoint.h"
#include "SurfaceNavigation/LNPRegressionFixture.h"

DEFINE_LOG_CATEGORY_STATIC(LogLNPCrawlFixture, Log, All);

namespace LNPCrawlFixture
{
	namespace
	{
		using LNPRegressionFixture::CrustRadius;
		using LNPRegressionFixture::FixtureFolder;

		const TCHAR* const LevelName = TEXT("LVI_Octant_Fixture_Crawl");
		const TCHAR* const CrustMeshName = TEXT("SM_FixtureCrawlCrust_R30000");

		/** 구조물을 지각 아래로 묻는 깊이(cm). 곡률로 바닥 가장자리가 뜨지 않게 한다. */
		constexpr double Bury = 50.0;

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
			int32 Boxes = 0;
			int32 Anchors = 0;
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

			void Anchor(double X, double Y, double Height)
			{
				ALNPPlacementMarker* Marker = World->SpawnActor<ALNPPlacementMarker>(Point(X, Y, Height), Rotation(X, Y).Rotator());
				Marker->SetActorLabel(FString::Printf(TEXT("CF_Anchor_%02d"), Anchors));
				Marker->MarkerId = FGuid(0x0E140002, 0, 0, Anchors + 1);
				Marker->ElementClass = AnchorClass;
				++Anchors;
			}

			void Spawn(double X, double Y, FName SpawnSet)
			{
				ALNPMassSpawnPoint* SpawnPoint = World->SpawnActor<ALNPMassSpawnPoint>(Point(X, Y, 0.0), Rotation(X, Y).Rotator());
				SpawnPoint->SetActorLabel(FString::Printf(TEXT("CF_Spawn_%02d_%s"), SpawnPoints, *SpawnSet.ToString()));
				SpawnPoint->SpawnPointId = FGuid(0x0E140002, 1, 0, SpawnPoints + 1);
				SpawnPoint->TargetSpawnSetId = SpawnSet;
				++SpawnPoints;
			}
		};

		void Run()
		{
			if (!FMath::IsNearlyEqual(GetDefault<ULNPSettings>()->SphereRadius, CrustRadius))
			{
				UE_LOG(LogLNPCrawlFixture, Error, TEXT("[CrawlFixture] SphereRadius %.0f differs from the fixture radius %.0f"),
					GetDefault<ULNPSettings>()->SphereRadius, CrustRadius);
				return;
			}
			const FString LevelPackageName = FString::Printf(TEXT("%s/%s"), FixtureFolder, LevelName);
			if (FPackageName::DoesPackageExist(LevelPackageName))
			{
				UE_LOG(LogLNPCrawlFixture, Error, TEXT("[CrawlFixture] %s already exists. Delete it first to rebuild."), *LevelPackageName);
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
			const FLNPFixtureMesh Crust = LNPFixtureMesh::BuildSphereOctant(CrustRadius, LNPRegressionFixture::CrustSubdivisions);
			UStaticMesh* CrustMesh = LNPFixtureMesh::WriteStaticMesh(FixtureFolder, CrustMeshName, Crust);
			if (!Context.BoxTop || !Context.BoxBody || !Context.AnchorClass || !CrustMesh)
			{
				UE_LOG(LogLNPCrawlFixture, Error,
					TEXT("[CrawlFixture] Box meshes (run LNP.SurfaceNav.BuildRegressionFixture), grapple anchor class or crust mesh is missing"));
				return;
			}

			Context.World = LNPFixtureMesh::CreateLevelWorld(LevelPackageName);
			LNPFixtureMesh::SpawnMeshActor(*Context.World, *CrustMesh, TEXT("CF_00_Crust"), FTransform::Identity,
				{FName(TEXT("LNP.Surface.Support")), FName(TEXT("LNP.Surface.Blocker")), FName(TEXT("LNP.Surface.Static"))},
				FName(TEXT("LNPStaticTerrain")));

			// 절벽: 오르기·볼록 윗모서리·윗면. 광장 쪽 옆면 중간에 돌출부가 붙어 밑면이 천장이다(오목 → 천장 → 볼록).
			Context.Box(TEXT("CF_01_Cliff"), 0.0, 3000.0, 5000.0, 800.0, 1200.0);
			Context.Box(TEXT("CF_01_CliffLedge"), 0.0, 2000.0, 2000.0, 1200.0, 200.0, 800.0);

			// 탑: 30m 수직면과 세로 볼록 모서리.
			Context.Box(TEXT("CF_02_Tower"), 3000.0, -1500.0, 1000.0, 1000.0, 3000.0);

			// 네 기둥 위 판: 기둥 → 판 밑면(천장) → 판 옆면 → 윗면.
			for (const double DX : {-900.0, 900.0})
			{
				for (const double DY : {-900.0, 900.0})
				{
					Context.Box(FString::Printf(TEXT("CF_03_TablePillar_%c%c"), DX < 0 ? TEXT('N') : TEXT('P'), DY < 0 ? TEXT('N') : TEXT('P')),
						-3000.0 + DX, -2500.0 + DY, 400.0, 400.0, 800.0);
				}
			}
			Context.Box(TEXT("CF_03_TableTop"), -3000.0, -2500.0, 2400.0, 2400.0, 200.0, 800.0);

			// 턱 줄: 넘는 턱(15cm 이하)부터 몸(반지름 80)보다 높은 턱까지. 낮은 턱 옆면에 누운 채 멈추는 현상 재현용이다.
			const double StepHeights[] = {10.0, 30.0, 50.0, 70.0, 100.0, 150.0, 250.0};
			for (int32 Index = 0; Index < UE_ARRAY_COUNT(StepHeights); ++Index)
			{
				Context.Box(FString::Printf(TEXT("CF_04_Step_%03d"), FMath::RoundToInt(StepHeights[Index])),
					-1800.0 + Index * 600.0, -4000.0, 500.0, 500.0, StepHeights[Index]);
			}

			// 가는 기둥: 몸 지름(160)과 비슷한 폭에서 모서리 감싸기.
			Context.Box(TEXT("CF_05_ThinPillarA"), -4500.0, 1500.0, 200.0, 200.0, 1500.0);
			Context.Box(TEXT("CF_05_ThinPillarB"), -4500.0, 500.0, 300.0, 300.0, 1500.0);

			// 그래플 앵커(사거리 4~25m): 구조물 위와 공중에 15~20m 간격으로 둬 연속 그래플로 수직 도주할 수 있게 한다.
			const FVector AnchorPoints[] = {
				{-1800.0, 3000.0, 1800.0}, {1800.0, 3000.0, 1800.0},   // 절벽 위
				{0.0, 1200.0, 1600.0},                                 // 돌출부 위 공중
				{3000.0, -1500.0, 3600.0}, {1800.0, -300.0, 2200.0},   // 탑 꼭대기·탑 옆
				{-3000.0, -2500.0, 1600.0}, {-1500.0, -1200.0, 1800.0}, // 판 위·판과 광장 사이
				{0.0, -4000.0, 1200.0}, {-4500.0, 1000.0, 2100.0},     // 턱 줄 위·가는 기둥 위
				{0.0, 0.0, 2000.0}, {3500.0, 1500.0, 1800.0}, {-3500.0, 3000.0, 1800.0}, // 광장·외곽 공중
			};
			for (const FVector& AnchorPoint : AnchorPoints)
			{
				Context.Anchor(AnchorPoint.X, AnchorPoint.Y, AnchorPoint.Z);
			}

			// Pod anchor: 세트당 두 곳. 적은 Pod 주변 반경 안 지면에서 스폰되므로 구조물 바로 곁에 둔다. 베이커가 anchor끼리 20m 이상 떨어지기를 요구한다.
			struct FSpawnPoint { double X; double Y; const TCHAR* SpawnSet; };
			const FSpawnPoint SpawnPoints[] = {
				{0.0, 1300.0, TEXT("Bench01")}, {1600.0, -1600.0, TEXT("Bench01")},
				{-1300.0, -900.0, TEXT("Bench01_2")}, {-200.0, -2900.0, TEXT("Bench01_2")},
				{-3300.0, 1000.0, TEXT("Bench01_3")}, {2200.0, 1200.0, TEXT("Bench01_3")},
				{4000.0, -300.0, TEXT("Bench01_4")}, {-3500.0, -1100.0, TEXT("Bench01_4")},
			};
			for (const FSpawnPoint& SpawnPoint : SpawnPoints)
			{
				Context.Spawn(SpawnPoint.X, SpawnPoint.Y, FName(SpawnPoint.SpawnSet));
			}

			const bool bSaved = LNPFixtureMesh::SaveWorld(*Context.World);
			UE_LOG(LogLNPCrawlFixture, Display,
				TEXT("[CrawlFixture] %s %s | Radius=%.0f boxes=%d grappleAnchors=%d spawnAnchors=%d"),
				*LevelPackageName, bSaved ? TEXT("saved") : TEXT("FAILED to save"), CrustRadius,
				Context.Boxes, Context.Anchors, Context.SpawnPoints);
		}

		FAutoConsoleCommand Command(
			TEXT("LNP.SurfaceNav.BuildCrawlFixture"),
			TEXT("Create the wall-crawler verification LVI (perfect-sphere crust with a cliff and ledge, tower, table, step row, ")
			TEXT("thin pillars, grapple anchors and Pod spawn anchors) under /Game/Maps/SurfaceNavigation/Fixtures. ")
			TEXT("Requires the regression box meshes. Refuses to overwrite an existing LVI."),
			FConsoleCommandDelegate::CreateStatic(&Run));
	}
}
