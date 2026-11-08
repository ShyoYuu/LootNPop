// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "WorldDevice/LNPGrappleAnchor.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPGrappleAnchorVisualTest,
	"LootNPop.WorldDevice.GrappleAnchor.VisualAsset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLNPGrappleAnchorVisualTest::RunTest(const FString& Parameters)
{
	// 프롬프트와 이동은 VFX가 꺼져도 정상 동작하므로 시각 에셋 배선을 별도로 검사한다.
	UClass* AnchorClass = LoadClass<ALNPGrappleAnchor>(nullptr,
		TEXT("/Game/WorldDevice/BP_GrappleAnchor.BP_GrappleAnchor_C"));
	if (!TestNotNull(TEXT("Grapple anchor Blueprint loads"), AnchorClass))
	{
		return false;
	}

	const ALNPGrappleAnchor* Anchor = AnchorClass->GetDefaultObject<ALNPGrappleAnchor>();
	const UNiagaraComponent* Effect = Anchor->FindComponentByClass<UNiagaraComponent>();
	if (!TestNotNull(TEXT("Grapple anchor has a Niagara component"), Effect))
	{
		return false;
	}

	TestNotNull(TEXT("Grapple anchor visual system is assigned"), Effect->GetAsset());
	// 거리에 따라 로컬 플레이어가 활성화한다. 자동 활성화하면 먼 앵커와 서버에서도 실행된다.
	TestFalse(TEXT("Grapple anchor visual waits for local distance update"), Effect->bAutoActivate != 0);
	TestTrue(TEXT("Grapple anchor visual is visible"), Effect->IsVisible());
	TestFalse(TEXT("Grapple anchor visual is hidden in game"), Effect->bHiddenInGame != 0);
	TestFalse(TEXT("Grapple anchor actor is hidden in game"), Anchor->IsHidden());
	if (const UNiagaraSystem* System = Effect->GetAsset())
	{
		TestEqual(TEXT("Grapple anchor uses one emitter"), System->GetEmitterHandles().Num(), 1);
		TestFalse(TEXT("Grapple anchor does not wait for current frame data"), System->bRequireCurrentFrameData != 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPGrappleAnchorDistanceVisualTest,
	"LootNPop.WorldDevice.GrappleAnchor.DistanceVisualState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLNPGrappleAnchorDistanceVisualTest::RunTest(const FString& Parameters)
{
	auto StateAt = [](float Distance, float MaxDistance = 5000.f)
	{
		return ALNPGrappleAnchor::CalculateDistanceVisualState(FMath::Square(Distance), 100.f, MaxDistance);
	};

	TestEqual(TEXT("No visual without a configured range"), StateAt(1000.f, 0.f).A, 0.f);
	TestEqual(TEXT("Invisible at discovery limit"), StateAt(10000.f).A, 0.f);
	TestEqual(TEXT("Invisible beyond discovery limit"), StateAt(12000.f).A, 0.f);
	TestTrue(TEXT("Discovery edge fades smoothly"), FMath::IsNearlyEqual(StateAt(9000.f).A, 0.5f, KINDA_SMALL_NUMBER));
	TestTrue(TEXT("Ring fills when approaching"), StateAt(6000.f).R > StateAt(8000.f).R);
	TestEqual(TEXT("Just outside maximum range remains unavailable"), StateAt(5000.1f).G, 0.f);
	TestEqual(TEXT("Maximum range is inclusive"), StateAt(5000.f).G, 1.f);
	TestEqual(TEXT("Minimum range is inclusive"), StateAt(100.f).G, 1.f);
	TestEqual(TEXT("Too close is unavailable"), StateAt(99.f).G, 0.f);
	TestEqual(TEXT("Short marker range is respected"), StateAt(2500.f, 2000.f).G, 0.f);
	TestEqual(TEXT("Long marker range is respected"), StateAt(6000.f, 8000.f).G, 1.f);
	TestEqual(TEXT("Discovery limit follows marker range"), StateAt(4000.f, 2000.f).A, 0.f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
