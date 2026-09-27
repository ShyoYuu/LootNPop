// Copyright (c) 2026 LootNPop. All rights reserved.

// 열린 옥탄트 LVI에 동굴 키트(공동 + 경사 통로)를 배치하고 지각 메시에 입구를 낸다(`design/TerrainContract.md` §6).
//
// 예: LNP.SurfaceNav.PlaceCaveKit 18 72 0          미리보기. 지붕 두께·입구 절단 범위·주변 프랍만 보고한다
//     LNP.SurfaceNav.PlaceCaveKit 18 72 0 Apply    지각 메시를 자르고 키트 액터 4개를 배치한다(저장은 호출자가 한다)
// 인자: 공동 중심 위도·방위(도), 통로 방향(접평면에서 위도 증가 쪽 기준 방위 증가 쪽으로 회전한 각, 도).
//
// 지각은 노이즈·Sculpt가 있으므로 입구 바닥점이 실제 지각 위에 오도록 입구 방향의 지각 반지름을 반복해 맞춘다.
// 지각 메시는 재생성하지 않고 기존 메시를 통로 내부 볼록 영역의 평면으로 쪼갠 뒤 안쪽 삼각형만 지운다.
// 분할은 통로 근처 모서리로 한정하므로 이음매 정점은 바뀌지 않고, 새 정점의 UV·법선은 보간된다.

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMeshToMeshDescription.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "MeshDescription.h"
#include "MeshDescriptionToDynamicMesh.h"
#include "Operations/MeshPlaneCut.h"
#include "SurfaceNavigation/LNPCaveKit.h"
#include "SurfaceNavigation/LNPFixtureMesh.h"
#include "SurfaceNavigation/LNPOctantTriangleExtractor.h"
#include "SurfaceNavigation/LNPSurfaceBakeGeometry.h"

DEFINE_LOG_CATEGORY_STATIC(LogLNPCaveKitPlacer, Log, All);

namespace LNPCaveKitPlacer
{
	namespace
	{
		const FName SupportTag(TEXT("LNP.Surface.Support"));
		const FName BlockerTag(TEXT("LNP.Surface.Blocker"));
		const FName StaticTag(TEXT("LNP.Surface.Static"));
		const TCHAR* const LabelPrefix = TEXT("CaveKit_00_");

		/** 공동 천장과 지각 사이에 남길 최소 두께(cm). */
		constexpr double MinRoofThickness = 100.0;
		/** 평면 분할을 허용할 모서리의 통로 내부 영역 여유(cm). 지각 격자 간격보다 커야 경계 삼각형이 온전히 쪼개진다. */
		constexpr double SplitMargin = 400.0;

		/** 원점에서 Direction 쪽 광선이 지각과 만나는 가장 가까운 반지름. 없으면 음수. */
		double CrustRadiusAlong(const FLNPBakeTriangleMesh& Crust, const FVector& Direction)
		{
			const FVector3d Dir(Direction.GetSafeNormal());
			double Best = -1.0;
			for (const FIntVector3& T : Crust.Triangles)
			{
				const FVector3d& A = Crust.Vertices[T.X];
				const FVector3d E1 = Crust.Vertices[T.Y] - A;
				const FVector3d E2 = Crust.Vertices[T.Z] - A;
				const FVector3d P = Dir.Cross(E2);
				const double Det = E1.Dot(P);
				if (FMath::Abs(Det) < 1e-9)
					continue;
				const double Inv = 1.0 / Det;
				const double U = (-A).Dot(P) * Inv;
				if (U < 0.0 || U > 1.0)
					continue;
				const FVector3d Q = (-A).Cross(E1);
				const double V = Dir.Dot(Q) * Inv;
				if (V < 0.0 || U + V > 1.0)
					continue;
				const double Distance = E2.Dot(Q) * Inv;
				if (Distance > 0.0 && (Best < 0.0 || Distance < Best))
					Best = Distance;
			}
			return Best;
		}

		bool InsideAll(TConstArrayView<FPlane> Planes, const FVector& Point, double Margin = 0.0)
		{
			for (const FPlane& Plane : Planes)
			{
				if (Plane.PlaneDot(Point) >= Margin)
					return false;
			}
			return true;
		}

		/** 모서리 AB가 Margin만큼 넓힌 볼록 영역과 겹칠 수 있는지. 한 평면 바깥에 둘 다 있으면 겹치지 않는다. */
		bool EdgeNearRegion(TConstArrayView<FPlane> Planes, const FVector& A, const FVector& B, double Margin)
		{
			for (const FPlane& Plane : Planes)
			{
				if (Plane.PlaneDot(A) >= Margin && Plane.PlaneDot(B) >= Margin)
					return false;
			}
			return true;
		}

