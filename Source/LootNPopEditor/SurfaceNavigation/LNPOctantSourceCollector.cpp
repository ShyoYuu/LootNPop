// Copyright (c) 2026 LootNPop. All rights reserved.

#include "SurfaceNavigation/LNPOctantSourceCollector.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Level.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Modules/ModuleManager.h"

namespace
{
constexpr ANSICHAR ManifestDomain[] = "LNP.OctantSourceManifest.v1";
constexpr ANSICHAR SemanticDomain[] = "LNP.OctantSourceSemantic.v1";
constexpr ANSICHAR SettingsDomain[] = "LNP.OctantBakeSettings.v1";
constexpr ANSICHAR SourceContentDomain[] = "LNP.OctantSourceContent.v1";

const FName SupportTag(TEXT("LNP.Surface.Support"));
const FName BlockerTag(TEXT("LNP.Surface.Blocker"));
const FName StaticTag(TEXT("LNP.Surface.Static"));
const FName DynamicTag(TEXT("LNP.Surface.Dynamic"));
const FName StatefulTraversalTag(TEXT("LNP.Surface.StatefulTraversal"));
const FName DestructibleTag(TEXT("LNP.Surface.Destructible"));
const FName DecorationTag(TEXT("LNP.Surface.Decoration"));

class FCanonicalBytes
{
public:
	void WriteByte(uint8 Value)
	{
		Bytes.Add(Value);
	}

	void WriteUInt32(uint32 Value)
	{
		for (uint32 Shift = 0; Shift < 32; Shift += 8)
		{
			WriteByte(static_cast<uint8>(Value >> Shift));
		}
	}

	void WriteUInt64(uint64 Value)
	{
		for (uint32 Shift = 0; Shift < 64; Shift += 8)
		{
			WriteByte(static_cast<uint8>(Value >> Shift));
		}
	}

	void WriteInt64(int64 Value)
	{
		WriteUInt64(static_cast<uint64>(Value));
	}

	void WriteDouble(double Value)
	{
		if (Value == 0.0)
		{
			Value = 0.0;
		}

		uint64 Bits = 0;
		static_assert(sizeof(Bits) == sizeof(Value));
		FMemory::Memcpy(&Bits, &Value, sizeof(Bits));
		WriteUInt64(Bits);
	}

	void WriteAnsi(const ANSICHAR* Value, int32 Length)
	{
		WriteUInt32(static_cast<uint32>(Length));
		Bytes.Append(reinterpret_cast<const uint8*>(Value), Length);
	}

	void WriteString(const FString& Value)
	{
		const FTCHARToUTF8 Utf8(*Value);
		WriteAnsi(Utf8.Get(), Utf8.Length());
	}

	void WriteName(FName Value)
	{
		WriteString(Value.ToString());
	}

	void WriteHash(const FLNPContentHash& Value)
	{
		const FIoHash IoHash = Value.ToIoHash();
		Bytes.Append(IoHash.GetBytes(), FLNPContentHash::NumBytes);
	}

	void WriteRaw(TConstArrayView<uint8> Value)
	{
		WriteUInt32(static_cast<uint32>(Value.Num()));
		Bytes.Append(Value.GetData(), Value.Num());
	}

	FLNPContentHash Finalize() const
	{
		return FLNPContentHash(FIoHash::HashBuffer(Bytes.GetData(), Bytes.Num()));
	}

