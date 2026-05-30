// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "LNPOctantSpawnSubsystem.generated.h"

class ULNPOctantPoolData;
class ALevelInstance;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FLNPOnWorldGenerationFinished);

/**
 * Octant Level Instance를 스폰하여 구형 세계를 생성하는 Subsystem.
 * 완료 이벤트를 Broadcast하기 전에 모든 인스턴스가 완전히 로드될 때까지 대기한다.
 */
UCLASS()
class LOOTNPOP_API ULNPOctantSpawnSubsystem : public UWorldSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	// FTickableGameObject interface
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override { return bIsGenerating; }
	virtual TStatId GetStatId() const override;
	// End FTickableGameObject

	/** World 생성 프로세스를 시작한다. */
	UFUNCTION(BlueprintCallable, Category = "LNP|World Generation")
	void StartWorldGeneration();

	/** 모든 Octant가 스폰되고 완전히 로드됐을 때 발동하는 이벤트. */
	UPROPERTY(BlueprintAssignable, Category = "LNP|World Generation")
	FLNPOnWorldGenerationFinished OnWorldGenerationFinished;

	bool bGenerationComplete = false;

private:
	static const FRotator OctantRotations[8];

	bool bIsGenerating = false;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ALevelInstance>> SpawnedOctants;
};
