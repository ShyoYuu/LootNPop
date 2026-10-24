#include "HAL/FileManager.h"
#include "Misc/CoreDelegates.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Modules/ModuleInterface.h"
#include "PropertyEditorModule.h"
#include "RewindDebuggerModule.h"
#include "TraceSessionsManager.h"
#include "Tools/LNPOctantMeshGenerator.h"

DEFINE_LOG_CATEGORY_STATIC(LogLNPRewind, Log, All);

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

		RewindDirectoryInitHandle = FCoreDelegates::GetOnPostEngineInit().AddRaw(
			this, &FLootNPopEditorModule::ConfigureRewindRecordingDirectory);
	}

	virtual void ShutdownModule() override
	{
		FCoreDelegates::GetOnPostEngineInit().Remove(RewindDirectoryInitHandle);

		if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
		{
			FPropertyEditorModule& PropertyModule =
				FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
			PropertyModule.UnregisterCustomClassLayout(
				ALNPOctantMeshGeneratorBase::StaticClass()->GetFName()
			);
		}
	}

private:
	void ConfigureRewindRecordingDirectory()
	{
#if UE_WITH_TRACE_BASED_DEBUGGERS_ANALYSIS
		FRewindDebuggerModule& RewindModule =
			FModuleManager::LoadModuleChecked<FRewindDebuggerModule>("RewindDebugger");
		const TSharedPtr<UE::TraceBasedDebuggers::FTraceSessionsManager>& Sessions =
			RewindModule.GetTraceSessionsManager();

		FFilePath Directory;
		Directory.FilePath = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("RewindDebugger"));
		// 생성 실패 때도 문서 폴더로 되돌아가지 않도록 먼저 경로를 지정한다.
		Sessions->SetSavePathOverride(Directory);
		if (!IFileManager::Get().MakeDirectory(*Directory.FilePath, true))
		{
			UE_LOG(LogLNPRewind, Error, TEXT("Failed to create recording directory: %s"), *Directory.FilePath);
			return;
		}
		UE_LOG(LogLNPRewind, Log, TEXT("Recording directory: %s"), *Sessions->GetSaveDirPath());
#endif
	}

	FDelegateHandle RewindDirectoryInitHandle;
};

IMPLEMENT_MODULE(FLootNPopEditorModule, LootNPopEditor)
