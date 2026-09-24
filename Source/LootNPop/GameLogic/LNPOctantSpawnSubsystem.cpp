// Copyright (c) 2026 LootNPop. All rights reserved.

#include "GameLogic/LNPOctantSpawnSubsystem.h"
#include "DataAsset/LNPOctantPoolData.h"
#include "Config/LNPSettings.h"
#include "GameMode/LNPGameState.h"
#include "LootNPop.h"

#include "Kismet/KismetMathLibrary.h"
#include "Engine/Level.h"
#include "LevelInstance/LevelInstanceActor.h"
#include "LevelInstance/LevelInstanceSubsystem.h"

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

			if (ALevelInstance* LevelInstance = World->SpawnActor<ALevelInstance>(ALevelInstance::StaticClass(), FVector::ZeroVector, OctantRotations[i], SpawnParams))
			{
				LevelInstance->SetWorldAsset(OctantLevel);
				LevelInstance->LoadLevelInstance();
				SpawnedOctants.Add(LevelInstance);
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
