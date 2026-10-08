// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "NiagaraComponent.h"
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
	TestTrue(TEXT("Grapple anchor visual auto activates"), Effect->bAutoActivate != 0);
	TestTrue(TEXT("Grapple anchor visual is visible"), Effect->IsVisible());
	TestFalse(TEXT("Grapple anchor visual is hidden in game"), Effect->bHiddenInGame != 0);
	TestFalse(TEXT("Grapple anchor actor is hidden in game"), Anchor->IsHidden());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
