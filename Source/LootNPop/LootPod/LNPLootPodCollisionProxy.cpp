// Copyright (c) 2026 LootNPop. All rights reserved.

#include "LootPod/LNPLootPodCollisionProxy.h"
#include "LootPod/LNPLootPodMassTypes.h"
#include "LootNPop.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "MassCommonFragments.h"
#include "MassExecutionContext.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	/** 엔진 BasicShapes Sphere의 반지름(cm). */
	constexpr float ProxyMeshRadius = 50.f;

	/** LootPod 캡슐(TerrainContract §2): 반지름 128cm, 반높이 128cm라 실질적으로 구다. 중심은 Pod 로컬 Up +128cm. */
	constexpr float PodProxyRadius = 128.f;
	constexpr float PodProxyCenterUp = 128.f;

}

// --- ULNPLootPodCollisionProxySubsystem ---

ULNPLootPodCollisionProxySubsystem::ULNPLootPodCollisionProxySubsystem()
{
	// 경로 문자열 LoadObject가 아니라 CDO 하드 참조로 잡아야 패키지 cook에 포함된다.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	ProxyMesh = SphereMesh.Object;
}

bool ULNPLootPodCollisionProxySubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

UInstancedStaticMeshComponent* ULNPLootPodCollisionProxySubsystem::GetOrCreateProxyComponent()
{
	if (ProxyComponent)
		return ProxyComponent;

	UWorld* World = GetWorld();
	check(World);

	// 머신마다 로컬로 만든다. 존재·위치는 이미 MassReplication이 전달하므로 복제하지 않는다.
	FActorSpawnParameters SpawnParams;
	SpawnParams.ObjectFlags |= RF_Transient;
	AActor* ProxyActor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, SpawnParams);
	check(ProxyActor);
#if WITH_EDITOR
	ProxyActor->SetActorLabel(TEXT("LootPodCollisionProxy"));
#endif

	ProxyComponent = NewObject<UInstancedStaticMeshComponent>(ProxyActor, TEXT("LootPodCollisionProxyISM"));
	ProxyComponent->SetStaticMesh(ProxyMesh);
	ProxyComponent->SetRemoveSwap();
	ProxyComponent->SetMobility(EComponentMobility::Movable);
	ProxyComponent->SetCollisionProfileName(TEXT("LNPStaticBlocker"));
	ProxyComponent->SetCanEverAffectNavigation(false);
	ProxyComponent->SetVisibility(false);
	ProxyComponent->SetCastShadow(false);
	ProxyActor->SetRootComponent(ProxyComponent);
	ProxyComponent->RegisterComponent();

	UE_CLOG(ProxyMesh == nullptr, LogLootNPop, Error,
		TEXT("LootPodCollisionProxy: Proxy mesh is missing. LootPods will have no collision."));

	return ProxyComponent;
}

void ULNPLootPodCollisionProxySubsystem::AddPod(const FMassEntityHandle Entity, const FTransform& PodTransform)
{
	if (EntityToInstance.Contains(Entity) || PendingAdds.ContainsByPredicate([Entity](const TPair<FMassEntityHandle, FTransform>& Pending) { return Pending.Key == Entity; }))
		return;

	PendingAdds.Emplace(Entity, PodTransform);
}

void ULNPLootPodCollisionProxySubsystem::RemovePod(const FMassEntityHandle Entity)
{
	// 아직 반영되지 않은 추가는 요청째 취소한다.
	if (PendingAdds.RemoveAll([Entity](const TPair<FMassEntityHandle, FTransform>& Pending) { return Pending.Key == Entity; }) > 0)
		return;

	if (EntityToInstance.Contains(Entity))
	{
		PendingRemoves.AddUnique(Entity);
	}
}

