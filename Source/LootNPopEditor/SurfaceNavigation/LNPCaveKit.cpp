// Copyright (c) 2026 LootNPop. All rights reserved.

// 동굴 키트 greybox 모듈 생성. 규약은 LNPCaveKit.h 머리말과 `design/TerrainContract.md` §6.
//
// 예: LNP.SurfaceNav.BuildCaveKit
//     /Game/Maps/CaveKit의 키트 메시를 제자리 갱신한다. 배치된 LVI는 그대로 새 메시를 쓴다.

#include "SurfaceNavigation/LNPCaveKit.h"

#include "Engine/StaticMesh.h"
#include "HAL/IConsoleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogLNPCaveKit, Log, All);

namespace LNPCaveKit
{
	namespace
	{
		constexpr int32 FloorGridCells = 16;
	}

	double RoomFloorHeight(double X, double Y)
	{
		const double R = FloorCurvatureRadius;
		return R - FMath::Sqrt(R * R - X * X - Y * Y);
	}

	double RampSlope()
	{
		return FMath::Tan(FMath::DegreesToRadians(RampAngleDeg));
	}

	FTransform GetRoomDoorTransform()
	{
		return FTransform(FVector(RoomSize * 0.5, 0.0, RoomFloorHeight(RoomSize * 0.5, 0.0)));
	}

	FTransform GetCorridorMouthTransform()
	{
		return FTransform(FVector(RampLength, 0.0, RampLength * RampSlope()));
	}

	FLNPFixtureMesh BuildRoomFloor()
	{
		FLNPFixtureMesh Mesh;
		const double Half = RoomSize * 0.5;
		const double Step = RoomSize / FloorGridCells;
		auto Point = [Half, Step](int32 I, int32 J)
		{
			const double X = -Half + Step * I;
			const double Y = -Half + Step * J;
			return FVector(X, Y, RoomFloorHeight(X, Y));
		};

		TArray<int32> Index;
		for (int32 J = 0; J <= FloorGridCells; ++J)
		{
			for (int32 I = 0; I <= FloorGridCells; ++I)
			{
				Index.Add(Mesh.AddVertex(Point(I, J)));
			}
		}
		const int32 Row = FloorGridCells + 1;
		for (int32 J = 0; J < FloorGridCells; ++J)
		{
			for (int32 I = 0; I < FloorGridCells; ++I)
			{
				const int32 V00 = Index[J * Row + I];
				const int32 V10 = Index[J * Row + I + 1];
				const int32 V01 = Index[(J + 1) * Row + I];
				const int32 V11 = Index[(J + 1) * Row + I + 1];
				Mesh.AddTriangle(V00, V10, V11, FVector::UpVector);
				Mesh.AddTriangle(V00, V11, V01, FVector::UpVector);
			}
		}
		return Mesh;
	}

	FLNPFixtureMesh BuildRoomShell()
	{
		FLNPFixtureMesh Mesh;
		const double H = RoomSize * 0.5;
		const double Bottom = -WallSink;
		const double Top = RoomHeight;
		const double DoorHalf = PassageWidth * 0.5;
		const double DoorTop = RoomFloorHeight(H, 0.0) + PassageHeight;

		Mesh.AddQuad(FVector(-H, -H, Top), FVector(H, -H, Top), FVector(H, H, Top), FVector(-H, H, Top), -FVector::UpVector, true);
		Mesh.AddQuad(FVector(-H, -H, Bottom), FVector(-H, H, Bottom), FVector(-H, H, Top), FVector(-H, -H, Top), FVector::XAxisVector, true);
		Mesh.AddQuad(FVector(-H, -H, Bottom), FVector(H, -H, Bottom), FVector(H, -H, Top), FVector(-H, -H, Top), FVector::YAxisVector, true);
		Mesh.AddQuad(FVector(-H, H, Bottom), FVector(H, H, Bottom), FVector(H, H, Top), FVector(-H, H, Top), -FVector::YAxisVector, true);

		// +X 벽: 문 양옆 두 조각과 문 위 인방.
		Mesh.AddQuad(FVector(H, -H, Bottom), FVector(H, -DoorHalf, Bottom), FVector(H, -DoorHalf, Top), FVector(H, -H, Top), -FVector::XAxisVector, true);
		Mesh.AddQuad(FVector(H, DoorHalf, Bottom), FVector(H, H, Bottom), FVector(H, H, Top), FVector(H, DoorHalf, Top), -FVector::XAxisVector, true);
		Mesh.AddQuad(FVector(H, -DoorHalf, DoorTop), FVector(H, DoorHalf, DoorTop), FVector(H, DoorHalf, Top), FVector(H, -DoorHalf, Top), -FVector::XAxisVector, true);
		return Mesh;
	}

