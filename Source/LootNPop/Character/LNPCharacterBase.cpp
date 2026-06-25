// Copyright Epic Games, Inc. All Rights Reserved.

#include "LNPCharacterBase.h"
#include "Movement/LNPCharacterMoverComponent.h"
#include "Character/LNPInputHandlerComponent.h"
#include "Gravity/LNPPawnGravityComponent.h"
#include "Player/LNPPlayerState.h"
#include "Item/LNPWeaponData.h"
#include "Animation/LNPMontageChooserContext.h"
#include "LNPGameplayTags.h"
#include "LootNPop.h"

#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "AbilitySystemComponent.h"
#include "MassAgentComponent.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "ChooserFunctionLibrary.h"


ALNPCharacterBase::ALNPCharacterBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;

	SetReplicatingMovement(false);

	CapsuleComponent = CreateDefaultSubobject<UCapsuleComponent>(TEXT("CapsuleComponent"));
	CapsuleComponent->InitCapsuleSize(42.f, 96.0f);
	SetRootComponent(CapsuleComponent);

	AnimSourceMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("AnimSourceMesh"));
	AnimSourceMesh->SetupAttachment(CapsuleComponent);
	AnimSourceMesh->SetVisibility(false);
	VisualMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("VisualMesh"));
	VisualMesh->SetupAttachment(AnimSourceMesh);

	MassAgentComponent = CreateDefaultSubobject<UMassAgentComponent>(TEXT("MassAgentComponent"));

	MoverComponent = CreateDefaultSubobject<ULNPCharacterMoverComponent>(TEXT("MoverComponent"));
	InputHandlerComponent = CreateDefaultSubobject<ULNPInputHandlerComponent>(TEXT("InputHandlerComponent"));
	GravityComponent = CreateDefaultSubobject<ULNPPawnGravityComponent>(TEXT("GravityComponent"));

	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
	WeaponMesh->SetupAttachment(VisualMesh);
	WeaponMesh->SetVisibility(false);

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;
}

bool ALNPCharacterBase::GetFaceMoveDirection() const
{
	return InputHandlerComponent ? InputHandlerComponent->GetFaceMoveDirection() : false;
}

UAbilitySystemComponent* ALNPCharacterBase::GetAbilitySystemComponent() const
{
	if (const ALNPPlayerState* PS = GetPlayerState<ALNPPlayerState>())
		return PS->GetAbilitySystemComponent();
	return nullptr;
}

void ALNPCharacterBase::SetAIMoveInput(FVector InMoveInput)
{
	if (InputHandlerComponent)
		InputHandlerComponent->SetAIMoveInput(InMoveInput);
}

void ALNPCharacterBase::SetAIOrientationIntent(FVector InOrientationIntent)
{
	if (InputHandlerComponent)
		InputHandlerComponent->SetAIOrientationIntent(InOrientationIntent);
}

bool ALNPCharacterBase::TryActivateAttack()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!ASC)
		return false;

	if (ASC->HasMatchingGameplayTag(TAG_State_ComboWindow))
	{
		// 윈도우를 즉시 소비해 이 분기가 중복 진입되지 않도록 막는다
		ASC->RemoveLooseGameplayTag(TAG_State_ComboWindow);
		IncrementComboIndex();
		CancelCurrentAttackAbility();
		return TryActivateAttack_Impl();
	}

	if (ASC->HasMatchingGameplayTag(TAG_Block_AttackInput))
		return false;

	ResetCombo();
	return TryActivateAttack_Impl();
}

bool ALNPCharacterBase::TryActivateAttack_Impl()
{
	return false;
}

bool ALNPCharacterBase::ConsumeComboInput()
{
	const bool bWasBuffered = bComboInputBuffered;
	bComboInputBuffered = false;
	return bWasBuffered;
}

