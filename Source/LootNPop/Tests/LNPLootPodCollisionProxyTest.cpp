// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "LootPod/LNPLootPodCollisionProxy.h"
#include "SurfaceNavigation/LNPCollisionChannels.h"
#include "SurfaceNavigation/LNPHitIdentityRegistry.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPLootPodCollisionProxySwapRemapTest,
	"LootNPop.SurfaceNavigation.HitIdentity.LootPodProxySwapRemap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
	constexpr int32 PodCount = 4;
	constexpr double PodSpacing = 1000.0;

	/** subsystem의 proxy 중심 규약과 같다: Pod 로컬 Up +128cm. */
	constexpr double TestPodProxyCenterUp = 128.0;

	FVector GetPodLocation(const int32 PodIndex)
	{
		return FVector(PodIndex * PodSpacing, 0.0, 0.0);
	}

	/** Pod 중심을 위에서 아래로 관통하는 LNPWorldExact line trace. */
	bool TracePod(UWorld* World, const int32 PodIndex, FHitResult& OutHit)
	{
		const FVector Center = GetPodLocation(PodIndex) + FVector(0.0, 0.0, TestPodProxyCenterUp);
		const FCollisionQueryParams Params(SCENE_QUERY_STAT(LNPLootPodProxySwapRemapTest), false);
		return World->LineTraceSingleByChannel(OutHit, Center + FVector(0.0, 0.0, 1000.0), Center - FVector(0.0, 0.0, 1000.0),
			LNPCollisionChannels::WorldExact, Params);
	}

	/** proxy에 맞은 instance index. 빗나가거나 다른 component면 INDEX_NONE. */
	int32 TraceProxyItem(UWorld* World, const UPrimitiveComponent* ProxyComponent, const int32 PodIndex)
	{
		FHitResult Hit;
		return TracePod(World, PodIndex, Hit) && Hit.GetComponent() == ProxyComponent ? Hit.Item : INDEX_NONE;
	}
}

