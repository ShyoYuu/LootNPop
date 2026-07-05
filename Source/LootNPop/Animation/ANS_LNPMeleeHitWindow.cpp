// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Animation/ANS_LNPMeleeHitWindow.h"
#include "Character/LNPCharacterBase.h"
#include "Item/LNPWeaponData.h"
#include "GAS/Attributes/LNPBaseAttributeSet.h"
#include "GAS/Abilities/LNPAbility_BasicAttack.h"
#include "HitDetection/LNPWeaponTraceMassTypes.h"
#include "LNPGameplayTags.h"
#include "Enemy/LNPEnemyCharacter.h"
#include "LootNPop.h"

#include "MassEntitySubsystem.h"
#include "MassEntityManager.h"
#include "MassAgentComponent.h"
#include "MassCommonFragments.h"
#include "MassCommands.h"
#include "AbilitySystemComponent.h"
#include "Components/SkeletalMeshComponent.h"

// ──────────────────────────────────────────────────────────────────────────────
// 내부 헬퍼
// ──────────────────────────────────────────────────────────────────────────────

static FMassEntityManager* GetEntityManager(UWorld* World)
{
	UMassEntitySubsystem* Sub = World ? World->GetSubsystem<UMassEntitySubsystem>() : nullptr;
	return Sub ? &Sub->GetMutableEntityManager() : nullptr;
}

// ──────────────────────────────────────────────────────────────────────────────

void UANS_LNPMeleeHitWindow::NotifyBegin(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation, float TotalDuration,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	// 이 노티파이 오브젝트는 몽타주를 재생하는 모든 캐릭터(서버·클라이언트 포함)가 공유하므로
	// MeshComp를 키로 스윙별 상태를 분리한다.
	ActiveSwings.Remove(MeshComp);

	ALNPCharacterBase* Character = MeshComp ? Cast<ALNPCharacterBase>(MeshComp->GetOwner()) : nullptr;
	if (!Character)
		return;

	const ULNPWeaponData* WeaponDef = Character->GetActiveWeaponDef();
	if (!WeaponDef)
		return;

	const USkeletalMeshComponent* WeaponMesh = Character->GetWeaponMesh();
	if (!WeaponMesh)
		return;

	FMassEntityManager* EntityManager = GetEntityManager(Character->GetWorld());
	if (!EntityManager)
		return;

	// --- 칼날 초기 위치 ---
	const FVector TipPos  = WeaponMesh->GetBoneLocation(BoneTipName);
	const FVector RootPos = WeaponMesh->GetBoneLocation(BoneRootName);

	// --- Fragment 초기값 ---
	FLNPWeaponTraceFragment MeleeData;
	MeleeData.SwordTipPrev      = MeleeData.SwordTipCurr  = TipPos;
	MeleeData.SwordRootPrev     = MeleeData.SwordRootCurr = RootPos;
	MeleeData.HitRadius         = 0.f < HitRadiusOverride ? HitRadiusOverride : WeaponDef->HitRadius;
	MeleeData.DamageEffectClass = WeaponDef->ProjectileDamageEffect;
	MeleeData.InstigatorTeam    = Cast<ALNPEnemyCharacter>(Character) ? ELNPInstigatorTeam::Enemy : ELNPInstigatorTeam::Player;
	MeleeData.TimeToLive        = TotalDuration + 0.2f;
	// 서버·클라이언트 각자 독립적으로 엔티티를 생성한다 (HasAuthority 분기 없음).
	// 로컬 컨트롤 공격자에서만 클라이언트 예측 HitStop을 수행하도록 표시한다.
	MeleeData.bIsLocalInstigator = Character->IsLocallyControlled();

	// 넉백 강도와 패링 반경을 태그별 독립 조회로 읽는다.
	// 각 태그가 비어있으면 기본 태그(TAG_Ability_HitEffect_*)로 자동 선택한다.
	if (const UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent())
	{
		const int32 ComboIdx = Character->GetCurrentComboIndex();
		const FGameplayTag KnockTag = KnockbackAbilityTag.IsValid() ? KnockbackAbilityTag : TAG_Ability_HitEffect_Knockback;
		const FGameplayTag ParryTag = ParryAbilityTag.IsValid()     ? ParryAbilityTag     : TAG_Ability_HitEffect_Parry;

		for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
		{
			if (!Spec.IsActive() || !Spec.Ability)
				continue;

			const ULNPAbility_BasicAttack* Ability = Cast<ULNPAbility_BasicAttack>(Spec.GetPrimaryInstance());
			if (!Ability)
				continue;

			MeleeData.Damage = Ability->GetAbilityDamage();

			const FGameplayTagContainer& Tags = Spec.Ability->GetAssetTags();
			if (Tags.HasTag(KnockTag))
				MeleeData.KnockbackStrength = Ability->GetKnockbackForCombo(ComboIdx);
			if (Tags.HasTag(ParryTag))
				MeleeData.ParryRadius = Ability->GetParryRadius();
		}
	}

	// 서버 Pass 3(워커 스레드)용 — Mass 엔티티 핸들.
	if (const UMassAgentComponent* AgentComp = Character->FindComponentByClass<UMassAgentComponent>())
		MeleeData.InstigatorEntity = AgentComp->GetEntityHandle();

	// 클라이언트 예측 판정용 — 액터 직접 참조 (Mass 핸들 왕복 없이 바로 사용, 위 필드 주석 참조).
	MeleeData.InstigatorActor = Character;

	FTransformFragment TransFrag;
	TransFrag.GetMutableTransform().SetLocation(Character->GetActorLocation());

	// --- 엔티티 생성 (Deferred) ---
	// Fragment 존재 자체가 활성 공격 윈도우를 의미한다. Tag는 사용하지 않는다.
	// (BuildEntity + AddTag를 동일 배치에서 디퍼드하면 아키타입 전환 타이밍으로 쿼리가 엔티티를 못 찾는 문제 있음)
	FActiveSwing& Swing = ActiveSwings.Add(MeshComp);
	Swing.Entity = EntityManager->ReserveEntity();
	EntityManager->Defer().PushCommand<FMassCommandBuildEntity<FLNPWeaponTraceFragment, FTransformFragment>>(
		Swing.Entity, MeleeData, TransFrag);
}