void ALNPCharacterBase::IncrementComboIndex()
{
	const ULNPWeaponData* WeaponDef = GetActiveWeaponDef();
	const int32 MaxCombo = WeaponDef ? WeaponDef->MaxComboCount : 5;
	CurrentComboIndex = (CurrentComboIndex + 1) % MaxCombo;
}

void ALNPCharacterBase::ResetCombo()
{
	CurrentComboIndex   = 0;
	bComboInputBuffered = false;
}

void ALNPCharacterBase::PostInitializeComponents()
{
	Super::PostInitializeComponents();
}

void ALNPCharacterBase::BeginPlay()
{
	Super::BeginPlay();

	MontageCtx = NewObject<ULNPMontageChooserContext>(this);

	if (UnarmedAnimLayerClass)
	{
		AnimSourceMesh->LinkAnimClassLayers(UnarmedAnimLayerClass);
	}
}

void ALNPCharacterBase::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	InitAbilitySystem();
}

void ALNPCharacterBase::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	InitAbilitySystem();
}

void ALNPCharacterBase::InitAbilitySystem()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!ASC)
		return;

	CurrentWeaponTag  = TAG_Weapon_Unarmed;
	CurrentAimModeTag = TAG_AimMode_None;
	ASC->AddLooseGameplayTag(CurrentWeaponTag);
	ASC->AddLooseGameplayTag(CurrentAimModeTag);

	for (const TSubclassOf<UGameplayAbility>& AbilityClass : DefaultAbilities)
	{
		if (AbilityClass)
			ASC->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1, INDEX_NONE, this));
	}

	if (InputHandlerComponent)
		InputHandlerComponent->CacheASC(ASC);
}

void ALNPCharacterBase::EquipWeapon(ULNPWeaponData* WeaponData)
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();

	// 기존 무기·조준모드 태그 제거
	if (ASC)
	{
		if (CurrentWeaponTag.IsValid())
			ASC->RemoveLooseGameplayTag(CurrentWeaponTag);
		if (CurrentAimModeTag.IsValid())
			ASC->RemoveLooseGameplayTag(CurrentAimModeTag);
	}

	// 신규 무기 태그 결정
	const FGameplayTag NewWeaponTag = (WeaponData && WeaponData->WeaponTag.IsValid())
		? WeaponData->WeaponTag
		: TAG_Weapon_Unarmed;

	// 조준 모드: 무기 데이터의 DefaultAimMode 사용, 미설정 시 None
	const FGameplayTag NewAimModeTag = (WeaponData && WeaponData->DefaultAimMode.IsValid())
		? WeaponData->DefaultAimMode
		: TAG_AimMode_None;

	// LockOn 전환 허용 여부는 DefaultAimMode == None일 때만 (추후 LockOn 로직에서 체크)
	const bool bFreeAim = (NewAimModeTag == TAG_AimMode_FreeAim);

	// ASC에 부여
	if (ASC)
	{
		ASC->AddLooseGameplayTag(NewWeaponTag);
		ASC->AddLooseGameplayTag(NewAimModeTag);
	}

	CurrentWeaponTag  = NewWeaponTag;
	CurrentAimModeTag = NewAimModeTag;

	// 이동 회전 방식 전환: FreeAim이면 카메라 정면 고정, 아니면 입력 방향
	if (InputHandlerComponent)
	{
		InputHandlerComponent->SetFaceMoveDirection(!bFreeAim);
	}

	// AnimLayer 연결
	TSubclassOf<UAnimInstance> LayerClass = (WeaponData && WeaponData->AnimLayerClass)
		? WeaponData->AnimLayerClass
		: UnarmedAnimLayerClass;
	AnimSourceMesh->LinkAnimClassLayers(LayerClass);

	// 무기 스켈레탈 메시 어태치
	if (WeaponData && WeaponData->WeaponMesh && !WeaponData->AttachSocketName.IsNone())
	{
		WeaponMesh->SetSkeletalMeshAsset(WeaponData->WeaponMesh);
		WeaponMesh->AttachToComponent(VisualMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, WeaponData->AttachSocketName);
		WeaponMesh->SetRelativeRotation(FRotator(0.0f, 90.0f, -4.0f));
		WeaponMesh->SetVisibility(true);
	}
	else
	{
		WeaponMesh->SetSkeletalMeshAsset(nullptr);
		WeaponMesh->SetVisibility(false);
	}

}

