// Copyright (c) 2026 LootNPop. All rights reserved.

#include "DynamicTerrain/LNPDynamicTerrainSubsystem.h"
#include "DynamicTerrain/LNPMovingPanel.h"
#include "DynamicTerrain/LNPPlacementMarker.h"
#include "GameLogic/LNPOctantSpawnSubsystem.h"
#include "LootNPop.h"

#include "Engine/Level.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "DefaultMovementSet/InstantMovementEffects/BasicInstantMovementEffects.h"
#include "GameFramework/PlayerController.h"
#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "MoverComponent.h"

namespace
{
	/** 옥탄트 slot 수. ULNPOctantSpawnSubsystem의 고정 8 slot과 같다. */
	constexpr int32 OctantSlotCount = 8;

	int32 GLogRidersInterval = 0;
	FAutoConsoleVariableRef CVarLogRiders(
		TEXT("LNP.DynamicTerrain.LogRiders"),
		GLogRidersInterval,
		TEXT("Log Mover pawns near moving panels every N frames at frame start (0 = off): movement base, panel-local position, velocities."));

	int32 GLogDepartures = 0;
	FAutoConsoleVariableRef CVarLogDepartures(
		TEXT("LNP.DynamicTerrain.LogDepartures"),
		GLogDepartures,
		TEXT("Log inertia when a Mover pawn leaves a moving panel (1 = on): tangential pawn vs panel velocity at departure and 0.3s later. ")
		TEXT("2 = also make server player pawns jump after 0.5s on a panel moving tangentially faster than 100cm/s."));

	/** 관성 검증용. 입력 경로를 거치지 않고 실제 점프와 같은 Jump()(FJumpImpulseEffect)를 부른다. */
	FAutoConsoleCommandWithWorldAndArgs GLNPRiderJump(
		TEXT("LNP.DynamicTerrain.RiderJump"),
		TEXT("Server-only: make a player's pawn jump through UCharacterMoverComponent::Jump. Args: [PlayerIndex]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (World == nullptr || World->GetNetMode() == NM_Client)
				return;

			TArray<APawn*> Pawns;
			for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
			{
				if (APawn* Pawn = It->Get() ? It->Get()->GetPawn() : nullptr)
					Pawns.Add(Pawn);
			}
			const int32 PlayerIndex = Args.Num() >= 1 ? FCString::Atoi(*Args[0]) : 0;
			UCharacterMoverComponent* Mover = Pawns.IsValidIndex(PlayerIndex) ? Pawns[PlayerIndex]->FindComponentByClass<UCharacterMoverComponent>() : nullptr;
			const bool bJumped = Mover != nullptr && Mover->Jump();
			UE_LOG(LogLootNPop, Log, TEXT("[DynamicTerrain] RiderJump: player %d jumped=%d"), PlayerIndex, bJumped ? 1 : 0);
		}));

	/** 탑승 검증용. Mover 텔레포트 효과로 폰을 가장 가까운 패널 위에 올린다(SetActorLocation은 sync state가 되돌린다). */
	FAutoConsoleCommandWithWorldAndArgs GLNPPlaceRider(
		TEXT("LNP.DynamicTerrain.PlaceRider"),
		TEXT("Server-only: teleport a player's pawn 120cm above the nearest moving panel via Mover. Args: [PlayerIndex]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (World == nullptr || World->GetNetMode() == NM_Client)
				return;

			TArray<APawn*> Pawns;
			for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
			{
				if (APawn* Pawn = It->Get() ? It->Get()->GetPawn() : nullptr)
					Pawns.Add(Pawn);
			}
			const int32 PlayerIndex = Args.Num() >= 1 ? FCString::Atoi(*Args[0]) : 0;
			UMoverComponent* Mover = Pawns.IsValidIndex(PlayerIndex) ? Pawns[PlayerIndex]->FindComponentByClass<UMoverComponent>() : nullptr;
			if (Mover == nullptr)
			{
				UE_LOG(LogLootNPop, Warning, TEXT("[DynamicTerrain] PlaceRider: no Mover pawn at index %d"), PlayerIndex);
				return;
			}

			const ALNPMovingPanel* Nearest = nullptr;
			for (TActorIterator<ALNPMovingPanel> It(World); It; ++It)
			{
				if (Nearest == nullptr || FVector::DistSquared(It->GetActorLocation(), Pawns[PlayerIndex]->GetActorLocation())
					< FVector::DistSquared(Nearest->GetActorLocation(), Pawns[PlayerIndex]->GetActorLocation()))
				{
					Nearest = *It;
				}
			}
			if (Nearest == nullptr)
				return;

			TSharedPtr<FTeleportEffect> Teleport = MakeShared<FTeleportEffect>();
			Teleport->TargetLocation = Nearest->GetActorLocation() + Nearest->GetActorUpVector() * 120.0;
			Teleport->bUseActorRotation = true;
			Mover->QueueInstantMovementEffect(Teleport);
			UE_LOG(LogLootNPop, Log, TEXT("[DynamicTerrain] PlaceRider: player %d -> panel %s"), PlayerIndex, *Nearest->GetPlacementId().ToString());
		}));
}

bool ULNPDynamicTerrainSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void ULNPDynamicTerrainSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	PreActorTickHandle = FWorldDelegates::OnWorldPreActorTick.AddUObject(this, &ULNPDynamicTerrainSubsystem::OnWorldPreActorTick);
}

void ULNPDynamicTerrainSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	PublishTick.Owner = this;
	PublishTick.bCanEverTick = true;
	PublishTick.bStartWithTickEnabled = true;
	PublishTick.TickGroup = TG_PrePhysics;
	PublishTick.RegisterTickFunction(InWorld.PersistentLevel);
}

void ULNPDynamicTerrainSubsystem::Deinitialize()
{
	if (PublishTick.IsTickFunctionRegistered())
	{
		PublishTick.UnRegisterTickFunction();
	}
	FWorldDelegates::OnWorldPreActorTick.Remove(PreActorTickHandle);
	Panels.Reset();
	Frame = MakeShared<FLNPDynamicSupportFrame, ESPMode::ThreadSafe>();

	Super::Deinitialize();
}

AActor* ULNPDynamicTerrainSubsystem::SpawnPlacedActor(UWorld& World, UClass* ActorClass, const FTransform& Transform,
	const TFunctionRef<void(AActor&)> Initialize)
{
	check(World.GetNetMode() != NM_Client);

	AActor* Actor = World.SpawnActorDeferred<AActor>(ActorClass, Transform, nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Actor == nullptr)
		return nullptr;

	Initialize(*Actor);
	Actor->FinishSpawning(Transform);
	return Actor;
}

void ULNPDynamicTerrainSubsystem::SpawnFromMarkers()
{
	UWorld* World = GetWorld();
	if (World == nullptr || World->GetNetMode() == NM_Client)
		return;

	// (slot, level). persistent level은 LVI가 아닌 맵(회귀 맵 등)의 마커다.
	TArray<TPair<int32, ULevel*>> Levels;
	Levels.Emplace(INDEX_NONE, World->PersistentLevel);
	if (const ULNPOctantSpawnSubsystem* OctantSubsystem = World->GetSubsystem<ULNPOctantSpawnSubsystem>())
	{
		for (int32 Slot = 0; Slot < OctantSlotCount; ++Slot)
		{
			if (ULevel* Level = OctantSubsystem->GetSlotLevel(Slot))
			{
				Levels.Emplace(Slot, Level);
			}
		}
	}

	const double StartSeconds = FPlatformTime::Seconds();
	TSet<FLNPPlacementId> Seen;
	int32 NumSpawned = 0;
	int32 NumSkipped = 0;
	for (const TPair<int32, ULevel*>& SlotLevel : Levels)
	{
		for (AActor* Actor : SlotLevel.Value->Actors)
		{
			const ALNPPlacementMarker* Marker = Cast<ALNPPlacementMarker>(Actor);
			if (Marker == nullptr)
				continue;

			FLNPPlacementId Id;
			Id.Slot = static_cast<int8>(SlotLevel.Key);
			Id.MarkerId = Marker->MarkerId;

			if (!Id.IsValid() || Marker->ElementClass == nullptr || Seen.Contains(Id))
			{
				UE_LOG(LogLootNPop, Warning, TEXT("[DynamicTerrain] Marker %s %s skipped: %s"),
					*GetNameSafe(Marker), *Id.ToString(),
					!Id.IsValid() ? TEXT("no MarkerId") : Marker->ElementClass == nullptr ? TEXT("no ElementClass") : TEXT("duplicate MarkerId in slot"));
				++NumSkipped;
				continue;
			}
			Seen.Add(Id);

			AActor* Spawned = SpawnPlacedActor(*World, Marker->ElementClass, Marker->GetActorTransform(), [Marker, &Id](AActor& Element)
			{
				if (ILNPPlacedElement* Placed = Cast<ILNPPlacedElement>(&Element))
				{
					Placed->InitializeFromMarker(*Marker, Id);
				}
			});
			NumSpawned += Spawned != nullptr ? 1 : 0;
		}
	}

	UE_LOG(LogLootNPop, Log, TEXT("[DynamicTerrain] Spawned %d marker elements from %d levels (%d skipped) in %.1f ms"),
		NumSpawned, Levels.Num(), NumSkipped, (FPlatformTime::Seconds() - StartSeconds) * 1000.0);
}

