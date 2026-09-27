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
#include "SurfaceNavigation/LNPCrustAtlas.h"
#include "SurfaceNavigation/LNPOctantSourceCollector.h"
#include "SurfaceNavigation/LNPOctantTriangleExtractor.h"
#include "SurfaceNavigation/LNPSupportLayers.h"
#include "SurfaceNavigation/LNPSurfaceBakeGeometry.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

DEFINE_LOG_CATEGORY_STATIC(LogLNPSurfaceBake, Log, All);

namespace LNPOctantSurfaceBaker
{
	TArray<FLNPOctantBakeSetting> MakeBakeSettings(
		const FLNPSupportRasterSettings& Raster,
		const FLNPSupportCodecSettings& Codec,
		const FLNPSupportLayerSettings& Layers,
		const FLNPOctantBakeOptions& Options)
	{
		return {
			FLNPOctantBakeSetting::UnsignedInteger(TEXT("Support.CodecVersion"), LNPSupportAtlas::CodecVersion),
			FLNPOctantBakeSetting::SignedInteger(TEXT("Crust.Subdivisions"), Raster.Subdivisions),
			FLNPOctantBakeSetting::Real(TEXT("Crust.WalkableMinDot"), Raster.WalkableMinDot),
			FLNPOctantBakeSetting::Real(TEXT("Crust.MaxNeighborNormalAngleDeg"), Raster.MaxNeighborNormalAngleDeg),
			FLNPOctantBakeSetting::Real(TEXT("Crust.HitMergeDistance"), Raster.HitMergeDistance),
			FLNPOctantBakeSetting::Real(TEXT("Crust.SeamSnapDistance"), Raster.SeamSnapDistance),
			FLNPOctantBakeSetting::Real(TEXT("Crust.BaseRadius"), Codec.BaseRadius),
			FLNPOctantBakeSetting::Real(TEXT("Crust.RadiusStep"), Codec.RadiusStep),
			FLNPOctantBakeSetting::SignedInteger(TEXT("Layer.SubdivisionMultiplier"), Options.LayerSubdivisionMultiplier),
			FLNPOctantBakeSetting::Real(TEXT("Layer.OverlapReportHeight"), Options.OverlapReportHeight),
			FLNPOctantBakeSetting::Real(TEXT("Layer.WalkableMinDot"), Layers.WalkableMinDot),
			FLNPOctantBakeSetting::Real(TEXT("Layer.WeldDistance"), Layers.WeldDistance),
		};
	}

	/** Raster 통계를 Layer 보고서에 채운다. */
	void FillLayerReport(const FLNPSupportLayerRaster& Raster, FLNPOctantBakeLayerReport& OutReport)
	{
		OutReport.Subdivisions = Raster.Layout.Subdivisions;
		OutReport.RowCount = Raster.Layout.Rows.Num();
		OutReport.SampleCount = Raster.Samples.Num();
		OutReport.BodyBytes = 7LL * Raster.Samples.Num();
		OutReport.MinRadius = TNumericLimits<double>::Max();
		OutReport.MaxRadius = TNumericLimits<double>::Lowest();
		for (const FLNPSupportSample& Sample : Raster.Samples)
		{
			if (EnumHasAnyFlags(Sample.Flags, ELNPSupportSampleFlags::Valid))
			{
				++OutReport.ValidCount;
				OutReport.MinRadius = FMath::Min(OutReport.MinRadius, Sample.Radius);
				OutReport.MaxRadius = FMath::Max(OutReport.MaxRadius, Sample.Radius);
			}
			OutReport.NeedsExactCount += EnumHasAnyFlags(Sample.Flags, ELNPSupportSampleFlags::NeedsExact) ? 1 : 0;
		}
		if (OutReport.ValidCount == 0)
		{
			OutReport.MinRadius = 0.0;
			OutReport.MaxRadius = 0.0;
		}
	}

