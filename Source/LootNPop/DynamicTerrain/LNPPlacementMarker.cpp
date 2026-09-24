// Copyright (c) 2026 LootNPop. All rights reserved.

#include "DynamicTerrain/LNPPlacementMarker.h"

#include "Components/SplineComponent.h"

ALNPPlacementMarker::ALNPPlacementMarker()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	SetCanBeDamaged(false);

	// 스플라인도 primitive component다. 슬롯 source 수집·audit가 마커를 지형으로 오인하지 않게 충돌을 끈다.
	Path = CreateDefaultSubobject<USplineComponent>(TEXT("Path"));
	Path->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Path->SetGenerateOverlapEvents(false);
	Path->SetHiddenInGame(true);
	SetRootComponent(Path);
}

void ALNPPlacementMarker::PostActorCreated()
{
	Super::PostActorCreated();

	if (!MarkerId.IsValid())
	{
		MarkerId = FGuid::NewGuid();
	}
}

#if WITH_EDITOR
void ALNPPlacementMarker::PostDuplicate(const EDuplicateMode::Type DuplicateMode)
{
	Super::PostDuplicate(DuplicateMode);

	// PIE 복제는 같은 마커다. 에디터 복제(Ctrl+D 등)만 새 마커다.
	if (DuplicateMode == EDuplicateMode::Normal)
	{
		MarkerId = FGuid::NewGuid();
	}
}

void ALNPPlacementMarker::PostEditImport()
{
	Super::PostEditImport();

	// 붙여넣기는 원본의 MarkerId를 텍스트로 가져온다.
	MarkerId = FGuid::NewGuid();
}
#endif
