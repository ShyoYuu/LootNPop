// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "LNPGameMode.generated.h"

class ALNPPlayerController;

UCLASS()
class LOOTNPOP_API ALNPGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ALNPGameMode();

	virtual void BeginPlay() override;

	/** 서버 초기화 완료 전까지 Pawn 스폰을 차단한다 */
	virtual void RestartPlayer(AController* NewPlayer) override;

	/**
	 * 항상 false — 매 스폰이 ChoosePlayerStart의 랜덤 추첨을 타게 한다.
	 *
	 * 엔진 기본 구현은 `Player->StartSpot != nullptr`이면 true를 반환하고,
	 * `AGameModeBase::FindPlayerStart_Implementation`이 그 StartSpot을 그대로 돌려주므로
	 * 랜덤 선택 코드에 **영영 도달하지 못한다**. 첫 스폰 때 `InitStartSpot`이 StartSpot을 채우기 때문에
	 * 이 오버라이드가 없으면 사망 리스폰이 항상 같은 지점으로 간다.
	 */
	virtual bool ShouldSpawnAtStartSpot(AController* Player) override { return false; }

	/** 클라이언트의 로컬 베이킹이 완료되면 RPC를 통해 호출된다 */
	void OnClientReady(ALNPPlayerController* PC);

	/** 사망한 컨트롤러의 리스폰을 Delay 초 뒤로 예약한다. 서버 전용. */
	void ScheduleRespawn(AController* DeadController, float Delay);

private:
	/** 랙돌 폰을 정리하고 랜덤 PlayerStart로 되살린다. */
	void DoRespawn(TWeakObjectPtr<AController> WeakController);

	UFUNCTION()
	void OnWorldGenerationComplete();

	UFUNCTION()
	void OnSurfaceBakingComplete();

	UFUNCTION()
	void OnEntitySpawningComplete();

	bool bServerInitComplete = false;
	TArray<TWeakObjectPtr<AController>> PendingPlayers;
	/** 사망 대기 중인 컨트롤러별 리스폰 타이머. */
	TMap<TWeakObjectPtr<AController>, FTimerHandle> RespawnTimers;
	/** ServerNotifyClientReady() RPC를 수신한 클라이언트 집합 — 폰 스폰의 두 번째 게이트 */
	TSet<TWeakObjectPtr<ALNPPlayerController>> ReadyClients;
};
