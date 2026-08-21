// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Enemy/LNPEnemyCharacter.h"
#include "Enemy/LNPEnemyConfig.h"
#include "Enemy/LNPEnemyMassTypes.h"
#include "GAS/Abilities/LNPGameplayAbility.h"
#include "Character/LNPInputHandlerComponent.h"
#include "Movement/LNPCharacterMoverComponent.h"
#include "Gravity/LNPPawnGravityComponent.h"
#include "GAS/Attributes/LNPBaseAttributeSet.h"
#include "GAS/LNPStatModifier.h"
#include "UI/LNPHpBarWidget.h"

#include "AbilitySystemComponent.h"
#include "Net/UnrealNetwork.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/WidgetComponent.h"
#include "Blueprint/UserWidget.h"

ALNPEnemyCharacter::ALNPEnemyCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetNetUpdateFrequency(30.f);

	ASC = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("ASC"));
	ASC->SetIsReplicated(true);
	ASC->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	AttributeSet = CreateDefaultSubobject<ULNPBaseAttributeSet>(TEXT("AttributeSet"));

	HpBarComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("HpBarComponent"));
	HpBarComponent->SetupAttachment(RootComponent);
	HpBarComponent->SetVisibility(false);

	LockOnMarkerComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("LockOnMarkerComponent"));
	LockOnMarkerComponent->SetupAttachment(RootComponent);
	LockOnMarkerComponent->SetVisibility(false);
}

UAbilitySystemComponent* ALNPEnemyCharacter::GetAbilitySystemComponent() const
{
	return ASC;
}

void ALNPEnemyCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (ASC)
	{
		ASC->InitAbilityActorInfo(this, this);

		if (InputHandlerComponent)
			InputHandlerComponent->CacheASC(ASC);

		if (HpBarWidgetClass)
		{
			HpBarComponent->SetWidgetClass(HpBarWidgetClass);
			ASC->GetGameplayAttributeValueChangeDelegate(ULNPBaseAttributeSet::GetHealthAttribute())
				.AddUObject(this, &ALNPEnemyCharacter::OnHpAttributeChanged);
		}
	}

	if (LockOnMarkerWidgetClass)
	{
		LockOnMarkerComponent->SetWidgetClass(LockOnMarkerWidgetClass);
	}
}

void ALNPEnemyCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (HpBarComponent->IsVisible() && HpBarComponent->GetWidgetSpace() == EWidgetSpace::World)
	{
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			FVector CameraLoc;
			FRotator CameraRot;
			PC->GetPlayerViewPoint(CameraLoc, CameraRot);
			const FVector ToCamera = (CameraLoc - HpBarComponent->GetComponentLocation()).GetSafeNormal();
			HpBarComponent->SetWorldRotation(ToCamera.Rotation());
		}
	}
}

void ALNPEnemyCharacter::InitializeOnce(ULNPEnemyConfig* InConfig)
{
	if (nullptr == InConfig)
		return;
	if (bInitializedOnce && EnemyConfig == InConfig)
		return;

	if (bInitializedOnce && ASC)
	{
		ASC->ClearAllAbilities();
		WeaponAbilityHandle = FGameplayAbilitySpecHandle();

		// 무기 스텟 GE도 함께 되돌린다 — 안 그러면 LOD 전환마다 스텟이 누적된다.
		for (const FActiveGameplayEffectHandle& Handle : WeaponStatEffects)
			ASC->RemoveActiveGameplayEffect(Handle);
		WeaponStatEffects.Reset();
	}

	bInitializedOnce = true;
	EnemyConfig = InConfig;

	if (UAbilitySystemComponent* EnemyASC = GetAbilitySystemComponent())
	{
		if (InConfig->WeaponData)
		{
			for (const TSubclassOf<ULNPGameplayAbility>& AbilityClass : InConfig->WeaponData->AbilitiesToGrant)
			{
				if (AbilityClass)
				{
					FGameplayAbilitySpecHandle Handle = EnemyASC->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1, INDEX_NONE, this));
					if (!WeaponAbilityHandle.IsValid())
						WeaponAbilityHandle = Handle;
				}
			}
			// 적은 EquipmentComponent를 거치지 않으므로 무기 스텟을 여기서 직접 적용한다.
			// 무기 스텟의 원본은 이제 레벨 테이블이다. 적은 레벨 개념이 없으므로 1레벨 행을 쓴다
			// (테이블이 없는 무기는 베이스 StatModifiers로 폴백된다).
			LNPStat::ApplyModifiers(*EnemyASC, InConfig->WeaponData->GetStatModifiersForLevel(1), WeaponStatEffects);

			// 서버 로컬 비주얼. 클라이언트는 EnemyConfig 복제 → OnRep_EnemyConfig가 같은 일을 한다.
			ApplyWeaponVisuals(InConfig->WeaponData);
		}

		for (const TSubclassOf<UGameplayAbility>& AbilityClass : InConfig->DefaultAbilities)
		{
			if (AbilityClass)
				EnemyASC->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1, INDEX_NONE, this));
		}
	}
}

