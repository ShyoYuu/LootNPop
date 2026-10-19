// Copyright (c) 2026 LootNPop. All rights reserved.

// GASP 구성 에셋을 LootNPop 에디터에서 읽기만 하는 에디터 명령(애니메이션 개선 단위 2-2).
//
// GASP 에디터(MCP 8001)에서는 Chooser 결과 행(UChooserTable::ResultsStructs)과 PSD 항목
// (UPoseSearchDatabase::DatabaseAnimationAssets)이 편집 플래그 없는 UPROPERTY()라 읽히지 않는다.
// C++에서는 접근할 수 있으므로, GASP Content/ 폴더를 임시 마운트 포인트(/GASPRef/)로 올려 그 패키지를 불러온다.
//
//  - dump  : 지정한 GASP 에셋 패키지 안의 모든 오브젝트(중첩 Chooser·채널 포함)의 프로퍼티를 텍스트로 JSON에 쓴다.
//  - clone : GASP 에셋을 LootNPop 경로에 새 구성 에셋으로 복제하고, 복제본끼리 참조를 다시 잇는다.
//            /GASPRef/ 를 가리키는 참조가 남으면 경고한다(정규화 세트는 /Game 밖 항목을 지운다). 저장하지 않는다.
//
// 불러오는 동안만 /Game/<redirectPrefix> → /GASPRef/<redirectPrefix> 패키지 리다이렉트를 걸어,
// LootNPop에 없는 GASP 구성 에셋(PSD·PSS·PSN·enum) 참조가 GASP 쪽 패키지로 풀리게 한다(AN-016 ②: LootNPop에는 반입하지 않는 경로).
// 시퀀스·스켈레톤처럼 LootNPop에 같은 경로로 반입한 소스는 리다이렉트하지 않아 LootNPop 쪽으로 풀린다.
// GASP 패키지는 저장하지 않는다(AN-009). 끝나면 리다이렉트와 마운트를 내린다(불러온 오브젝트는 에디터를 다시 켤 때까지 메모리에 남는다).
//
// 사용: LNP.Editor.GASPRef <spec.json>
// {
//   "gaspContentDir": "D:/UnrealProjects/GameAnimationSample/Content/",
//   "redirectPrefixes": ["/Game/Characters/UEFN_Mannequin/Animations/MotionMatchingData/"],
//   "dumpOut": "D:/.../Saved/AnimOverhaul/GASPRef.json",
//   "dump": ["/Game/Characters/.../CHT_PoseSearchDatabases_Relaxed"],
//   "clone": [ { "from": "/Game/.../PSS_Relaxed_Loops", "to": "/Game/Characters/MotionMatchingData/PSS_Relaxed_Loops_UEFN" } ]
// }
//
// LNP.Editor.SetChooserResults <Chooser> [Asset ...]
//   Chooser 결과 행을 에셋 결과(FAssetChooser)로 바꾼다(행 수 = 에셋 수). 에셋을 주지 않으면 결과는 두고 컴파일만 한다.
//   컬럼 행 값은 MCP ObjectTools.set_properties(columnsStructs)로 쓴다. 저장하지 않는다.

#include "AssetToolsModule.h"
#include "Chooser.h"
#include "Dom/JsonObject.h"
#include "HAL/IConsoleManager.h"
#include "IAssetTools.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchNormalizationSet.h"
#include "Serialization/ArchiveReplaceObjectRef.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/CoreRedirects.h"
#include "UObject/Package.h"
#include "UObject/UObjectHash.h"

DEFINE_LOG_CATEGORY_STATIC(LogLNPGASPRef, Log, All);

namespace
{
	const TCHAR* GASPRefRoot = TEXT("/GASPRef/");

	/** "/Game/A/B" 또는 "/Game/A/B.B" → "/Game/A/B.B" */
	FString LNPGASPRefObjectPath(const FString& InPath)
	{
		return InPath.Contains(TEXT(".")) ? InPath : InPath + TEXT(".") + FPackageName::GetShortName(InPath);
	}

	/** GASP 쪽 /Game 경로를 마운트 경로로 */
	FString LNPGASPRefToMounted(const FString& InGamePath)
	{
		FString Out = InGamePath;
		Out.RemoveFromStart(TEXT("/Game/"));
		return FString(GASPRefRoot) + Out;
	}

