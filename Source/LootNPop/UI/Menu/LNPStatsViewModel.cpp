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
	// AttackMultiplier는 별도 행이 아니라 공격력 행의 곱연산 재료로 쓰이지만,
	// 값이 바뀌면 공격력 행이 달라지므로 구독 대상에는 포함한다.
	return {
		ULNPBaseAttributeSet::GetHealthAttribute(),
		ULNPBaseAttributeSet::GetMaxHealthAttribute(),
		ULNPBaseAttributeSet::GetAttackPowerAttribute(),
		ULNPBaseAttributeSet::GetAttackMultiplierAttribute(),
		ULNPBaseAttributeSet::GetAttackSpeedAttribute(),
		ULNPBaseAttributeSet::GetDefensePowerAttribute(),
		ULNPBaseAttributeSet::GetMoveSpeedAttribute(),
		ULNPBaseAttributeSet::GetLootSpeedAttribute(),
	};
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

			// AddBase(구 Additive)만 "기본 스탯에 합산된 것처럼" 취급한다 — 기획 §5-1.
			if (Modifiers[i].ModifierOp == EGameplayModOp::AddBase)
				Result += Effect.Spec.GetModifierMagnitude(i);
		}
	}

	return Result;
}

FString ULNPStatsViewModel::MakeStatLine(const FString& Label, float FinalValue, float AdditiveResult, bool bIsInteger)
{
	const int32 Decimals = bIsInteger ? 0 : 2;
	const float MultiplicativeBonus = FinalValue - AdditiveResult;

	FNumberFormattingOptions Format;
	Format.MinimumFractionalDigits = Decimals;
	Format.MaximumFractionalDigits = Decimals;

	const FString FinalStr  = FText::AsNumber(FinalValue, &Format).ToString();
	const FString BaseStr   = FText::AsNumber(AdditiveResult, &Format).ToString();
	const FString BonusStr  = FText::AsNumber(MultiplicativeBonus, &Format).ToString();

	const FString PaddedLabel = EscapeForRichText(Label).RightPad(GLabelColumnWidth);

	// "라벨          최종값 (합연산결과 + 곱연산증가량)"
	return FString::Printf(
		TEXT("<sub>%s</><final>%s</><sub> (%s + </><buff>%s</><sub>)</>"),
		*PaddedLabel, *FinalStr, *BaseStr, *BonusStr);
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

	// HP는 현재/최대 쌍이라 합·곱 분해 포맷을 쓰지 않는다.
	{
		FNumberFormattingOptions IntFormat;
		IntFormat.MinimumFractionalDigits = 0;
		IntFormat.MaximumFractionalDigits = 0;

		const FString Health    = FText::AsNumber(ASC->GetNumericAttribute(ULNPBaseAttributeSet::GetHealthAttribute()), &IntFormat).ToString();
		const FString MaxHealth = FText::AsNumber(ASC->GetNumericAttribute(ULNPBaseAttributeSet::GetMaxHealthAttribute()), &IntFormat).ToString();

		Lines.Add(FString::Printf(TEXT("<sub>%s</><final>%s / %s</>"),
			*FString(TEXT("HP")).RightPad(GLabelColumnWidth), *Health, *MaxHealth));
	}

	// 공격력 — AttackPower와 AttackMultiplier를 한 행으로 합친다 (기획 §5-1).
	// 데미지 공식 (AttackPower + 무기보너스) × AttackMultiplier와 같은 관계이며,
	// 무기 보너스는 어빌리티가 장착 무기에서 읽는 지역 값이라 캐릭터 스탯에는 넣지 않는다.
	{
		const float AttackPower      = ASC->GetNumericAttribute(ULNPBaseAttributeSet::GetAttackPowerAttribute());
		const float AttackMultiplier = ASC->GetNumericAttribute(ULNPBaseAttributeSet::GetAttackMultiplierAttribute());
		const float AdditiveResult   = GetAdditiveResult(ASC, ULNPBaseAttributeSet::GetAttackPowerAttribute());

		Lines.Add(MakeStatLine(TEXT("Attack"), AttackPower * AttackMultiplier, AdditiveResult, /*bIsInteger=*/false));
	}

	struct FStatRow
	{
		const TCHAR*        Label;
		FGameplayAttribute  Attribute;
	};

	const TArray<FStatRow> Rows = {
		{ TEXT("Attack Speed"),  ULNPBaseAttributeSet::GetAttackSpeedAttribute()  },
		{ TEXT("Defense"),       ULNPBaseAttributeSet::GetDefensePowerAttribute() },
		{ TEXT("Move Speed"),    ULNPBaseAttributeSet::GetMoveSpeedAttribute()    },
		{ TEXT("Loot Speed"),    ULNPBaseAttributeSet::GetLootSpeedAttribute()    },
	};

	for (const FStatRow& Row : Rows)
	{
		Lines.Add(MakeStatLine(Row.Label,
			ASC->GetNumericAttribute(Row.Attribute),
			GetAdditiveResult(ASC, Row.Attribute),
			/*bIsInteger=*/false));
	}

	SetStatsRichText(FText::FromString(FString::Join(Lines, TEXT("\n"))));
}

void ULNPStatsViewModel::SetStatsRichText(const FText& InValue)
{
	UE_MVVM_SET_PROPERTY_VALUE(StatsRichText, InValue);
}
