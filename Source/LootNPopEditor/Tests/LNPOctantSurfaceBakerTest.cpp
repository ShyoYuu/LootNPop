// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/StaticMeshComponent.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "DataAsset/LNPOctantSurfaceData.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameLogic/LNPOctantSpawnSubsystem.h"
#include "Misc/PackageName.h"
#include "Misc/ScopeExit.h"
#include "SurfaceNavigation/LNPCollisionChannels.h"
#include "SurfaceNavigation/LNPCrustAtlas.h"
#include "SurfaceNavigation/LNPMassSpawnPoint.h"
#include "SurfaceNavigation/LNPOctantSurfaceBaker.h"
#include "SurfaceNavigation/LNPOctantTriangleExtractor.h"
#include "SurfaceNavigation/LNPRegressionFixture.h"
#include "SurfaceNavigation/LNPSpawnData.h"
#include "UObject/StrongObjectPtr.h"

namespace LNPOctantSurfaceBakerTest
{
	constexpr TCHAR FixtureLevelPath[] =
		TEXT("/Game/Maps/SurfaceNavigation/Fixtures/LVI_Octant_Fixture_Crust.LVI_Octant_Fixture_Crust");
	constexpr TCHAR MeadowLevelPath[] = TEXT("/Game/Maps/Meadow_00/LVI_Octant_Meadow_00.LVI_Octant_Meadow_00");

	FIntPoint DirectionToNavCoord(const FVector3d& Position, const int32 Subdivisions)
	{
		const FVector3d Direction = Position.GetSafeNormal();
		const double Sum = Direction.X + Direction.Y + Direction.Z;
		int32 I = FMath::Clamp(FMath::RoundToInt32(Direction.X / Sum * Subdivisions), 0, Subdivisions);
		int32 J = FMath::Clamp(FMath::RoundToInt32(Direction.Y / Sum * Subdivisions), 0, Subdivisions);
		while (I + J > Subdivisions)
		{
			I >= J ? --I : --J;
		}
		return FIntPoint(I, J);
	}

	/** fixture 입구 구멍(`LNPSurfaceFixtureBuilder.cpp`): 위도 30°·방위 45°, 각반지름 1.5°. */
	constexpr double FixtureHoleLatDeg = 30.0;
	constexpr double FixtureHoleAzDeg = 45.0;
	constexpr double FixtureHoleAngleDeg = 1.5;

	/**
	 * 기본 해상도(지각 100cm, 비지각 Layer 25cm)의 Atlas 보간 대 exact 합격 기준. 2026-09-27 `Meadow_00` 지각 실측(P99 1.06cm,
	 * 최대 5.0cm, 법선 P99 2.2°)에 약 2배 여유를 둔 값이다(`phases/Phase04a_CrustAtlasAndSeams.md` §3.6). 비지각 Layer는 실측
	 * (P99 0.12cm, 최대 0.34cm, 법선 P99 0.2°)이 훨씬 작지만 같은 값을 쓴다.
	 */
	constexpr double MaxRadiusErrorP99 = 2.0;
	constexpr double MaxRadiusError = 10.0;
	constexpr double MaxNormalErrorP99Deg = 5.0;

	/** 이음매 일치 기준(`phases/Phase04a_CrustAtlasAndSeams.md` §3.7). */
	constexpr double SeamDirectionTolerance = 1e-6;
	constexpr double SeamRadiusTolerance = 1.0;

	constexpr int32 RandomDirectionCount = 20000;
	constexpr int32 RandomSeed = 20260927;

	FVector3d DirectionFromLatAz(double LatDeg, double AzDeg)
	{
		const double Lat = FMath::DegreesToRadians(LatDeg);
		const double Az = FMath::DegreesToRadians(AzDeg);
		return FVector3d(FMath::Cos(Lat) * FMath::Cos(Az), FMath::Cos(Lat) * FMath::Sin(Az), FMath::Sin(Lat));
	}

	double Percentile(TArray<double>& Values, double Fraction)
	{
		if (Values.IsEmpty())
		{
			return 0.0;
		}
		Values.Sort();
		return Values[FMath::Clamp(FMath::FloorToInt32(Fraction * (Values.Num() - 1)), 0, Values.Num() - 1)];
	}

