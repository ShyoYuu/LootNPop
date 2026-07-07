// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Character/LNPPlayerReplication.h"
#include "Net/UnrealNetwork.h"

ALNPPlayerClientBubbleInfo::ALNPPlayerClientBubbleInfo(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Serializers.Add(&PlayerSerializer);
}

void ALNPPlayerClientBubbleInfo::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams SharedParams;
	SharedParams.bIsPushBased = true;

	// FastArray 자체는 PushModel 대상이 아니지만 관례상 그대로 설정한다.
	DOREPLIFETIME_WITH_PARAMS_FAST(ALNPPlayerClientBubbleInfo, PlayerSerializer, SharedParams);
}
