// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/StaticMeshComponent.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Engine/Engine.h"
#include "Engine/Level.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMeshSocket.h"
#include "Engine/World.h"
#include "GameLogic/LNPOctantSpawnSubsystem.h"
#include "SurfaceNavigation/LNPCaveKit.h"
#include "SurfaceNavigation/LNPHitIdentityRegistry.h"
#include "SurfaceNavigation/LNPMassWorldCollision.h"
#include "SurfaceNavigation/LNPOctantTriangleExtractor.h"
#include "SurfaceNavigation/LNPRegressionFixture.h"
#include "Modules/ModuleManager.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPRegressionFixtureExactTest,
	"LootNPop.SurfaceNavigation.WorldCollision.RegressionFixture",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPMeadowCaveKitTest,
	"LootNPop.SurfaceNavigation.WorldCollision.MeadowCaveKit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPCaveKitContractTest,
	"LootNPop.SurfaceNavigation.Bake.CaveKitContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace LNPRegressionFixtureTest
{
	using namespace LNPRegressionFixture;

	/** fixture LVI의 fixture actor 수와 그중 exact source(Decoration 제외) 수. */
	constexpr int32 ExpectedActorsPerSlot = 29;
	constexpr int32 ExpectedExactSourcesPerSlot = 15;

	/** 완전 구면 지각을 평면 삼각형으로 근사한 반지름 오차(0.4cm 이하)를 덮는다. */
	constexpr double Tolerance = 1.0;

	/** slot 회전을 적용한 사례 틀. 로컬 좌표로 위치를 만들고 월드로 옮긴다. */
	struct FSlotFrame
	{
		FQuat Rotation;
		FCaseFrame Local;

		FSlotFrame(const FQuat& InRotation, const FCaseFrame& InLocal) : Rotation(InRotation), Local(InLocal) {}

		FVector At(double Radius, double TangentOffset = 0.0, double BitangentOffset = 0.0) const
		{
			return Rotation.RotateVector(Local.At(Radius, TangentOffset, BitangentOffset));
		}
		FVector Radial() const { return Rotation.RotateVector(Local.Radial); }
		FVector Tangent() const { return Rotation.RotateVector(Local.Tangent); }
		double RadiusOf(const FVector& Point) const { return FVector::DotProduct(Point, Radial()); }
		double TangentOf(const FVector& Point) const { return FVector::DotProduct(Point, Tangent()); }
	};

	/** 양면 ray-삼각형 교차(Möller–Trumbore). 맞으면 ray 매개변수 T(0~1)를 돌려준다. */
	bool IntersectTriangle(const FVector3d& Start, const FVector3d& Delta, const FVector3d& A, const FVector3d& B, const FVector3d& C, double& OutT)
	{
		const FVector3d E1 = B - A;
		const FVector3d E2 = C - A;
		const FVector3d P = Delta.Cross(E2);
		const double Det = E1.Dot(P);
		if (FMath::Abs(Det) < 1e-12)
		{
			return false;
		}
		const double InvDet = 1.0 / Det;
		const FVector3d S = Start - A;
		const double U = S.Dot(P) * InvDet;
		if (U < 0.0 || U > 1.0)
		{
			return false;
		}
		const FVector3d Q = S.Cross(E1);
		const double V = Delta.Dot(Q) * InvDet;
		if (V < 0.0 || U + V > 1.0)
		{
			return false;
		}
		OutT = E2.Dot(Q) * InvDet;
		return OutT >= 0.0 && OutT <= 1.0;
	}

	/** Start→End에서 가장 가까운 교차 거리 비율. 없으면 false. */
	bool RaycastMesh(const FLNPBakeTriangleMesh& Mesh, const FVector3d& Start, const FVector3d& End, double& OutT)
	{
		OutT = TNumericLimits<double>::Max();
		const FVector3d Delta = End - Start;
		for (const FIntVector3& Triangle : Mesh.Triangles)
		{
			double T;
			if (IntersectTriangle(Start, Delta, Mesh.Vertices[Triangle.X], Mesh.Vertices[Triangle.Y], Mesh.Vertices[Triangle.Z], T))
			{
				OutT = FMath::Min(OutT, T);
			}
		}
		return OutT <= 1.0;
	}

	/** 메시 로컬 (X, Y)에서 바닥 높이. 수직 ray의 가장 위쪽 교차다. */
	bool FloorHeightAt(const FLNPBakeTriangleMesh& Floor, double X, double Y, double& OutZ)
	{
		double T;
		if (!RaycastMesh(Floor, FVector3d(X, Y, 10000.0), FVector3d(X, Y, -10000.0), T))
		{
			return false;
		}
		OutZ = 10000.0 - 20000.0 * T;
		return true;
	}
}

/**
 * `design/RegressionMap.md` §3의 정적 사례를 fixture LVI로 확인한다. LVI를 8 slot 회전으로 테스트 월드에 복제해
 * 모든 slot에서 같은 기대값이 나오는지, 이음매·꼭짓점에서 이웃 slot 지각이 이어지는지 본다.
 */
