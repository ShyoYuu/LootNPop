// Copyright (c) 2026 LootNPop. All rights reserved.

#include "GameLogic/LNPMassSpawnSubsystem.h"
#include "GameLogic/LNPSurfaceCacheSubsystem.h"
#include "DataAsset/LNPMassSpawnConfig.h"
#include "Config/LNPSettings.h"
#include "Enemy/LNPEnemyMassTypes.h"
#include "LootPod/LNPLootPodMassTypes.h"
#include "Replication/LNPMassReplication.h"
#include "LootNPop.h"

#include "Async/Async.h"

#include "MassEntityConfigAsset.h"
#include "MassAgentComponent.h"
#include "MassSpawnerSubsystem.h"
#include "MassEntityManager.h"
#include "MassEntitySubsystem.h"
#include "MassObserverManager.h"
#include "MassProcessor.h"
#include "MassReplicationProcessor.h"
#include "GameFramework/Pawn.h"
#include "MassCommonFragments.h"
#include "MassReplicationSubsystem.h"
#include "MassReplicationFragments.h"
#include "MassEntityTemplate.h"
#include "Engine/World.h"
#include "Kismet/KismetMathLibrary.h"

void ULNPMassSpawnSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	RandomStream.GenerateNewSeed();

	// MassReplication(Phase 6/6.5/7): 클라이언트가 접속하기 전에 BubbleInfoClass를 등록해야 한다 (RegisterBubbleInfoClass 문서 제약).
	// 모든 복제 타입(Enemy·Player·LootPod)이 통합 버블 하나를 공유한다 — 다중 버블은 엔진 파괴 경로가
	// 타입 무구분이라 크래시/오염을 유발한다 (EngineAnalysis_MassReplication.md §7.1).
	if (UMassReplicationSubsystem* ReplicationSubsystem = GetWorld()->GetSubsystem<UMassReplicationSubsystem>())
	{
		ReplicationSubsystem->RegisterBubbleInfoClass(ALNPMassClientBubbleInfo::StaticClass());
	}
}

void ULNPMassSpawnSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// Enemy MassReplication(Phase 6): Enemy 스폰(BeginSpawning)은 서버 전용 ALNPGameMode가 호출하므로
	// 클라이언트는 EnemyEntityConfig의 FMassEntityTemplate을 로컬 TemplateRegistry에 등록할 기회가 전혀 없다.
	// Bubble이 복제 스폰(SpawnEntities by TemplateID) 시도 시 "TemplateID must have been registered!"로 죽는 것을 막기 위해,
	// 실제 스폰 없이 템플릿만 서버·클라이언트 양쪽에서 미리 등록(warm-up)한다.
	//
	// 템플릿 warm-up은 Initialize()가 아니라 반드시 여기(OnWorldBeginPlay)에서 해야 한다.
	// UEngine::LoadMap은 InitWorld() -> Listen(URL) 순서로 진행하고, UWorld::AttemptDeriveFromURL()은
	// 현재 실행 URL의 ?Listen 옵션을 보지 않는다. 따라서 -game 리슨 서버는 Initialize() 시점에
	// NM_Standalone을 반환하고, UMassReplicationTrait::BuildTemplate이 조기 반환해 복제 프래그먼트가
	// 빠진 템플릿이 월드 수명 내내 캐시된다 (ConfigGuid 키 캐시라 Listen 이후에도 소급 수정이 불가능하다).
	// 그 결과 게스트 버블이 영원히 비어 Low LOD 엔티티가 클라이언트에 나타나지 않는다.
	// PIE는 PlayInEditorNetMode 폴백 덕에 이 문제가 드러나지 않아 -game에서만 재현된다.
	// OnWorldBeginPlay는 NetDriver 생성 이후이자 GameMode::StartPlay 이전이라 두 조건을 모두 만족한다.
	const ENetMode NetMode = InWorld.GetNetMode();
	UE_LOG(LogLootNPop, Log, TEXT("Mass entity template warm-up starting. NetMode=%d"), static_cast<int32>(NetMode));

	RestoreServerOnlyMassObservers(InWorld, NetMode);

	if (const ULNPSettings* Settings = GetDefault<ULNPSettings>())
	{
		if (const ULNPMassSpawnConfig* Config = Settings->MassSpawnConfig.LoadSynchronous())
		{
			for (const FLNPLootPodSpawnEntry& PodEntry : Config->LootPodSpawnSets)
			{
				// LootPod MassReplication(Phase 7): Pod 템플릿도 클라이언트 warm-up 필요 (Enemy와 동일한 이유)
				if (UMassEntityConfigAsset* PodConfig = PodEntry.LootPodEntityConfig)
				{
					const FMassEntityTemplate& PodTemplate = PodConfig->GetOrCreateEntityTemplate(InWorld);

					// 복제 프래그먼트 누락은 크래시도 경고도 없는 무음 실패라 PIE로는 잡히지 않는다. 트립와이어를 남긴다.
					if (NetMode != NM_Standalone && !PodTemplate.GetCompositionDescriptor().Contains<FMassNetworkIDFragment>())
					{
						UE_LOG(LogLootNPop, Error, TEXT("LootPod entity template %s was built without replication fragments (NetMode=%d). Mass entities will never reach clients."), *GetNameSafe(PodConfig), static_cast<int32>(NetMode));
					}
				}

				for (const FLNPEnemySpawnEntry& EnemyEntry : PodEntry.AssociatedEnemies)
				{
					if (UMassEntityConfigAsset* EnemyConfig = EnemyEntry.EnemyEntityConfig)
						EnemyConfig->GetOrCreateEntityTemplate(InWorld);
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
				AgentComp->GetEntityConfig().GetOrCreateEntityTemplate(InWorld);
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

void ULNPMassSpawnSubsystem::RestoreServerOnlyMassObservers(UWorld& InWorld, const ENetMode NetMode)
{
	// 바로 위 템플릿 warm-up과 **같은 원인의 두 번째 증상**을 고친다.
	//
	// FMassObserverManager::Initialize()는 월드 서브시스템 초기화 시점의 GetNetMode()를 딱 한 번 읽어
	// (MassObserverManager.cpp의 DetermineProcessorExecutionFlags) 그 실행 플래그에 맞는 옵저버만
	// 인스턴스화한다. -game 리슨 서버는 그 시점에 NM_Standalone이고, Standalone 플래그는 Server를
	// 포함하지 않으므로 EProcessorExecutionFlags::Server 전용인
	// UMassReplicationEntityDestructionObserver가 통째로 빠진다.
	//
	// 빠지면 엔티티 파괴가 UMassReplicationSubsystem::NotifyEntityDestroyed로 통지되지 않아
	// bPendingDestruction이 서지 않고, 따라서 ULNPMassReplicator의 RemoveEntityCallback이 한 번도
	// 호출되지 않는다 → 클라이언트 버블에서 에이전트가 영원히 제거되지 않는다.
	// 증상은 **게스트 화면에만** 죽은 NPC와 루팅이 끝난 LootPod이 남는 것이다. 호스트는 자기 엔티티를
	// 직접 파괴하므로 멀쩡해 보여, 호스트만 확인하면 놓친다.
	// (2026-08-20 2P 실측: 호스트 RemoveEntityCallback 0건, 게스트 수신 0건, 에이전트 235개 누적)
	//
	// 매니저를 통째로 다시 세우는 편이 일반적이지만 FMassObserverManager::Initialize/DeInitialize는
	// protected(friend FMassEntityManager)라 호출할 수 없다. 다행히 이 프로젝트가 쓰는 MassGameplay
	// 플러그인 전체에서 Server/Client로 좁혀진 프로세서는 이 옵저버 하나뿐이므로
	// (MassReplicationProcessor.cpp:384), 이 하나를 되살리면 노출 범위가 전부 덮인다.
	// ⚠️ 엔진 업데이트로 Server 전용 옵저버가 늘어나면 여기도 같이 늘려야 한다.
	if (NetMode != NM_ListenServer && NetMode != NM_DedicatedServer)
	{
		return;
	}

	UMassEntitySubsystem* EntitySubsystem = InWorld.GetSubsystem<UMassEntitySubsystem>();
	if (EntitySubsystem == nullptr)
	{
		return;
	}

	FMassObserverManager& ObserverManager = EntitySubsystem->GetMutableEntityManager().GetObserverManager();

#if WITH_MASSENTITY_DEBUG
	// 초기화 시점에 이미 올바른 넷 모드를 본 구성(PIE 등)에서는 옵저버가 정상 등록돼 있다.
	// NotifyEntityDestroyed는 멱등이라 중복돼도 결과가 틀어지진 않지만, 헛도는 순회를 만들 이유는 없다.
	TArray<const UMassProcessor*> Existing;
	ObserverManager.DebugGatherUniqueProcessors(Existing);
	for (const UMassProcessor* Processor : Existing)
	{
		if (Processor && Processor->IsA<UMassReplicationEntityDestructionObserver>())
		{
			UE_LOG(LogLootNPop, Verbose,
				TEXT("[MassObserver] Destruction observer already registered (NetMode=%d) — nothing to restore."),
				static_cast<int32>(NetMode));
			return;
		}
	}
#endif // WITH_MASSENTITY_DEBUG

	// AddObserverInstance가 ObservedTypes/ObservedOperations를 읽고 CallInitialize까지 수행한다.
	UMassReplicationEntityDestructionObserver* Observer = NewObject<UMassReplicationEntityDestructionObserver>(this);
	ObserverManager.AddObserverInstance(Observer);

	UE_LOG(LogLootNPop, Log,
		TEXT("[MassObserver] Restored UMassReplicationEntityDestructionObserver (NetMode=%d) — it was filtered out at world init while GetNetMode() still reported Standalone."),
		static_cast<int32>(NetMode));
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
					FVector BaseDir = Rand.GetUnitVector();

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

		// 3. LootPod 고유 ID 발급 — 트레잇은 템플릿을 만들어 모든 Pod가 공유하므로 ID는 여기서
		// 엔티티마다 부여해야 한다. ULNPLootDiceRewardTable.RewardsByPodID 조회 키로 쓰인다.
		if (FLNPLootPodFragment* PodFragment = EntityManager.GetFragmentDataPtr<FLNPLootPodFragment>(Entity))
		{
			PodFragment->PodID = NextPodID++;
		}
	}
	
	UE_LOG(LogLootNPop, Log, TEXT("LNPMassSpawnSubsystem: Initialized %d spawned entities."), NumToProcess);
}
