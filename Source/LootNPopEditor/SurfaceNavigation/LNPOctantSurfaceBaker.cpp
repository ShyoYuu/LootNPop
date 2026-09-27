// Copyright (c) 2026 LootNPop. All rights reserved.

// 옥탄트 SurfaceData 에디터 베이크 명령.
//
// 예: LNP.SurfaceNav.BakeOctant /Game/Maps/Meadow_00/LVI_Octant_Meadow_00
//     LVI 옆 DA_OctantSurface_Meadow_00을 만들거나 제자리 갱신하고 저장한다.

#include "SurfaceNavigation/LNPOctantSurfaceBaker.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Config/LNPSettings.h"
#include "DataAsset/LNPOctantSurfaceData.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Misc/PackageName.h"
#include "SurfaceNavigation/LNPOctantSourceCollector.h"
#include "SurfaceNavigation/LNPOctantTriangleExtractor.h"
#include "SurfaceNavigation/LNPSurfaceBakeGeometry.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

DEFINE_LOG_CATEGORY_STATIC(LogLNPSurfaceBake, Log, All);

namespace LNPOctantSurfaceBaker
{
	TArray<FLNPOctantBakeSetting> MakeBakeSettings(
		const FLNPCrustRasterSettings& Raster,
		const FLNPCrustCodecSettings& Codec)
	{
		return {
			FLNPOctantBakeSetting::UnsignedInteger(TEXT("Crust.CodecVersion"), LNPCrustAtlas::CodecVersion),
			FLNPOctantBakeSetting::SignedInteger(TEXT("Crust.Subdivisions"), Raster.Subdivisions),
			FLNPOctantBakeSetting::Real(TEXT("Crust.WalkableMinDot"), Raster.WalkableMinDot),
			FLNPOctantBakeSetting::Real(TEXT("Crust.MaxNeighborNormalAngleDeg"), Raster.MaxNeighborNormalAngleDeg),
			FLNPOctantBakeSetting::Real(TEXT("Crust.HitMergeDistance"), Raster.HitMergeDistance),
			FLNPOctantBakeSetting::Real(TEXT("Crust.SeamSnapDistance"), Raster.SeamSnapDistance),
			FLNPOctantBakeSetting::Real(TEXT("Crust.BaseRadius"), Codec.BaseRadius),
			FLNPOctantBakeSetting::Real(TEXT("Crust.RadiusStep"), Codec.RadiusStep),
		};
	}

	UWorld* LoadSourceWorld(const FSoftObjectPath& SourceLevel)
	{
		IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
		const FAssetData Asset = AssetRegistry.GetAssetByObjectPath(SourceLevel);
		TSet<FName> LoadTags;
		LoadTags.Add(ULevel::LoadAllExternalObjectsTag);
		return Asset.IsValid() ? Cast<UWorld>(Asset.GetAsset(MoveTemp(LoadTags))) : nullptr;
	}

	/** 패키지 경로만 받아도 `<패키지>.<에셋>` object path로 바꾼다. */
	FSoftObjectPath ToLevelObjectPath(const FString& Argument)
	{
		if (Argument.Contains(TEXT(".")))
		{
			return FSoftObjectPath(Argument);
		}
		return FSoftObjectPath(FString::Printf(TEXT("%s.%s"), *Argument, *FPackageName::GetShortName(Argument)));
	}

	void Run(const TArray<FString>& Args)
	{
		if (Args.Num() != 1)
		{
			UE_LOG(LogLNPSurfaceBake, Error, TEXT("Usage: LNP.SurfaceNav.BakeOctant <LevelPath>"));
			return;
		}

		// 시작 직후 -ExecCmds로 부르면 Asset Registry 스캔이 끝나지 않았을 수 있다.
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get().WaitForCompletion();
		const FSoftObjectPath SourceLevel = ToLevelObjectPath(Args[0]);
		FLNPOctantBakeReport Report;
		FString Error;
		if (!FLNPOctantSurfaceBaker::BakeAndSave(SourceLevel, FLNPOctantBakeOptions(), Report, Error))
		{
			UE_LOG(LogLNPSurfaceBake, Error, TEXT("[BakeOctant] %s failed: %s"), *SourceLevel.ToString(), *Error);
			return;
		}
		UE_LOG(LogLNPSurfaceBake, Display, TEXT("[BakeOctant] %s -> %s | %s"),
			*SourceLevel.ToString(), *FLNPOctantSurfaceBaker::GetSurfaceDataPackageName(SourceLevel), *Report.ToString());
	}

