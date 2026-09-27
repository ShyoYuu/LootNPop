// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/StaticMeshComponent.h"
#include "DataAsset/LNPOctantSurfaceData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Engine/Engine.h"
#include "Engine/Level.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMeshSocket.h"
#include "Engine/World.h"
#include "GameLogic/LNPOctantSpawnSubsystem.h"
#include "Misc/PackageName.h"
#include "SurfaceNavigation/LNPCaveKit.h"
#include "SurfaceNavigation/LNPCollisionChannels.h"
#include "SurfaceNavigation/LNPHitIdentityRegistry.h"
#include "SurfaceNavigation/LNPMassWorldCollision.h"
#include "SurfaceNavigation/LNPOctantSurfaceBaker.h"
#include "SurfaceNavigation/LNPOctantTriangleExtractor.h"
#include "SurfaceNavigation/LNPRegressionFixture.h"
#include "SurfaceNavigation/LNPSupportAtlas.h"
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPLayerIdentityTest,
	"LootNPop.SurfaceNavigation.WorldCollision.LayerIdentity",
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

namespace LNPLayerIdentityTest
{
	/** 발은 지지면보다 이만큼(cm) 위에 둔다. */
	constexpr double FeetClearance = 2.0;
	/** `FLNPSupportProbeQuery` 기본값과 같은 탐색 창. 적 exact 이동 기준값은 Phase 6에서 맞춘다. */
	constexpr double MaxStepUp = 50.0;
	constexpr double MaxDrop = 100.0;
	/** 한 광선에서 non-walkable face를 건너뛰는 최대 횟수. */
	constexpr int32 MaxTraceSkips = 8;

	/** 한 방향·발 반지름에서 Atlas 조회와 exact hit face→Layer 해석을 함께 한 결과. */
	struct FLayerProbe
	{
		ELNPSupportQueryResult Result = ELNPSupportQueryResult::NoSupport;
		FLNPSupportLayerHit Atlas;
		/** 탐색 창 안에서 exact가 처음 맞힌 walkable face의 Layer. 없으면 NoLayer다. */
		uint16 ExactLayer = LNPSupportLayers::NoLayer;
		double ExactRadius = 0.0;
		/** exact hit의 component나 FaceIndex를 해석하지 못했다. */
		bool bUnresolved = false;

		/** Supported면 exact와 같은 Layer, NoSupport면 exact도 없음, NeedsExact면 exact가 판정한다. */
		bool IsConsistent() const
		{
			switch (Result)
			{
			case ELNPSupportQueryResult::Supported: return Atlas.Layer == ExactLayer;
			case ELNPSupportQueryResult::NoSupport: return ExactLayer == LNPSupportLayers::NoLayer;
			default: return true;
			}
		}

		FString ToString() const
		{
			static const TCHAR* Results[] = {TEXT("Supported"), TEXT("NeedsExact"), TEXT("NoSupport")};
			return FString::Printf(TEXT("atlas %s Layer %d r=%.2f, exact Layer %d r=%.2f"), Results[static_cast<int32>(Result)],
				Result == ELNPSupportQueryResult::Supported ? Atlas.Layer : -1, Atlas.Radius,
				ExactLayer == LNPSupportLayers::NoLayer ? -1 : ExactLayer, ExactRadius);
		}
	};

	bool LoadSavedAtlas(const TCHAR* LevelPath, FLNPSupportAtlas& OutAtlas, FString& OutError)
	{
		const FString Package = FLNPOctantSurfaceBaker::GetSurfaceDataPackageName(FSoftObjectPath(LevelPath));
		const ULNPOctantSurfaceData* Data = LoadObject<ULNPOctantSurfaceData>(
			nullptr, *FString::Printf(TEXT("%s.%s"), *Package, *FPackageName::GetShortName(Package)));
		if (!Data)
		{
			OutError = FString::Printf(TEXT("SurfaceData %s is missing"), *Package);
			return false;
		}
		return LNPSupportAtlas::Decode(Data->SupportPayload, OutAtlas, OutError);
	}

