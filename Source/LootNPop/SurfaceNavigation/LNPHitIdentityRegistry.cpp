// Copyright (c) 2026 LootNPop. All rights reserved.

#include "SurfaceNavigation/LNPHitIdentityRegistry.h"
#include "SurfaceNavigation/LNPCollisionChannels.h"
#include "GameLogic/LNPOctantSpawnSubsystem.h"
#include "LootPod/LNPLootPodCollisionProxy.h"
#include "LootNPop.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/HitResult.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

namespace
{
	/** 옥탄트 slot 수. ULNPOctantSpawnSubsystem의 고정 8 slot과 같다. */
	constexpr int32 OctantSlotCount = 8;

	bool IsRegistrable(const UPrimitiveComponent& Component)
	{
		return Component.IsQueryCollisionEnabled()
			&& Component.GetCollisionResponseToChannel(LNPCollisionChannels::WorldExact) == ECR_Block;
	}
}

bool ULNPHitIdentitySubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

bool ULNPHitIdentitySubsystem::ClassifyProfile(const FName ProfileName, ELNPExactSourceLifetime& OutLifetime, uint8& OutRoles)
{
	using namespace ELNPExactSourceRole;

	// Terrain Contract profile 표(TerrainContract.md §4). Support-only profile은 LNPWorldExact를 무시하므로 목록에 없다.
	struct FProfileClass { const TCHAR* Name; ELNPExactSourceLifetime Lifetime; uint8 Roles; };
	static const FProfileClass Classes[] =
	{
		{ TEXT("LNPStaticTerrain"),       ELNPExactSourceLifetime::Static,       Support | Blocker },
		{ TEXT("LNPStaticBlocker"),       ELNPExactSourceLifetime::Static,       Blocker },
		{ TEXT("LNPDynamicTerrain"),      ELNPExactSourceLifetime::Dynamic,      Support | Blocker },
		{ TEXT("LNPStatefulTraversal"),   ELNPExactSourceLifetime::Dynamic,      Blocker },
		{ TEXT("LNPDestructibleBlocker"), ELNPExactSourceLifetime::Destructible, Blocker },
		{ TEXT("LNPDestructibleTerrain"), ELNPExactSourceLifetime::Destructible, Support | Blocker },
	};

	for (const FProfileClass& Class : Classes)
	{
		if (ProfileName == Class.Name)
		{
			OutLifetime = Class.Lifetime;
			OutRoles = Class.Roles;
			return true;
		}
	}
	return false;
}

void ULNPHitIdentitySubsystem::RegisterRuntimeSource(UPrimitiveComponent* Component)
{
	check(IsInGameThread());
	if (Component == nullptr)
		return;

	FLNPExactSourceEntry Entry;
	if (!IsRegistrable(*Component) || !ClassifyProfile(Component->GetCollisionProfileName(), Entry.Lifetime, Entry.Roles))
	{
		UE_LOG(LogLootNPop, Warning, TEXT("HitIdentity: %s (profile %s) is not an LNPWorldExact source. Not registered."),
			*GetPathNameSafe(Component), *Component->GetCollisionProfileName().ToString());
		return;
	}

	RuntimeSources.Add(Component, Entry);
	bDirty = true;
}

void ULNPHitIdentitySubsystem::UnregisterRuntimeSource(UPrimitiveComponent* Component)
{
	check(IsInGameThread());
	if (RuntimeSources.Remove(Component) > 0)
	{
		bDirty = true;
	}
}

void ULNPHitIdentitySubsystem::RefreshSlotSources()
{
	TArray<TWeakObjectPtr<ULevel>> CurrentSlotLevels;
	if (const ULNPOctantSpawnSubsystem* OctantSubsystem = GetWorld()->GetSubsystem<ULNPOctantSpawnSubsystem>())
	{
		if (OctantSubsystem->bGenerationComplete)
		{
			for (int32 Slot = 0; Slot < OctantSlotCount; ++Slot)
			{
				CurrentSlotLevels.Add(OctantSubsystem->GetSlotLevel(Slot));
			}
		}
	}

	bool bChanged = CurrentSlotLevels.Num() != RegisteredSlotLevels.Num();
	for (int32 Slot = 0; !bChanged && Slot < CurrentSlotLevels.Num(); ++Slot)
	{
		bChanged = !CurrentSlotLevels[Slot].HasSameIndexAndSerialNumber(RegisteredSlotLevels[Slot]);
	}
	if (!bChanged)
		return;

	// 생성 재시작·언로드도 여기로 온다. 이전 slot의 source는 모두 버린다.
	RegisteredSlotLevels = CurrentSlotLevels;
	SlotSources.Reset();
	bDirty = true;

	int32 UnclassifiedCount = 0;
	for (int32 Slot = 0; Slot < RegisteredSlotLevels.Num(); ++Slot)
	{
		const ULevel* Level = RegisteredSlotLevels[Slot].Get();
		if (Level == nullptr)
			continue;

		for (const AActor* Actor : Level->Actors)
		{
			if (Actor == nullptr)
				continue;

			Actor->ForEachComponent<UPrimitiveComponent>(/*bIncludeFromChildActors=*/false, [&](UPrimitiveComponent* Component)
			{
				if (!IsRegistrable(*Component))
					return;

				FLNPExactSourceEntry Entry;
				Entry.Slot = static_cast<int8>(Slot);
				if (!ClassifyProfile(Component->GetCollisionProfileName(), Entry.Lifetime, Entry.Roles))
				{
					// 미분류는 hit 때 UnknownExactSurface가 된다. 목록은 LNP.SurfaceNav.AuditExactResponse로 본다.
					++UnclassifiedCount;
					return;
				}
				SlotSources.Add(Component, Entry);
			});
		}
	}

	UE_LOG(LogLootNPop, Log, TEXT("HitIdentity: Registered %d slot sources from %d slot levels (%d unclassified LNPWorldExact blockers)."),
		SlotSources.Num(), RegisteredSlotLevels.Num(), UnclassifiedCount);
}