	static FAutoConsoleCommand Command(
		TEXT("LNP.SurfaceNav.BakeOctant"),
		TEXT("Bake the crust Support Atlas of an octant LVI into DA_OctantSurface_<Name> next to it and save it. ")
		TEXT("Usage: LNP.SurfaceNav.BakeOctant <LevelPath>"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&Run));
}

FString FLNPOctantBakeReport::ToString() const
{
	return FString::Printf(
		TEXT("Crust=%s (%d tris, %d Support sources) N=%d Samples=%d Valid=%d Walkable=%d NeedsExact=%d (%.2f%%) ")
		TEXT("Radius=[%.2f, %.2f] Payload=%lld bytes Time collect=%.2fs extract=%.2fs raster=%.2fs encode=%.2fs"),
		*CrustName, CrustTriangleCount, SupportSourceCount, Subdivisions, SampleCount, ValidCount, WalkableCount,
		NeedsExactCount, SampleCount > 0 ? 100.0 * NeedsExactCount / SampleCount : 0.0,
		MinRadius, MaxRadius, PayloadBytes, CollectSeconds, ExtractSeconds, RasterSeconds, EncodeSeconds);
}

bool FLNPOctantSurfaceBaker::Bake(
	const FSoftObjectPath& SourceLevel,
	const FLNPOctantBakeOptions& Options,
	ULNPOctantSurfaceData& OutData,
	FLNPOctantBakeReport& OutReport,
	FString& OutError)
{
	using namespace LNPOctantSurfaceBaker;
	OutReport = FLNPOctantBakeReport();

	FLNPCrustCodecSettings Codec = Options.Codec;
	Codec.BaseRadius = Options.BaseRadius > 0.0 ? Options.BaseRadius : GetDefault<ULNPSettings>()->SphereRadius;
	FLNPCrustRasterSettings Raster = Options.Raster;
	Raster.Subdivisions = LNPCrustAtlas::ComputeSubdivisionsForSpacing(Codec.BaseRadius, Options.CrustSpacing);

	double StartSeconds = FPlatformTime::Seconds();
	FLNPOctantSourceCollection Collection;
	if (!FLNPOctantSourceCollector::CollectFromLevel(
		SourceLevel, BakerSchemaVersion, MakeBakeSettings(Raster, Codec), Collection, OutError))
	{
		return false;
	}
	OutReport.CollectSeconds = FPlatformTime::Seconds() - StartSeconds;

	StartSeconds = FPlatformTime::Seconds();
	const UWorld* SourceWorld = LoadSourceWorld(SourceLevel);
	if (!SourceWorld)
	{
		OutError = FString::Printf(TEXT("Source Level '%s' is not loadable."), *SourceLevel.ToString());
		return false;
	}
	TArray<FLNPBakeSupportSource> Sources;
	if (!FLNPOctantTriangleExtractor::ExtractSupportSources(*SourceWorld, Sources, OutError))
	{
		return false;
	}
	for (const FLNPBakeSupportSource& Source : Sources)
	{
		if (!LNPSurfaceBake::ValidateSupportSource(Source, OutError))
		{
			return false;
		}
	}
	int32 CrustIndex = INDEX_NONE;
	if (!LNPSurfaceBake::IdentifyCrust(Sources, CrustIndex, OutError))
	{
		return false;
	}
	const FLNPBakeSupportSource& Crust = Sources[CrustIndex];
	OutReport.ExtractSeconds = FPlatformTime::Seconds() - StartSeconds;

	StartSeconds = FPlatformTime::Seconds();
	FLNPCrustAtlasRaster AtlasRaster;
	if (!LNPCrustAtlas::Rasterize(Crust.Mesh, Raster, AtlasRaster, OutError))
	{
		OutError = FString::Printf(TEXT("Crust '%s': %s"), *Crust.Name, *OutError);
		return false;
	}
	OutReport.RasterSeconds = FPlatformTime::Seconds() - StartSeconds;

	StartSeconds = FPlatformTime::Seconds();
	TArray<uint8> Payload;
	if (!LNPCrustAtlas::Encode(AtlasRaster, Codec, Payload, OutError))
	{
		OutError = FString::Printf(TEXT("Crust '%s': %s"), *Crust.Name, *OutError);
		return false;
	}
	OutReport.EncodeSeconds = FPlatformTime::Seconds() - StartSeconds;

	OutReport.CrustName = Crust.Name;
	OutReport.SupportSourceCount = Sources.Num();
	OutReport.CrustTriangleCount = Crust.Mesh.Triangles.Num();
	OutReport.Subdivisions = Raster.Subdivisions;
	OutReport.SampleCount = AtlasRaster.Samples.Num();
	OutReport.PayloadBytes = Payload.Num();
	OutReport.MinRadius = TNumericLimits<double>::Max();
	OutReport.MaxRadius = TNumericLimits<double>::Lowest();
	for (const FLNPCrustSample& Sample : AtlasRaster.Samples)
	{
		if (EnumHasAnyFlags(Sample.Flags, ELNPSupportSampleFlags::Valid))
		{
			++OutReport.ValidCount;
			OutReport.MinRadius = FMath::Min(OutReport.MinRadius, Sample.Radius);
			OutReport.MaxRadius = FMath::Max(OutReport.MaxRadius, Sample.Radius);
		}
		OutReport.WalkableCount += EnumHasAnyFlags(Sample.Flags, ELNPSupportSampleFlags::Walkable) ? 1 : 0;
		OutReport.NeedsExactCount += EnumHasAnyFlags(Sample.Flags, ELNPSupportSampleFlags::NeedsExact) ? 1 : 0;
	}

	// 실패 경로에서 기존 에셋을 반쯤 바꾸지 않도록 모든 계산이 끝난 뒤에만 쓴다.
	FLNPSurfaceBakeHeader Header;
	Header.DataVersion = FLNPSurfaceBakeHeader::CurrentDataVersion;
	Header.SourceContentHash = Collection.SourceContentHash;
	Header.SourceSemanticHash = Collection.SourceSemanticHash;
	Header.BakeSettingsHash = Collection.BakeSettingsHash;
	Header.SourceManifest = MoveTemp(Collection.Manifest);
	Header.Support.ElementCount = static_cast<uint32>(AtlasRaster.Samples.Num());
	Header.Support.UncompressedSize = static_cast<uint64>(Payload.Num());
	Header.Support.ContentHash = FLNPContentHash(FIoHash::HashBuffer(Payload.GetData(), Payload.Num()));

	OutData.Header = MoveTemp(Header);
	OutData.SupportPayload = MoveTemp(Payload);
	OutData.NavigationPayload.Reset();
	OutData.TraversalPayload.Reset();
	OutData.SpawnPayload.Reset();
	return true;
}