	TArray<uint8> Bytes;
};

void WriteDomain(FCanonicalBytes& Writer, const ANSICHAR* Domain, int32 Length)
{
	Writer.WriteAnsi(Domain, Length);
}

bool ByteArrayLess(TConstArrayView<uint8> A, TConstArrayView<uint8> B)
{
	const int32 SharedLength = FMath::Min(A.Num(), B.Num());
	const int32 CompareResult = SharedLength > 0 ? FMemory::Memcmp(A.GetData(), B.GetData(), SharedLength) : 0;
	return CompareResult != 0 ? CompareResult < 0 : A.Num() < B.Num();
}

bool HasTag(const UActorComponent& Component, FName Tag)
{
	return Component.ComponentTags.Contains(Tag);
}

bool IsTerrainContractTag(FName Tag)
{
	return Tag == SupportTag
		|| Tag == BlockerTag
		|| Tag == StaticTag
		|| Tag == DynamicTag
		|| Tag == StatefulTraversalTag
		|| Tag == DestructibleTag
		|| Tag == DecorationTag;
}

bool IsPackageWithinRoot(const FString& PackageName, const FString& Root)
{
	return PackageName == Root || PackageName.StartsWith(Root + TEXT("/"));
}

bool TryMakePackageEntry(
	FName PackageName,
	ELNPOctantSourcePackageKind Kind,
	IAssetRegistry& AssetRegistry,
	FLNPOctantSourcePackage& OutEntry,
	FString& OutError)
{
	FAssetPackageData PackageData;
	if (AssetRegistry.TryGetAssetPackageData(PackageName, PackageData) != UE::AssetRegistry::EExists::Exists)
	{
		OutError = FString::Printf(TEXT("Asset Registry has no saved package data for '%s'."), *PackageName.ToString());
		return false;
	}

	const FIoHash SavedHash = PackageData.GetPackageSavedHash();
	if (SavedHash.IsZero())
	{
		OutError = FString::Printf(TEXT("Package '%s' has no non-zero saved hash."), *PackageName.ToString());
		return false;
	}

	OutEntry.PackageName = PackageName;
	OutEntry.Kind = Kind;
	OutEntry.PackageSavedHash = FLNPContentHash(SavedHash);
	return true;
}

void WriteManifest(FCanonicalBytes& Writer, TConstArrayView<FLNPOctantSourcePackage> Manifest)
{
	Writer.WriteUInt32(static_cast<uint32>(Manifest.Num()));
	for (const FLNPOctantSourcePackage& Entry : Manifest)
	{
		Writer.WriteName(Entry.PackageName);
		Writer.WriteByte(static_cast<uint8>(Entry.Kind));
		Writer.WriteHash(Entry.PackageSavedHash);
	}
}

bool BuildSemanticBytes(
	const FLNPTerrainSourceSemantic& Semantic,
	TArray<uint8>& OutBytes,
	FString& OutError)
{
	if (Semantic.StaticMeshPackageName.IsNone())
	{
		OutError = TEXT("Terrain semantic has no Static Mesh package name.");
		return false;
	}

	const FVector Location = Semantic.Transform.GetLocation();
	const FVector Scale = Semantic.Transform.GetScale3D();
	FQuat Rotation = Semantic.Transform.GetRotation();
	if (!FMath::IsFinite(Location.X) || !FMath::IsFinite(Location.Y) || !FMath::IsFinite(Location.Z)
		|| !FMath::IsFinite(Scale.X) || !FMath::IsFinite(Scale.Y) || !FMath::IsFinite(Scale.Z)
		|| !FMath::IsFinite(Rotation.X) || !FMath::IsFinite(Rotation.Y)
		|| !FMath::IsFinite(Rotation.Z) || !FMath::IsFinite(Rotation.W))
	{
		OutError = FString::Printf(
			TEXT("Terrain semantic for mesh '%s' has a non-finite transform."),
			*Semantic.StaticMeshPackageName.ToString());
		return false;
	}

	if (Rotation.SizeSquared() <= UE_SMALL_NUMBER)
	{
		OutError = FString::Printf(
			TEXT("Terrain semantic for mesh '%s' has an invalid rotation."),
			*Semantic.StaticMeshPackageName.ToString());
		return false;
	}
	Rotation.Normalize();

	const bool bNegateRotation = Rotation.W < 0.0
		|| (Rotation.W == 0.0 && Rotation.X < 0.0)
		|| (Rotation.W == 0.0 && Rotation.X == 0.0 && Rotation.Y < 0.0)
		|| (Rotation.W == 0.0 && Rotation.X == 0.0 && Rotation.Y == 0.0 && Rotation.Z < 0.0);
	if (bNegateRotation)
	{
		Rotation = Rotation * -1.0;
	}

	TArray<FName> Tags;
	for (FName Tag : Semantic.TerrainTags)
	{
		if (IsTerrainContractTag(Tag))
		{
			Tags.AddUnique(Tag);
		}
	}
	Tags.Sort([](FName A, FName B) { return A.LexicalLess(B); });

	FCanonicalBytes Writer;
	Writer.WriteUInt32(static_cast<uint32>(Tags.Num()));
	for (FName Tag : Tags)
	{
		Writer.WriteName(Tag);
	}
	Writer.WriteDouble(Location.X);
	Writer.WriteDouble(Location.Y);
	Writer.WriteDouble(Location.Z);
	Writer.WriteDouble(Rotation.X);
	Writer.WriteDouble(Rotation.Y);
	Writer.WriteDouble(Rotation.Z);
	Writer.WriteDouble(Rotation.W);
	Writer.WriteDouble(Scale.X);
	Writer.WriteDouble(Scale.Y);
	Writer.WriteDouble(Scale.Z);
	Writer.WriteName(Semantic.CollisionProfileName);
	Writer.WriteName(Semantic.StaticMeshPackageName);
	OutBytes = MoveTemp(Writer.Bytes);
	return true;
}

struct FCanonicalSetting
{
	FString Name;
	TArray<uint8> Bytes;
};

bool BuildSettingBytes(
	const FLNPOctantBakeSetting& Setting,
	FCanonicalSetting& OutSetting,
	FString& OutError)
{
	if (Setting.Name.IsNone())
	{
		OutError = TEXT("Bake setting name cannot be None.");
		return false;
	}
	if (Setting.Type == ELNPOctantBakeSettingType::Real && !FMath::IsFinite(Setting.RealValue))
	{
		OutError = FString::Printf(TEXT("Bake setting '%s' has a non-finite real value."), *Setting.Name.ToString());
		return false;
	}

	OutSetting.Name = Setting.Name.ToString();
	FCanonicalBytes Writer;
	Writer.WriteName(Setting.Name);
	Writer.WriteByte(static_cast<uint8>(Setting.Type));
	switch (Setting.Type)
	{
	case ELNPOctantBakeSettingType::Boolean:
		Writer.WriteByte(Setting.BoolValue ? 1 : 0);
		break;
	case ELNPOctantBakeSettingType::SignedInteger:
		Writer.WriteInt64(Setting.SignedIntegerValue);
		break;
	case ELNPOctantBakeSettingType::UnsignedInteger:
		Writer.WriteUInt64(Setting.UnsignedIntegerValue);
		break;
	case ELNPOctantBakeSettingType::Real:
		Writer.WriteDouble(Setting.RealValue);
		break;
	case ELNPOctantBakeSettingType::String:
		Writer.WriteString(Setting.StringValue);
		break;
	default:
		OutError = FString::Printf(TEXT("Bake setting '%s' has an unknown type."), *Setting.Name.ToString());
		return false;
	}

	OutSetting.Bytes = MoveTemp(Writer.Bytes);
	return true;
}
}

