// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Config/LNPSettings.h"
#include "DataAsset/LNPOctantPoolData.h"
#include "DataAsset/LNPOctantSurfaceData.h"
#include "GameLogic/LNPOctantSpawnSubsystem.h"
#include "SurfaceNavigation/LNPCrustAtlas.h"
#include "SurfaceNavigation/LNPNavQuery.h"
#include "SurfaceNavigation/LNPRegressionFixture.h"
#include "SurfaceNavigation/LNPSpawnData.h"

namespace LNPNavReachabilityTest
{
	constexpr TCHAR RegressionSurfacePath[] =
		TEXT("/Game/Maps/SurfaceNavigation/Fixtures/DA_OctantSurface_Fixture_Regression.DA_OctantSurface_Fixture_Regression");

	/** 지면에서 위(중심 방향)로 띄운 발 위치. Support 창 [발 - 50, 발 + 100]이 지면만 담도록 한다. */
	constexpr double FeetLift = 5.0;

	/** 발 위치에서 Support handle을 찾아 같은 Layer의 node와 group을 얻는다. */
	bool ProjectFeet(
		const FLNPSurfaceDataSnapshot& Snapshot, const FVector3d& Feet, FLNPNavProjection& OutProjection,
		FLNPNavGroupRef& OutGroup, uint16* OutLayer = nullptr)
	{
		FLNPSurfaceQuery Query;
		Query.WorldPosition = Feet;
		Query.MaxStepUp = 50.0;
		Query.MaxDrop = 100.0;
		FLNPSurfaceQueryResult Result;
		if (LNPSurfaceDataLoading::QuerySupport(Snapshot, Query, Result) != ELNPSurfaceQueryStatus::HighConfidence)
		{
			return false;
		}
		if (OutLayer != nullptr)
		{
			*OutLayer = Result.Surface.LocalLayerId;
		}
		return LNPNavQuery::ProjectToNode(Snapshot, Feet, Result.Surface, LNPNavQuery::DefaultProjectionRadius, OutProjection)
			&& LNPNavQuery::GetReachabilityGroup(Snapshot, OutProjection.Node, OutGroup);
	}

	/** handle을 직접 지정해 투영한다. Spawn stream처럼 slot·Layer를 이미 아는 입력용이다. */
	bool ProjectOnLayer(
		const FLNPSurfaceDataSnapshot& Snapshot, const int32 Slot, const uint16 Layer, const FVector3d& WorldPosition,
		FLNPNavProjection& OutProjection, FLNPNavGroupRef& OutGroup)
	{
		return LNPNavQuery::ProjectToNode(Snapshot, WorldPosition, {static_cast<uint16>(Slot), Layer, Snapshot.Generation},
				LNPNavQuery::DefaultProjectionRadius, OutProjection)
			&& LNPNavQuery::GetReachabilityGroup(Snapshot, OutProjection.Node, OutGroup);
	}

	struct FSpawnProjectionStats
	{
		int32 Anchors = 0;
		int32 AnchorFailures = 0;
		int32 Candidates = 0;
		int32 CandidateFailures = 0;
		/** 성공한 투영 거리. 7b 목표 스냅 반경 입력이다. */
		TArray<double> Distances;
		/** grid edge가 없는 node와 작은 runtime StaticNavComponent로 투영된 수. 7b 목표 스냅 정책 입력이다. */
		int32 IsolatedNodeHits = 0;
		int32 SmallComponentHits = 0;
		static constexpr uint64 SmallComponentNodes = 10;
		TArray<FString> FailureSamples;

		double Percentile(const double Fraction) const
		{
			return Distances.IsEmpty() ? 0.0
				: Distances[FMath::Clamp(FMath::CeilToInt32(Fraction * Distances.Num()) - 1, 0, Distances.Num() - 1)];
		}
	};

