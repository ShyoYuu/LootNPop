// Copyright LootNPop. All Rights Reserved.

#include "Movement/LNPMoveSpeedModifier.h"
#include "Movement/LNPCharacterMoverComponent.h"
#include "Movement/LNPCharacterMovementSettings.h"
#include "GAS/Attributes/LNPBaseAttributeSet.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "MoverComponent.h"
#include "DefaultMovementSet/Settings/CommonLegacyMovementSettings.h"
#include "MoveLibrary/MovementUtils.h"

FLNPMoveSpeedModifier::FLNPMoveSpeedModifier()
{
	DurationMs = -1.0f; // 캐릭터 수명 동안 상시 활성
}

void FLNPMoveSpeedModifier::OnPreMovement(UMoverComponent* MoverComp, const FMoverTimeStep& TimeStep)
{
	AActor* Owner = MoverComp->GetOwner();
	if (Owner == nullptr)
		return;

	// 기준값은 항상 CDO에서 읽는다 — 라이브 설정은 Sprint/Guard가 이미 덮어썼을 수 있어
	// 그걸 기준으로 곱하면 배율이 매 틱 누적된다.
	const UMoverComponent* CDOMoverComp = UMovementUtils::GetOriginalComponentType<UMoverComponent>(Owner);
	if (CDOMoverComp == nullptr)
		return;

	const UCommonLegacyMovementSettings* OriginalCommon = CDOMoverComp->FindSharedSettings<UCommonLegacyMovementSettings>();
	const ULNPCharacterMovementSettings* OriginalLNP    = CDOMoverComp->FindSharedSettings<ULNPCharacterMovementSettings>();
	UCommonLegacyMovementSettings*       CurrentCommon  = MoverComp->FindSharedSettings_Mutable<UCommonLegacyMovementSettings>();
	if (OriginalCommon == nullptr || OriginalLNP == nullptr || CurrentCommon == nullptr)
		return;

	// 현재 이동 상태가 요구하는 기준 속도 — Sprint/Guard의 OnStart와 같은 출처를 쓴다.
	float BaseSpeed = OriginalCommon->MaxSpeed;
	if (MoverComp->HasGameplayTag(LNP_Mover_IsSprinting, /*bExactMatch=*/true))
		BaseSpeed = OriginalLNP->SprintSpeed;
	else if (MoverComp->HasGameplayTag(LNP_Mover_IsGuarding, /*bExactMatch=*/true))
		BaseSpeed = OriginalLNP->GuardWalkSpeed;
	else if (MoverComp->HasGameplayTag(LNP_Mover_IsADS, /*bExactMatch=*/true))
		BaseSpeed = OriginalLNP->ADSWalkSpeed;

	float Multiplier = 1.0f;
	if (const IAbilitySystemInterface* ASCInterface = Cast<IAbilitySystemInterface>(Owner))
	{
		if (const UAbilitySystemComponent* ASC = ASCInterface->GetAbilitySystemComponent())
			Multiplier = FMath::Max(0.01f, ASC->GetNumericAttribute(ULNPBaseAttributeSet::GetMoveSpeedAttribute()));
	}

	CurrentCommon->MaxSpeed = BaseSpeed * Multiplier;
}

FMovementModifierBase* FLNPMoveSpeedModifier::Clone() const
{
	return new FLNPMoveSpeedModifier(*this);
}

void FLNPMoveSpeedModifier::NetSerialize(FArchive& Ar)
{
	Super::NetSerialize(Ar);
	// 자체 페이로드 없음 — 배율은 ASC 어트리뷰트에서 매 틱 읽는다.
}

UScriptStruct* FLNPMoveSpeedModifier::GetScriptStruct() const
{
	return FLNPMoveSpeedModifier::StaticStruct();
}

FString FLNPMoveSpeedModifier::ToSimpleString() const
{
	return FString::Printf(TEXT("LNPMoveSpeed"));
}
