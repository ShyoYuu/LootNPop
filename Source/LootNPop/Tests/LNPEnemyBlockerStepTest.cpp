// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Enemy/LNPEnemyExactMovement.h"
#include "Enemy/LNPEnemySurfaceMovement.h"
#include "GameFramework/Actor.h"
#include "LootPod/LNPLootPodCollisionProxy.h"
#include "SurfaceNavigation/LNPHitIdentityRegistry.h"
#include "SurfaceNavigation/LNPMassWorldCollision.h"
#include "SurfaceNavigation/LNPSupportAtlas.h"
#include "SurfaceNavigation/LNPSurfaceDataSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPEnemySupportlessBlockerStepTest,
	"LootNPop.SurfaceNavigation.EnemyMovement.SupportlessBlockerStep",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
	using namespace LNPEnemyExactMovement;

	constexpr double BlockerStepSurfaceRadius = 30000.0;
	constexpr float BlockerStepDeltaTime = 1.f / 60.f;
	constexpr float BlockerStepWalkSpeed = 300.f;
	constexpr int32 BlockerStepWalkFrames = 150;

	/**
	 * 캐시 경로 시험 월드. 반지름 30000cm 내부형 구의 Support Atlas(지각 Layer 하나)를 합성하고, 지각 자체의 exact 형상은
	 * 두지 않는다. 캐시 접지가 지면을 맡고 exact 형상은 시험하는 Blocker·Terrain·LootPod proxy뿐이다.
	 * 지면 좌표는 (1,1,1) 방향 지점의 접평면 x(전진)·y(옆)·h(현지 Up 높이)다.
	 */
	struct FBlockerStepWorld
	{
		UWorld* World = nullptr;
		ULNPHitIdentitySubsystem* HitIdentity = nullptr;
		ULNPMassWorldCollisionSubsystem* Collision = nullptr;
		ULNPLootPodCollisionProxySubsystem* LootPodProxy = nullptr;
		FLNPSurfaceDataSnapshot Snapshot;
		FParams Params;
		FVector Direction = FVector(1.0, 1.0, 1.0).GetSafeNormal();
		FVector Forward;
		FVector Side;

		explicit FBlockerStepWorld(const TCHAR* Name)
		{
			World = UWorld::CreateWorld(EWorldType::Game, /*bInformEngineOfWorld=*/false, Name);
			FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
			WorldContext.SetCurrentWorld(World);
			HitIdentity = World->GetSubsystem<ULNPHitIdentitySubsystem>();
			Collision = World->GetSubsystem<ULNPMassWorldCollisionSubsystem>();
			LootPodProxy = World->GetSubsystem<ULNPLootPodCollisionProxySubsystem>();

			Params.GravityOrigin = FVector::ZeroVector;
			Params.CapsuleRadius = 35.f;
			Params.CapsuleHalfHeight = 88.f;

			const FVector Up = -Direction;
			Forward = FVector::CrossProduct(Up, FVector::UpVector).GetSafeNormal();
			Side = FVector::CrossProduct(Up, Forward);
		}

		~FBlockerStepWorld()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}

		bool BuildCrustCache(FAutomationTestBase& Test)
		{
			FLNPSupportLayerRaster Crust;
			Crust.Layout = FLNPSupportLayout::MakeFull(2);
			Crust.SourceIndex = 0;
			Crust.Samples.SetNum(Crust.Layout.Num());
			for (int32 J = 0; J <= Crust.Layout.Subdivisions; ++J)
			{
				for (int32 I = 0; I <= Crust.Layout.Subdivisions - J; ++I)
				{
					const int32 Index = LNPSupportAtlas::GetSampleIndex(Crust.Layout.Subdivisions, I, J);
					const FVector3d SampleDirection = LNPSupportAtlas::GetSampleDirection(Crust.Layout.Subdivisions, I, J);
					Crust.Samples[Index].Radius = BlockerStepSurfaceRadius;
					Crust.Samples[Index].Normal = FVector3f(-SampleDirection);
					Crust.Samples[Index].Flags = ELNPSupportSampleFlags::Valid | ELNPSupportSampleFlags::Walkable;
				}
			}

			FLNPSupportAtlasSource Source;
			Source.Key = TEXT("CrustActor.CrustComponent");
			Source.FaceMap.UniformLayer = 0;
			TArray<FLNPSupportAtlasSource> Sources = {MoveTemp(Source)};
			FLNPSupportCodecSettings Codec;
			Codec.BaseRadius = BlockerStepSurfaceRadius;
			TArray<uint8> Payload;
			FString Error;
			TSharedPtr<FLNPSupportAtlas, ESPMode::ThreadSafe> Atlas = MakeShared<FLNPSupportAtlas, ESPMode::ThreadSafe>();
			if (!LNPSupportAtlas::Encode(MakeArrayView(&Crust, 1), Sources, Codec, Payload, Error)
				|| !LNPSupportAtlas::Decode(Payload, *Atlas, Error))
			{
				Test.AddError(FString::Printf(TEXT("Synthetic crust cache failed: %s"), *Error));
				return false;
			}
			Snapshot.Generation = 31;
			Snapshot.Slots.SetNum(8);
			Snapshot.Slots[0].Support = Atlas;
			return true;
		}

		FVector GroundPoint(const double X, const double Y) const
		{
			return (Direction * BlockerStepSurfaceRadius + Forward * X + Side * Y).GetSafeNormal() * BlockerStepSurfaceRadius;
		}

		static FVector UpAt(const FVector& Point) { return -Point.GetSafeNormal(); }

		/** 캐시 규약의 접지 캡슐 중심(캡슐 바닥이 지면). */
		FVector CapsuleCenterAt(const double X) const
		{
			const FVector Ground = GroundPoint(X, 0.0);
			return Ground + UpAt(Ground) * Params.CapsuleHalfHeight;
		}

		/** 지면 위에 놓인 상자. 크기는 전진·옆·높이(cm)다. */
		UStaticMeshComponent* Box(const double X, const FVector& Size, const FName Profile)
		{
			const FVector Ground = GroundPoint(X, 0.0);
			const FVector Up = UpAt(Ground);
			AActor* Owner = World->SpawnActor<AActor>();
			UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(Owner);
			Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
			Mesh->SetMobility(EComponentMobility::Static);
			Mesh->SetCollisionProfileName(Profile);
			Mesh->SetWorldLocationAndRotation(Ground + Up * (Size.Z * 0.5), FRotationMatrix::MakeFromZX(Up, Forward).Rotator());
			Mesh->SetWorldScale3D(Size / 100.0);
			Owner->SetRootComponent(Mesh);
			Mesh->RegisterComponent();
			HitIdentity->RegisterRuntimeSource(Mesh);
			return Mesh;
		}

		void LootPod(const double X)
		{
			const FVector Ground = GroundPoint(X, 0.0);
			LootPodProxy->AddPod(FMassEntityHandle(500, 1), FTransform(FRotationMatrix::MakeFromZX(UpAt(Ground), Forward).ToQuat(), Ground));
		}

		/** 형상을 모두 놓은 뒤 identity snapshot(LootPod proxy 포함)을 게시한다. */
		void Publish() const { HitIdentity->Tick(0.f); }

		double ForwardOf(const FVector& Location) const { return FVector::DotProduct(Location, Forward); }
	};

	struct FBlockerStepWalkResult
	{
		double FinalX = 0.0;
		double MaxX = -UE_BIG_NUMBER;
		int32 Rejected = 0;
		int32 LostSupport = 0;
		bool bEverInsideBlocker = false;
	};

	/** 이동 프로세서와 같은 접지 한 프레임(cache-first)을 전진 속도로 반복한다. */
	FBlockerStepWalkResult WalkBlockerStep(const FBlockerStepWorld& Fixture, const double StartX)
	{
		FBlockerStepWalkResult Result;
		FVector Location = Fixture.CapsuleCenterAt(StartX);
		FLNPSurfaceHandle Surface;
		Surface.OctantSlot = 0;
		Surface.LocalLayerId = 0;
		Surface.Generation = Fixture.Snapshot.Generation;
		const bool bStartedInside = OverlapsSupportlessBlocker(*Fixture.Collision, Fixture.Params, Location);
		for (int32 Frame = 0; Frame < BlockerStepWalkFrames; ++Frame)
		{
			const FVector Velocity = FVector::VectorPlaneProject(Fixture.Forward, FBlockerStepWorld::UpAt(Location)).GetSafeNormal() * BlockerStepWalkSpeed;
			FVector Next;
			FVector PhysVelocity;
			const EGroundResult Ground = LNPEnemySurfaceMovement::StepGrounded(
				&Fixture.Snapshot, *Fixture.Collision, Fixture.Params, Location, Velocity, BlockerStepDeltaTime, Next, PhysVelocity, Surface);
			Result.Rejected += Ground == EGroundResult::Rejected ? 1 : 0;
			Result.LostSupport += Ground == EGroundResult::LostSupport ? 1 : 0;
			Location = Next;
			Result.MaxX = FMath::Max(Result.MaxX, Fixture.ForwardOf(Location));
			if (!bStartedInside)
			{
				Result.bEverInsideBlocker |= OverlapsSupportlessBlocker(*Fixture.Collision, Fixture.Params, Location);
			}
		}
		Result.FinalX = Fixture.ForwardOf(Location);
		return Result;
	}
}

