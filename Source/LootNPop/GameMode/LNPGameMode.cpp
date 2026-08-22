// Copyright (c) 2026 LootNPop. All rights reserved.

#include "GameMode/LNPGameMode.h"
#include "GameMode/LNPGameState.h"
#include "Player/LNPPlayerState.h"
#include "GameLogic/LNPOctantSpawnSubsystem.h"
#include "GameLogic/LNPSurfaceCacheSubsystem.h"
#include "GameLogic/LNPMassSpawnSubsystem.h"
#include "Player/LNPPlayerController.h"
#include "GAS/Attributes/LNPBaseAttributeSet.h"
#include "LootNPop.h"

#include "AbilitySystemComponent.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"

ALNPGameMode::ALNPGameMode()
{
	GameStateClass = ALNPGameState::StaticClass();
	PlayerControllerClass = ALNPPlayerController::StaticClass();
	PlayerStateClass = ALNPPlayerState::StaticClass();
}

void ALNPGameMode::BeginPlay()
{
	Super::BeginPlay();

	UWorld* World = GetWorld();
	check(World);

	ALNPGameState* GS = World->GetGameState<ALNPGameState>();
	check(GS);

	// Seed 결정: config로 설정되지 않았으면 지금 생성하여 모든 클라이언트가 동일한 값을 사용
	if (GS->OctantGenSeed == 0)
	{
		GS->OctantGenSeed = FMath::Rand();
	}

	GS->ServerPhase = ELNPInitPhase::WorldGeneration;

	if (ULNPOctantSpawnSubsystem* OctantSub = World->GetSubsystem<ULNPOctantSpawnSubsystem>())
	{
		OctantSub->OnWorldGenerationFinished.AddDynamic(this, &ALNPGameMode::OnWorldGenerationComplete);
		OctantSub->StartWorldGeneration();
	}
}

void ALNPGameMode::OnWorldGenerationComplete()
{
	UWorld* World = GetWorld();
	ALNPGameState* GS = World->GetGameState<ALNPGameState>();
	GS->ServerPhase = ELNPInitPhase::SurfaceBaking;

	if (ULNPSurfaceCacheSubsystem* SurfaceSub = World->GetSubsystem<ULNPSurfaceCacheSubsystem>())
	{
		SurfaceSub->OnBakingComplete.AddDynamic(this, &ALNPGameMode::OnSurfaceBakingComplete);
		SurfaceSub->BeginBaking();
	}
}

void ALNPGameMode::OnSurfaceBakingComplete()
{
	UWorld* World = GetWorld();
	ALNPGameState* GS = World->GetGameState<ALNPGameState>();
	GS->ServerPhase = ELNPInitPhase::EntitySpawning;

	if (ULNPMassSpawnSubsystem* SpawnSub = World->GetSubsystem<ULNPMassSpawnSubsystem>())
	{
		SpawnSub->OnSpawningComplete.AddDynamic(this, &ALNPGameMode::OnEntitySpawningComplete);
		SpawnSub->BeginSpawning();
	}
}

void ALNPGameMode::OnEntitySpawningComplete()
{
	UWorld* World = GetWorld();
	ALNPGameState* GS = World->GetGameState<ALNPGameState>();
	GS->ServerPhase = ELNPInitPhase::Complete;
	bServerInitComplete = true;

	// 클라이언트 준비까지 완료된 플레이어만 즉시 스폰. 미완료 플레이어는 OnClientReady에서 처리.
	TArray<TWeakObjectPtr<AController>> StillPending;
	int32 SpawnedCount = 0;
	for (TWeakObjectPtr<AController>& Ctrl : PendingPlayers)
	{
		if (!Ctrl.IsValid()) continue;
		ALNPPlayerController* PC = Cast<ALNPPlayerController>(Ctrl.Get());
		if (!PC || ReadyClients.Contains(PC))
		{
			Super::RestartPlayer(Ctrl.Get());
			++SpawnedCount;
		}
		else
		{
			StillPending.Add(Ctrl);
		}
	}
	PendingPlayers = MoveTemp(StillPending);

	UE_LOG(LogLootNPop, Log, TEXT("ALNPGameMode: Server init complete. Spawned %d players, %d awaiting client ready."),
		SpawnedCount, PendingPlayers.Num());
}

