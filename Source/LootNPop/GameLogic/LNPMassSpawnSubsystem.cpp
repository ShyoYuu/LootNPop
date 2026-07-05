// Copyright (c) 2026 LootNPop. All rights reserved.

#include "GameLogic/LNPMassSpawnSubsystem.h"
#include "GameLogic/LNPSurfaceCacheSubsystem.h"
#include "DataAsset/LNPMassSpawnConfig.h"
#include "Config/LNPSettings.h"
#include "Enemy/LNPEnemyMassTypes.h"
#include "Enemy/LNPEnemyReplication.h"
#include "Character/LNPPlayerReplication.h"
#include "LootNPop.h"

#include "Async/Async.h"

#include "MassEntityConfigAsset.h"
#include "MassAgentComponent.h"
#include "MassSpawnerSubsystem.h"
#include "MassEntityManager.h"
#include "GameFramework/Pawn.h"
#include "MassCommonFragments.h"
#include "MassReplicationSubsystem.h"
#include "Engine/World.h"
#include "Kismet/KismetMathLibrary.h"

void ULNPMassSpawnSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	RandomStream.GenerateNewSeed();

	// MassReplication(Phase 6/6.5): 클라이언트가 접속하기 전에 BubbleInfoClass를 등록해야 한다 (RegisterBubbleInfoClass 문서 제약).
	if (UMassReplicationSubsystem* ReplicationSubsystem = GetWorld()->GetSubsystem<UMassReplicationSubsystem>())
	{
		ReplicationSubsystem->RegisterBubbleInfoClass(ALNPEnemyClientBubbleInfo::StaticClass());
		ReplicationSubsystem->RegisterBubbleInfoClass(ALNPPlayerClientBubbleInfo::StaticClass());
	}

	// Enemy MassReplication(Phase 6): Enemy 스폰(BeginSpawning)은 서버 전용 ALNPGameMode가 호출하므로
	// 클라이언트는 EnemyEntityConfig의 FMassEntityTemplate을 로컬 TemplateRegistry에 등록할 기회가 전혀 없다.
	// Bubble이 복제 스폰(SpawnEntities by TemplateID) 시도 시 "TemplateID must have been registered!"로 죽는 것을 막기 위해,
	// 실제 스폰 없이 템플릿만 서버·클라이언트 양쪽에서 미리 등록(warm-up)한다.
	if (const ULNPSettings* Settings = GetDefault<ULNPSettings>())
	{
		if (const ULNPMassSpawnConfig* Config = Settings->MassSpawnConfig.LoadSynchronous())
		{
			for (const FLNPLootPodSpawnEntry& PodEntry : Config->LootPodSpawnSets)
			{
				for (const FLNPEnemySpawnEntry& EnemyEntry : PodEntry.AssociatedEnemies)
				{
					if (UMassEntityConfigAsset* EnemyConfig = EnemyEntry.EnemyEntityConfig)
						EnemyConfig->GetOrCreateEntityTemplate(*GetWorld());
				}
			}
		}

		// Player MassReplication(Phase 6.5): Player 엔티티 템플릿도 동일한 이유로 warm-up이 필요하다.
		// 단, Player의 TemplateID는 폰 BP에 저장된 MassAgentComponent::EntityConfig(구조체 GUID)에서 파생되므로
		// DA 에셋이 아니라 폰 CDO의 컴포넌트를 기준으로 등록해야 서버가 방송하는 TemplateID와 일치한다.
		if (UClass* PawnClass = Settings->PlayerPawnClass.LoadSynchronous())
		{
			const APawn* PawnCDO = PawnClass->GetDefaultObject<APawn>();
			const UMassAgentComponent* AgentComp = PawnCDO ? PawnCDO->FindComponentByClass<UMassAgentComponent>() : nullptr;
			if (AgentComp)
			{
				AgentComp->GetEntityConfig().GetOrCreateEntityTemplate(*GetWorld());
			}
			else
			{
				UE_LOG(LogLootNPop, Warning, TEXT("Player template warm-up failed: PawnCDO=%s has no MassAgentComponent"), *GetNameSafe(PawnCDO));
			}
		}
		else
		{
			UE_LOG(LogLootNPop, Warning, TEXT("Player template warm-up skipped: LNPSettings.PlayerPawnClass not set — client puppet linking will not work"));
		}
	}
}

