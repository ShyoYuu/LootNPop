// Copyright (c) 2026 LootNPop. All rights reserved.

#include "DynamicTerrain/LNPStatefulTerrain.h"
#include "DynamicTerrain/LNPPlacementMarker.h"
#include "DynamicTerrain/LNPDynamicTerrainSubsystem.h"
#include "SurfaceNavigation/LNPHitIdentityRegistry.h"
#include "SurfaceNavigation/LNPSurfaceDataSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "LootNPop.h"

DEFINE_LOG_CATEGORY_STATIC(LogLNPStatefulTerrain, Log, All);

namespace
{
	// 패키지·리슨 2P에서도 같은 서버 API와 상태를 수동으로 확인한다.
	FAutoConsoleCommandWithWorldAndArgs StatefulTerrainCommand(
		TEXT("LNP.SurfaceNav.StatefulTerrain"),
		TEXT("Inspect or change stateful terrain: <status|transition|break> [slot=0] [MarkerGuid]. Changes are server-only."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (!World || Args.IsEmpty()) { return; }
			const int32 Slot = Args.Num() > 1 ? FCString::Atoi(*Args[1]) : 0;
			FGuid Marker;
			if (Args.Num() > 2 && !FGuid::Parse(Args[2], Marker)) { return; }
			if (Args[0] != TEXT("status") && Args[0] != TEXT("transition") && Args[0] != TEXT("break")) { return; }
			const auto* Surfaces = World->GetSubsystem<ULNPSurfaceDataSubsystem>();
			const auto Snapshot = Surfaces ? Surfaces->TakeSnapshot() : nullptr;
			for (TActorIterator<ALNPStatefulTerrain> It(World); It; ++It)
			{
				ALNPStatefulTerrain& Terrain = **It;
				const FLNPPlacementId& Id = Terrain.GetPlacementId();
				if (Id.Slot != Slot || (Marker.IsValid() && Id.MarkerId != Marker)) { continue; }
				bool bChanged = false;
				if (Args[0] == TEXT("transition")) { bChanged = Terrain.StartTransition(); }
				else if (Args[0] == TEXT("break")) { bChanged = Terrain.BreakTerrain(); }
				TInlineComponentArray<UStaticMeshComponent*> Components(&Terrain);
				int32 CollisionCount = 0;
				for (const UStaticMeshComponent* Component : Components)
				{
					CollisionCount += Component->GetCollisionEnabled() != ECollisionEnabled::NoCollision;
				}
				UE_LOG(LogLootNPop, Display,
					TEXT("[StatefulTerrain] world=%s net=%d id=%s action=%s changed=%d state=%d revision=%u collision=%d patch=%d position=%s"),
					*World->GetName(), static_cast<int32>(World->GetNetMode()), *Id.ToString(), *Args[0], bChanged,
					static_cast<int32>(Terrain.GetTerrainState()), Terrain.GetStateRevision(), CollisionCount,
					Snapshot.IsValid() && Snapshot->ActivePatches.Contains(Id), *Terrain.GetActorLocation().ToString());
			}
		}));
}

ALNPStatefulTerrain::ALNPStatefulTerrain()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(false);
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("TerrainRoot")));
}

void ALNPStatefulTerrain::InitializeFromMarker(const ALNPPlacementMarker& Marker, const FLNPPlacementId& Id)
{
	Config.Id = Id;
	Config.Origin = Marker.GetActorTransform();
	Config.Standing = Marker.StandingTransform;
	Config.TransitionSeconds = Marker.TransitionSeconds;
	Config.Condition = Marker.PatchCondition;
	Config.Sources = Marker.PatchSources;
	ChangeState(Config.Condition == ELNPConditionalPatchCondition::Bridge ? ELNPTerrainState::Standing : ELNPTerrainState::Intact, ServerTime());
}

double ALNPStatefulTerrain::ServerTime() const
{
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	return GameState ? GameState->GetServerWorldTimeSeconds()
		: HasAuthority() ? GetWorld()->GetTimeSeconds() : State.StartServerTime;
}

void ALNPStatefulTerrain::ChangeState(ELNPTerrainState Value, double Time)
{
	State.Value = Value;
	if (++State.Revision == 0) { State.Revision = 1; }
	State.StartServerTime = Time;
	bPatchQueued = false;
	ForceNetUpdate();
}

bool ALNPStatefulTerrain::StartTransition()
{
	if (!HasAuthority() || State.Revision == 0 || State.Value != ELNPTerrainState::Standing) { return false; }
	ChangeState(ELNPTerrainState::Transitioning, ServerTime());
	return true;
}

bool ALNPStatefulTerrain::BreakTerrain()
{
	if (!HasAuthority() || State.Revision == 0 || State.Value == ELNPTerrainState::Destroyed) { return false; }
	ChangeState(ELNPTerrainState::Destroyed, ServerTime());
	return true;
}

