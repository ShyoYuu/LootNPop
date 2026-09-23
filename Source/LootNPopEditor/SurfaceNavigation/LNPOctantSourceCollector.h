// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "DataAsset/LNPOctantSurfaceData.h"

class IAssetRegistry;
class UWorld;

/** BakeSettingsHash에 넣을 값의 canonical type. */
enum class ELNPOctantBakeSettingType : uint8
{
	Boolean,
	SignedInteger,
	UnsignedInteger,
	Real,
	String
};

/**
 * Phase별 베이커가 고유 설정을 이름과 typed value로 전달하는 비-reflected 입력.
 * 같은 이름의 동일 값은 중복 제거하며, 같은 이름의 서로 다른 값은 오류다.
 */
struct LOOTNPOPEDITOR_API FLNPOctantBakeSetting
{
	FName Name;
	ELNPOctantBakeSettingType Type = ELNPOctantBakeSettingType::Boolean;
	bool BoolValue = false;
	int64 SignedIntegerValue = 0;
	uint64 UnsignedIntegerValue = 0;
	double RealValue = 0.0;
	FString StringValue;

	static FLNPOctantBakeSetting Boolean(FName InName, bool bInValue);
	static FLNPOctantBakeSetting SignedInteger(FName InName, int64 InValue);
	static FLNPOctantBakeSetting UnsignedInteger(FName InName, uint64 InValue);
	static FLNPOctantBakeSetting Real(FName InName, double InValue);
	static FLNPOctantBakeSetting String(FName InName, FString InValue);
};

/** Terrain Contract 역할 component에서 hash에 넣는 의미 값. */
struct LOOTNPOPEDITOR_API FLNPTerrainSourceSemantic
{
	/** Terrain Contract에 정의된 LNP.Surface.* tag만 포함한다. */
	TArray<FName> TerrainTags;

	/** Source level 좌표계의 component transform. */
	FTransform Transform = FTransform::Identity;

	FName CollisionProfileName;
	FName StaticMeshPackageName;
};

/** Source manifest와 세 freshness hash를 한 번에 생성한 결과. */
struct LOOTNPOPEDITOR_API FLNPOctantSourceCollection
{
	TArray<FLNPOctantSourcePackage> Manifest;
	TArray<FLNPTerrainSourceSemantic> TerrainSemantics;
	FLNPContentHash SourceSemanticHash;
	FLNPContentHash BakeSettingsHash;
	FLNPContentHash SourceContentHash;
};

/**
 * 옥탄트 LVI의 제한된 source dependency와 canonical freshness hash를 생성한다.
 * Asset Registry dependency는 source package의 직접 external package만 사용하며 재귀 순회하지 않는다.
 */
class LOOTNPOPEDITOR_API FLNPOctantSourceCollector
{
public:
	static bool CollectFromLevel(
		const FSoftObjectPath& SourceLevel,
		uint32 BakerSchemaVersion,
		TConstArrayView<FLNPOctantBakeSetting> BakeSettings,
		FLNPOctantSourceCollection& OutCollection,
		FString& OutError);

	static bool CollectFromWorld(
		const UWorld& SourceWorld,
		FName SourceLevelPackageName,
		IAssetRegistry& AssetRegistry,
		uint32 BakerSchemaVersion,
		TConstArrayView<FLNPOctantBakeSetting> BakeSettings,
		FLNPOctantSourceCollection& OutCollection,
		FString& OutError);

	/** Source LVI와 그 package가 직접 참조하는 external actor/object package만 추가한다. */
	static bool CollectLevelPackages(
		FName SourceLevelPackageName,
		IAssetRegistry& AssetRegistry,
		TArray<FLNPOctantSourcePackage>& OutManifest,
		FString& OutError);

	/** Terrain Contract 역할 StaticMeshComponent의 mesh와 의미 값을 수집한다. */
	static bool CollectTerrainComponents(
		const UWorld& SourceWorld,
		IAssetRegistry& AssetRegistry,
		TArray<FLNPOctantSourcePackage>& InOutManifest,
		TArray<FLNPTerrainSourceSemantic>& OutSemantics,
		FString& OutError);

	/** PackageName, Kind 순으로 정렬하고 완전히 동일한 행을 중복 제거한다. */
	static bool CanonicalizeManifest(
		TArray<FLNPOctantSourcePackage>& InOutManifest,
		FString& OutError);

	static bool BuildSourceSemanticHash(
		TConstArrayView<FLNPTerrainSourceSemantic> Semantics,
		FLNPContentHash& OutHash,
		FString& OutError);

	static bool BuildBakeSettingsHash(
		uint32 BakerSchemaVersion,
		TConstArrayView<FLNPOctantBakeSetting> BakeSettings,
		FLNPContentHash& OutHash,
		FString& OutError);

	static bool BuildSourceContentHash(
		TConstArrayView<FLNPOctantSourcePackage> Manifest,
		const FLNPContentHash& SourceSemanticHash,
		const FLNPContentHash& BakeSettingsHash,
		FLNPContentHash& OutHash,
		FString& OutError);
};
