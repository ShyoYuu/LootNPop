#include "Modules/ModuleManager.h"
#include "Modules/ModuleInterface.h"
#include "PropertyEditorModule.h"
#include "Tools/LNPOctantMeshGenerator.h"

class FLootNPopEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FPropertyEditorModule& PropertyModule =
			FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

		PropertyModule.RegisterCustomClassLayout(
			ALNPOctantMeshGeneratorBase::StaticClass()->GetFName(),
			FOnGetDetailCustomizationInstance::CreateStatic(&FLNPOctantMeshGeneratorCustomization::MakeInstance)
		);
		PropertyModule.NotifyCustomizationModuleChanged();
	}

	virtual void ShutdownModule() override
	{
		if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
		{
			FPropertyEditorModule& PropertyModule =
				FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
			PropertyModule.UnregisterCustomClassLayout(
				ALNPOctantMeshGeneratorBase::StaticClass()->GetFName()
			);
		}
	}
};

IMPLEMENT_MODULE(FLootNPopEditorModule, LootNPopEditor)
