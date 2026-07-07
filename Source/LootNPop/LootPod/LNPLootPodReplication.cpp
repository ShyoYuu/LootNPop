// Copyright (c) 2026 LootNPop. All rights reserved.

#include "LootPod/LNPLootPodReplication.h"
#include "Net/UnrealNetwork.h"

ALNPLootPodClientBubbleInfo::ALNPLootPodClientBubbleInfo(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Serializers.Add(&LootPodSerializer);
}

void ALNPLootPodClientBubbleInfo::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams SharedParams;
	SharedParams.bIsPushBased = true;

	// FastArray 자체는 PushModel 대상이 아니지만 관례상 그대로 설정한다.
	DOREPLIFETIME_WITH_PARAMS_FAST(ALNPLootPodClientBubbleInfo, LootPodSerializer, SharedParams);
}
