#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "PhysicsEngine/BodySetup.h"

namespace
{
constexpr TCHAR COptionMeshObjectPath[] =
	TEXT("/Game/SurfaceNavigationTests/MeshTerrain/SM_COptionSphereSculpt.SM_COptionSphereSculpt");

UWorld* FindGameWorld()
{
	if (!GEngine)
	{
		return nullptr;
	}

	for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
	{
		if (WorldContext.WorldType == EWorldType::Game
			|| WorldContext.WorldType == EWorldType::PIE)
		{
			return WorldContext.World();
		}
	}
	return nullptr;
}

TArray<FRotator> GetOctantRotations()
{
	return {
		FRotator(0.0, 0.0, 0.0),
		FRotator(0.0, 90.0, 0.0),
		FRotator(0.0, 180.0, 0.0),
		FRotator(0.0, 270.0, 0.0),
		FRotator(180.0, 0.0, 0.0),
		FRotator(180.0, 90.0, 0.0),
		FRotator(180.0, 180.0, 0.0),
		FRotator(180.0, 270.0, 0.0)
	};
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPSurfaceNavigationPackagedRuntimeTest,
	"LootNPop.SurfaceNavigation.PackagedRuntime.COptionNaniteAndExactCollision",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FLNPSurfaceNavigationPackagedRuntimeTest::RunTest(const FString& Parameters)
{
	UWorld* TestWorld = FindGameWorld();
	TestNotNull(TEXT("Packaged runtime game world exists"), TestWorld);
	if (!TestWorld)
	{
		return false;
	}

	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, COptionMeshObjectPath);
	TestNotNull(TEXT("Cooked C-option Static Mesh loads"), Mesh);
	if (!Mesh)
	{
		return false;
	}

	TestTrue(TEXT("Cooked C-option mesh has valid render data"), Mesh->HasValidRenderData());
	TestTrue(TEXT("Cooked C-option mesh has valid Nanite data"), Mesh->HasValidNaniteData());
	TestTrue(TEXT("Cooked C-option mesh contains physics triangle data"),
		Mesh->ContainsPhysicsTriMeshData(true));

	const UBodySetup* BodySetup = Mesh->GetBodySetup();
	TestNotNull(TEXT("Cooked C-option mesh has a BodySetup"), BodySetup);
	if (BodySetup)
	{
		TestEqual(TEXT("Cooked C-option mesh uses exact complex collision"),
			BodySetup->CollisionTraceFlag,
			CTF_UseComplexAsSimple);
	}

	const FVector TestCenter(1000000.0, 0.0, 0.0);
	const TArray<FRotator> SlotRotations = GetOctantRotations();
	TArray<UStaticMeshComponent*> Components;
	Components.Reserve(SlotRotations.Num());
	for (const FRotator& SlotRotation : SlotRotations)
	{
		UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(TestWorld, NAME_None, RF_Transient);
		Component->SetStaticMesh(Mesh);
		Component->SetWorldLocationAndRotation(TestCenter, SlotRotation);
		Component->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Component->SetCollisionResponseToAllChannels(ECR_Block);
		Component->RegisterComponentWithWorld(TestWorld);
		Components.Add(Component);
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(LNPSurfaceNavigationPackagedRuntime), true);
	int32 SuccessfulTraces = 0;
	for (int32 SlotIndex = 0; SlotIndex < SlotRotations.Num(); ++SlotIndex)
	{
		const FVector Direction = SlotRotations[SlotIndex].RotateVector(
			FVector(1.0, 1.0, 1.0).GetSafeNormal());
		FHitResult Hit;
		if (TestWorld->LineTraceSingleByChannel(
			Hit,
			TestCenter,
			TestCenter + Direction * 40000.0,
			ECC_Visibility,
			QueryParams)
			&& Components.Contains(Hit.GetComponent()))
		{
			++SuccessfulTraces;
		}
	}
	TestEqual(TEXT("Packaged runtime exact scene query hits every octant"), SuccessfulTraces, 8);

	for (UStaticMeshComponent* Component : Components)
	{
		Component->UnregisterComponent();
		Component->DestroyComponent();
	}

	AddInfo(FString::Printf(
		TEXT("Packaged C-option mesh: valid Nanite data, exact traces=%d/%d"),
		SuccessfulTraces,
		SlotRotations.Num()));
	return !HasAnyErrors();
}

#endif