bool FLNPRegressionFixtureExactTest::RunTest(const FString& Parameters)
{
	using namespace LNPRegressionFixtureTest;
	using namespace ELNPExactSourceRole;

	UPackage* MapPackage = LoadPackage(nullptr, *FPackageName::ObjectPathToPackageName(FString(LevelPath)), LOAD_None);
	UWorld* MapWorld = MapPackage ? UWorld::FindWorldInPackage(MapPackage) : nullptr;
	if (!TestNotNull(TEXT("Regression fixture LVI loads"), MapWorld) || !TestNotNull(TEXT("LVI has a level"), MapWorld->PersistentLevel.Get()))
		return false;

	UWorld* World = UWorld::CreateWorld(EWorldType::Game, /*bInformEngineOfWorld=*/false, TEXT("LNPRegressionFixtureTest"));
	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	WorldContext.SetCurrentWorld(World);
	ON_SCOPE_EXIT
	{
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
	};

	ULNPHitIdentitySubsystem* HitIdentity = World->GetSubsystem<ULNPHitIdentitySubsystem>();
	ULNPMassWorldCollisionSubsystem* Collision = World->GetSubsystem<ULNPMassWorldCollisionSubsystem>();
	if (!TestNotNull(TEXT("Hit identity subsystem exists"), HitIdentity) || !TestNotNull(TEXT("World collision subsystem exists"), Collision))
		return false;

	constexpr int32 SlotCount = UE_ARRAY_COUNT(ULNPOctantSpawnSubsystem::OctantRotations);
	int32 ActorCount = 0;
	int32 RegisteredCount = 0;
	for (int32 Slot = 0; Slot < SlotCount; ++Slot)
	{
		const FTransform SlotTransform(ULNPOctantSpawnSubsystem::OctantRotations[Slot]);
		for (const AActor* MapActor : MapWorld->PersistentLevel->Actors)
		{
			const AStaticMeshActor* MeshActor = Cast<AStaticMeshActor>(MapActor);
			if (!MeshActor)
				continue;

			const UStaticMeshComponent* Source = MeshActor->GetStaticMeshComponent();
			AActor* Owner = World->SpawnActor<AActor>();
			UStaticMeshComponent* Copy = NewObject<UStaticMeshComponent>(Owner);
			Copy->SetStaticMesh(Source->GetStaticMesh());
			Copy->SetMobility(Source->Mobility);
			Copy->SetCollisionProfileName(Source->GetCollisionProfileName());
			Copy->SetWorldTransform(Source->GetRelativeTransform() * SlotTransform);
			Owner->SetRootComponent(Copy);
			Copy->RegisterComponent();
			++ActorCount;

			ELNPExactSourceLifetime Lifetime;
			uint8 Roles;
			if (ULNPHitIdentitySubsystem::ClassifyProfile(Copy->GetCollisionProfileName(), Lifetime, Roles))
			{
				HitIdentity->RegisterRuntimeSource(Copy);
				++RegisteredCount;
			}
		}

		// Pawn 제외: 정적 프랍 사례 지면 앞에 Pawn profile 판을 둔다. 등록하지 않으므로 맞으면 Unknown counter가 오른다.
		const FSlotFrame Props(SlotTransform.GetRotation(), StaticProps());
		AActor* PawnOwner = World->SpawnActor<AActor>();
		UStaticMeshComponent* PawnSlab = NewObject<UStaticMeshComponent>(PawnOwner);
		PawnSlab->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
		PawnSlab->SetMobility(EComponentMobility::Movable);
		PawnSlab->SetCollisionProfileName(TEXT("Pawn"));
		PawnSlab->SetWorldLocation(Props.At(CrustRadius - 500.0, -DecorationTangent));
		PawnOwner->SetRootComponent(PawnSlab);
		PawnSlab->RegisterComponent();
	}
	TestEqual(TEXT("Fixture actor count"), ActorCount, ExpectedActorsPerSlot * SlotCount);
	TestEqual(TEXT("Exact source count"), RegisteredCount, ExpectedExactSourcesPerSlot * SlotCount);
	HitIdentity->Tick(0.f);
	Collision->ResetStats();

	const FLNPWorldQueryParams Query(ELNPWorldQueryClass::DebugValidation);
	const LNPCaveKit::FPlacement CavePlacement = LNPCaveKit::PlaceUnderSphere(Cave().Radial, Cave().Bitangent, CrustRadius);
	TestEqual(TEXT("Cave: corridor mouth lies on the crust"),
		CavePlacement.Corridor.TransformPosition(LNPCaveKit::GetCorridorMouthTransform().GetLocation()).Size(), CrustRadius, 0.01);

	for (int32 Slot = 0; Slot < SlotCount; ++Slot)
	{
		const FQuat SlotRotation = ULNPOctantSpawnSubsystem::OctantRotations[Slot].Quaternion();
		auto Name = [Slot](const TCHAR* Text) { return FString::Printf(TEXT("Slot %d %s"), Slot, Text); };

		// 1. 기본 지각: 바깥 ray가 지각 안쪽 면에 맞는다.
		{
			const FSlotFrame F(SlotRotation, BasicCrust());
			FLNPWorldHit Hit;
			TestTrue(Name(TEXT("BasicCrust: outward ray hits")), Collision->RaycastWorld(F.At(CrustRadius - 1000.0), F.At(CrustRadius + 1000.0), Query, Hit));
			TestEqual(Name(TEXT("BasicCrust: hit radius")), F.RadiusOf(Hit.ImpactPoint), CrustRadius, Tolerance);
			TestTrue(Name(TEXT("BasicCrust: normal faces the center")), FVector::DotProduct(Hit.ImpactNormal, -F.Radial()) > 0.99);
			TestTrue(Name(TEXT("BasicCrust: static support")),
				Hit.Identity.Lifetime == ELNPExactSourceLifetime::Static && (Hit.Identity.Roles & Support) != 0);
		}

		// 2. 부유섬 하나: 섬 윗면(Support)이 먼저, 섬 밑에서 출발한 ray는 지각에 맞는다.
		{
			const FSlotFrame F(SlotRotation, IslandOne());
			FLNPWorldHit Hit;
			TestTrue(Name(TEXT("IslandOne: island hit")), Collision->RaycastWorld(F.At(IslandOneTop - 1000.0), F.At(CrustRadius + 1000.0), Query, Hit));
			TestEqual(Name(TEXT("IslandOne: top radius")), F.RadiusOf(Hit.ImpactPoint), IslandOneTop, Tolerance);
			TestTrue(Name(TEXT("IslandOne: top is support")), (Hit.Identity.Roles & Support) != 0);
			TestTrue(Name(TEXT("IslandOne: crust behind island")),
				Collision->RaycastWorld(F.At(IslandOneTop + IslandThickness + 50.0), F.At(CrustRadius + 1000.0), Query, Hit));
			TestEqual(Name(TEXT("IslandOne: crust radius")), F.RadiusOf(Hit.ImpactPoint), CrustRadius, Tolerance);
		}

		// 3. 부유섬 둘: 안쪽 섬 → 바깥 섬 → 지각 순서로 구분된다.
		{
			const FSlotFrame F(SlotRotation, IslandTwo());
			const double Starts[] = {IslandTwoInnerTop - 900.0, IslandTwoInnerTop + IslandThickness + 100.0, IslandTwoOuterTop + IslandThickness + 100.0};
			const double Expected[] = {IslandTwoInnerTop, IslandTwoOuterTop, CrustRadius};
			for (int32 Index = 0; Index < UE_ARRAY_COUNT(Starts); ++Index)
			{
				FLNPWorldHit Hit;
				TestTrue(Name(*FString::Printf(TEXT("IslandTwo: layer %d hit"), Index)),
					Collision->RaycastWorld(F.At(Starts[Index]), F.At(CrustRadius + 1000.0), Query, Hit));
				TestEqual(Name(*FString::Printf(TEXT("IslandTwo: layer %d radius"), Index)), F.RadiusOf(Hit.ImpactPoint), Expected[Index], Tolerance);
			}
		}

		// 4·5. 섬 가장자리: 안쪽(접선 550)은 윗면 support, 바깥(750)은 support miss, 측벽(접선 600)은 Blocker만.
		{
			const FSlotFrame F(SlotRotation, IslandEdge());
			FLNPSupportProbeQuery Probe;
			Probe.Up = -F.Radial();
			FLNPSupportProbeResult Result;

			Probe.Position = F.At(IslandEdgeTop - 50.0, 550.0);
			TestTrue(Name(TEXT("IslandEdge inside: supported")), Collision->ProbeSupport(Probe, Query, Result));
			TestEqual(Name(TEXT("IslandEdge inside: top radius")), F.RadiusOf(Result.Hit.ImpactPoint), IslandEdgeTop, Tolerance);

			Probe.Position = F.At(IslandEdgeTop - 50.0, 750.0);
			TestFalse(Name(TEXT("IslandEdge outside: not supported")), Collision->ProbeSupport(Probe, Query, Result));
			TestFalse(Name(TEXT("IslandEdge outside: nothing below")), Result.Hit.bBlockingHit);

			FLNPWorldHit Hit;
			const double Mid = IslandEdgeTop + IslandThickness * 0.5;
			TestTrue(Name(TEXT("IslandEdge side wall: sweep hits")), Collision->SweepSphereWorld(F.At(Mid, 900.0), F.At(Mid, 0.0), 20.f, Query, Hit));
			TestEqual(Name(TEXT("IslandEdge side wall: tangent")), F.TangentOf(Hit.ImpactPoint), IslandEdgeWidth * 0.5, Tolerance);
			TestTrue(Name(TEXT("IslandEdge side wall: normal faces outward")), FVector::DotProduct(Hit.ImpactNormal, F.Tangent()) > 0.99);
			TestEqual(Name(TEXT("IslandEdge side wall: blocker only")), static_cast<int32>(Hit.Identity.Roles), static_cast<int32>(Blocker));
		}

		// 6. 동굴 키트: 공동 바닥·천장·벽, 문을 지나 통로 바닥, 지각 입구, 입구 옆 지각과 통로 외벽.
		{
			const FTransform SlotTransform(SlotRotation);
			const FTransform Room = CavePlacement.Room * SlotTransform;
			const FTransform Corridor = CavePlacement.Corridor * SlotTransform;
			const FVector Center = Room.TransformPosition(FVector(0.0, 0.0, 250.0));
			FLNPWorldHit Hit;

			TestTrue(Name(TEXT("Cave: floor hit")), Collision->RaycastWorld(Center, Room.TransformPosition(FVector(0.0, 0.0, -500.0)), Query, Hit));
			TestEqual(Name(TEXT("Cave: floor height")), Room.InverseTransformPosition(Hit.ImpactPoint).Z, 0.0, Tolerance);
			TestTrue(Name(TEXT("Cave: floor is support")), (Hit.Identity.Roles & Support) != 0);

			TestTrue(Name(TEXT("Cave: ceiling hit")), Collision->RaycastWorld(Center, Room.TransformPosition(FVector(0.0, 0.0, 1000.0)), Query, Hit));
			TestEqual(Name(TEXT("Cave: ceiling height")), Room.InverseTransformPosition(Hit.ImpactPoint).Z, LNPCaveKit::RoomHeight, Tolerance);
			TestEqual(Name(TEXT("Cave: ceiling is blocker only")), static_cast<int32>(Hit.Identity.Roles), static_cast<int32>(Blocker));

			for (const FVector& Direction : {FVector::YAxisVector, -FVector::YAxisVector, -FVector::XAxisVector})
			{
				TestTrue(Name(TEXT("Cave: wall hit")), Collision->RaycastWorld(Center, Room.TransformPosition(FVector(0.0, 0.0, 250.0) + Direction * 2000.0), Query, Hit));
				TestEqual(Name(TEXT("Cave: wall distance")), FVector::DotProduct(Room.InverseTransformPosition(Hit.ImpactPoint), Direction),
					LNPCaveKit::RoomSize * 0.5, Tolerance);
				TestEqual(Name(TEXT("Cave: wall is blocker only")), static_cast<int32>(Hit.Identity.Roles), static_cast<int32>(Blocker));
			}

			// 문(+X)은 열려 있어 수평 ray가 통로 경사 바닥에 맞는다.
			TestTrue(Name(TEXT("Cave: ray through the door hits the corridor floor")),
				Collision->RaycastWorld(Center, Room.TransformPosition(FVector(4000.0, 0.0, 250.0)), Query, Hit));
			const FVector DoorHit = Corridor.InverseTransformPosition(Hit.ImpactPoint);
			TestTrue(Name(TEXT("Cave: door hit is inside the corridor")), DoorHit.X > 0.0);
			TestEqual(Name(TEXT("Cave: door hit is on the ramp")), DoorHit.Z, DoorHit.X * LNPCaveKit::RampSlope(), Tolerance);
			TestTrue(Name(TEXT("Cave: corridor floor is support")), (Hit.Identity.Roles & Support) != 0);

			// 입구: 통로 안(지각보다 중심 쪽)에서 바닥으로 쏜 ray가 지각이 아니라 지각 너머의 통로 바닥에 맞는다.
			const double EntranceX = LNPCaveKit::RampLength - 300.0;
			const double EntranceFloorZ = EntranceX * LNPCaveKit::RampSlope();
			const FVector EntranceStart = Corridor.TransformPosition(FVector(EntranceX, 0.0, EntranceFloorZ + 200.0));
			TestTrue(Name(TEXT("Cave entrance: probe starts above the crust")), EntranceStart.Size() < CrustRadius);
			TestTrue(Name(TEXT("Cave entrance: floor hit")),
				Collision->RaycastWorld(EntranceStart, Corridor.TransformPosition(FVector(EntranceX, 0.0, EntranceFloorZ - 100.0)), Query, Hit));
			TestEqual(Name(TEXT("Cave entrance: hit is the corridor floor")), Corridor.InverseTransformPosition(Hit.ImpactPoint).Z, EntranceFloorZ, Tolerance);
			TestTrue(Name(TEXT("Cave entrance: floor is below the crust")), Hit.ImpactPoint.Size() > CrustRadius + 50.0);

			// 입구 옆(벽 바깥)의 지각은 남아 있다.
			const FVector Beside = Corridor.TransformPosition(FVector(EntranceX, LNPCaveKit::PassageWidth * 0.5 + 300.0, EntranceFloorZ + 200.0));
			TestTrue(Name(TEXT("Cave entrance: crust beside the corridor")),
				Collision->RaycastWorld(Beside, Beside.GetSafeNormal() * (CrustRadius + 1000.0), Query, Hit));
			TestEqual(Name(TEXT("Cave entrance: crust radius beside the corridor")), Hit.ImpactPoint.Size(), CrustRadius, Tolerance);

			// 지각 위로 솟은 통로 벽은 바깥에서도 막는다(양면 shell).
			const double OutsideX = LNPCaveKit::RampLength - 100.0;
			const double OutsideZ = OutsideX * LNPCaveKit::RampSlope() + 100.0;
			TestTrue(Name(TEXT("Cave entrance: outer wall blocks from outside")),
				Collision->RaycastWorld(Corridor.TransformPosition(FVector(OutsideX, 600.0, OutsideZ)), Corridor.TransformPosition(FVector(OutsideX, 0.0, OutsideZ)), Query, Hit));
			TestEqual(Name(TEXT("Cave entrance: outer wall position")), Corridor.InverseTransformPosition(Hit.ImpactPoint).Y, LNPCaveKit::PassageWidth * 0.5, Tolerance);
			TestEqual(Name(TEXT("Cave entrance: outer wall is blocker only")), static_cast<int32>(Hit.Identity.Roles), static_cast<int32>(Blocker));
		}

		// 7. 정적 프랍: 나무·바위는 hit(Blocker만), 장식과 Pawn은 통과해 지면에 맞는다.
		{
			const FSlotFrame F(SlotRotation, StaticProps());
			FLNPWorldHit Hit;
			TestTrue(Name(TEXT("Props: tree hit")), Collision->RaycastWorld(F.At(CrustRadius - 350.0), F.At(CrustRadius - 350.0, -1000.0), Query, Hit));
			TestEqual(Name(TEXT("Props: tree surface tangent")), F.TangentOf(Hit.ImpactPoint), TreeTangent + TreeDiameter * 0.5, 5.0);
			TestEqual(Name(TEXT("Props: tree is blocker only")), static_cast<int32>(Hit.Identity.Roles), static_cast<int32>(Blocker));

			// 바위는 비균등 scale 구라 표면 위치 대신 바위 영역 안에서 맞았는지만 본다.
			TestTrue(Name(TEXT("Props: rock hit")), Collision->RaycastWorld(F.At(CrustRadius - 200.0), F.At(CrustRadius - 200.0, 1000.0), Query, Hit));
			TestTrue(Name(TEXT("Props: rock surface is before the rock center")), F.TangentOf(Hit.ImpactPoint) > 100.0 && F.TangentOf(Hit.ImpactPoint) < RockTangent);
			TestEqual(Name(TEXT("Props: rock is blocker only")), static_cast<int32>(Hit.Identity.Roles), static_cast<int32>(Blocker));

			TestTrue(Name(TEXT("Props: ray through decoration hits ground")),
				Collision->RaycastWorld(F.At(CrustRadius - 1000.0, DecorationTangent), F.At(CrustRadius + 1000.0, DecorationTangent), Query, Hit));
			TestEqual(Name(TEXT("Props: decoration is missed")), Hit.ImpactPoint.Size(), CrustRadius, Tolerance);

			TestTrue(Name(TEXT("Props: ray through pawn hits ground")),
				Collision->RaycastWorld(F.At(CrustRadius - 1000.0, -DecorationTangent), F.At(CrustRadius + 1000.0, -DecorationTangent), Query, Hit));
			TestEqual(Name(TEXT("Props: pawn is excluded")), Hit.ImpactPoint.Size(), CrustRadius, Tolerance);
		}

		// 8. 이음매 변 중점: 경계 양쪽에서 같은 반지름으로 hit, 경계를 가로지르는 sweep에 턱이 없다.
		{
			const TPair<FVector, FVector> Edges[] = {
				{FVector(1.0, 1.0, 0.0).GetSafeNormal(), FVector::ZAxisVector},
				{FVector(0.0, 1.0, 1.0).GetSafeNormal(), FVector::XAxisVector},
				{FVector(1.0, 0.0, 1.0).GetSafeNormal(), FVector::YAxisVector},
			};
			for (const TPair<FVector, FVector>& Edge : Edges)
			{
				const FVector Mid = SlotRotation.RotateVector(Edge.Key);
				const FVector Across = SlotRotation.RotateVector(Edge.Value);
				for (const double Offset : {-5.0, 5.0})
				{
					FLNPWorldHit Hit;
					TestTrue(Name(TEXT("Seam: hit")),
						Collision->RaycastWorld(Mid * (CrustRadius - 1000.0) + Across * Offset, Mid * (CrustRadius + 1000.0) + Across * Offset, Query, Hit));
					TestEqual(Name(TEXT("Seam: radius")), FVector::DotProduct(Hit.ImpactPoint, Mid), CrustRadius, Tolerance);
				}
				FLNPWorldHit Hit;
				TestFalse(Name(TEXT("Seam: sweep across the seam has no step")),
					Collision->SweepSphereWorld(Mid * (CrustRadius - 100.0) - Across * 500.0, Mid * (CrustRadius - 100.0) + Across * 500.0, 50.f, Query, Hit));
			}
		}

		// 9. 꼭짓점(좌표축): 네 slot이 만나는 점 근처에서도 지각이 이어진다.
		{
			const FVector Offsets[] = {FVector(0.0, 3.0, 4.0), FVector(4.0, 0.0, 3.0), FVector(3.0, 4.0, 0.0)};
			const FVector Axes[] = {FVector::XAxisVector, FVector::YAxisVector, FVector::ZAxisVector};
			for (int32 Axis = 0; Axis < 3; ++Axis)
			{
				const FVector Direction = SlotRotation.RotateVector(Axes[Axis]);
				const FVector Offset = SlotRotation.RotateVector(Offsets[Axis]);
				FLNPWorldHit Hit;
				TestTrue(Name(TEXT("Corner: hit")),
					Collision->RaycastWorld(Direction * (CrustRadius - 1000.0) + Offset, Direction * (CrustRadius + 1000.0) + Offset, Query, Hit));
				TestEqual(Name(TEXT("Corner: radius")), FVector::DotProduct(Hit.ImpactPoint, Direction), CrustRadius, Tolerance);
			}
		}
	}

	TestEqual(TEXT("No unknown hits"), Collision->GetUnknownHitCount(), static_cast<uint64>(0));
	return true;
}