		/** 지각 메시를 Planes(메시 로컬) 볼록 영역으로 잘라낸다. 지운 삼각형 수를 돌려준다. */
		int32 CutMesh(UStaticMesh& Mesh, TConstArrayView<FPlane> Planes)
		{
			FMeshDescription* Description = Mesh.GetMeshDescription(0);
			if (!Description)
				return -1;

			UE::Geometry::FDynamicMesh3 Dynamic;
			FMeshDescriptionToDynamicMesh ToDynamic;
			ToDynamic.Convert(Description, Dynamic, /*bCopyTangents=*/true);

			for (const FPlane& Plane : Planes)
			{
				UE::Geometry::FMeshPlaneCut Cut(&Dynamic, FVector3d(Plane.GetNormal() * Plane.W), FVector3d(Plane.GetNormal()));
				Cut.bCollapseDegenerateEdgesOnCut = false;
				Cut.EdgeFilterFunc = [&Dynamic, Planes](int32 EdgeId)
				{
					const UE::Geometry::FIndex2i Vertices = Dynamic.GetEdgeV(EdgeId);
					return EdgeNearRegion(Planes, FVector(Dynamic.GetVertex(Vertices.A)), FVector(Dynamic.GetVertex(Vertices.B)), SplitMargin);
				};
				Cut.SplitEdgesOnly(false, nullptr);
			}

			TArray<int32> Remove;
			for (const int32 TriangleId : Dynamic.TriangleIndicesItr())
			{
				const UE::Geometry::FIndex3i T = Dynamic.GetTriangle(TriangleId);
				const FVector Centroid = FVector(Dynamic.GetVertex(T.A) + Dynamic.GetVertex(T.B) + Dynamic.GetVertex(T.C)) / 3.0;
				if (InsideAll(Planes, Centroid))
					Remove.Add(TriangleId);
			}
			for (const int32 TriangleId : Remove)
			{
				Dynamic.RemoveTriangle(TriangleId, /*bRemoveIsolatedVertices=*/true);
			}

			Mesh.Modify();
			FDynamicMeshToMeshDescription ToDescription;
			ToDescription.Convert(&Dynamic, *Description, /*bCopyTangents=*/true);
			Mesh.CommitMeshDescription(0);
			Mesh.Build(false);
			Mesh.PostEditChange();
			Mesh.MarkPackageDirty();
			return Remove.Num();
		}

