// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LNPMassSpawnPoint.generated.h"

class USceneComponent;

/**
 * 옥탄트 LVI에 배치하는 editor-only Pod 세트 앵커(D-059).
 * 런타임은 Actor를 스캔하지 않고 베이크된 Spawn stream만 읽는다.
 */
UCLASS(NotBlueprintable)
class LOOTNPOP_API ALNPMassSpawnPoint : public AActor
{
	GENERATED_BODY()

public:
	ALNPMassSpawnPoint();

	/** 결정론 정렬용 ID. 에디터 복제·붙여넣기는 새 ID를 발급한다. */
	UPROPERTY(VisibleAnywhere, Category = "LNP|Spawn")
	FGuid SpawnPointId;

	/** 특정 Pod 세트를 지정한다. None이면 어느 세트나 쓸 수 있는 일반 앵커다. */
	UPROPERTY(EditAnywhere, Category = "LNP|Spawn")
	FName TargetSpawnSetId;

	/** 위치는 Pod 발밑 접점, 로컬 +X는 표면에 투영할 authored yaw다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Spawn")
	TObjectPtr<USceneComponent> Anchor;

	virtual void PostActorCreated() override;
	virtual bool IsEditorOnly() const override { return true; }

#if WITH_EDITOR
	virtual void PostDuplicate(EDuplicateMode::Type DuplicateMode) override;
	virtual void PostEditImport() override;
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
