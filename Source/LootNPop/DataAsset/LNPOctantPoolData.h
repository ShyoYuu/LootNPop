// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "DataAsset/LNPOctantDefinition.h"
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
	/**
	 * 새 정의 목록을 반환한다. 아직 마이그레이션되지 않은 asset은 legacy Level 목록을
	 * 동일 순서와 전체 slot 허용값을 가진 임시 정의로 승격한다.
	 */
	void BuildEffectiveDefinitions(TArray<FLNPOctantDefinition>& OutDefinitions) const;

	/** Level과 SurfaceData를 함께 보존하는 새 옥탄트 정의 목록. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|World Generation")
	TArray<FLNPOctantDefinition> OctantDefinitions;

	/**
	 * Phase 2 content migration 전까지 유지하는 기존 Level 전용 목록.
	 * 런타임 선택 경로를 OctantDefinitions로 전환한 뒤 제거한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|World Generation",
		meta = (DeprecatedProperty, DeprecationMessage = "Use OctantDefinitions instead."))
	TArray<TSoftObjectPtr<UWorld>> OctantPool;
};