bool FLNPLootPodCollisionProxySwapRemapTest::RunTest(const FString& Parameters)
{
	// subsystem은 Game·PIE 월드에서만 생긴다. 월드 tick이 없으므로 게시는 HitIdentity->Tick을 직접 호출한다.
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, /*bInformEngineOfWorld=*/false, TEXT("LNPLootPodProxySwapRemapTest"));
	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	WorldContext.SetCurrentWorld(World);

	ON_SCOPE_EXIT
	{
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
	};

	ULNPLootPodCollisionProxySubsystem* Proxy = World->GetSubsystem<ULNPLootPodCollisionProxySubsystem>();
	ULNPHitIdentitySubsystem* HitIdentity = World->GetSubsystem<ULNPHitIdentitySubsystem>();
	if (!TestNotNull(TEXT("Proxy subsystem exists in a Game world"), Proxy)
		|| !TestNotNull(TEXT("Hit identity subsystem exists in a Game world"), HitIdentity))
		return false;

	// 엔티티 핸들은 subsystem에서 키로만 쓰이므로 EntityManager 없이 만든다.
	TArray<FMassEntityHandle> Entities;
	for (int32 PodIndex = 0; PodIndex < PodCount; ++PodIndex)
	{
		Entities.Add(FMassEntityHandle(100 + PodIndex, 1));
		Proxy->AddPod(Entities.Last(), FTransform(GetPodLocation(PodIndex)));
	}

	TestNull(TEXT("Adds are deferred until publish"), Proxy->GetProxyComponent());
	HitIdentity->Tick(0.f);

	UInstancedStaticMeshComponent* ISM = Proxy->GetProxyComponent();
	if (!TestNotNull(TEXT("Proxy ISM is created on first publish"), ISM))
		return false;

	TestEqual(TEXT("Instance count after adds"), ISM->GetInstanceCount(), PodCount);
	for (int32 PodIndex = 0; PodIndex < PodCount; ++PodIndex)
	{
		TestTrue(FString::Printf(TEXT("Initial resolve of index %d"), PodIndex), Proxy->ResolveInstance(PodIndex) == Entities[PodIndex]);
		TestEqual(FString::Printf(TEXT("Initial trace Item for pod %d"), PodIndex), TraceProxyItem(World, ISM, PodIndex), PodIndex);
	}

	// registry 해석: proxy hit는 Static Blocker이고 엔티티로 해석된다.
	{
		FHitResult Hit;
		TracePod(World, 2, Hit);
		const FLNPExactHitIdentity Identity = HitIdentity->ResolveHit(Hit);
		TestTrue(TEXT("Proxy hit is known"), Identity.IsKnown());
		TestTrue(TEXT("Proxy hit is static"), Identity.Lifetime == ELNPExactSourceLifetime::Static);
		TestEqual(TEXT("Proxy hit is blocker only"), static_cast<int32>(Identity.Roles), static_cast<int32>(ELNPExactSourceRole::Blocker));
		TestEqual(TEXT("Proxy hit has no slot"), static_cast<int32>(Identity.Slot), static_cast<int32>(INDEX_NONE));
		TestTrue(TEXT("Proxy hit resolves to pod 2"), Identity.Entity == Entities[2]);
	}

	// 중복 추가는 무시한다.
	const uint32 GenerationAfterAdds = Proxy->GetGeneration();
	const uint32 SnapshotGenerationAfterAdds = HitIdentity->GetSnapshot()->Generation;
	Proxy->AddPod(Entities[0], FTransform(GetPodLocation(0)));
	HitIdentity->Tick(0.f);
	TestEqual(TEXT("Duplicate add keeps generation"), Proxy->GetGeneration(), GenerationAfterAdds);
	TestEqual(TEXT("Duplicate add does not republish"), HitIdentity->GetSnapshot()->Generation, SnapshotGenerationAfterAdds);
	TestEqual(TEXT("Duplicate add keeps instance count"), ISM->GetInstanceCount(), PodCount);

	// 중간 index 제거 요청: 게시 전에는 물리와 표 모두 그대로다.
	Proxy->RemovePod(Entities[1]);
	TestEqual(TEXT("Pending remove keeps instance count"), ISM->GetInstanceCount(), PodCount);
	TestEqual(TEXT("Pending remove keeps physics index of pod 1"), TraceProxyItem(World, ISM, 1), 1);
	TestTrue(TEXT("Pending remove keeps table entry of pod 1"), Proxy->ResolveInstance(1) == Entities[1]);

	// 게시: 마지막 Pod(3)가 index 1로 옮겨진다.
	HitIdentity->Tick(0.f);
	TestEqual(TEXT("Middle remove bumps proxy generation once"), Proxy->GetGeneration(), GenerationAfterAdds + 1);
	TestEqual(TEXT("Middle remove republishes"), HitIdentity->GetSnapshot()->Generation, SnapshotGenerationAfterAdds + 1);
	TestEqual(TEXT("Instance count after middle remove"), ISM->GetInstanceCount(), PodCount - 1);
	TestTrue(TEXT("Swapped index 1 resolves to the last pod"), Proxy->ResolveInstance(1) == Entities[3]);
	TestTrue(TEXT("Index 0 unchanged"), Proxy->ResolveInstance(0) == Entities[0]);
	TestTrue(TEXT("Index 2 unchanged"), Proxy->ResolveInstance(2) == Entities[2]);
	TestFalse(TEXT("Removed tail index resolves to nothing"), Proxy->ResolveInstance(3).IsSet());

	FTransform MovedInstanceTransform;
	ISM->GetInstanceTransform(1, MovedInstanceTransform, /*bWorldSpace=*/true);
	TestTrue(TEXT("ISM index 1 holds the last pod's transform"),
		MovedInstanceTransform.GetLocation().Equals(GetPodLocation(3) + FVector(0.0, 0.0, TestPodProxyCenterUp), 0.1));

	// 물리 body도 같이 swap돼야 exact hit의 Item이 표와 일치한다.
	TestEqual(TEXT("Trace at the moved pod hits Item 1"), TraceProxyItem(World, ISM, 3), 1);
	TestEqual(TEXT("Trace at the removed pod misses the proxy"), TraceProxyItem(World, ISM, 1), INDEX_NONE);
	{
		FHitResult Hit;
		TracePod(World, 3, Hit);
		TestTrue(TEXT("Registry resolves the moved pod to the same entity"), HitIdentity->ResolveHit(Hit).Entity == Entities[3]);
	}

	// 마지막 index 제거는 swap 없이 끝난다.
	Proxy->RemovePod(Entities[2]);
	HitIdentity->Tick(0.f);
	TestEqual(TEXT("Tail remove bumps generation once"), Proxy->GetGeneration(), GenerationAfterAdds + 2);
	TestEqual(TEXT("Instance count after tail remove"), ISM->GetInstanceCount(), PodCount - 2);
	TestTrue(TEXT("Index 0 still pod 0"), Proxy->ResolveInstance(0) == Entities[0]);
	TestTrue(TEXT("Index 1 still pod 3"), Proxy->ResolveInstance(1) == Entities[3]);
	TestEqual(TEXT("Trace at pod 3 still hits Item 1"), TraceProxyItem(World, ISM, 3), 1);

	// 없는 엔티티 제거는 표를 건드리지 않는다.
	Proxy->RemovePod(Entities[1]);
	HitIdentity->Tick(0.f);
	TestEqual(TEXT("Unknown remove keeps generation"), Proxy->GetGeneration(), GenerationAfterAdds + 2);

	// 게시 전에 취소된 추가는 흔적을 남기지 않는다.
	const FMassEntityHandle Transient(200, 1);
	Proxy->AddPod(Transient, FTransform(GetPodLocation(5)));
	Proxy->RemovePod(Transient);
	HitIdentity->Tick(0.f);
	TestEqual(TEXT("Cancelled add keeps generation"), Proxy->GetGeneration(), GenerationAfterAdds + 2);
	TestEqual(TEXT("Cancelled add keeps instance count"), ISM->GetInstanceCount(), PodCount - 2);

	// 미등록 hit: registry에 없는 component는 UnknownExactSurface다.
	{
		FHitResult Hit;
		Hit.Component = ISM;
		FLNPHitIdentitySnapshot EmptySnapshot;
		TestFalse(TEXT("Unregistered component is unknown"), ULNPHitIdentitySubsystem::ResolveHit(EmptySnapshot, Hit).IsKnown());

		const uint32 UnknownBefore = HitIdentity->GetUnknownHitCount();
		Hit.Item = 99;
		TestFalse(TEXT("Out-of-range proxy item is unknown"), HitIdentity->ResolveHit(Hit).IsKnown());
		TestEqual(TEXT("Unknown hit counter increments"), HitIdentity->GetUnknownHitCount(), UnknownBefore + 1);
	}

	return !HasAnyErrors();
}

#endif
