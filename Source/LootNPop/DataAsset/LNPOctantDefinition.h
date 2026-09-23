// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "LNPOctantDefinition.generated.h"

class ULNPOctantSurfaceData;

/** FLNPOctantDefinition이 허용하는 고정 runtime slot 회전 비트. */
UENUM(meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class ELNPOctantSlotRotation : uint8
{
	None = 0 UMETA(Hidden),
	Pitch0Yaw0 = 1 << 0,
	Pitch0Yaw90 = 1 << 1,
	Pitch0Yaw180 = 1 << 2,
	Pitch0Yaw270 = 1 << 3,
	Pitch180Yaw0 = 1 << 4,
	Pitch180Yaw90 = 1 << 5,
	Pitch180Yaw180 = 1 << 6,
	Pitch180Yaw270 = 1 << 7,
	All = 0xff UMETA(Hidden)
};
ENUM_CLASS_FLAGS(ELNPOctantSlotRotation);

/** Level Instance와 그 베이크 데이터를 함께 선택하는 최소 옥탄트 정의. */
USTRUCT(BlueprintType)
struct LOOTNPOP_API FLNPOctantDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|World Generation")
	TSoftObjectPtr<UWorld> LevelAsset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|World Generation")
	TSoftObjectPtr<ULNPOctantSurfaceData> SurfaceData;

	/** 8개 고정 slot 중 이 옥탄트를 배치할 수 있는 slot bitmask. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|World Generation",
		meta = (Bitmask, BitmaskEnum = "/Script/LootNPop.ELNPOctantSlotRotation"))
	uint8 AllowedSlotRotations = static_cast<uint8>(ELNPOctantSlotRotation::All);

	/** 서로 맞물릴 수 있는 seam 규약의 안정된 식별자. None은 아직 미지정인 legacy 정의다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|World Generation")
	FName SeamSignature;

	bool AllowsSlot(const uint8 SlotIndex) const
	{
		return SlotIndex < 8 && (AllowedSlotRotations & (1u << SlotIndex)) != 0;
	}
};
