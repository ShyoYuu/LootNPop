// Copyright (c) 2026 LootNPop. All rights reserved.

#include "LootNPop.h"
#include "Animation/UAFSpike/LNPUAFSpikeRootInputs.h"
#include "Modules/ModuleManager.h"
#include "RigVMCore/RigVMRegistry.h"

class FLootNPopModule : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override
	{
		FDefaultGameModuleImpl::StartupModule();

		// UAF 공유 변수 Getter에서 사용할 네이티브 구조체를 RigVM에도 등록한다.
		UScriptStruct* RootInputsStruct = FLNPUAFSpikeRootInputs::StaticStruct();
		FRigVMRegistry& Registry = FRigVMRegistry::Get();
		Registry.RegisterStructTypes(MakeArrayView(&RootInputsStruct, 1));
		Registry.FindOrAddType(FRigVMTemplateArgumentType(RootInputsStruct));
	}
};

IMPLEMENT_PRIMARY_GAME_MODULE( FLootNPopModule, LootNPop, "LootNPop" );

DEFINE_LOG_CATEGORY(LogLootNPop)