void ALNPGameMode::RestartPlayer(AController* NewPlayer)
{
	if (!bServerInitComplete)
	{
		PendingPlayers.AddUnique(NewPlayer);
		return;
	}
	// 서버 초기화 완료 후에도 해당 클라이언트가 로컬 베이킹을 끝낼 때까지 대기
	ALNPPlayerController* PC = Cast<ALNPPlayerController>(NewPlayer);
	if (PC && !ReadyClients.Contains(PC))
	{
		PendingPlayers.AddUnique(NewPlayer);
		return;
	}
	Super::RestartPlayer(NewPlayer);
}

void ALNPGameMode::ScheduleRespawn(AController* DeadController, float Delay)
{
	if (DeadController == nullptr)
		return;

	const TWeakObjectPtr<AController> WeakController(DeadController);
	FTimerHandle& Handle = RespawnTimers.FindOrAdd(WeakController);
	GetWorldTimerManager().SetTimer(Handle,
		FTimerDelegate::CreateUObject(this, &ALNPGameMode::DoRespawn, WeakController), Delay, false);
}

void ALNPGameMode::DoRespawn(TWeakObjectPtr<AController> WeakController)
{
	RespawnTimers.Remove(WeakController);

	AController* Controller = WeakController.Get();
	if (Controller == nullptr)
		return;

	// 랙돌 폰을 반드시 치운다 — AGameModeBase::RestartPlayerAtPlayerStart는 컨트롤러가 폰을 갖고 있으면
	// 스폰 분기를 건너뛴다. UnPossess를 먼저 부르는 이유: 그냥 Destroy하면 APawn::Destroyed 경로가
	// 컨트롤러를 Inactive 상태로 밀어 넣는다.
	if (APawn* OldPawn = Controller->GetPawn())
	{
		Controller->UnPossess();
		OldPawn->Destroy();
	}

	// ASC는 PlayerState 소유라 폰을 넘어 살아남는다 — HP가 0인 채로 남아 있으므로 직접 되돌린다.
	// 새 폰의 PossessedBy가 사망 델리게이트를 다시 걸기 전에 복구해야 "부활 즉시 재사망"을 피한다.
	if (const ALNPPlayerState* PS = Controller->GetPlayerState<ALNPPlayerState>())
	{
		if (UAbilitySystemComponent* ASC = PS->GetAbilitySystemComponent())
		{
			const float MaxHealth = ASC->GetNumericAttribute(ULNPBaseAttributeSet::GetMaxHealthAttribute());
			ASC->SetNumericAttributeBase(ULNPBaseAttributeSet::GetHealthAttribute(), MaxHealth);
		}
	}

	// ShouldSpawnAtStartSpot()가 false이므로 ChoosePlayerStart의 랜덤 추첨을 탄다.
	// 새 폰의 PossessedBy가 EnsureDefaultWeapon()을 다시 부르고, 가방이 비어 있으므로
	// 기본 무기 인스턴스를 새로 만들어 장착한다.
	RestartPlayer(Controller);
}

void ALNPGameMode::OnClientReady(ALNPPlayerController* PC)
{
	ReadyClients.Add(PC);

	if (bServerInitComplete)
	{
		// 서버 완료 후 뒤늦게 준비된 클라이언트 (중간 참여 포함) — 지금 스폰
		PendingPlayers.RemoveAll([PC](const TWeakObjectPtr<AController>& Weak)
		{
			return Weak.Get() == PC;
		});
		Super::RestartPlayer(PC);
	}
}
