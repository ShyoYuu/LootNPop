// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"          // FGameplayAttribute (값 타입으로 사용)
#include "GAS/LNPStatModifier.h"   // ELNPStatDisplay
#include "MVVMViewModelBase.h"
#include "LNPStatsViewModel.generated.h"

class UAbilitySystemComponent;
struct FOnAttributeChangeData;

/**
 * 캐릭터 스탯 탭용 MVVM ViewModel.
 *
 * 스탯 리드아웃 전체를 RichText 마크업 문자열 하나(StatsRichText)로 만들어 노출한다.
 * 스탯마다 FieldNotify 프로퍼티를 두면 6행 × 3값 = 18개가 되므로, 색상은 Rich Text
 * Style Set(DT_LNPMenuTextStyles)에 맡기고 ViewModel은 필드 하나만 갱신한다.
 *
 * Blueprint 바인딩: CommonRichTextBlock.Text ← Stats_ViewModel.StatsRichText (One Way)
 *
 * 표기는 `C (A × B)` — A는 기초 스탯 총량(캐릭터 기본 + 무기 스텟 + 합연산 버프),
 * B는 곱연산 버프의 총 배율(퍼센트). "+40%" 버프 하나를 얻으면 B가 100% → 140%로 움직여
 * 아이템 표기와 1:1로 대응된다.
 *
 * 마크업 태그 3종 — 스타일 셋에 같은 이름의 행이 있어야 한다:
 *  - <final> 최종값 C (흰색)
 *  - <sub>   합연산결과 A와 괄호·구분자 (회색)
 *  - <buff>  곱연산 배율 B (초록)
 * ⚠️ 열 정렬을 공백 패딩으로 하므로 세 스타일 모두 모노스페이스 폰트여야 한다.
 */
UCLASS(BlueprintType)
class LOOTNPOP_API ULNPStatsViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	/** ASC의 스탯 변경을 구독하고 초기 리드아웃을 만든다. 재호출 시 기존 구독을 먼저 해제한다. */
	void Initialize(UAbilitySystemComponent* InASC);

	/** 구독을 모두 해제한다. */
	void Deinitialize();

	/**
	 * 어트리뷰트의 "합연산결과" = BaseValue + 활성 GameplayEffect의 AddBase 모디파이어 합.
	 *
	 * UE 5.8의 어그리게이터 평가식이
	 *   ((Base + AddBase) * MultiplyAdditive / DivideAdditive * MultiplyCompound) + AddFinal
	 * 이므로(GameplayEffectAggregator.cpp), 이 값이 곧 "곱연산이 적용되기 직전의 값"이다.
	 *
	 * 무기 스텟도 장착 GE의 AddBase로 들어오므로 이 값에 자동으로 포함된다.
	 *
	 * ⚠️ 한계: 조건부 모디파이어의 태그 평가 파라미터까지는 재현하지 않고 bIsInhibited만 거른다.
	 */
	static float GetAdditiveResult(const UAbilitySystemComponent* ASC, const FGameplayAttribute& Attribute);

private:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "LNP|Menu", meta = (AllowPrivateAccess = "true"))
	FText StatsRichText;

	void SetStatsRichText(const FText& InValue);

	/** 구독하는 어트리뷰트 목록 — LNPStat::GetStatMetaTable()의 스탯 전체 + Health. */
	static TArray<FGameplayAttribute> GetObservedAttributes();

	/** 리드아웃 전체를 다시 만든다. */
	void RebuildStatsRichText();

	void OnObservedAttributeChanged(const FOnAttributeChangeData& Data);

	/** 한 행을 만든다: "라벨   C (A × B)" */
	static FString MakeStatLine(const FText& Label, float FinalValue, float AdditiveResult, ELNPStatDisplay Display);

	TWeakObjectPtr<UAbilitySystemComponent> BoundASC;

	/** GetObservedAttributes()와 같은 순서의 구독 핸들 — 해제 시 인덱스로 짝을 맞춘다. */
	TArray<FDelegateHandle> AttributeHandles;
};