void ALNPCharacterBase::EquipTestWeapon(int32 SlotIndex)
{
	ULNPWeaponData* Target = TestWeaponList.IsValidIndex(SlotIndex) ? TestWeaponList[SlotIndex].Get() : nullptr;
	EquipWeapon(Target);
}

void ALNPCharacterBase::PlayHitReact(FVector HitFromWorldDir)
{
	UAnimInstance* Anim = GetAnimInstance();
	if (!Anim)
		return;

	// 피격자 로컬 공간으로 변환 (X=Forward, Y=Right)
	const FVector LocalDir = GetActorTransform().InverseTransformVectorNoScale(HitFromWorldDir);
	const float Angle = FMath::RadiansToDegrees(FMath::Atan2(LocalDir.Y, LocalDir.X));

	FGameplayTag Direction;
	if      (Angle >= -45.0f  && Angle <   45.0f) Direction = TAG_Montage_Value_Direction_Front;
	else if (Angle >=  45.0f  && Angle <  135.0f) Direction = TAG_Montage_Value_Direction_Right;
	else if (Angle >=  135.0f || Angle < -135.0f) Direction = TAG_Montage_Value_Direction_Back;
	else if (Angle >= -135.0f && Angle <  -45.0f) Direction = TAG_Montage_Value_Direction_Left;
	else                                          Direction = TAG_Montage_Value_Direction_Front;

	PlayMontage(TAG_Montage_Situation_HitReaction, Direction);
}

void ALNPCharacterBase::ApplyHitStop(float Duration, float TimeDilation)
{
	CustomTimeDilation = TimeDilation;
	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		CustomTimeDilation = 1.0f;
	}), Duration, false);
}

void ALNPCharacterBase::ApplyKnockback(const FVector HitFromDirection, const float Strength)
{
	if (MoverComponent)
		MoverComponent->ApplyKnockback(HitFromDirection, Strength);
}

FVector ALNPCharacterBase::GetUpDirection() const
{
	return GravityComponent ? GravityComponent->GetUpDirection() : FVector::UpVector;
}

UAnimMontage* ALNPCharacterBase::EvaluateMontage(FGameplayTag WeaponType, FGameplayTag SituationType, FGameplayTag Value) const
{
	if (!MontageChooser || !MontageCtx)
		return nullptr;

	MontageCtx->WeaponType    = FGameplayTagContainer(WeaponType);
	MontageCtx->SituationType = FGameplayTagContainer(SituationType);
	MontageCtx->Value         = Value.IsValid() ? FGameplayTagContainer(Value) : FGameplayTagContainer();

	return Cast<UAnimMontage>(UChooserFunctionLibrary::EvaluateChooser(MontageCtx, MontageChooser, UAnimMontage::StaticClass()));
}

UAnimMontage* ALNPCharacterBase::EvaluateMontage(FGameplayTag SituationType, FGameplayTag Value) const
{
	return EvaluateMontage(CurrentWeaponTag, SituationType, Value);
}

bool ALNPCharacterBase::PlayMontage(FGameplayTag SituationType, FGameplayTag Value) const
{
	UAnimInstance* Anim = GetAnimInstance();
	if (!Anim)
		return false;

	if (UAnimMontage* Montage = EvaluateMontage(SituationType, Value))
	{
		Anim->Montage_Play(Montage);
		return true;
	}

	return false;
}

void ALNPCharacterBase::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (InputHandlerComponent)
		InputHandlerComponent->SetupPlayerInputComponent(PlayerInputComponent);
}