		void Run(const TArray<FString>& Args)
		{
			if (Args.Num() < 3)
			{
				UE_LOG(LogLNPCaveKitPlacer, Error, TEXT("[PlaceCaveKit] Usage: LNP.SurfaceNav.PlaceCaveKit <LatDeg> <AzDeg> <HeadingDeg> [Apply]"));
				return;
			}
			const double Lat = FCString::Atod(*Args[0]);
			const double Az = FCString::Atod(*Args[1]);
			const double Heading = FMath::DegreesToRadians(FCString::Atod(*Args[2]));
			const bool bApply = Args.Num() > 3 && Args[3].Equals(TEXT("Apply"), ESearchCase::IgnoreCase);

			UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
			if (!World || !World->PersistentLevel)
			{
				UE_LOG(LogLNPCaveKitPlacer, Error, TEXT("[PlaceCaveKit] No editor world"));
				return;
			}
			for (const AActor* Actor : World->PersistentLevel->Actors)
			{
				if (IsValid(Actor) && Actor->GetActorLabel().StartsWith(LabelPrefix))
				{
					UE_LOG(LogLNPCaveKitPlacer, Error, TEXT("[PlaceCaveKit] %s already has a cave kit (%s). Remove it first."),
						*World->GetOutermost()->GetName(), *Actor->GetActorLabel());
					return;
				}
			}

			// 지각 식별(D-055)과 source 좌표계 삼각형.
			TArray<FLNPBakeSupportSource> Sources;
			FString Error;
			int32 CrustIndex = INDEX_NONE;
			if (!FLNPOctantTriangleExtractor::ExtractSupportSources(*World, Sources, Error) || !LNPSurfaceBake::IdentifyCrust(Sources, CrustIndex, Error))
			{
				UE_LOG(LogLNPCaveKitPlacer, Error, TEXT("[PlaceCaveKit] %s"), *Error);
				return;
			}
			const FLNPBakeTriangleMesh& Crust = Sources[CrustIndex].Mesh;
			UStaticMeshComponent* CrustComponent = FindObject<UStaticMeshComponent>(nullptr, *Sources[CrustIndex].Name);
			if (!CrustComponent || !CrustComponent->GetStaticMesh())
			{
				UE_LOG(LogLNPCaveKitPlacer, Error, TEXT("[PlaceCaveKit] Crust component %s does not resolve"), *Sources[CrustIndex].Name);
				return;
			}

			// 입구 바닥점이 입구 방향의 실제 지각 반지름에 오도록 반복한다.
			const FVector RoomDirection = LNPFixtureMesh::DirectionFromLatAz(Lat, Az);
			const FVector North(-FMath::Sin(FMath::DegreesToRadians(Lat)) * FMath::Cos(FMath::DegreesToRadians(Az)),
				-FMath::Sin(FMath::DegreesToRadians(Lat)) * FMath::Sin(FMath::DegreesToRadians(Az)), FMath::Cos(FMath::DegreesToRadians(Lat)));
			const FVector East(-FMath::Sin(FMath::DegreesToRadians(Az)), FMath::Cos(FMath::DegreesToRadians(Az)), 0.0);
			const FVector Forward = North * FMath::Cos(Heading) + East * FMath::Sin(Heading);
			const FVector MouthLocal = LNPCaveKit::GetCorridorMouthTransform().GetLocation();

			double MouthCrustRadius = 30000.0;
			LNPCaveKit::FPlacement Placement;
			for (int32 Iteration = 0; Iteration < 8; ++Iteration)
			{
				Placement = LNPCaveKit::PlaceUnderSphere(RoomDirection, Forward, MouthCrustRadius);
				const double Measured = CrustRadiusAlong(Crust, Placement.Corridor.TransformPosition(MouthLocal));
				if (Measured < 0.0)
				{
					UE_LOG(LogLNPCaveKitPlacer, Error, TEXT("[PlaceCaveKit] No crust above the corridor mouth"));
					return;
				}
				const bool bConverged = FMath::Abs(Measured - MouthCrustRadius) < 0.01;
				MouthCrustRadius = Measured;
				if (bConverged)
					break;
			}

			// 공동 지붕: 천장 표본점마다 같은 방향의 지각이 천장보다 MinRoofThickness 이상 중심 쪽에 있어야 한다.
			double MinRoof = TNumericLimits<double>::Max();
			const double Half = LNPCaveKit::RoomSize * 0.5;
			for (int32 I = 0; I <= 4; ++I)
			{
				for (int32 J = 0; J <= 4; ++J)
				{
					const FVector Ceiling = Placement.Room.TransformPosition(FVector(-Half + Half * I / 2.0, -Half + Half * J / 2.0, LNPCaveKit::RoomHeight));
					const double CrustR = CrustRadiusAlong(Crust, Ceiling);
					MinRoof = FMath::Min(MinRoof, CrustR < 0.0 ? -1.0 : Ceiling.Size() - CrustR);
				}
			}

			// 입구 절단 범위: 통로 내부에 들어오는 지각 삼각형의 통로 로컬 x 범위.
			TArray<FPlane> TubePlanes;
			for (const FPlane& Local : LNPCaveKit::GetCorridorInteriorPlanes())
			{
				TubePlanes.Emplace(Placement.Corridor.TransformPosition(Local.GetNormal() * Local.W), Placement.Corridor.TransformVectorNoScale(Local.GetNormal()));
			}
			int32 InsideTriangles = 0;
			double CutMinX = TNumericLimits<double>::Max();
			for (const FIntVector3& T : Crust.Triangles)
			{
				const FVector Centroid((Crust.Vertices[T.X] + Crust.Vertices[T.Y] + Crust.Vertices[T.Z]) / 3.0);
				if (InsideAll(TubePlanes, Centroid, 50.0))
				{
					++InsideTriangles;
					CutMinX = FMath::Min(CutMinX, Placement.Corridor.InverseTransformPosition(Centroid).X);
				}
			}

			// 입구 주변 프랍: 통로 폭 + 양옆 300cm, 입구 절단 시작 300cm 앞부터 입구 500cm 너머까지.
			int32 NearProps = 0;
			for (const AActor* Actor : World->PersistentLevel->Actors)
			{
				if (!IsValid(Actor))
					continue;
				TInlineComponentArray<UInstancedStaticMeshComponent*> Instanced(Actor);
				for (const UInstancedStaticMeshComponent* Component : Instanced)
				{
					for (int32 Index = 0; Index < Component->GetInstanceCount(); ++Index)
					{
						FTransform Instance;
						Component->GetInstanceTransform(Index, Instance, /*bWorldSpace=*/true);
						const FVector Local = Placement.Corridor.InverseTransformPosition(Instance.GetLocation());
						NearProps += (FMath::Abs(Local.Y) < LNPCaveKit::PassageWidth * 0.5 + 300.0 && Local.X > CutMinX - 300.0
							&& Local.X < LNPCaveKit::RampLength + 500.0) ? 1 : 0;
					}
				}
			}

			const FVector Mouth = Placement.Corridor.TransformPosition(MouthLocal);
			const FVector RoomLocation = Placement.Room.GetLocation();
			UE_LOG(LogLNPCaveKitPlacer, Display,
				TEXT("[PlaceCaveKit] %s | Room lat=%.2f az=%.2f floorR=%.1f | Mouth lat=%.2f az=%.2f crustR=%.1f | ")
				TEXT("MinRoof=%.1fcm (need %.0f) | CrustInTube=%d tris, x>=%.0f (mouth %.0f) | PropsNearEntrance=%d"),
				bApply ? TEXT("APPLY") : TEXT("PREVIEW"), Lat, Az, RoomLocation.Size(),
				FMath::RadiansToDegrees(FMath::Asin(Mouth.Z / Mouth.Size())), FMath::RadiansToDegrees(FMath::Atan2(Mouth.Y, Mouth.X)), MouthCrustRadius,
				MinRoof, MinRoofThickness, InsideTriangles, CutMinX, LNPCaveKit::RampLength, NearProps);

			// int16 복제 캡은 베이커가 Support source마다 검사한다(ValidateSupportSource).
			if (MinRoof < MinRoofThickness)
			{
				UE_LOG(LogLNPCaveKitPlacer, Error, TEXT("[PlaceCaveKit] Placement rejected: the crust is too close to the room ceiling"));
				return;
			}
			if (!bApply)
				return;

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
				UE_LOG(LogLNPCaveKitPlacer, Error, TEXT("[PlaceCaveKit] Cave kit meshes are missing. Run LNP.SurfaceNav.BuildCaveKit first."));
				return;
			}