void ULNPHitIdentitySubsystem::Publish()
{
	TSharedRef<FLNPHitIdentitySnapshot, ESPMode::ThreadSafe> NewSnapshot = MakeShared<FLNPHitIdentitySnapshot, ESPMode::ThreadSafe>();
	NewSnapshot->Sources.Reserve(SlotSources.Num() + RuntimeSources.Num() + 1);
	NewSnapshot->Sources.Append(SlotSources);
	NewSnapshot->Sources.Append(RuntimeSources);

	if (const ULNPLootPodCollisionProxySubsystem* ProxySubsystem = GetWorld()->GetSubsystem<ULNPLootPodCollisionProxySubsystem>())
	{
		if (UInstancedStaticMeshComponent* ProxyComponent = ProxySubsystem->GetProxyComponent())
		{
			FLNPExactSourceEntry Entry;
			if (ClassifyProfile(ProxyComponent->GetCollisionProfileName(), Entry.Lifetime, Entry.Roles))
			{
				Entry.bLootPodProxy = true;
				NewSnapshot->Sources.Add(ProxyComponent, Entry);
				NewSnapshot->LootPodProxyInstances = ProxySubsystem->GetInstanceEntities();
			}
		}
	}

	NewSnapshot->Generation = Snapshot->Generation + 1;
	Snapshot = NewSnapshot;
}

void ULNPHitIdentitySubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	RefreshSlotSources();

	// proxy의 물리 index 변경과 표 게시를 같은 시점에 묶는다(ULNPLootPodCollisionProxySubsystem 주석).
	if (ULNPLootPodCollisionProxySubsystem* ProxySubsystem = GetWorld()->GetSubsystem<ULNPLootPodCollisionProxySubsystem>())
	{
		bDirty |= ProxySubsystem->ApplyPendingChanges();
	}

	if (bDirty)
	{
		bDirty = false;
		Publish();
	}
}

TStatId ULNPHitIdentitySubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(ULNPHitIdentitySubsystem, STATGROUP_Tickables);
}

void ULNPHitIdentitySubsystem::Deinitialize()
{
	SlotSources.Reset();
	RuntimeSources.Reset();
	RegisteredSlotLevels.Reset();
	Snapshot = MakeShared<FLNPHitIdentitySnapshot, ESPMode::ThreadSafe>();

	Super::Deinitialize();
}

FLNPExactHitIdentity ULNPHitIdentitySubsystem::ResolveHit(const FLNPHitIdentitySnapshot& InSnapshot, const FHitResult& Hit)
{
	FLNPExactHitIdentity Identity;
	Identity.RegistryGeneration = InSnapshot.Generation;
	Identity.FaceIndex = Hit.FaceIndex;

	const FLNPExactSourceEntry* Entry = InSnapshot.Sources.Find(Hit.Component);
	if (Entry == nullptr)
		return Identity;

	if (Entry->bLootPodProxy)
	{
		if (!InSnapshot.LootPodProxyInstances.IsValidIndex(Hit.Item))
			return Identity;

		Identity.InstanceIndex = Hit.Item;
		Identity.Entity = InSnapshot.LootPodProxyInstances[Hit.Item];
	}
	else
	{
		// 일반 component의 Item은 INDEX_NONE이고, 슬롯 HISM은 instance index다.
		Identity.InstanceIndex = Hit.Item;
	}

	Identity.Lifetime = Entry->Lifetime;
	Identity.Roles = Entry->Roles;
	Identity.Slot = Entry->Slot;
	return Identity;
}

FLNPExactHitIdentity ULNPHitIdentitySubsystem::ResolveHit(const FHitResult& Hit) const
{
	const FLNPExactHitIdentity Identity = ResolveHit(*GetSnapshot(), Hit);
	if (!Identity.IsKnown())
	{
		UnknownHitCount.fetch_add(1, std::memory_order_relaxed);
	}
	return Identity;
}