	/** 비지각 Layer A의 Valid 샘플마다 앞 번호 Layer B를 같은 방향에서 조회해 겹침 높이 안이면 센다. */
	TArray<FLNPOctantBakeLayerOverlap> CountOverlaps(
		TConstArrayView<FLNPSupportLayerRaster> Rasters,
		const FLNPSupportAtlas& Atlas,
		double Height)
	{
		TArray<FLNPOctantBakeLayerOverlap> Overlaps;
		for (int32 LayerA = 1; LayerA < Rasters.Num(); ++LayerA)
		{
			const FLNPSupportLayout& Layout = Rasters[LayerA].Layout;
			TArray<int32> Counts;
			Counts.SetNumZeroed(LayerA);
			for (int32 Row = 0; Row < Layout.Rows.Num(); ++Row)
			{
				const FLNPSupportRowSpan& Span = Layout.Rows[Row];
				for (int32 I = Span.IStart; I < Span.IStart + Span.Count; ++I)
				{
					const FLNPSupportSample& Sample = Rasters[LayerA].Samples[Layout.RowOffsets[Row] + I - Span.IStart];
					if (!EnumHasAnyFlags(Sample.Flags, ELNPSupportSampleFlags::Valid))
					{
						continue;
					}
					const FVector3d Direction = LNPSupportAtlas::GetSampleDirection(Layout.Subdivisions, I, Layout.J0 + Row);
					for (int32 LayerB = 0; LayerB < LayerA; ++LayerB)
					{
						FLNPSupportLayerQuery Query;
						LNPSupportAtlas::QueryLayer(Atlas.Layers[LayerB], Direction, Query);
						const bool bOverlaps = Query.ValidCorners > 0
							&& Sample.Radius >= Query.MinCornerRadius - Height
							&& Sample.Radius <= Query.MaxCornerRadius + Height;
						Counts[LayerB] += bOverlaps ? 1 : 0;
					}
				}
			}
			for (int32 LayerB = 0; LayerB < LayerA; ++LayerB)
			{
				if (Counts[LayerB] > 0)
				{
					Overlaps.Add({LayerA, LayerB, Counts[LayerB]});
				}
			}
		}
		return Overlaps;
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
		for (const FLNPOctantBakeLayerReport& Layer : Report.Layers)
		{
			if (Layer.ValidCount == 0)
			{
				UE_LOG(LogLNPSurfaceBake, Warning,
					TEXT("[BakeOctant] A Layer of '%s' has no valid Atlas sample; it is narrower than the Layer grid."), *Layer.SourceKey);
			}
		}
	}

	static FAutoConsoleCommand Command(
		TEXT("LNP.SurfaceNav.BakeOctant"),
		TEXT("Bake the multi-layer Support Atlas of an octant LVI into DA_OctantSurface_<Name> next to it and save it. ")
		TEXT("Usage: LNP.SurfaceNav.BakeOctant <LevelPath>"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&Run));
}

