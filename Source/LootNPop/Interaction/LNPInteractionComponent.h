// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LNPInteractionComponent.generated.h"

class ALNPLootPod;

/**
 * ALNPLootPod Actor를 탐색하고 상호작용하는 Component.
 */
UCLASS(ClassGroup = (LNP), meta = (BlueprintSpawnableComponent))
class LOOTNPOP_API ULNPInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	ULNPInteractionComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** 주변에서 최선의 상호작용 타겟을 지속적으로 탐색한다 */
	void UpdateInteractionCandidate();

	/** 현재 후보와 실제 상호작용을 시작한다 */
	void PerformInteraction();

	/** 현재 강조된 모든 상호작용 타겟을 반환한다 (UI용) */
	UFUNCTION(BlueprintPure, Category = "LNP|Interaction")
	TArray<AActor*> GetInteractionCandidates() const;

	/** 첫 번째 유효한 상호작용 타겟을 반환한다 (하위 호환 또는 단순 로직용) */
	UFUNCTION(BlueprintPure, Category = "LNP|Interaction")
	AActor* GetFirstInteractionCandidate() const;

protected:
	/** 상호작용 가능한 오브젝트 탐색 거리 */
	UPROPERTY(EditAnywhere, Category = "LNP|Interaction")
	float InteractionRadius = 500.0f;

	/** 캐릭터가 현재 바라보거나 근처에 있는 LootPod/Actor (잠재적 타겟) */
	UPROPERTY(Transient)
	TSet<TWeakObjectPtr<AActor>> InteractionCandidates;
};

