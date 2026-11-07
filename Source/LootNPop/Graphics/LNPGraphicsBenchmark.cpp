// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Graphics/LNPGraphicsBenchmark.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Character/LNPPlayerCharacter.h"
#include "Enemy/LNPEnemyConfig.h"
#include "Enemy/LNPEnemyMassTypes.h"
#include "GameLogic/LNPMassSpawnSubsystem.h"
#include "GameLogic/LNPOctantSpawnSubsystem.h"
#include "GameMode/LNPGameState.h"
#include "Movement/LNPCharacterMoverComponent.h"
#include "SurfaceNavigation/LNPLoadBaseline.h"
#include "SurfaceNavigation/LNPSurfaceDataSubsystem.h"
#include "LootNPop.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Dom/JsonObject.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameUserSettings.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "MassCommonFragments.h"
#include "MassEntityConfigAsset.h"
#include "MassEntitySubsystem.h"
#include "MassExecutionContext.h"
#include "MassRepresentationFragments.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "RenderTimer.h"
#include "RHI.h"
#include "RHIStats.h"
#include "SceneView.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	TAutoConsoleVariable<int32> GGraphicsBenchmark(TEXT("LNP.Graphics.Benchmark"), 0,
		TEXT("Enable standalone graphics observation at world start. Forces seed 1, selected profile and density 1."), ECVF_Cheat);
	TAutoConsoleVariable<FString> GGraphicsProfile(TEXT("LNP.Graphics.Benchmark.Profile"), TEXT("Low"),
		TEXT("Spawn profile: Low, Medium or High. Set before world start."), ECVF_Cheat);
	TAutoConsoleVariable<float> GGraphicsWarmup(TEXT("LNP.Graphics.Benchmark.WarmupSeconds"), 60.f,
		TEXT("Warm-up seconds. Values below 60 are preview only."), ECVF_Cheat);
	TAutoConsoleVariable<float> GGraphicsDuration(TEXT("LNP.Graphics.Benchmark.DurationSeconds"), 60.f,
		TEXT("Camera loop and capture seconds. Values below 60 are preview only."), ECVF_Cheat);

	FString ProfileName()
	{
		FString Name = GGraphicsProfile.GetValueOnGameThread();
		FParse::Value(FCommandLine::Get(), TEXT("LNPGraphicsProfile="), Name);
		return Name;
	}

	bool ReadPose(const TSharedPtr<FJsonObject>& Object, FTransform& Out)
	{
		if (!Object.IsValid()) return false;
		const TArray<TSharedPtr<FJsonValue>>* P;
		const TArray<TSharedPtr<FJsonValue>>* Q;
		if (!Object->TryGetArrayField(TEXT("location"), P) || P->Num() != 3
			|| !Object->TryGetArrayField(TEXT("rotation_quaternion_xyzw"), Q) || Q->Num() != 4) return false;
		double Values[7];
		for (int32 I = 0; I < 7; ++I)
			if (!(I < 3 ? (*P)[I] : (*Q)[I - 3])->TryGetNumber(Values[I]) || !FMath::IsFinite(Values[I])) return false;
		FQuat Rotation(Values[3], Values[4], Values[5], Values[6]);
		if (!FMath::IsNearlyEqual(Rotation.SizeSquared(), 1.0, 1.e-4)) return false;
		Rotation.Normalize();
		Out = FTransform(Rotation, FVector(Values[0], Values[1], Values[2]));
		return true;
	}
}

bool LNPGraphicsBenchmark::IsRequested()
{
#if UE_BUILD_SHIPPING
	return false;
#else
	static const bool bCommandLine = FParse::Param(FCommandLine::Get(), TEXT("LNPGraphicsBenchmark"));
	return bCommandLine || GGraphicsBenchmark.GetValueOnAnyThread() != 0;
#endif
}

FString LNPGraphicsBenchmark::GetSpawnConfigPath()
{
	const FString Name = TEXT("DA_MassSpawnConfig_Graphics_") + ProfileName();
	return TEXT("/Game/SpawnData/Graphics/") + Name + TEXT(".") + Name;
}

