#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetCompilingManager.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "CollisionQueryParams.h"
#include "Editor.h"
#include "Engine/Level.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#include "InteractiveToolsContext.h"
#include "InteractiveToolManager.h"
#include "LevelInstance/LevelInstanceActor.h"
#include "Materials/Material.h"
#include "MeshDescription.h"
#include "MeshPartitionHeightSculptTool.h"
#include "Misc/PackageName.h"
#include "PackageTools.h"
#include "PCGComponent.h"
#include "Data/PCGSpatialData.h"
#include "PCGGraph.h"
#include "PhysicsEngine/BodySetup.h"
#include "Sculpting/MeshBrushOpBase.h"
#include "SingleSelectionTool.h"
#include "StaticMeshAttributes.h"
#include "TargetInterfaces/DynamicMeshCommitter.h"
#include "TargetInterfaces/DynamicMeshProvider.h"
#include "TargetInterfaces/MaterialProvider.h"
#include "TargetInterfaces/SceneComponentBackedTarget.h"
#include "ToolContextInterfaces.h"
#include "ToolTargetManager.h"
#include "ToolTargets/StaticMeshComponentToolTarget.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/UObjectHash.h"
#include "WorldPartition/LevelInstance/LevelInstanceActorDesc.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionActorDescInstance.h"
#include "WorldPartition/WorldPartitionHelpers.h"

namespace
{
constexpr TCHAR SourceMeshObjectPath[] = TEXT("/Game/Maps/Meshes/SM_Octant_00.SM_Octant_00");
constexpr TCHAR SculptedMeshPackageName[] = TEXT("/Game/SurfaceNavigationTests/MeshTerrain/SM_COptionSphereSculpt");
constexpr TCHAR SculptedMeshAssetName[] = TEXT("SM_COptionSphereSculpt");
constexpr TCHAR MeadowOctantBlueprintClassPath[] =
	TEXT("/Game/Maps/Meadow_00/BP_Octant_Meadow_00.BP_Octant_Meadow_00_C");
constexpr TCHAR COptionOctantBlueprintClassPath[] =
	TEXT("/Game/SurfaceNavigationTests/MeshTerrain/BP_Octant_COption.BP_Octant_COption_C");
constexpr TCHAR MeadowOctantLevelWorldPath[] =
	TEXT("/Game/Maps/Meadow_00/LVI_Octant_Meadow_00.LVI_Octant_Meadow_00");
constexpr TCHAR COptionOctantLevelWorldPath[] =
	TEXT("/Game/SurfaceNavigationTests/MeshTerrain/LVI_Octant_COption.LVI_Octant_COption");
constexpr TCHAR COptionEightSlotWorldPath[] =
	TEXT("/Game/SurfaceNavigationTests/MeshTerrain/L_COptionEightSlotIntegration.L_COptionEightSlotIntegration");
constexpr TCHAR BOptionAuthoringWorldPackageName[] =
	TEXT("/Game/SurfaceNavigationTests/MeshTerrain/L_BOptionMeshTerrainAuthoring");
constexpr TCHAR BOptionExtractedMeshPackageName[] =
	TEXT("/Game/SurfaceNavigationTests/MeshTerrain/SM_BOptionExtracted");
constexpr TCHAR BOptionExtractedMeshAssetName[] = TEXT("SM_BOptionExtracted");

struct FStaticMeshSnapshot
{
	int32 VertexCount = 0;
	int32 TriangleCount = 0;
	int32 PolygonCount = 0;
	TArray<FVector3f> VertexPositions;
	TArray<FStaticMaterial> Materials;
	FMeshNaniteSettings NaniteSettings;
	ECollisionTraceFlag CollisionTraceFlag = CTF_UseDefault;
	bool bDoubleSidedGeometry = false;
	bool bHasPhysicsTriMeshData = false;
};

bool CaptureStaticMeshSnapshot(UStaticMesh* Mesh, FStaticMeshSnapshot& OutSnapshot)
{
	if (!Mesh)
	{
		return false;
	}

	FAssetCompilingManager::Get().FinishAllCompilation();
	const FMeshDescription* MeshDescription = Mesh->GetMeshDescription(0);
	if (!MeshDescription)
	{
		return false;
	}

	OutSnapshot.VertexCount = MeshDescription->Vertices().Num();
	OutSnapshot.TriangleCount = MeshDescription->Triangles().Num();
	OutSnapshot.PolygonCount = MeshDescription->Polygons().Num();
	OutSnapshot.VertexPositions.Reset(OutSnapshot.VertexCount);

	const FStaticMeshConstAttributes Attributes(*MeshDescription);
	const TVertexAttributesConstRef<FVector3f> Positions = Attributes.GetVertexPositions();
	for (const FVertexID VertexID : MeshDescription->Vertices().GetElementIDs())
	{
		OutSnapshot.VertexPositions.Add(Positions[VertexID]);
	}

	OutSnapshot.Materials = Mesh->GetStaticMaterials();
	OutSnapshot.NaniteSettings = Mesh->GetNaniteSettings();
	if (const UBodySetup* BodySetup = Mesh->GetBodySetup())
	{
		OutSnapshot.CollisionTraceFlag = BodySetup->GetCollisionTraceFlag();
		OutSnapshot.bDoubleSidedGeometry = BodySetup->bDoubleSidedGeometry;
		OutSnapshot.bHasPhysicsTriMeshData = Mesh->ContainsPhysicsTriMeshData(true);
	}
	return true;
}

double GetMaximumVertexDisplacement(
	const TArray<FVector3f>& BeforePositions,
	const TArray<FVector3f>& AfterPositions)
{
	if (BeforePositions.Num() != AfterPositions.Num())
	{
		return TNumericLimits<double>::Max();
	}

	double MaximumSquaredDisplacement = 0.0;
	for (int32 Index = 0; Index < BeforePositions.Num(); ++Index)
	{
		MaximumSquaredDisplacement = FMath::Max(
			MaximumSquaredDisplacement,
			static_cast<double>(FVector3f::DistSquared(BeforePositions[Index], AfterPositions[Index])));
	}
	return FMath::Sqrt(MaximumSquaredDisplacement);
}

double GetMaximumVertexDisplacement(
	const TArray<FVector3d>& BeforePositions,
	const TArray<FVector3d>& AfterPositions)
{
	if (BeforePositions.Num() != AfterPositions.Num())
	{
		return TNumericLimits<double>::Max();
	}

	double MaximumSquaredDisplacement = 0.0;
	for (int32 Index = 0; Index < BeforePositions.Num(); ++Index)
	{
		MaximumSquaredDisplacement = FMath::Max(
			MaximumSquaredDisplacement,
			FVector3d::DistSquared(BeforePositions[Index], AfterPositions[Index]));
	}
	return FMath::Sqrt(MaximumSquaredDisplacement);
}

TArray<FVector3d> CaptureToolVertexPositions(UMeshVertexSculptTool* SculptTool)
{
	TArray<FVector3d> Positions;
	SculptTool->UpdateToolMeshes(
		[&Positions](UE::Geometry::FDynamicMesh3& Mesh, int32 MeshIndex)
		{
			Positions.Reserve(Mesh.VertexCount());
			for (const int32 VertexID : Mesh.VertexIndicesItr())
			{
				Positions.Add(Mesh.GetVertex(VertexID));
			}
			return TUniquePtr<FMeshRegionChangeBase>();
		});
	return Positions;
}

bool IsOctantSeamPosition(const FVector3f& Position, const float Tolerance = 1.0f)
{
	return FMath::Abs(Position.X) <= Tolerance
		|| FMath::Abs(Position.Y) <= Tolerance
		|| FMath::Abs(Position.Z) <= Tolerance;
}

FIntVector QuantizePosition(const FVector& Position, const double GridSize = 0.1)
{
	return FIntVector(
		FMath::RoundToInt(Position.X / GridSize),
		FMath::RoundToInt(Position.Y / GridSize),
		FMath::RoundToInt(Position.Z / GridSize));
}

TArray<FRotator> GetRuntimeOctantSlotRotations()
{
	TArray<FRotator> Rotations;
	Rotations.Reserve(8);
	for (int32 Hemisphere = 0; Hemisphere < 2; ++Hemisphere)
	{
		for (int32 Quarter = 0; Quarter < 4; ++Quarter)
		{
			Rotations.Emplace(Hemisphere * 180.0, Quarter * 90.0, 0.0);
		}
	}
	return Rotations;
}

class FMeshTerrainTestQueries final : public IToolsContextQueriesAPI
{
public:
	UWorld* World = nullptr;
	UStaticMeshComponent* SelectedComponent = nullptr;
	UInteractiveToolsContext* ToolsContext = nullptr;