	double AngleDeg(const FVector3d& A, const FVector3d& B)
	{
		return FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector3d::DotProduct(A, B), -1.0, 1.0)));
	}

	/** 로드만 한 source World에는 physics scene이 없으므로 exact 기준값은 새 physics world에 복제한 component로 잰다. */
	struct FExactPhysicsWorld
	{
		TStrongObjectPtr<UWorld> World;

		FExactPhysicsWorld()
		{
			World.Reset(NewObject<UWorld>(GetTransientPackage()));
			World->WorldType = EWorldType::EditorPreview;
			FWorldContext& WorldContext = GEngine->CreateNewWorldContext(World->WorldType);
			WorldContext.SetCurrentWorld(World.Get());
			World->InitializeNewWorld(UWorld::InitializationValues()
				.AllowAudioPlayback(false)
				.CreatePhysicsScene(true)
				.RequiresHitProxies(false)
				.CreateNavigation(false)
				.CreateAISystem(false)
				.ShouldSimulatePhysics(false)
				.SetTransactional(false));
		}

		~FExactPhysicsWorld()
		{
			GEngine->DestroyWorldContext(World.Get());
			World->DestroyWorld(true);
		}

		UStaticMeshComponent* AddProbe(const UStaticMeshComponent& Source) const
		{
			UStaticMeshComponent* Probe = NewObject<UStaticMeshComponent>(World.Get());
			Probe->SetStaticMesh(Source.GetStaticMesh());
			Probe->SetWorldTransform(FLNPOctantTriangleExtractor::GetSourceTransform(Source));
			Probe->SetCollisionProfileName(Source.GetCollisionProfileName());
			Probe->RegisterComponentWithWorld(World.Get());
			return Probe;
		}
	};

	/** 지각 component 하나만 등록한 physics world. exact 기준값은 `LNPSurfaceSupport` 채널 trace다. */
	struct FCrustExactWorld : FExactPhysicsWorld
	{
		explicit FCrustExactWorld(const UStaticMeshComponent& Crust)
		{
			AddProbe(Crust);
		}

		/** 구 중심에서 바깥쪽 방향의 첫 지각 hit. 지각 앞면은 중심 쪽이다. */
		bool Trace(const FVector3d& Direction, double MaxRadius, FHitResult& OutHit) const
		{
			const FCollisionQueryParams Params(SCENE_QUERY_STAT(LNPCrustExactError), /*bTraceComplex=*/false);
			return World->LineTraceSingleByChannel(
				OutHit, FVector::ZeroVector, Direction * MaxRadius, LNPCollisionChannels::SurfaceSupport, Params);
		}
	};

	struct FErrorStats
	{
		int32 Supported = 0;
		int32 NeedsExact = 0;
		/** Atlas는 지면을 냈는데 exact는 지면이 없는 방향. 0이어야 한다. */
		int32 GhostFloor = 0;
		TArray<double> RadiusErrors;
		TArray<double> NormalErrorsDeg;

		void Add(bool bSupported, const FLNPSupportLayerQuery& Atlas, bool bExactHit, const FHitResult& Exact, const FVector3d& Direction)
		{
			if (!bSupported)
			{
				++NeedsExact;
				return;
			}
			++Supported;
			if (!bExactHit)
			{
				++GhostFloor;
				return;
			}
			RadiusErrors.Add(FMath::Abs(Atlas.Radius - FVector3d::DotProduct(FVector3d(Exact.ImpactPoint), Direction)));
			NormalErrorsDeg.Add(AngleDeg(FVector3d(Atlas.Normal), FVector3d(Exact.ImpactNormal)));
		}

		FString ToString()
		{
			const int32 Total = Supported + NeedsExact;
			return FString::Printf(
				TEXT("queries=%d NeedsExact=%.2f%% ghost=%d radiusErr P50=%.3f P99=%.3f max=%.3fcm normalErr P50=%.3f P99=%.3f max=%.3fdeg"),
				Total, Total > 0 ? 100.0 * NeedsExact / Total : 0.0, GhostFloor,
				Percentile(RadiusErrors, 0.5), Percentile(RadiusErrors, 0.99), Percentile(RadiusErrors, 1.0),
				Percentile(NormalErrorsDeg, 0.5), Percentile(NormalErrorsDeg, 0.99), Percentile(NormalErrorsDeg, 1.0));
		}
	};

	constexpr int32 LayerDirectionCount = 4000;
	/** Layer footprint 밖 유령 지면도 보도록 무작위 방향 영역을 footprint 행·열 범위보다 이만큼(격자 칸) 넓힌다. */
	constexpr double LayerFootprintMargin = 2.0;
	/** 한 광선에서 다른 Layer·non-walkable face를 건너뛰는 최대 횟수. */
	constexpr int32 MaxLayerTraceSkips = 8;

	/**
	 * source component 하나만 대상으로 구 중심에서 바깥쪽 광선을 쏴 Layer의 첫 앞면 hit를 찾는다. 앞선 hit가 같은 source의
	 * 다른 Layer나 non-walkable face면 그 뒤에서 다시 쏜다. Atlas가 Layer 삼각형만으로 샘플링하는 것과 같은 기준이다.
	 * hit FaceIndex를 face 표로 해석하지 못하면 OutUnresolved를 올리고 false다.
	 */
	bool TraceLayer(UStaticMeshComponent& Probe, const FLNPSupportFaceMap& FaceMap, uint16 Layer,
		const FVector3d& Direction, double MaxRadius, FHitResult& OutHit, int32& OutUnresolved)
	{
		FCollisionQueryParams Params(SCENE_QUERY_STAT(LNPLayerExactError), /*bTraceComplex=*/false);
		Params.bReturnFaceIndex = true;
		double StartRadius = 0.0;
		for (int32 Skip = 0; Skip <= MaxLayerTraceSkips; ++Skip)
		{
			if (!Probe.LineTraceComponent(OutHit, Direction * StartRadius, Direction * MaxRadius, Params))
			{
				return false;
			}
			if (OutHit.FaceIndex < 0)
			{
				++OutUnresolved;
				return false;
			}
			if (FaceMap.Resolve(OutHit.FaceIndex) == Layer)
			{
				return true;
			}
			StartRadius = FVector3d::DotProduct(FVector3d(OutHit.ImpactPoint), Direction) + 0.01;
		}
		++OutUnresolved;
		return false;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPOctantBakeDeterministicTest,
	"LootNPop.SurfaceNavigation.Bake.OctantBakeDeterministic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLNPOctantBakeDeterministicTest::RunTest(const FString& Parameters)
{
	using namespace LNPOctantSurfaceBakerTest;
	for (const TCHAR* LevelPath : {FixtureLevelPath, LNPRegressionFixture::LevelPath, MeadowLevelPath})
	{
		TStrongObjectPtr<ULNPOctantSurfaceData> First(NewObject<ULNPOctantSurfaceData>(GetTransientPackage()));
		TStrongObjectPtr<ULNPOctantSurfaceData> Second(NewObject<ULNPOctantSurfaceData>(GetTransientPackage()));
		FLNPOctantBakeReport FirstReport;
		FLNPOctantBakeReport SecondReport;
		FString Error;
		if (!FLNPOctantSurfaceBaker::Bake(FSoftObjectPath(LevelPath), FLNPOctantBakeOptions(), *First, FirstReport, Error)
			|| !FLNPOctantSurfaceBaker::Bake(FSoftObjectPath(LevelPath), FLNPOctantBakeOptions(), *Second, SecondReport, Error))
		{
			AddError(FString::Printf(TEXT("%s: %s"), LevelPath, *Error));
			continue;
		}
		AddInfo(FString::Printf(TEXT("%s: %s | payload hash %s"),
			LevelPath, *FirstReport.ToString(), *First->Header.Support.ContentHash.ToString()));

		TestEqual(FString::Printf(TEXT("%s: DataVersion"), LevelPath),
			First->Header.DataVersion, FLNPSurfaceBakeHeader::CurrentDataVersion);
		TestTrue(FString::Printf(TEXT("%s: payload is identical across bakes"), LevelPath),
			First->SupportPayload == Second->SupportPayload);
		TestTrue(FString::Printf(TEXT("%s: payload hash is identical across bakes"), LevelPath),
			First->Header.Support.ContentHash == Second->Header.Support.ContentHash);
		TestTrue(FString::Printf(TEXT("%s: Navigation payload is identical across bakes"), LevelPath),
			First->NavigationPayload == Second->NavigationPayload);
		TestTrue(FString::Printf(TEXT("%s: Navigation payload hash is identical across bakes"), LevelPath),
			First->Header.Navigation.ContentHash == Second->Header.Navigation.ContentHash);
		TestTrue(FString::Printf(TEXT("%s: Traversal payload is identical across bakes"), LevelPath),
			First->TraversalPayload == Second->TraversalPayload);
		TestTrue(FString::Printf(TEXT("%s: Traversal payload hash is identical across bakes"), LevelPath),
			First->Header.Traversal.ContentHash == Second->Header.Traversal.ContentHash);
		TestTrue(FString::Printf(TEXT("%s: Spawn payload is identical across bakes"), LevelPath),
			First->SpawnPayload == Second->SpawnPayload);
		TestTrue(FString::Printf(TEXT("%s: Spawn payload hash is identical across bakes"), LevelPath),
			First->Header.Spawn.ContentHash == Second->Header.Spawn.ContentHash);
		TestTrue(FString::Printf(TEXT("%s: source hash is identical across bakes"), LevelPath),
			First->Header.SourceContentHash == Second->Header.SourceContentHash);
		TestEqual(FString::Printf(TEXT("%s: descriptor size"), LevelPath),
			First->Header.Support.UncompressedSize, static_cast<uint64>(First->SupportPayload.Num()));
		TestEqual(FString::Printf(TEXT("%s: descriptor element count"), LevelPath),
			First->Header.Support.ElementCount, static_cast<uint32>(FirstReport.TotalSampleCount));
		TestEqual(FString::Printf(TEXT("%s: Navigation descriptor size"), LevelPath),
			First->Header.Navigation.UncompressedSize, static_cast<uint64>(First->NavigationPayload.Num()));
		TestEqual(FString::Printf(TEXT("%s: Navigation descriptor element count"), LevelPath),
			First->Header.Navigation.ElementCount, static_cast<uint32>(FirstReport.Nav.CellCount));
		TestEqual(FString::Printf(TEXT("%s: Traversal descriptor size"), LevelPath),
			First->Header.Traversal.UncompressedSize, static_cast<uint64>(First->TraversalPayload.Num()));
		TestEqual(FString::Printf(TEXT("%s: Spawn descriptor size"), LevelPath),
			First->Header.Spawn.UncompressedSize, static_cast<uint64>(First->SpawnPayload.Num()));
		TestEqual(FString::Printf(TEXT("%s: Spawn descriptor element count"), LevelPath),
			First->Header.Spawn.ElementCount,
			static_cast<uint32>(FirstReport.SpawnAuthoredCount + FirstReport.SpawnCandidateCount));

		// 저장된 에셋이 현재 source로 구운 결과와 같아야 한다. 다르면 `LNP.SurfaceNav.BakeOctant`로 다시 굽는다.
		const FString SavedPackage = FLNPOctantSurfaceBaker::GetSurfaceDataPackageName(FSoftObjectPath(LevelPath));
		const ULNPOctantSurfaceData* Saved = LoadObject<ULNPOctantSurfaceData>(
			nullptr, *FString::Printf(TEXT("%s.%s"), *SavedPackage, *FPackageName::GetShortName(SavedPackage)));
		if (TestNotNull(FString::Printf(TEXT("%s: saved SurfaceData exists"), *SavedPackage), Saved))
		{
			TestEqual(FString::Printf(TEXT("%s: saved DataVersion"), *SavedPackage),
				Saved->Header.DataVersion, FLNPSurfaceBakeHeader::CurrentDataVersion);
			TestTrue(FString::Printf(TEXT("%s: saved source hash is current"), *SavedPackage),
				Saved->Header.SourceContentHash == First->Header.SourceContentHash);
			TestTrue(FString::Printf(TEXT("%s: saved Support payload is current"), *SavedPackage),
				Saved->Header.Support.ContentHash == First->Header.Support.ContentHash
				&& Saved->SupportPayload == First->SupportPayload);
			TestTrue(FString::Printf(TEXT("%s: saved Navigation payload is current"), *SavedPackage),
				Saved->Header.Navigation.ContentHash == First->Header.Navigation.ContentHash
				&& Saved->NavigationPayload == First->NavigationPayload);
			TestTrue(FString::Printf(TEXT("%s: saved Traversal payload is current"), *SavedPackage),
				Saved->Header.Traversal.ContentHash == First->Header.Traversal.ContentHash
				&& Saved->TraversalPayload == First->TraversalPayload);
			TestTrue(FString::Printf(TEXT("%s: saved Spawn payload is current"), *SavedPackage),
				Saved->Header.Spawn.ContentHash == First->Header.Spawn.ContentHash
				&& Saved->SpawnPayload == First->SpawnPayload);
		}

		FLNPSupportAtlas Atlas;
		if (!TestTrue(FString::Printf(TEXT("%s: payload decodes"), LevelPath),
			LNPSupportAtlas::Decode(First->SupportPayload, Atlas, Error)))
		{
			AddError(Error);
			continue;
		}
		TestEqual(FString::Printf(TEXT("%s: decoded Layer count"), LevelPath), Atlas.Layers.Num(), FirstReport.SupportLayerCount);
		TestEqual(FString::Printf(TEXT("%s: decoded source count"), LevelPath), Atlas.Sources.Num(), FirstReport.SupportSourceCount);
		FLNPNavData Navigation;
		if (TestTrue(FString::Printf(TEXT("%s: Navigation payload decodes"), LevelPath),
			LNPNavData::DecodeNavigation(First->NavigationPayload, Navigation, Error)))
		{
			TestEqual(FString::Printf(TEXT("%s: decoded Nav Layer count"), LevelPath),
				Navigation.Layers.Num(), FirstReport.Nav.LayerCount);
			FLNPNavTraversalData Traversal;
			if (TestTrue(FString::Printf(TEXT("%s: Traversal payload decodes"), LevelPath),
				LNPNavData::DecodeTraversal(First->TraversalPayload, Navigation, Traversal, Error)))
			{
				TestEqual(FString::Printf(TEXT("%s: decoded portal count"), LevelPath),
					Traversal.Portals.Num(), FirstReport.Nav.PortalCount);
				TestEqual(FString::Printf(TEXT("%s: decoded seam count"), LevelPath),
					Traversal.SeamEndpoints.Num(), FirstReport.Nav.SeamEndpointCount);
				if (FStringView(LevelPath) == FStringView(LNPRegressionFixture::LevelPath))
				{
					TestEqual(TEXT("Regression fixture keeps seven Layer-local components"),
						Navigation.LocalStaticComponentCount, static_cast<uint16>(7));
					TestEqual(TEXT("Regression fixture connects room-corridor and corridor-crust"), Traversal.Portals.Num(), 2);
					TSet<uint32> PortalLayerPairs;
					for (const FLNPNavPortal& Portal : Traversal.Portals)
					{
						PortalLayerPairs.Add(
							(static_cast<uint32>(Portal.A.LocalNavLayerId) << 16) | Portal.B.LocalNavLayerId);
						FIntPoint ACoord;
						FIntPoint BCoord;
						const FLNPNavCell* ACell = LNPNavData::ResolveLocalNode(Navigation, Portal.A, &ACoord);
						const FLNPNavCell* BCell = LNPNavData::ResolveLocalNode(Navigation, Portal.B, &BCoord);
						AddInfo(FString::Printf(TEXT("Regression portal Layer %u (%d,%d) Component %u -> Layer %u (%d,%d) Component %u"),
							Portal.A.LocalNavLayerId, ACoord.X, ACoord.Y,
							ACell ? ACell->LocalStaticComponentId : MAX_uint16,
							Portal.B.LocalNavLayerId, BCoord.X, BCoord.Y,
							BCell ? BCell->LocalStaticComponentId : MAX_uint16));
					}
					TestTrue(TEXT("Regression portal joins the cave room and corridor Layers"),
						PortalLayerPairs.Contains((2u << 16) | 3u));
					TestTrue(TEXT("Regression portal joins the corridor to the crust entrance"),
						PortalLayerPairs.Contains((0u << 16) | 3u));

					const FLNPNavLayer& CrustNav = Navigation.Layers[0];
					const LNPRegressionFixture::FCaseFrame Props = LNPRegressionFixture::StaticProps();
					for (const TPair<FString, FVector>& Blocker : {
						TPair<FString, FVector>(TEXT("Tree"), Props.At(LNPRegressionFixture::CrustRadius, LNPRegressionFixture::TreeTangent)),
						TPair<FString, FVector>(TEXT("Rock"), Props.At(LNPRegressionFixture::CrustRadius, LNPRegressionFixture::RockTangent))})
					{
						const FIntPoint Coord = DirectionToNavCoord(Blocker.Value, CrustNav.Subdivisions);
						FLNPLocalNavNodeRef Node;
						TestFalse(FString::Printf(TEXT("Regression %s is dilated out of the crust Grid"), *Blocker.Key),
							LNPNavData::MakeLocalNodeRef(Navigation, 0, Coord.X, Coord.Y, Node));
					}
					const FIntPoint DecorationCoord = DirectionToNavCoord(
						Props.At(LNPRegressionFixture::CrustRadius, LNPRegressionFixture::DecorationTangent), CrustNav.Subdivisions);
					FLNPLocalNavNodeRef DecorationNode;
					TestTrue(TEXT("Regression Decoration does not remove its crust node"),
						LNPNavData::MakeLocalNodeRef(
							Navigation, 0, DecorationCoord.X, DecorationCoord.Y, DecorationNode));
				}
			}
		}
		FLNPSpawnData SpawnData;
		if (TestTrue(FString::Printf(TEXT("%s: Spawn payload decodes"), LevelPath),
			LNPSpawnData::Decode(First->SpawnPayload, SpawnData, Error)))
		{
			TestEqual(FString::Printf(TEXT("%s: decoded authored count"), LevelPath),
				SpawnData.AuthoredAnchors.Num(), FirstReport.SpawnAuthoredCount);
			TestEqual(FString::Printf(TEXT("%s: decoded random count"), LevelPath),
				SpawnData.RandomCandidates.Num(), FirstReport.SpawnCandidateCount);
			TestTrue(FString::Printf(TEXT("%s: has random spawn candidates"), LevelPath),
				!SpawnData.RandomCandidates.IsEmpty());
			if (FStringView(LevelPath) == FStringView(LNPRegressionFixture::LevelPath))
			{
				TestEqual(TEXT("Regression fixture has crust, island, and cave anchors"), SpawnData.AuthoredAnchors.Num(), 3);
				TSet<uint16> AuthoredLayers;
				for (const FLNPSpawnAuthoredAnchor& Anchor : SpawnData.AuthoredAnchors)
				{
					AuthoredLayers.Add(Anchor.LocalLayerId);
				}
				TestTrue(TEXT("Regression authored anchors include crust Layer 0"), AuthoredLayers.Contains(0));
				TestTrue(TEXT("Regression authored anchors include non-crust Layers"), AuthoredLayers.Num() >= 2);
			}
		}
		AddInfo(FString::Printf(TEXT("%s: seam hash x=0 %s y=0 %s z=0 %s"), LevelPath,
			*LexToString(Atlas.SeamHashes[0]), *LexToString(Atlas.SeamHashes[1]), *LexToString(Atlas.SeamHashes[2])));
	}
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPSpawnAuthoringValidationTest,
	"LootNPop.SurfaceNavigation.Bake.SpawnAuthoringValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPOctantBakeSourceResolutionTest,
	"LootNPop.SurfaceNavigation.Bake.OctantSourceResolution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLNPOctantBakeSourceResolutionTest::RunTest(const FString& Parameters)
{
	using namespace LNPOctantSurfaceBakerTest;
	const FSoftObjectPath LevelPath(FixtureLevelPath);
	TStrongObjectPtr<ULNPOctantSurfaceData> Fine(NewObject<ULNPOctantSurfaceData>(GetTransientPackage()));
	TStrongObjectPtr<ULNPOctantSurfaceData> Mixed(NewObject<ULNPOctantSurfaceData>(GetTransientPackage()));
	FLNPOctantBakeReport FineReport, MixedReport;
	FString Error;
	if (!FLNPOctantSurfaceBaker::Bake(LevelPath, FLNPOctantBakeOptions(), *Fine, FineReport, Error))
	{
		AddError(Error);
		return false;
	}
	UStaticMeshComponent* Source = nullptr;
	for (const FLNPOctantBakeLayerReport& Layer : FineReport.Layers)
	{
		UStaticMeshComponent* Candidate = FindObject<UStaticMeshComponent>(nullptr, *Layer.SourceName);
		if (Candidate && Candidate->GetStaticMesh()->GetName() == TEXT("SM_FixtureSplitSheet"))
		{
			Source = Candidate;
			break;
		}
	}
	if (!TestNotNull(TEXT("Split sheet source loads"), Source))
	{
		return false;
	}
	const TArray<FName> OriginalTags = Source->ComponentTags;
	ON_SCOPE_EXIT { Source->ComponentTags = OriginalTags; };
	Source->ComponentTags.AddUnique(TEXT("LNP.Surface.CoarseSupport"));
	if (!FLNPOctantSurfaceBaker::Bake(LevelPath, FLNPOctantBakeOptions(), *Mixed, MixedReport, Error))
	{
		AddError(Error);
		return false;
	}
	TestTrue(TEXT("Source resolution changes semantic hash"), Fine->Header.SourceSemanticHash != Mixed->Header.SourceSemanticHash);
	FLNPSupportAtlas Atlas;
	if (!LNPSupportAtlas::Decode(Mixed->SupportPayload, Atlas, Error))
	{
		AddError(Error);
		return false;
	}
	int32 CoarseLayers = 0;
	for (int32 LayerId = 1; LayerId < MixedReport.Layers.Num(); ++LayerId)
	{
		const FLNPOctantBakeLayerReport& Layer = MixedReport.Layers[LayerId];
		const bool bCoarse = Layer.SourceName == Source->GetPathName();
		CoarseLayers += bCoarse ? 1 : 0;
		const int32 Expected = MixedReport.Subdivisions * (bCoarse ? 1 : FLNPOctantBakeOptions().LayerSubdivisionMultiplier);
		TestEqual(TEXT("Baked layer follows its source tag"), Layer.Subdivisions, Expected);
		TestEqual(TEXT("Saved codec carries source grid"), Atlas.Layers[LayerId].Layout.Subdivisions, Expected);
	}
	TestEqual(TEXT("Both sheets of the tagged source are coarse"), CoarseLayers, 2);
	TestTrue(TEXT("Coarse source reduces payload"), Mixed->SupportPayload.Num() < Fine->SupportPayload.Num());
	return !HasAnyErrors();
}

bool FLNPSpawnAuthoringValidationTest::RunTest(const FString& Parameters)
{
	using namespace LNPOctantSurfaceBakerTest;
	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	const FAssetData Asset = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(LNPRegressionFixture::LevelPath));
	TSet<FName> LoadTags;
	LoadTags.Add(ULevel::LoadAllExternalObjectsTag);
	UWorld* SourceWorld = Asset.IsValid() ? Cast<UWorld>(Asset.GetAsset(MoveTemp(LoadTags))) : nullptr;
	if (!TestNotNull(TEXT("Regression fixture loads for Spawn authoring validation"), SourceWorld))
	{
		return false;
	}

	TArray<ALNPMassSpawnPoint*> Points;
	for (AActor* Actor : SourceWorld->PersistentLevel->Actors)
	{
		if (ALNPMassSpawnPoint* Point = Cast<ALNPMassSpawnPoint>(Actor))
		{
			Points.Add(Point);
		}
	}
	if (!TestEqual(TEXT("Regression fixture contains three authored anchors"), Points.Num(), 3))
	{
		return false;
	}
	Points.Sort([](const ALNPMassSpawnPoint& A, const ALNPMassSpawnPoint& B)
	{
		return A.GetActorLabel() < B.GetActorLabel();
	});

	const TArray<FTransform> OriginalTransforms = {
		Points[0]->GetActorTransform(), Points[1]->GetActorTransform(), Points[2]->GetActorTransform()};
	const TArray<FGuid> OriginalIds = {Points[0]->SpawnPointId, Points[1]->SpawnPointId, Points[2]->SpawnPointId};
	const TArray<FName> OriginalTargets = {Points[0]->TargetSpawnSetId, Points[1]->TargetSpawnSetId, Points[2]->TargetSpawnSetId};
	ON_SCOPE_EXIT
	{
		for (int32 Index = 0; Index < Points.Num(); ++Index)
		{
			Points[Index]->SetActorTransform(OriginalTransforms[Index]);
			Points[Index]->SpawnPointId = OriginalIds[Index];
			Points[Index]->TargetSpawnSetId = OriginalTargets[Index];
		}
	};

	auto Restore = [&]()
	{
		for (int32 Index = 0; Index < Points.Num(); ++Index)
		{
			Points[Index]->SetActorTransform(OriginalTransforms[Index]);
			Points[Index]->SpawnPointId = OriginalIds[Index];
			Points[Index]->TargetSpawnSetId = OriginalTargets[Index];
		}
	};
	auto ExpectBakeFailure = [&](const TCHAR* Case, const TCHAR* Expected)
	{
		TStrongObjectPtr<ULNPOctantSurfaceData> Data(NewObject<ULNPOctantSurfaceData>(GetTransientPackage()));
		FLNPOctantBakeReport Report;
		FString Error;
		const bool bResult = FLNPOctantSurfaceBaker::Bake(
			FSoftObjectPath(LNPRegressionFixture::LevelPath), FLNPOctantBakeOptions(), *Data, Report, Error);
		TestFalse(FString::Printf(TEXT("%s is rejected"), Case), bResult);
		TestTrue(FString::Printf(TEXT("%s reports '%s' (actual: %s)"), Case, Expected, *Error), Error.Contains(Expected));
	};

	Points[0]->TargetSpawnSetId = TEXT("DefinitelyMissing");
	ExpectBakeFailure(TEXT("Unknown SpawnSetId"), TEXT("unknown SpawnSetId"));
	Restore();

	const FVector AirDirection = OriginalTransforms[0].GetLocation().GetSafeNormal();
	Points[0]->SetActorLocation(AirDirection * (OriginalTransforms[0].GetLocation().Size() - 500.0));
	ExpectBakeFailure(TEXT("Airborne authored point"), TEXT("nearest interpolable Support surface"));
	Restore();

	Points[1]->SpawnPointId = Points[0]->SpawnPointId;
	ExpectBakeFailure(TEXT("Duplicate SpawnPointId"), TEXT("duplicates SpawnPointId"));
	Restore();

	const LNPRegressionFixture::FCaseFrame Props = LNPRegressionFixture::StaticProps();
	Points[0]->SetActorTransform(FTransform(
		Props.UpRotation(), Props.At(LNPRegressionFixture::CrustRadius, LNPRegressionFixture::TreeTangent)));
	ExpectBakeFailure(TEXT("Insufficient blocker clearance"), TEXT("does not have Pod capsule clearance"));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPCrustAtlasExactErrorTest,
	"LootNPop.SurfaceNavigation.Bake.CrustAtlasExactError",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLNPCrustAtlasExactErrorTest::RunTest(const FString& Parameters)
{
	using namespace LNPOctantSurfaceBakerTest;
	for (const TCHAR* LevelPath : {FixtureLevelPath, LNPRegressionFixture::LevelPath, MeadowLevelPath})
	{
		for (const double Spacing : {200.0, 100.0, 50.0})
		{
			FLNPOctantBakeOptions Options;
			Options.CrustSpacing = Spacing;
			TStrongObjectPtr<ULNPOctantSurfaceData> Data(NewObject<ULNPOctantSurfaceData>(GetTransientPackage()));
			FLNPOctantBakeReport Report;
			FString Error;
			FLNPSupportAtlas Atlas;
			if (!FLNPOctantSurfaceBaker::Bake(FSoftObjectPath(LevelPath), Options, *Data, Report, Error)
				|| !LNPSupportAtlas::Decode(Data->SupportPayload, Atlas, Error))
			{
				AddError(FString::Printf(TEXT("%s: %s"), LevelPath, *Error));
				continue;
			}
			const UStaticMeshComponent* Crust = FindObject<UStaticMeshComponent>(nullptr, *Report.CrustName);
			if (!TestNotNull(FString::Printf(TEXT("%s: crust component resolves"), LevelPath), Crust))
			{
				continue;
			}

			const FCrustExactWorld ExactWorld(*Crust);
			const double MaxRadius = Atlas.Layers[0].BaseRadius * 2.0;
			FRandomStream Random(RandomSeed);
			FErrorStats Stats;
			for (int32 Sample = 0; Sample < RandomDirectionCount; ++Sample)
			{
				const FVector3d Direction = Random.GetUnitVector().GetAbs();
				FLNPSupportLayerQuery AtlasHit;
				FHitResult ExactHit;
				const bool bSupported = LNPSupportAtlas::QueryLayer(Atlas.Layers[0], Direction, AtlasHit);
				Stats.Add(bSupported, AtlasHit, ExactWorld.Trace(Direction, MaxRadius, ExactHit), ExactHit, Direction);
			}
			AddInfo(FString::Printf(TEXT("%s spacing=%.0fcm N=%d payload=%lld bytes raster=%.2fs samplesNeedsExact=%.2f%% | %s"),
				LevelPath, Spacing, Atlas.Layers[0].Layout.Subdivisions, Report.PayloadBytes, Report.RasterSeconds,
				100.0 * Report.NeedsExactCount / Report.SampleCount, *Stats.ToString()));
			TestEqual(FString::Printf(TEXT("%s spacing %.0f: no ghost floor"), LevelPath, Spacing), Stats.GhostFloor, 0);
			if (Spacing == FLNPOctantBakeOptions().CrustSpacing)
			{
				TestTrue(FString::Printf(TEXT("%s: radius error P99 within %.1fcm"), LevelPath, MaxRadiusErrorP99),
					Percentile(Stats.RadiusErrors, 0.99) <= MaxRadiusErrorP99);
				TestTrue(FString::Printf(TEXT("%s: radius error within %.1fcm"), LevelPath, MaxRadiusError),
					Percentile(Stats.RadiusErrors, 1.0) <= MaxRadiusError);
				TestTrue(FString::Printf(TEXT("%s: normal error P99 within %.1fdeg"), LevelPath, MaxNormalErrorP99Deg),
					Percentile(Stats.NormalErrorsDeg, 0.99) <= MaxNormalErrorP99Deg);
			}

			// 입구 구멍 안쪽을 조밀하게 찍어 유령 지면이 없는지 본다.
			if (LevelPath == FixtureLevelPath)
			{
				const FVector3d HoleDirection = DirectionFromLatAz(FixtureHoleLatDeg, FixtureHoleAzDeg);
				FVector3d TangentA, TangentB;
				HoleDirection.FindBestAxisVectors(TangentA, TangentB);
				FErrorStats HoleStats;
				int32 ExactHoleCount = 0;
				for (int32 Sample = 0; Sample < 2000; ++Sample)
				{
					const double Angle = FMath::DegreesToRadians(FixtureHoleAngleDeg * 1.5) * FMath::Sqrt(Random.GetFraction());
					const double Phi = UE_TWO_PI * Random.GetFraction();
					const FVector3d Direction = (HoleDirection * FMath::Cos(Angle)
						+ (TangentA * FMath::Cos(Phi) + TangentB * FMath::Sin(Phi)) * FMath::Sin(Angle)).GetSafeNormal();
					FLNPSupportLayerQuery AtlasHit;
					FHitResult ExactHit;
					const bool bExactHit = ExactWorld.Trace(Direction, MaxRadius, ExactHit);
					ExactHoleCount += bExactHit ? 0 : 1;
					HoleStats.Add(LNPSupportAtlas::QueryLayer(Atlas.Layers[0], Direction, AtlasHit), AtlasHit, bExactHit, ExactHit, Direction);
				}
				AddInfo(FString::Printf(TEXT("%s spacing=%.0fcm hole cone: exactMiss=%d | %s"),
					LevelPath, Spacing, ExactHoleCount, *HoleStats.ToString()));
				TestTrue(FString::Printf(TEXT("Spacing %.0f: hole cone contains exact misses"), Spacing), ExactHoleCount > 0);
				TestEqual(FString::Printf(TEXT("Spacing %.0f: no ghost floor in the hole cone"), Spacing), HoleStats.GhostFloor, 0);
			}
		}
	}
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPLayerAtlasExactErrorTest,
	"LootNPop.SurfaceNavigation.Bake.LayerAtlasExactError",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLNPLayerAtlasExactErrorTest::RunTest(const FString& Parameters)
{
	using namespace LNPOctantSurfaceBakerTest;
	for (const TCHAR* LevelPath : {FixtureLevelPath, LNPRegressionFixture::LevelPath, MeadowLevelPath})
	{
		// source component는 Layer 해상도와 무관하므로 옥탄트마다 한 번만 복제한다.
		const FExactPhysicsWorld ExactWorld;
		TMap<FString, UStaticMeshComponent*> Probes;
		for (const int32 Multiplier : {2, 4})
		{
			FLNPOctantBakeOptions Options;
			Options.LayerSubdivisionMultiplier = Multiplier;
			TStrongObjectPtr<ULNPOctantSurfaceData> Data(NewObject<ULNPOctantSurfaceData>(GetTransientPackage()));
			FLNPOctantBakeReport Report;
			FString Error;
			FLNPSupportAtlas Atlas;
			if (!FLNPOctantSurfaceBaker::Bake(FSoftObjectPath(LevelPath), Options, *Data, Report, Error)
				|| !LNPSupportAtlas::Decode(Data->SupportPayload, Atlas, Error))
			{
				AddError(FString::Printf(TEXT("%s m=%d: %s"), LevelPath, Multiplier, *Error));
				continue;
			}

			FErrorStats LevelStats;
			int64 LayerBodyBytes = 0;
			for (int32 LayerId = 1; LayerId < Atlas.Layers.Num(); ++LayerId)
			{
				const FLNPSupportAtlasLayer& Layer = Atlas.Layers[LayerId];
				const FLNPOctantBakeLayerReport& LayerReport = Report.Layers[LayerId];
				LayerBodyBytes += LayerReport.BodyBytes;
				UStaticMeshComponent* Probe = Probes.FindRef(LayerReport.SourceName);
				if (!Probe)
				{
					const UStaticMeshComponent* Source = FindObject<UStaticMeshComponent>(nullptr, *LayerReport.SourceName);
					if (!TestNotNull(FString::Printf(TEXT("%s: Layer source component resolves"), *LayerReport.SourceName), Source))
					{
						continue;
					}
					Probe = Probes.Add(LayerReport.SourceName, ExactWorld.AddProbe(*Source));
				}
				const FLNPSupportFaceMap& FaceMap = Atlas.Sources[Layer.SourceIndex].FaceMap;

				// footprint 행·열 범위에 여유를 둔 영역에서 연속 격자 좌표를 고른다.
				const int32 N = Layer.Layout.Subdivisions;
				int32 IMin = MAX_int32;
				int32 IMax = MIN_int32;
				for (const FLNPSupportRowSpan& Span : Layer.Layout.Rows)
				{
					IMin = FMath::Min(IMin, Span.IStart);
					IMax = FMath::Max(IMax, Span.IStart + Span.Count - 1);
				}
				const double UMin = IMin - LayerFootprintMargin;
				const double UMax = IMax + LayerFootprintMargin;
				const double VMin = Layer.Layout.J0 - LayerFootprintMargin;
				const double VMax = Layer.Layout.J0 + Layer.Layout.Rows.Num() - 1 + LayerFootprintMargin;
				const double MaxRadius = Atlas.Layers[0].BaseRadius * 2.0;

				FRandomStream Random(RandomSeed + LayerId);
				FErrorStats Stats;
				int32 ExactHitCount = 0;
				int32 MissedFloor = 0;
				int32 Unresolved = 0;
				for (int32 Sample = 0; Sample < LayerDirectionCount;)
				{
					const double U = Random.FRandRange(UMin, UMax);
					const double V = Random.FRandRange(VMin, VMax);
					if (U < 0.0 || V < 0.0 || U + V > N)
					{
						continue;
					}
					++Sample;
					const FVector3d Direction = FVector3d(U, V, N - U - V).GetSafeNormal();
					FLNPSupportLayerQuery AtlasHit;
					FHitResult ExactHit;
					const bool bInterpolated = LNPSupportAtlas::QueryLayer(Layer, Direction, AtlasHit);
					const bool bExactHit = TraceLayer(*Probe, FaceMap, LayerId, Direction, MaxRadius, ExactHit, Unresolved);
					ExactHitCount += bExactHit ? 1 : 0;
					// Atlas가 Layer 후보로도 보지 않는데 exact는 Layer가 있는 방향. QueryLayers가 이 Layer를 건너뛰고 아래 Layer로 떨어진다.
					MissedFloor += bExactHit && !AtlasHit.IsCandidate() ? 1 : 0;
					// 둘 다 Layer가 없는 방향은 세지 않는다. NeedsExact 비율은 exact가 Layer를 맞힌 방향 중 보간하지 못한 비율이다.
					if (bExactHit || bInterpolated)
					{
						Stats.Add(bInterpolated, AtlasHit, bExactHit, ExactHit, Direction);
						LevelStats.Add(bInterpolated, AtlasHit, bExactHit, ExactHit, Direction);
					}
				}
				AddInfo(FString::Printf(TEXT("%s m=%d Layer %d %s: N=%d samples=%d valid=%d bakeNeedsExact=%.1f%% exactHits=%d missedFloor=%d | %s"),
					LevelPath, Multiplier, LayerId, *LayerReport.SourceKey, N, LayerReport.SampleCount, LayerReport.ValidCount,
					LayerReport.SampleCount > 0 ? 100.0 * LayerReport.NeedsExactCount / LayerReport.SampleCount : 0.0,
					ExactHitCount, MissedFloor, *Stats.ToString()));
				TestTrue(FString::Printf(TEXT("%s m=%d Layer %d: exact trace hits the Layer"), LevelPath, Multiplier, LayerId), ExactHitCount > 0);
				TestEqual(FString::Printf(TEXT("%s m=%d Layer %d: exact hits resolve through the face map"), LevelPath, Multiplier, LayerId),
					Unresolved, 0);
				TestEqual(FString::Printf(TEXT("%s m=%d Layer %d: no ghost floor"), LevelPath, Multiplier, LayerId), Stats.GhostFloor, 0);
				TestEqual(FString::Printf(TEXT("%s m=%d Layer %d: no missed floor at the footprint edge"), LevelPath, Multiplier, LayerId),
					MissedFloor, 0);
			}
			AddInfo(FString::Printf(TEXT("%s m=%d: Layers=%d payload=%lld bytes (Layer bodies %lld) layerRaster=%.2fs | all Layers %s"),
				LevelPath, Multiplier, Atlas.Layers.Num() - 1, Report.PayloadBytes, LayerBodyBytes, Report.LayerRasterSeconds,
				*LevelStats.ToString()));
			if (Multiplier == FLNPOctantBakeOptions().LayerSubdivisionMultiplier)
			{
				TestTrue(FString::Printf(TEXT("%s: Layer radius error P99 within %.1fcm"), LevelPath, MaxRadiusErrorP99),
					Percentile(LevelStats.RadiusErrors, 0.99) <= MaxRadiusErrorP99);
				TestTrue(FString::Printf(TEXT("%s: Layer radius error within %.1fcm"), LevelPath, MaxRadiusError),
					Percentile(LevelStats.RadiusErrors, 1.0) <= MaxRadiusError);
				TestTrue(FString::Printf(TEXT("%s: Layer normal error P99 within %.1fdeg"), LevelPath, MaxNormalErrorP99Deg),
					Percentile(LevelStats.NormalErrorsDeg, 0.99) <= MaxNormalErrorP99Deg);
			}
		}
	}
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPCrustSeamMatchTest,
	"LootNPop.SurfaceNavigation.Bake.CrustSeamMatch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLNPCrustSeamMatchTest::RunTest(const FString& Parameters)
{
	using namespace LNPOctantSurfaceBakerTest;
	const TConstArrayView<FRotator> SlotRotations = ULNPOctantSpawnSubsystem::OctantRotations;
	TArray<FLNPCrustSeamPair> Pairs;
	FString Error;
	if (!LNPCrustAtlas::ComputeSeamPairs(SlotRotations, Pairs, Error))
	{
		AddError(Error);
		return false;
	}

	// 같은 definition이 8 slot을 모두 채운다고 보고 저장된 Atlas를 slot 회전으로 합성한다.
	for (const TCHAR* LevelPath : {FixtureLevelPath, LNPRegressionFixture::LevelPath, MeadowLevelPath})
	{
		const FString SavedPackage = FLNPOctantSurfaceBaker::GetSurfaceDataPackageName(FSoftObjectPath(LevelPath));
		const ULNPOctantSurfaceData* Saved = LoadObject<ULNPOctantSurfaceData>(
			nullptr, *FString::Printf(TEXT("%s.%s"), *SavedPackage, *FPackageName::GetShortName(SavedPackage)));
		FLNPSupportAtlas Atlas;
		if (!TestNotNull(FString::Printf(TEXT("%s: saved SurfaceData exists"), *SavedPackage), Saved)
			|| !LNPSupportAtlas::Decode(Saved->SupportPayload, Atlas, Error))
		{
			AddError(FString::Printf(TEXT("%s: %s"), *SavedPackage, *Error));
			continue;
		}
		const int32 N = Atlas.Layers[0].Layout.Subdivisions;

		auto WorldDirection = [N, SlotRotations](int32 Slot, const FIntPoint& Coord)
		{
			return SlotRotations[Slot].RotateVector(LNPSupportAtlas::GetSampleDirection(N, Coord.X, Coord.Y));
		};
		auto WorldNormal = [&Atlas, N, SlotRotations](int32 Slot, const FIntPoint& Coord)
		{
			return SlotRotations[Slot].RotateVector(FVector3d(Atlas.Layers[0].GetNormal(LNPSupportAtlas::GetSampleIndex(N, Coord.X, Coord.Y))));
		};
		auto IsValid = [&Atlas, N](const FIntPoint& Coord)
		{
			return EnumHasAnyFlags(Atlas.Layers[0].GetFlags(LNPSupportAtlas::GetSampleIndex(N, Coord.X, Coord.Y)), ELNPSupportSampleFlags::Valid);
		};
		auto RadiusAt = [&Atlas, N](const FIntPoint& Coord)
		{
			return Atlas.Layers[0].GetRadius(LNPSupportAtlas::GetSampleIndex(N, Coord.X, Coord.Y));
		};

		// 변 중점을 포함한 모든 변 샘플.
		int32 DirectionMismatch = 0;
		int32 InvalidCount = 0;
		int32 RadiusMismatch = 0;
		double MaxRadiusDiff = 0.0;
		TArray<double> NormalDiffsDeg;
		for (const FLNPCrustSeamPair& Pair : Pairs)
		{
			for (int32 Step = 0; Step <= N; ++Step)
			{
				const FIntPoint CoordA = LNPCrustAtlas::GetSeamSampleCoord(N, Pair.A.Edge, Step);
				const FIntPoint CoordB = LNPCrustAtlas::GetSeamSampleCoord(N, Pair.B.Edge, Pair.bReversed ? N - Step : Step);
				DirectionMismatch += WorldDirection(Pair.A.Slot, CoordA).Equals(WorldDirection(Pair.B.Slot, CoordB), SeamDirectionTolerance) ? 0 : 1;
				if (!IsValid(CoordA) || !IsValid(CoordB))
				{
					++InvalidCount;
					continue;
				}
				const double RadiusDiff = FMath::Abs(RadiusAt(CoordA) - RadiusAt(CoordB));
				MaxRadiusDiff = FMath::Max(MaxRadiusDiff, RadiusDiff);
				RadiusMismatch += RadiusDiff <= SeamRadiusTolerance ? 0 : 1;
				NormalDiffsDeg.Add(AngleDeg(WorldNormal(Pair.A.Slot, CoordA), WorldNormal(Pair.B.Slot, CoordB)));
			}
		}

		// 옥탄트 꼭짓점(좌표축)은 네 slot이 만난다. 로컬 꼭짓점 +X·+Y·+Z의 격자 좌표.
		const FIntPoint CornerCoords[3] = {FIntPoint(N, 0), FIntPoint(0, N), FIntPoint(0, 0)};
		TMap<FIntVector, TArray<FIntPoint>> Corners;
		for (int32 Slot = 0; Slot < SlotRotations.Num(); ++Slot)
		{
			for (const FIntPoint& Coord : CornerCoords)
			{
				const FVector3d Direction = WorldDirection(Slot, Coord);
				Corners.FindOrAdd(FIntVector(FMath::RoundToInt32(Direction.X), FMath::RoundToInt32(Direction.Y), FMath::RoundToInt32(Direction.Z)))
					.Add(Coord);
			}
		}
		double MaxCornerSpread = 0.0;
		for (const TPair<FIntVector, TArray<FIntPoint>>& Corner : Corners)
		{
			TestEqual(FString::Printf(TEXT("%s: world corner %s joins 4 slots"), LevelPath, *Corner.Key.ToString()), Corner.Value.Num(), 4);
			double MinRadius = TNumericLimits<double>::Max();
			double MaxRadius = TNumericLimits<double>::Lowest();
			for (const FIntPoint& Coord : Corner.Value)
			{
				TestTrue(FString::Printf(TEXT("%s: world corner %s is valid"), LevelPath, *Corner.Key.ToString()), IsValid(Coord));
				MinRadius = FMath::Min(MinRadius, RadiusAt(Coord));
				MaxRadius = FMath::Max(MaxRadius, RadiusAt(Coord));
			}
			MaxCornerSpread = FMath::Max(MaxCornerSpread, MaxRadius - MinRadius);
		}
		TestEqual(FString::Printf(TEXT("%s: 6 world corners"), LevelPath), Corners.Num(), 6);

		AddInfo(FString::Printf(TEXT("%s: N=%d seamSamples=%d invalid=%d radiusDiff max=%.3fcm cornerSpread=%.3fcm normalDiff P50=%.3f P99=%.3f max=%.3fdeg"),
			LevelPath, N, Pairs.Num() * (N + 1), InvalidCount, MaxRadiusDiff, MaxCornerSpread,
			Percentile(NormalDiffsDeg, 0.5), Percentile(NormalDiffsDeg, 0.99), Percentile(NormalDiffsDeg, 1.0)));
		TestEqual(FString::Printf(TEXT("%s: seam sample directions match"), LevelPath), DirectionMismatch, 0);
		TestEqual(FString::Printf(TEXT("%s: seam samples are valid on both sides"), LevelPath), InvalidCount, 0);
		TestEqual(FString::Printf(TEXT("%s: seam radii match within %.1fcm"), LevelPath, SeamRadiusTolerance), RadiusMismatch, 0);
		TestTrue(FString::Printf(TEXT("%s: corner radii match within %.1fcm"), LevelPath, SeamRadiusTolerance),
			MaxCornerSpread <= SeamRadiusTolerance);
		// 법선은 옥탄트마다 한쪽 삼각형만 봐서 이음매에서 꺾인다(변위 마스크 기울기). 옥탄트 안이었다면 NeedsExact가 될 각도는 넘지 않아야 한다.
		const double MaxSeamNormalDiffDeg = FLNPSupportRasterSettings().MaxNeighborNormalAngleDeg;
		TestTrue(FString::Printf(TEXT("%s: seam normal crease within %.0fdeg"), LevelPath, MaxSeamNormalDiffDeg),
			Percentile(NormalDiffsDeg, 1.0) <= MaxSeamNormalDiffDeg);

		// 단일 대칭 프로필(D-030)이면 세 변의 hash가 같다. 짝 관계만으로는 x=0·y=0 일치와 z=0 회문이면 충분하므로 더 강한 조건이다.
		TestTrue(FString::Printf(TEXT("%s: seam hashes are identical across edges"), LevelPath),
			Atlas.SeamHashes[0] == Atlas.SeamHashes[1] && Atlas.SeamHashes[1] == Atlas.SeamHashes[2]);
	}
	return !HasAnyErrors();
}

#endif
