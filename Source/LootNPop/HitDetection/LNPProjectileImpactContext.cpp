// Copyright (c) 2026 LootNPop. All rights reserved.

#include "HitDetection/LNPProjectileImpactContext.h"
#include "VFX/LNPVFXData.h"
#include "UObject/CoreNet.h"

bool FLNPProjectileImpactContext::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	Super::NetSerialize(Ar, Map, bOutSuccess);

	Ar << PredictionKeyID;
	Ar << SpawnIndex;
	Ar << InstigatorPlayerID;

	UObject* VFXDataObj = VFXData;
	Map->SerializeObject(Ar, ULNPVFXData::StaticClass(), VFXDataObj);
	VFXData = Cast<ULNPVFXData>(VFXDataObj);

	bOutSuccess = true;
	return true;
}