FLNPOctantBakeSetting FLNPOctantBakeSetting::Boolean(FName InName, bool bInValue)
{
	FLNPOctantBakeSetting Result;
	Result.Name = InName;
	Result.Type = ELNPOctantBakeSettingType::Boolean;
	Result.BoolValue = bInValue;
	return Result;
}

FLNPOctantBakeSetting FLNPOctantBakeSetting::SignedInteger(FName InName, int64 InValue)
{
	FLNPOctantBakeSetting Result;
	Result.Name = InName;
	Result.Type = ELNPOctantBakeSettingType::SignedInteger;
	Result.SignedIntegerValue = InValue;
	return Result;
}

FLNPOctantBakeSetting FLNPOctantBakeSetting::UnsignedInteger(FName InName, uint64 InValue)
{
	FLNPOctantBakeSetting Result;
	Result.Name = InName;
	Result.Type = ELNPOctantBakeSettingType::UnsignedInteger;
	Result.UnsignedIntegerValue = InValue;
	return Result;
}

FLNPOctantBakeSetting FLNPOctantBakeSetting::Real(FName InName, double InValue)
{
	FLNPOctantBakeSetting Result;
	Result.Name = InName;
	Result.Type = ELNPOctantBakeSettingType::Real;
	Result.RealValue = InValue;
	return Result;
}

FLNPOctantBakeSetting FLNPOctantBakeSetting::String(FName InName, FString InValue)
{
	FLNPOctantBakeSetting Result;
	Result.Name = InName;
	Result.Type = ELNPOctantBakeSettingType::String;
	Result.StringValue = MoveTemp(InValue);
	return Result;
}

bool FLNPOctantSourceCollector::CollectFromLevel(
	const FSoftObjectPath& SourceLevel,
	uint32 BakerSchemaVersion,
	TConstArrayView<FLNPOctantBakeSetting> BakeSettings,
	FLNPOctantSourceCollection& OutCollection,
	FString& OutError)
{
	OutCollection = FLNPOctantSourceCollection();
	OutError.Reset();

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	const FAssetData SourceAsset = AssetRegistry.GetAssetByObjectPath(SourceLevel);
	if (!SourceAsset.IsValid())
	{
		OutError = FString::Printf(TEXT("Source Level asset '%s' was not found."), *SourceLevel.ToString());
		return false;
	}

	TSet<FName> LoadTags;
	LoadTags.Add(ULevel::LoadAllExternalObjectsTag);
	const UWorld* SourceWorld = Cast<UWorld>(SourceAsset.GetAsset(MoveTemp(LoadTags)));
	if (!SourceWorld)
	{
		OutError = FString::Printf(TEXT("Source asset '%s' is not a loadable World."), *SourceLevel.ToString());
		return false;
	}

	return CollectFromWorld(
		*SourceWorld,
		SourceLevel.GetLongPackageFName(),
		AssetRegistry,
		BakerSchemaVersion,
		BakeSettings,
		OutCollection,
		OutError);
}