void ULNPMassSpawnSubsystem::BeginSpawning()
{
	UE_LOG(LogLootNPop, Log, TEXT("LNPMassSpawnSubsystem: World ready. Loading config from LNPSettings."));

	if (const ULNPSettings* Settings = GetDefault<ULNPSettings>())
	{
		if (ULNPMassSpawnConfig* Config = Settings->MassSpawnConfig.LoadSynchronous())
		{
			EnqueueSpawnProject(Config, Settings->SphereRadius);
		}
		else
		{
			UE_LOG(LogLootNPop, Warning, TEXT("LNPMassSpawnSubsystem: MassSpawnConfig is null in LNPSettings!"));
		}
	}
}

void ULNPMassSpawnSubsystem::Deinitialize()
{
	if (SpawnBuildFuture.IsValid() && !SpawnBuildFuture.IsReady())
		SpawnBuildFuture.Wait();
	Super::Deinitialize();
}

void ULNPMassSpawnSubsystem::Tick(float DeltaTime)
{
	if (SpawnBuildFuture.IsValid())
	{
		if (!SpawnBuildFuture.IsReady())
			return;

		SpawnBuildFuture = TFuture<void>{};
		AssembleSpawnQueueFromAsyncResult();
	}

	ProcessQueue();
}

TStatId ULNPMassSpawnSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(ULNPMassSpawnSubsystem, STATGROUP_Tickables);
}

