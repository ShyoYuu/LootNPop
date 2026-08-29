// Copyright (c) 2026 LootNPop. All rights reserved.

#include "GAS/LNPPoiseDebugDrawProcessor.h"

#include "GAS/LNPPoiseTypes.h"
#include "GAS/LNPPoiseProcessor.h"
#include "Enemy/LNPEnemyMassTypes.h"
#include "Config/LNPSettings.h"
#include "LNPMassUtils.h"

#include "MassCommonFragments.h"
#include "MassExecutionContext.h"
#include "Engine/World.h"

#if WITH_EDITOR
#include "DrawDebugHelpers.h"

namespace
{
	TAutoConsoleVariable<int32> CVarDrawPoise(
		TEXT("LNP.Debug.DrawPoise"),
		0,
		TEXT("Draw the poise gauge above every entity that has one. Server only (poise is not replicated).\n")
		TEXT("  0: off (default)\n")
		TEXT("  1: on"),
		ECVF_Cheat);

	TAutoConsoleVariable<float> CVarDrawPoiseDistance(
		TEXT("LNP.Debug.DrawPoiseDistance"),
		5000.f,
		TEXT("Max distance (cm) from any player at which the poise gauge is drawn."),
		ECVF_Cheat);

	constexpr float BarWidth        = 70.f;   // 게이지 가로 길이 (cm)
	constexpr float BarHeightOffset = 130.f;  // 캡슐 중심에서 위로 띄우는 거리 (cm)
	constexpr float BarThickness    = 6.f;
	constexpr float TickHalfLength  = 7.f;

	/** 채움 비율에 따른 색: 초록 → 노랑 → 빨강. */
	FColor PoiseFillColor(float Fraction)
	{
		const FLinearColor Low  = FLinearColor::Green;
		const FLinearColor Mid  = FLinearColor(1.f, 0.85f, 0.f);
		const FLinearColor High = FLinearColor::Red;

		return (Fraction < 0.5f)
			? FLinearColor::LerpUsingHSV(Low, Mid, Fraction * 2.f).ToFColor(true)
			: FLinearColor::LerpUsingHSV(Mid, High, (Fraction - 0.5f) * 2.f).ToFColor(true);
	}
}

ULNPPoiseDebugDrawProcessor::ULNPPoiseDebugDrawProcessor()
	: PoiseQuery(*this), PlayerQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;   // DrawDebug* 는 게임 스레드 전용
	// 이번 프레임의 감쇠·임계 판정이 끝난 값을 그린다.
	ProcessingPhase = EMassProcessingPhase::PostPhysics;
	ExecutionOrder.ExecuteAfter.Add(ULNPPoiseProcessor::StaticClass()->GetFName());
}

void ULNPPoiseDebugDrawProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	PoiseQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	PoiseQuery.AddRequirement<FLNPPoiseFragment>(EMassFragmentAccess::ReadOnly);
	PoiseQuery.RegisterWithProcessor(*this);

	// 거리 컬링 기준점. 기존 ULNPEnemyDebugDrawProcessor와 같은 방식이다.
	PlayerQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	PlayerQuery.AddTagRequirement<FLNPPlayerTag>(EMassFragmentPresence::All);
	PlayerQuery.RegisterWithProcessor(*this);
}

void ULNPPoiseDebugDrawProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	if (CVarDrawPoise.GetValueOnGameThread() == 0)
		return;

	// 경직도는 서버 권위 값이라 클라이언트 월드에서는 전부 0이다 — 그릴 이유가 없다.
	if (LNPMass::IsClientWorld(EntityManager))
		return;

	UWorld* World = EntityManager.GetWorld();
	if (!World)
		return;

	const ULNPSettings* Settings = GetDefault<ULNPSettings>();
	// 적 디버그 드로우(ULNPEnemyDebugDrawProcessor)와 거리를 공유하지 않는다 — 패링 넉백처럼
	// 대상이 멀리 튕겨 나가는 상황을 쫓아가야 해서 훨씬 넓어야 하고, 테스트 중 조절할 수 있어야 한다.
	const float ProximityDistSq  = FMath::Square(FMath::Max(1.f, CVarDrawPoiseDistance.GetValueOnGameThread()));

	TArray<FVector> PlayerLocations;
	PlayerQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& Ctx)
	{
		const TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();
		for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
			PlayerLocations.Add(Transforms[i].GetTransform().GetLocation());
	});

	if (PlayerLocations.IsEmpty())
		return;

	PoiseQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& Ctx)
	{
		const TConstArrayView<FTransformFragment>  Transforms = Ctx.GetFragmentView<FTransformFragment>();
		const TConstArrayView<FLNPPoiseFragment>   Poises     = Ctx.GetFragmentView<FLNPPoiseFragment>();

		for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
		{
			const FVector Location = Transforms[i].GetTransform().GetLocation();

			// 가장 가까운 플레이어를 시선 기준으로 삼는다 — 게이지를 그쪽으로 눕혀야 판독이 된다.
			int32 NearestIdx  = INDEX_NONE;
			float NearestDist = TNumericLimits<float>::Max();
			for (int32 p = 0; p < PlayerLocations.Num(); ++p)
			{
				const float DistSq = FVector::DistSquared(Location, PlayerLocations[p]);
				if (DistSq < NearestDist)
				{
					NearestDist = DistSq;
					NearestIdx  = p;
				}
			}
			if (NearestIdx == INDEX_NONE || NearestDist >= ProximityDistSq)
				continue;

			const FLNPPoiseFragment& Poise = Poises[i];

			// 임계값은 폰별이다 — 적은 딜 구간이 넓고 플레이어는 좁다.
			const float DownThreshold = FMath::Max(1.f, Poise.DownThreshold);
			const float LightFraction = FMath::Clamp(Poise.StaggerThreshold / DownThreshold, 0.f, 1.f);

			// 구 내벽이라 Up은 월드 중심 방향이다 (이동·판정 코드와 같은 규약).
			const FVector UpDir     = (-Location).GetSafeNormal();
			const FVector ToViewer  = (PlayerLocations[NearestIdx] - Location).GetSafeNormal();
			FVector RightDir = FVector::CrossProduct(UpDir, ToViewer).GetSafeNormal();
			if (RightDir.IsNearlyZero())
				RightDir = Transforms[i].GetTransform().GetRotation().GetRightVector();

			const FVector BarCenter = Location + UpDir * BarHeightOffset;
			const FVector BarLeft   = BarCenter - RightDir * (BarWidth * 0.5f);
			const FVector BarRight  = BarCenter + RightDir * (BarWidth * 0.5f);

			// 바탕
			DrawDebugLine(World, BarLeft, BarRight, FColor(30, 30, 30), false, -1.f, 0, BarThickness);

			const float Fraction  = FMath::Clamp(Poise.Current / DownThreshold, 0.f, 1.f);
			const bool  bDowned   = Poise.ImmunityTimeRemaining > 0.f;   // 다운은 게이지를 0으로 리셋한다
			const bool  bGroggy   = Poise.bIsGroggy != 0;

			// 자주색 = 다운(면역 소진 중), 시안 = 그로기(딜 구간), 그 외 초록→노랑→빨강.
			const FColor FillColor = bDowned ? FColor(190, 90, 255)
			                       : bGroggy ? FColor::Cyan
			                       : PoiseFillColor(Fraction);

			// 다운 중에는 면역이 얼마나 남았는지를 게이지로 보여 준다 (게이지 자체는 0이므로).
			const float DrawFraction = bDowned
				? FMath::Clamp(Poise.ImmunityTimeRemaining / FMath::Max(0.01f, Settings->PoiseDownImmunitySeconds), 0.f, 1.f)
				: Fraction;

			if (DrawFraction > 0.f)
			{
				DrawDebugLine(World, BarLeft, BarLeft + RightDir * (BarWidth * DrawFraction),
					FillColor, false, -1.f, 0, BarThickness);
			}

			// 1단계 임계 눈금 — 여기를 넘으면 짧은 경직, 오른쪽 끝이 2단계다.
			const FVector TickBase = BarLeft + RightDir * (BarWidth * LightFraction);
			DrawDebugLine(World, TickBase - UpDir * TickHalfLength, TickBase + UpDir * TickHalfLength,
				FColor::White, false, -1.f, 0, 2.f);

			// 숫자까지 찍어야 저항 미러(FLNPPoiseFragment::Resistance)가 실제로 들어왔는지 확인된다.
			FString Label;
			if (bDowned)
				Label = FString::Printf(TEXT("DOWN  immune %.1fs"), Poise.ImmunityTimeRemaining);
			else if (bGroggy)
				Label = FString::Printf(TEXT("GROGGY %.1fs   %.0f / %.0f"), Poise.GroggyElapsed, Poise.Current, DownThreshold);
			else
				Label = FString::Printf(TEXT("%.0f / %.0f   R %.0f"), Poise.Current, DownThreshold, Poise.Resistance);
			DrawDebugString(World, BarCenter + UpDir * 14.f, Label, nullptr, FillColor, 0.f, true, 1.1f);
		}
	});
}

#else   // !WITH_EDITOR

ULNPPoiseDebugDrawProcessor::ULNPPoiseDebugDrawProcessor()
	: PoiseQuery(*this), PlayerQuery(*this)
{
	bAutoRegisterWithProcessingPhases = false;
}
void ULNPPoiseDebugDrawProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>&) {}
void ULNPPoiseDebugDrawProcessor::Execute(FMassEntityManager&, FMassExecutionContext&) {}

#endif
