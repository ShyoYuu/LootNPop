// Copyright (c) 2026 LootNPop. All rights reserved.

#include "SurfaceNavigation/LNPCollisionChannels.h"
#include "GameLogic/LNPOctantSpawnSubsystem.h"
#include "LootPod/LNPLootPodCollisionProxy.h"
#include "LootPod/LNPLootPodMassTypes.h"
#include "LootNPop.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "MassEntitySubsystem.h"
#include "Misc/PackageName.h"

#if !UE_BUILD_SHIPPING

namespace
{
	/** Gate -1 B 스파이크: exact hit에서 hit identity(D-037) 후보 필드를 얻을 수 있는지 확인한다. */
	void LogHitIdentity(const FHitResult& Hit, const UWorld& World)
	{
		const UPrimitiveComponent* Component = Hit.GetComponent();
		const ULevel* Level = Component ? Component->GetComponentLevel() : nullptr;

		int32 Slot = INDEX_NONE;
		if (const ULNPOctantSpawnSubsystem* OctantSubsystem = World.GetSubsystem<ULNPOctantSpawnSubsystem>())
		{
			Slot = OctantSubsystem->FindSlotForLevel(Level);
		}

		const FString Source = (Level == nullptr || Level == World.PersistentLevel)
			? FString(TEXT("Persistent"))
			: FPackageName::GetShortName(Level->GetOutermost()->GetName());

		// 내부형 구: 지면의 위쪽은 중심 방향이다.
		const FVector Up = -Hit.ImpactPoint.GetSafeNormal();

		UE_LOG(LogLootNPop, Display,
			TEXT("[HitIdentity] Distance=%.1f Point=%s Normal=%s UpDot=%.3f"),
			Hit.Distance, *Hit.ImpactPoint.ToCompactString(), *Hit.ImpactNormal.ToCompactString(),
			FVector::DotProduct(Hit.ImpactNormal, Up));
		UE_LOG(LogLootNPop, Display,
			TEXT("[HitIdentity] Actor=%s Component=%s(%s) Profile=%s Source=%s Slot=%d"),
			*GetNameSafe(Hit.GetActor()), *GetNameSafe(Component),
			Component ? *Component->GetClass()->GetName() : TEXT("-"),
			Component ? *Component->GetCollisionProfileName().ToString() : TEXT("-"),
			*Source, Slot);
		UE_LOG(LogLootNPop, Display,
			TEXT("[HitIdentity] Item=%d FaceIndex=%d ElementIndex=%d MyItem=%d BoneName=%s"),
			Hit.Item, Hit.FaceIndex, static_cast<int32>(Hit.ElementIndex), Hit.MyItem, *Hit.BoneName.ToString());

		if (const UInstancedStaticMeshComponent* ISM = Cast<UInstancedStaticMeshComponent>(Component))
		{
			UE_LOG(LogLootNPop, Display, TEXT("[HitIdentity] ISM InstanceCount=%d"), ISM->GetInstanceCount());
		}

		const ULNPLootPodCollisionProxySubsystem* ProxySubsystem = World.GetSubsystem<ULNPLootPodCollisionProxySubsystem>();
		if (ProxySubsystem != nullptr && Component != nullptr && Component == ProxySubsystem->GetProxyComponent())
		{
			const FMassEntityHandle Entity = ProxySubsystem->ResolveInstance(Hit.Item);

			// PodID는 서버에만 발급된다. 클라이언트는 0이다.
			int32 PodID = 0;
			if (const UMassEntitySubsystem* EntitySubsystem = World.GetSubsystem<UMassEntitySubsystem>())
			{
				const FMassEntityManager& EntityManager = EntitySubsystem->GetEntityManager();
				if (EntityManager.IsEntityValid(Entity))
				{
					if (const FLNPLootPodFragment* PodFragment = EntityManager.GetFragmentDataPtr<FLNPLootPodFragment>(Entity))
					{
						PodID = PodFragment->PodID;
					}
				}
			}

			UE_LOG(LogLootNPop, Display, TEXT("[HitIdentity] LootPodProxy Entity=%s PodID=%d Generation=%u"),
				*Entity.DebugGetDescription(), PodID, ProxySubsystem->GetGeneration());
		}
	}

	/**
	 * LNP.SurfaceNav.ProbeHitIdentity [Distance]
	 * 로컬 플레이어 시점 방향으로 LNPWorldExact line trace를 쏘고 hit identity 후보 필드를 로그에 남긴다.
	 */
	FAutoConsoleCommandWithWorldAndArgs GLNPProbeHitIdentity(
		TEXT("LNP.SurfaceNav.ProbeHitIdentity"),
		TEXT("Line trace LNPWorldExact from the local player view and log hit identity fields ")
		TEXT("(component, level slot, Item, FaceIndex, LootPod proxy entity). Args: [Distance] (default 100000)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (World == nullptr)
			{
				return;
			}

			APlayerController* PlayerController = World->GetFirstPlayerController();
			if (PlayerController == nullptr)
			{
				UE_LOG(LogLootNPop, Warning, TEXT("[HitIdentity] No local player controller in %s."), *World->GetName());
				return;
			}

			const float Distance = Args.Num() > 0 ? FCString::Atof(*Args[0]) : 100000.f;

			FVector ViewLocation;
			FRotator ViewRotation;
			PlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);
			const FVector End = ViewLocation + ViewRotation.Vector() * Distance;

			FCollisionQueryParams Params(SCENE_QUERY_STAT(LNPProbeHitIdentity), /*bTraceComplex=*/false);
			Params.bReturnFaceIndex = true;
			Params.AddIgnoredActor(PlayerController->GetPawn());

			FHitResult Hit;
			const bool bHit = World->LineTraceSingleByChannel(Hit, ViewLocation, End, LNPCollisionChannels::WorldExact, Params);

			DrawDebugLine(World, ViewLocation, bHit ? Hit.ImpactPoint : End, bHit ? FColor::Green : FColor::Red, false, 10.f, 0, 2.f);
			if (!bHit)
			{
				UE_LOG(LogLootNPop, Display, TEXT("[HitIdentity] No LNPWorldExact hit within %.0f cm (NetMode=%d)."),
					Distance, static_cast<int32>(World->GetNetMode()));
				return;
			}

			DrawDebugPoint(World, Hit.ImpactPoint, 16.f, FColor::Yellow, false, 10.f);
			UE_LOG(LogLootNPop, Display, TEXT("[HitIdentity] --- %s (NetMode=%d) ---"),
				*World->GetName(), static_cast<int32>(World->GetNetMode()));
			LogHitIdentity(Hit, *World);
		}));
}

#endif // !UE_BUILD_SHIPPING