	virtual UWorld* GetCurrentEditingWorld() const override
	{
		return World;
	}

	virtual void GetCurrentSelectionState(FToolBuilderState& StateOut) const override
	{
		StateOut.World = World;
		StateOut.ToolManager = ToolsContext ? ToolsContext->ToolManager : nullptr;
		StateOut.TargetManager = ToolsContext ? ToolsContext->TargetManager : nullptr;
		StateOut.GizmoManager = ToolsContext ? ToolsContext->GizmoManager : nullptr;
		if (SelectedComponent)
		{
			StateOut.SelectedComponents.Add(SelectedComponent);
		}
	}

	virtual void GetCurrentViewState(FViewCameraState& StateOut) const override
	{
		StateOut.Position = FVector::ZeroVector;
		StateOut.Orientation = FQuat::Identity;
	}

	virtual UMaterialInterface* GetStandardMaterial(EStandardToolContextMaterials MaterialType) const override
	{
		return UMaterial::GetDefaultMaterial(MD_Surface);
	}

	virtual FViewport* GetHoveredViewport() const override
	{
		return nullptr;
	}

	virtual FViewport* GetFocusedViewport() const override
	{
		return nullptr;
	}
};

class FMeshTerrainTestTransactions final : public IToolsContextTransactionsAPI
{
public:
	virtual void DisplayMessage(const FText& Message, EToolMessageLevel Level) override
	{
	}

	virtual void PostInvalidation() override
	{
	}

	virtual void BeginUndoTransaction(const FText& Description) override
	{
	}

	virtual void EndUndoTransaction() override
	{
	}

	virtual void CancelUndoTransaction() override
	{
	}

	virtual void AppendChange(
		UObject* TargetObject,
		TUniquePtr<FToolCommandChange> Change,
		const FText& Description) override
	{
	}

	virtual bool RequestSelectionChange(const FSelectedObjectsChangeList& SelectionChange) override
	{
		return false;
	}
};

UStaticMesh* LoadOrCreateSculptExperimentAsset(UStaticMesh* SourceMesh)
{
	const FString ObjectPath = FString::Printf(TEXT("%s.%s"), SculptedMeshPackageName, SculptedMeshAssetName);
	UStaticMesh* ExperimentMesh = LoadObject<UStaticMesh>(nullptr, *ObjectPath);
	if (!ExperimentMesh)
	{
		UPackage* Package = CreatePackage(SculptedMeshPackageName);
		ExperimentMesh = DuplicateObject<UStaticMesh>(SourceMesh, Package, SculptedMeshAssetName);
		if (ExperimentMesh)
		{
			ExperimentMesh->SetFlags(RF_Public | RF_Standalone);
			ExperimentMesh->ClearFlags(RF_Transient);
			FAssetRegistryModule::AssetCreated(ExperimentMesh);
			Package->MarkPackageDirty();
		}
	}

	if (!ExperimentMesh)
	{
		return nullptr;
	}

	FMeshDescription SourceMeshDescription;
	if (!SourceMesh->CloneMeshDescription(0, SourceMeshDescription))
	{
		return nullptr;
	}

	ExperimentMesh->Modify();
	if (!ExperimentMesh->CreateMeshDescription(0, MoveTemp(SourceMeshDescription)))
	{
		return nullptr;
	}
	ExperimentMesh->SetStaticMaterials(SourceMesh->GetStaticMaterials());
	if (const UBodySetup* SourceBodySetup = SourceMesh->GetBodySetup())
	{
		if (UBodySetup* ExperimentBodySetup = ExperimentMesh->GetBodySetup())
		{
			ExperimentBodySetup->CollisionTraceFlag = SourceBodySetup->CollisionTraceFlag;
			ExperimentBodySetup->bDoubleSidedGeometry = SourceBodySetup->bDoubleSidedGeometry;
		}
	}
	FMeshNaniteSettings NaniteSettings = ExperimentMesh->GetNaniteSettings();
	NaniteSettings.bEnabled = true;
	ExperimentMesh->SetNaniteSettings(NaniteSettings);
	ExperimentMesh->CommitMeshDescription(0);
	ExperimentMesh->Build(false);
	FAssetCompilingManager::Get().FinishAllCompilation();
	ExperimentMesh->MarkPackageDirty();
	return ExperimentMesh;
}

UStaticMesh* FindCurrentBOptionEmbeddedMesh()
{
	UWorld* EditorWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!EditorWorld
		|| EditorWorld->GetOutermost()->GetName() != BOptionAuthoringWorldPackageName
		|| !EditorWorld->PersistentLevel)
	{
		return nullptr;
	}

	TArray<UObject*> NestedObjects;
	GetObjectsWithOuter(
		EditorWorld->PersistentLevel,
		NestedObjects,
		EGetObjectsFlags::IncludeNestedObjects);
	for (UObject* Object : NestedObjects)
	{
		UStaticMesh* StaticMesh = Cast<UStaticMesh>(Object);
		if (StaticMesh && StaticMesh->GetName().StartsWith(TEXT("MeshPartitionStaticMesh")))
		{
			return StaticMesh;
		}
	}
	return nullptr;
}