	/**
	 * 옥탄트 Support source를 slot 회전으로 테스트 월드에 복제하고, exact hit의 (component, FaceIndex)를 저장된 Atlas의
	 * source 표로 Layer까지 해석한다. component → source 대응은 source key로 맺는다(Phase 5 registry binding과 같은 key).
	 */
	class FLayerIdentityWorld
	{
	public:
		explicit FLayerIdentityWorld(const FLNPSupportAtlas& InAtlas) : Atlas(InAtlas)
		{
			World = UWorld::CreateWorld(EWorldType::Game, /*bInformEngineOfWorld=*/false, TEXT("LNPLayerIdentityTest"));
			FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
			WorldContext.SetCurrentWorld(World);
		}

		~FLayerIdentityWorld()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}

		bool AddSources(TConstArrayView<FLNPBakeSupportSource> Sources, const FQuat& SlotRotation, FString& OutError)
		{
			for (const FLNPBakeSupportSource& Source : Sources)
			{
				const int32 AtlasSource = Atlas.Sources.IndexOfByPredicate(
					[&Source](const FLNPSupportAtlasSource& Entry) { return Entry.Key.Equals(Source.Key, ESearchCase::CaseSensitive); });
				const UStaticMeshComponent* Component = FindObject<UStaticMeshComponent>(nullptr, *Source.Name);
				if (AtlasSource == INDEX_NONE || !Component)
				{
					OutError = FString::Printf(TEXT("Source %s (key %s) is not in the saved Atlas or does not resolve"), *Source.Name, *Source.Key);
					return false;
				}
				AActor* Owner = World->SpawnActor<AActor>();
				UStaticMeshComponent* Copy = NewObject<UStaticMeshComponent>(Owner);
				Copy->SetStaticMesh(Component->GetStaticMesh());
				Copy->SetCollisionProfileName(Component->GetCollisionProfileName());
				Copy->SetWorldTransform(FLNPOctantTriangleExtractor::GetSourceTransform(*Component) * FTransform(SlotRotation));
				Owner->SetRootComponent(Copy);
				Copy->RegisterComponent();
				SourceByComponent.Add(Copy, AtlasSource);
			}
			return true;
		}

		FLayerProbe Probe(const FQuat& SlotRotation, const FVector3d& WorldDirection, double FeetRadius, double StepUp, double Drop) const
		{
			FLayerProbe Out;
			Out.Result = LNPSupportAtlas::QueryLayers(Atlas, SlotRotation.UnrotateVector(WorldDirection), FeetRadius, StepUp, Drop,
				LNPSupportLayers::NoLayer, Out.Atlas);

			FCollisionQueryParams Params(SCENE_QUERY_STAT(LNPLayerIdentity), /*bTraceComplex=*/false);
			Params.bReturnFaceIndex = true;
			double StartRadius = FeetRadius - StepUp;
			for (int32 Skip = 0; Skip <= MaxTraceSkips; ++Skip)
			{
				FHitResult Hit;
				if (!World->LineTraceSingleByChannel(Hit, WorldDirection * StartRadius, WorldDirection * (FeetRadius + Drop),
					LNPCollisionChannels::SurfaceSupport, Params))
				{
					break;
				}
				const int32* AtlasSource = SourceByComponent.Find(Hit.GetComponent());
				if (!AtlasSource || Hit.FaceIndex < 0)
				{
					Out.bUnresolved = true;
					break;
				}
				const double Radius = FVector3d::DotProduct(FVector3d(Hit.ImpactPoint), WorldDirection);
				const uint16 Layer = Atlas.Sources[*AtlasSource].FaceMap.Resolve(Hit.FaceIndex);
				if (Layer != LNPSupportLayers::NoLayer)
				{
					Out.ExactLayer = Layer;
					Out.ExactRadius = Radius;
					break;
				}
				StartRadius = Radius + 0.01;
			}
			return Out;
		}

