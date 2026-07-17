// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "LNPLootDiceRewardTable.generated.h"

class ULNPItemDefinitionBase;

/** 한 Pod가 Pop할 때 스폰되는 Dice 목록 — 엔트리 1개 = Dice 1개 */
USTRUCT(BlueprintType)
struct FLNPLootDiceRewardSet
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LNP|LootDice")
	TArray<TObjectPtr<ULNPItemDefinitionBase>> Items;
};

/**
 * LootPod 보상 테이블 — PodID로 보상 목록을 조회한다 (GameDesign_LootPod.md §3:
 * 보상 종류는 코드 수정 없이 데이터 에셋으로 관리).
 * LNPSettings.LootDiceRewardTable에 지정하며, ALNPLootDice::SpawnPodRewards가 조회한다.
 * 확장 지점: 가중치 랜덤 선정이 필요해지면 엔트리에 확률 필드를 추가한다 (기획 미확정으로 보류).
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