/**
 * `Meadow_00`에 배치한 동굴 키트(`LNP.SurfaceNav.PlaceCaveKit`)가 production 지각에 입구를 냈는지 exact로 확인한다.
 * 지각 component와 키트 액터만 slot 0 그대로 테스트 월드에 복제한다.
 */
bool FLNPMeadowCaveKitTest::RunTest(const FString& Parameters)
{
	using namespace ELNPExactSourceRole;
	constexpr TCHAR MeadowLevelPath[] = TEXT("/Game/Maps/Meadow_00/LVI_Octant_Meadow_00.LVI_Octant_Meadow_00");
	constexpr TCHAR KitLabelPrefix[] = TEXT("CaveKit_00_");

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	AssetRegistry.WaitForCompletion();
	TSet<FName> LoadTags;
	LoadTags.Add(ULevel::LoadAllExternalObjectsTag);
	const FAssetData Asset = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(MeadowLevelPath));
	UWorld* SourceWorld = Asset.IsValid() ? Cast<UWorld>(Asset.GetAsset(MoveTemp(LoadTags))) : nullptr;
	if (!TestNotNull(TEXT("Meadow_00 loads"), SourceWorld))
		return false;

	TArray<FLNPBakeSupportSource> Sources;
	FString Error;
	int32 CrustIndex = INDEX_NONE;
	if (!TestTrue(TEXT("Support sources extract"), FLNPOctantTriangleExtractor::ExtractSupportSources(*SourceWorld, Sources, Error))
		|| !TestTrue(TEXT("Crust is identified"), LNPSurfaceBake::IdentifyCrust(Sources, CrustIndex, Error)))
	{
		AddError(Error);
		return false;
	}
	const UStaticMeshComponent* CrustSource = FindObject<UStaticMeshComponent>(nullptr, *Sources[CrustIndex].Name);
	if (!TestNotNull(TEXT("Crust component resolves"), CrustSource))
		return false;

	TArray<const UStaticMeshComponent*> Copies = {CrustSource};
	const AStaticMeshActor* RoomFloor = nullptr;
	const AStaticMeshActor* CorridorFloor = nullptr;
	for (const AActor* Actor : SourceWorld->PersistentLevel->Actors)
	{
		const AStaticMeshActor* MeshActor = Cast<AStaticMeshActor>(Actor);
		if (!IsValid(MeshActor) || !MeshActor->GetActorLabel().StartsWith(KitLabelPrefix))
			continue;
		Copies.Add(MeshActor->GetStaticMeshComponent());
		RoomFloor = MeshActor->GetActorLabel().EndsWith(TEXT("Room_Floor")) ? MeshActor : RoomFloor;
		CorridorFloor = MeshActor->GetActorLabel().EndsWith(TEXT("Corridor_Floor")) ? MeshActor : CorridorFloor;
	}
	TestEqual(TEXT("Four cave kit actors"), Copies.Num(), 5);
	if (!TestNotNull(TEXT("Room floor actor"), RoomFloor) || !TestNotNull(TEXT("Corridor floor actor"), CorridorFloor))
		return false;

	UWorld* World = UWorld::CreateWorld(EWorldType::Game, /*bInformEngineOfWorld=*/false, TEXT("LNPMeadowCaveKitTest"));
	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	WorldContext.SetCurrentWorld(World);
	ON_SCOPE_EXIT
	{
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
	};
	ULNPHitIdentitySubsystem* HitIdentity = World->GetSubsystem<ULNPHitIdentitySubsystem>();
	ULNPMassWorldCollisionSubsystem* Collision = World->GetSubsystem<ULNPMassWorldCollisionSubsystem>();
	for (const UStaticMeshComponent* Source : Copies)
	{
		AActor* Owner = World->SpawnActor<AActor>();
		UStaticMeshComponent* Copy = NewObject<UStaticMeshComponent>(Owner);
		Copy->SetStaticMesh(Source->GetStaticMesh());
		Copy->SetCollisionProfileName(Source->GetCollisionProfileName());
		Copy->SetWorldTransform(FLNPOctantTriangleExtractor::GetSourceTransform(*Source));
		Owner->SetRootComponent(Copy);
		Copy->RegisterComponent();
		HitIdentity->RegisterRuntimeSource(Copy);
	}
	HitIdentity->Tick(0.f);
	Collision->ResetStats();

	const FLNPWorldQueryParams Query(ELNPWorldQueryClass::DebugValidation);
	constexpr double Tolerance = 1.0;
	const FTransform Room = RoomFloor->GetActorTransform();
	const FTransform Corridor = CorridorFloor->GetActorTransform();
	FLNPWorldHit Hit;

	TestTrue(TEXT("Room floor hit"), Collision->RaycastWorld(Room.TransformPosition(FVector(0.0, 0.0, 250.0)), Room.TransformPosition(FVector(0.0, 0.0, -500.0)), Query, Hit));
	TestEqual(TEXT("Room floor height"), Room.InverseTransformPosition(Hit.ImpactPoint).Z, 0.0, Tolerance);
	TestTrue(TEXT("Room floor is support"), (Hit.Identity.Roles & Support) != 0);

	// 입구 바닥점은 지각 높이에 있다. 입구 끝 천장을 피해 통로 끝에서 100cm 바깥 지각과 비교한다(지형 경사 허용 50cm).
	const FVector Mouth = Corridor.TransformPosition(LNPCaveKit::GetCorridorMouthTransform().GetLocation());
	const FVector BeyondMouth = Corridor.TransformPosition(LNPCaveKit::GetCorridorMouthTransform().GetLocation() + FVector(100.0, 0.0, 0.0));
	TestTrue(TEXT("Crust beyond the mouth"), Collision->RaycastWorld(BeyondMouth * 0.99, BeyondMouth * 1.01, Query, Hit));
	TestEqual(TEXT("Mouth floor is at the crust height"), Hit.ImpactPoint.Size(), Mouth.Size(), 50.0);
	TestTrue(TEXT("Crust beyond the mouth is static support"), Hit.Identity.Lifetime == ELNPExactSourceLifetime::Static && (Hit.Identity.Roles & Support) != 0);

	// 입구 구멍: 통로 안에서 바닥으로 쏜 ray가 지각이 아니라 통로 바닥에 맞는다.
	const double EntranceX = LNPCaveKit::RampLength - 300.0;
	const double FloorZ = EntranceX * LNPCaveKit::RampSlope();
	TestTrue(TEXT("Entrance: corridor floor hit"),
		Collision->RaycastWorld(Corridor.TransformPosition(FVector(EntranceX, 0.0, FloorZ + 200.0)), Corridor.TransformPosition(FVector(EntranceX, 0.0, FloorZ - 100.0)), Query, Hit));
	TestEqual(TEXT("Entrance: hit is the corridor floor"), Corridor.InverseTransformPosition(Hit.ImpactPoint).Z, FloorZ, Tolerance);
	TestTrue(TEXT("Entrance: corridor floor is support"), (Hit.Identity.Roles & Support) != 0);

	// 입구 옆(벽 바깥)의 지각은 남아 있다.
	const FVector Beside = Corridor.TransformPosition(FVector(EntranceX, LNPCaveKit::PassageWidth * 0.5 + 300.0, FloorZ));
	TestTrue(TEXT("Entrance: crust beside the corridor"), Collision->RaycastWorld(Beside * 0.95, Beside * 1.05, Query, Hit));
	TestTrue(TEXT("Entrance: crust beside is static support"), Hit.Identity.Lifetime == ELNPExactSourceLifetime::Static && (Hit.Identity.Roles & Support) != 0);

	TestEqual(TEXT("No unknown hits"), Collision->GetUnknownHitCount(), static_cast<uint64>(0));
	return true;
}

