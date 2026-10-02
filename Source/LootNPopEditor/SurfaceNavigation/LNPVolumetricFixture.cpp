// Copyright (c) 2026 LootNPop. All rights reserved.

#include "SurfaceNavigation/LNPVolumetricFixture.h"

#include "Components/StaticMeshComponent.h"
#include "DynamicTerrain/LNPPlacementMarker.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Misc/PackageName.h"
#include "SurfaceNavigation/LNPCaveKit.h"
#include "SurfaceNavigation/LNPMassSpawnPoint.h"

DEFINE_LOG_CATEGORY_STATIC(LogLNPVolumetricFixture, Log, All);

namespace LNPVolumetricFixture
{
	namespace
	{
		/** 구면 바닥을 100cm 이하 사각형으로 나눈다. 높이가 다른 층도 같은 곡률을 쓴다. */
		void Floor(FLNPFixtureMesh& Mesh, double X0, double X1, double Y0, double Y1, double Height = 0.0)
		{
			const int32 NX = FMath::CeilToInt((X1 - X0) / 100.0);
			const int32 NY = FMath::CeilToInt((Y1 - Y0) / 100.0);
			for (int32 Y = 0; Y < NY; ++Y)
			{
				for (int32 X = 0; X < NX; ++X)
				{
					const double A = FMath::Lerp(X0, X1, static_cast<double>(X) / NX);
					const double B = FMath::Lerp(X0, X1, static_cast<double>(X + 1) / NX);
					const double C = FMath::Lerp(Y0, Y1, static_cast<double>(Y) / NY);
					const double D = FMath::Lerp(Y0, Y1, static_cast<double>(Y + 1) / NY);
					Mesh.AddQuad(FloorPoint(A, C, Height), FloorPoint(B, C, Height),
						FloorPoint(B, D, Height), FloorPoint(A, D, Height), FVector::UpVector, false);
				}
			}
		}

		void Wall(FLNPFixtureMesh& Mesh, const FVector& A, const FVector& B, double Top)
		{
			Mesh.AddQuad(A, B, FVector(B.X, B.Y, Top), FVector(A.X, A.Y, Top),
				FVector::CrossProduct(B - A, FVector::UpVector).GetSafeNormal(), true);
		}

		void Roof(FLNPFixtureMesh& Mesh, double X0, double X1, double Y0, double Y1, double Z)
		{
			Mesh.AddQuad({X0, Y0, Z}, {X1, Y0, Z}, {X1, Y1, Z}, {X0, Y1, Z}, -FVector::UpVector, true);
		}

		TArray<FPlane> RampInterior(double Y)
		{
			const FVector Bottom = RampPoint(0.0, Y);
			const FVector N = FVector(RampRise() / RampLength, 0.0, -1.0).GetSafeNormal();
			return {
				FPlane(Bottom - FVector(0.0, PassageWidth * 0.5, 0.0), -FVector::YAxisVector),
				FPlane(Bottom + FVector(0.0, PassageWidth * 0.5, 0.0), FVector::YAxisVector),
				FPlane(Bottom, -FVector::XAxisVector), FPlane(RampPoint(RampLength, Y), FVector::XAxisVector),
				FPlane(Bottom, N), FPlane(Bottom + FVector(0.0, 0.0, PassageHeight), -N)};
		}

		void Cut(FLNPFixtureMesh& Mesh, TConstArrayView<FPlane> LocalPlanes, const FTransform& Placement)
		{
			TArray<FPlane> WorldPlanes;
			for (const FPlane& P : LocalPlanes)
			{
				WorldPlanes.Emplace(Placement.TransformPosition(P.GetNormal() * P.W), Placement.TransformVectorNoScale(P.GetNormal()));
			}
			Mesh.RemoveInsideConvex(WorldPlanes);
		}

