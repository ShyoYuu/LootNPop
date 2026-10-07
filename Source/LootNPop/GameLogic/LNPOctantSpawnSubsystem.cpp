// Copyright (c) 2026 LootNPop. All rights reserved.

#include "GameLogic/LNPOctantSpawnSubsystem.h"
#include "DataAsset/LNPOctantPoolData.h"
#include "Config/LNPSettings.h"
#include "GameMode/LNPGameState.h"
#include "LootNPop.h"
#include "SurfaceNavigation/LNPLoadBaseline.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

#include "GameFramework/GameModeBase.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/KismetMathLibrary.h"
#include "Engine/Level.h"
#include "GameLogic/LNPOctantLevelInstance.h"
#include "LevelInstance/LevelInstanceSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/TargetPoint.h"
#include "EngineUtils.h"

const FRotator ULNPOctantSpawnSubsystem::OctantRotations[8] = {
	FRotator(0.f, 0.f, 0.f),
	FRotator(0.f, 90.f, 0.f),
	FRotator(0.f, 180.f, 0.f),
	FRotator(0.f, 270.f, 0.f),
	FRotator(180.f, 0.f, 0.f),
	FRotator(180.f, 90.f, 0.f),
	FRotator(180.f, 180.f, 0.f),
	FRotator(180.f, 270.f, 0.f)
};

void ULNPOctantSpawnSubsystem::Tick(float DeltaTime)
{
	if (!bIsGenerating)
		return;

	ULevelInstanceSubsystem* LevelInstanceSub = GetWorld()->GetSubsystem<ULevelInstanceSubsystem>();

	bool bAllLoaded = true;
	for (ALevelInstance* Octant : SpawnedOctants)
	{
		if (!Octant || !Octant->IsLoaded())
		{
			bAllLoaded = false;
			break;
		}

		// IsLoaded()는 레벨 "패키지" 로드만 보장할 뿐 AddToWorld 완료(콜리전 물리 씬 등록)는 보장하지 않는다.
		// bIsVisible이 true여야 컴포넌트 등록이 끝나 라인트레이스가 표면에 명중한다.
		// 이 검사가 없으면 중간참여 클라이언트처럼 AddToWorld가 프레임에 걸쳐 지연될 때
		// 콜리전이 아직 없는 상태로 표면 베이킹이 시작되어 트레이스 미스가 발생한다.
		ULevel* OctantLevel = LevelInstanceSub ? LevelInstanceSub->GetLevelInstanceLevel(Octant) : nullptr;
		if (!OctantLevel || !OctantLevel->bIsVisible)
		{
			bAllLoaded = false;
			break;
		}
	}

	if (bAllLoaded && SpawnedOctants.Num() >= 8)
	{
		// SpawnedOctants는 스폰 순서 = slot 순서다(StartWorldGeneration이 8개 전부 성공해야 진행).
		for (ALevelInstance* Octant : SpawnedOctants)
		{
			SlotLevelInstances.Add(Octant);
			SlotLevels.Add(LevelInstanceSub->GetLevelInstanceLevel(Octant));
		}

		bIsGenerating = false;
		bGenerationComplete = true;
		UE_LOG(LogLootNPop, Log, TEXT("LNPOctantSpawnSubsystem: All 8 octants are fully loaded. Broadcasting Finished event."));
		OnWorldGenerationFinished.Broadcast();
		SpawnedOctants.Empty();
	}
}

TStatId ULNPOctantSpawnSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(ULNPOctantSpawnSubsystem, STATGROUP_Tickables);
}

ULevel* ULNPOctantSpawnSubsystem::GetSlotLevel(const int32 SlotIndex) const
{
	return SlotLevels.IsValidIndex(SlotIndex) ? SlotLevels[SlotIndex].Get() : nullptr;
}

int32 ULNPOctantSpawnSubsystem::FindSlotForLevel(const ULevel* Level) const
{
	if (Level == nullptr)
		return INDEX_NONE;

	for (int32 SlotIndex = 0; SlotIndex < SlotLevels.Num(); ++SlotIndex)
	{
		if (SlotLevels[SlotIndex].Get() == Level)
			return SlotIndex;
	}
	return INDEX_NONE;
}

