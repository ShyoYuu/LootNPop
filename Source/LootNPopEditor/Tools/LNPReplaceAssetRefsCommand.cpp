// Copyright (c) 2026 LootNPop. All rights reserved.

// 한 에셋 패키지 안의 오브젝트 참조를 지정한 쌍대로 바꾸는 에디터 명령(애니메이션 개선 단위 2-0b).
//
// Chooser 테이블의 결과 행(UChooserTable::ResultsStructs)은 편집 플래그가 없는 UPROPERTY()라
// Unreal MCP·에디터 Python으로 읽거나 쓸 수 없다. 원본을 지우는 ConsolidateAssets도 쓸 수 없다(Lyra 원본 유지).
// 그래서 FArchiveReplaceObjectRef로 대상 패키지의 모든 오브젝트(결과 행·하위 테이블·BP CDO·그래프 노드 포함)를
// 직렬화하며 참조만 바꾼다. 패키지를 dirty로 표시할 뿐 저장하지 않는다. 블루프린트는 바꾼 뒤 컴파일한다.
//
// 예: LNP.Editor.ReplaceAssetRefs /Game/Characters/CHT_Montage
//         /Game/Characters/Lyra_Mannequin/Animations/Sword/AM_SW_Attack_01=/Game/Characters/Retargeted/Lyra/AM_SW_Attack_01_UEFN

#include "HAL/IConsoleManager.h"
#include "Misc/PackageName.h"
#include "Serialization/ArchiveReplaceObjectRef.h"
#include "UObject/Package.h"
#include "UObject/UObjectHash.h"

DEFINE_LOG_CATEGORY_STATIC(LogLNPReplaceAssetRefs, Log, All);

namespace
{
	/** "/Game/A/B" 또는 "/Game/A/B.B" 형식을 받아 에셋 오브젝트를 읽는다. */
	UObject* LNPReplaceRefsLoadAsset(const FString& InPath)
	{
		FString ObjectPath = InPath;
		if (!ObjectPath.Contains(TEXT(".")))
			ObjectPath += TEXT(".") + FPackageName::GetShortName(ObjectPath);
		return LoadObject<UObject>(nullptr, *ObjectPath);
	}

	void LNPRunReplaceAssetRefs(const TArray<FString>& Args)
	{
		if (Args.Num() < 2)
		{
			UE_LOG(LogLNPReplaceAssetRefs, Error, TEXT("Usage: LNP.Editor.ReplaceAssetRefs <TargetAsset> <OldAsset>=<NewAsset> ..."));
			return;
		}

		UObject* Target = LNPReplaceRefsLoadAsset(Args[0]);
		if (Target == nullptr)
		{
			UE_LOG(LogLNPReplaceAssetRefs, Error, TEXT("Target asset not found: %s"), *Args[0]);
			return;
		}

		TMap<UObject*, UObject*> ReplacementMap;
		for (int32 Index = 1; Index < Args.Num(); ++Index)
		{
			FString OldPath, NewPath;
			if (!Args[Index].Split(TEXT("="), &OldPath, &NewPath))
			{
				UE_LOG(LogLNPReplaceAssetRefs, Error, TEXT("Invalid pair (expected Old=New): %s"), *Args[Index]);
				return;
			}
			UObject* OldObject = LNPReplaceRefsLoadAsset(OldPath);
			UObject* NewObject = LNPReplaceRefsLoadAsset(NewPath);
			if (OldObject == nullptr || NewObject == nullptr)
			{
				UE_LOG(LogLNPReplaceAssetRefs, Error, TEXT("Pair asset not found: %s"), *Args[Index]);
				return;
			}
			ReplacementMap.Add(OldObject, NewObject);
		}

		// UPackage 자체는 직렬화할 참조가 없어 검색 루트로 쓰면 아무것도 순회하지 않는다.
		// 패키지 안의 오브젝트(에셋·BP CDO·그래프 노드 등)마다 따로 돌린다.
		UPackage* Package = Target->GetOutermost();
		TArray<UObject*> PackageObjects;
		GetObjectsWithPackage(Package, PackageObjects, /*bIncludeNestedObjects=*/true);
		int64 ReplacedCount = 0;
		for (UObject* Object : PackageObjects)
		{
			FArchiveReplaceObjectRef<UObject> Replacer(Object, ReplacementMap,
				EArchiveReplaceObjectFlags::IgnoreOuterRef | EArchiveReplaceObjectFlags::IgnoreArchetypeRef);
			ReplacedCount += Replacer.GetCount();
		}

		Target->PostEditChange();
		Package->MarkPackageDirty();

		UE_LOG(LogLNPReplaceAssetRefs, Display, TEXT("[ReplaceAssetRefs] %s: %d pair(s), %lld reference(s) replaced. Package marked dirty (not saved)."),
			*Package->GetName(), ReplacementMap.Num(), ReplacedCount);
	}

	static FAutoConsoleCommand LNPReplaceAssetRefsCommand(
		TEXT("LNP.Editor.ReplaceAssetRefs"),
		TEXT("Editor only. Replace object references inside one asset package: ")
		TEXT("LNP.Editor.ReplaceAssetRefs <TargetAsset> <OldAsset>=<NewAsset> ... ")
		TEXT("Marks the package dirty but does not save. Compile blueprints afterwards."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&LNPRunReplaceAssetRefs));
}
