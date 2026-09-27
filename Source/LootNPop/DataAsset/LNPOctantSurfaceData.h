// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "IO/IoHash.h"
#include "LNPOctantSurfaceData.generated.h"

/**
 * FIoHash의 BLAKE3-160 값을 UObject property로 손실 없이 저장하는 고정 크기 타입.
 * FIoHash 자체는 reflected type이 아니므로 asset schema 경계에서는 이 타입을 사용한다.
 */
USTRUCT()
struct LOOTNPOP_API FLNPContentHash
{
	GENERATED_BODY()

public:
	static constexpr int32 NumBytes = sizeof(FIoHash::ByteArray);

	FLNPContentHash();
	explicit FLNPContentHash(const FIoHash& InHash);

	FIoHash ToIoHash() const;
	FString ToString() const;
	bool IsZero() const;

	bool operator==(const FLNPContentHash& Other) const;
	bool operator!=(const FLNPContentHash& Other) const { return !(*this == Other); }

private:
	UPROPERTY(VisibleAnywhere, Category = "LNP|Surface Navigation")
	uint8 Bytes[20];
};

/** Source manifest package가 베이크 입력에서 맡는 역할. */
UENUM()
enum class ELNPOctantSourcePackageKind : uint8
{
	SourceLevel,
	/** 현재 3-kind schema에서 UE의 external actor와 external object package를 함께 나타낸다. */
	ExternalActor,
	TerrainMesh
};

/** 정렬된 source manifest의 package 단위 항목. */
USTRUCT()
struct LOOTNPOP_API FLNPOctantSourcePackage
{
	GENERATED_BODY()

	/** Long package name. Manifest는 이 값, Kind 순으로 정렬한다. */
	UPROPERTY(VisibleAnywhere, Category = "LNP|Surface Navigation")
	FName PackageName;

	UPROPERTY(VisibleAnywhere, Category = "LNP|Surface Navigation")
	ELNPOctantSourcePackageKind Kind = ELNPOctantSourcePackageKind::TerrainMesh;

	/** Asset Registry의 FAssetPackageData::GetPackageSavedHash() 원본 20바이트. */
	UPROPERTY(VisibleAnywhere, Category = "LNP|Surface Navigation")
	FLNPContentHash PackageSavedHash;
};

/** Header가 payload를 로드하지 않고도 검증할 수 있는 stream 요약. */
USTRUCT()
struct LOOTNPOP_API FLNPSurfacePayloadDescriptor
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = "LNP|Surface Navigation")
	uint32 ElementCount = 0;

	UPROPERTY(VisibleAnywhere, Category = "LNP|Surface Navigation")
	uint64 UncompressedSize = 0;

	UPROPERTY(VisibleAnywhere, Category = "LNP|Surface Navigation")
	FLNPContentHash ContentHash;
};

/**
 * 옥탄트 SurfaceData의 항상 상주하는 소형 header.
 * Source 검증과 stream 선택은 payload 역직렬화 전에 이 정보만으로 수행한다.
 */
USTRUCT()
struct LOOTNPOP_API FLNPSurfaceBakeHeader
{
	GENERATED_BODY()

	/** 3: Support payload에 다층 Support Atlas codec v2(`LNPSupportAtlas::Encode`). 2는 지각 전용 codec v1이었다. */
	static constexpr uint32 CurrentDataVersion = 3;

	FLNPSurfaceBakeHeader();

	/**
	 * 베이커가 CurrentDataVersion을 명시적으로 쓴다. 기본값이 0이어야 한다. 기본값이 현재 버전이면 tagged property
	 * 직렬화가 기본값과 같은 값을 저장하지 않아, 버전을 올린 뒤 옛 에셋을 로드해도 새 버전으로 보인다.
	 */
	UPROPERTY(VisibleAnywhere, Category = "LNP|Surface Navigation")
	uint32 DataVersion;

	/** 정렬된 manifest, semantic hash, bake settings hash를 합친 최종 source hash. */
	UPROPERTY(VisibleAnywhere, Category = "LNP|Surface Navigation")
	FLNPContentHash SourceContentHash;

	/** Component tag, transform, collision profile을 canonical form으로 만든 hash. */
	UPROPERTY(VisibleAnywhere, Category = "LNP|Surface Navigation")
	FLNPContentHash SourceSemanticHash;

	/** 베이커 schema와 품질 설정을 canonical form으로 만든 hash. */
	UPROPERTY(VisibleAnywhere, Category = "LNP|Surface Navigation")
	FLNPContentHash BakeSettingsHash;

	UPROPERTY(VisibleAnywhere, Category = "LNP|Surface Navigation")
	TArray<FLNPOctantSourcePackage> SourceManifest;

	UPROPERTY(VisibleAnywhere, Category = "LNP|Surface Navigation")
	FLNPSurfacePayloadDescriptor Support;

	UPROPERTY(VisibleAnywhere, Category = "LNP|Surface Navigation")
	FLNPSurfacePayloadDescriptor Navigation;

	UPROPERTY(VisibleAnywhere, Category = "LNP|Surface Navigation")
	FLNPSurfacePayloadDescriptor Traversal;

	UPROPERTY(VisibleAnywhere, Category = "LNP|Surface Navigation")
	FLNPSurfacePayloadDescriptor Spawn;
};

/**
 * 옥탄트별 에디터 베이크 결과.
 * Header와 네 payload stream을 의도적으로 분리해 향후 각 stream을 BulkData로 전환할 수 있게 한다.
 */
UCLASS(BlueprintType)
class LOOTNPOP_API ULNPOctantSurfaceData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, Category = "LNP|Surface Navigation")
	FLNPSurfaceBakeHeader Header;

	/** 다층 Support Atlas(`LNPSupportAtlas` codec v2). Header.Support의 ElementCount는 모든 Layer의 샘플 수 합이다. */
	UPROPERTY(VisibleAnywhere, Category = "LNP|Surface Navigation")
	TArray<uint8> SupportPayload;

	/** Phase 7에서 Nav codec을 확정하기 전까지 사용하는 직렬화 경계. */
	UPROPERTY(VisibleAnywhere, Category = "LNP|Surface Navigation")
	TArray<uint8> NavigationPayload;

	/** 정적 seam, portal, Walk link stream. */
	UPROPERTY(VisibleAnywhere, Category = "LNP|Surface Navigation")
	TArray<uint8> TraversalPayload;

	/** spawn candidate와 clearance stream. */
	UPROPERTY(VisibleAnywhere, Category = "LNP|Surface Navigation")
	TArray<uint8> SpawnPayload;
};
