// Copyright (c) 2026 LootNPop. All rights reserved.

#include "DataAsset/LNPWorldDeviceConfig.h"
#include "Config/LNPSettings.h"

const ULNPWorldDeviceConfig* ULNPWorldDeviceConfig::Get(const UObject* WorldContext)
{
	// 소비처가 매 틱 부르므로(프롬프트 판정) 동기 로드 결과를 캐시한다. 설정은 런타임에 바뀌지 않는다.
	static TWeakObjectPtr<const ULNPWorldDeviceConfig> Cached;
	if (Cached.IsValid())
	{
		return Cached.Get();
	}

	const ULNPWorldDeviceConfig* Config = GetDefault<ULNPSettings>()->WorldDeviceConfig.LoadSynchronous();
	Cached = Config;
	return Config;
}
