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
	if (false == bIsGenerating)
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

void ULNPOctantSpawnSubsystem::StartWorldGeneration()
{
	const ULNPSettings* Settings = GetDefault<ULNPSettings>();
	if (Settings == nullptr)
		return;

	ULNPOctantPoolData* PoolData = Settings->OctantPool.LoadSynchronous();
	if (PoolData == nullptr || PoolData->OctantPool.Num() == 0)
	{
		UE_LOG(LogLootNPop, Warning, TEXT("LNPOctantSpawnSubsystem: OctantPool is empty or not set in LNPSettings!"));
		return;
	}

	UWorld* World = GetWorld();
	check(World);

	int32 OctantGenSeed = 0;
	if (ALNPGameState* GS = World->GetGameState<ALNPGameState>())
		OctantGenSeed = GS->OctantGenSeed;

	FRandomStream RandomStream(OctantGenSeed == 0 ? FMath::Rand() : OctantGenSeed);

	SpawnedOctants.Empty();
	bIsGenerating = true;

	TArray<int32> SelectedIndices;
	while (SelectedIndices.Num() < 8)
	{
		TArray<int32> Batch;
		for (int32 i = 0; i < PoolData->OctantPool.Num(); ++i)
			Batch.Add(i);
		for (int32 i = Batch.Num() - 1; i > 0; --i)
			Batch.Swap(i, RandomStream.RandRange(0, i));
		SelectedIndices.Append(Batch);
	}

	for (int32 i = 0; i < 8; ++i)
	{
		int32 OctantIndex = SelectedIndices[i];
		TSoftObjectPtr<UWorld> OctantLevel = PoolData->OctantPool[OctantIndex];

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

	UE_LOG(LogLootNPop, Log, TEXT("LNPOctantSpawnSubsystem: Spawned 8 LevelInstances. Waiting for load..."));
}