void UANS_LNPMeleeHitWindow::NotifyTick(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation, float FrameDeltaTime,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);

	if (!MeshComp)
		return;

	FActiveSwing* Swing = ActiveSwings.Find(MeshComp);
	if (!Swing || !Swing->Entity.IsValid())
		return;

	ALNPCharacterBase* Character = Cast<ALNPCharacterBase>(MeshComp->GetOwner());
	if (!Character)
		return;

	const USkeletalMeshComponent* WeaponMesh = Character->GetWeaponMesh();
	if (!WeaponMesh)
		return;

	FMassEntityManager* EntityManager = GetEntityManager(Character->GetWorld());
	if (!EntityManager)
		return;

	if (EntityManager->IsEntityActive(Swing->Entity))
	{
		if (FLNPWeaponTraceFragment* Frag = EntityManager->GetFragmentDataPtr<FLNPWeaponTraceFragment>(Swing->Entity))
		{
			// 이전 프레임 위치로 교체한 뒤 현재 본 위치를 기록 → Swept Volume 선분 완성
			Frag->SwordTipPrev = Frag->SwordTipCurr;
			Frag->SwordRootPrev = Frag->SwordRootCurr;
			Frag->SwordTipCurr = WeaponMesh->GetBoneLocation(BoneTipName);
			Frag->SwordRootCurr = WeaponMesh->GetBoneLocation(BoneRootName);
		}
	}
}

void UANS_LNPMeleeHitWindow::NotifyEnd(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (!MeshComp)
		return;

	FActiveSwing Swing;
	if (!ActiveSwings.RemoveAndCopyValue(MeshComp, Swing) || !Swing.Entity.IsValid())
		return;

	ALNPCharacterBase* Character = Cast<ALNPCharacterBase>(MeshComp->GetOwner());
	FMassEntityManager* EntityManager = GetEntityManager(Character ? Character->GetWorld() : nullptr);
	if (!EntityManager)
		return;

	EntityManager->Defer().PushCommand<FMassCommandDestroyEntities>(
		TConstArrayView<FMassEntityHandle>(&Swing.Entity, 1));
}
