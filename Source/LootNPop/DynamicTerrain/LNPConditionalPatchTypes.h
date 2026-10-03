// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "LNPConditionalPatchTypes.generated.h"

class UStaticMesh;

/** 요소 Actor의 상태가 이 조건을 만족할 때 patch가 활성화된다. */
UENUM()
enum class ELNPConditionalPatchCondition : uint8
{
	Intact,
	Bridge,
};

/** 마커 로컬 공간의 안정 상태 exact 형상. Support는 같은 형상의 보행 가능한 앞면에서만 만든다. */
USTRUCT()
struct FLNPConditionalPatchSource
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "LNP|Conditional Patch")
	TObjectPtr<UStaticMesh> Mesh;

	UPROPERTY(EditAnywhere, Category = "LNP|Conditional Patch")
	FTransform Transform = FTransform::Identity;

	UPROPERTY(EditAnywhere, Category = "LNP|Conditional Patch")
	bool bSupport = true;
};