bool ULNPOctantSpawnSubsystem::SelectOctantDefinitions(
	const TArray<FLNPOctantDefinition>& Definitions,
	const int32 Seed,
	TArray<FLNPOctantDefinition>& OutSelectedDefinitions,
	TArray<int32>* OutSourceIndices,
	FString* OutError)
{
	constexpr int32 SlotCount = UE_ARRAY_COUNT(OctantRotations);

	OutSelectedDefinitions.Reset(SlotCount);
	if (OutSourceIndices != nullptr)
	{
		OutSourceIndices->Reset(SlotCount);
	}
	if (OutError != nullptr)
	{
		OutError->Reset();
	}

	if (Definitions.IsEmpty())
	{
		if (OutError != nullptr)
		{
			*OutError = TEXT("Octant definition pool is empty.");
		}
		return false;
	}

	FRandomStream RandomStream(Seed);
	TSet<int32> UsedInBatch;

	for (int32 SlotIndex = 0; SlotIndex < SlotCount; ++SlotIndex)
	{
		TArray<int32> EligibleIndices;
		for (int32 DefinitionIndex = 0; DefinitionIndex < Definitions.Num(); ++DefinitionIndex)
		{
			const FLNPOctantDefinition& Definition = Definitions[DefinitionIndex];
			if (!Definition.LevelAsset.IsNull()
				&& Definition.AllowsSlot(static_cast<uint8>(SlotIndex))
				&& !UsedInBatch.Contains(DefinitionIndex))
			{
				EligibleIndices.Add(DefinitionIndex);
			}
		}

		// 현재 batch에서 허용 후보를 모두 사용했다면 새 batch를 시작한다.
		if (EligibleIndices.IsEmpty())
		{
			UsedInBatch.Reset();
			for (int32 DefinitionIndex = 0; DefinitionIndex < Definitions.Num(); ++DefinitionIndex)
			{
				const FLNPOctantDefinition& Definition = Definitions[DefinitionIndex];
				if (!Definition.LevelAsset.IsNull()
					&& Definition.AllowsSlot(static_cast<uint8>(SlotIndex)))
				{
					EligibleIndices.Add(DefinitionIndex);
				}
			}
		}

		if (EligibleIndices.IsEmpty())
		{
			OutSelectedDefinitions.Reset();
			if (OutSourceIndices != nullptr)
			{
				OutSourceIndices->Reset();
			}
			if (OutError != nullptr)
			{
				*OutError = FString::Printf(TEXT("No valid octant definition can be placed in slot %d."), SlotIndex);
			}
			return false;
		}

		for (int32 Index = EligibleIndices.Num() - 1; Index > 0; --Index)
		{
			EligibleIndices.Swap(Index, RandomStream.RandRange(0, Index));
		}

		const int32 SelectedIndex = EligibleIndices[0];
		OutSelectedDefinitions.Add(Definitions[SelectedIndex]);
		if (OutSourceIndices != nullptr)
		{
			OutSourceIndices->Add(SelectedIndex);
		}
		UsedInBatch.Add(SelectedIndex);
	}

	return true;
}

