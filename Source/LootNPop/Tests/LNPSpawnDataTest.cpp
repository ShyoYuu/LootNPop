// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Algo/Reverse.h"
#include "SurfaceNavigation/LNPSpawnData.h"

namespace
{
	FLNPSpawnData MakeSpawnData()
	{
		FLNPSpawnData Data;
		FLNPSpawnAuthoredAnchor& B = Data.AuthoredAnchors.AddDefaulted_GetRef();
		B.SpawnPointId = FGuid(2, 0, 0, 0);
		B.TargetSpawnSetId = TEXT("Bench");
		B.LocalTransform = FTransform3f(FQuat4f::Identity, FVector3f(100.0f, 200.0f, 300.0f));
		B.LocalLayerId = 2;
		B.EdgeClearance = 400.0f;
		B.CapsuleClearance = 350.0f;

		FLNPSpawnAuthoredAnchor& A = Data.AuthoredAnchors.AddDefaulted_GetRef();
		A.SpawnPointId = FGuid(1, 0, 0, 0);
		A.LocalTransform = FTransform3f(FQuat4f::Identity, FVector3f(300.0f, 200.0f, 100.0f));
		A.LocalLayerId = 0;
		A.EdgeClearance = 800.0f;
		A.CapsuleClearance = 800.0f;

		FLNPSpawnRandomCandidate& Candidate = Data.RandomCandidates.AddDefaulted_GetRef();
		Candidate.CandidateIndex = 7;
		Candidate.LocalPosition = FVector3f(1000.0f, 2000.0f, 3000.0f);
		Candidate.LocalNormal = FVector3f(0.0f, 0.0f, -1.0f);
		Candidate.LocalLayerId = 3;
		Candidate.Allowed = ELNPSpawnCandidateFlags::Pod | ELNPSpawnCandidateFlags::Enemy;
		Candidate.SlopeDot = 1.0f;
		Candidate.EdgeClearance = 500.0f;
		Candidate.CapsuleClearance = 450.0f;
		return Data;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPSpawnCodecTest,
	"LootNPop.SurfaceNavigation.Bake.SpawnCodec",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLNPSpawnCodecTest::RunTest(const FString& Parameters)
{
	const FLNPSpawnData Source = MakeSpawnData();
	TArray<uint8> First;
	TArray<uint8> Second;
	FString Error;
	TestTrue(TEXT("Spawn payload encodes"), LNPSpawnData::Encode(Source, First, Error));

	FLNPSpawnData Reordered = Source;
	Algo::Reverse(Reordered.AuthoredAnchors);
	TestTrue(TEXT("Reordered records encode"), LNPSpawnData::Encode(Reordered, Second, Error));
	TestTrue(TEXT("Encoding is canonical"), First == Second);

	FLNPSpawnData Decoded;
	if (TestTrue(TEXT("Spawn payload decodes"), LNPSpawnData::Decode(First, Decoded, Error)))
	{
		TestEqual(TEXT("Authored count round trips"), Decoded.AuthoredAnchors.Num(), 2);
		TestEqual(TEXT("Random count round trips"), Decoded.RandomCandidates.Num(), 1);
		TestEqual(TEXT("Authored anchors are GUID sorted"), Decoded.AuthoredAnchors[0].SpawnPointId, FGuid(1, 0, 0, 0));
		TestEqual(TEXT("Target SpawnSetId round trips"), Decoded.AuthoredAnchors[1].TargetSpawnSetId, FName(TEXT("Bench")));
		TestEqual(TEXT("Layer round trips"), Decoded.RandomCandidates[0].LocalLayerId, static_cast<uint16>(3));
		TestTrue(TEXT("Normal round trips"), Decoded.RandomCandidates[0].LocalNormal.Equals(FVector3f(0.0f, 0.0f, -1.0f), 0.001f));
	}

	TArray<uint8> Trailing = First;
	Trailing.Add(0xff);
	TestFalse(TEXT("Trailing bytes are rejected"), LNPSpawnData::Decode(Trailing, Decoded, Error));

	FLNPSpawnData Duplicate = Source;
	Duplicate.AuthoredAnchors[1].SpawnPointId = Duplicate.AuthoredAnchors[0].SpawnPointId;
	TestFalse(TEXT("Duplicate SpawnPointId is rejected"), LNPSpawnData::Encode(Duplicate, Second, Error));
	return !HasAnyErrors();
}

#endif
