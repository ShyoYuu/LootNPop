// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Animation/ANS_LNPMeleeHitWindow.h"
#include "Character/LNPCharacterBase.h"
#include "Item/LNPWeaponData.h"
#include "GAS/Attributes/LNPBaseAttributeSet.h"
#include "HitDetection/LNPWeaponTraceMassTypes.h"
#include "Enemy/LNPEnemyCharacter.h"

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

	bEntityActive = false;
	MeleeEntity   = FMassEntityHandle();

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

	// --- 피해량 계산 (AttackPower + WeaponBonus) * AttackMultiplier ---
	float Damage = WeaponDef->ProjectileDamage;
	if (const UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent())
	{
		if (const ULNPBaseAttributeSet* Attrs = ASC->GetSet<ULNPBaseAttributeSet>())
		{
			Damage = (Attrs->GetAttackPower() + WeaponDef->ProjectileDamage) * Attrs->GetAttackMultiplier();
		}
	}

	// --- 칼날 초기 위치 ---
	const FVector TipPos  = WeaponMesh->GetBoneLocation(BoneTipName);
	const FVector RootPos = WeaponMesh->GetBoneLocation(BoneRootName);

	// --- Fragment 초기값 ---
	FLNPWeaponTraceFragment MeleeData;
	MeleeData.SwordTipPrev  = MeleeData.SwordTipCurr  = TipPos;
	MeleeData.SwordRootPrev = MeleeData.SwordRootCurr = RootPos;
	MeleeData.HitRadius         = HitRadiusOverride > 0.f ? HitRadiusOverride : WeaponDef->ProjectileHitRadius;
	MeleeData.Damage            = Damage;
	MeleeData.DamageEffectClass = WeaponDef->ProjectileDamageEffect;
	MeleeData.InstigatorTeam    = Cast<ALNPEnemyCharacter>(Character) ? ELNPInstigatorTeam::Enemy : ELNPInstigatorTeam::Player;

	if (const UMassAgentComponent* AgentComp = Character->FindComponentByClass<UMassAgentComponent>())
		MeleeData.InstigatorEntity = AgentComp->GetEntityHandle();

	FTransformFragment TransFrag;
	TransFrag.GetMutableTransform().SetLocation(Character->GetActorLocation());

	// --- 엔티티 생성 (Deferred) ---
	// Fragment 존재 자체가 활성 공격 윈도우를 의미한다. Tag는 사용하지 않는다.
	// (BuildEntity + AddTag를 동일 배치에서 디퍼드하면 아키타입 전환 타이밍으로 쿼리가 엔티티를 못 찾는 문제 있음)
	MeleeEntity = EntityManager->ReserveEntity();
	EntityManager->Defer().PushCommand<FMassCommandBuildEntity<FLNPWeaponTraceFragment, FTransformFragment>>(
		MeleeEntity, MeleeData, TransFrag);
}

void UANS_LNPMeleeHitWindow::NotifyTick(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation, float FrameDeltaTime,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);

	if (!MeshComp || !MeleeEntity.IsValid())
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

	// Deferred BuildEntity 커맨드는 다음 Mass 처리 단계 시작 시 플러시된다.
	// 첫 Tick에서 아직 엔티티가 활성화되지 않았을 수 있으므로 활성 확인 후 진행.
	if (!bEntityActive)
	{
		bEntityActive = EntityManager->IsEntityActive(MeleeEntity);
		if (!bEntityActive)
			return;
	}

	if (FLNPWeaponTraceFragment* Frag = EntityManager->GetFragmentDataPtr<FLNPWeaponTraceFragment>(MeleeEntity))
	{
		// 이전 프레임 위치로 교체한 뒤 현재 본 위치를 기록 → Swept Volume 선분 완성
		Frag->SwordTipPrev = Frag->SwordTipCurr;
		Frag->SwordRootPrev = Frag->SwordRootCurr;
		Frag->SwordTipCurr = WeaponMesh->GetBoneLocation(BoneTipName);
		Frag->SwordRootCurr = WeaponMesh->GetBoneLocation(BoneRootName);
	}
}

void UANS_LNPMeleeHitWindow::NotifyEnd(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (!MeleeEntity.IsValid())
		return;

	ALNPCharacterBase* Character = MeshComp ? Cast<ALNPCharacterBase>(MeshComp->GetOwner()) : nullptr;
	FMassEntityManager* EntityManager = GetEntityManager(Character ? Character->GetWorld() : nullptr);
	if (!EntityManager)
		return;

	EntityManager->Defer().PushCommand<FMassCommandDestroyEntities>(
		TConstArrayView<FMassEntityHandle>(&MeleeEntity, 1));

	MeleeEntity   = FMassEntityHandle();
	bEntityActive = false;
}