bool LNPGraphicsBenchmark::GetObserverLocation(const UWorld* World, FVector& OutLocation)
{
	if (!IsRequested() || !World) return false;
	if (const APlayerController* PC = World->GetFirstPlayerController())
	{
		FRotator Rotation;
		PC->GetPlayerViewPoint(OutLocation, Rotation);
		return true;
	}
	return false;
}

bool ULNPGraphicsBenchmarkSubsystem::DoesSupportWorldType(EWorldType::Type WorldType) const
{
	return LNPGraphicsBenchmark::IsRequested() && (WorldType == EWorldType::Game || WorldType == EWorldType::PIE);
}

bool ULNPGraphicsBenchmarkSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	// 게임 시작 시 생성·폐기되는 임시 월드와 옥탄트 서브레벨에는 측정 수명주기를 만들지 않는다.
	return Super::ShouldCreateSubsystem(Outer) && CastChecked<UWorld>(Outer)->GetMapName().EndsWith(TEXT("TestMap03"));
}

ETickableTickType ULNPGraphicsBenchmarkSubsystem::GetTickableTickType() const
{
	return Stage == EStage::Done ? ETickableTickType::Never : Super::GetTickableTickType();
}

TStatId ULNPGraphicsBenchmarkSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(ULNPGraphicsBenchmarkSubsystem, STATGROUP_Tickables);
}

void ULNPGraphicsBenchmarkSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UMassEntitySubsystem>();
	Profile = ProfileName();
	WarmupSeconds = GGraphicsWarmup.GetValueOnGameThread();
	DurationSeconds = GGraphicsDuration.GetValueOnGameThread();
	FParse::Value(FCommandLine::Get(), TEXT("LNPGraphicsWarmup="), WarmupSeconds);
	FParse::Value(FCommandLine::Get(), TEXT("LNPGraphicsDuration="), DurationSeconds);
	bQuit = FParse::Param(FCommandLine::Get(), TEXT("LNPGraphicsQuit"));
	FString Root = FPaths::ProjectSavedDir() / TEXT("Profiling/Graphics/Phase00");
	FParse::Value(FCommandLine::Get(), TEXT("LNPGraphicsOutput="), Root);
	OutputDirectory = FPaths::ConvertRelativePathToFull(Root / (Profile + TEXT("_") + FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	IFileManager::Get().MakeDirectory(*OutputDirectory, true);
	Manifest = MakeShared<FJsonObject>();
	Manifest->SetStringField(TEXT("profile"), Profile);
	Manifest->SetStringField(TEXT("engine"), FEngineVersion::Current().ToString());
	Manifest->SetStringField(TEXT("build"), LexToString(FApp::GetBuildConfiguration()));
	Manifest->SetBoolField(TEXT("with_editor"), WITH_EDITOR != 0);
	Manifest->SetNumberField(TEXT("warmup_seconds"), WarmupSeconds);
	Manifest->SetNumberField(TEXT("duration_seconds"), DurationSeconds);
	Manifest->SetStringField(TEXT("scenario"), TEXT("Passive observation; AI activity follows camera; observer excluded from targeting"));
	StageStart = FPlatformTime::Seconds();
	if (!FMath::IsFinite(WarmupSeconds) || WarmupSeconds < 0 || !FMath::IsFinite(DurationSeconds) || DurationSeconds <= 0
		|| (Profile != TEXT("Low") && Profile != TEXT("Medium") && Profile != TEXT("High")) || LNPLoadBaseline::IsActive())
	{
		Finish(false, TEXT("Invalid profile/duration or incompatible load baseline"));
		return;
	}
	SaveManifest(TEXT("waiting"));
	UE_LOG(LogLootNPop, Display, TEXT("[GraphicsBenchmark] Waiting. Profile=%s Output=%s"), *Profile, *OutputDirectory);
}

void ULNPGraphicsBenchmarkSubsystem::Deinitialize()
{
	if (Stage != EStage::Done) Finish(false, TEXT("World ended before capture completed"));
	RestoreCVars();
	Super::Deinitialize();
}

bool ULNPGraphicsBenchmarkSubsystem::ReadViewpoints(const FString& Json, TArray<FTransform>& OutViews, float& OutFOV, float& OutAspect)
{
	OutViews.Reset();
	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root.IsValid()) return false;
	FString Space, Asset, Map;
	double Seed, Slot, PCGOffset;
	if (!Root->TryGetStringField(TEXT("coordinate_space"), Space) || Space != TEXT("octant_local")
		|| !Root->TryGetStringField(TEXT("map"), Map) || Map != TEXT("/Game/Maps/TestMap03")
		|| !Root->TryGetStringField(TEXT("octant_asset"), Asset) || Asset != TEXT("/Game/Maps/Acropolis_00/LVI_Octant_Acropolis_00")
		|| !Root->TryGetNumberField(TEXT("world_seed"), Seed) || Seed != 1
		|| !Root->TryGetNumberField(TEXT("pcg_seed_offset"), PCGOffset) || PCGOffset != 0
		|| !Root->TryGetNumberField(TEXT("target_slot"), Slot) || Slot != 6) return false;
	const TSharedPtr<FJsonObject>* Lens;
	double FOV, Aspect;
	FString Axis;
	if (!Root->TryGetObjectField(TEXT("lens"), Lens) || !(*Lens)->TryGetNumberField(TEXT("fieldOfView"), FOV)
		|| !(*Lens)->TryGetStringField(TEXT("aspectRatioAxisConstraint"), Axis) || Axis != TEXT("AspectRatio_MaintainYFOV")
		|| !(*Lens)->TryGetNumberField(TEXT("aspectRatio"), Aspect) || !FMath::IsFinite(FOV) || FOV <= 0 || FOV >= 180
		|| !FMath::IsFinite(Aspect) || Aspect <= 0) return false;
	const TArray<TSharedPtr<FJsonValue>>* Points;
	if (!Root->TryGetArrayField(TEXT("points"), Points) || Points->Num() != 3) return false;
	for (const auto& Point : *Points)
	{
		const TSharedPtr<FJsonObject>* Object;
		FTransform View;
		if (!Point->AsObject().IsValid() || !Point->AsObject()->TryGetObjectField(TEXT("camera"), Object) || !ReadPose(*Object, View)) return false;
		OutViews.Add(View);
	}
	OutFOV = FOV;
	OutAspect = Aspect;
	return true;
}