	/** 패키지 안 오브젝트 하나의 모든 프로퍼티를 텍스트로 */
	TSharedRef<FJsonObject> LNPGASPRefExportObject(UObject* Object)
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("class"), Object->GetClass()->GetPathName());
		TSharedRef<FJsonObject> Props = MakeShared<FJsonObject>();
		for (TFieldIterator<FProperty> It(Object->GetClass()); It; ++It)
		{
			FString Value;
			It->ExportTextItem_InContainer(Value, Object, nullptr, Object, PPF_None);
			if (!Value.IsEmpty() && Value != TEXT("()") && Value != TEXT("None"))
				Props->SetStringField(It->GetName(), Value);
		}
		Json->SetObjectField(TEXT("properties"), Props);
		return Json;
	}

	void LNPRunGASPRef(const TArray<FString>& Args)
	{
		if (Args.Num() < 1)
		{
			UE_LOG(LogLNPGASPRef, Error, TEXT("Usage: LNP.Editor.GASPRef <spec.json>"));
			return;
		}

		FString SpecText;
		TSharedPtr<FJsonObject> Spec;
		if (!FFileHelper::LoadFileToString(SpecText, *Args[0])
			|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(SpecText), Spec) || !Spec.IsValid())
		{
			UE_LOG(LogLNPGASPRef, Error, TEXT("Cannot read spec: %s"), *Args[0]);
			return;
		}

		const FString GaspContentDir = Spec->GetStringField(TEXT("gaspContentDir"));
		if (!FPaths::DirectoryExists(GaspContentDir))
		{
			UE_LOG(LogLNPGASPRef, Error, TEXT("GASP content dir not found: %s"), *GaspContentDir);
			return;
		}

		// 마운트 + 불러오는 동안만 쓰는 패키지 리다이렉트
		FPackageName::RegisterMountPoint(GASPRefRoot, GaspContentDir);

		TArray<FCoreRedirect> Redirects;
		const TArray<TSharedPtr<FJsonValue>>* PrefixValues = nullptr;
		if (Spec->TryGetArrayField(TEXT("redirectPrefixes"), PrefixValues))
		{
			for (const TSharedPtr<FJsonValue>& Value : *PrefixValues)
			{
				const FString Prefix = Value->AsString();
				Redirects.Emplace(ECoreRedirectFlags::Type_Package | ECoreRedirectFlags::Option_MatchPrefix, Prefix, LNPGASPRefToMounted(Prefix));
			}
		}
		const FString RedirectSource(TEXT("LNP.Editor.GASPRef"));
		FCoreRedirects::AddRedirectList(Redirects, RedirectSource);

		// --- dump ---
		const TArray<TSharedPtr<FJsonValue>>* DumpValues = nullptr;
		if (Spec->TryGetArrayField(TEXT("dump"), DumpValues))
		{
			TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();
			for (const TSharedPtr<FJsonValue>& Value : *DumpValues)
			{
				const FString GamePath = Value->AsString();
				UObject* Asset = LoadObject<UObject>(nullptr, *LNPGASPRefObjectPath(LNPGASPRefToMounted(GamePath)));
				if (Asset == nullptr)
				{
					UE_LOG(LogLNPGASPRef, Error, TEXT("[GASPRef] dump: cannot load %s"), *GamePath);
					continue;
				}

				TArray<UObject*> PackageObjects;
				GetObjectsWithPackage(Asset->GetOutermost(), PackageObjects, EGetObjectsFlags::IncludeNestedObjects);
				TSharedRef<FJsonObject> AssetJson = MakeShared<FJsonObject>();
				for (UObject* Object : PackageObjects)
				{
					if (Object->GetClass()->GetName().Contains(TEXT("MetaData")))
						continue;
					AssetJson->SetObjectField(Object->GetPathName(Asset->GetOutermost()), LNPGASPRefExportObject(Object));
				}
				Out->SetObjectField(GamePath, AssetJson);
				UE_LOG(LogLNPGASPRef, Display, TEXT("[GASPRef] dumped %s (%d objects)"), *GamePath, PackageObjects.Num());
			}

			FString OutText;
			FJsonSerializer::Serialize(Out, TJsonWriterFactory<>::Create(&OutText));
			const FString OutPath = Spec->GetStringField(TEXT("dumpOut"));
			FFileHelper::SaveStringToFile(OutText, *OutPath);
			UE_LOG(LogLNPGASPRef, Display, TEXT("[GASPRef] dump written: %s"), *OutPath);
		}

		// --- clone ---
		const TArray<TSharedPtr<FJsonValue>>* CloneValues = nullptr;
		if (Spec->TryGetArrayField(TEXT("clone"), CloneValues))
		{
			IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
			TMap<UObject*, UObject*> SourceToClone;
			for (const TSharedPtr<FJsonValue>& Value : *CloneValues)
			{
				const TSharedPtr<FJsonObject>& Pair = Value->AsObject();
				const FString From = Pair->GetStringField(TEXT("from"));
				const FString To   = Pair->GetStringField(TEXT("to"));
				UObject* Source = LoadObject<UObject>(nullptr, *LNPGASPRefObjectPath(LNPGASPRefToMounted(From)));
				if (Source == nullptr)
				{
					UE_LOG(LogLNPGASPRef, Error, TEXT("[GASPRef] clone: cannot load %s"), *From);
					continue;
				}
				if (FPackageName::DoesPackageExist(To))
				{
					UE_LOG(LogLNPGASPRef, Error, TEXT("[GASPRef] clone: target already exists, skipped: %s"), *To);
					continue;
				}
				UObject* Clone = AssetTools.DuplicateAsset(FPackageName::GetShortName(To), FPackageName::GetLongPackagePath(To), Source);
				if (Clone == nullptr)
				{
					UE_LOG(LogLNPGASPRef, Error, TEXT("[GASPRef] clone failed: %s -> %s"), *From, *To);
					continue;
				}
				SourceToClone.Add(Source, Clone);
			}

			// 복제본이 다른 GASP 원본을 가리키면 그 복제본으로 잇는다(PSD → PSS·PSN, PSN → PSD)
			for (const TPair<UObject*, UObject*>& Pair : SourceToClone)
			{
				UObject* Clone = Pair.Value;
				TArray<UObject*> PackageObjects;
				GetObjectsWithPackage(Clone->GetOutermost(), PackageObjects, EGetObjectsFlags::IncludeNestedObjects);
				int64 Replaced = 0;
				for (UObject* Object : PackageObjects)
				{
					FArchiveReplaceObjectRef<UObject> Replacer(Object, SourceToClone,
						EArchiveReplaceObjectFlags::IgnoreOuterRef | EArchiveReplaceObjectFlags::IgnoreArchetypeRef);
					Replaced += Replacer.GetCount();
				}

				// 정규화 세트는 복제하지 않은 GASP DB를 뺀다
				if (UPoseSearchNormalizationSet* NormalizationSet = Cast<UPoseSearchNormalizationSet>(Clone))
				{
					NormalizationSet->Databases.RemoveAll([](const TObjectPtr<const UPoseSearchDatabase>& Database)
					{
						return Database == nullptr || !Database->GetPathName().StartsWith(TEXT("/Game/"));
					});
				}

				Clone->PostEditChange();
				Clone->MarkPackageDirty();

				// /GASPRef/ 참조가 남았는지 확인(저장하면 깨진 참조가 된다)
				TArray<FString> Leftovers;
				for (UObject* Object : PackageObjects)
				{
					for (TFieldIterator<FProperty> It(Object->GetClass()); It; ++It)
					{
						FString Text;
						It->ExportTextItem_InContainer(Text, Object, nullptr, Object, PPF_None);
						if (Text.Contains(GASPRefRoot))
							Leftovers.AddUnique(Object->GetName() + TEXT(".") + It->GetName());
					}
				}

				UE_LOG(LogLNPGASPRef, Display, TEXT("[GASPRef] cloned %s -> %s (%lld refs relinked)%s%s"),
					*Pair.Key->GetPathName(), *Clone->GetPathName(), Replaced,
					Leftovers.IsEmpty() ? TEXT("") : TEXT(", WARNING refs to /GASPRef/ remain: "),
					*FString::Join(Leftovers, TEXT(", ")));
			}
			UE_LOG(LogLNPGASPRef, Display, TEXT("[GASPRef] %d clone(s) created. Packages marked dirty (not saved)."), SourceToClone.Num());
		}

		FCoreRedirects::RemoveRedirectList(Redirects, RedirectSource);
		FPackageName::UnRegisterMountPoint(GASPRefRoot, GaspContentDir);
		UE_LOG(LogLNPGASPRef, Display, TEXT("[GASPRef] done. Redirects and mount point removed; GASP packages stay loaded until editor restart (never save them)."));
	}

	void LNPRunSetChooserResults(const TArray<FString>& Args)
	{
		if (Args.Num() < 1)
		{
			UE_LOG(LogLNPGASPRef, Error, TEXT("Usage: LNP.Editor.SetChooserResults <Chooser> [Asset ...]"));
			return;
		}

		UChooserTable* Chooser = LoadObject<UChooserTable>(nullptr, *LNPGASPRefObjectPath(Args[0]));
		if (Chooser == nullptr)
		{
			UE_LOG(LogLNPGASPRef, Error, TEXT("Chooser not found: %s"), *Args[0]);
			return;
		}

		Chooser->Modify();
		if (Args.Num() > 1)
		{
			// FAssetChooser는 모듈 밖으로 내보내지 않은 구조체라 StaticStruct()를 링크할 수 없다 → 리플렉션으로 채운다
			const UScriptStruct* AssetChooserStruct = FindObject<UScriptStruct>(nullptr, TEXT("/Script/Chooser.AssetChooser"));
			const FObjectPropertyBase* AssetProperty = AssetChooserStruct
				? CastField<FObjectPropertyBase>(AssetChooserStruct->FindPropertyByName(TEXT("Asset"))) : nullptr;
			if (AssetProperty == nullptr)
			{
				UE_LOG(LogLNPGASPRef, Error, TEXT("Chooser asset result struct not found (/Script/Chooser.AssetChooser.Asset)"));
				return;
			}

			TArray<FInstancedStruct> Results;
			for (int32 Index = 1; Index < Args.Num(); ++Index)
			{
				UObject* Asset = LoadObject<UObject>(nullptr, *LNPGASPRefObjectPath(Args[Index]));
				if (Asset == nullptr)
				{
					UE_LOG(LogLNPGASPRef, Error, TEXT("Result asset not found (row %d): %s"), Index - 1, *Args[Index]);
					return;
				}
				FInstancedStruct& Result = Results.AddDefaulted_GetRef();
				Result.InitializeAs(AssetChooserStruct);
				AssetProperty->SetObjectPropertyValue_InContainer(Result.GetMutableMemory(), Asset);
			}
			Chooser->ResultsStructs = MoveTemp(Results);
			Chooser->DisabledRows.Init(false, Chooser->ResultsStructs.Num());
		}

		Chooser->Compile(/*bForce=*/true);
		Chooser->PostEditChange();
		Chooser->MarkPackageDirty();
		UE_LOG(LogLNPGASPRef, Display, TEXT("[SetChooserResults] %s: %d row(s). Compiled, marked dirty (not saved)."),
			*Chooser->GetPathName(), Chooser->ResultsStructs.Num());
	}

	static FAutoConsoleCommand LNPGASPRefCommand(
		TEXT("LNP.Editor.GASPRef"),
		TEXT("Editor only. Read GASP config assets by mounting the GASP Content folder as /GASPRef/: ")
		TEXT("LNP.Editor.GASPRef <spec.json> (dump properties to JSON, clone assets into LootNPop paths). ")
		TEXT("Never saves GASP packages; clones are marked dirty but not saved."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&LNPRunGASPRef));

	static FAutoConsoleCommand LNPSetChooserResultsCommand(
		TEXT("LNP.Editor.SetChooserResults"),
		TEXT("Editor only. Replace chooser result rows with asset results: ")
		TEXT("LNP.Editor.SetChooserResults <Chooser> [Asset ...] (no assets = compile only). Marks dirty, does not save."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&LNPRunSetChooserResults));
}
