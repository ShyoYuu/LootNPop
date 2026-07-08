// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "LNPGameState.generated.h"

UENUM(BlueprintType)
enum class ELNPInitPhase : uint8
{
	WorldGeneration  UMETA(DisplayName = "World Generation"),
	SurfaceBaking    UMETA(DisplayName = "Surface Baking"),
	EntitySpawning   UMETA(DisplayName = "Entity Spawning"),
	Complete         UMETA(DisplayName = "Complete"),
};

/**
 * 서버 초기화 단계(ServerPhase)와 월드 생성 시드(OctantGenSeed)를 클라이언트에 복제하는 GameState.
 * 클라이언트 측 초기화(옥탄트 월드 생성 → 표면 베이킹)는 각 OnRep 콜백에서 트리거되며,
 * 서버 페이즈 신호와 로컬 월드 생성 완료가 모두 충족되어야 베이킹을 시작한다 (투-게이트 패턴).
 */
UCLASS()
class LOOTNPOP_API ALNPGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	ALNPGameState();

	/** 모든 Player의 구형 중력 모드 전환을 위한 전역 플래그 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "LNP|World Settings")
	bool bIsSphereWorld = false;

	/** 결정론적 World 생성 Seed. 시작 시 GameMode가 설정; 0은 랜덤 (서버 측에서 결정). */
	UPROPERTY(Config, EditAnywhere, ReplicatedUsing=OnRep_OctantGenSeed, Category = "LNP|World Settings")
	int32 OctantGenSeed = 0;

	/** PCG 생성 일관성을 위한 전역 Seed 오프셋 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "LNP|PCG")
	int32 PCGSeedOffset = 0;

	/** 서버 권한 초기화 단계, 클라이언트가 반응할 수 있도록 복제됨 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_ServerPhase, Category = "LNP|Init")
	ELNPInitPhase ServerPhase = ELNPInitPhase::WorldGeneration;

	// 복제 설정
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UFUNCTION()
	void OnRep_OctantGenSeed();

	UFUNCTION()
	void OnRep_ServerPhase();

	/** 클라이언트에서 Octant 로딩이 완료되면 발동한다. 서버 단계가 이미 진행됐으면 베이킹을 트리거한다. */
	UFUNCTION()
	void OnClientWorldGenerationFinished();

private:
	void TryBeginClientBaking();
};