UStaticMesh* ExtractBOptionMesh(UStaticMesh* EmbeddedMesh)
{
	FMeshDescription EmbeddedMeshDescription;
	if (!EmbeddedMesh || !EmbeddedMesh->CloneMeshDescription(0, EmbeddedMeshDescription))
	{
		return nullptr;
	}

	const FString ObjectPath = FString::Printf(
		TEXT("%s.%s"),
		BOptionExtractedMeshPackageName,
		BOptionExtractedMeshAssetName);
	UStaticMesh* ExtractedMesh = LoadObject<UStaticMesh>(nullptr, *ObjectPath);
	if (!ExtractedMesh)
	{
		UPackage* Package = CreatePackage(BOptionExtractedMeshPackageName);
		ExtractedMesh = NewObject<UStaticMesh>(
			Package,
			BOptionExtractedMeshAssetName,
			RF_Public | RF_Standalone);
		if (ExtractedMesh)
		{
			ExtractedMesh->AddSourceModel();
			FAssetRegistryModule::AssetCreated(ExtractedMesh);
		}
	}
	else if (ExtractedMesh->GetNumSourceModels() == 0)
	{
		ExtractedMesh->AddSourceModel();
	}

	if (!ExtractedMesh)
	{
		return nullptr;
	}

	ExtractedMesh->Modify();
	if (!ExtractedMesh->CreateMeshDescription(0, MoveTemp(EmbeddedMeshDescription)))
	{
		return nullptr;
	}
	ExtractedMesh->SetStaticMaterials(EmbeddedMesh->GetStaticMaterials());
	ExtractedMesh->CreateBodySetup();
	if (UBodySetup* BodySetup = ExtractedMesh->GetBodySetup())
	{
		BodySetup->CollisionTraceFlag = CTF_UseComplexAsSimple;
		BodySetup->bDoubleSidedGeometry = true;
	}
	FMeshNaniteSettings NaniteSettings = ExtractedMesh->GetNaniteSettings();
	NaniteSettings.bEnabled = true;
	ExtractedMesh->SetNaniteSettings(NaniteSettings);
	ExtractedMesh->CommitMeshDescription(0);
	ExtractedMesh->Build(false);
	FAssetCompilingManager::Get().FinishAllCompilation();
	ExtractedMesh->MarkPackageDirty();
	return ExtractedMesh;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPMeshTerrainNonWPStaticMeshTargetTest,
	"LootNPop.SurfaceNavigation.MeshTerrain.NonWPStaticMeshTarget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLNPMeshTerrainNonWPStaticMeshTargetTest::RunTest(const FString& Parameters)
{
	UWorld* TestWorld = NewObject<UWorld>(GetTransientPackage(), NAME_None, RF_Transient);
	TestNotNull(TEXT("Transient test world exists"), TestWorld);
	if (!TestWorld)
	{
		return false;
	}

	TestNull(TEXT("Transient test world has no World Partition"), TestWorld->GetWorldPartition());

	UStaticMesh* SourceMesh = LoadObject<UStaticMesh>(nullptr, SourceMeshObjectPath);
	TestNotNull(TEXT("Production octant source mesh is available for read-only duplication"), SourceMesh);
	if (!SourceMesh)
	{
		return false;
	}

	UPackage* TestPackage = CreatePackage(TEXT("/Game/SurfaceNavigationTests/Transient/SM_COptionTarget"));
	TestPackage->SetFlags(RF_Transient);
	UStaticMesh* WritableMesh = DuplicateObject<UStaticMesh>(SourceMesh, TestPackage, TEXT("SM_COptionTarget"));
	TestNotNull(TEXT("Writable transient mesh copy exists"), WritableMesh);
	if (!WritableMesh)
	{
		return false;
	}
	FAssetCompilingManager::Get().FinishAllCompilation();

	UStaticMeshComponent* MeshComponent = NewObject<UStaticMeshComponent>(TestWorld, NAME_None, RF_Transient);
	MeshComponent->SetStaticMesh(WritableMesh);
	TestTrue(TEXT("Static Mesh target factory treats the project-package copy as writable"),
		UStaticMeshComponentToolTargetFactory::CanWriteToSource(MeshComponent));

	UToolTargetManager* TargetManager = NewObject<UToolTargetManager>();
	TargetManager->Initialize();
	UStaticMeshComponentToolTargetFactory* TargetFactory =
		NewObject<UStaticMeshComponentToolTargetFactory>(TargetManager);
	TargetManager->AddTargetFactory(TargetFactory);

	const FToolTargetTypeRequirements HeightSculptRequirements({
		UMaterialProvider::StaticClass(),
		UDynamicMeshProvider::StaticClass(),
		UDynamicMeshCommitter::StaticClass(),
		USceneComponentBackedTarget::StaticClass()
	});
	TestTrue(TEXT("Writable Static Mesh target class satisfies Height Sculpt requirements"),
		HeightSculptRequirements.AreSatisfiedBy(UStaticMeshComponentToolTarget::StaticClass()));
	TestTrue(TEXT("Static Mesh target factory can build the writable mesh target"),
		TargetFactory->CanBuildTarget(MeshComponent, HeightSculptRequirements));

	FToolBuilderState BuilderState;
	BuilderState.World = TestWorld;
	BuilderState.ToolManager = NewObject<UInteractiveToolManager>();
	BuilderState.TargetManager = TargetManager;
	BuilderState.SelectedComponents.Add(MeshComponent);

	UE::MeshPartition::UHeightSculptToolBuilder* Builder =
		NewObject<UE::MeshPartition::UHeightSculptToolBuilder>();
	const bool bCanBuild = Builder->CanBuildTool(BuilderState);
	TestTrue(TEXT("Sphere Height Sculpt builder accepts one regular Static Mesh component"), bCanBuild);

	if (bCanBuild)
	{
		UInteractiveTool* BuiltTool = Builder->BuildTool(BuilderState);
		UE::MeshPartition::UHeightSculptTool* HeightSculptTool =
			Cast<UE::MeshPartition::UHeightSculptTool>(BuiltTool);
		TestNotNull(TEXT("Builder creates the Mesh Partition Height Sculpt tool"), HeightSculptTool);

		UToolTarget* Target = HeightSculptTool ? HeightSculptTool->GetTarget() : nullptr;
		TestNotNull(TEXT("Height Sculpt tool receives a target"), Target);
		if (Target)
		{
			TestTrue(TEXT("Target provides dynamic mesh input"),
				Target->GetClass()->ImplementsInterface(UDynamicMeshProvider::StaticClass()));
			TestTrue(TEXT("Target provides dynamic mesh commit"),
				Target->GetClass()->ImplementsInterface(UDynamicMeshCommitter::StaticClass()));
			TestTrue(TEXT("Target provides materials"),
				Target->GetClass()->ImplementsInterface(UMaterialProvider::StaticClass()));
			TestTrue(TEXT("Target is backed by a scene component"),
				Target->GetClass()->ImplementsInterface(USceneComponentBackedTarget::StaticClass()));
		}
	}

	TargetManager->Shutdown();
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPMeshTerrainNonWPSphereSculptRoundTripTest,
	"LootNPop.SurfaceNavigation.MeshTerrain.NonWPSphereSculptRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLNPMeshTerrainNonWPSphereSculptRoundTripTest::RunTest(const FString& Parameters)
{
	UStaticMesh* SourceMesh = LoadObject<UStaticMesh>(nullptr, SourceMeshObjectPath);
	TestNotNull(TEXT("Production octant source mesh is available for read-only duplication"), SourceMesh);
	if (!SourceMesh)
	{
		return false;
	}

	UStaticMesh* WritableMesh = LoadOrCreateSculptExperimentAsset(SourceMesh);
	TestNotNull(TEXT("Independent Sphere Sculpt experiment asset exists"), WritableMesh);
	if (!WritableMesh)
	{
		return false;
	}
	FAssetCompilingManager::Get().FinishAllCompilation();

	FStaticMeshSnapshot BeforeSculpt;
	TestTrue(TEXT("Experiment asset has readable LOD 0 mesh data"), CaptureStaticMeshSnapshot(WritableMesh, BeforeSculpt));
	if (BeforeSculpt.VertexPositions.IsEmpty())
	{
		return false;
	}

	TStrongObjectPtr<UWorld> TestWorld(NewObject<UWorld>(GetTransientPackage()));
	TestWorld->WorldType = EWorldType::EditorPreview;
	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(TestWorld->WorldType);
	WorldContext.SetCurrentWorld(TestWorld.Get());
	TestWorld->InitializeNewWorld(UWorld::InitializationValues()
		.AllowAudioPlayback(false)
		.CreatePhysicsScene(false)
		.RequiresHitProxies(false)
		.CreateNavigation(false)
		.CreateAISystem(false)
		.ShouldSimulatePhysics(false)
		.SetTransactional(false));
	TestNull(TEXT("Sphere Sculpt experiment world has no World Partition"), TestWorld->GetWorldPartition());

	UStaticMeshComponent* MeshComponent = NewObject<UStaticMeshComponent>(TestWorld.Get(), NAME_None, RF_Transient);
	MeshComponent->SetStaticMesh(WritableMesh);
	MeshComponent->ComponentTags = {
		TEXT("LNP.Terrain.Role.Support"),
		TEXT("LNP.Terrain.Lifecycle.Static")
	};
	const TArray<FName> ComponentTagsBeforeSculpt = MeshComponent->ComponentTags;

	FMeshTerrainTestQueries Queries;
	FMeshTerrainTestTransactions Transactions;
	Queries.World = TestWorld.Get();
	Queries.SelectedComponent = MeshComponent;

	UInteractiveToolsContext* ToolsContext = NewObject<UInteractiveToolsContext>();
	Queries.ToolsContext = ToolsContext;
	ToolsContext->Initialize(&Queries, &Transactions);
	ToolsContext->TargetManager->AddTargetFactory(
		NewObject<UStaticMeshComponentToolTargetFactory>(ToolsContext->TargetManager));

	UE::MeshPartition::UHeightSculptToolBuilder* Builder =
		NewObject<UE::MeshPartition::UHeightSculptToolBuilder>(ToolsContext->ToolManager);
	Builder->DefaultPrimaryBrushID = static_cast<int32>(UE::MeshPartition::UHeightSculptTool::EBrushType::HeightSculpt);
	const FString ToolIdentifier = TEXT("LNPNonWPSphereSculpt");
	ToolsContext->ToolManager->RegisterToolType(ToolIdentifier, Builder);
	TestTrue(TEXT("Sphere Height Sculpt tool type can be selected"),
		ToolsContext->ToolManager->SelectActiveToolType(EToolSide::Left, ToolIdentifier));
	TestTrue(TEXT("Sphere Height Sculpt tool activates in a non-World Partition world"),
		ToolsContext->ToolManager->ActivateTool(EToolSide::Left));

	UE::MeshPartition::UHeightSculptTool* HeightSculptTool =
		Cast<UE::MeshPartition::UHeightSculptTool>(ToolsContext->ToolManager->GetActiveTool(EToolSide::Left));
	TestNotNull(TEXT("Active tool is the Mesh Partition Height Sculpt tool"), HeightSculptTool);
	if (!HeightSculptTool)
	{
		ToolsContext->Shutdown();
		GEngine->DestroyWorldContext(TestWorld.Get());
		TestWorld->DestroyWorld(true);
		return false;
	}

	UE::MeshPartition::UHeightSculptToolProperties* HeightProperties = nullptr;
	for (UObject* PropertyObject : HeightSculptTool->GetToolProperties(false))
	{
		if (UE::MeshPartition::UHeightSculptToolProperties* Candidate =
			Cast<UE::MeshPartition::UHeightSculptToolProperties>(PropertyObject))
		{
			HeightProperties = Candidate;
			break;
		}
	}
	TestNotNull(TEXT("Height Sculpt exposes its reference surface properties"), HeightProperties);
	if (HeightProperties)
	{
		HeightProperties->ReferenceSurface = UE::MeshPartition::EHeightSculptReferenceSurface::Sphere;
		HeightProperties->bRequireConnectivity = false;
	}
	HeightSculptTool->SetActiveBrushType(
		static_cast<int32>(UE::MeshPartition::UHeightSculptTool::EBrushType::HeightSculpt));
	HeightSculptTool->SetBrushStrengthPressureEnabled(false);
	for (UObject* PropertyObject : HeightSculptTool->GetToolProperties(false))
	{
		if (UMeshSculptBrushOpProps* BrushOpProperties = Cast<UMeshSculptBrushOpProps>(PropertyObject))
		{
			BrushOpProperties->SetStrength(1.0f);
		}
	}
	HeightSculptTool->BrushProperties->BrushSize.SizeType = EBrushToolSizeType::World;
	HeightSculptTool->BrushProperties->BrushSize.WorldRadius = 20000.0f;
	HeightSculptTool->BrushProperties->BrushSize.bEnablePressureSensitivity = false;
	HeightSculptTool->BrushProperties->FlowRate = 1.0f;
	HeightSculptTool->GizmoProperties->bShowGizmo = false;

	const FVector BoundsCenter = WritableMesh->GetBoundingBox().GetCenter();
	const FVector RadialDirection = BoundsCenter.GetSafeNormal();
	FRay StrokeRay(FVector::ZeroVector, RadialDirection, true);
	FHitResult StrokeHit;
	if (!HeightSculptTool->HitTest(StrokeRay, StrokeHit))
	{
		const double OutsideRadius =
			WritableMesh->GetBoundingBox().GetExtent().Size() + BoundsCenter.Length() + 10000.0;
		StrokeRay = FRay(RadialDirection * OutsideRadius, -RadialDirection, true);
	}
	TestTrue(TEXT("Sphere Sculpt stroke ray hits the experiment mesh"), HeightSculptTool->HitTest(StrokeRay, StrokeHit));

	HeightSculptTool->OnUpdateHover(FInputDeviceRay(StrokeRay));
	const TArray<FVector3d> ToolPositionsBeforeStroke = CaptureToolVertexPositions(HeightSculptTool);
	HeightSculptTool->OnBeginDrag(StrokeRay);
	ToolsContext->ToolManager->Tick(0.1f);
	ToolsContext->ToolManager->Tick(0.1f);
	HeightSculptTool->OnEndDrag(StrokeRay);
	const TArray<FVector3d> ToolPositionsAfterStroke = CaptureToolVertexPositions(HeightSculptTool);
	const double ToolMaximumDisplacement = GetMaximumVertexDisplacement(
		ToolPositionsBeforeStroke,
		ToolPositionsAfterStroke);
	AddInfo(FString::Printf(
		TEXT("Sphere Sculpt preview maximum vertex displacement: %.6f cm"),
		ToolMaximumDisplacement));
	TestTrue(TEXT("Sphere Height Sculpt changes the preview mesh"), ToolMaximumDisplacement > 0.01);
	ToolsContext->ToolManager->DeactivateTool(EToolSide::Left, EToolShutdownType::Accept);
	FAssetCompilingManager::Get().FinishAllCompilation();

	TestEqual(TEXT("Component Tags survive the tool edit and Accept"),
		MeshComponent->ComponentTags, ComponentTagsBeforeSculpt);

	FStaticMeshSnapshot AfterAccept;
	TestTrue(TEXT("Accepted asset has readable LOD 0 mesh data"), CaptureStaticMeshSnapshot(WritableMesh, AfterAccept));
	TestEqual(TEXT("Height Sculpt preserves the vertex count"), AfterAccept.VertexCount, BeforeSculpt.VertexCount);
	TestEqual(TEXT("Height Sculpt preserves the triangle count"), AfterAccept.TriangleCount, BeforeSculpt.TriangleCount);
	TestEqual(TEXT("Height Sculpt preserves the polygon count"), AfterAccept.PolygonCount, BeforeSculpt.PolygonCount);
	TestTrue(TEXT("Sphere Height Sculpt changes at least one vertex position"),
		GetMaximumVertexDisplacement(BeforeSculpt.VertexPositions, AfterAccept.VertexPositions) > 0.01);
	TestTrue(TEXT("Height Sculpt preserves material slots"), AfterAccept.Materials == BeforeSculpt.Materials);
	TestTrue(TEXT("Height Sculpt preserves Nanite settings"), AfterAccept.NaniteSettings == BeforeSculpt.NaniteSettings);
	TestEqual(TEXT("Height Sculpt preserves the collision trace flag"),
		AfterAccept.CollisionTraceFlag, BeforeSculpt.CollisionTraceFlag);
	TestEqual(TEXT("Height Sculpt preserves double-sided collision"),
		AfterAccept.bDoubleSidedGeometry, BeforeSculpt.bDoubleSidedGeometry);
	TestTrue(TEXT("Accepted mesh has physics triangle mesh data"), AfterAccept.bHasPhysicsTriMeshData);

	ToolsContext->Shutdown();
	GEngine->DestroyWorldContext(TestWorld.Get());
	TestWorld->DestroyWorld(true);
	TestWorld.Reset();

	UPackage* ExperimentPackage = WritableMesh->GetOutermost();
	ExperimentPackage->MarkPackageDirty();
	const FString PackageFilename = FPackageName::LongPackageNameToFilename(
		SculptedMeshPackageName,
		FPackageName::GetAssetPackageExtension());
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArgs.SaveFlags = SAVE_NoError;
	TestTrue(TEXT("Accepted Sphere Sculpt asset saves to an independent package"),
		UPackage::SavePackage(ExperimentPackage, WritableMesh, *PackageFilename, SaveArgs));

	FText ReloadError;
	TestTrue(TEXT("Saved Sphere Sculpt package reloads without interaction"),
		UPackageTools::ReloadPackages(
			{ExperimentPackage},
			ReloadError,
			EReloadPackagesInteractionMode::AssumePositive));
	if (!ReloadError.IsEmpty())
	{
		AddError(FString::Printf(TEXT("Sphere Sculpt package reload reported: %s"), *ReloadError.ToString()));
	}

	const FString SculptedMeshObjectPath = FString::Printf(
		TEXT("%s.%s"),
		SculptedMeshPackageName,
		SculptedMeshAssetName);
	UStaticMesh* ReloadedMesh = LoadObject<UStaticMesh>(nullptr, *SculptedMeshObjectPath);
	TestNotNull(TEXT("Saved Sphere Sculpt asset is loadable by object path"), ReloadedMesh);
	FStaticMeshSnapshot AfterReload;
	TestTrue(TEXT("Reloaded asset has readable LOD 0 mesh data"), CaptureStaticMeshSnapshot(ReloadedMesh, AfterReload));
	TestEqual(TEXT("Reload preserves the vertex count"), AfterReload.VertexCount, AfterAccept.VertexCount);
	TestEqual(TEXT("Reload preserves the triangle count"), AfterReload.TriangleCount, AfterAccept.TriangleCount);
	TestEqual(TEXT("Reload preserves the polygon count"), AfterReload.PolygonCount, AfterAccept.PolygonCount);
	TestTrue(TEXT("Reload preserves sculpted vertex positions"),
		GetMaximumVertexDisplacement(AfterAccept.VertexPositions, AfterReload.VertexPositions) <= 0.01);
	TestTrue(TEXT("Reload preserves material slots"), AfterReload.Materials == AfterAccept.Materials);
	TestTrue(TEXT("Reload preserves Nanite settings"), AfterReload.NaniteSettings == AfterAccept.NaniteSettings);
	TestEqual(TEXT("Reload preserves the collision trace flag"),
		AfterReload.CollisionTraceFlag, AfterAccept.CollisionTraceFlag);
	TestEqual(TEXT("Sculpted asset remains configured for complex-as-simple collision"),
		AfterReload.CollisionTraceFlag, CTF_UseComplexAsSimple);
	TestEqual(TEXT("Reload preserves double-sided collision"),
		AfterReload.bDoubleSidedGeometry, AfterAccept.bDoubleSidedGeometry);
	TestTrue(TEXT("Reloaded mesh has physics triangle mesh data"), AfterReload.bHasPhysicsTriMeshData);

	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPMeshTerrainCOptionEightSlotSeamTest,
	"LootNPop.SurfaceNavigation.MeshTerrain.COptionEightSlotSeamAndPCG",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLNPMeshTerrainCOptionEightSlotSeamTest::RunTest(const FString& Parameters)
{
	UStaticMesh* SourceMesh = LoadObject<UStaticMesh>(nullptr, SourceMeshObjectPath);
	const FString SculptedMeshObjectPath = FString::Printf(
		TEXT("%s.%s"),
		SculptedMeshPackageName,
		SculptedMeshAssetName);
	UStaticMesh* SculptedMesh = LoadObject<UStaticMesh>(nullptr, *SculptedMeshObjectPath);
	TestNotNull(TEXT("Production octant source mesh is available for seam comparison"), SourceMesh);
	TestNotNull(TEXT("Independent sculpted mesh is available for slot validation"), SculptedMesh);
	if (!SourceMesh || !SculptedMesh)
	{
		return false;
	}

	FStaticMeshSnapshot SourceSnapshot;
	FStaticMeshSnapshot SculptedSnapshot;
	TestTrue(TEXT("Source mesh snapshot is readable"), CaptureStaticMeshSnapshot(SourceMesh, SourceSnapshot));
	TestTrue(TEXT("Sculpted mesh snapshot is readable"), CaptureStaticMeshSnapshot(SculptedMesh, SculptedSnapshot));
	TestEqual(TEXT("Source and sculpted vertex arrays remain comparable"),
		SculptedSnapshot.VertexPositions.Num(), SourceSnapshot.VertexPositions.Num());
	if (SculptedSnapshot.VertexPositions.Num() != SourceSnapshot.VertexPositions.Num())
	{
		return false;
	}

	int32 SeamVertexCount = 0;
	double MaximumSeamDisplacement = 0.0;
	double MaximumInteriorDisplacement = 0.0;
	for (int32 Index = 0; Index < SourceSnapshot.VertexPositions.Num(); ++Index)
	{
		const double Displacement = FVector3f::Distance(
			SourceSnapshot.VertexPositions[Index],
			SculptedSnapshot.VertexPositions[Index]);
		if (IsOctantSeamPosition(SourceSnapshot.VertexPositions[Index]))
		{
			++SeamVertexCount;
			MaximumSeamDisplacement = FMath::Max(MaximumSeamDisplacement, Displacement);
		}
		else
		{
			MaximumInteriorDisplacement = FMath::Max(MaximumInteriorDisplacement, Displacement);
		}
	}
	AddInfo(FString::Printf(
		TEXT("C-option seam vertices: %d, maximum seam displacement: %.6f cm, maximum interior displacement: %.6f cm"),
		SeamVertexCount,
		MaximumSeamDisplacement,
		MaximumInteriorDisplacement));
	TestTrue(TEXT("Octant source exposes seam vertices on its local coordinate planes"), SeamVertexCount > 0);
	TestTrue(TEXT("Sphere Sculpt changes the octant interior"), MaximumInteriorDisplacement > 0.01);
	TestTrue(TEXT("Sphere Sculpt keeps the octant seam vertices fixed"), MaximumSeamDisplacement <= 0.01);

	const TArray<FRotator> SlotRotations = GetRuntimeOctantSlotRotations();
	TMap<FIntVector, uint8> SeamSlotMasks;
	for (int32 SlotIndex = 0; SlotIndex < SlotRotations.Num(); ++SlotIndex)
	{
		const FQuat SlotRotation = SlotRotations[SlotIndex].Quaternion();
		for (const FVector3f& Position : SculptedSnapshot.VertexPositions)
		{
			if (IsOctantSeamPosition(Position))
			{
				const FIntVector Key = QuantizePosition(
					SlotRotation.RotateVector(FVector(Position)));
				SeamSlotMasks.FindOrAdd(Key) |= static_cast<uint8>(1u << SlotIndex);
			}
		}
	}

	int32 UnmatchedSeamPositionCount = 0;
	for (const TPair<FIntVector, uint8>& Entry : SeamSlotMasks)
	{
		if (FPlatformMath::CountBits(Entry.Value) < 2)
		{
			++UnmatchedSeamPositionCount;
		}
	}
	AddInfo(FString::Printf(
		TEXT("Eight-slot unique seam positions: %d, unmatched positions: %d"),
		SeamSlotMasks.Num(),
		UnmatchedSeamPositionCount));
	TestTrue(TEXT("Eight-slot assembly produces seam positions"), !SeamSlotMasks.IsEmpty());
	TestEqual(TEXT("Every transformed seam position is shared by another runtime slot"),
		UnmatchedSeamPositionCount, 0);

	TStrongObjectPtr<UWorld> TestWorld(NewObject<UWorld>(GetTransientPackage()));
	TestWorld->WorldType = EWorldType::EditorPreview;
	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(TestWorld->WorldType);
	WorldContext.SetCurrentWorld(TestWorld.Get());
	TestWorld->InitializeNewWorld(UWorld::InitializationValues()
		.AllowAudioPlayback(false)
		.CreatePhysicsScene(true)
		.RequiresHitProxies(false)
		.CreateNavigation(false)
		.CreateAISystem(false)
		.ShouldSimulatePhysics(false)
		.SetTransactional(false));

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(LNPMeshTerrainPCGInput), false);
	int32 SuccessfulComponentTraces = 0;
	for (int32 SlotIndex = 0; SlotIndex < SlotRotations.Num(); ++SlotIndex)
	{
		UStaticMeshComponent* MeshComponent = NewObject<UStaticMeshComponent>(TestWorld.Get());
		MeshComponent->SetStaticMesh(SculptedMesh);
		MeshComponent->SetWorldRotation(SlotRotations[SlotIndex]);
		MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		MeshComponent->SetCollisionResponseToAllChannels(ECR_Block);
		MeshComponent->RegisterComponentWithWorld(TestWorld.Get());

		const FVector SampleDirection = SlotRotations[SlotIndex].RotateVector(
			FVector(1.0, 1.0, 1.0).GetSafeNormal());
		FHitResult Hit;
		if (MeshComponent->LineTraceComponent(
			Hit,
			FVector::ZeroVector,
			SampleDirection * 100000.0,
			QueryParams))
		{
			++SuccessfulComponentTraces;
		}
		MeshComponent->UnregisterComponent();
	}
	AddInfo(FString::Printf(
		TEXT("PCG-style simple component traces: %d/%d"),
		SuccessfulComponentTraces,
		SlotRotations.Num()));
	TestEqual(TEXT("PCG-style simple component trace hits every rotated octant"),
		SuccessfulComponentTraces, SlotRotations.Num());

	GEngine->DestroyWorldContext(TestWorld.Get());
	TestWorld->DestroyWorld(true);
	TestWorld.Reset();
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPMeshTerrainCOptionLevelInstancePCGTest,
	"LootNPop.SurfaceNavigation.MeshTerrain.COptionLevelInstancePCGGraph",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLNPMeshTerrainCOptionLevelInstancePCGTest::RunTest(const FString& Parameters)
{
	UClass* ProductionOctantClass = LoadObject<UClass>(nullptr, MeadowOctantBlueprintClassPath);
	UClass* COptionOctantClass = LoadObject<UClass>(nullptr, COptionOctantBlueprintClassPath);
	UWorld* LevelInstanceWorld = LoadObject<UWorld>(nullptr, MeadowOctantLevelWorldPath);
	const FString SculptedMeshObjectPath = FString::Printf(
		TEXT("%s.%s"),
		SculptedMeshPackageName,
		SculptedMeshAssetName);
	UStaticMesh* SculptedMesh = LoadObject<UStaticMesh>(nullptr, *SculptedMeshObjectPath);
	TestNotNull(TEXT("Production octant Blueprint class is loadable"), ProductionOctantClass);
	TestNotNull(TEXT("C-option octant Blueprint class is loadable"), COptionOctantClass);
	TestNotNull(TEXT("Production octant Level Instance world is loadable"), LevelInstanceWorld);
	TestNotNull(TEXT("C-option sculpted mesh is loadable"), SculptedMesh);
	if (!ProductionOctantClass || !COptionOctantClass || !LevelInstanceWorld || !SculptedMesh)
	{
		return false;
	}

	int32 MatchingLevelActorCount = 0;
	int32 SavedLevelISMComponentCount = 0;
	int32 SavedLevelISMInstanceCount = 0;
	const UPCGGraph* ProductionPCGGraph = nullptr;
	if (ULevel* PersistentLevel = LevelInstanceWorld->PersistentLevel)
	{
		for (AActor* Actor : PersistentLevel->Actors)
		{
			if (Actor && Actor->IsA(ProductionOctantClass))
			{
				++MatchingLevelActorCount;
				if (const UPCGComponent* SavedPCGComponent = Actor->FindComponentByClass<UPCGComponent>())
				{
					ProductionPCGGraph = SavedPCGComponent->GetGraph();
				}
				TInlineComponentArray<UInstancedStaticMeshComponent*> SavedISMComponents(Actor);
				for (const UInstancedStaticMeshComponent* ISMComponent : SavedISMComponents)
				{
					if (ISMComponent)
					{
						++SavedLevelISMComponentCount;
						SavedLevelISMInstanceCount += ISMComponent->GetInstanceCount();
					}
				}
			}
		}
	}
	TestEqual(TEXT("Production Level Instance contains exactly one octant Blueprint actor"),
		MatchingLevelActorCount, 1);
	AddInfo(FString::Printf(
		TEXT("Saved production LVI: PCG graph=%s, ISM components=%d, instances=%d"),
		ProductionPCGGraph ? *ProductionPCGGraph->GetPathName() : TEXT("None"),
		SavedLevelISMComponentCount,
		SavedLevelISMInstanceCount));
	TestNotNull(TEXT("Saved production LVI actor retains its PCG graph"), ProductionPCGGraph);
	TestTrue(TEXT("Saved production LVI retains generated ISM components"), SavedLevelISMComponentCount > 0);
	TestTrue(TEXT("Saved production LVI retains generated prop instances"), SavedLevelISMInstanceCount > 0);

	TStrongObjectPtr<UWorld> TestWorld(NewObject<UWorld>(GetTransientPackage()));
	TestWorld->WorldType = EWorldType::EditorPreview;
	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(TestWorld->WorldType);
	WorldContext.SetCurrentWorld(TestWorld.Get());
	TestWorld->InitializeNewWorld(UWorld::InitializationValues()
		.AllowAudioPlayback(false)
		.CreatePhysicsScene(true)
		.RequiresHitProxies(false)
		.CreateNavigation(false)
		.CreateAISystem(false)
		.ShouldSimulatePhysics(false)
		.SetTransactional(false));

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* OctantActor = TestWorld->SpawnActor<AActor>(
		COptionOctantClass,
		FTransform::Identity,
		SpawnParameters);
	TestNotNull(TEXT("C-option octant Blueprint spawns in a transient non-WP world"), OctantActor);
	if (!OctantActor)
	{
		GEngine->DestroyWorldContext(TestWorld.Get());
		TestWorld->DestroyWorld(true);
		return false;
	}

	UStaticMeshComponent* CrustComponent = nullptr;
	TInlineComponentArray<UStaticMeshComponent*> StaticMeshComponents(OctantActor);
	for (UStaticMeshComponent* Component : StaticMeshComponents)
	{
		if (Component && Component->GetStaticMesh() == SculptedMesh)
		{
			CrustComponent = Component;
			break;
		}
	}
	UPCGComponent* PCGComponent = OctantActor->FindComponentByClass<UPCGComponent>();
	TestNotNull(TEXT("Spawned C-option octant uses the independent sculpted crust"), CrustComponent);
	TestNotNull(TEXT("Spawned C-option octant exposes its PCG component"), PCGComponent);
	TestNotNull(TEXT("C-option PCG component has an assigned graph"),
		PCGComponent ? PCGComponent->GetGraph() : nullptr);
	if (!CrustComponent || !PCGComponent || !PCGComponent->GetGraph())
	{
		TestWorld->DestroyActor(OctantActor);
		GEngine->DestroyWorldContext(TestWorld.Get());
		TestWorld->DestroyWorld(true);
		return false;
	}
	TestTrue(TEXT("Spawned C-option crust carries the Support role tag"),
		CrustComponent->ComponentHasTag(TEXT("LNP.Terrain.Role.Support")));
	TestTrue(TEXT("Spawned C-option crust carries the PCG surface role tag"),
		CrustComponent->ComponentHasTag(TEXT("LNP.Terrain.Role.PCGSurface")));
	TestFalse(TEXT("Spawned C-option crust does not affect legacy navigation"),
		CrustComponent->CanEverAffectNavigation());
	TestTrue(TEXT("C-option Blueprint reuses the production PCG graph"),
		PCGComponent->GetGraph() == ProductionPCGGraph);
	TestTrue(TEXT("C-option PCG component is registered on the transient actor"),
		PCGComponent->IsRegistered());
	const UPCGData* ActorPCGData = PCGComponent->GetActorPCGData();
	const UPCGData* InputPCGData = PCGComponent->GetInputPCGData();
	const UPCGSpatialData* ActorSpatialData = Cast<UPCGSpatialData>(ActorPCGData);
	AddInfo(FString::Printf(
		TEXT("C-option PCG input data: actor=%s, input=%s, spatial bounds valid=%s, bounds=%s"),
		ActorPCGData ? *ActorPCGData->GetClass()->GetName() : TEXT("None"),
		InputPCGData ? *InputPCGData->GetClass()->GetName() : TEXT("None"),
		ActorSpatialData && ActorSpatialData->GetBounds().IsValid ? TEXT("true") : TEXT("false"),
		ActorSpatialData ? *ActorSpatialData->GetBounds().ToString() : TEXT("None")));
	TestNotNull(TEXT("C-option PCG component creates actor spatial input from the sculpted crust"),
		ActorSpatialData);
	if (ActorSpatialData)
	{
		const FBox MeshBounds = SculptedMesh->GetBoundingBox();
		TestTrue(TEXT("C-option PCG spatial input matches the sculpted mesh bounds"),
			ActorSpatialData->GetBounds().Min.Equals(MeshBounds.Min, 0.1)
			&& ActorSpatialData->GetBounds().Max.Equals(MeshBounds.Max, 0.1));
	}
	TestWorld->DestroyActor(OctantActor);
	GEngine->DestroyWorldContext(TestWorld.Get());
	TestWorld->DestroyWorld(true);
	TestWorld.Reset();
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPMeshTerrainCOptionEightLevelInstanceAssetTest,
	"LootNPop.SurfaceNavigation.MeshTerrain.COptionEightLevelInstanceAsset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLNPMeshTerrainCOptionEightLevelInstanceAssetTest::RunTest(const FString& Parameters)
{
	UWorld* IntegrationWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!IntegrationWorld
		|| IntegrationWorld->GetPathName() != COptionEightSlotWorldPath)
	{
		const FString IntegrationMapFilename = FPackageName::LongPackageNameToFilename(
			FPackageName::ObjectPathToPackageName(FString(COptionEightSlotWorldPath)),
			FPackageName::GetMapPackageExtension());
		const bool bLoadedIntegrationMap = FEditorFileUtils::LoadMap(
			IntegrationMapFilename,
			false,
			false);
		TestTrue(TEXT("Eight-slot integration map opens with its external actors"), bLoadedIntegrationMap);
		IntegrationWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	}
	UWorld* COptionLevelWorld = LoadObject<UWorld>(nullptr, COptionOctantLevelWorldPath);
	UClass* ProductionOctantClass = LoadObject<UClass>(nullptr, MeadowOctantBlueprintClassPath);
	UClass* COptionOctantClass = LoadObject<UClass>(nullptr, COptionOctantBlueprintClassPath);
	const FString SculptedMeshObjectPath = FString::Printf(
		TEXT("%s.%s"),
		SculptedMeshPackageName,
		SculptedMeshAssetName);
	UStaticMesh* SculptedMesh = LoadObject<UStaticMesh>(nullptr, *SculptedMeshObjectPath);
	TestNotNull(TEXT("Eight-slot integration world is loadable"), IntegrationWorld);
	TestNotNull(TEXT("C-option Level Instance world is loadable"), COptionLevelWorld);
	TestNotNull(TEXT("Production octant Blueprint class is loadable"), ProductionOctantClass);
	TestNotNull(TEXT("C-option octant Blueprint class is loadable"), COptionOctantClass);
	TestNotNull(TEXT("C-option sculpted mesh is loadable"), SculptedMesh);
	if (!IntegrationWorld || !COptionLevelWorld || !ProductionOctantClass || !COptionOctantClass || !SculptedMesh)
	{
		return false;
	}

	const TArray<FRotator> ExpectedRotations = GetRuntimeOctantSlotRotations();
	TArray<bool> MatchedRotations;
	MatchedRotations.Init(false, ExpectedRotations.Num());
	int32 LevelInstanceCount = 0;
	int32 MatchingWorldAssetCount = 0;
	const FName ExpectedWorldAssetPackage(
		TEXT("/Game/SurfaceNavigationTests/MeshTerrain/LVI_Octant_COption"));
	auto RecordLevelInstance = [this, &ExpectedRotations, &MatchedRotations,
		&LevelInstanceCount, &MatchingWorldAssetCount, ExpectedWorldAssetPackage](
		const FTransform& ActorTransform,
		FName WorldAssetPackage)
	{
		++LevelInstanceCount;
		if (WorldAssetPackage == ExpectedWorldAssetPackage)
		{
			++MatchingWorldAssetCount;
		}
		TestTrue(TEXT("Eight-slot Level Instance remains at the world origin"),
			ActorTransform.GetLocation().IsNearlyZero(0.01));
		TestTrue(TEXT("Eight-slot Level Instance keeps unit scale"),
			ActorTransform.GetScale3D().Equals(FVector::OneVector, 0.001));

		for (int32 RotationIndex = 0; RotationIndex < ExpectedRotations.Num(); ++RotationIndex)
		{
			if (!MatchedRotations[RotationIndex]
				&& ActorTransform.GetRotation().AngularDistance(
					ExpectedRotations[RotationIndex].Quaternion()) <= 0.0001)
			{
				MatchedRotations[RotationIndex] = true;
				break;
			}
		}
	};
	if (UWorldPartition* WorldPartition = IntegrationWorld->GetWorldPartition())
	{
		FWorldPartitionHelpers::ForEachActorDescInstance<ALevelInstance>(
			WorldPartition,
			[&RecordLevelInstance](const FWorldPartitionActorDescInstance* ActorDescInstance)
			{
				const FLevelInstanceActorDesc* LevelInstanceActorDesc =
					static_cast<const FLevelInstanceActorDesc*>(ActorDescInstance->GetActorDesc());
				RecordLevelInstance(
					ActorDescInstance->GetActorTransform(),
					LevelInstanceActorDesc->GetChildContainerPackage());
				return true;
			});
	}
	else if (ULevel* PersistentLevel = IntegrationWorld->PersistentLevel)
	{
		for (AActor* Actor : PersistentLevel->Actors)
		{
			if (const ALevelInstance* LevelInstance = Cast<ALevelInstance>(Actor))
			{
				RecordLevelInstance(
					LevelInstance->GetActorTransform(),
					LevelInstance->GetWorldAsset().ToSoftObjectPath().GetLongPackageFName());
			}
		}
	}
	TestEqual(TEXT("Integration world stores exactly eight Level Instances"), LevelInstanceCount, 8);
	TestEqual(TEXT("Every integration slot references the C-option Level Instance"),
		MatchingWorldAssetCount, 8);
	for (int32 RotationIndex = 0; RotationIndex < MatchedRotations.Num(); ++RotationIndex)
	{
		TestTrue(
			*FString::Printf(TEXT("Runtime octant rotation %d is represented exactly once"), RotationIndex),
			MatchedRotations[RotationIndex]);
	}

	int32 COptionActorCount = 0;
	int32 ProductionActorCount = 0;
	int32 SavedHISMComponentCount = 0;
	int32 SavedHISMInstanceCount = 0;
	if (ULevel* PersistentLevel = COptionLevelWorld->PersistentLevel)
	{
		for (AActor* Actor : PersistentLevel->Actors)
		{
			if (!Actor)
			{
				continue;
			}
			if (Actor->IsA(ProductionOctantClass))
			{
				++ProductionActorCount;
			}
			if (!Actor->IsA(COptionOctantClass))
			{
				continue;
			}

			++COptionActorCount;
			UStaticMeshComponent* CrustComponent = nullptr;
			TInlineComponentArray<UStaticMeshComponent*> StaticMeshComponents(Actor);
			for (UStaticMeshComponent* Component : StaticMeshComponents)
			{
				if (Component && Component->GetStaticMesh() == SculptedMesh)
				{
					CrustComponent = Component;
					break;
				}
			}
			TestNotNull(TEXT("C-option Level Instance contains the sculpted crust"), CrustComponent);
			if (CrustComponent)
			{
				TestTrue(TEXT("C-option Level Instance crust carries the Support role tag"),
					CrustComponent->ComponentHasTag(TEXT("LNP.Terrain.Role.Support")));
				TestTrue(TEXT("C-option Level Instance crust carries the PCG surface role tag"),
					CrustComponent->ComponentHasTag(TEXT("LNP.Terrain.Role.PCGSurface")));
				TestFalse(TEXT("C-option Level Instance crust does not affect legacy navigation"),
					CrustComponent->CanEverAffectNavigation());
			}
			const UPCGComponent* PCGComponent = Actor->FindComponentByClass<UPCGComponent>();
			TestNotNull(TEXT("C-option Level Instance actor retains its PCG component"), PCGComponent);
			TestNotNull(TEXT("C-option Level Instance actor retains its PCG graph"),
				PCGComponent ? PCGComponent->GetGraph() : nullptr);

			TInlineComponentArray<UInstancedStaticMeshComponent*> ISMComponents(Actor);
			for (const UInstancedStaticMeshComponent* ISMComponent : ISMComponents)
			{
				if (ISMComponent)
				{
					++SavedHISMComponentCount;
					SavedHISMInstanceCount += ISMComponent->GetInstanceCount();
				}
			}
		}
	}
	AddInfo(FString::Printf(
		TEXT("Saved C-option LVI: actors=%d, HISM components=%d, instances=%d"),
		COptionActorCount,
		SavedHISMComponentCount,
		SavedHISMInstanceCount));
	TestEqual(TEXT("C-option Level Instance stores exactly one C-option actor"), COptionActorCount, 1);
	TestEqual(TEXT("C-option Level Instance no longer stores the production actor"), ProductionActorCount, 0);
	TestTrue(TEXT("C-option Level Instance stores generated HISM components"), SavedHISMComponentCount > 0);
	TestTrue(TEXT("C-option Level Instance stores generated prop instances"), SavedHISMInstanceCount > 0);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPMeshTerrainBOptionExtractionTest,
	"LootNPop.SurfaceNavigation.MeshTerrain.BOptionIndependentAssetExtraction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLNPMeshTerrainBOptionExtractionTest::RunTest(const FString& Parameters)
{
	const FString ExtractedObjectPath = FString::Printf(
		TEXT("%s.%s"),
		BOptionExtractedMeshPackageName,
		BOptionExtractedMeshAssetName);
	UStaticMesh* EmbeddedMesh = FindCurrentBOptionEmbeddedMesh();
	UStaticMesh* ExtractedMesh = nullptr;
	FStaticMeshSnapshot EmbeddedSnapshot;
	if (EmbeddedMesh)
	{
		TestTrue(TEXT("B-option embedded Mesh Partition result has readable LOD 0 data"),
			CaptureStaticMeshSnapshot(EmbeddedMesh, EmbeddedSnapshot));
		ExtractedMesh = ExtractBOptionMesh(EmbeddedMesh);
	}
	else
	{
		AddWarning(TEXT(
			"B-option authoring world is not current; validating the previously extracted independent asset."));
		ExtractedMesh = LoadObject<UStaticMesh>(nullptr, *ExtractedObjectPath);
	}

	TestNotNull(TEXT("B-option result is available as an independent Static Mesh asset"), ExtractedMesh);
	if (!ExtractedMesh)
	{
		return false;
	}

	TestEqual(TEXT("B-option output is owned by its independent package"),
		ExtractedMesh->GetOutermost()->GetName(),
		FString(BOptionExtractedMeshPackageName));
	TestTrue(TEXT("B-option output has public standalone asset flags"),
		ExtractedMesh->HasAllFlags(RF_Public | RF_Standalone));

	FStaticMeshSnapshot ExtractedSnapshot;
	const bool bCapturedExtractedMesh = CaptureStaticMeshSnapshot(ExtractedMesh, ExtractedSnapshot);
	TestTrue(TEXT("B-option independent asset has readable LOD 0 data"), bCapturedExtractedMesh);
	TestTrue(TEXT("B-option independent asset contains vertices"), ExtractedSnapshot.VertexCount > 0);
	TestTrue(TEXT("B-option independent asset contains triangles"), ExtractedSnapshot.TriangleCount > 0);
	if (!bCapturedExtractedMesh
		|| ExtractedSnapshot.VertexCount <= 0
		|| ExtractedSnapshot.TriangleCount <= 0)
	{
		return false;
	}
	TestTrue(TEXT("B-option independent asset preserves material slots"),
		!ExtractedSnapshot.Materials.IsEmpty());
	TestTrue(TEXT("B-option independent asset enables Nanite"),
		ExtractedSnapshot.NaniteSettings.bEnabled);
	TestEqual(TEXT("B-option independent asset uses exact complex collision"),
		ExtractedSnapshot.CollisionTraceFlag,
		CTF_UseComplexAsSimple);
	TestTrue(TEXT("B-option independent asset has double-sided collision"),
		ExtractedSnapshot.bDoubleSidedGeometry);
	TestTrue(TEXT("B-option independent asset has physics triangle mesh data"),
		ExtractedSnapshot.bHasPhysicsTriMeshData);

	if (EmbeddedMesh)
	{
		TestEqual(TEXT("Extraction preserves the Mesh Partition vertex count"),
			ExtractedSnapshot.VertexCount,
			EmbeddedSnapshot.VertexCount);
		TestEqual(TEXT("Extraction preserves the Mesh Partition triangle count"),
			ExtractedSnapshot.TriangleCount,
			EmbeddedSnapshot.TriangleCount);
		TestEqual(TEXT("Extraction preserves the Mesh Partition polygon count"),
			ExtractedSnapshot.PolygonCount,
			EmbeddedSnapshot.PolygonCount);
		TestTrue(TEXT("Extraction preserves Mesh Partition vertex positions"),
			GetMaximumVertexDisplacement(
				EmbeddedSnapshot.VertexPositions,
				ExtractedSnapshot.VertexPositions) <= 0.01);
		TestTrue(TEXT("Extraction preserves Mesh Partition material slots"),
			ExtractedSnapshot.Materials == EmbeddedSnapshot.Materials);
	}

	UPackage* ExtractedPackage = ExtractedMesh->GetOutermost();
	ExtractedPackage->MarkPackageDirty();
	const FString PackageFilename = FPackageName::LongPackageNameToFilename(
		BOptionExtractedMeshPackageName,
		FPackageName::GetAssetPackageExtension());
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArgs.SaveFlags = SAVE_NoError;
	TestTrue(TEXT("B-option independent asset saves to its own package"),
		UPackage::SavePackage(ExtractedPackage, ExtractedMesh, *PackageFilename, SaveArgs));

	FText ReloadError;
	TestTrue(TEXT("B-option independent package reloads without interaction"),
		UPackageTools::ReloadPackages(
			{ExtractedPackage},
			ReloadError,
			EReloadPackagesInteractionMode::AssumePositive));
	if (!ReloadError.IsEmpty())
	{
		AddError(FString::Printf(TEXT("B-option package reload reported: %s"), *ReloadError.ToString()));
	}

	UStaticMesh* ReloadedMesh = LoadObject<UStaticMesh>(nullptr, *ExtractedObjectPath);
	TestNotNull(TEXT("Reloaded B-option independent asset is loadable by object path"), ReloadedMesh);
	FStaticMeshSnapshot ReloadedSnapshot;
	TestTrue(TEXT("Reloaded B-option asset has readable LOD 0 data"),
		CaptureStaticMeshSnapshot(ReloadedMesh, ReloadedSnapshot));
	TestEqual(TEXT("B-option reload preserves the vertex count"),
		ReloadedSnapshot.VertexCount,
		ExtractedSnapshot.VertexCount);
	TestEqual(TEXT("B-option reload preserves the triangle count"),
		ReloadedSnapshot.TriangleCount,
		ExtractedSnapshot.TriangleCount);
	TestTrue(TEXT("B-option reload preserves vertex positions"),
		GetMaximumVertexDisplacement(
			ExtractedSnapshot.VertexPositions,
			ReloadedSnapshot.VertexPositions) <= 0.01);
	TestTrue(TEXT("B-option reload preserves Nanite"),
		ReloadedSnapshot.NaniteSettings.bEnabled);
	TestEqual(TEXT("B-option reload preserves exact complex collision"),
		ReloadedSnapshot.CollisionTraceFlag,
		CTF_UseComplexAsSimple);

	AddInfo(FString::Printf(
		TEXT("B-option extracted mesh: vertices=%d, triangles=%d, polygons=%d, materials=%d"),
		ReloadedSnapshot.VertexCount,
		ReloadedSnapshot.TriangleCount,
		ReloadedSnapshot.PolygonCount,
		ReloadedSnapshot.Materials.Num()));
	return !HasAnyErrors();
}

#endif