FString FLNPOctantSurfaceBaker::GetSurfaceDataPackageName(const FSoftObjectPath& SourceLevel)
{
	const FString LevelPackage = SourceLevel.GetLongPackageName();
	FString Name = FPackageName::GetShortName(LevelPackage);
	Name.RemoveFromStart(TEXT("LVI_Octant_"));
	return FString::Printf(TEXT("%s/DA_OctantSurface_%s"), *FPackageName::GetLongPackagePath(LevelPackage), *Name);
}

bool FLNPOctantSurfaceBaker::BakeAndSave(
	const FSoftObjectPath& SourceLevel,
	const FLNPOctantBakeOptions& Options,
	FLNPOctantBakeReport& OutReport,
	FString& OutError)
{
	const FString PackageName = GetSurfaceDataPackageName(SourceLevel);
	const FString AssetName = FPackageName::GetShortName(PackageName);

	ULNPOctantSurfaceData* Baked = NewObject<ULNPOctantSurfaceData>(GetTransientPackage());
	if (!Bake(SourceLevel, Options, *Baked, OutReport, OutError))
	{
		return false;
	}

	UPackage* Package = CreatePackage(*PackageName);
	Package->FullyLoad();
	ULNPOctantSurfaceData* SurfaceData = FindObject<ULNPOctantSurfaceData>(Package, *AssetName);
	if (!SurfaceData)
	{
		SurfaceData = NewObject<ULNPOctantSurfaceData>(Package, *AssetName, RF_Public | RF_Standalone);
		FAssetRegistryModule::AssetCreated(SurfaceData);
	}
	SurfaceData->Modify();
	SurfaceData->Header = Baked->Header;
	SurfaceData->SupportPayload = Baked->SupportPayload;
	SurfaceData->NavigationPayload.Reset();
	SurfaceData->TraversalPayload.Reset();
	SurfaceData->SpawnPayload.Reset();
	Package->MarkPackageDirty();

	const FString Filename = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
	FSavePackageArgs Args;
	Args.TopLevelFlags = RF_Public | RF_Standalone;
	if (!UPackage::SavePackage(Package, SurfaceData, *Filename, Args))
	{
		OutError = FString::Printf(TEXT("Failed to save '%s'."), *Filename);
		return false;
	}
	return true;
}