	/**
	 * 투영 실패 사유. 순서대로 group 없음, 막혔거나 비활성 Layer인 node, 넓은 반경의 최근접 열린 node, node 없음이다.
	 * 다음 콘텐츠에서 실패가 나오면 재현 없이 원인 방향을 알 수 있게 한다.
	 */
	FString DescribeProjectionFailure(
		const FLNPSurfaceDataSnapshot& Snapshot, const int32 Slot, const uint16 Layer, const FVector3d& WorldPosition)
	{
		constexpr double WideRadius = 3000.0;
		const FLNPSurfaceHandle Handle{static_cast<uint16>(Slot), Layer, Snapshot.Generation};
		FLNPNavProjection Projection;
		if (LNPNavQuery::ProjectToNode(Snapshot, WorldPosition, Handle, LNPNavQuery::DefaultProjectionRadius, Projection))
		{
			return FString::Printf(TEXT("node at %.1fcm has no reachability group"), Projection.Distance);
		}
		const FLNPSurfaceDataSlotSnapshot& SlotSnapshot = Snapshot.Slots[Slot];
		double LocalDistance = 0.0;
		if (SlotSnapshot.Navigation.IsValid() && SlotSnapshot.Support.IsValid()
			&& LNPNavQuery::FindLocalNodeNear(*SlotSnapshot.Navigation, *SlotSnapshot.Support, Layer,
				SlotSnapshot.WorldToSlotRotation.RotateVector(WorldPosition), LNPNavQuery::DefaultProjectionRadius, LocalDistance))
		{
			return FString::Printf(TEXT("node at %.1fcm is blocked or on an inactive layer"), LocalDistance);
		}
		if (LNPNavQuery::ProjectToNode(Snapshot, WorldPosition, Handle, WideRadius, Projection))
		{
			return FString::Printf(TEXT("nearest open node %.1fcm"), Projection.Distance);
		}
		return FString::Printf(TEXT("no open node within %.0fcm"), WideRadius);
	}