bool ULNPLootPodCollisionProxySubsystem::ApplyPendingChanges()
{
	if (PendingAdds.IsEmpty() && PendingRemoves.IsEmpty())
		return false;

	for (const FMassEntityHandle Entity : PendingRemoves)
	{
		int32 InstanceIndex = INDEX_NONE;
		if (!EntityToInstance.RemoveAndCopyValue(Entity, InstanceIndex))
			continue;

		check(ProxyComponent);
		ProxyComponent->RemoveInstance(InstanceIndex);

		// ISM과 같은 RemoveAtSwap — 마지막 인스턴스가 빈 index로 옮겨진다.
		InstanceToEntity.RemoveAtSwap(InstanceIndex, EAllowShrinking::No);
		if (InstanceToEntity.IsValidIndex(InstanceIndex))
		{
			EntityToInstance[InstanceToEntity[InstanceIndex]] = InstanceIndex;
		}
	}
	PendingRemoves.Reset();

	if (!PendingAdds.IsEmpty())
	{
		UInstancedStaticMeshComponent* ISM = GetOrCreateProxyComponent();
		for (const TPair<FMassEntityHandle, FTransform>& Pending : PendingAdds)
		{
			const FTransform& PodTransform = Pending.Value;
			const FVector Center = PodTransform.TransformPositionNoScale(FVector(0.f, 0.f, PodProxyCenterUp));
			const FTransform InstanceTransform(PodTransform.GetRotation(), Center, FVector(PodProxyRadius / ProxyMeshRadius));
			const int32 InstanceIndex = ISM->AddInstance(InstanceTransform, /*bWorldSpace=*/true);
			check(InstanceIndex == InstanceToEntity.Num());

			InstanceToEntity.Add(Pending.Key);
			EntityToInstance.Add(Pending.Key, InstanceIndex);
		}
		PendingAdds.Reset();
	}

	++Generation;
	return true;
}

FMassEntityHandle ULNPLootPodCollisionProxySubsystem::ResolveInstance(const int32 InstanceIndex) const
{
	return InstanceToEntity.IsValidIndex(InstanceIndex) ? InstanceToEntity[InstanceIndex] : FMassEntityHandle();
}

// --- ULNPLootPodCollisionProxyAddProcessor ---

ULNPLootPodCollisionProxyAddProcessor::ULNPLootPodCollisionProxyAddProcessor()
	: EntityQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionFlags = (int32)EProcessorExecutionFlags::AllNetModes;
	bRequiresGameThreadExecution = true; // ISM 인스턴스 추가
}

void ULNPLootPodCollisionProxyAddProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddTagRequirement<FLNPLootPodTag>(EMassFragmentPresence::All);
	EntityQuery.AddTagRequirement<FLNPLootPodCollisionProxyTag>(EMassFragmentPresence::None);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
}

void ULNPLootPodCollisionProxyAddProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	ULNPLootPodCollisionProxySubsystem* ProxySubsystem = UWorld::GetSubsystem<ULNPLootPodCollisionProxySubsystem>(EntityManager.GetWorld());
	if (ProxySubsystem == nullptr)
		return;

	EntityQuery.ForEachEntityChunk(Context, [ProxySubsystem](FMassExecutionContext& IterContext)
	{
		const TConstArrayView<FTransformFragment> Transforms = IterContext.GetFragmentView<FTransformFragment>();
		for (FMassExecutionContext::FEntityIterator EntityIt = IterContext.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			const FMassEntityHandle Entity = IterContext.GetEntity(EntityIt);
			ProxySubsystem->AddPod(Entity, Transforms[EntityIt].GetTransform());
			IterContext.Defer().AddTag<FLNPLootPodCollisionProxyTag>(Entity);
		}
	});
}

// --- ULNPLootPodCollisionProxyRemoveObserver ---

ULNPLootPodCollisionProxyRemoveObserver::ULNPLootPodCollisionProxyRemoveObserver()
	: EntityQuery(*this)
{
	ObservedTypes.Add(FLNPLootPodTag::StaticStruct());
	ObservedOperations = EMassObservedOperationFlags::Remove;
	ExecutionFlags = (int32)EProcessorExecutionFlags::AllNetModes;
	bRequiresGameThreadExecution = true; // ISM 인스턴스 제거
}

void ULNPLootPodCollisionProxyRemoveObserver::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddTagRequirement<FLNPLootPodTag>(EMassFragmentPresence::All);
	EntityQuery.AddTagRequirement<FLNPLootPodCollisionProxyTag>(EMassFragmentPresence::All);
}

void ULNPLootPodCollisionProxyRemoveObserver::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	ULNPLootPodCollisionProxySubsystem* ProxySubsystem = UWorld::GetSubsystem<ULNPLootPodCollisionProxySubsystem>(EntityManager.GetWorld());
	if (ProxySubsystem == nullptr)
		return;

	EntityQuery.ForEachEntityChunk(Context, [ProxySubsystem](FMassExecutionContext& IterContext)
	{
		for (FMassExecutionContext::FEntityIterator EntityIt = IterContext.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			ProxySubsystem->RemovePod(IterContext.GetEntity(EntityIt));
		}
	});
}