void ULNPOctantSpawnSubsystem::StartWorldGeneration()
{
	const ULNPSettings* Settings = GetDefault<ULNPSettings>();
	if (Settings == nullptr)
		return;

	ULNPOctantPoolData* PoolData = Settings->OctantPool.LoadSynchronous();
	if (PoolData == nullptr)
	{
		UE_LOG(LogLootNPop, Error, TEXT("LNPOctantSpawnSubsystem: OctantPool is not set or could not be loaded."));
		return;
	}

	TArray<FLNPOctantDefinition> EffectiveDefinitions;
	PoolData->BuildEffectiveDefinitions(EffectiveDefinitions);
	// 동적 회귀 맵만 별도 authoring·베이크 쌍을 사용한다. 일반 pool과 정적 oracle은 보존한다.
	if (GetWorld()->GetMapName().EndsWith(TEXT("L_ConditionalPatchRegression")))
	{
		FLNPOctantDefinition Fixture;
		Fixture.LevelAsset = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Maps/SurfaceNavigation/Fixtures/LVI_Octant_Fixture_Conditional.LVI_Octant_Fixture_Conditional")));
		Fixture.SurfaceData = TSoftObjectPtr<ULNPOctantSurfaceData>(FSoftObjectPath(TEXT("/Game/Maps/SurfaceNavigation/Fixtures/DA_OctantSurface_Fixture_Conditional.DA_OctantSurface_Fixture_Conditional")));
		Fixture.SeamSignature = TEXT("RegressionSeam");
		EffectiveDefinitions = {Fixture};
	}
	// 부하 실행에만 회귀 옥탄트를 사용한다. production pool 에셋은 수정하지 않는다.
	if (LNPLoadBaseline::IsActive() && FParse::Param(FCommandLine::Get(), TEXT("LNPLoadBaselineVolumetric")))
	{
		FLNPOctantDefinition Fixture;
		Fixture.LevelAsset = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Maps/SurfaceNavigation/Fixtures/LVI_Octant_Fixture_Regression.LVI_Octant_Fixture_Regression")));
		Fixture.SurfaceData = TSoftObjectPtr<ULNPOctantSurfaceData>(FSoftObjectPath(TEXT("/Game/Maps/SurfaceNavigation/Fixtures/DA_OctantSurface_Fixture_Regression.DA_OctantSurface_Fixture_Regression")));
		Fixture.SeamSignature = TEXT("RegressionSeam");
		EffectiveDefinitions = {Fixture};
	}
	if (PoolData->OctantDefinitions.IsEmpty() && !PoolData->OctantPool.IsEmpty())
	{
		UE_LOG(LogLootNPop, Warning,
			TEXT("LNPOctantSpawnSubsystem: Using legacy OctantPool fallback. Migrate the asset to OctantDefinitions."));
	}

	UWorld* World = GetWorld();
	check(World);

	// 서버는 GameMode가 BeginPlay에서 seed를 확정한 뒤 호출하고, 클라이언트는 OnRep으로 동일 seed를 수신한 뒤 호출한다.
	// GameState가 없는 환경에서는 0도 유효한 결정론적 seed로 사용한다.
	int32 OctantGenSeed = 0;
	if (ALNPGameState* GS = World->GetGameState<ALNPGameState>())
		OctantGenSeed = GS->OctantGenSeed;

	SpawnedOctants.Empty();
	SlotLevelInstances.Empty();
	SlotLevels.Empty();
	SelectedOctantDefinitions.Empty();
	bGenerationComplete = false;

	FString SelectionError;
	if (!SelectOctantDefinitions(
		EffectiveDefinitions,
		OctantGenSeed,
		SelectedOctantDefinitions,
		nullptr,
		&SelectionError))
	{
		UE_LOG(LogLootNPop, Error, TEXT("LNPOctantSpawnSubsystem: %s"), *SelectionError);
		return;
	}

	bIsGenerating = true;

	for (int32 i = 0; i < 8; ++i)
	{
		const FLNPOctantDefinition& SelectedDefinition = SelectedOctantDefinitions[i];
		const TSoftObjectPtr<UWorld>& OctantLevel = SelectedDefinition.LevelAsset;

		if (!OctantLevel.IsNull())
		{
			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

			if (ALNPOctantLevelInstance* LevelInstance = World->SpawnActor<ALNPOctantLevelInstance>(ALNPOctantLevelInstance::StaticClass(), FVector::ZeroVector, OctantRotations[i], SpawnParams))
			{
				if (!LevelInstance->SetRuntimeWorldAsset(OctantLevel))
				{
					UE_LOG(LogLootNPop, Error, TEXT("LNPOctantSpawnSubsystem: Failed to set world asset %s for slot %d."), *OctantLevel.ToString(), i);
				}
				LevelInstance->LoadLevelInstance();
				SpawnedOctants.Add(LevelInstance);
				UE_LOG(LogLootNPop, Log, TEXT("LNPOctantSpawnSubsystem: slot %d (rotation %s) <- %s"),
					i, *OctantRotations[i].ToCompactString(), *FPackageName::GetShortName(OctantLevel.ToSoftObjectPath().GetLongPackageName()));
#if WITH_EDITOR
				LevelInstance->SetActorLabel(FString::Printf(TEXT("Octant_LVI_%d"), i));
#endif
			}
		}
	}

	if (SpawnedOctants.Num() != 8)
	{
		UE_LOG(LogLootNPop, Error,
			TEXT("LNPOctantSpawnSubsystem: Failed to spawn all 8 LevelInstances (spawned %d)."),
			SpawnedOctants.Num());
		for (ALevelInstance* SpawnedOctant : SpawnedOctants)
		{
			if (IsValid(SpawnedOctant))
			{
				SpawnedOctant->Destroy();
			}
		}
		SpawnedOctants.Empty();
		SelectedOctantDefinitions.Empty();
		bIsGenerating = false;
		return;
	}

	UE_LOG(LogLootNPop, Log, TEXT("LNPOctantSpawnSubsystem: Spawned 8 LevelInstances. Waiting for load..."));
}