	FLNPFixtureMesh BuildCorridorFloor()
	{
		FLNPFixtureMesh Mesh;
		const double Half = PassageWidth * 0.5;
		const double TopZ = RampLength * RampSlope();
		Mesh.AddQuad(FVector(0.0, -Half, 0.0), FVector(RampLength, -Half, TopZ), FVector(RampLength, Half, TopZ), FVector(0.0, Half, 0.0),
			FVector::UpVector, false);
		return Mesh;
	}

	FLNPFixtureMesh BuildCorridorShell()
	{
		// 지각 위로 솟는 입구 구간도 천장·벽을 그대로 둔다. 지각 곡률·요철과 무관하게 입구 주변이 닫힌다.
		FLNPFixtureMesh Mesh;
		const double Half = PassageWidth * 0.5;
		const double TopZ = RampLength * RampSlope();
		const double L = RampLength;
		const double Ceil = PassageHeight;
		Mesh.AddQuad(FVector(0.0, -Half, Ceil), FVector(L, -Half, TopZ + Ceil), FVector(L, Half, TopZ + Ceil), FVector(0.0, Half, Ceil),
			-FVector::UpVector, true);
		for (const double Side : {-1.0, 1.0})
		{
			const double Y = Side * Half;
			Mesh.AddQuad(FVector(0.0, Y, -WallSink), FVector(L, Y, TopZ - WallSink), FVector(L, Y, TopZ + Ceil), FVector(0.0, Y, Ceil),
				FVector(0.0, -Side, 0.0), true);
		}
		return Mesh;
	}

	TArray<FPlane> GetCorridorInteriorPlanes()
	{
		const double Slope = RampSlope();
		const FVector FloorNormal = FVector(Slope, 0.0, -1.0).GetUnsafeNormal();
		return {
			FPlane(FVector(0.0, PassageWidth * 0.5, 0.0), FVector::YAxisVector),
			FPlane(FVector(0.0, -PassageWidth * 0.5, 0.0), -FVector::YAxisVector),
			FPlane(FVector::ZeroVector, -FVector::XAxisVector),
			FPlane(FVector(RampLength, 0.0, 0.0), FVector::XAxisVector),
			FPlane(FVector::ZeroVector, FloorNormal),
			FPlane(FVector(0.0, 0.0, PassageHeight), -FloorNormal),
		};
	}

	FPlacement PlaceUnderSphere(const FVector& RoomDirection, const FVector& Forward, double CrustRadius)
	{
		const FVector Up = -RoomDirection.GetSafeNormal();
		const FQuat Rotation = FRotationMatrix::MakeFromZX(Up, Forward).ToQuat();

		// Mouth = 공동 피벗 + X·(d) + Z·(zM)이고 공동 피벗은 방향 -Up·rf에 있으므로 |Mouth|² = (rf - zM)² + d².
		const FVector Door = GetRoomDoorTransform().GetLocation();
		const FVector Mouth = GetCorridorMouthTransform().GetLocation();
		const double D = Door.X + Mouth.X;
		const double ZM = Door.Z + Mouth.Z;
		const double FloorRadius = ZM + FMath::Sqrt(CrustRadius * CrustRadius - D * D);

		FPlacement Placement;
		Placement.Room = FTransform(Rotation, -Up * FloorRadius);
		Placement.Corridor = GetRoomDoorTransform() * Placement.Room;
		return Placement;
	}

	bool WriteKitAssets()
	{
		const TPair<FName, FTransform> Door(DoorSocket, GetRoomDoorTransform());
		const TPair<FName, FTransform> Lower(LowerSocket, FTransform::Identity);
		const TPair<FName, FTransform> Mouth(MouthSocket, GetCorridorMouthTransform());

		bool bOk = true;
		bOk &= LNPFixtureMesh::WriteStaticMesh(KitFolder, RoomFloorName, BuildRoomFloor(), {Door}) != nullptr;
		bOk &= LNPFixtureMesh::WriteStaticMesh(KitFolder, RoomShellName, BuildRoomShell(), {Door}) != nullptr;
		bOk &= LNPFixtureMesh::WriteStaticMesh(KitFolder, CorridorFloorName, BuildCorridorFloor(), {Lower, Mouth}) != nullptr;
		bOk &= LNPFixtureMesh::WriteStaticMesh(KitFolder, CorridorShellName, BuildCorridorShell(), {Lower, Mouth}) != nullptr;
		return bOk;
	}

	static FAutoConsoleCommand BuildCommand(
		TEXT("LNP.SurfaceNav.BuildCaveKit"),
		TEXT("Create or update the greybox cave kit meshes (box room, ramp corridor; floor and shell each) under /Game/Maps/CaveKit."),
		FConsoleCommandDelegate::CreateLambda([]
		{
			const bool bOk = WriteKitAssets();
			UE_LOG(LogLNPCaveKit, Display, TEXT("[CaveKit] %s | Room=%.0f Height=%.0f Passage=%.0fx%.0f Ramp=%.0f@%.0fdeg"),
				bOk ? TEXT("saved") : TEXT("FAILED"), RoomSize, RoomHeight, PassageWidth, PassageHeight, RampLength, RampAngleDeg);
		}));
}