FTransform ULNPGraphicsBenchmarkSubsystem::EvaluateView(TConstArrayView<FTransform> InViews, double Phase)
{
	const double Position = FMath::Clamp(Phase, 0.0, 1.0) * InViews.Num();
	const int32 Index = FMath::Min(FMath::FloorToInt(Position), InViews.Num() - 1);
	const double Alpha = Position - Index;
	const FTransform& A = InViews[Index];
	const FTransform& B = InViews[(Index + 1) % InViews.Num()];
	return FTransform(FQuat::Slerp(A.GetRotation(), B.GetRotation(), Alpha).GetNormalized(),
		FMath::Lerp(A.GetLocation(), B.GetLocation(), Alpha));
}

void ULNPGraphicsBenchmarkSubsystem::SetRunCVar(const TCHAR* Name, const TCHAR* Value)
{
	if (IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(Name))
	{
		SavedCVars.Add({Name, Var->GetString(), uint32(Var->GetFlags() & ECVF_SetByMask)});
		Var->Set(Value, ECVF_SetByConsole);
	}
}

void ULNPGraphicsBenchmarkSubsystem::RestoreCVars()
{
	for (int32 I = SavedCVars.Num() - 1; I >= 0; --I)
		if (IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(*SavedCVars[I].Name))
		{
			Var->ClearFlags(ECVF_SetByMask);
			Var->Set(*SavedCVars[I].Value, EConsoleVariableFlags(SavedCVars[I].Priority));
		}
	SavedCVars.Reset();
}