namespace
{
	/** 옥탄트 로컬 프레임(옥탄트 중앙 위도 35.26°·방위 45°). X는 위도 증가, Y는 방위 증가 방향 호 길이(cm)다. */
	struct FOctantFrame
	{
		FVector Radial, Tangent, Bitangent;
		double Radius;

		FOctantFrame()
		{
			const double Lat = FMath::DegreesToRadians(35.264389682754654), Az = FMath::DegreesToRadians(45.0);
			Radial = FVector(FMath::Cos(Lat) * FMath::Cos(Az), FMath::Cos(Lat) * FMath::Sin(Az), FMath::Sin(Lat));
			Tangent = FVector(-FMath::Sin(Az), FMath::Cos(Az), 0.0);
			Bitangent = FVector(-FMath::Sin(Lat) * FMath::Cos(Az), -FMath::Sin(Lat) * FMath::Sin(Az), FMath::Cos(Lat));
			Radius = GetDefault<ULNPSettings>()->SphereRadius;
		}

		/** 로컬 (X, Y, H)의 월드 위치. H는 기준 반지름에서 구 중심 쪽 높이다. */
		FVector ToWorld(int32 Slot, double X, double Y, double H) const
		{
			const FVector Local = (Radial * Radius + Bitangent * X + Tangent * Y).GetSafeNormal() * (Radius - H);
			return ULNPOctantSpawnSubsystem::OctantRotations[Slot].RotateVector(Local);
		}

		/** 월드 위치가 속한 slot(회전을 되돌린 성분의 최솟값이 가장 큰 slot)과 로컬 (X, Y, H). */
		int32 ToLocal(const FVector& World, FVector& OutLocal) const
		{
			int32 Best = 0;
			double BestMin = -UE_BIG_NUMBER;
			for (int32 Slot = 0; Slot < UE_ARRAY_COUNT(ULNPOctantSpawnSubsystem::OctantRotations); ++Slot)
			{
				const double Min = ULNPOctantSpawnSubsystem::OctantRotations[Slot].UnrotateVector(World).GetMin();
				if (Min > BestMin)
				{
					BestMin = Min;
					Best = Slot;
				}
			}
			const FVector P = ULNPOctantSpawnSubsystem::OctantRotations[Best].UnrotateVector(World);
			const FVector Dir = P.GetSafeNormal();
			const double K = Radius / FVector::DotProduct(Dir, Radial);
			OutLocal = FVector(FVector::DotProduct(Dir, Bitangent) * K, FVector::DotProduct(Dir, Tangent) * K, Radius - P.Length());
			return Best;
		}
	};

	/**
	 * 모든 플레이어를 Location에 리스폰한다(Up은 구 중심 쪽). 게스트 폰은 Mover 텔레포트로 옮겨지지 않으므로
	 * 리스폰을 쓴다(LNPLoadBaseline과 같다). 여러 명이면 서로 겹치지 않게 접평면에서 200cm씩 띄운다.
	 */
	int32 RespawnAllPlayersAt(UWorld& World, AGameModeBase& GameMode, const FVector& Location)
	{
		const FVector Up = -Location.GetSafeNormal();
		int32 Index = 0;
		for (FConstPlayerControllerIterator It = World.GetPlayerControllerIterator(); It; ++It, ++Index)
		{
			APlayerController* PC = It->Get();
			if (!PC)
				continue;
			if (APawn* OldPawn = PC->GetPawn())
			{
				PC->UnPossess();
				OldPawn->Destroy();
			}
			const FVector Side = FVector::CrossProduct(Up, FVector::UpVector).GetSafeNormal() * (Index * 200.0);
			GameMode.RestartPlayerAtTransform(PC, FTransform(UKismetMathLibrary::MakeRotFromZ(Up), Location + Side));
		}
		return Index;
	}

