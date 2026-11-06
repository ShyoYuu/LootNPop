// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Movement/LNPDashCooldownModifier.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLNPDashProxyDodgeTest,
	"LootNPop.Combat.JustDodge.ProxyWindow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLNPDashProxyDodgeTest::RunTest(const FString& Parameters)
{
	FLNPDashCooldownModifier Dash;
	Dash.DodgeIFrameEndOffsetMs = 150.f;
	TestEqual(TEXT("An unstarted dash has no cosmetic invulnerability"), Dash.GetDodgeIFrameRemainingSeconds(1000.0), -1.f);
	Dash.StartSimTimeMs = 1000.0;
	Dash.DurationMs = 1000.f;
	TestEqual(TEXT("A future dash does not open invulnerability early"), Dash.GetDodgeIFrameRemainingSeconds(999.0), -1.f);
	TestTrue(TEXT("A late dash state retains only the remaining 50 ms"),
		FMath::IsNearlyEqual(Dash.GetDodgeIFrameRemainingSeconds(1100.0), 0.05f));
	TestEqual(TEXT("An expired dash does not reopen on receipt"), Dash.GetDodgeIFrameRemainingSeconds(1151.0), -1.f);
	TestEqual(TEXT("An active cooldown does not extend invulnerability"), Dash.GetDodgeIFrameRemainingSeconds(1500.0), -1.f);
	TestTrue(TEXT("Replayed earlier state uses simulation time without a montage"),
		Dash.DashMontage == nullptr && FMath::IsNearlyEqual(Dash.GetDodgeIFrameRemainingSeconds(1050.0), 0.1f));

	// 서버가 원격 게스트의 누름을 95ms 소급하면 실행 후 남는 창은 55ms다.
	Dash.DodgeIFrameEndOffsetMs = 55.f;
	TestTrue(TEXT("The server press adjustment shortens the remaining window"),
		FMath::IsNearlyEqual(Dash.GetDodgeIFrameRemainingSeconds(1050.0), 0.005f));
	TestEqual(TEXT("The adjusted window expires before the nominal dash window"),
		Dash.GetDodgeIFrameRemainingSeconds(1060.0), -1.f);

	TArray<uint8> Bytes;
	FMemoryWriter Writer(Bytes);
	FObjectAndNameAsStringProxyArchive SaveArchive(Writer, false);
	Dash.NetSerialize(SaveArchive);
	FLNPDashCooldownModifier Received;
	FMemoryReader Reader(Bytes);
	FObjectAndNameAsStringProxyArchive LoadArchive(Reader, false);
	Received.NetSerialize(LoadArchive);
	TestFalse(TEXT("Dash state serializes without errors"), SaveArchive.IsError() || LoadArchive.IsError());
	TestEqual(TEXT("The replicated state preserves the simulation start"), Received.StartSimTimeMs, Dash.StartSimTimeMs);
	TestEqual(TEXT("The replicated state preserves the adjusted window"), Received.DodgeIFrameEndOffsetMs, 55.f);
	TestTrue(TEXT("The received state computes the same remaining window"),
		FMath::IsNearlyEqual(Received.GetDodgeIFrameRemainingSeconds(1050.0), 0.005f));

	TUniquePtr<FMovementModifierBase> Cloned(Received.Clone());
	TestEqual(TEXT("Rollback cloning preserves the adjusted window"),
		static_cast<FLNPDashCooldownModifier*>(Cloned.Get())->DodgeIFrameEndOffsetMs, 55.f);
	Received.DodgeIFrameEndOffsetMs = -25.f;
	TestEqual(TEXT("An already expired server window stays closed at dash execution"),
		Received.GetDodgeIFrameRemainingSeconds(1000.0), -1.f);
	return !HasAnyErrors();
}
#endif