void ULNPDynamicTerrainSubsystem::RegisterPanel(ALNPMovingPanel* Panel)
{
	check(IsInGameThread());
	Panels.AddUnique(Panel);
	PublishTick.AddPrerequisite(Panel, Panel->PrimaryActorTick);
}

void ULNPDynamicTerrainSubsystem::UnregisterPanel(ALNPMovingPanel* Panel)
{
	check(IsInGameThread());
	Panels.Remove(Panel);
	PublishTick.RemovePrerequisite(Panel, Panel->PrimaryActorTick);
}

void FLNPDynamicSupportPublishTickFunction::ExecuteTick(float DeltaTime, ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	if (Owner != nullptr)
	{
		Owner->Publish();
	}
}

void ULNPDynamicTerrainSubsystem::Publish()
{
	if (Panels.IsEmpty())
		return;

	TSharedRef<FLNPDynamicSupportFrame, ESPMode::ThreadSafe> NewFrame = MakeShared<FLNPDynamicSupportFrame, ESPMode::ThreadSafe>();
	NewFrame->FrameNumber = GFrameCounter;
	NewFrame->Supports.Reserve(Panels.Num());

	for (int32 Index = Panels.Num() - 1; Index >= 0; --Index)
	{
		const ALNPMovingPanel* Panel = Panels[Index].Get();
		if (Panel == nullptr)
		{
			Panels.RemoveAtSwap(Index);
			continue;
		}

		FLNPDynamicSupportSnapshot& Support = NewFrame->Supports.AddDefaulted_GetRef();
		Support.Id = Panel->GetPlacementId();
		Support.PreviousTransform = Panel->GetPreviousTransform();
		Support.CurrentTransform = Panel->GetActorTransform();
		Support.LinearVelocity = Panel->GetLinearVelocity();
		Support.bWalkable = true; // LNPDynamicTerrain = Support+Blocker
	}

	Frame = NewFrame;
}

void ULNPDynamicTerrainSubsystem::OnWorldPreActorTick(UWorld* InWorld, ELevelTick TickType, float DeltaSeconds)
{
	// 진단만 한다. 프레임 선두라 Mover·패널 모두 직전 프레임의 최종 상태다.
	if (InWorld != GetWorld() || Panels.IsEmpty())
		return;

	const AGameStateBase* GameState = InWorld->GetGameState();
	const double ServerTime = GameState ? GameState->GetServerWorldTimeSeconds() : InWorld->GetTimeSeconds();
	if (GLogDepartures > 0)
	{
		TrackDepartures(ServerTime);
	}
	if (GLogRidersInterval > 0 && GFrameCounter % GLogRidersInterval == 0)
	{
		LogRiders(ServerTime);
	}
}

