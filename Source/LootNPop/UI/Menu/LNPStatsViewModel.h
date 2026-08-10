// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"          // FGameplayAttribute (값 타입으로 사용)
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
 * 마크업 태그 3종 — 스타일 셋에 같은 이름의 행이 있어야 한다:
 *  - <final> 최종값 (흰색)
 *  - <sub>   합연산결과와 괄호·구분자 (회색)
 *  - <buff>  곱연산 증가량 (초록)
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
	 * ⚠️ 한계: 조건부 모디파이어의 태그 평가 파라미터까지는 재현하지 않고 bIsInhibited만 거른다.
	 * 버프 시스템을 개선할 때 이 함수 내부만 교체하면 되도록 격리해 두었다.
	 */
	static float GetAdditiveResult(const UAbilitySystemComponent* ASC, const FGameplayAttribute& Attribute);

private:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "LNP|Menu", meta = (AllowPrivateAccess = "true"))
	FText StatsRichText;

	void SetStatsRichText(const FText& InValue);

	/** 구독하는 어트리뷰트 목록. 스탯을 추가하면 여기와 BuildStatsRichText를 함께 고친다. */
	static TArray<FGameplayAttribute> GetObservedAttributes();

	/** 리드아웃 전체를 다시 만든다. */
	void RebuildStatsRichText();

	void OnObservedAttributeChanged(const FOnAttributeChangeData& Data);

	/**
	 * 한 행을 만든다: "라벨   최종값 (합연산결과 + 곱연산증가량)"
	 * @param bIsInteger true면 소수점 없이 출력한다.
	 */
	static FString MakeStatLine(const FString& Label, float FinalValue, float AdditiveResult, bool bIsInteger);

	TWeakObjectPtr<UAbilitySystemComponent> BoundASC;

	/** GetObservedAttributes()와 같은 순서의 구독 핸들 — 해제 시 인덱스로 짝을 맞춘다. */
	TArray<FDelegateHandle> AttributeHandles;
};
