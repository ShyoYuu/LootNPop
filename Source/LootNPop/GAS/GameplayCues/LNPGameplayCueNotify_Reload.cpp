// Copyright (c) 2026 LootNPop. All rights reserved.

#include "GAS/GameplayCues/LNPGameplayCueNotify_Reload.h"
#include "Character/LNPCharacterBase.h"
#include "GAS/Attributes/LNPBaseAttributeSet.h"
#include "Item/LNPWeaponData.h"
#include "Item/LNPWeaponVisualSet.h"

#include "AbilitySystemComponent.h"
#include "Animation/AnimSequence.h"

bool ULNPGameplayCueNotify_Reload::OnActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	const ALNPCharacterBase* Character = Cast<ALNPCharacterBase>(MyTarget);
	const ULNPWeaponData* WeaponDef = Character ? Character->GetActiveWeaponDef() : nullptr;
	UAnimSequence* ReloadAnim = (WeaponDef && WeaponDef->VisualSet) ? WeaponDef->VisualSet->WeaponReloadAnim.Get() : nullptr;
	if (ReloadAnim == nullptr)
		return false;

	// 재장전 어빌리티와 같은 식으로 시간을 구한다 — 캐릭터 몽타주와 박자가 맞아야 한다.
	float AttackSpeed = 1.f;
	if (const UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent())
		AttackSpeed = ASC->GetNumericAttribute(ULNPBaseAttributeSet::GetAttackSpeedAttribute());
	const float Duration = FMath::Max(0.05f, WeaponDef->GetReloadDuration(AttackSpeed));

	Character->PlayWeaponMeshAnimation(ReloadAnim, ReloadAnim->GetPlayLength() / Duration);
	return true;
}

bool ULNPGameplayCueNotify_Reload::OnRemove_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	// 취소(대시·교체·경직)로 중간에 떼어져도 탄창이 빠진 포즈로 굳지 않게 되돌린다.
	if (const ALNPCharacterBase* Character = Cast<ALNPCharacterBase>(MyTarget))
		Character->StopWeaponMeshAnimation();
	return true;
}