void ALNPEnemyCharacter::SyncFromEntity(float InHealth, ELNPTargetingState InTargetingState, FVector InVelocity)
{
	if (AnimSourceMesh)
		AnimSourceMesh->SetVisibility(false);

	if (InputHandlerComponent)
	{
		InputHandlerComponent->SetAIMoveInput(FVector::ZeroVector);
		InputHandlerComponent->SetAIOrientationIntent(FVector::ZeroVector);
	}

	if (AttributeSet)
		AttributeSet->SetHealth(InHealth);

	if (MoverComponent)
		MoverComponent->LaunchWithVelocity(InVelocity);

	RefreshHpBar(InHealth, AttributeSet ? AttributeSet->GetMaxHealth() : 0.f);
}

void ALNPEnemyCharacter::TriggerRagdoll()
{
	if (UAnimInstance* AnimInst = GetAnimInstance())
		AnimInst->Montage_Stop(0.3f);
	if (AnimSourceMesh)
		AnimSourceMesh->SetActive(false);
	if (VisualMesh)
	{
		VisualMesh->SetAllBodiesSimulatePhysics(true);
		VisualMesh->SetCollisionProfileName(TEXT("Ragdoll"));
		VisualMesh->WakeAllRigidBodies();
	}
	if (CapsuleComponent)
		CapsuleComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (MoverComponent)
		MoverComponent->ApplyKnockback(MoverComponent->GetUpDirection(), 10000.f);
}

void ALNPEnemyCharacter::SetLockOnMarkerVisible(bool bVisible)
{
	if (LockOnMarkerComponent)
		LockOnMarkerComponent->SetVisibility(bVisible);
}

bool ALNPEnemyCharacter::TryActivateAttack_Impl()
{
	if (!WeaponAbilityHandle.IsValid() || !ASC)
		return false;

	return ASC->TryActivateAbility(WeaponAbilityHandle);
}

void ALNPEnemyCharacter::CancelCurrentAttackAbility()
{
	if (!WeaponAbilityHandle.IsValid() || !ASC)
		return;

	ASC->CancelAbilityHandle(WeaponAbilityHandle);
}

const ULNPWeaponData* ALNPEnemyCharacter::GetActiveWeaponDef() const
{
	return ResolveWeaponDefForVisuals();
}

ULNPWeaponData* ALNPEnemyCharacter::ResolveWeaponDefForVisuals() const
{
	return EnemyConfig ? EnemyConfig->WeaponData.Get() : nullptr;
}

void ALNPEnemyCharacter::OnRep_EnemyConfig()
{
	ApplyWeaponVisuals(ResolveWeaponDefForVisuals());
}

void ALNPEnemyCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ALNPEnemyCharacter, EnemyConfig);
}

void ALNPEnemyCharacter::SyncToEntity(float& OutHealth, FVector& OutVelocity) const
{
	OutHealth = AttributeSet ? AttributeSet->GetHealth() : 0.f;
	OutVelocity = (MoverComponent && MoverComponent->IsAirborne())
		? MoverComponent->GetVelocity()
		: FVector::ZeroVector;
}

void ALNPEnemyCharacter::OnHpAttributeChanged(const FOnAttributeChangeData& Data)
{
	RefreshHpBar(Data.NewValue, AttributeSet ? AttributeSet->GetMaxHealth() : 0.f);
}

void ALNPEnemyCharacter::RefreshHpBar(float Current, float Max)
{
	const bool bShouldShow = Current > 0.f && Max > 0.f && Current < Max;
	HpBarComponent->SetVisibility(bShouldShow);

	if (bShouldShow)
	{
		if (auto* Widget = Cast<ULNPHpBarWidget>(HpBarComponent->GetWidget()))
			Widget->UpdateHpBar(Current, Max);
	}
}