	/**
	 * 레벨 검토용 이동 커맨드. 옥탄트 로컬 좌표를 slot 회전으로 옮겨 모든 플레이어를 그 위 150cm에 리스폰한다.
	 * Usage: LNP.Debug.TeleportToOctant <Slot> <X> <Y> [H]
	 */
	FAutoConsoleCommandWithWorldAndArgs GLNPDebugTeleportToOctant(
		TEXT("LNP.Debug.TeleportToOctant"),
		TEXT("Server-only: respawn all players above an octant-local point of a slot. Args: <Slot> <X> <Y> [H]. ")
		TEXT("X/Y are arc lengths (cm) from the octant center toward latitude/azimuth increase, H is height (cm) above the sphere radius."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			AGameModeBase* GameMode = World ? World->GetAuthGameMode() : nullptr;
			const int32 Slot = Args.Num() >= 3 ? FCString::Atoi(*Args[0]) : INDEX_NONE;
			if (!GameMode || Slot < 0 || Slot >= UE_ARRAY_COUNT(ULNPOctantSpawnSubsystem::OctantRotations))
			{
				UE_LOG(LogLootNPop, Warning, TEXT("[LNP.Debug.TeleportToOctant] Server only. Usage: <Slot 0-7> <X> <Y> [H]"));
				return;
			}
			const double X = FCString::Atod(*Args[1]), Y = FCString::Atod(*Args[2]);
			const double H = Args.Num() >= 4 ? FCString::Atod(*Args[3]) : 0.0;
			const FVector Location = FOctantFrame().ToWorld(Slot, X, Y, H + 150.0);
			const int32 Count = RespawnAllPlayersAt(*World, *GameMode, Location);
			UE_LOG(LogLootNPop, Display, TEXT("[LNP.Debug.TeleportToOctant] %d players -> slot %d (%.0f, %.0f, %.0f) %s"),
				Count, Slot, X, Y, H, *Location.ToString());
		}));

	/** 빌더가 LVI에 두는 검토 지점(ATargetPoint, 태그 LNP.ReviewPoint + 이름 태그). */
	const FName ReviewPointTag(TEXT("LNP.ReviewPoint"));

	FName ReviewPointName(const AActor& Actor)
	{
		for (const FName& Tag : Actor.Tags)
		{
			if (Tag != ReviewPointTag)
			{
				return Tag;
			}
		}
		return NAME_None;
	}

	/**
	 * 이름 붙은 검토 지점으로 이동한다. 인자가 없으면 로드된 검토 지점 이름과 slot을 출력한다.
	 * Usage: LNP.Debug.TeleportToReview [Name] [Slot]
	 */
	FAutoConsoleCommandWithWorldAndArgs GLNPDebugTeleportToReview(
		TEXT("LNP.Debug.TeleportToReview"),
		TEXT("Server-only: respawn all players at a named review point placed by the slice-octant builder. Args: [Name] [Slot]. ")
		TEXT("Without arguments, list the loaded review points and their slots."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			AGameModeBase* GameMode = World ? World->GetAuthGameMode() : nullptr;
			const ULNPOctantSpawnSubsystem* Spawn = World ? World->GetSubsystem<ULNPOctantSpawnSubsystem>() : nullptr;
			if (!GameMode || !Spawn)
			{
				UE_LOG(LogLootNPop, Warning, TEXT("[LNP.Debug.TeleportToReview] Server only."));
				return;
			}
			const int32 WantedSlot = Args.Num() >= 2 ? FCString::Atoi(*Args[1]) : INDEX_NONE;
			TMap<FName, TArray<int32>> Listing;
			for (TActorIterator<ATargetPoint> It(World); It; ++It)
			{
				if (!It->ActorHasTag(ReviewPointTag))
					continue;
				const FName Name = ReviewPointName(**It);
				const int32 Slot = Spawn->FindSlotForLevel(It->GetLevel());
				Listing.FindOrAdd(Name).Add(Slot);
				if (Args.Num() >= 1 && Name.ToString().Equals(Args[0], ESearchCase::IgnoreCase)
					&& (WantedSlot == INDEX_NONE || WantedSlot == Slot))
				{
					const FVector Location = It->GetActorLocation() - It->GetActorLocation().GetSafeNormal() * 150.0;
					const int32 Count = RespawnAllPlayersAt(*World, *GameMode, Location);
					UE_LOG(LogLootNPop, Display, TEXT("[LNP.Debug.TeleportToReview] %d players -> %s (slot %d)"), Count, *Name.ToString(), Slot);
					return;
				}
			}
			if (Args.Num() >= 1)
			{
				UE_LOG(LogLootNPop, Warning, TEXT("[LNP.Debug.TeleportToReview] No review point '%s'."), *Args[0]);
			}
			for (const TPair<FName, TArray<int32>>& Pair : Listing)
			{
				FString Slots;
				for (const int32 Slot : Pair.Value)
				{
					Slots += FString::Printf(TEXT("%s%d"), Slots.IsEmpty() ? TEXT("") : TEXT(","), Slot);
				}
				UE_LOG(LogLootNPop, Display, TEXT("[LNP.Debug.TeleportToReview] %s (slots %s)"), *Pair.Key.ToString(), *Slots);
			}
		}));

	/**
	 * 각 플레이어의 slot·옥탄트 로컬 좌표·발밑 메시를 출력한다. 레벨 검토 중 위치를 알려 주는 용도다.
	 * Usage: LNP.Debug.WhereAmI
	 */
	FAutoConsoleCommandWithWorld GLNPDebugWhereAmI(
		TEXT("LNP.Debug.WhereAmI"),
		TEXT("Print each player's slot, octant level, octant-local position (X, Y, H, polar radius/degrees) and the mesh under their feet."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			const ULNPOctantSpawnSubsystem* Spawn = World ? World->GetSubsystem<ULNPOctantSpawnSubsystem>() : nullptr;
			if (!Spawn)
				return;
			const FOctantFrame Frame;
			for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
			{
				const APawn* Pawn = It->Get() ? It->Get()->GetPawn() : nullptr;
				if (!Pawn)
					continue;
				const FVector Position = Pawn->GetActorLocation();
				FVector Local;
				const int32 Slot = Frame.ToLocal(Position, Local);
				const TArray<FLNPOctantDefinition>& Definitions = Spawn->GetSelectedOctantDefinitions();
				const FString Level = Definitions.IsValidIndex(Slot) ? Definitions[Slot].LevelAsset.GetAssetName() : TEXT("?");

				// 발밑: 중력 방향(구 바깥)으로 5m 아래까지.
				FHitResult Hit;
				const FCollisionQueryParams Params(SCENE_QUERY_STAT(LNPWhereAmI), false, Pawn);
				FString Floor = TEXT("none");
				if (World->LineTraceSingleByChannel(Hit, Position, Position + Position.GetSafeNormal() * 500.0, ECC_Visibility, Params))
				{
					const UStaticMeshComponent* Mesh = Cast<UStaticMeshComponent>(Hit.GetComponent());
					Floor = Mesh && Mesh->GetStaticMesh() ? Mesh->GetStaticMesh()->GetName() : GetNameSafe(Hit.GetComponent());
				}
				UE_LOG(LogLootNPop, Display,
					TEXT("[LNP.Debug.WhereAmI] %s slot %d (%s) local X=%.0f Y=%.0f H=%.0f | r=%.0f deg=%.1f | floor=%s"),
					*GetNameSafe(Pawn), Slot, *Level, Local.X, Local.Y, Local.Z,
					FVector2D(Local.X, Local.Y).Length(), FMath::RadiansToDegrees(FMath::Atan2(Local.Y, Local.X)), *Floor);
			}
		}));
}
