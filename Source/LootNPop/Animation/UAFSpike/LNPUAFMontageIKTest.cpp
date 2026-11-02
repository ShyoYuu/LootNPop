// Copyright (c) 2026 LootNPop. All rights reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Animation/UAFSpike/LNPUAFMontageIKTrait.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLNPUAFMontageIKTest, "LootNPop.Animation.UAF.HandIKFeedback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLNPUAFMontageIKTest::RunTest(const FString& Parameters)
{
	FLNPUAFHandIKFeedback Feedback;
	Feedback.OnBeginExecution(0.016f);
	// 뒤쪽 슬롯이 앞쪽 슬롯을 감쇠하는 교체 구간: local 최대0.8 대신 global 최대0.48이다.
	Feedback.RecordSlot(2, 0.4f, 1.0f);
	Feedback.RecordSlot(1, 0.8f, 0.6f);
	Feedback.RecordSlot(0, 0.0f, 0.12f);
	TestFalse(TEXT("Incomplete execution is not published"), Feedback.Read().bValid);
	TestTrue(TEXT("Evaluation can consume the current update before publication"), Feedback.ReadCurrentUpdate().bValid);
	TestTrue(TEXT("Evaluation uses current slot demand"), FMath::IsNearlyEqual(Feedback.ReadCurrentUpdate().GetDemand(0), 0.48f, 1.e-6f));
	Feedback.OnEndExecution(0.016f);
	const FLNPUAFHandIKSnapshot First = Feedback.Read();
	TestTrue(TEXT("All three slots completed"), First.bValid);
	TestTrue(TEXT("Actual global contribution attenuates a preceding slot"), FMath::IsNearlyEqual(First.GetDemand(0), 0.48f, 1.e-6f));
	TestEqual(TEXT("Curve may disable more IK than montages"), First.GetDemand(0.9f), 0.9f);
	TestEqual(TEXT("Curve is clamped"), First.GetDemand(2), 1.0f);

	Feedback.OnBeginExecution(0.016f);
	TestEqual(TEXT("Readers retain the completed execution while updating"), Feedback.Read().Serial, First.Serial);
	Feedback.RecordSlot(2, 0, 1);
	TestFalse(TEXT("Missing current slots keep aim correction disabled"), Feedback.ReadCurrentUpdate().bValid);
	Feedback.OnEndExecution(0.016f);
	TestFalse(TEXT("Missing slots invalidate the new execution"), Feedback.Read().bValid);
	TestEqual(TEXT("Missing slots do not retain previous montage demand"), Feedback.Read().GetDemand(0), 0.0f);

	Feedback.OnBeginExecution(0.016f);
	for (int32 Index = 0; Index < 3; ++Index)
		Feedback.RecordSlot(Index, 0, 1);
	Feedback.RecordSlot(0, 1, 1);
	TestFalse(TEXT("Duplicate current traversal keeps aim correction disabled"), Feedback.ReadCurrentUpdate().bValid);
	Feedback.OnEndExecution(0.016f);
	TestFalse(TEXT("Duplicate slot traversal cannot masquerade as one update"), Feedback.Read().bValid);

	Feedback.OnBeginExecution(0.016f);
	for (int32 Index = 0; Index < 3; ++Index)
		Feedback.RecordSlot(Index, 0, 1);
	Feedback.OnEndExecution(0.016f);
	TestTrue(TEXT("Idle execution recovers"), Feedback.Read().bValid);
	TestEqual(TEXT("Ended montages restore IK"), Feedback.Read().GetDemand(0), 0.0f);
	return true;
}

#endif