bool ULNPGraphicsBenchmarkSubsystem::Prepare()
{
	UWorld* World = GetWorld();
	const ALNPGameState* State = World->GetGameState<ALNPGameState>();
	const auto* Octants = World->GetSubsystem<ULNPOctantSpawnSubsystem>();
	if (World->GetNetMode() != NM_Standalone || !State || State->OctantGenSeed != 1 || State->PCGSeedOffset != 0
		|| State->PlayerArray.Num() != 1 || GUsingNullRHI || !World->GetMapName().EndsWith(TEXT("TestMap03")))
	{
		Finish(false, TEXT("Requires rendered standalone, seed 1, PCG offset 0 and one player"));
		return false;
	}
	const auto& Definitions = Octants->GetSelectedOctantDefinitions();
	if (!Definitions.IsValidIndex(6) || Definitions[6].LevelAsset.ToSoftObjectPath().GetLongPackageName() != TEXT("/Game/Maps/Acropolis_00/LVI_Octant_Acropolis_00"))
	{
		Finish(false, TEXT("Acropolis is not in target slot 6"));
		return false;
	}
	FString Path = FPaths::ProjectDir() / TEXT("Scripts/Profiling/Graphics/AcropolisViewpoints.json");
	FParse::Value(FCommandLine::Get(), TEXT("LNPGraphicsPath="), Path);
	if (!FFileHelper::LoadFileToString(PathJson, *Path) || !ReadViewpoints(PathJson, Views, FieldOfView, ReferenceAspect))
	{
		Finish(false, TEXT("Invalid or missing viewpoint JSON"));
		return false;
	}
	FFileHelper::SaveStringToFile(PathJson, *(OutputDirectory / TEXT("Viewpoints.json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	if (World->WorldType == EWorldType::Game)
		UGameUserSettings::RequestResolutionChange(2560, 1440, EWindowMode::WindowedFullscreen, false);
	for (const TCHAR* Name : {TEXT("sg.ViewDistanceQuality"), TEXT("sg.AntiAliasingQuality"), TEXT("sg.ShadowQuality"),
		TEXT("sg.GlobalIlluminationQuality"), TEXT("sg.ReflectionQuality"), TEXT("sg.PostProcessQuality"),
		TEXT("sg.TextureQuality"), TEXT("sg.EffectsQuality"), TEXT("sg.FoliageQuality"), TEXT("sg.ShadingQuality"), TEXT("sg.LandscapeQuality")}) SetRunCVar(Name, TEXT("3"));
	SetRunCVar(TEXT("r.VSync"), TEXT("0"));
	SetRunCVar(TEXT("t.MaxFPS"), TEXT("0"));
	SetRunCVar(TEXT("r.DynamicRes.OperationMode"), TEXT("0"));
	SetRunCVar(TEXT("r.AntiAliasingMethod"), TEXT("4"));
	SetRunCVar(TEXT("r.ScreenPercentage"), TEXT("100"));
	SetRunCVar(TEXT("r.GPUStatsEnabled"), TEXT("1"));
	SetRunCVar(TEXT("csv.Benchmark"), TEXT("0"));
	// 에디터 전용 디버그 드로우와 진단 로그는 측정 구간에서 끈다. 원래 값은 월드 종료 때 복구한다.
	IConsoleManager::Get().ForEachConsoleObjectThatStartsWith(FConsoleObjectVisitor::CreateLambda([this](const TCHAR* Name, IConsoleObject* Object)
	{
		if (IConsoleVariable* Var = Object->AsVariable(); Var && !FCString::Strstr(Name, TEXT("Distance"))) SetRunCVar(Name, TEXT("0"));
	}), TEXT("LNP.Debug."));
	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC || !PC->GetPawn() || !World->GetGameViewport() || !World->GetGameViewport()->Viewport)
	{
		Finish(false, TEXT("No player or render viewport"));
		return false;
	}
	PC->SetIgnoreMoveInput(true);
	PC->SetIgnoreLookInput(true);
	PC->GetPawn()->SetActorHiddenInGame(true);
	PC->GetPawn()->SetActorEnableCollision(false);
	if (auto* Player = Cast<ALNPPlayerCharacter>(PC->GetPawn()))
		if (auto* Mover = Player->GetMoverComponent()) Mover->EnterDeadMode();
	UWidgetLayoutLibrary::RemoveAllWidgets(World);
	Camera = World->SpawnActor<ACameraActor>();
	if (!Camera) { Finish(false, TEXT("Camera spawn failed")); return false; }
	Camera->GetCameraComponent()->FieldOfView = FieldOfView;
	Camera->GetCameraComponent()->AspectRatio = ReferenceAspect;
	Camera->GetCameraComponent()->bConstrainAspectRatio = false;
	Camera->GetCameraComponent()->bOverrideAspectRatioAxisConstraint = true;
	Camera->GetCameraComponent()->AspectRatioAxisConstraint = AspectRatio_MaintainYFOV;
	PC->SetViewTarget(Camera);
	UpdateView(0.0);
	Manifest->SetNumberField(TEXT("world_seed"), State->OctantGenSeed);
	Manifest->SetNumberField(TEXT("pcg_seed_offset"), State->PCGSeedOffset);
	Manifest->SetStringField(TEXT("spawn_config"), LNPGraphicsBenchmark::GetSpawnConfigPath());
	Manifest->SetNumberField(TEXT("effective_enemy_density"), 1);
	Manifest->SetStringField(TEXT("adapter"), GRHIAdapterName);
	Manifest->SetNumberField(TEXT("field_of_view"), FieldOfView);
	Manifest->SetNumberField(TEXT("reference_aspect_ratio"), ReferenceAspect);
	Manifest->SetStringField(TEXT("aspect_ratio_axis_constraint"), TEXT("MaintainYFOV"));
	Manifest->SetNumberField(TEXT("population_sample_interval_seconds"), 1);
	TArray<TSharedPtr<FJsonValue>> Slots;
	for (const auto& Definition : Definitions) Slots.Add(MakeShared<FJsonValueString>(Definition.LevelAsset.ToString()));
	Manifest->SetArrayField(TEXT("octant_slots"), Slots);
	PopulationRows.Add(TEXT("ElapsedSeconds,EnemyConfig,SupportSlot,Alive,Dormant,Wander,Active,FrustumCandidates,Actor,Instanced,None"));
	SamplePopulation(0, false);
	int32 InitialEnemies = 0, InitialPods = 0;
	for (const auto& Pair : InitialSpawns)
		for (const auto& Counts : Pair.Value)
			(Counts.bEnemy ? InitialEnemies : InitialPods) += Counts.Accepted;
	Manifest->SetNumberField(TEXT("initial_enemy_total"), InitialEnemies);
	Manifest->SetNumberField(TEXT("initial_pod_total"), InitialPods);
	Manifest->SetStringField(TEXT("initial_request_scope"), TEXT("After spatial planning; plan shortfall is reported separately in the spawn log"));
	if (InitialEnemies <= 0 || InitialEnemies != LastAlive)
	{
		Finish(false, TEXT("Initial spawn counters do not match living enemy query"));
		return false;
	}
	SaveManifest(TEXT("warmup"));
	return true;
}

void ULNPGraphicsBenchmarkSubsystem::UpdateView(double Phase)
{
	const FTransform Local = EvaluateView(Views, Phase);
	const FTransform Slot(ULNPOctantSpawnSubsystem::OctantRotations[6]);
	Camera->SetActorTransform(Local * Slot);
}

void ULNPGraphicsBenchmarkSubsystem::RecordSpawn(const UMassEntityConfigAsset& Config, int32 Slot, int32 Requested, int32 Accepted, bool bEnemy)
{
	auto& Counts = InitialSpawns.FindOrAdd(Config.GetPathName());
	Counts.SetNum(9);
	auto& Entry = Counts[FMath::IsWithin(Slot, 0, 8) ? Slot : 8];
	Entry.Requested += Requested;
	Entry.Accepted += Accepted;
	Entry.bEnemy = bEnemy;
}

void ULNPGraphicsBenchmarkSubsystem::SamplePopulation(double Elapsed, bool bWriteRows)
{
	auto& Manager = GetWorld()->GetSubsystem<UMassEntitySubsystem>()->GetMutableEntityManager();
	if (!PopulationQuery.IsInitialized())
	{
		PopulationQuery = FMassEntityQuery(Manager.AsShared());
		PopulationQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
		PopulationQuery.AddRequirement<FLNPEnemyFragment>(EMassFragmentAccess::ReadOnly);
		PopulationQuery.AddRequirement<FMassRepresentationFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
		PopulationQuery.AddConstSharedRequirement<FLNPEnemySharedFragment>();
		PopulationQuery.AddTagRequirement<FLNPEnemyDyingTag>(EMassFragmentPresence::None);
	}
	FSceneViewProjectionData Projection;
	FConvexVolume Frustum;
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	const ULocalPlayer* LocalPlayer = PC->GetLocalPlayer();
	const bool bHasFrustum = LocalPlayer && LocalPlayer->GetProjectionData(GetWorld()->GetGameViewport()->Viewport, Projection);
	if (bHasFrustum) GetViewFrustumBounds(Frustum, Projection.ComputeViewProjectionMatrix(), false);
	struct FCounts { int32 Alive = 0, Bands[3] = {}, Frustum = 0, Actors = 0, Instances = 0, None = 0; };
	TMap<FString, TArray<FCounts>> Buckets;
	FMassExecutionContext Context(Manager, 0.f);
	PopulationQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& Ctx)
	{
		const auto& Shared = Ctx.GetConstSharedFragment<FLNPEnemySharedFragment>();
		if (!Shared.Config) return;
		auto& Counts = Buckets.FindOrAdd(Shared.Config->GetPathName());
		Counts.SetNum(9);
		const auto Enemies = Ctx.GetFragmentView<FLNPEnemyFragment>();
		const auto Positions = Ctx.GetFragmentView<FTransformFragment>();
		const auto Representations = Ctx.GetFragmentView<FMassRepresentationFragment>();
		for (int32 I = 0; I < Ctx.GetNumEntities(); ++I)
		{
			if (Enemies[I].Health <= 0) continue;
			const int32 Slot = Enemies[I].SurfaceHandle.OctantSlot;
			auto& Entry = Counts[FMath::IsWithin(Slot, 0, 8) ? Slot : 8];
			++Entry.Alive;
			++Entry.Bands[static_cast<int32>(Enemies[I].ActivityBand)];
			if (bHasFrustum && Frustum.IntersectSphere(Positions[I].GetTransform().GetLocation(),
				FMath::Max(Shared.Config->CapsuleHalfHeight, Shared.Config->CapsuleRadius))) ++Entry.Frustum;
			const auto Type = Representations.IsValidIndex(I) ? Representations[I].CurrentRepresentation : EMassRepresentationType::None;
			if (Type == EMassRepresentationType::StaticMeshInstance || Type == EMassRepresentationType::SkinnedMeshInstance) ++Entry.Instances;
			else if (Type == EMassRepresentationType::None) ++Entry.None;
			else ++Entry.Actors;
		}
	});
	LastAlive = LastActive = LastFrustumCandidates = LastActors = LastInstances = 0;
	for (const auto& Pair : Buckets)
		for (int32 Slot = 0; Slot < 9; ++Slot)
		{
			const auto& C = Pair.Value[Slot];
			LastAlive += C.Alive;
			LastActive += C.Bands[static_cast<int32>(ELNPEnemyActivityBand::Active)];
			LastFrustumCandidates += C.Frustum;
			LastActors += C.Actors;
			LastInstances += C.Instances;
			if (bWriteRows && C.Alive > 0) PopulationRows.Add(FString::Printf(TEXT("%.3f,%s,%d,%d,%d,%d,%d,%d,%d,%d,%d"),
				Elapsed, *Pair.Key, Slot == 8 ? INDEX_NONE : Slot, C.Alive, C.Bands[0], C.Bands[1], C.Bands[2], C.Frustum, C.Actors, C.Instances, C.None));
		}
}

