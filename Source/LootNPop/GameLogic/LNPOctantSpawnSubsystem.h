// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "DataAsset/LNPOctantDefinition.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "LNPOctantSpawnSubsystem.generated.h"

class ULNPOctantPoolData;
class ALevelInstance;
class ULevel;

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

	/** 생성 완료 뒤 slot의 Level Instance 내부 레벨. 미완료·언로드면 nullptr. */
	ULevel* GetSlotLevel(int32 SlotIndex) const;

	/** 레벨이 속한 slot. 옥탄트 레벨이 아니면 INDEX_NONE. exact hit의 component 레벨로 slot을 찾는 용도다. */
	int32 FindSlotForLevel(const ULevel* Level) const;

	/** 모든 Octant가 스폰되고 완전히 로드됐을 때 발동하는 이벤트. */
	UPROPERTY(BlueprintAssignable, Category = "LNP|World Generation")
	FLNPOnWorldGenerationFinished OnWorldGenerationFinished;

	/** 월드 생성 완료 여부. GameState가 클라이언트 베이킹 시작 조건(투-게이트)으로 참조한다. */
	bool bGenerationComplete = false;

	/** slot 순서의 Level Instance 회전. 8-slot exact oracle이 같은 로컬 방향을 slot별로 돌려 비교한다. */
	static const FRotator OctantRotations[8];

private:

	bool bIsGenerating = false;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ALevelInstance>> SpawnedOctants;

	/**
	 * 완료 시점에 확정한 slot 순서의 Level Instance와 내부 레벨. match 동안 보존한다.
	 * 월드 Actor 검색이나 회전값 추론으로 slot을 복원하지 않기 위한 원본이다.
	 */
	TArray<TWeakObjectPtr<ALevelInstance>> SlotLevelInstances;
	TArray<TWeakObjectPtr<ULevel>> SlotLevels;

	/** SurfaceData를 포함한 정의 전체를 slot 순서로 보존한다. */
	TArray<FLNPOctantDefinition> SelectedOctantDefinitions;
};