FString FLNPOctantBakeReport::ToString() const
{
	FString Result = FString::Printf(
		TEXT("Crust=%s (%d tris, %d Support sources, %d Layers) N=%d Samples=%d Valid=%d Walkable=%d NeedsExact=%d (%.2f%%) ")
		TEXT("Radius=[%.2f, %.2f] TotalSamples=%d Payload=%lld bytes Time collect=%.2fs extract=%.2fs raster=%.2fs layers=%.2fs encode=%.2fs"),
		*CrustName, CrustTriangleCount, SupportSourceCount, SupportLayerCount, Subdivisions, SampleCount, ValidCount, WalkableCount,
		NeedsExactCount, SampleCount > 0 ? 100.0 * NeedsExactCount / SampleCount : 0.0,
		MinRadius, MaxRadius, TotalSampleCount, PayloadBytes, CollectSeconds, ExtractSeconds, RasterSeconds, LayerRasterSeconds, EncodeSeconds);
	for (int32 LayerId = 1; LayerId < Layers.Num(); ++LayerId)
	{
		const FLNPOctantBakeLayerReport& Layer = Layers[LayerId];
		Result += FString::Printf(
			TEXT("\n  Layer %d %s: %d tris N=%d rows=%d Samples=%d Valid=%d NeedsExact=%d (%.1f%%) Radius=[%.2f, %.2f] body=%lld bytes raster=%.3fs"),
			LayerId, *Layer.SourceKey, Layer.TriangleCount, Layer.Subdivisions, Layer.RowCount, Layer.SampleCount, Layer.ValidCount,
			Layer.NeedsExactCount, Layer.SampleCount > 0 ? 100.0 * Layer.NeedsExactCount / Layer.SampleCount : 0.0,
			Layer.MinRadius, Layer.MaxRadius, Layer.BodyBytes, Layer.RasterSeconds);
	}
	for (const FLNPOctantBakeLayerOverlap& Overlap : Overlaps)
	{
		Result += FString::Printf(TEXT("\n  Overlap Layer %d over Layer %d: %d samples"), Overlap.LayerA, Overlap.LayerB, Overlap.SampleCount);
	}
	return Result;
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

	FLNPSupportCodecSettings Codec = Options.Codec;
	Codec.BaseRadius = Options.BaseRadius > 0.0 ? Options.BaseRadius : GetDefault<ULNPSettings>()->SphereRadius;
	FLNPSupportRasterSettings Raster = Options.Raster;
	Raster.Subdivisions = LNPSupportAtlas::ComputeSubdivisionsForSpacing(Codec.BaseRadius, Options.CrustSpacing);
	const FLNPSupportLayerSettings LayerSettings;
	if (Options.LayerSubdivisionMultiplier < 1)
	{
		OutError = FString::Printf(TEXT("Invalid Layer subdivision multiplier %d."), Options.LayerSubdivisionMultiplier);
		return false;
	}

	double StartSeconds = FPlatformTime::Seconds();
	FLNPOctantSourceCollection Collection;
	if (!FLNPOctantSourceCollector::CollectFromLevel(
		SourceLevel, BakerSchemaVersion, MakeBakeSettings(Raster, Codec, LayerSettings, Options), Collection, OutError))
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
	// payload source 표는 Key 오름차순이다. 추출 순서(Actor 순회)에 기대지 않는다.
	Sources.Sort([](const FLNPBakeSupportSource& A, const FLNPBakeSupportSource& B)
	{
		return A.Key.Compare(B.Key, ESearchCase::CaseSensitive) < 0;
	});
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
	FLNPSupportLayerSet LayerSet;
	if (!LNPSupportLayers::BuildLayers(Sources, CrustIndex, LayerSettings, LayerSet, OutError))
	{
		return false;
	}
	OutReport.SupportLayerCount = LayerSet.Layers.Num();
	OutReport.ExtractSeconds = FPlatformTime::Seconds() - StartSeconds;

	TArray<FLNPSupportLayerRaster> Rasters;
	Rasters.SetNum(LayerSet.Layers.Num());
	OutReport.Layers.SetNum(LayerSet.Layers.Num());
	StartSeconds = FPlatformTime::Seconds();
	if (!LNPCrustAtlas::Rasterize(Crust.Mesh, Raster, Rasters[0], OutError))
	{
		OutError = FString::Printf(TEXT("Crust '%s': %s"), *Crust.Name, *OutError);
		return false;
	}
	Rasters[0].SourceIndex = CrustIndex;
	OutReport.RasterSeconds = FPlatformTime::Seconds() - StartSeconds;

	StartSeconds = FPlatformTime::Seconds();
	const int32 LayerSubdivisions = Raster.Subdivisions * Options.LayerSubdivisionMultiplier;
	for (int32 LayerId = 1; LayerId < LayerSet.Layers.Num(); ++LayerId)
	{
		const double LayerStartSeconds = FPlatformTime::Seconds();
		const FLNPSupportLayer& Layer = LayerSet.Layers[LayerId];
		const FLNPBakeSupportSource& Source = Sources[Layer.SourceIndex];
		const FLNPBakeTriangleMesh LayerMesh = LNPSupportLayers::MakeLayerMesh(Source.Mesh, Layer);
		if (!LNPSupportAtlas::Rasterize(
			LayerMesh, Raster, LNPSupportAtlas::ComputeFootprint(LayerMesh, LayerSubdivisions), Rasters[LayerId], OutError))
		{
			OutError = FString::Printf(TEXT("Layer %d of '%s': %s"), LayerId, *Source.Name, *OutError);
			return false;
		}
		Rasters[LayerId].SourceIndex = Layer.SourceIndex;
		OutReport.Layers[LayerId].RasterSeconds = FPlatformTime::Seconds() - LayerStartSeconds;
	}
	OutReport.LayerRasterSeconds = FPlatformTime::Seconds() - StartSeconds;

	StartSeconds = FPlatformTime::Seconds();
	TArray<FLNPSupportAtlasSource> AtlasSources;
	for (int32 SourceIndex = 0; SourceIndex < Sources.Num(); ++SourceIndex)
	{
		AtlasSources.Add({Sources[SourceIndex].Key, LayerSet.FaceMaps[SourceIndex]});
	}
	TArray<uint8> Payload;
	if (!LNPSupportAtlas::Encode(Rasters, AtlasSources, Codec, Payload, OutError))
	{
		return false;
	}
	OutReport.EncodeSeconds = FPlatformTime::Seconds() - StartSeconds;

	// 저장 전에 payload를 다시 읽어 겹침 보고에 쓴다. 디코딩 실패는 codec 결함이므로 베이크 오류다.
	FLNPSupportAtlas Decoded;
	if (!LNPSupportAtlas::Decode(Payload, Decoded, OutError))
	{
		OutError = FString::Printf(TEXT("Encoded Support payload does not decode: %s"), *OutError);
		return false;
	}
	OutReport.Overlaps = CountOverlaps(Rasters, Decoded, Options.OverlapReportHeight);

	for (int32 LayerId = 0; LayerId < Rasters.Num(); ++LayerId)
	{
		FLNPOctantBakeLayerReport& LayerReport = OutReport.Layers[LayerId];
		LayerReport.SourceKey = Sources[Rasters[LayerId].SourceIndex].Key;
		LayerReport.TriangleCount = LayerSet.Layers[LayerId].Triangles.Num();
		FillLayerReport(Rasters[LayerId], LayerReport);
		OutReport.TotalSampleCount += LayerReport.SampleCount;
	}

	const FLNPOctantBakeLayerReport& CrustReport = OutReport.Layers[0];
	OutReport.CrustName = Crust.Name;
	OutReport.SupportSourceCount = Sources.Num();
	OutReport.CrustTriangleCount = Crust.Mesh.Triangles.Num();
	OutReport.Subdivisions = Raster.Subdivisions;
	OutReport.SampleCount = CrustReport.SampleCount;
	OutReport.ValidCount = CrustReport.ValidCount;
	OutReport.NeedsExactCount = CrustReport.NeedsExactCount;
	OutReport.MinRadius = CrustReport.MinRadius;
	OutReport.MaxRadius = CrustReport.MaxRadius;
	for (const FLNPSupportSample& Sample : Rasters[0].Samples)
	{
		OutReport.WalkableCount += EnumHasAnyFlags(Sample.Flags, ELNPSupportSampleFlags::Walkable) ? 1 : 0;
	}
	OutReport.PayloadBytes = Payload.Num();

	// 실패 경로에서 기존 에셋을 반쯤 바꾸지 않도록 모든 계산이 끝난 뒤에만 쓴다.
	FLNPSurfaceBakeHeader Header;
	Header.DataVersion = FLNPSurfaceBakeHeader::CurrentDataVersion;
	Header.SourceContentHash = Collection.SourceContentHash;
	Header.SourceSemanticHash = Collection.SourceSemanticHash;
	Header.BakeSettingsHash = Collection.BakeSettingsHash;
	Header.SourceManifest = MoveTemp(Collection.Manifest);
	Header.Support.ElementCount = static_cast<uint32>(OutReport.TotalSampleCount);
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