bool FLNPOctantSourceCollector::CollectFromWorld(
	const UWorld& SourceWorld,
	FName SourceLevelPackageName,
	IAssetRegistry& AssetRegistry,
	uint32 BakerSchemaVersion,
	TConstArrayView<FLNPOctantBakeSetting> BakeSettings,
	FLNPOctantSourceCollection& OutCollection,
	FString& OutError)
{
	OutCollection = FLNPOctantSourceCollection();
	OutError.Reset();

	if (!CollectLevelPackages(SourceLevelPackageName, AssetRegistry, OutCollection.Manifest, OutError)
		|| !CollectTerrainComponents(
			SourceWorld,
			AssetRegistry,
			OutCollection.Manifest,
			OutCollection.TerrainSemantics,
			OutError)
		|| !CanonicalizeManifest(OutCollection.Manifest, OutError)
		|| !BuildSourceSemanticHash(
			OutCollection.TerrainSemantics,
			OutCollection.SourceSemanticHash,
			OutError)
		|| !BuildBakeSettingsHash(
			BakerSchemaVersion,
			BakeSettings,
			OutCollection.BakeSettingsHash,
			OutError)
		|| !BuildSourceContentHash(
			OutCollection.Manifest,
			OutCollection.SourceSemanticHash,
			OutCollection.BakeSettingsHash,
			OutCollection.SourceContentHash,
			OutError))
	{
		OutCollection = FLNPOctantSourceCollection();
		return false;
	}

	return true;
}

bool FLNPOctantSourceCollector::CollectLevelPackages(
	FName SourceLevelPackageName,
	IAssetRegistry& AssetRegistry,
	TArray<FLNPOctantSourcePackage>& OutManifest,
	FString& OutError)
{
	OutManifest.Reset();
	OutError.Reset();
	if (SourceLevelPackageName.IsNone())
	{
		OutError = TEXT("Source Level package name cannot be None.");
		return false;
	}

	FLNPOctantSourcePackage SourceEntry;
	if (!TryMakePackageEntry(
		SourceLevelPackageName,
		ELNPOctantSourcePackageKind::SourceLevel,
		AssetRegistry,
		SourceEntry,
		OutError))
	{
		return false;
	}
	OutManifest.Add(MoveTemp(SourceEntry));

	const TArray<FString> ExternalRoots = ULevel::GetExternalObjectsPaths(SourceLevelPackageName.ToString());
	TArray<FName> DirectDependencies;
	if (!AssetRegistry.GetDependencies(
		SourceLevelPackageName,
		DirectDependencies,
		UE::AssetRegistry::EDependencyCategory::Package))
	{
		OutError = FString::Printf(
			TEXT("Asset Registry could not enumerate direct dependencies for '%s'."),
			*SourceLevelPackageName.ToString());
		return false;
	}

	for (FName Dependency : DirectDependencies)
	{
		const FString DependencyString = Dependency.ToString();
		const bool bIsOwnedExternalPackage = ExternalRoots.ContainsByPredicate(
			[&DependencyString](const FString& Root)
			{
				return IsPackageWithinRoot(DependencyString, Root);
			});
		if (!bIsOwnedExternalPackage)
		{
			continue;
		}

		FLNPOctantSourcePackage ExternalEntry;
		if (!TryMakePackageEntry(
			Dependency,
			ELNPOctantSourcePackageKind::ExternalActor,
			AssetRegistry,
			ExternalEntry,
			OutError))
		{
			return false;
		}
		OutManifest.Add(MoveTemp(ExternalEntry));
	}

	return CanonicalizeManifest(OutManifest, OutError);
}

