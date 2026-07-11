// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "LNPLootPodSubsystem.generated.h"

class ALNPLootPod;

/**
 * 월드에 살아 있는 LootPod Actor 레지스트리.
 *
 * 상호작용 탐색용 — SmartObject 공간 쿼리는 Mass Representation의 액터 풀링(재사용·텔레포트)과
 * 충돌해 폐기했고(등록 시점 위치가 파티션에 고정됨), 월드 전체 액터 순회(TActorIterator)는 비효율적이라
 * Pod Actor가 BeginPlay/EndPlay에 스스로 등록/해제하는 목록을 쓴다. High LOD Actor는 항상 소수다.
 */
UCLASS()
class LOOTNPOP_API ULNPLootPodSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	void RegisterPod(ALNPLootPod* Pod);
	void UnregisterPod(ALNPLootPod* Pod);

	/** 살아 있는 Pod Actor 목록. 풀에 반납된(Hidden) 액터가 섞여 있을 수 있으므로 사용처에서 걸러낸다. */
	const TArray<TWeakObjectPtr<ALNPLootPod>>& GetActivePods() const { return ActivePods; }

private:
	TArray<TWeakObjectPtr<ALNPLootPod>> ActivePods;
};