		/** 위에서 내려다본 첫 walkable 지지면에 발을 두고 조회한다. 지지면이 없으면 ExactLayer가 NoLayer다. */
		FLayerProbe ProbeStandingOn(const FQuat& SlotRotation, const FVector3d& WorldDirection, double AboveRadius, double BelowRadius) const
		{
			const FLayerProbe Surface = Probe(SlotRotation, WorldDirection, AboveRadius, 0.0, BelowRadius - AboveRadius);
			if (Surface.bUnresolved || Surface.ExactLayer == LNPSupportLayers::NoLayer)
			{
				return Surface;
			}
			return Probe(SlotRotation, WorldDirection, Surface.ExactRadius - FeetClearance, MaxStepUp, MaxDrop);
		}

	private:
		const FLNPSupportAtlas& Atlas;
		UWorld* World = nullptr;
		TMap<const UPrimitiveComponent*, int32> SourceByComponent;
	};
}

/**
 * exact hit의 (source key, FaceIndex)를 face 표로 해석한 Layer가 같은 지점의 `QueryLayers` Layer와 같은지 본다
 * (`phases/Phase04b_MultiLayerSupport.md` 구현 단위 4). 저장된 SurfaceData를 쓰므로 `Bake.OctantBakeDeterministic`이
 * 통과한 상태를 전제한다.
 * 1. fixture 회귀 LVI를 8 slot으로 합성해 섬 둘 3층, 동굴 공동·통로 바닥, 섬 가장자리 안팎을 검사한다.
 * 2. `Meadow_00` 섬 B 계단(`SM_TerrainBox`)에서 칸 위는 그 칸 Layer로 Supported이거나 NeedsExact, 칸 옆은 다른 Layer다.
 */
