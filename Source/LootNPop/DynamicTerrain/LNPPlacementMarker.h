// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LNPPlacementMarker.generated.h"

class USplineComponent;

/**
 * Placement Marker(TerrainContract.md §2-1). 옥탄트 LVI 안에 수동 배치하는 빈 슬롯이다.
 *
 * 비복제이고 충돌이 없으며 스스로는 아무것도 하지 않는다. 서버가 로드된 레벨에서 마커를 수집해
 * 마커의 월드 transform(slot 회전이 이미 적용됨)에 ElementClass를 스폰한다(ULNPDynamicTerrainSubsystem).
 *
 * 루트가 스플라인이므로 스플라인 로컬 공간 = 마커 로컬 공간이다. 경로가 필요 없는 요소는 스플라인을 무시한다.
 */
UCLASS()
class LOOTNPOP_API ALNPPlacementMarker : public AActor
{
	GENERATED_BODY()

public:
	ALNPPlacementMarker();

	/**
	 * 배치할 때 발급하고 복제·붙여넣기로 생긴 마커는 새로 받는다.
	 * AActor::ActorGuid는 editor-only라 cooked 런타임에 없으므로 쓰지 않는다.
	 */
	UPROPERTY(VisibleAnywhere, Category = "LNP|Placement")
	FGuid MarkerId;

	/** 스폰할 요소. ILNPPlacedElement를 구현해야 한다. */
	UPROPERTY(EditAnywhere, Category = "LNP|Placement", meta = (MustImplement = "/Script/LootNPop.LNPPlacedElement"))
	TSubclassOf<AActor> ElementClass;

	/** 경로(마커 로컬). 움직이는 패널은 첫 포인트에서 출발해 끝까지 등속으로 간다. 닫힌 루프면 순환, 아니면 왕복한다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Placement")
	TObjectPtr<USplineComponent> Path;

	/** 경로 이동 속도(cm/s). */
	UPROPERTY(EditAnywhere, Category = "LNP|Placement|Moving Panel", meta = (ClampMin = "1.0"))
	float PathSpeed = 300.f;

	/** 열린 경로 양 끝에서 멈추는 시간(초). 닫힌 루프에서는 쓰지 않는다. */
	UPROPERTY(EditAnywhere, Category = "LNP|Placement|Moving Panel", meta = (ClampMin = "0.0"))
	float PathEndHoldSeconds = 1.f;

	virtual void PostActorCreated() override;
#if WITH_EDITOR
	virtual void PostDuplicate(EDuplicateMode::Type DuplicateMode) override;
	virtual void PostEditImport() override;
#endif
};
