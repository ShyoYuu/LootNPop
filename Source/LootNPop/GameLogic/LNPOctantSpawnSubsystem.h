// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "DataAsset/LNPOctantDefinition.h"
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

	/**
	 * 고정 8개 slot에 허용되는 정의를 결정론적으로 선택한다.
	 * OutSourceIndices는 각 slot이 참조한 Definitions 원본 index를 같은 순서로 보존한다.
	 */
	static bool SelectOctantDefinitions(
		const TArray<FLNPOctantDefinition>& Definitions,
		int32 Seed,
		TArray<FLNPOctantDefinition>& OutSelectedDefinitions,
		TArray<int32>* OutSourceIndices = nullptr,
		FString* OutError = nullptr);

	/** Level Instance 로드 뒤에도 유지되는 8개 slot 순서의 선택 결과. */
	const TArray<FLNPOctantDefinition>& GetSelectedOctantDefinitions() const
	{
		return SelectedOctantDefinitions;
	}

	/** 모든 Octant가 스폰되고 완전히 로드됐을 때 발동하는 이벤트. */
	UPROPERTY(BlueprintAssignable, Category = "LNP|World Generation")
	FLNPOnWorldGenerationFinished OnWorldGenerationFinished;

	/** 월드 생성 완료 여부. GameState가 클라이언트 베이킹 시작 조건(투-게이트)으로 참조한다. */
	bool bGenerationComplete = false;

private:
	static const FRotator OctantRotations[8];

	bool bIsGenerating = false;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ALevelInstance>> SpawnedOctants;

	/** SurfaceData를 포함한 정의 전체를 slot 순서로 보존한다. */
	TArray<FLNPOctantDefinition> SelectedOctantDefinitions;
};