FTransform ALNPStatefulTerrain::EvaluatePose(const FLNPStatefulTerrainConfig& Data,
	const FLNPStatefulTerrainState& Status, double Time)
{
	FTransform Local = FTransform::Identity;
	if (Status.Value == ELNPTerrainState::Standing) { Local = Data.Standing; }
	else if (Status.Value == ELNPTerrainState::Transitioning)
	{
		const double Alpha = FMath::Clamp((Time - Status.StartServerTime) / Data.TransitionSeconds, 0.0, 1.0);
		Local = FTransform(FQuat::Slerp(Data.Standing.GetRotation(), FQuat::Identity, Alpha),
			FMath::Lerp(Data.Standing.GetTranslation(), FVector::ZeroVector, Alpha));
	}
	return Local * Data.Origin;
}

void ALNPStatefulTerrain::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ALNPStatefulTerrain, Config);
	DOREPLIFETIME(ALNPStatefulTerrain, State);
}

void ALNPStatefulTerrain::BeginPlay()
{
	Super::BeginPlay();
	Apply(ServerTime());
}

void ALNPStatefulTerrain::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	LastTickFrame = GFrameCounter;
	const double Time = ServerTime();
	if (HasAuthority() && State.Value == ELNPTerrainState::Transitioning && Time >= State.StartServerTime + Config.TransitionSeconds)
	{
		ChangeState(ELNPTerrainState::Bridge, Time);
	}
	Apply(Time);
}