void ULNPDynamicTerrainSubsystem::TrackDepartures(const double ServerTime)
{
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		const UMoverComponent* Mover = It->FindComponentByClass<UMoverComponent>();
		if (Mover == nullptr)
			continue;

		// 관성은 접평면 성분만 비교한다. 수직 성분은 점프 속도·중력이 섞인다.
		const FVector Up = Mover->GetUpDirection();
		const FVector PawnTangent = FVector::VectorPlaneProject(Mover->GetVelocity(), Up);

		const UPrimitiveComponent* Base = Mover->GetMovementBase();
		ALNPMovingPanel* BasePanel = Base ? Cast<ALNPMovingPanel>(Base->GetOwner()) : nullptr;
		FRiderTrack& Track = RiderTracks.FindOrAdd(*It);

		if (BasePanel != nullptr)
		{
			if (Track.OnBaseSince < 0.0 || Track.Panel != BasePanel)
			{
				Track.OnBaseSince = ServerTime;
			}
			Track.Panel = BasePanel;
			Track.PanelVelocity = BasePanel->GetLinearVelocity();
			Track.DepartedTime = -1.0;
			Track.bFollowUpLogged = true;

			// 관성 측정용 자동 점프. 실제 점프와 같은 Jump()(FJumpImpulseEffect) 경로다.
			if (GLogDepartures >= 2 && It->IsPlayerControlled() && GetWorld()->GetNetMode() != NM_Client
				&& ServerTime - Track.OnBaseSince >= 0.5
				&& FVector::VectorPlaneProject(Track.PanelVelocity, Up).Size() > 100.0)
			{
				if (UCharacterMoverComponent* CharacterMover = It->FindComponentByClass<UCharacterMoverComponent>())
				{
					CharacterMover->Jump();
					Track.OnBaseSince = ServerTime + 10.0; // 이탈이 기록될 때까지 다시 뛰지 않는다
				}
			}
			continue;
		}
		Track.OnBaseSince = -1.0;

		if (Track.Panel.IsValid() && Track.DepartedTime < 0.0)
		{
			// 이탈 첫 프레임. 직전 프레임에 기록한 패널 속도가 Mover가 이탈 시 읽은 값이다.
			const FVector PanelTangent = FVector::VectorPlaneProject(Track.PanelVelocity, Up);
			const double Carried = PanelTangent.SizeSquared() > 1.0 ? FVector::DotProduct(PawnTangent, PanelTangent) / PanelTangent.SizeSquared() : 0.0;
			UE_LOG(LogLootNPop, Log, TEXT("[DynamicTerrain] Depart %s net=%d t=%.3f panel=%s panelTan=%.1f pawnTan=%.1f carried=%.2f pawnVel=%s panelVel=%s"),
				*It->GetName(), static_cast<int32>(GetWorld()->GetNetMode()), ServerTime, *Track.Panel->GetPlacementId().ToString(),
				PanelTangent.Size(), PawnTangent.Size(), Carried, *Mover->GetVelocity().ToCompactString(), *Track.PanelVelocity.ToCompactString());
			Track.DepartedTime = ServerTime;
			Track.bFollowUpLogged = false;
		}
		else if (!Track.bFollowUpLogged && ServerTime - Track.DepartedTime >= 0.3)
		{
			const FVector PanelTangent = FVector::VectorPlaneProject(Track.PanelVelocity, Up);
			const double Carried = PanelTangent.SizeSquared() > 1.0 ? FVector::DotProduct(PawnTangent, PanelTangent) / PanelTangent.SizeSquared() : 0.0;
			UE_LOG(LogLootNPop, Log, TEXT("[DynamicTerrain] Depart+%.2fs %s net=%d pawnTan=%.1f carried=%.2f pawnVel=%s mode=%s"),
				ServerTime - Track.DepartedTime, *It->GetName(), static_cast<int32>(GetWorld()->GetNetMode()),
				PawnTangent.Size(), Carried, *Mover->GetVelocity().ToCompactString(), *Mover->GetMovementModeName().ToString());
			Track.bFollowUpLogged = true;
			Track.Panel.Reset();
		}
	}
}

void ULNPDynamicTerrainSubsystem::LogRiders(const double ServerTime) const
{
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		const UMoverComponent* Mover = It->FindComponentByClass<UMoverComponent>();
		if (Mover == nullptr)
			continue;

		const ALNPMovingPanel* Nearest = nullptr;
		double NearestDistSq = TNumericLimits<double>::Max();
		for (const TWeakObjectPtr<ALNPMovingPanel>& Panel : Panels)
		{
			if (Panel.IsValid())
			{
				const double DistSq = FVector::DistSquared(Panel->GetActorLocation(), It->GetActorLocation());
				if (DistSq < NearestDistSq)
				{
					NearestDistSq = DistSq;
					Nearest = Panel.Get();
				}
			}
		}
		if (Nearest == nullptr || NearestDistSq > FMath::Square(1000.0))
			continue;

		const FVector Local = Nearest->GetActorTransform().InverseTransformPositionNoScale(It->GetActorLocation());
		const UPrimitiveComponent* Base = Mover->GetMovementBase();
		const FVector BodyVelocity = Nearest->GetPanelMesh()->GetPhysicsLinearVelocityAtPoint(It->GetActorLocation());
		UE_LOG(LogLootNPop, Log, TEXT("[DynamicTerrain] Rider %s net=%d role=%d frame=%llu t=%.3f base=%s panel=%s local=(%.1f, %.1f, %.1f) moverVel=%s panelVel=%s bodyVel=%s"),
			*It->GetName(), static_cast<int32>(GetWorld()->GetNetMode()), static_cast<int32>(It->GetLocalRole()), GFrameCounter, ServerTime,
			*GetNameSafe(Base ? Base->GetOwner() : nullptr), *Nearest->GetPlacementId().ToString(),
			Local.X, Local.Y, Local.Z, *Mover->GetVelocity().ToCompactString(), *Nearest->GetLinearVelocity().ToCompactString(),
			*BodyVelocity.ToCompactString());
	}
}