void ULNPMassSpawnSubsystem::EnqueueSpawnProject(ULNPMassSpawnConfig* InConfig, const float SphereRadius)
{
	if (!InConfig)
		return;
	if (!GetWorld())
		return;

	if (SpawnBuildFuture.IsValid() && !SpawnBuildFuture.IsReady())
	{
		UE_LOG(LogLootNPop, Warning, TEXT("LNPMassSpawnSubsystem: EnqueueSpawnProject called while async build is in progress. Ignoring."));
		return;
	}

	ActiveConfig = InConfig;
	SpawnQueue.Empty();
	AsyncBuildResult.Empty();
	CapturedAssets.Empty();
	SpawnQueueHead = 0;

	// UObject 참조를 미리 캡처하고 (게임 Thread 전용) 정수 Index 할당.
	// pod 세트당 레이아웃: [pod_config, enemy_config_0, enemy_config_1, ...]
	struct FPodSetBuildParams
	{
		int32 PodSetCount;
		int32 PodAssetIndex;
		struct FEnemyEntry { int32 Count; int32 AssetIndex; };
		TArray<FEnemyEntry> Enemies;
	};

	TArray<FPodSetBuildParams> SetParams;
	for (const FLNPLootPodSpawnEntry& PodSet : InConfig->LootPodSpawnSets)
	{
		FPodSetBuildParams P;
		P.PodSetCount = PodSet.PodSetCount;
		P.PodAssetIndex = CapturedAssets.Num();
		CapturedAssets.Add(PodSet.LootPodEntityConfig);

		for (const FLNPEnemySpawnEntry& Enemy : PodSet.AssociatedEnemies)
		{
			P.Enemies.Add({ Enemy.Count, CapturedAssets.Num() });
			CapturedAssets.Add(Enemy.EnemyEntityConfig);
		}
		SetParams.Add(MoveTemp(P));
	}

	// 표면 Cache의 읽기 전용 Snapshot 획득 — 태스크에서 UObject 접근 불필요
	ULNPSurfaceCacheSubsystem* SurfaceCache = GetWorld()->GetSubsystem<ULNPSurfaceCacheSubsystem>();
	FLNPSurfaceCacheSnapshot CacheSnap = SurfaceCache->TakeSnapshot();

	const float MinDist          = InConfig->MinDistanceBetweenPods;
	const float EnemyRadius      = InConfig->EnemySpawnRadiusAroundPod;
	const int32 MaxRetry         = InConfig->MaxRetryCount;
	const FRandomStream Rand     = RandomStream;

	UE_LOG(LogLootNPop, Log, TEXT("LNPMassSpawnSubsystem: Launching async queue build."));

	SpawnBuildFuture = Async(EAsyncExecution::TaskGraph,
		[this,
		 Sets    = MoveTemp(SetParams),
		 Cache   = MoveTemp(CacheSnap),
		 Rand,
		 SR      = SphereRadius,
		 MinDist,
		 EnemyRadius,
		 MaxRetry]() mutable
		{
			TArray<FVector> OccupiedPods;
			TArray<FLNPAsyncSpawnEntry> Results;

			// FVector::DownVector(0,0,-1) 기준 10도 이내 영역 제외 (PlayerStart 배치 영역)
			const float CosDownExclude = FMath::Cos(FMath::DegreesToRadians(10.0f));

			for (const auto& Set : Sets)
			{
				for (int32 i = 0; i < Set.PodSetCount; ++i)
				{
					// Cache 조회 + 최소 거리 체크로 LootPod 표면 지점 탐색
					FVector PodLocation;
					bool bFoundPod = false;
					FVector BaseDir = Rand.GetUnitVector();//FVector::DownVector;

					for (int32 Retry = 0; Retry < MaxRetry; ++Retry)
					{
						FVector SearchDir = (Retry == 0) ? BaseDir : (BaseDir + Rand.GetUnitVector() * 0.05f).GetSafeNormal();
						FVector Candidate;
						if (!Cache.GetPoint(SearchDir, Candidate))
							continue;

						// FVector::DownVector 10도 이내 제외
						if (FVector::DotProduct(Candidate.GetSafeNormal(), FVector::DownVector) > CosDownExclude)
							continue;

						if (MinDist > 0.0f)
						{
							bool bTooClose = false;
							for (const FVector& Occ : OccupiedPods)
							{
								if (FVector::DistSquared(Candidate, Occ) < FMath::Square(MinDist))
								{
									bTooClose = true;
									break;
								}
							}
							if (bTooClose)
								continue;
						}

						PodLocation = Candidate;
						bFoundPod = true;
						break;
					}

					if (!bFoundPod)
						continue;

					OccupiedPods.Add(PodLocation);
					TSharedPtr<FLNPSpawnLink> SpawnLink = MakeShared<FLNPSpawnLink>();
					SpawnLink->PodLocation = PodLocation;

					// Pod 항목
					FLNPAsyncSpawnEntry PodEntry;
					PodEntry.RequestType = ELNPSpawnRequestType::LootPod;
					PodEntry.AssetIndex  = Set.PodAssetIndex;
					PodEntry.SpawnLink   = SpawnLink;
					FVector PodUp = -PodLocation.GetSafeNormal();
					PodEntry.Transforms.Add(FTransform(UKismetMathLibrary::MakeRotFromZ(PodUp), PodLocation));
					Results.Add(MoveTemp(PodEntry));

					// Enemy 항목
					const FVector PodNormal = PodLocation.GetSafeNormal();
					for (const auto& EnemySet : Set.Enemies)
					{
						FLNPAsyncSpawnEntry EnemyEntry;
						EnemyEntry.RequestType = ELNPSpawnRequestType::Enemy;
						EnemyEntry.AssetIndex  = EnemySet.AssetIndex;
						EnemyEntry.SpawnLink   = SpawnLink;

						TArray<FVector> BatchLocations;
						for (int32 j = 0; j < EnemySet.Count; ++j)
						{
							FVector EnemyPos;
							bool bFoundSpot = false;

							for (int32 ERetry = 0; ERetry < 10; ++ERetry)
							{
								FVector Tangent = Rand.GetUnitVector();
								Tangent = FVector::VectorPlaneProject(Tangent, PodNormal).GetSafeNormal();
								float Dist = Rand.FRandRange(400.0f, EnemyRadius);
								FVector EDir = (PodNormal + (Tangent * (Dist / SR))).GetSafeNormal();

								if (!Cache.GetPoint(EDir, EnemyPos))
									continue;

								bool bTooClose = false;
								for (const FVector& OtherPos : BatchLocations)
								{
									if (FVector::DistSquared(EnemyPos, OtherPos) < FMath::Square(200.0f))
									{
										bTooClose = true;
										break;
									}
								}
								if (!bTooClose)
								{
									bFoundSpot = true;
									break;
								}
							}

							if (bFoundSpot)
							{
								BatchLocations.Add(EnemyPos);
								FVector EUp = -EnemyPos.GetSafeNormal();
								EnemyEntry.Transforms.Add(FTransform(UKismetMathLibrary::MakeRotFromZ(EUp), EnemyPos));
							}
						}

						if (EnemyEntry.Transforms.Num() > 0)
							Results.Add(MoveTemp(EnemyEntry));
					}
				}
			}

			// TFuture 완료가 이 쓰기에 대한 happens-before 보장을 제공함
			AsyncBuildResult = MoveTemp(Results);
		});
}

void ULNPMassSpawnSubsystem::AssembleSpawnQueueFromAsyncResult()
{
	SpawnQueue.Empty();
	SpawnQueue.Reserve(AsyncBuildResult.Num());

	for (FLNPAsyncSpawnEntry& Entry : AsyncBuildResult)
	{
		if (!CapturedAssets.IsValidIndex(Entry.AssetIndex))
			continue;

		FLNPMassSpawnRequest Req;
		Req.ConfigAsset     = CapturedAssets[Entry.AssetIndex];
		Req.TargetTransforms = MoveTemp(Entry.Transforms);
		Req.RequestType     = Entry.RequestType;
		Req.SpawnLink       = Entry.SpawnLink;
		SpawnQueue.Add(MoveTemp(Req));
	}

	AsyncBuildResult.Empty();
	CapturedAssets.Empty();
	SpawnQueueHead = 0;

	UE_LOG(LogLootNPop, Log, TEXT("LNPMassSpawnSubsystem: Assembled %d spawn requests from async build."), SpawnQueue.Num());
}