bool FLNPOctantSourceCollector::CollectTerrainComponents(
	const UWorld& SourceWorld,
	IAssetRegistry& AssetRegistry,
	TArray<FLNPOctantSourcePackage>& InOutManifest,
	TArray<FLNPTerrainSourceSemantic>& OutSemantics,
	FString& OutError)
{
	OutSemantics.Reset();
	OutError.Reset();
	if (!SourceWorld.PersistentLevel)
	{
		OutError = TEXT("Source World has no persistent Level.");
		return false;
	}

	for (const AActor* Actor : SourceWorld.PersistentLevel->Actors)
	{
		if (!IsValid(Actor))
		{
			continue;
		}

		TArray<UPrimitiveComponent*> PrimitiveComponents;
		Actor->GetComponents(PrimitiveComponents);
		for (const UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
		{
			if (!IsValid(PrimitiveComponent))
			{
				continue;
			}

			const bool bHasRole = HasTag(*PrimitiveComponent, SupportTag) || HasTag(*PrimitiveComponent, BlockerTag);
			const bool bIsDecoration = HasTag(*PrimitiveComponent, DecorationTag);
			if (!bHasRole)
			{
				continue;
			}
			if (bIsDecoration)
			{
				OutError = FString::Printf(
					TEXT("Component '%s' combines Decoration with a Terrain Contract role."),
					*PrimitiveComponent->GetPathName());
				return false;
			}

			const int32 LifecycleCount = static_cast<int32>(HasTag(*PrimitiveComponent, StaticTag))
				+ static_cast<int32>(HasTag(*PrimitiveComponent, DynamicTag))
				+ static_cast<int32>(HasTag(*PrimitiveComponent, StatefulTraversalTag))
				+ static_cast<int32>(HasTag(*PrimitiveComponent, DestructibleTag));
			if (LifecycleCount != 1)
			{
				OutError = FString::Printf(
					TEXT("Component '%s' must have exactly one Terrain Contract lifecycle tag."),
					*PrimitiveComponent->GetPathName());
				return false;
			}

			const UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(PrimitiveComponent);
			if (!StaticMeshComponent || !StaticMeshComponent->GetStaticMesh())
			{
				OutError = FString::Printf(
					TEXT("Terrain Contract role component '%s' must reference a Static Mesh."),
					*PrimitiveComponent->GetPathName());
				return false;
			}

			const FName MeshPackageName = StaticMeshComponent->GetStaticMesh()->GetOutermost()->GetFName();
			FLNPOctantSourcePackage MeshEntry;
			if (!TryMakePackageEntry(
				MeshPackageName,
				ELNPOctantSourcePackageKind::TerrainMesh,
				AssetRegistry,
				MeshEntry,
				OutError))
			{
				return false;
			}
			InOutManifest.Add(MoveTemp(MeshEntry));

			FLNPTerrainSourceSemantic& Semantic = OutSemantics.AddDefaulted_GetRef();
			for (FName Tag : PrimitiveComponent->ComponentTags)
			{
				if (IsTerrainContractTag(Tag))
				{
					Semantic.TerrainTags.Add(Tag);
				}
			}
			Semantic.Transform = PrimitiveComponent->GetComponentTransform();
			Semantic.CollisionProfileName = PrimitiveComponent->GetCollisionProfileName();
			Semantic.StaticMeshPackageName = MeshPackageName;
		}
	}

	return CanonicalizeManifest(InOutManifest, OutError);
}

bool FLNPOctantSourceCollector::CanonicalizeManifest(
	TArray<FLNPOctantSourcePackage>& InOutManifest,
	FString& OutError)
{
	OutError.Reset();
	InOutManifest.Sort([](const FLNPOctantSourcePackage& A, const FLNPOctantSourcePackage& B)
	{
		if (A.PackageName != B.PackageName)
		{
			return A.PackageName.LexicalLess(B.PackageName);
		}
		if (A.Kind != B.Kind)
		{
			return static_cast<uint8>(A.Kind) < static_cast<uint8>(B.Kind);
		}
		return A.PackageSavedHash.ToIoHash() < B.PackageSavedHash.ToIoHash();
	});

	for (int32 Index = InOutManifest.Num() - 1; Index > 0; --Index)
	{
		const FLNPOctantSourcePackage& Current = InOutManifest[Index];
		const FLNPOctantSourcePackage& Previous = InOutManifest[Index - 1];
		if (Current.PackageName == Previous.PackageName && Current.Kind == Previous.Kind)
		{
			if (Current.PackageSavedHash != Previous.PackageSavedHash)
			{
				OutError = FString::Printf(
					TEXT("Manifest has conflicting saved hashes for package '%s' and kind %u."),
					*Current.PackageName.ToString(),
					static_cast<uint8>(Current.Kind));
				return false;
			}
			InOutManifest.RemoveAt(Index);
		}
	}

	return true;
}

bool FLNPOctantSourceCollector::BuildSourceSemanticHash(
	TConstArrayView<FLNPTerrainSourceSemantic> Semantics,
	FLNPContentHash& OutHash,
	FString& OutError)
{
	OutError.Reset();
	TArray<TArray<uint8>> Entries;
	Entries.Reserve(Semantics.Num());
	for (const FLNPTerrainSourceSemantic& Semantic : Semantics)
	{
		TArray<uint8>& Entry = Entries.AddDefaulted_GetRef();
		if (!BuildSemanticBytes(Semantic, Entry, OutError))
		{
			OutHash = FLNPContentHash();
			return false;
		}
	}
	Entries.Sort([](const TArray<uint8>& A, const TArray<uint8>& B)
	{
		return ByteArrayLess(A, B);
	});

	FCanonicalBytes Writer;
	WriteDomain(Writer, SemanticDomain, UE_ARRAY_COUNT(SemanticDomain) - 1);
	Writer.WriteUInt32(static_cast<uint32>(Entries.Num()));
	for (const TArray<uint8>& Entry : Entries)
	{
		Writer.WriteRaw(Entry);
	}
	OutHash = Writer.Finalize();
	return true;
}

bool FLNPOctantSourceCollector::BuildBakeSettingsHash(
	uint32 BakerSchemaVersion,
	TConstArrayView<FLNPOctantBakeSetting> BakeSettings,
	FLNPContentHash& OutHash,
	FString& OutError)
{
	OutError.Reset();
	TArray<FCanonicalSetting> Settings;
	Settings.Reserve(BakeSettings.Num());
	for (const FLNPOctantBakeSetting& Setting : BakeSettings)
	{
		FCanonicalSetting& CanonicalSetting = Settings.AddDefaulted_GetRef();
		if (!BuildSettingBytes(Setting, CanonicalSetting, OutError))
		{
			OutHash = FLNPContentHash();
			return false;
		}
	}

	Settings.Sort([](const FCanonicalSetting& A, const FCanonicalSetting& B)
	{
		const int32 NameCompare = A.Name.Compare(B.Name, ESearchCase::CaseSensitive);
		return NameCompare != 0 ? NameCompare < 0 : ByteArrayLess(A.Bytes, B.Bytes);
	});
	for (int32 Index = Settings.Num() - 1; Index > 0; --Index)
	{
		if (Settings[Index].Name == Settings[Index - 1].Name)
		{
			if (Settings[Index].Bytes != Settings[Index - 1].Bytes)
			{
				OutError = FString::Printf(
					TEXT("Bake setting '%s' is defined with conflicting values."),
					*Settings[Index].Name);
				OutHash = FLNPContentHash();
				return false;
			}
			Settings.RemoveAt(Index);
		}
	}

	FCanonicalBytes Writer;
	WriteDomain(Writer, SettingsDomain, UE_ARRAY_COUNT(SettingsDomain) - 1);
	Writer.WriteUInt32(BakerSchemaVersion);
	Writer.WriteUInt32(static_cast<uint32>(Settings.Num()));
	for (const FCanonicalSetting& Setting : Settings)
	{
		Writer.WriteRaw(Setting.Bytes);
	}
	OutHash = Writer.Finalize();
	return true;
}

bool FLNPOctantSourceCollector::BuildSourceContentHash(
	TConstArrayView<FLNPOctantSourcePackage> Manifest,
	const FLNPContentHash& SourceSemanticHash,
	const FLNPContentHash& BakeSettingsHash,
	FLNPContentHash& OutHash,
	FString& OutError)
{
	TArray<FLNPOctantSourcePackage> CanonicalManifest;
	CanonicalManifest.Append(Manifest);
	if (!CanonicalizeManifest(CanonicalManifest, OutError))
	{
		OutHash = FLNPContentHash();
		return false;
	}

	FCanonicalBytes Writer;
	WriteDomain(Writer, SourceContentDomain, UE_ARRAY_COUNT(SourceContentDomain) - 1);
	WriteDomain(Writer, ManifestDomain, UE_ARRAY_COUNT(ManifestDomain) - 1);
	WriteManifest(Writer, CanonicalManifest);
	Writer.WriteHash(SourceSemanticHash);
	Writer.WriteHash(BakeSettingsHash);
	OutHash = Writer.Finalize();
	return true;
}
