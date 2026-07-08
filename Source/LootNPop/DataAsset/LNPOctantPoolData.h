// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "LNPOctantPoolData.generated.h"

/**
 * 월드 생성에 사용할 Octant Level Instance 후보 목록을 담는 Data Asset.
 * ULNPOctantSpawnSubsystem이 결정론적 시드로 이 풀에서 8개를 선택해 구체를 조립한다.
 */
UCLASS()
class LOOTNPOP_API ULNPOctantPoolData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Octant로 제작된 레벨 목록. 제작 절차는 Guide_OctantLevelInstance.md 참조. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|World Generation")
	TArray<TSoftObjectPtr<UWorld>> OctantPool;
};