bool FLNPEnemySupportlessBlockerStepTest::RunTest(const FString& Parameters)
{
	// 상자 앞면 x=180, 캡슐 반지름35 → 겹침 허용치1cm까지 접근하면 축은 x≈146에서 멈춘다(전진 좌표는 구면 투영으로 약0.3% 짧다).
	constexpr double BlockerX = 200.0;
	constexpr double BlockerFace = BlockerX - 20.0;
	const double StopLimit = BlockerFace - 35.0 + 1.0 + 0.5;

	auto RunCase = [this](const TCHAR* Name, TFunctionRef<void(FBlockerStepWorld&)> Build, const double StartX, FBlockerStepWalkResult& OutResult)
	{
		FBlockerStepWorld Fixture(Name);
		if (!Fixture.BuildCrustCache(*this))
			return false;
		Build(Fixture);
		Fixture.Publish();
		OutResult = WalkBlockerStep(Fixture, StartX);
		TestEqual(FString::Printf(TEXT("%s: cache keeps support"), Name), OutResult.LostSupport, 0);
		return true;
	};

	// 1. Sandstone 크기(20cm) Blocker: MaxStepUp(45cm) 아래지만 턱으로 넘지 않는다.
	{
		FBlockerStepWalkResult Result;
		if (!RunCase(TEXT("LowBlocker"), [](FBlockerStepWorld& F) { F.Box(200.0, FVector(40, 200, 20), TEXT("LNPStaticBlocker")); }, 0.0, Result))
			return false;
		TestTrue(TEXT("Low Blocker is not stepped over"), Result.MaxX <= StopLimit);
		TestTrue(TEXT("Low Blocker stops the walk with Rejected"), Result.Rejected > 0);
		TestFalse(TEXT("Capsule never ends inside the low Blocker"), Result.bEverInsideBlocker);
	}

	// 2. Moss 경계(55cm): 올린 sweep은 윗부분만 보므로 아래쪽이 캡슐에 들어오기 전에 같은 규칙으로 멈춘다.
	{
		FBlockerStepWalkResult Result;
		if (!RunCase(TEXT("BoundaryBlocker"), [](FBlockerStepWorld& F) { F.Box(200.0, FVector(40, 200, 55), TEXT("LNPStaticBlocker")); }, 0.0, Result))
			return false;
		TestTrue(TEXT("Blocker slightly above MaxStepUp stops before overlap"), Result.MaxX <= StopLimit);
		TestFalse(TEXT("Capsule never ends inside the boundary Blocker"), Result.bEverInsideBlocker);
	}

	// 3. 같은 높이의 Support+Blocker(지형 계약)는 막지 않는다. 캐시는 평면 지각이므로 판정 술어만 확인한다.
	{
		FBlockerStepWalkResult Result;
		if (!RunCase(TEXT("SupportStep"), [](FBlockerStepWorld& F) { F.Box(200.0, FVector(40, 200, 30), TEXT("LNPStaticTerrain")); }, 0.0, Result))
			return false;
		TestTrue(TEXT("Support+Blocker step is not treated as a supportless Blocker"), Result.FinalX > 400.0);
		TestEqual(TEXT("Support+Blocker step causes no rejection"), Result.Rejected, 0);
	}

	// 4. 캡슐 아랫부분이 Support에 묻힌 경사 접지 상황: Support 겹침이 Blocker 판정을 가리지 않는다.
	{
		FBlockerStepWalkResult Result;
		if (!RunCase(TEXT("EmbeddedSupport"), [](FBlockerStepWorld& F)
			{
				F.Box(150.0, FVector(900, 200, 5), TEXT("LNPStaticTerrain"));
				F.Box(200.0, FVector(40, 200, 20), TEXT("LNPStaticBlocker"));
			}, 0.0, Result))
			return false;
		TestTrue(TEXT("Blocker is found while the capsule overlaps Support"), Result.MaxX <= StopLimit);
		TestFalse(TEXT("Capsule never ends inside the Blocker on embedded Support"), Result.bEverInsideBlocker);

		FBlockerStepWalkResult Clear;
		if (!RunCase(TEXT("EmbeddedSupportOnly"), [](FBlockerStepWorld& F) { F.Box(150.0, FVector(900, 200, 5), TEXT("LNPStaticTerrain")); }, 0.0, Clear))
			return false;
		TestEqual(TEXT("Embedded Support alone causes no rejection"), Clear.Rejected, 0);
	}

	// 5. 이미 Blocker와 겹친 채 출발하면(스폰·상태형 지형이 덮음) 빠져나갈 수 있다.
	{
		FBlockerStepWalkResult Result;
		if (!RunCase(TEXT("EscapeBlocker"), [](FBlockerStepWorld& F) { F.Box(200.0, FVector(40, 200, 20), TEXT("LNPStaticBlocker")); }, 200.0, Result))
			return false;
		TestTrue(TEXT("Entity starting inside a Blocker walks out"), Result.FinalX > 300.0);
	}

	// 6. LootPod proxy(반지름128cm 구, 중심 +128cm): 올린 sweep이 적도 높이에서 멈추던 거리(구 중심에서 163cm+WallSkin)를 유지한다.
	{
		constexpr double PodX = 400.0;
		FBlockerStepWalkResult Result;
		if (!RunCase(TEXT("LootPodProxy"), [](FBlockerStepWorld& F) { F.LootPod(400.0); }, 0.0, Result))
			return false;
		const double ExpectedStop = PodX - (LNPLootPodCollisionProxy::Radius + 35.0 + 1.0);
		TestEqual(TEXT("LootPod proxy causes no new rejection"), Result.Rejected, 0);
		TestTrue(TEXT("LootPod proxy stop distance is unchanged"), FMath::Abs(Result.FinalX - ExpectedStop) <= 3.0);
	}

	return !HasAnyErrors();
}

#endif