void ALNPStatefulTerrain::Apply(double Time)
{
	if (State.Revision == 0 || !Config.Id.IsValid() || Config.Sources.IsEmpty()) { return; }
	ULNPDynamicTerrainSubsystem* Dynamic = GetWorld()->GetSubsystem<ULNPDynamicTerrainSubsystem>();
	ULNPHitIdentitySubsystem* Identity = GetWorld()->GetSubsystem<ULNPHitIdentitySubsystem>();
	ULNPSurfaceDataSubsystem* Surfaces = GetWorld()->GetSubsystem<ULNPSurfaceDataSubsystem>();
	const bool bFirstPose = !bRegistered;
	if (!bRegistered)
	{
		for (int32 Index = 0; Index < Config.Sources.Num(); ++Index)
		{
			const FLNPConditionalPatchSource& Source = Config.Sources[Index];
			UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(this, *FString::Printf(TEXT("TerrainSource_%d"), Index));
			Mesh->SetNetAddressable();
			Mesh->SetMobility(EComponentMobility::Movable);
			Mesh->SetupAttachment(GetRootComponent());
			Mesh->SetRelativeTransform(Source.Transform);
			Mesh->SetStaticMesh(Source.Mesh);
			Mesh->SetSimulatePhysics(false);
			AddInstanceComponent(Mesh);
			Mesh->RegisterComponent();
			Meshes.Add(Mesh);
		}
		if (Dynamic) { Dynamic->RegisterStateful(this); }
		bRegistered = true;
	}
	PreviousTransform = GetActorTransform();
	const FTransform Pose = EvaluatePose(Config, State, Time);
	SetActorTransform(Pose, false, nullptr, ETeleportType::TeleportPhysics);
	if (bFirstPose) { PreviousTransform = Pose; }
	LinearVelocity = AngularVelocity = FVector::ZeroVector;
	if (State.Value == ELNPTerrainState::Transitioning && Time > State.StartServerTime && Time < State.StartServerTime + Config.TransitionSeconds)
	{
		LinearVelocity = Config.Origin.TransformVector(-Config.Standing.GetTranslation()) / Config.TransitionSeconds;
		FQuat Delta = Config.Standing.GetRotation().Inverse();
		if (Delta.W < 0.0) { Delta = Delta * -1.0; }
		FVector Axis; double Angle;
		Delta.ToAxisAndAngle(Axis, Angle);
		AngularVelocity = Config.Origin.GetRotation().RotateVector(Axis) * (Angle / Config.TransitionSeconds);
	}
	const bool bStable = State.Value == ELNPTerrainState::Bridge || State.Value == ELNPTerrainState::Intact;
	const auto Snapshot = Surfaces ? Surfaces->TakeSnapshot() : nullptr;
	const uint64 Generation = Snapshot.IsValid() ? Snapshot->Generation : 0;
	const bool bChanged = AppliedRevision != State.Revision || BoundGeneration != Generation;
	if (bChanged)
	{
		const FLNPConditionalPatch* Patch = nullptr;
		if (Snapshot.IsValid() && Snapshot->Slots.IsValidIndex(Config.Id.Slot))
		{
			const auto& Data = Snapshot->Slots[Config.Id.Slot].ConditionalPatches;
			if (Data.IsValid()) { Patch = Data->Patches.FindByPredicate([&](const auto& P) { return P.MarkerId == Config.Id.MarkerId; }); }
		}
		for (int32 Index = 0; Index < Meshes.Num(); ++Index)
		{
			UStaticMeshComponent* Mesh = Meshes[Index];
			if (Identity) { Identity->UnregisterRuntimeSource(Mesh); }
			Mesh->SetVisibility(State.Value != ELNPTerrainState::Destroyed);
			Mesh->SetCollisionProfileName(Config.Condition == ELNPConditionalPatchCondition::Intact
				? (Config.Sources[Index].bSupport ? TEXT("LNPDestructibleTerrain") : TEXT("LNPDestructibleBlocker"))
				: (Config.Sources[Index].bSupport ? TEXT("LNPDynamicTerrain") : TEXT("LNPStatefulTraversal")));
			if (State.Value == ELNPTerrainState::Destroyed) { Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); continue; }
			if (Identity)
			{
				// 회전·평행이동의 전 구간을 원점 기준 구로 보수적으로 둘러싼다.
				const FBoxSphereBounds Bounds = Mesh->CalcBounds(FTransform(Pose.GetRotation(), FVector::ZeroVector,
					Config.Sources[Index].Transform.GetScale3D() * Pose.GetScale3D()));
				const float Radius = Config.Origin.GetLocation().Size() + Config.Standing.GetTranslation().Size() * Config.Origin.GetScale3D().GetAbsMax()
					+ Bounds.Origin.Size() + Bounds.SphereRadius
					+ Mesh->GetRelativeLocation().Size() * Pose.GetScale3D().GetAbsMax();
				Identity->RegisterRuntimeSource(Mesh, Config.Id, Radius);
				if (bStable && Patch && Config.Sources[Index].bSupport)
				{
					const auto* Map = Patch->FaceMaps.FindByPredicate([&](const auto& M) { return M.SourceIndex == Index; });
					if (Map) { Identity->BindRuntimePatchFaceMap(Mesh, MakeShared<FLNPSupportFaceMap, ESPMode::ThreadSafe>(Map->FaceMap), Generation); }
				}
			}
		}
		AppliedRevision = State.Revision;
		BoundGeneration = Generation;
		UE_LOG(LogLNPStatefulTerrain, Verbose,
			TEXT("Applied world=%s net=%d id=%s state=%d revision=%u collision=%d serverTime=%.3f stateAge=%.3f frame=%llu"),
			*GetWorld()->GetName(), static_cast<int32>(GetNetMode()), *Config.Id.ToString(),
			static_cast<int32>(State.Value), State.Revision, State.Value != ELNPTerrainState::Destroyed,
			Time, Time - State.StartServerTime, GFrameCounter);
	}
	for (UStaticMeshComponent* Mesh : Meshes)
	{
		const FVector Velocity = LinearVelocity + FVector::CrossProduct(AngularVelocity, Mesh->GetComponentLocation() - Pose.GetLocation());
		Mesh->ComponentVelocity = Velocity;
		Mesh->SetPhysicsLinearVelocity(Velocity);
		Mesh->SetPhysicsAngularVelocityInRadians(AngularVelocity);
	}
	if (HasAuthority() && Surfaces && (!bPatchQueued || bChanged)) { bPatchQueued = Surfaces->SetPatchActive(Config.Id, bStable); }
}

bool ALNPStatefulTerrain::GetDynamicSupport(FLNPDynamicSupportSnapshot& Out) const
{
	if (!bRegistered || State.Value == ELNPTerrainState::Destroyed) { return false; }
	Out.Id = Config.Id;
	Out.PreviousTransform = PreviousTransform;
	Out.CurrentTransform = GetActorTransform();
	Out.LinearVelocity = LinearVelocity;
	Out.AngularVelocity = AngularVelocity;
	Out.bWalkable = Config.Sources.ContainsByPredicate([](const auto& Source) { return Source.bSupport; });
	return true;
}

void ALNPStatefulTerrain::EndPlay(EEndPlayReason::Type Reason)
{
	if (bRegistered)
	{
		if (auto* Dynamic = GetWorld()->GetSubsystem<ULNPDynamicTerrainSubsystem>()) { Dynamic->UnregisterStateful(this); }
		if (auto* Identity = GetWorld()->GetSubsystem<ULNPHitIdentitySubsystem>())
		{
			for (UStaticMeshComponent* Mesh : Meshes) { Identity->UnregisterRuntimeSource(Mesh); }
		}
		if (HasAuthority())
		{
			if (auto* Surfaces = GetWorld()->GetSubsystem<ULNPSurfaceDataSubsystem>(); Surfaces && Surfaces->IsReady()) { Surfaces->SetPatchActive(Config.Id, false); }
		}
	}
	Super::EndPlay(Reason);
}
