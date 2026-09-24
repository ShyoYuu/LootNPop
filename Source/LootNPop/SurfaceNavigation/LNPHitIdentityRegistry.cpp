// Copyright (c) 2026 LootNPop. All rights reserved.

#include "SurfaceNavigation/LNPHitIdentityRegistry.h"
#include "SurfaceNavigation/LNPCollisionChannels.h"
#include "GameLogic/LNPOctantSpawnSubsystem.h"
#include "LootPod/LNPLootPodCollisionProxy.h"
#include "LootNPop.h"

#include "Chaos/TriangleMeshImplicitObject.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/HitResult.h"
#include "Engine/Level.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "PhysicsEngine/BodySetup.h"

namespace
{
	/** 옥탄트 slot 수. ULNPOctantSpawnSubsystem의 고정 8 slot과 같다. */
	constexpr int32 OctantSlotCount = 8;

	bool IsRegistrable(const UPrimitiveComponent& Component)
	{
		return Component.IsQueryCollisionEnabled()
			&& Component.GetCollisionResponseToChannel(LNPCollisionChannels::WorldExact) == ECR_Block;
	}

	/** AABB 안에서 원점에 가장 먼 점(꼭짓점)까지의 거리 제곱. */
	double FarthestCornerDistSquared(const FBox& Box)
	{
		const FVector Far(
			FMath::Max(FMath::Abs(Box.Min.X), FMath::Abs(Box.Max.X)),
			FMath::Max(FMath::Abs(Box.Min.Y), FMath::Abs(Box.Max.Y)),
			FMath::Max(FMath::Abs(Box.Min.Z), FMath::Abs(Box.Max.Z)));
		return Far.SizeSquared();
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

float ULNPHitIdentitySubsystem::ComputeSourceMaxRadius(UPrimitiveComponent& Component)
{
	double MaxDistSquared = 0.0;

	if (const UInstancedStaticMeshComponent* Instanced = Cast<UInstancedStaticMeshComponent>(&Component))
	{
		// 프랍 HISM은 작아서 instance별 mesh bounds로 충분하다.
		if (const UStaticMesh* Mesh = Instanced->GetStaticMesh())
		{
			const FBox LocalBox = Mesh->GetBounds().GetBox();
			for (int32 Index = 0; Index < Instanced->GetInstanceCount(); ++Index)
			{
				FTransform InstanceToWorld;
				if (Instanced->GetInstanceTransform(Index, InstanceToWorld, /*bWorldSpace=*/true))
				{
					MaxDistSquared = FMath::Max(MaxDistSquared, FarthestCornerDistSquared(LocalBox.TransformBy(InstanceToWorld)));
				}
			}
		}
		return static_cast<float>(FMath::Sqrt(MaxDistSquared));
	}

	const FTransform& ToWorld = Component.GetComponentTransform();
	bool bMeasured = false;
	if (UBodySetup* BodySetup = Component.GetBodySetup())
	{
		// cooked 런타임에도 남아 있는 물리 trimesh 정점이다. 정점은 scale 전 mesh 공간이다.
		for (const Chaos::FTriangleMeshImplicitObjectPtr& TriMesh : BodySetup->TriMeshGeometries)
		{
			if (!TriMesh.IsValid())
				continue;

			const Chaos::FTriangleMeshImplicitObject::ParticlesType& Particles = TriMesh->Particles();
			for (uint32 Index = 0; Index < Particles.Size(); ++Index)
			{
				const FVector WorldPos = ToWorld.TransformPosition(FVector(Particles.GetX(Index)));
				MaxDistSquared = FMath::Max(MaxDistSquared, WorldPos.SizeSquared());
			}
			bMeasured = true;
		}

		const FBox SimpleBox = BodySetup->AggGeom.CalcAABB(ToWorld);
		if (SimpleBox.IsValid)
		{
			MaxDistSquared = FMath::Max(MaxDistSquared, FarthestCornerDistSquared(SimpleBox));
			bMeasured = true;
		}
	}

	if (!bMeasured)
	{
		// BodySetup이 없는 primitive(shape component 등)는 bounds sphere로 보수적으로 잰다.
		const FBoxSphereBounds& Bounds = Component.Bounds;
		return static_cast<float>(Bounds.Origin.Size() + Bounds.SphereRadius);
	}
	return static_cast<float>(FMath::Sqrt(MaxDistSquared));
}

void ULNPHitIdentitySubsystem::RegisterRuntimeSource(UPrimitiveComponent* Component, const FLNPPlacementId& Placement, const float MaxRadiusOverride)
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

	// 서버 스폰 장치는 스폰 뒤 움직이지 않는다(D-045). 움직이는 패널은 등록 시점 자세가 아니라 경로 swept 반지름을 넘긴다.
	Entry.MaxRadius = MaxRadiusOverride > 0.f ? MaxRadiusOverride : ComputeSourceMaxRadius(*Component);
	Entry.Slot = Placement.Slot;
	Entry.MarkerId = Placement.MarkerId;
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

	const double StartSeconds = FPlatformTime::Seconds();
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
				Entry.MaxRadius = ComputeSourceMaxRadius(*Component);
				SlotSources.Add(Component, Entry);
			});
		}
	}

	float SlotEnvelopeRadius = 0.f;
	for (const TPair<TWeakObjectPtr<UPrimitiveComponent>, FLNPExactSourceEntry>& Pair : SlotSources)
	{
		SlotEnvelopeRadius = FMath::Max(SlotEnvelopeRadius, Pair.Value.MaxRadius);
	}

	UE_LOG(LogLootNPop, Log, TEXT("HitIdentity: Registered %d slot sources from %d slot levels (%d unclassified LNPWorldExact blockers). ")
		TEXT("Slot envelope radius %.0f cm, collected in %.1f ms."),
		SlotSources.Num(), RegisteredSlotLevels.Num(), UnclassifiedCount, SlotEnvelopeRadius, (FPlatformTime::Seconds() - StartSeconds) * 1000.0);
}

void ULNPHitIdentitySubsystem::Publish()
{
	TSharedRef<FLNPHitIdentitySnapshot, ESPMode::ThreadSafe> NewSnapshot = MakeShared<FLNPHitIdentitySnapshot, ESPMode::ThreadSafe>();
	NewSnapshot->Sources.Reserve(SlotSources.Num() + RuntimeSources.Num() + 1);
	NewSnapshot->Sources.Append(SlotSources);
	NewSnapshot->Sources.Append(RuntimeSources);
	for (const TPair<TWeakObjectPtr<UPrimitiveComponent>, FLNPExactSourceEntry>& Pair : NewSnapshot->Sources)
	{
		NewSnapshot->WorldEnvelopeRadius = FMath::Max(NewSnapshot->WorldEnvelopeRadius, Pair.Value.MaxRadius);
	}

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
	Identity.MarkerId = Entry->MarkerId;
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
