// Copyright (c) 2026 LootNPop. All rights reserved.

#include "UI/LNPDeathScreenWidget.h"

#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "TimerManager.h"

#define LOCTEXT_NAMESPACE "LNPDeath"

void ULNPDeathScreenWidget::ShowCountdown(float Seconds)
{
	UWorld* World = GetWorld();
	if (World == nullptr)
		return;

	RespawnWorldTime = World->GetTimeSeconds() + Seconds;

	SetVisibility(ESlateVisibility::HitTestInvisible);
	RefreshCountdown();

	// 1초 간격이면 충분하다 — 매 프레임 갱신할 이유가 없는 정수 표시다.
	World->GetTimerManager().SetTimer(CountdownTimer, FTimerDelegate::CreateUObject(
		this, &ULNPDeathScreenWidget::RefreshCountdown), 1.0f, /*bLoop=*/true);
}

void ULNPDeathScreenWidget::HideCountdown()
{
	if (UWorld* World = GetWorld())
		World->GetTimerManager().ClearTimer(CountdownTimer);

	SetVisibility(ESlateVisibility::Collapsed);
}

void ULNPDeathScreenWidget::RefreshCountdown()
{
	UWorld* World = GetWorld();
	if (World == nullptr || CountdownText == nullptr)
		return;

	const double Remaining = RespawnWorldTime - World->GetTimeSeconds();
	const int32 DisplaySeconds = FMath::Max(0, FMath::CeilToInt(Remaining));

	CountdownText->SetText(FText::Format(
		LOCTEXT("RespawnIn", "Respawning in {0}"), FText::AsNumber(DisplaySeconds)));

	// 0에 닿으면 더 셀 것이 없다. 오버레이를 걷는 건 리스폰 빙의(HideCountdown)의 몫이라
	// 여기서 숨기지 않는다 — 서버 타이머가 로컬 카운트보다 조금 늦게 도착할 수 있다.
	if (Remaining <= 0.0)
		World->GetTimerManager().ClearTimer(CountdownTimer);
}

void ULNPDeathScreenWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
		World->GetTimerManager().ClearTimer(CountdownTimer);

	Super::NativeDestruct();
}

#undef LOCTEXT_NAMESPACE