		void Run()
		{
			UWorld* World = LoadObject<UWorld>(nullptr, LNPRegressionFixture::LevelPath);
			if (!World) { UE_LOG(LogLNPVolumetricFixture, Error, TEXT("[VolumetricFixture] Base regression LVI is missing")); return; }
			AStaticMeshActor* CrustActor = nullptr;
			for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
			{
				if (It->GetActorLabel().StartsWith(TEXT("RF_07_")))
				{
					UE_LOG(LogLNPVolumetricFixture, Error, TEXT("[VolumetricFixture] Extension already exists; restore the backup before rebuilding"));
					return;
				}
				if (It->GetActorLabel() == TEXT("RF_00_Crust")) { CrustActor = *It; }
			}
			if (!CrustActor) { UE_LOG(LogLNPVolumetricFixture, Error, TEXT("[VolumetricFixture] Crust actor is missing")); return; }

			FLNPFixtureMesh Ground, Shell, Entrances, EntranceShell, Structure, ShaftFloor, ShaftShell;
			Floor(Ground, -RoomHalf, RoomHalf, -RoomHalf, RoomHalf);
			Floor(Ground, -600.0, 600.0, -3600.0, -RoomHalf);
			Roof(Shell, -RoomHalf, RoomHalf, -RoomHalf, RoomHalf, RoomHeight);
			Wall(Shell, {-RoomHalf, -RoomHalf, -30.0}, {-RoomHalf, RoomHalf, -30.0}, RoomHeight);
			Wall(Shell, {-RoomHalf, RoomHalf, -30.0}, {RoomHalf, RoomHalf, -30.0}, RoomHeight);
			for (const TPair<double, double>& Range : {TPair<double, double>(-RoomHalf, -1500.0),
				TPair<double, double>(-900.0, 900.0), TPair<double, double>(1500.0, RoomHalf)})
			{
				Wall(Shell, {RoomHalf, Range.Key, -30.0}, {RoomHalf, Range.Value, -30.0}, RoomHeight);
			}
			for (double Y : {-DoorY, DoorY})
			{
				Wall(Shell, {RoomHalf, Y - 300.0, FloorZ(RoomHalf, DoorY) + PassageHeight},
					{RoomHalf, Y + 300.0, FloorZ(RoomHalf, DoorY) + PassageHeight}, RoomHeight);
				const FVector A = RampPoint(0.0, Y - 300.0), B = RampPoint(RampLength, Y - 300.0);
				const FVector C = RampPoint(RampLength, Y + 300.0), D = RampPoint(0.0, Y + 300.0);
				Entrances.AddQuad(A, B, C, D, FVector::UpVector, false);
				EntranceShell.AddQuad(A + FVector(0.0, 0.0, PassageHeight), B + FVector(0.0, 0.0, PassageHeight),
					C + FVector(0.0, 0.0, PassageHeight), D + FVector(0.0, 0.0, PassageHeight), -FVector::UpVector, true);
				for (double Side : {-1.0, 1.0})
				{
					const FVector E = RampPoint(0.0, Y + Side * 300.0), F = RampPoint(RampLength, Y + Side * 300.0);
					EntranceShell.AddQuad(E - FVector(0.0, 0.0, 30.0), F - FVector(0.0, 0.0, 30.0),
						F + FVector(0.0, 0.0, PassageHeight), E + FVector(0.0, 0.0, PassageHeight), FVector::YAxisVector, true);
				}
			}
			// 낮은 분기 통로는 공동 바닥과 같은 source이며 천장만 낮아진다.
			Wall(Shell, {-RoomHalf, -RoomHalf, -30.0}, {-600.0, -RoomHalf, -30.0}, RoomHeight);
			Wall(Shell, {600.0, -RoomHalf, -30.0}, {RoomHalf, -RoomHalf, -30.0}, RoomHeight);
			Wall(Shell, {-600.0, -RoomHalf, FloorZ(0.0, RoomHalf) + LowHeight},
				{600.0, -RoomHalf, FloorZ(0.0, RoomHalf) + LowHeight}, RoomHeight);
			const double BranchRoof = FloorZ(0.0, 3600.0) + LowHeight;
			Roof(Shell, -600.0, 600.0, -3600.0, -RoomHalf, BranchRoof);
			Wall(Shell, {-600.0, -3600.0, -30.0}, {-600.0, -RoomHalf, -30.0}, BranchRoof);
			Wall(Shell, {600.0, -3600.0, -30.0}, {600.0, -RoomHalf, -30.0}, BranchRoof);
			Wall(Shell, {-600.0, -3600.0, -30.0}, {600.0, -3600.0, -30.0}, BranchRoof);

			Floor(Structure, 1100.0, 1900.0, -1800.0, 1800.0, UpperHeight);
			for (double Y : {-1500.0, 1500.0})
			{
				Structure.AddQuad(FloorPoint(-1900.0, Y - 200.0), FloorPoint(1100.0, Y - 200.0, UpperHeight),
					FloorPoint(1100.0, Y + 200.0, UpperHeight), FloorPoint(-1900.0, Y + 200.0), FVector::UpVector, false);
			}
			for (int32 S = 0; S < SpiralSegments; ++S)
			{
				for (int32 Band = 0; Band < 4; ++Band)
				{
					const double Inner = SpiralInner + Band * 100.0;
					const double Outer = Inner + 100.0;
					Structure.AddQuad(SpiralPoint(S, Inner), SpiralPoint(S, Outer),
						SpiralPoint(S + 1, Outer), SpiralPoint(S + 1, Inner), FVector::UpVector, false);
				}
			}
			const FVector EndA = SpiralPoint(SpiralSegments, SpiralInner), EndB = SpiralPoint(SpiralSegments, SpiralOuter);
			// 마지막 회전 위를 가로지르지 않고 나선 바깥으로 나온 뒤 발코니에 연결한다.
			const double EndAngle = UE_TWO_PI * SpiralTurns;
			const FVector ExitOffset(500.0 * FMath::Cos(EndAngle), 500.0 * FMath::Sin(EndAngle), 0.0);
			const FVector ExitA = FloorPoint(EndA.X + ExitOffset.X, EndA.Y + ExitOffset.Y, UpperHeight);
			const FVector ExitB = FloorPoint(EndB.X + ExitOffset.X, EndB.Y + ExitOffset.Y, UpperHeight);
			Structure.AddQuad(EndA, EndB, ExitB, ExitA, FVector::UpVector, false);
			for (int32 Step = 0; Step < 24; ++Step)
			{
				const double T0 = Step / 24.0, T1 = (Step + 1) / 24.0;
				Structure.AddQuad(FloorPoint(FMath::Lerp(ExitA.X, 1100.0, T0), ExitA.Y, UpperHeight),
					FloorPoint(FMath::Lerp(ExitA.X, 1100.0, T1), ExitA.Y, UpperHeight),
					FloorPoint(FMath::Lerp(ExitB.X, 1100.0, T1), ExitB.Y, UpperHeight),
					FloorPoint(FMath::Lerp(ExitB.X, 1100.0, T0), ExitB.Y, UpperHeight), FVector::UpVector, false);
			}
			// Support 단면의 뒷면 관통을 허용하지 않도록 30cm 아래의 밑면은 Shell Blocker로 둔다.
			for (const FIntVector& Triangle : Structure.Triangles)
			{
				const int32 A = Shell.AddVertex(Structure.Positions[Triangle.X] - FVector(0.0, 0.0, 30.0));
				const int32 B = Shell.AddVertex(Structure.Positions[Triangle.Y] - FVector(0.0, 0.0, 30.0));
				const int32 C = Shell.AddVertex(Structure.Positions[Triangle.Z] - FVector(0.0, 0.0, 30.0));
				Shell.AddTriangle(A, B, C, -FVector::UpVector);
			}

			// 별도 공동은 수직 통로만 열어 지상 Nav와 연결되지 않는다.
			Floor(ShaftFloor, -ShaftHalf, ShaftHalf, ShaftY - ShaftHalf, ShaftY + ShaftHalf);
			Roof(ShaftShell, -ShaftHalf, ShaftHalf, ShaftY - ShaftHalf, ShaftY - ShaftOpeningHalf, RoomHeight);
			Roof(ShaftShell, -ShaftHalf, ShaftHalf, ShaftY + ShaftOpeningHalf, ShaftY + ShaftHalf, RoomHeight);
			Roof(ShaftShell, -ShaftHalf, -ShaftOpeningHalf, ShaftY - ShaftOpeningHalf, ShaftY + ShaftOpeningHalf, RoomHeight);
			Roof(ShaftShell, ShaftOpeningHalf, ShaftHalf, ShaftY - ShaftOpeningHalf, ShaftY + ShaftOpeningHalf, RoomHeight);
			for (const double Side : {-1.0, 1.0})
			{
				Wall(ShaftShell, {Side * ShaftHalf, ShaftY - ShaftHalf, -30.0}, {Side * ShaftHalf, ShaftY + ShaftHalf, -30.0}, RoomHeight);
				Wall(ShaftShell, {-ShaftHalf, ShaftY + Side * ShaftHalf, -30.0}, {ShaftHalf, ShaftY + Side * ShaftHalf, -30.0}, RoomHeight);
				Wall(ShaftShell, {Side * ShaftOpeningHalf, ShaftY - ShaftOpeningHalf, RoomHeight},
					{Side * ShaftOpeningHalf, ShaftY + ShaftOpeningHalf, RoomHeight}, 4500.0);
				Wall(ShaftShell, {-ShaftOpeningHalf, ShaftY + Side * ShaftOpeningHalf, RoomHeight},
					{ShaftOpeningHalf, ShaftY + Side * ShaftOpeningHalf, RoomHeight}, 4500.0);
			}

			FLNPFixtureMesh Crust = LNPFixtureMesh::BuildSphereOctant(LNPRegressionFixture::CrustRadius, LNPRegressionFixture::CrustSubdivisions);
			const auto Cave = LNPRegressionFixture::Cave();
			const auto OldPlacement = LNPCaveKit::PlaceUnderSphere(Cave.Radial, Cave.Bitangent, LNPRegressionFixture::CrustRadius);
			Cut(Crust, LNPCaveKit::GetCorridorInteriorPlanes(), OldPlacement.Corridor);
			for (double Y : {-DoorY, DoorY}) { Cut(Crust, RampInterior(Y), Transform()); }
			const TArray<FPlane> ShaftPlanes = {
				FPlane(FVector(ShaftOpeningHalf, ShaftY, 0.0), FVector::XAxisVector),
				FPlane(FVector(-ShaftOpeningHalf, ShaftY, 0.0), -FVector::XAxisVector),
				FPlane(FVector(0.0, ShaftY + ShaftOpeningHalf, 0.0), FVector::YAxisVector),
				FPlane(FVector(0.0, ShaftY - ShaftOpeningHalf, 0.0), -FVector::YAxisVector),
				FPlane(FVector(0.0, 0.0, RoomHeight), -FVector::UpVector), FPlane(FVector(0.0, 0.0, 4500.0), FVector::UpVector)};
			Cut(Crust, ShaftPlanes, Transform());

			const FLNPFixtureMesh* Meshes[] = {&Ground, &Shell, &Entrances, &EntranceShell, &Structure, &ShaftFloor, &ShaftShell};
			const TCHAR* Names[] = {TEXT("Ground"), TEXT("Shell"), TEXT("Entrances"), TEXT("EntranceShell"), TEXT("Structure"), TEXT("ShaftFloor"), TEXT("ShaftShell")};
			TArray<UStaticMesh*> Assets;
			for (int32 I = 0; I < UE_ARRAY_COUNT(Meshes); ++I)
			{
				UStaticMesh* Mesh = LNPFixtureMesh::WriteStaticMesh(LNPRegressionFixture::FixtureFolder,
					FString(TEXT("SM_FixtureVolumetric_")) + Names[I], *Meshes[I]);
				if (!Mesh) { return; }
				Assets.Add(Mesh);
			}
			UStaticMesh* CrustMesh = LNPFixtureMesh::WriteStaticMesh(LNPRegressionFixture::FixtureFolder, LNPRegressionFixture::CrustMeshName, Crust);
			if (!CrustMesh) { return; }
			CrustActor->GetStaticMeshComponent()->SetStaticMesh(CrustMesh);
			for (int32 I = 0; I < Assets.Num(); ++I)
			{
				const bool bSupport = I == 0 || I == 2 || I == 4 || I == 5;
				TArray<FName> Tags = {FName(TEXT("LNP.Surface.Blocker")), FName(TEXT("LNP.Surface.Static"))};
				if (bSupport) { Tags.Add(FName(TEXT("LNP.Surface.Support"))); }
				if (I == 0) { Tags.Add(FName(TEXT("LNP.Surface.CoarseSupport"))); }
				LNPFixtureMesh::SpawnMeshActor(*World, *Assets[I], FString(TEXT("RF_07_")) + Names[I], Transform(),
					MoveTemp(Tags), FName(bSupport ? TEXT("LNPStaticTerrain") : TEXT("LNPStaticBlocker")));
			}
			// 계단 칸은 Decoration이며 실제 충돌은 위 Structure의 연속 경사면이다.
			UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
			UClass* AnchorClass = LoadClass<AActor>(nullptr, TEXT("/Game/WorldDevice/BP_GrappleAnchor.BP_GrappleAnchor_C"));
			if (!Cube || !AnchorClass) { UE_LOG(LogLNPVolumetricFixture, Error, TEXT("[VolumetricFixture] Decoration cube or grapple anchor class is missing")); return; }
			for (double Y : {-1500.0, 1500.0})
			{
				for (int32 Step = 0; Step < 30; ++Step)
				{
					const double X = -1850.0 + Step * 100.0;
					const FTransform Local(FQuat::Identity, FloorPoint(X, Y, (Step + 0.5) * 40.0 - 25.0), FVector(1.0, 4.0, 0.5));
					LNPFixtureMesh::SpawnMeshActor(*World, *Cube, FString::Printf(TEXT("RF_07_Step_%s_%02d"), Y < 0 ? TEXT("A") : TEXT("B"), Step),
						Local * Transform(), {FName(TEXT("LNP.Surface.Decoration"))}, FName(TEXT("LNPDecoration")));
				}
			}
			for (int32 Index = 0; Index < 2; ++Index)
			{
				const FVector Location = Transform().TransformPosition(FVector(0.0, ShaftY, 3000.0 + Index * 2100.0));
				ALNPPlacementMarker* Marker = World->SpawnActor<ALNPPlacementMarker>(Location, Transform().Rotator());
				Marker->SetActorLabel(FString::Printf(TEXT("RF_07_ShaftAnchor_%d"), Index));
				Marker->MarkerId = FGuid(0x070C0004, 0, 0, Index + 1);
				Marker->ElementClass = AnchorClass;
			}
			// 기존 anchor를 유지하고 공동의 지상·비행 편성을 직접 검증할 위치를 추가한다.
			const FVector SpawnFeet[] = {FloorPoint(1700.0, -600.0), FloorPoint(-1400.0, -900.0)};
			const FName SpawnSets[] = {TEXT("Bench01"), TEXT("Bench01_2")};
			for (int32 Index = 0; Index < 2; ++Index)
			{
				const FVector Location = Transform().TransformPosition(SpawnFeet[Index]);
				ALNPMassSpawnPoint* Point = World->SpawnActor<ALNPMassSpawnPoint>(Location, Transform().Rotator());
				Point->SetActorLabel(FString::Printf(TEXT("RF_07_Spawn_%d"), Index));
				Point->SpawnPointId = FGuid(0x070C0004, 1, 0, Index + 1);
				Point->TargetSpawnSetId = SpawnSets[Index];
			}
			const bool bSaved = LNPFixtureMesh::SavePackage(*World->GetOutermost(), *World, FPackageName::GetMapPackageExtension());
			UE_LOG(LogLNPVolumetricFixture, Display, TEXT("[VolumetricFixture] %s | floorRadius=%.0f height=%.0f rampLength=%.0f rise=%.2f meshActorsAdded=67 markersAdded=2 spawnAnchorsAdded=2"),
				bSaved ? TEXT("saved") : TEXT("FAILED"), FloorRadius, RoomHeight, RampLength, RampRise());
		}

		FAutoConsoleCommand Command(TEXT("LNP.SurfaceNav.ExtendRegressionFixture"),
			TEXT("Add Phase 7c volumetric cases to the existing regression LVI; preserves base actors and anchors. Back up the LVI and crust mesh first. Refuses duplicate extension."),
			FConsoleCommandDelegate::CreateStatic(&Run));
	}
}
