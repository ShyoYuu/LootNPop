// Copyright (c) 2026 LootNPop. All rights reserved.

#include "UI/Menu/LNPStatsViewModel.h"
#include "GAS/Attributes/LNPBaseAttributeSet.h"

#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"

namespace
{
	/** 라벨 열 너비 (모노스페이스 문자 수). 리드아웃의 값 열을 맞춘다. */
	constexpr int32 GLabelColumnWidth = 14;

	/** RichText 마크업에서 특별한 의미를 갖는 '<'만 이스케이프한다. */
	FString EscapeForRichText(const FString& In)
	{
		return In.Replace(TEXT("<"), TEXT("&lt;"));
	}
}

TArray<FGameplayAttribute> ULNPStatsViewModel::GetObservedAttributes()
{
	// 스탯 목록은 LNPStat 메타 테이블이 단일 출처다. Health만 HP 행 전용이라 따로 더한다.
	TArray<FGameplayAttribute> Attributes;
	Attributes.Add(ULNPBaseAttributeSet::GetHealthAttribute());
	for (const FLNPStatMeta& Stat : LNPStat::GetStatMetaTable())
		Attributes.Add(Stat.Attribute);

	return Attributes;
}

void ULNPStatsViewModel::Initialize(UAbilitySystemComponent* InASC)
{
	// 재초기화 안전망 — 기존 구독을 먼저 해제한다.
	Deinitialize();

	BoundASC = InASC;
	if (InASC != nullptr)
	{
		for (const FGameplayAttribute& Attribute : GetObservedAttributes())
		{
			AttributeHandles.Add(
				InASC->GetGameplayAttributeValueChangeDelegate(Attribute)
					.AddUObject(this, &ULNPStatsViewModel::OnObservedAttributeChanged));
		}
	}

	RebuildStatsRichText();
}

void ULNPStatsViewModel::Deinitialize()
{
	if (UAbilitySystemComponent* ASC = BoundASC.Get())
	{
		// 구독 시점과 같은 목록·순서이므로 인덱스로 짝지어 해제한다.
		const TArray<FGameplayAttribute> Attributes = GetObservedAttributes();
		for (int32 i = 0; i < AttributeHandles.Num() && i < Attributes.Num(); ++i)
		{
			ASC->GetGameplayAttributeValueChangeDelegate(Attributes[i]).Remove(AttributeHandles[i]);
		}
	}

	AttributeHandles.Reset();
	BoundASC.Reset();
}

void ULNPStatsViewModel::OnObservedAttributeChanged(const FOnAttributeChangeData& /*Data*/)
{
	// 어느 스탯이 바뀌든 리드아웃 전체를 다시 만든다 (행 수가 적어 비용이 무시할 만하다).
	RebuildStatsRichText();
}

float ULNPStatsViewModel::GetAdditiveResult(const UAbilitySystemComponent* ASC, const FGameplayAttribute& Attribute)
{
	if (ASC == nullptr)
		return 0.f;

	float Result = ASC->GetNumericAttributeBase(Attribute);

	for (auto It = ASC->GetActiveGameplayEffects().CreateConstIterator(); It; ++It)
	{
		const FActiveGameplayEffect& Effect = *It;
		if (Effect.bIsInhibited || Effect.Spec.Def == nullptr)
			continue;

		const TArray<FGameplayModifierInfo>& Modifiers = Effect.Spec.Def->Modifiers;
		for (int32 i = 0; i < Modifiers.Num(); ++i)
		{
			if (Modifiers[i].Attribute != Attribute)
				continue;

			// AddBase만 A(기초 스탯 총량)에 넣는다 — 무기 스텟·합연산 버프가 모두 이 채널로 들어온다.
			if (Modifiers[i].ModifierOp == EGameplayModOp::AddBase)
				Result += Effect.Spec.GetModifierMagnitude(i);
		}
	}

	return Result;
}

FString ULNPStatsViewModel::MakeStatLine(const FText& Label, float FinalValue, float AdditiveResult, ELNPStatDisplay Display)
{
	// B = 곱연산 총 배율. GE를 다시 순회해 계산하지 않고 최종값에서 역산한다 —
	// 그래야 표시값이 실제 게임플레이 값과 항상 일치한다.
	const float Multiplier = (FMath::Abs(AdditiveResult) > KINDA_SMALL_NUMBER)
		? FinalValue / AdditiveResult
		: 1.0f;

	const FString FinalStr      = LNPStat::FormatStatValue(FinalValue, Display);
	const FString AdditiveStr   = LNPStat::FormatStatValue(AdditiveResult, Display);
	const FString MultiplierStr = LNPStat::FormatStatValue(Multiplier, ELNPStatDisplay::Ratio);

	const FString PaddedLabel = EscapeForRichText(Label.ToString()).RightPad(GLabelColumnWidth);

	// "라벨          C (A × B)"
	return FString::Printf(
		TEXT("<sub>%s</><final>%s</><sub> (%s × </><buff>%s</><sub>)</>"),
		*PaddedLabel, *FinalStr, *AdditiveStr, *MultiplierStr);
}

void ULNPStatsViewModel::RebuildStatsRichText()
{
	UAbilitySystemComponent* ASC = BoundASC.Get();
	if (ASC == nullptr)
	{
		SetStatsRichText(FText::GetEmpty());
		return;
	}

	TArray<FString> Lines;

	// HP는 현재/최대 쌍이라 합·곱 분해 포맷을 쓰지 않는다 (MaxHealth 분해는 아래 Max HP 행이 담당).
	{
		const FString Health    = LNPStat::FormatStatValue(
			ASC->GetNumericAttribute(ULNPBaseAttributeSet::GetHealthAttribute()), ELNPStatDisplay::Integer);
		const FString MaxHealth = LNPStat::FormatStatValue(
			ASC->GetNumericAttribute(ULNPBaseAttributeSet::GetMaxHealthAttribute()), ELNPStatDisplay::Integer);

		Lines.Add(FString::Printf(TEXT("<sub>%s</><final>%s / %s</>"),
			*FString(TEXT("HP")).RightPad(GLabelColumnWidth), *Health, *MaxHealth));
	}

	for (const FLNPStatMeta& Stat : LNPStat::GetStatMetaTable())
	{
		Lines.Add(MakeStatLine(Stat.DisplayName,
			ASC->GetNumericAttribute(Stat.Attribute),
			GetAdditiveResult(ASC, Stat.Attribute),
			Stat.Display));
	}

	SetStatsRichText(FText::FromString(FString::Join(Lines, TEXT("\n"))));
}

void ULNPStatsViewModel::SetStatsRichText(const FText& InValue)
{
	UE_MVVM_SET_PROPERTY_VALUE(StatsRichText, InValue);
}
