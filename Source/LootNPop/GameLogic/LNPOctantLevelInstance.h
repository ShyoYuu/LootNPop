// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "LevelInstance/LevelInstanceActor.h"
#include "LNPOctantLevelInstance.generated.h"

/**
 * 런타임에 스폰하는 Octant Level Instance.
 * 엔진의 ALevelInstance::SetWorldAsset은 WITH_EDITOR 전용이라 패키지 빌드에서는
 * ILevelInstanceInterface의 기본 구현(아무것도 하지 않고 false 반환)으로 떨어진다.
 * 월드 에셋이 비면 RequestLoadLevelInstance가 조용히 무시되어 IsLoaded()가 영원히 false가 된다.
 * 패키지 빌드에서는 GetWorldAsset()이 읽는 CookedWorldAsset을 직접 채운다.
 */
UCLASS()
class LOOTNPOP_API ALNPOctantLevelInstance : public ALevelInstance
{
	GENERATED_BODY()

public:
	/** 에디터·패키지 빌드 모두에서 로드할 월드 에셋을 지정한다. 이후 LoadLevelInstance()를 호출해야 로드가 시작된다. */
	bool SetRuntimeWorldAsset(const TSoftObjectPtr<UWorld>& InWorldAsset);
};