void ULNPMassSpawnSubsystem::ProcessQueue()
{
	if (SpawnQueue.Num() <= SpawnQueueHead  || ActiveConfig == nullptr)
		return;

	UMassSpawnerSubsystem* SpawnerSubsystem = UWorld::GetSubsystem<UMassSpawnerSubsystem>(GetWorld());
	if (SpawnerSubsystem == nullptr)
		return;

	UWorld* World = GetWorld();
	check(World);

	int32 SpawnsThisFrame = 0;
	int32 MaxPerFrame = ActiveConfig->MaxSpawnsPerFrame;

	while (SpawnQueueHead < SpawnQueue.Num() && SpawnsThisFrame < MaxPerFrame)
	{
		FLNPMassSpawnRequest& Request = SpawnQueue[SpawnQueueHead];
		
		int32 RemainingInBatch = Request.TargetTransforms.Num() - Request.ProcessedCount;
		int32 ToSpawn = FMath::Min(RemainingInBatch, MaxPerFrame - SpawnsThisFrame);

		if (ToSpawn > 0)
		{
			TArray<FTransform> Slice;
			for (int32 i = 0; i < ToSpawn; ++i)
			{
				Slice.Add(Request.TargetTransforms[Request.ProcessedCount + i]);
			}

			if (const UMassEntityConfigAsset* EntityConfig = Request.ConfigAsset)
			{
				const FMassEntityTemplate& EntityTemplate = EntityConfig->GetOrCreateEntityTemplate(*World);
				if (EntityTemplate.IsValid())
				{
					TArray<FMassEntityHandle> OutEntities;
					SpawnerSubsystem->SpawnEntities(EntityTemplate, ToSpawn, OutEntities);
					
					// Pod이면 Enqueue 로직상 한 번에 하나만 스폰해야 함
					if (Request.RequestType == ELNPSpawnRequestType::LootPod && OutEntities.Num() > 0)
					{
						Request.SpawnLink->PodHandle = OutEntities[0];
					}

					FMassEntityHandle ParentPod;
					FVector ParentLoc = FVector::ZeroVector;
					if (Request.SpawnLink.IsValid())
					{
						ParentPod = Request.SpawnLink->PodHandle;
						ParentLoc = Request.SpawnLink->PodLocation;
					}

					SetupSpawnedEntities(OutEntities, Slice, ParentPod, ParentLoc);
				}
			}

			Request.ProcessedCount += ToSpawn;
			SpawnsThisFrame += ToSpawn;
		}

		if (Request.IsComplete())
		{
			++SpawnQueueHead;
		}
	}

	// 모두 처리 완료: 메모리 해제 및 알림
	if (SpawnQueueHead >= SpawnQueue.Num())
	{
		SpawnQueue.Empty();
		SpawnQueueHead = 0;
		ActiveConfig = nullptr;
		UE_LOG(LogLootNPop, Log, TEXT("LNPMassSpawnSubsystem: All entities spawned."));
		OnSpawningComplete.Broadcast();
	}
}

void ULNPMassSpawnSubsystem::SetupSpawnedEntities(TConstArrayView<FMassEntityHandle> Entities, TConstArrayView<FTransform> Transforms, FMassEntityHandle ParentLootPod, const FVector& ParentPodLocation)
{
	UWorld* World = GetWorld();
	check(World);

	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(*World);

	int32 NumToProcess = FMath::Min(Entities.Num(), Transforms.Num());
	for (int32 i = 0; i < NumToProcess; ++i)
	{
		const FMassEntityHandle Entity = Entities[i];

		// 1. Transform 설정
		if (FTransformFragment* TransformFragment = EntityManager.GetFragmentDataPtr<FTransformFragment>(Entity))
		{
			TransformFragment->SetTransform(Transforms[i]);
		}

		// 2. Leash 메타데이터 설정 (Enemy이고 부모가 유효한 경우)
		if (ParentLootPod.IsValid())
		{
			if (FLNPEnemyFragment* EnemyFragment = EntityManager.GetFragmentDataPtr<FLNPEnemyFragment>(Entity))
			{
				EnemyFragment->ParentLootPod = ParentLootPod;
				EnemyFragment->ParentPodLocation = ParentPodLocation;
			}
		}
	}
	
	UE_LOG(LogLootNPop, Verbose, TEXT("LNPMassSpawnSubsystem: Initialized %d spawned entities."), NumToProcess);
}