bool FLNPLayerIdentityTest::RunTest(const FString& Parameters)
{
	using namespace LNPRegressionFixture;
	using namespace LNPLayerIdentityTest;
	constexpr double Tolerance = 1.0;
	constexpr uint16 NoLayer = LNPSupportLayers::NoLayer;

	auto CheckProbe = [this](const FString& Label, const FLayerProbe& Probe)
	{
		TestFalse(Label + TEXT(": exact hit resolves"), Probe.bUnresolved);
		if (!Probe.IsConsistent())
		{
			AddError(FString::Printf(TEXT("%s: Atlas and exact disagree (%s)"), *Label, *Probe.ToString()));
		}
	};

	// 1. fixture 회귀 LVI, 8 slot.
	{
		FLNPSupportAtlas Atlas;
		FString Error;
		UPackage* MapPackage = LoadPackage(nullptr, *FPackageName::ObjectPathToPackageName(FString(LevelPath)), LOAD_None);
		UWorld* MapWorld = MapPackage ? UWorld::FindWorldInPackage(MapPackage) : nullptr;
		TArray<FLNPBakeSupportSource> Sources;
		if (!TestNotNull(TEXT("Regression fixture LVI loads"), MapWorld)
			|| !TestTrue(TEXT("Fixture support sources extract"), FLNPOctantTriangleExtractor::ExtractSupportSources(*MapWorld, Sources, Error))
			|| !TestTrue(TEXT("Fixture SurfaceData decodes"), LoadSavedAtlas(LevelPath, Atlas, Error)))
		{
			AddError(Error);
			return false;
		}

		FLayerIdentityWorld Identity(Atlas);
		constexpr int32 SlotCount = UE_ARRAY_COUNT(ULNPOctantSpawnSubsystem::OctantRotations);
		for (int32 Slot = 0; Slot < SlotCount; ++Slot)
		{
			if (!TestTrue(TEXT("Fixture sources copy"), Identity.AddSources(Sources, ULNPOctantSpawnSubsystem::OctantRotations[Slot].Quaternion(), Error)))
			{
				AddError(Error);
				return false;
			}
		}

		const LNPCaveKit::FPlacement CavePlacement = LNPCaveKit::PlaceUnderSphere(Cave().Radial, Cave().Bitangent, CrustRadius);
		int32 EdgeCounts[3] = {};
		for (int32 Slot = 0; Slot < SlotCount; ++Slot)
		{
			const FQuat SlotRotation = ULNPOctantSpawnSubsystem::OctantRotations[Slot].Quaternion();
			auto Name = [Slot](const TCHAR* Text) { return FString::Printf(TEXT("Slot %d %s"), Slot, Text); };

			// 섬 둘: 안쪽 섬 위 → 안쪽 섬, 안쪽 섬 밑 → 바깥 섬, 바깥 섬 밑 → 지각. 세 Layer가 서로 다르다.
			{
				const FVector3d Direction = SlotRotation.RotateVector(IslandTwo().Radial);
				const double Tops[] = {IslandTwoInnerTop, IslandTwoOuterTop, CrustRadius};
				uint16 Layers[3];
				for (int32 Index = 0; Index < 3; ++Index)
				{
					const FString Label = Name(*FString::Printf(TEXT("IslandTwo level %d"), Index));
					const FLayerProbe Probe = Identity.Probe(SlotRotation, Direction, Tops[Index] - FeetClearance, MaxStepUp, MaxDrop);
					CheckProbe(Label, Probe);
					TestTrue(Label + TEXT(": Atlas supports"), Probe.Result == ELNPSupportQueryResult::Supported);
					TestEqual(Label + TEXT(": Atlas radius"), Probe.Atlas.Radius, Tops[Index], Tolerance);
					Layers[Index] = Probe.Atlas.Layer;
				}
				TestTrue(Name(TEXT("IslandTwo: islands are not the crust")), Layers[0] != 0 && Layers[1] != 0);
				TestEqual(Name(TEXT("IslandTwo: bottom is the crust")), static_cast<int32>(Layers[2]), 0);
				TestTrue(Name(TEXT("IslandTwo: three distinct Layers")), Layers[0] != Layers[1]);
			}

			// 동굴: 공동·통로 바닥은 지각·천장 뒤에 있어도 각자의 Layer로 조회된다.
			{
				const FTransform SlotTransform(SlotRotation);
				const FTransform Room = CavePlacement.Room * SlotTransform;
				const FTransform Corridor = CavePlacement.Corridor * SlotTransform;
				const double RampX = LNPCaveKit::RampLength * 0.5;
				const FVector Floors[] = {
					Room.TransformPosition(FVector::ZeroVector),
					Corridor.TransformPosition(FVector(RampX, 0.0, RampX * LNPCaveKit::RampSlope())),
				};
				uint16 Layers[2];
				for (int32 Index = 0; Index < 2; ++Index)
				{
					const FString Label = Name(Index == 0 ? TEXT("Cave room floor") : TEXT("Cave corridor floor"));
					const FLayerProbe Probe = Identity.Probe(SlotRotation, Floors[Index].GetSafeNormal(), Floors[Index].Size() - FeetClearance, MaxStepUp, MaxDrop);
					CheckProbe(Label, Probe);
					TestTrue(Label + TEXT(": Atlas supports"), Probe.Result == ELNPSupportQueryResult::Supported);
					TestEqual(Label + TEXT(": Atlas radius"), Probe.Atlas.Radius, Floors[Index].Size(), Tolerance);
					TestTrue(Label + TEXT(": floor is below the crust"), Floors[Index].Size() > CrustRadius);
					Layers[Index] = Probe.Atlas.Layer;
				}
				TestTrue(Name(TEXT("Cave: room and corridor floors are distinct non-crust Layers")),
					Layers[0] != 0 && Layers[1] != 0 && Layers[0] != Layers[1]);
			}

			// 섬 가장자리: 윗면 높이의 발로 가장자리를 가로지른다. 창을 지각까지 넓혀 밖에서 지각으로 떨어지게 한다.
			// 안(윗면 Layer)·밖(지각)에서 Atlas가 고른 Layer는 항상 exact와 같고, 가장자리 밖에 유령 지면이 없다.
			{
				const LNPRegressionFixtureTest::FSlotFrame F(SlotRotation, IslandEdge());
				const double Drop = CrustRadius - IslandEdgeTop + 100.0;
				uint16 TopLayer = NoLayer;
				for (double Tangent = 300.0; Tangent <= 900.0; Tangent += 5.0)
				{
					const FVector3d Direction = F.At(IslandEdgeTop, Tangent).GetSafeNormal();
					const FLayerProbe Probe = Identity.Probe(SlotRotation, Direction, IslandEdgeTop - FeetClearance, MaxStepUp, Drop);
					CheckProbe(Name(*FString::Printf(TEXT("IslandEdge tangent %.0f"), Tangent)), Probe);
					if (Probe.Result == ELNPSupportQueryResult::Supported)
					{
						TopLayer = Probe.Atlas.Layer != 0 ? Probe.Atlas.Layer : TopLayer;
						++EdgeCounts[Probe.Atlas.Layer == 0 ? 1 : 0];
					}
					else
					{
						++EdgeCounts[2];
					}
				}
				const FLayerProbe Outside = Identity.Probe(SlotRotation, F.At(IslandEdgeTop, 750.0).GetSafeNormal(), IslandEdgeTop - FeetClearance, MaxStepUp, Drop);
				TestTrue(Name(TEXT("IslandEdge outside: Atlas falls to the crust")),
					Outside.Result == ELNPSupportQueryResult::Supported && Outside.Atlas.Layer == 0);
				TestTrue(Name(TEXT("IslandEdge inside: top Layer is supported")), TopLayer != NoLayer);
			}
		}
		AddInfo(FString::Printf(TEXT("Fixture IslandEdge scan over 8 slots: top Supported=%d crust Supported=%d NeedsExact=%d"),
			EdgeCounts[0], EdgeCounts[1], EdgeCounts[2]));
	}

	// 2. Meadow_00 섬 B 계단, slot 0.
	{
		constexpr TCHAR MeadowLevelPath[] = TEXT("/Game/Maps/Meadow_00/LVI_Octant_Meadow_00.LVI_Octant_Meadow_00");
		constexpr TCHAR StairMeshName[] = TEXT("SM_TerrainBox");
		/** 계단 footprint 주변에 두는 여유(격자 칸). 칸 옆 섬 윗면을 포함한다. */
		constexpr int32 StairMargin = 8;
		constexpr int32 StairDirectionCount = 4000;
		constexpr int32 RandomSeed = 20260927;

		IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
		AssetRegistry.WaitForCompletion();
		TSet<FName> LoadTags;
		LoadTags.Add(ULevel::LoadAllExternalObjectsTag);
		const FAssetData Asset = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(MeadowLevelPath));
		UWorld* SourceWorld = Asset.IsValid() ? Cast<UWorld>(Asset.GetAsset(MoveTemp(LoadTags))) : nullptr;
		FLNPSupportAtlas Atlas;
		TArray<FLNPBakeSupportSource> Sources;
		FString Error;
		if (!TestNotNull(TEXT("Meadow_00 loads"), SourceWorld)
			|| !TestTrue(TEXT("Meadow support sources extract"), FLNPOctantTriangleExtractor::ExtractSupportSources(*SourceWorld, Sources, Error))
			|| !TestTrue(TEXT("Meadow SurfaceData decodes"), LoadSavedAtlas(MeadowLevelPath, Atlas, Error)))
		{
			AddError(Error);
			return false;
		}
		FLayerIdentityWorld Identity(Atlas);
		const FQuat Slot0 = FQuat::Identity;
		if (!TestTrue(TEXT("Meadow sources copy"), Identity.AddSources(Sources, Slot0, Error)))
		{
			AddError(Error);
			return false;
		}

		// 계단 칸 Layer와 그 footprint 행·열 범위.
		TSet<uint16> StairLayers;
		for (const FLNPBakeSupportSource& Source : Sources)
		{
			const UStaticMeshComponent* Component = FindObject<UStaticMeshComponent>(nullptr, *Source.Name);
			if (!Component || !Component->GetStaticMesh() || Component->GetStaticMesh()->GetName() != StairMeshName)
				continue;
			for (int32 LayerId = 1; LayerId < Atlas.Layers.Num(); ++LayerId)
			{
				if (Atlas.Sources[Atlas.Layers[LayerId].SourceIndex].Key == Source.Key)
				{
					StairLayers.Add(LayerId);
				}
			}
		}
		if (!TestTrue(TEXT("Meadow stairs: at least three step Layers"), StairLayers.Num() >= 3))
			return false;

		int32 N = 0;
		int32 IMin = MAX_int32, IMax = MIN_int32, JMin = MAX_int32, JMax = MIN_int32;
		double TopRadius = TNumericLimits<double>::Max();
		for (const uint16 LayerId : StairLayers)
		{
			const FLNPSupportAtlasLayer& Layer = Atlas.Layers[LayerId];
			N = Layer.Layout.Subdivisions;
			JMin = FMath::Min(JMin, Layer.Layout.J0);
			JMax = FMath::Max(JMax, Layer.Layout.J0 + Layer.Layout.Rows.Num() - 1);
			for (const FLNPSupportRowSpan& Span : Layer.Layout.Rows)
			{
				IMin = FMath::Min(IMin, Span.IStart);
				IMax = FMath::Max(IMax, Span.IStart + Span.Count - 1);
			}
			for (int32 Index = 0; Index < Layer.Num(); ++Index)
			{
				TopRadius = EnumHasAnyFlags(Layer.GetFlags(Index), ELNPSupportSampleFlags::Valid) ? FMath::Min(TopRadius, Layer.GetRadius(Index)) : TopRadius;
			}
		}

		// 칸마다 Supported·NeedsExact, 칸 옆(다른 Layer 위) Supported·NeedsExact.
		TMap<uint16, TPair<int32, int32>> OnStair;
		int32 BesideSupported = 0;
		int32 BesideNeedsExact = 0;
		int32 Inconsistent = 0;
		int32 NoSurface = 0;
		FRandomStream Random(RandomSeed);
		for (int32 Sample = 0; Sample < StairDirectionCount; ++Sample)
		{
			const double U = Random.FRandRange(IMin - StairMargin, IMax + StairMargin);
			const double V = Random.FRandRange(JMin - StairMargin, JMax + StairMargin);
			const FVector3d Direction = FVector3d(U, V, N - U - V).GetSafeNormal();
			const FLayerProbe Probe = Identity.ProbeStandingOn(Slot0, Direction, TopRadius - 1000.0, Atlas.Layers[0].BaseRadius + 1000.0);
			TestFalse(TEXT("Meadow stairs: exact hit resolves"), Probe.bUnresolved);
			if (Probe.ExactLayer == NoLayer)
			{
				++NoSurface;
				continue;
			}
			if (!Probe.IsConsistent())
			{
				if (++Inconsistent <= 10)
				{
					AddError(FString::Printf(TEXT("Meadow stairs direction (%.3f, %.3f): %s"), U, V, *Probe.ToString()));
				}
				continue;
			}
			const bool bSupported = Probe.Result == ELNPSupportQueryResult::Supported;
			if (StairLayers.Contains(Probe.ExactLayer))
			{
				TPair<int32, int32>& Counts = OnStair.FindOrAdd(Probe.ExactLayer);
				++(bSupported ? Counts.Key : Counts.Value);
			}
			else
			{
				++(bSupported ? BesideSupported : BesideNeedsExact);
			}
		}
		FString StairReport;
		for (const uint16 LayerId : StairLayers)
		{
			const TPair<int32, int32> Counts = OnStair.FindRef(LayerId);
			StairReport += FString::Printf(TEXT(" Layer %d Supported=%d NeedsExact=%d;"), LayerId, Counts.Key, Counts.Value);
			TestTrue(FString::Printf(TEXT("Meadow stairs: Layer %d is stood on"), LayerId), Counts.Key + Counts.Value > 0);
		}
		AddInfo(FString::Printf(TEXT("Meadow stairs N=%d:%s beside Supported=%d NeedsExact=%d, no surface=%d"),
			N, *StairReport, BesideSupported, BesideNeedsExact, NoSurface));
		TestEqual(TEXT("Meadow stairs: Atlas never picks another Layer"), Inconsistent, 0);
		TestTrue(TEXT("Meadow stairs: the island top is supported beside the steps"), BesideSupported > 0);
	}
	return !HasAnyErrors();
}

#endif // WITH_DEV_AUTOMATION_TESTS