			// 평면을 지각 메시 로컬로 옮겨 자른다.
			const FTransform CrustTransform = FLNPOctantTriangleExtractor::GetSourceTransform(*CrustComponent);
			TArray<FPlane> MeshPlanes;
			for (const FPlane& Plane : TubePlanes)
			{
				MeshPlanes.Emplace(CrustTransform.InverseTransformPosition(Plane.GetNormal() * Plane.W),
					CrustTransform.InverseTransformVectorNoScale(Plane.GetNormal()).GetSafeNormal());
			}
			const int32 Removed = CutMesh(*CrustComponent->GetStaticMesh(), MeshPlanes);

			auto Spawn = [World](UStaticMesh& Mesh, const TCHAR* Suffix, const FTransform& Transform, bool bSupport)
			{
				TArray<FName> Tags = bSupport ? TArray<FName>{SupportTag, BlockerTag, StaticTag} : TArray<FName>{BlockerTag, StaticTag};
				AStaticMeshActor* Actor = LNPFixtureMesh::SpawnMeshActor(*World, Mesh, FString(LabelPrefix) + Suffix, Transform, MoveTemp(Tags),
					bSupport ? TEXT("LNPStaticTerrain") : TEXT("LNPStaticBlocker"));
				if (Actor)
				{
					Actor->SetFolderPath(TEXT("CaveKit"));
				}
			};
			Spawn(*RoomFloor, TEXT("Room_Floor"), Placement.Room, true);
			Spawn(*RoomShell, TEXT("Room_Shell"), Placement.Room, false);
			Spawn(*CorridorFloor, TEXT("Corridor_Floor"), Placement.Corridor, true);
			Spawn(*CorridorShell, TEXT("Corridor_Shell"), Placement.Corridor, false);
			World->MarkPackageDirty();

			UE_LOG(LogLNPCaveKitPlacer, Display, TEXT("[PlaceCaveKit] Applied: crust %s removed %d triangles, 4 kit actors spawned. Save dirty packages and rebake."),
				*CrustComponent->GetStaticMesh()->GetName(), Removed);
		}

		FAutoConsoleCommand Command(
			TEXT("LNP.SurfaceNav.PlaceCaveKit"),
			TEXT("Place the cave kit (box room + ramp corridor) under the crust of the open octant LVI. ")
			TEXT("Args: <LatDeg> <AzDeg> <HeadingDeg> [Apply]. Without Apply it only reports roof thickness, entrance cut and nearby props. ")
			TEXT("Apply cuts the crust mesh along the corridor interior and spawns the kit actors; save and rebake afterwards."),
			FConsoleCommandWithArgsDelegate::CreateStatic(&Run));
	}
}