/**
 * 동굴 키트 에셋 규약(`LNPCaveKit.h`, `design/TerrainContract.md` §6)을 저장된 에셋의 cooked 충돌 삼각형으로 검사한다.
 * 제작 도구와 무관하게 교체된 에셋도 이 검사를 통과해야 한다.
 */
bool FLNPCaveKitContractTest::RunTest(const FString& Parameters)
{
	using namespace LNPRegressionFixtureTest;
	using namespace LNPCaveKit;

	struct FKitMesh
	{
		UStaticMesh* Asset = nullptr;
		FLNPBakeTriangleMesh Triangles;
	};
	auto Load = [this](const TCHAR* Name, FKitMesh& Out)
	{
		Out.Asset = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("%s/%s.%s"), KitFolder, Name, Name));
		if (!TestNotNull(*FString::Printf(TEXT("%s exists"), Name), Out.Asset))
			return false;
		UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(GetTransientPackage());
		Component->SetStaticMesh(Out.Asset);
		FString Error;
		const bool bExtracted = FLNPOctantTriangleExtractor::ExtractComponent(*Component, Out.Triangles, Error);
		return TestTrue(*FString::Printf(TEXT("%s extracts complex-as-simple triangles: %s"), Name, *Error), bExtracted);
	};
	FKitMesh RoomFloor, RoomShell, CorridorFloor, CorridorShell;
	if (!Load(RoomFloorName, RoomFloor) || !Load(RoomShellName, RoomShell) || !Load(CorridorFloorName, CorridorFloor)
		|| !Load(CorridorShellName, CorridorShell))
		return false;

	// 바닥: 모든 삼각형이 위(+Z)를 향하고 walkable(법선 dot ≥ 0.71)이다. 뒷면이 위를 향하는 삼각형이 없다.
	for (const FKitMesh* Floor : {&RoomFloor, &CorridorFloor})
	{
		int32 NonWalkable = 0;
		for (int32 Index = 0; Index < Floor->Triangles.Triangles.Num(); ++Index)
		{
			NonWalkable += Floor->Triangles.GetTriangleNormal(Index).Z < 0.71 ? 1 : 0;
		}
		TestEqual(*FString::Printf(TEXT("%s: every triangle is walkable and faces up"), *Floor->Asset->GetName()), NonWalkable, 0);
	}

	// Shell: 모든 삼각형에 반대 방향 짝이 있다(안팎 모두 막는 양면).
	for (const FKitMesh* Shell : {&RoomShell, &CorridorShell})
	{
		TMap<FString, int32> Faces;
		auto Key = [](const FVector3d& A, const FVector3d& B, const FVector3d& C)
		{
			TArray<FString, TFixedAllocator<3>> Corners = {A.ToString(), B.ToString(), C.ToString()};
			Corners.Sort();
			return FString::Join(Corners, TEXT("|"));
		};
		for (int32 Index = 0; Index < Shell->Triangles.Triangles.Num(); ++Index)
		{
			const FIntVector3& T = Shell->Triangles.Triangles[Index];
			const FVector3d Normal = Shell->Triangles.GetTriangleNormal(Index);
			// 같은 정점 집합의 법선 부호 합이 0이면 양면이다. 주축 성분 부호로 센다.
			const int32 Sign = Normal.X + Normal.Y * 1e-3 + Normal.Z * 1e-6 > 0.0 ? 1 : -1;
			Faces.FindOrAdd(Key(Shell->Triangles.Vertices[T.X], Shell->Triangles.Vertices[T.Y], Shell->Triangles.Vertices[T.Z])) += Sign;
		}
		int32 SingleSided = 0;
		for (const TPair<FString, int32>& Face : Faces)
		{
			SingleSided += Face.Value != 0 ? 1 : 0;
		}
		TestEqual(*FString::Printf(TEXT("%s: every face is double-sided"), *Shell->Asset->GetName()), SingleSided, 0);
	}

	// 소켓: 공동 문과 통로 양 끝이 바닥 위에 있다.
	const UStaticMeshSocket* Door = RoomFloor.Asset->FindSocket(DoorSocket);
	const UStaticMeshSocket* Lower = CorridorFloor.Asset->FindSocket(LowerSocket);
	const UStaticMeshSocket* Mouth = CorridorFloor.Asset->FindSocket(MouthSocket);
	if (!TestNotNull(TEXT("Room floor has the door socket"), Door) || !TestNotNull(TEXT("Corridor floor has the lower socket"), Lower)
		|| !TestNotNull(TEXT("Corridor floor has the mouth socket"), Mouth))
		return false;
	double Z;
	TestTrue(TEXT("Door socket is on the room floor edge"), FloorHeightAt(RoomFloor.Triangles, Door->RelativeLocation.X - 1.0, Door->RelativeLocation.Y, Z)
		&& FMath::IsNearlyEqual(Z, Door->RelativeLocation.Z, 1.0));
	TestTrue(TEXT("Lower socket is on the corridor floor"), FloorHeightAt(CorridorFloor.Triangles, Lower->RelativeLocation.X + 1.0, Lower->RelativeLocation.Y, Z)
		&& FMath::IsNearlyEqual(Z, Lower->RelativeLocation.Z, 1.0));
	TestTrue(TEXT("Mouth socket is on the corridor floor"), FloorHeightAt(CorridorFloor.Triangles, Mouth->RelativeLocation.X - 1.0, Mouth->RelativeLocation.Y, Z)
		&& FMath::IsNearlyEqual(Z, Mouth->RelativeLocation.Z, 1.0));

	// 천장 높이: 바닥 표본점에서 위로 쏜 ray가 shell에 닿기까지 MinClearance 이상이다.
	auto CheckClearance = [this](const TCHAR* Label, const FLNPBakeTriangleMesh& Floor, const FLNPBakeTriangleMesh& Shell, const TArray<FVector2D>& Samples)
	{
		double Lowest = TNumericLimits<double>::Max();
		for (const FVector2D& Sample : Samples)
		{
			double FloorZ, T;
			if (!FloorHeightAt(Floor, Sample.X, Sample.Y, FloorZ))
			{
				AddError(FString::Printf(TEXT("%s: no floor at (%.0f, %.0f)"), Label, Sample.X, Sample.Y));
				continue;
			}
			const FVector3d Start(Sample.X, Sample.Y, FloorZ + 1.0);
			const double Clearance = RaycastMesh(Shell, Start, Start + FVector3d(0.0, 0.0, 10000.0), T) ? T * 10000.0 + 1.0 : TNumericLimits<double>::Max();
			Lowest = FMath::Min(Lowest, Clearance);
		}
		TestTrue(*FString::Printf(TEXT("%s: clearance %.0f >= %.0f"), Label, Lowest, MinClearance), Lowest >= MinClearance);
	};
	TArray<FVector2D> RoomSamples;
	const double RoomInset = RoomSize * 0.5 - 50.0;
	for (int32 I = 0; I <= 8; ++I)
		for (int32 J = 0; J <= 8; ++J)
			RoomSamples.Emplace(-RoomInset + RoomInset * 2.0 * I / 8.0, -RoomInset + RoomInset * 2.0 * J / 8.0);
	CheckClearance(TEXT("Room"), RoomFloor.Triangles, RoomShell.Triangles, RoomSamples);

	TArray<FVector2D> CorridorSamples;
	const double SideInset = PassageWidth * 0.5 - 40.0;
	for (int32 I = 1; I < 12; ++I)
		for (const double Y : {-SideInset, 0.0, SideInset})
			CorridorSamples.Emplace(RampLength * I / 12.0, Y);
	CheckClearance(TEXT("Corridor"), CorridorFloor.Triangles, CorridorShell.Triangles, CorridorSamples);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