void ULNPGraphicsBenchmarkSubsystem::SaveManifest(const FString& Status, const FString& Reason)
{
	if (!Manifest) return;
	Manifest->SetStringField(TEXT("status"), Status);
	Manifest->SetStringField(TEXT("reason"), Reason);
	Manifest->SetNumberField(TEXT("measured_frames"), MeasuredFrames);
	Manifest->SetNumberField(TEXT("last_alive"), LastAlive);
	Manifest->SetNumberField(TEXT("last_active"), LastActive);
	Manifest->SetBoolField(TEXT("frustum_count_includes_occluded"), true);
	Manifest->SetStringField(TEXT("instance_count_scope"), TEXT("ISM and ISKM combined; representation state, not visibility"));
	TArray<TSharedPtr<FJsonValue>> Initial;
	for (const auto& Pair : InitialSpawns)
		for (int32 Slot = 0; Slot < Pair.Value.Num(); ++Slot)
		{
			const auto& C = Pair.Value[Slot];
			if (C.Requested == 0) continue;
			auto Row = MakeShared<FJsonObject>();
			Row->SetStringField(TEXT("entity_config"), Pair.Key);
			Row->SetNumberField(TEXT("slot"), Slot == 8 ? INDEX_NONE : Slot);
			Row->SetBoolField(TEXT("enemy"), C.bEnemy);
			Row->SetNumberField(TEXT("post_plan_requested"), C.Requested);
			Row->SetNumberField(TEXT("initialized"), C.Accepted);
			Row->SetNumberField(TEXT("rejected"), C.Requested - C.Accepted);
			Initial.Add(MakeShared<FJsonValueObject>(Row));
		}
	Manifest->SetArrayField(TEXT("initial_spawns"), Initial);
	auto CVars = MakeShared<FJsonObject>();
	for (const auto& Backup : SavedCVars)
		if (IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(*Backup.Name)) CVars->SetStringField(Backup.Name, Var->GetString());
	for (const TCHAR* Name : {TEXT("r.DynamicGlobalIlluminationMethod"), TEXT("r.ReflectionMethod"), TEXT("r.Shadow.Virtual.Enable"),
		TEXT("r.RayTracing"), TEXT("r.Lumen.HardwareRayTracing"), TEXT("rhi.Name")})
		if (IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(Name)) CVars->SetStringField(Name, Var->GetString());
	Manifest->SetObjectField(TEXT("effective_cvars"), CVars);
	FIntPoint Size = FIntPoint::ZeroValue;
	if (GetWorld()->GetGameViewport() && GetWorld()->GetGameViewport()->Viewport) Size = GetWorld()->GetGameViewport()->Viewport->GetSizeXY();
	Manifest->SetNumberField(TEXT("viewport_width"), Size.X);
	Manifest->SetNumberField(TEXT("viewport_height"), Size.Y);
	Manifest->SetBoolField(TEXT("performance_eligible"), !WITH_EDITOR && WarmupSeconds >= 60 && DurationSeconds >= 60 && Size == FIntPoint(2560, 1440));
	FString Json;
	FJsonSerializer::Serialize(Manifest.ToSharedRef(), TJsonWriterFactory<>::Create(&Json));
	if (!FFileHelper::SaveStringToFile(Json, *(OutputDirectory / TEXT("Run.json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		UE_LOG(LogLootNPop, Error, TEXT("[GraphicsBenchmark] Failed to write manifest: %s"), *OutputDirectory);
}

void ULNPGraphicsBenchmarkSubsystem::Finish(bool bSuccess, const FString& Reason)
{
#if CSV_PROFILER
	if (bOwnsCsv) { FCsvProfiler::Get()->EndCapture(); bOwnsCsv = false; }
#endif
	Stage = EStage::Done;
	SaveManifest(bSuccess ? TEXT("complete") : TEXT("failed"), Reason);
	if (PopulationRows.Num() > 0) FFileHelper::SaveStringArrayToFile(PopulationRows, *(OutputDirectory / TEXT("Population.csv")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	RestoreCVars();
	UE_LOG(LogLootNPop, Display, TEXT("[GraphicsBenchmark] %s Frames=%d Output=%s Reason=%s"),
		bSuccess ? TEXT("Complete") : TEXT("Failed"), MeasuredFrames, *OutputDirectory, *Reason);
	if (bQuit && GetWorld()->WorldType == EWorldType::Game) FPlatformMisc::RequestExitWithStatus(false, bSuccess ? 0 : 1);
}

void ULNPGraphicsBenchmarkSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	const double Now = FPlatformTime::Seconds();
	if (Stage == EStage::Waiting)
	{
		const auto* Spawner = GetWorld()->GetSubsystem<ULNPMassSpawnSubsystem>();
		const auto* Surface = GetWorld()->GetSubsystem<ULNPSurfaceDataSubsystem>();
		APlayerController* PC = GetWorld()->GetFirstPlayerController();
		if (Spawner && Spawner->HasFinishedSpawning() && Surface && Surface->TakeSnapshot().IsValid() && PC && PC->GetPawn())
		{
			if (Prepare()) { Stage = EStage::Warmup; StageStart = Now; }
		}
		else if (Now - StageStart > 180) Finish(false, TEXT("Initialization timed out"));
		return;
	}
#if CSV_PROFILER
	CSV_CUSTOM_STAT_GLOBAL(LNPGraphics_CaptureFrame, 0, ECsvCustomStatOp::Set);
	if (Stage == EStage::Warmup)
	{
		UpdateView(FMath::Fmod((Now - StageStart) / DurationSeconds, 1.0));
		if (Now - StageStart < WarmupSeconds) return;
		if (FCsvProfiler::Get()->IsCapturing() || FCsvProfiler::Get()->IsEndCapturePending()) { Finish(false, TEXT("CSV profiler is already in use")); return; }
		FCsvProfiler::Get()->BeginCapture(-1, OutputDirectory, TEXT("Frames.csv"));
		bOwnsCsv = true;
		Stage = EStage::Arming;
		return;
	}
	if (Stage == EStage::Arming)
	{
		if (!FCsvProfiler::Get()->IsCapturing()) return;
		StageStart = Now;
		NextPopulationSample = Now;
		Stage = EStage::Capture;
		SaveManifest(TEXT("capturing"));
		UE_LOG(LogLootNPop, Display, TEXT("[GraphicsBenchmark] Capture started. Seconds=%.3f"), DurationSeconds);
	}
	if (Stage == EStage::Capture)
	{
		const double Elapsed = Now - StageStart;
		if (Elapsed >= DurationSeconds)
		{
			CsvWrite = FCsvProfiler::Get()->EndCapture();
			bOwnsCsv = false;
			Stage = EStage::Flushing;
			StageStart = Now;
			return;
		}
		UpdateView(Elapsed / DurationSeconds);
		if (Now >= NextPopulationSample) { SamplePopulation(Elapsed, true); NextPopulationSample = Now + 1.0; }
		++MeasuredFrames;
		CSV_CUSTOM_STAT_GLOBAL(LNPGraphics_CaptureFrame, 1, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT_GLOBAL(LNPGraphics_PathSeconds, Elapsed, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT_GLOBAL(LNPGraphics_FrameMs, DeltaTime * 1000.f, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT_GLOBAL(LNPGraphics_GTMs, FPlatformTime::ToMilliseconds(GGameThreadTime), ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT_GLOBAL(LNPGraphics_Alive, LastAlive, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT_GLOBAL(LNPGraphics_Active, LastActive, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT_GLOBAL(LNPGraphics_FrustumCandidates, LastFrustumCandidates, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT_GLOBAL(LNPGraphics_Actors, LastActors, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT_GLOBAL(LNPGraphics_Instances, LastInstances, ECsvCustomStatOp::Set);
		ENQUEUE_RENDER_COMMAND(LNPGraphicsCsv)([](FRHICommandListImmediate&)
		{
			CSV_CUSTOM_STAT_GLOBAL(LNPGraphics_RTMs, FPlatformTime::ToMilliseconds(GRenderThreadTime), ECsvCustomStatOp::Set);
			CSV_CUSTOM_STAT_GLOBAL(LNPGraphics_GPUMs, FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles()), ECsvCustomStatOp::Set);
			CSV_CUSTOM_STAT_GLOBAL(LNPGraphics_DrawCalls, GNumDrawCallsRHI[0], ECsvCustomStatOp::Set);
		});
	}
	if (Stage == EStage::Flushing)
	{
		if (CsvWrite.IsValid() && CsvWrite.IsReady())
		{
			const FString Csv = CsvWrite.Get();
			Manifest->SetStringField(TEXT("frames_csv"), Csv);
			Finish(!Csv.IsEmpty() && IFileManager::Get().FileSize(*Csv) > 0 && MeasuredFrames > 1, TEXT(""));
		}
		else if (Now - StageStart > 60) Finish(false, TEXT("CSV write timed out"));
	}
#else
	Finish(false, TEXT("CSV profiler is not compiled into this target"));
#endif
}
