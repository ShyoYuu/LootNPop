// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/StaticMeshComponent.h"
#include "DataAsset/LNPOctantSurfaceData.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameLogic/LNPOctantSpawnSubsystem.h"
#include "Misc/PackageName.h"
#include "SurfaceNavigation/LNPCollisionChannels.h"
#include "SurfaceNavigation/LNPCrustAtlas.h"
#include "SurfaceNavigation/LNPOctantSurfaceBaker.h"
#include "SurfaceNavigation/LNPOctantTriangleExtractor.h"
#include "SurfaceNavigation/LNPRegressionFixture.h"
#include "UObject/StrongObjectPtr.h"

namespace LNPOctantSurfaceBakerTest
{
	constexpr TCHAR FixtureLevelPath[] =
		TEXT("/Game/Maps/SurfaceNavigation/Fixtures/LVI_Octant_Fixture_Crust.LVI_Octant_Fixture_Crust");
	constexpr TCHAR MeadowLevelPath[] = TEXT("/Game/Maps/Meadow_00/LVI_Octant_Meadow_00.LVI_Octant_Meadow_00");

	/** fixture 입구 구멍(`LNPSurfaceFixtureBuilder.cpp`): 위도 30°·방위 45°, 각반지름 1.5°. */
	constexpr double FixtureHoleLatDeg = 30.0;
	constexpr double FixtureHoleAzDeg = 45.0;
	constexpr double FixtureHoleAngleDeg = 1.5;

	/**
	 * 기본 해상도(100cm)의 Atlas 보간 대 exact 합격 기준. 2026-09-27 `Meadow_00` 실측(P99 1.06cm, 최대 5.0cm,
	 * 법선 P99 2.2°)에 약 2배 여유를 둔 값이다(`phases/Phase04a_CrustAtlasAndSeams.md` §3.6).
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

	/** 지각 component 하나만 등록한 physics world. exact 기준값은 `LNPSurfaceSupport` 채널 trace다. */
	struct FCrustExactWorld
	{
		TStrongObjectPtr<UWorld> World;

		explicit FCrustExactWorld(const UStaticMeshComponent& Crust)
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

			UStaticMeshComponent* Probe = NewObject<UStaticMeshComponent>(World.Get());
			Probe->SetStaticMesh(Crust.GetStaticMesh());
			Probe->SetWorldTransform(FLNPOctantTriangleExtractor::GetSourceTransform(Crust));
			Probe->SetCollisionProfileName(Crust.GetCollisionProfileName());
			Probe->RegisterComponentWithWorld(World.Get());
		}

		~FCrustExactWorld()
		{
			GEngine->DestroyWorldContext(World.Get());
			World->DestroyWorld(true);
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
		TestTrue(FString::Printf(TEXT("%s: source hash is identical across bakes"), LevelPath),
			First->Header.SourceContentHash == Second->Header.SourceContentHash);
		TestEqual(FString::Printf(TEXT("%s: descriptor size"), LevelPath),
			First->Header.Support.UncompressedSize, static_cast<uint64>(First->SupportPayload.Num()));
		TestEqual(FString::Printf(TEXT("%s: descriptor element count"), LevelPath),
			First->Header.Support.ElementCount, static_cast<uint32>(FirstReport.TotalSampleCount));

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
		AddInfo(FString::Printf(TEXT("%s: seam hash x=0 %s y=0 %s z=0 %s"), LevelPath,
			*LexToString(Atlas.SeamHashes[0]), *LexToString(Atlas.SeamHashes[1]), *LexToString(Atlas.SeamHashes[2])));
	}
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
