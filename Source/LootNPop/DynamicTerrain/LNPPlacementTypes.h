// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "LNPPlacementTypes.generated.h"

class ALNPPlacementMarker;

/**
 * 마커로 배치한 동적 요소의 런타임 식별자 `(slot, MarkerId)`(D-026).
 * 같은 LVI가 여러 slot에 들어가도 slot으로 구분된다. persistent level의 마커는 Slot = INDEX_NONE이다.
 */
USTRUCT()
struct FLNPPlacementId
{
	GENERATED_BODY()

	UPROPERTY()
	int8 Slot = INDEX_NONE;

	UPROPERTY()
	FGuid MarkerId;

	bool IsValid() const { return MarkerId.IsValid(); }
	bool operator==(const FLNPPlacementId& Other) const { return Slot == Other.Slot && MarkerId == Other.MarkerId; }

	friend uint32 GetTypeHash(const FLNPPlacementId& Id) { return HashCombine(GetTypeHash(Id.Slot), GetTypeHash(Id.MarkerId)); }

	FString ToString() const { return FString::Printf(TEXT("(%d, %s)"), Slot, *MarkerId.ToString(EGuidFormats::Short)); }
};

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class ULNPPlacedElement : public UInterface
{
	GENERATED_BODY()
};

/**
 * 마커에서 스폰되는 요소 Actor가 구현한다. 서버의 deferred 스폰 중 FinishSpawning 전에 호출되므로,
 * 여기서 채운 복제 프로퍼티는 초기 스폰 번치에 실린다.
 */
class ILNPPlacedElement
{
	GENERATED_BODY()

public:
	virtual void InitializeFromMarker(const ALNPPlacementMarker& Marker, const FLNPPlacementId& Id) = 0;
};
