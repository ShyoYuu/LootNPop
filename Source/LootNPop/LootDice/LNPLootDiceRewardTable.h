// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "LNPLootDiceRewardTable.generated.h"

class ULNPItemDefinitionBase;

/** 보상 후보 1종 — 가중 추첨에 쓰이는 아이템과 비중 */
USTRUCT(BlueprintType)
struct FLNPLootDiceRewardEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LNP|LootDice")
	TObjectPtr<ULNPItemDefinitionBase> Item;

	/** 가중 추첨 비중. 0 이하면 후보에서 제외된다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LNP|LootDice", meta = (ClampMin = "0"))
	float Weight = 1.0f;
};

/**
 * 한 Pod가 Pop할 때의 보상 후보 풀 — 전체 후보를 담아 두고 그중 MinDrops~MaxDrops개만
 * 가중 추첨해 Dice로 스폰한다 (중복 허용). 보상 종류가 늘어도 한 번에 쏟아지지 않게 하는 장치.
 */
USTRUCT(BlueprintType)
struct FLNPLootDiceRewardSet
{
	GENERATED_BODY()

	/** 드랍 후보 전체 목록 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LNP|LootDice")
	TArray<FLNPLootDiceRewardEntry> Entries;

	/** 1회 Pop당 스폰할 Dice 최소 개수 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LNP|LootDice", meta = (ClampMin = "0"))
	int32 MinDrops = 3;

	/** 1회 Pop당 스폰할 Dice 최대 개수 (MinDrops 미만이면 MinDrops로 올림 처리) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LNP|LootDice", meta = (ClampMin = "0"))
	int32 MaxDrops = 4;
};

/**
 * LootPod 보상 테이블 — PodID로 보상 목록을 조회한다 (GameDesign_LootPod.md §3:
 * 보상 종류는 코드 수정 없이 데이터 에셋으로 관리).
 * LNPSettings.LootDiceRewardTable에 지정하며, ALNPLootDice::SpawnPodRewards가 조회한다.
 * 후보 전체를 등록해 두고 스폰 시 가중 추첨으로 일부만 뽑는다 (FLNPLootDiceRewardSet 참고).
 */
UCLASS(BlueprintType)
class LOOTNPOP_API ULNPLootDiceRewardTable : public UDataAsset
{
	GENERATED_BODY()

public:
	/** PodID별 보상 목록 */
	UPROPERTY(EditAnywhere, Category = "LNP|LootDice")
	TMap<int32, FLNPLootDiceRewardSet> RewardsByPodID;

	/** PodID가 테이블에 없을 때의 폴백 보상 */
	UPROPERTY(EditAnywhere, Category = "LNP|LootDice")
	FLNPLootDiceRewardSet DefaultRewards;
};
