// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Item/LNPItemDefinitionBase.h"

FPrimaryAssetId ULNPItemDefinitionBase::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(GetClass()->GetFName(), GetFName());
}