	/**
	 * 모든 slot의 authored anchor와, asset마다 처음 쓰는 slot의 random candidate를 같은 Layer node로 투영해 Stats에 더한다.
	 * Slots가 있으면 그 slot만 본다. 여러 배치에 걸쳐 풀 정의마다 한 번씩 검사할 때 쓴다.
	 */
	void ProjectSpawnStream(const FLNPSurfaceDataSnapshot& Snapshot, FSpawnProjectionStats& Stats, const TBitArray<>* Slots = nullptr)
	{
		TSet<const FLNPSpawnData*> CandidateAssets;
		// runtime StaticNavComponent별 node 수. asset-local descriptor를 runtime ID로 합산한다.
		TArray<uint64> ComponentSizes;
		ComponentSizes.SetNumZeroed(Snapshot.Nav.RuntimeStaticComponentCount);
		for (int32 Slot = 0; Slot < Snapshot.Slots.Num(); ++Slot)
		{
			for (const FLNPNavStaticComponent& Component : Snapshot.Slots[Slot].Traversal->StaticComponents)
			{
				const uint32 Runtime = LNPNavRuntime::GetRuntimeStaticComponent(
					Snapshot.Nav, Slot, Component.LocalStaticComponentId);
				if (ComponentSizes.IsValidIndex(Runtime))
				{
					ComponentSizes[Runtime] += Component.NodeCount;
				}
			}
		}
		auto Record = [&Stats, &Snapshot, &ComponentSizes](
			const bool bProjected, const FLNPNavProjection& Projection, int32& Failures, const FString& Label)
		{
			if (bProjected)
			{
				Stats.Distances.Add(Projection.Distance);
				int32 Slot = INDEX_NONE;
				FLNPLocalNavNodeRef Local;
				const FLNPNavCell* Cell = LNPNavRuntime::ResolveRuntimeNodeRef(Snapshot.Nav, Projection.Node, Slot, Local)
					? LNPNavData::ResolveLocalNode(*Snapshot.Slots[Slot].Navigation, Local) : nullptr;
				Stats.IsolatedNodeHits += Cell != nullptr && Cell->EdgeMask == 0 ? 1 : 0;
				uint32 Component = MAX_uint32;
				Stats.SmallComponentHits += LNPNavQuery::GetStaticComponent(Snapshot, Projection.Node, Component)
					&& ComponentSizes.IsValidIndex(Component)
					&& ComponentSizes[Component] <= FSpawnProjectionStats::SmallComponentNodes ? 1 : 0;
				return;
			}
			++Failures;
			if (Stats.FailureSamples.Num() < 8)
			{
				Stats.FailureSamples.Add(Label);
			}
		};
		for (int32 Slot = 0; Slot < Snapshot.Slots.Num(); ++Slot)
		{
			if (Slots != nullptr && !(*Slots)[Slot])
			{
				continue;
			}
			const FLNPSurfaceDataSlotSnapshot& SlotSnapshot = Snapshot.Slots[Slot];
			const FString Asset = SlotSnapshot.SurfaceDataAsset.GetAssetName();
			for (const FLNPSpawnAuthoredAnchor& Anchor : SlotSnapshot.Spawn->AuthoredAnchors)
			{
				const FVector3d Local(Anchor.LocalTransform.GetLocation());
				const FVector3d World = SlotSnapshot.SlotRotation.RotateVector(Local);
				FLNPNavProjection Projection;
				FLNPNavGroupRef Group;
				++Stats.Anchors;
				const bool bProjected = ProjectOnLayer(Snapshot, Slot, Anchor.LocalLayerId, World, Projection, Group);
				Record(bProjected, Projection, Stats.AnchorFailures, bProjected ? FString()
					: FString::Printf(TEXT("anchor %s slot=%d layer=%u local=%s: %s"), *Asset, Slot, Anchor.LocalLayerId,
						*Local.ToString(), *DescribeProjectionFailure(Snapshot, Slot, Anchor.LocalLayerId, World)));
			}
			bool bAlreadyCounted = false;
			CandidateAssets.Add(SlotSnapshot.Spawn.Get(), &bAlreadyCounted);
			if (bAlreadyCounted)
			{
				continue;
			}
			for (const FLNPSpawnRandomCandidate& Candidate : SlotSnapshot.Spawn->RandomCandidates)
			{
				const FVector3d Local(Candidate.LocalPosition);
				const FVector3d World = SlotSnapshot.SlotRotation.RotateVector(Local);
				FLNPNavProjection Projection;
				FLNPNavGroupRef Group;
				++Stats.Candidates;
				const bool bProjected = ProjectOnLayer(Snapshot, Slot, Candidate.LocalLayerId, World, Projection, Group);
				Record(bProjected, Projection, Stats.CandidateFailures, bProjected ? FString()
					: FString::Printf(TEXT("candidate %u %s slot=%d layer=%u allowed=%u local=%s: %s"), Candidate.CandidateIndex,
						*Asset, Slot, Candidate.LocalLayerId, static_cast<uint32>(Candidate.Allowed), *Local.ToString(),
						*DescribeProjectionFailure(Snapshot, Slot, Candidate.LocalLayerId, World)));
			}
		}
		Stats.Distances.Sort();
	}

