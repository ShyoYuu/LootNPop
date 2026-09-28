// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Config/LNPSettings.h"
#include "SurfaceNavigation/LNPCollisionChannels.h"
#include "SurfaceNavigation/LNPHitIdentityRegistry.h"
#include "SurfaceNavigation/LNPMassWorldCollision.h"
#include "DynamicTerrain/LNPMovingPanel.h"
#include "GameLogic/LNPOctantSpawnSubsystem.h"
#include "LootPod/LNPLootPodCollisionProxy.h"
#include "LootPod/LNPLootPodMassTypes.h"
#include "LootNPop.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "EngineUtils.h"
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

		if (const ULNPHitIdentitySubsystem* HitIdentity = World.GetSubsystem<ULNPHitIdentitySubsystem>())
		{
			static const TCHAR* LifetimeNames[] = { TEXT("Unknown"), TEXT("Static"), TEXT("Dynamic"), TEXT("Destructible") };
			const FLNPExactHitIdentity Identity = HitIdentity->ResolveHit(Hit);
			UE_LOG(LogLootNPop, Display,
				TEXT("[HitIdentity] Registry Lifetime=%s Roles=%s%s Slot=%d Face=%d Instance=%d Entity=%s Generation=%u UnknownHits=%u"),
				LifetimeNames[static_cast<uint8>(Identity.Lifetime)],
				(Identity.Roles & ELNPExactSourceRole::Support) ? TEXT("S") : TEXT("-"),
				(Identity.Roles & ELNPExactSourceRole::Blocker) ? TEXT("B") : TEXT("-"),
				Identity.Slot, Identity.FaceIndex, Identity.InstanceIndex, *Identity.Entity.DebugGetDescription(),
				Identity.RegistryGeneration, HitIdentity->GetUnknownHitCount());
		}

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

	/**
	 * LNP.SurfaceNav.ProbePanels
	 * 활성 움직이는 패널마다 패널 법선을 따라 중심을 관통하는 MassWorldCollision raycast를 쏘고,
	 * hit가 Dynamic·패널의 (slot, MarkerId)로 해석되는지 검사한다. 서버와 클라이언트가 각자 실행한다.
	 */
	FAutoConsoleCommandWithWorld GLNPProbePanels(
		TEXT("LNP.SurfaceNav.ProbePanels"),
		TEXT("Raycast through the center of every moving panel with MassWorldCollision and check the hit resolves to ")
		TEXT("Dynamic with the panel's (slot, MarkerId). Logs PASS/FAIL per world."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			const ULNPMassWorldCollisionSubsystem* Collision = World ? World->GetSubsystem<ULNPMassWorldCollisionSubsystem>() : nullptr;
			if (Collision == nullptr)
			{
				return;
			}

			int32 PanelCount = 0;
			int32 Failures = 0;
			const FLNPWorldQueryParams Params(ELNPWorldQueryClass::DebugValidation);
			for (TActorIterator<ALNPMovingPanel> It(World); It; ++It)
			{
				const ALNPMovingPanel* Panel = *It;
				const UStaticMeshComponent* Mesh = Panel->GetPanelMesh();
				if (Mesh == nullptr)
					continue;

				// 판 두께 30cm. 법선 방향 ±100cm 선분은 패널 밖의 다른 geometry에 닿기 어렵다.
				++PanelCount;
				const FVector Center = Mesh->Bounds.Origin;
				const FVector Normal = Mesh->GetUpVector();
				FLNPWorldHit Hit;
				const bool bHit = Collision->RaycastWorld(Center + Normal * 100.f, Center - Normal * 100.f, Params, Hit);

				const FLNPPlacementId& Expected = Panel->GetPlacementId();
				const bool bPass = bHit
					&& Hit.Identity.Lifetime == ELNPExactSourceLifetime::Dynamic
					&& Hit.Identity.Slot == Expected.Slot
					&& Hit.Identity.MarkerId == Expected.MarkerId;
				if (!bPass)
				{
					++Failures;
					UE_LOG(LogLootNPop, Warning, TEXT("[ProbePanels] FAIL panel %s: hit=%d lifetime=%d slot=%d marker=%s generation=%u"),
						*Expected.ToString(), bHit, static_cast<int32>(Hit.Identity.Lifetime), Hit.Identity.Slot,
						*Hit.Identity.MarkerId.ToString(EGuidFormats::Short), Hit.Identity.RegistryGeneration);
				}
			}

			UE_LOG(LogLootNPop, Display, TEXT("[ProbePanels] %s NetMode=%d panels=%d failures=%d -> %s"),
				*World->GetName(), static_cast<int32>(World->GetNetMode()), PanelCount, Failures,
				(PanelCount > 0 && Failures == 0) ? TEXT("PASS") : TEXT("FAIL"));
		}));

	/**
	 * LNP.SurfaceNav.ProbeFaceIndex [Count]
	 * 월드 중심에서 고르게 퍼진 방향으로 LNPSurfaceSupport trace를 쏴 slot Level의 hit FaceIndex가 유효한지 센다.
	 * 베이크 Support source는 모두 complex-as-simple trimesh라서 face→Layer 표(D-037)가 쓰려면 모든 hit에 FaceIndex가 있어야 한다.
	 * slot 밖의 런타임 source(스프링 런처 등)는 simple collision일 수 있고 베이크 face 표 대상이 아니므로 건너뛴다.
	 * cooked 패키지에서 external face 표가 살아 있는지 확인하는 용도다. 시점과 무관해 헤드리스 호스트에서도 돈다.
	 */
	FAutoConsoleCommandWithWorldAndArgs GLNPProbeFaceIndex(
		TEXT("LNP.SurfaceNav.ProbeFaceIndex"),
		TEXT("Trace LNPSurfaceSupport outward from the world center along evenly spread directions and check every hit ")
		TEXT("returns a valid FaceIndex. Args: [Count] (default 2000). Logs PASS/FAIL per world."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (World == nullptr)
			{
				return;
			}

			const int32 Count = Args.Num() > 0 ? FMath::Max(1, FCString::Atoi(*Args[0])) : 2000;
			const double TraceLength = GetDefault<ULNPSettings>()->SphereRadius * 1.5;
			const ULNPOctantSpawnSubsystem* OctantSubsystem = World->GetSubsystem<ULNPOctantSpawnSubsystem>();
			FCollisionQueryParams Params(SCENE_QUERY_STAT(LNPProbeFaceIndex), /*bTraceComplex=*/false);
			Params.bReturnFaceIndex = true;

			int32 HitCount = 0;
			int32 NonSlotHitCount = 0;
			int32 MissingFaceCount = 0;
			for (int32 Index = 0; Index < Count; ++Index)
			{
				// Fibonacci 구면 분포.
				const double Z = 1.0 - 2.0 * (Index + 0.5) / Count;
				const double Ring = FMath::Sqrt(FMath::Max(0.0, 1.0 - Z * Z));
				const double Phi = Index * UE_PI * (3.0 - FMath::Sqrt(5.0));
				const FVector Direction(Ring * FMath::Cos(Phi), Ring * FMath::Sin(Phi), Z);

				FHitResult Hit;
				if (!World->LineTraceSingleByChannel(Hit, FVector::ZeroVector, Direction * TraceLength, LNPCollisionChannels::SurfaceSupport, Params))
				{
					continue;
				}
				const UPrimitiveComponent* Component = Hit.GetComponent();
				if (OctantSubsystem == nullptr || OctantSubsystem->FindSlotForLevel(Component ? Component->GetComponentLevel() : nullptr) == INDEX_NONE)
				{
					++NonSlotHitCount;
					continue;
				}
				++HitCount;
				if (Hit.FaceIndex < 0)
				{
					if (MissingFaceCount == 0)
					{
						UE_LOG(LogLootNPop, Warning, TEXT("[ProbeFaceIndex] First hit without FaceIndex: %s at %s"),
							*GetNameSafe(Hit.GetComponent()), *Hit.ImpactPoint.ToCompactString());
					}
					++MissingFaceCount;
				}
			}

			UE_LOG(LogLootNPop, Display, TEXT("[ProbeFaceIndex] %s NetMode=%d directions=%d slotHits=%d nonSlotHits=%d missingFaceIndex=%d -> %s"),
				*World->GetName(), static_cast<int32>(World->GetNetMode()), Count, HitCount, NonSlotHitCount, MissingFaceCount,
				(HitCount > 0 && MissingFaceCount == 0) ? TEXT("PASS") : TEXT("FAIL"));
		}));

	/**
	 * LNP.SurfaceNav.ProbeSourceKeys
	 * runtime slot Level Instance의 Support component key를 모아 slot 사이에서 같은 이름이 유지되는지 검사한다.
	 * 베이크 key와 같은 `<Actor FName>.<Component FName>` 형식을 출력하므로 에디터 베이크 결과와 직접 대조할 수 있다.
	 */
	FAutoConsoleCommandWithWorld GLNPProbeSourceKeys(
		TEXT("LNP.SurfaceNav.ProbeSourceKeys"),
		TEXT("List <Actor FName>.<Component FName> keys for Support components in runtime octant slots and verify "
			"that every key occurs once per loaded slot. Logs PASS/FAIL per world."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			const ULNPOctantSpawnSubsystem* OctantSubsystem = World
				? World->GetSubsystem<ULNPOctantSpawnSubsystem>()
				: nullptr;
			if (OctantSubsystem == nullptr || !OctantSubsystem->bGenerationComplete)
			{
				UE_LOG(LogLootNPop, Warning, TEXT("[ProbeSourceKeys] World generation is not complete."));
				return;
			}

			const FName SupportTag(TEXT("LNP.Surface.Support"));
			TMap<FString, int32> Occurrences;
			int32 LoadedSlotCount = 0;
			int32 DuplicateCount = 0;
			int32 ReferenceSourceCount = INDEX_NONE;
			bool bSameCountPerSlot = true;

			for (int32 SlotIndex = 0; SlotIndex < 8; ++SlotIndex)
			{
				const ULevel* Level = OctantSubsystem->GetSlotLevel(SlotIndex);
				if (Level == nullptr)
				{
					continue;
				}
				++LoadedSlotCount;

				TSet<FString> SlotKeys;
				for (const AActor* Actor : Level->Actors)
				{
					if (!IsValid(Actor))
					{
						continue;
					}

					TInlineComponentArray<UPrimitiveComponent*> Components(Actor);
					for (const UPrimitiveComponent* Component : Components)
					{
						if (!IsValid(Component) || !Component->ComponentHasTag(SupportTag))
						{
							continue;
						}

						const FString Key = FString::Printf(TEXT("%s.%s"),
							*Actor->GetFName().ToString(), *Component->GetFName().ToString());
						if (SlotKeys.Contains(Key))
						{
							++DuplicateCount;
						}
						SlotKeys.Add(Key);
						++Occurrences.FindOrAdd(Key);
					}
				}

				if (ReferenceSourceCount == INDEX_NONE)
				{
					ReferenceSourceCount = SlotKeys.Num();
				}
				else if (ReferenceSourceCount != SlotKeys.Num())
				{
					bSameCountPerSlot = false;
				}

				UE_LOG(LogLootNPop, Display, TEXT("[ProbeSourceKeys] Slot=%d Sources=%d Level=%s"),
					SlotIndex, SlotKeys.Num(), *FPackageName::GetShortName(Level->GetOutermost()->GetName()));
			}

			TArray<FString> SortedKeys;
			Occurrences.GenerateKeyArray(SortedKeys);
			SortedKeys.Sort();
			int32 InconsistentKeyCount = 0;
			for (const FString& Key : SortedKeys)
			{
				const int32 Count = Occurrences.FindChecked(Key);
				InconsistentKeyCount += Count == LoadedSlotCount ? 0 : 1;
				UE_LOG(LogLootNPop, Display, TEXT("[ProbeSourceKeys] Key=%s Slots=%d/%d"),
					*Key, Count, LoadedSlotCount);
			}

			const bool bPass = LoadedSlotCount == 8
				&& ReferenceSourceCount > 0
				&& bSameCountPerSlot
				&& DuplicateCount == 0
				&& InconsistentKeyCount == 0;
			UE_LOG(LogLootNPop, Display,
				TEXT("[ProbeSourceKeys] %s NetMode=%d loadedSlots=%d uniqueKeys=%d sourcesPerSlot=%d duplicates=%d inconsistentKeys=%d -> %s"),
				*World->GetName(), static_cast<int32>(World->GetNetMode()), LoadedSlotCount, SortedKeys.Num(),
				ReferenceSourceCount, DuplicateCount, InconsistentKeyCount, bPass ? TEXT("PASS") : TEXT("FAIL"));
		}));
}

#endif // !UE_BUILD_SHIPPING