	TArray<FLNPOctantDefinition> MakeRegressionDefinitions()
	{
		TArray<FLNPOctantDefinition> Definitions;
		Definitions.SetNum(8);
		for (FLNPOctantDefinition& Definition : Definitions)
		{
			Definition.LevelAsset = TSoftObjectPtr<UWorld>(FSoftObjectPath(LNPRegressionFixture::LevelPath));
			Definition.SurfaceData = TSoftObjectPtr<ULNPOctantSurfaceData>(FSoftObjectPath(RegressionSurfacePath));
			Definition.SeamSignature = TEXT("RegressionSeam");
		}
		return Definitions;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPNavRegressionReachabilityTest,
	"LootNPop.SurfaceNavigation.Nav.RegressionReachability",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPNavProductionSpawnProjectionTest,
	"LootNPop.SurfaceNavigation.Nav.ProductionSpawnProjection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLNPNavRegressionReachabilityTest::RunTest(const FString& Parameters)
{
	using namespace LNPNavReachabilityTest;
	namespace Fixture = LNPRegressionFixture;

	ULNPOctantSurfaceData* Regression = TSoftObjectPtr<ULNPOctantSurfaceData>(
		FSoftObjectPath(RegressionSurfacePath)).LoadSynchronous();
	if (!TestNotNull(TEXT("Regression SurfaceData loads"), Regression))
	{
		return false;
	}
	TArray<ULNPOctantSurfaceData*> LoadedData;
	LoadedData.Init(Regression, 8);
	FLNPSurfaceDataSnapshot Snapshot;
	FString Error;
	if (!TestTrue(TEXT("Regression 8-slot snapshot builds"),
		LNPSurfaceDataLoading::ValidateAndBuildSnapshot(MakeRegressionDefinitions(), LoadedData, 3, Snapshot, Error)))
	{
		AddError(Error);
		return false;
	}
	const FLNPNavSnapshot& Nav = Snapshot.Nav;
	TestEqual(TEXT("ConnectivityGraphVersion starts at 1"), Nav.ConnectivityGraphVersion, 1u);
	// 지각·경사로 공동 1개 + slot당 끊긴 섬 4개·수직 통로 공동 1개.
	TestEqual(TEXT("Regression has one crust group and five isolated groups per slot"), Nav.ReachabilityGroupCount, 41u);

	const double R = Fixture::CrustRadius;
	FLNPNavGroupRef CrustGroup;
	TSet<uint32> IslandGroups;
	for (int32 Slot = 0; Slot < 8; ++Slot)
	{
		const FQuat4d& Rotation = Snapshot.Slots[Slot].SlotRotation;
		auto World = [&Rotation](const FVector& Local) { return Rotation.RotateVector(FVector3d(Local)); };

		FLNPNavProjection Projection;
		FLNPNavGroupRef Group;
		uint16 Layer = MAX_uint16;
		if (!TestTrue(FString::Printf(TEXT("Slot %d basic crust projects"), Slot),
			ProjectFeet(Snapshot, World(Fixture::BasicCrust().At(R - FeetLift)), Projection, Group, &Layer) && Layer == 0))
		{
			continue;
		}
		if (Slot == 0)
		{
			CrustGroup = Group;
		}
		TestTrue(FString::Printf(TEXT("Slot %d crust reaches slot 0 crust through seams"), Slot),
			LNPNavQuery::TestReachability(Nav, CrustGroup, Group) == ELNPNavReachability::Reachable);

		const TPair<const TCHAR*, FVector> Islands[] = {
			{TEXT("IslandOne"), Fixture::IslandOne().At(Fixture::IslandOneTop - FeetLift)},
			{TEXT("IslandTwoOuter"), Fixture::IslandTwo().At(Fixture::IslandTwoOuterTop - FeetLift)},
			{TEXT("IslandTwoInner"), Fixture::IslandTwo().At(Fixture::IslandTwoInnerTop - FeetLift)},
			{TEXT("IslandEdge"), Fixture::IslandEdge().At(Fixture::IslandEdgeTop - FeetLift)},
		};
		for (const TPair<const TCHAR*, FVector>& Island : Islands)
		{
			FLNPNavGroupRef IslandGroup;
			uint16 IslandLayer = MAX_uint16;
			if (TestTrue(FString::Printf(TEXT("Slot %d %s projects to its own Layer"), Slot, Island.Key),
				ProjectFeet(Snapshot, World(Island.Value), Projection, IslandGroup, &IslandLayer) && IslandLayer != 0))
			{
				TestTrue(FString::Printf(TEXT("Slot %d %s is unreachable from the crust"), Slot, Island.Key),
					LNPNavQuery::TestReachability(Nav, CrustGroup, IslandGroup) == ELNPNavReachability::Unreachable);
				bool bDuplicate = false;
				IslandGroups.Add(IslandGroup.Group, &bDuplicate);
				TestFalse(FString::Printf(TEXT("Slot %d %s has its own group"), Slot, Island.Key), bDuplicate);
			}
		}
		// 섬 윗면에서 지각 Layer로 투영하면 2km 아래 지각으로 스냅하지 않는다.
		TestFalse(FString::Printf(TEXT("Slot %d island top does not snap to the crust Layer"), Slot),
			ProjectOnLayer(Snapshot, Slot, 0, World(Fixture::IslandOne().At(Fixture::IslandOneTop - FeetLift)),
				Projection, Group));

		// 나무·바위 둘레 500cm 원의 node가 모두 지각 group이면 dilation 뒤에도 우회로가 남은 것이다.
		const Fixture::FCaseFrame Props = Fixture::StaticProps();
		for (const TPair<const TCHAR*, double>& Prop : {
			TPair<const TCHAR*, double>(TEXT("Tree"), Fixture::TreeTangent),
			TPair<const TCHAR*, double>(TEXT("Rock"), Fixture::RockTangent)})
		{
			int32 RingReachable = 0;
			constexpr int32 RingSamples = 12;
			for (int32 Sample = 0; Sample < RingSamples; ++Sample)
			{
				const double Angle = UE_TWO_PI * Sample / RingSamples;
				const FVector Point = Props.At(R - FeetLift,
					Prop.Value + 500.0 * FMath::Cos(Angle), 500.0 * FMath::Sin(Angle));
				RingReachable += ProjectFeet(Snapshot, World(Point), Projection, Group)
					&& LNPNavQuery::TestReachability(Nav, CrustGroup, Group) == ELNPNavReachability::Reachable ? 1 : 0;
			}
			TestEqual(FString::Printf(TEXT("Slot %d %s ring stays in the crust group"), Slot, Prop.Key),
				RingReachable, RingSamples);
			TestTrue(FString::Printf(TEXT("Slot %d %s center has no walkable node under it"), Slot, Prop.Key),
				!ProjectFeet(Snapshot, World(Props.At(R - FeetLift, Prop.Value)), Projection, Group)
				|| Projection.Distance > 50.0);
		}
		TestTrue(FString::Printf(TEXT("Slot %d Decoration keeps a nearby crust node"), Slot),
			ProjectFeet(Snapshot, World(Props.At(R - FeetLift, Fixture::DecorationTangent)), Projection, Group)
			&& Projection.Distance < 150.0
			&& LNPNavQuery::TestReachability(Nav, CrustGroup, Group) == ELNPNavReachability::Reachable);

		// authored anchor: 지각·섬·동굴. 반지름으로 분류한다(구 내부이므로 동굴은 지각 R보다 바깥, 섬은 안쪽).
		int32 CaveAnchors = 0;
		for (const FLNPSpawnAuthoredAnchor& Anchor : Snapshot.Slots[Slot].Spawn->AuthoredAnchors)
		{
			const FVector3d Local(Anchor.LocalTransform.GetLocation());
			const double AnchorRadius = Local.Length();
			FLNPNavGroupRef AnchorGroup;
			if (!TestTrue(FString::Printf(TEXT("Slot %d anchor on Layer %u projects"), Slot, Anchor.LocalLayerId),
				ProjectOnLayer(Snapshot, Slot, Anchor.LocalLayerId, World(FVector(Local)), Projection, AnchorGroup)))
			{
				continue;
			}
			const ELNPNavReachability Expected = AnchorRadius < R - 100.0
				? ELNPNavReachability::Unreachable
				: ELNPNavReachability::Reachable;
			CaveAnchors += AnchorRadius > R + 100.0 ? 1 : 0;
			TestTrue(FString::Printf(TEXT("Slot %d anchor at radius %.0f has the expected crust reachability"), Slot, AnchorRadius),
				LNPNavQuery::TestReachability(Nav, CrustGroup, AnchorGroup) == Expected);
		}
		TestEqual(FString::Printf(TEXT("Slot %d has three cave anchors"), Slot), CaveAnchors, 3);
	}
	TestEqual(TEXT("All 32 islands have distinct groups"), IslandGroups.Num(), 32);
	TestFalse(TEXT("No island shares the crust group"), IslandGroups.Contains(CrustGroup.Group));

	// 12개 world seam 중점과 6개 world 꼭짓점: 양쪽(꼭짓점은 네) slot 사본이 모두 지각 group의 node로 투영된다.
	TArray<FLNPCrustSeamPair> Pairs;
	if (TestTrue(TEXT("Seam pairs compute"),
		LNPCrustAtlas::ComputeSeamPairs(MakeArrayView(ULNPOctantSpawnSubsystem::OctantRotations), Pairs, Error)))
	{
		TestEqual(TEXT("Twelve world seams"), Pairs.Num(), 12);
		const int32 N = Snapshot.Slots[0].Navigation->Layers[0].Subdivisions;
		for (const FLNPCrustSeamPair& Pair : Pairs)
		{
			const FIntPoint Coord = LNPCrustAtlas::GetSeamSampleCoord(N, Pair.A.Edge, N / 2);
			const FVector3d Position = Snapshot.Slots[Pair.A.Slot].SlotRotation.RotateVector(
				LNPSupportAtlas::GetSampleDirection(N, Coord.X, Coord.Y)) * (R - FeetLift);
			for (const int32 Slot : {Pair.A.Slot, Pair.B.Slot})
			{
				FLNPNavProjection Projection;
				FLNPNavGroupRef Group;
				TestTrue(FString::Printf(TEXT("Seam %d:%d-%d:%d midpoint projects from slot %d"),
						Pair.A.Slot, static_cast<int32>(Pair.A.Edge), Pair.B.Slot, static_cast<int32>(Pair.B.Edge), Slot),
					ProjectOnLayer(Snapshot, Slot, 0, Position, Projection, Group)
					&& Projection.Distance < 10.0
					&& LNPNavQuery::TestReachability(Nav, CrustGroup, Group) == ELNPNavReachability::Reachable);
			}
		}
	}
	else
	{
		AddError(Error);
	}
	for (const FVector3d& Axis : {FVector3d::XAxisVector, -FVector3d::XAxisVector, FVector3d::YAxisVector,
		-FVector3d::YAxisVector, FVector3d::ZAxisVector, -FVector3d::ZAxisVector})
	{
		int32 Copies = 0;
		for (int32 Slot = 0; Slot < 8; ++Slot)
		{
			const FVector3d Local = Snapshot.Slots[Slot].WorldToSlotRotation.RotateVector(Axis);
			if (Local.GetMax() < 1.0 - 1.e-6)
			{
				continue;
			}
			++Copies;
			FLNPNavProjection Projection;
			FLNPNavGroupRef Group;
			TestTrue(FString::Printf(TEXT("Vertex %s projects from slot %d"), *Axis.ToString(), Slot),
				ProjectOnLayer(Snapshot, Slot, 0, Axis * (R - FeetLift), Projection, Group)
				&& Projection.Distance < 10.0
				&& LNPNavQuery::TestReachability(Nav, CrustGroup, Group) == ELNPNavReachability::Reachable);
		}
		TestEqual(FString::Printf(TEXT("Vertex %s has four slot copies"), *Axis.ToString()), Copies, 4);
	}

	FSpawnProjectionStats Stats;
	ProjectSpawnStream(Snapshot, Stats);
	TestEqual(TEXT("Regression authored anchors all project"), Stats.AnchorFailures, 0);
	TestEqual(TEXT("Regression random candidates all project"), Stats.CandidateFailures, 0);
	for (const FString& Sample : Stats.FailureSamples)
	{
		AddInfo(Sample);
	}
	AddInfo(FString::Printf(TEXT("Regression Nav oracle: groups=%u anchors=%d candidates=%d projection P50=%.1f P99=%.1f max=%.1fcm"),
		Nav.ReachabilityGroupCount, Stats.Anchors, Stats.Candidates,
		Stats.Percentile(0.5), Stats.Percentile(0.99), Stats.Percentile(1.0)));
	return !HasAnyErrors();
}

bool FLNPNavProductionSpawnProjectionTest::RunTest(const FString& Parameters)
{
	using namespace LNPNavReachabilityTest;

	const ULNPSettings* Settings = GetDefault<ULNPSettings>();
	ULNPOctantPoolData* Pool = Settings ? Settings->OctantPool.LoadSynchronous() : nullptr;
	if (!TestNotNull(TEXT("Production OctantPool loads"), Pool))
	{
		return false;
	}
	TArray<FLNPOctantDefinition> EffectiveDefinitions;
	Pool->BuildEffectiveDefinitions(EffectiveDefinitions);
	// 후보는 asset마다 한 배치에서 한 번 센다. 기본 배치(seed 135792468)의 모든 slot을 본 뒤, 거기 없던 풀 정의는
	// 고정 seed 열에서 처음 나오는 배치의 그 slot만 본다. 풀이 8종을 넘거나 구성이 바뀌어도 모든 정의를 덮는다.
	TSet<FSoftObjectPath> Remaining;
	for (const FLNPOctantDefinition& Definition : EffectiveDefinitions)
	{
		if (!Definition.LevelAsset.IsNull())
		{
			Remaining.Add(Definition.SurfaceData.ToSoftObjectPath());
		}
	}
	FSpawnProjectionStats Stats;
	uint32 Groups = 0;
	uint32 Version = 0;
	FRandomStream SeedStream(20261108);
	int32 Seed = 135792468;
	for (int32 Attempt = 0; Attempt < 64 && !Remaining.IsEmpty(); ++Attempt, Seed = static_cast<int32>(SeedStream.GetUnsignedInt()))
	{
		TArray<FLNPOctantDefinition> SelectedDefinitions;
		FString Error;
		if (!TestTrue(FString::Printf(TEXT("Production definitions select all eight slots (seed %d)"), Seed),
			ULNPOctantSpawnSubsystem::SelectOctantDefinitions(EffectiveDefinitions, Seed, SelectedDefinitions, nullptr, &Error)))
		{
			AddError(Error);
			return false;
		}
		TBitArray<> Slots(Attempt == 0, SelectedDefinitions.Num());
		FString Layout;
		for (int32 Slot = 0; Slot < SelectedDefinitions.Num(); ++Slot)
		{
			const FSoftObjectPath Path = SelectedDefinitions[Slot].SurfaceData.ToSoftObjectPath();
			Slots[Slot] = Slots[Slot] || Remaining.Contains(Path);
			Layout += FString::Printf(TEXT("%s%s%s"), Slot == 0 ? TEXT("") : TEXT(", "), *Path.GetAssetName(),
				Slots[Slot] ? TEXT("") : TEXT("(skipped)"));
		}
		if (Slots.Find(true) == INDEX_NONE)
		{
			continue;
		}
		TArray<ULNPOctantSurfaceData*> LoadedData;
		for (const FLNPOctantDefinition& Definition : SelectedDefinitions)
		{
			LoadedData.Add(Definition.SurfaceData.LoadSynchronous());
		}
		FLNPSurfaceDataSnapshot Snapshot;
		if (!TestTrue(FString::Printf(TEXT("Production 8-slot snapshot builds (seed %d)"), Seed),
			LNPSurfaceDataLoading::ValidateAndBuildSnapshot(SelectedDefinitions, LoadedData, 1, Snapshot, Error)))
		{
			AddError(Error);
			return false;
		}
		ProjectSpawnStream(Snapshot, Stats, &Slots);
		for (int32 Slot = 0; Slot < SelectedDefinitions.Num(); ++Slot)
		{
			if (Slots[Slot])
			{
				Remaining.Remove(SelectedDefinitions[Slot].SurfaceData.ToSoftObjectPath());
			}
		}
		if (Attempt == 0)
		{
			Groups = Snapshot.Nav.ReachabilityGroupCount;
			Version = Snapshot.Nav.ConnectivityGraphVersion;
		}
		AddInfo(FString::Printf(TEXT("Seed %d slots: %s"), Seed, *Layout));
	}
	TestTrue(FString::Printf(TEXT("Every pool definition's spawn stream is projected (%d left)"), Remaining.Num()), Remaining.IsEmpty());
	TestEqual(TEXT("Production authored anchors all project"), Stats.AnchorFailures, 0);
	TestEqual(TEXT("Production random candidates all project"), Stats.CandidateFailures, 0);
	for (const FString& Sample : Stats.FailureSamples)
	{
		AddInfo(Sample);
	}
	AddInfo(FString::Printf(
		TEXT("Production Nav projection: groups=%u version=%u anchors=%d anchorFailures=%d candidates=%d candidateFailures=%d projection P50=%.1f P90=%.1f P99=%.1f max=%.1fcm isolatedNodeHits=%d smallComponentHits(<=%llu nodes)=%d"),
		Groups, Version, Stats.Anchors, Stats.AnchorFailures,
		Stats.Candidates, Stats.CandidateFailures,
		Stats.Percentile(0.5), Stats.Percentile(0.9), Stats.Percentile(0.99), Stats.Percentile(1.0),
		Stats.IsolatedNodeHits, FSpawnProjectionStats::SmallComponentNodes, Stats.SmallComponentHits));
	return !HasAnyErrors();
}

#endif // WITH_DEV_AUTOMATION_TESTS
