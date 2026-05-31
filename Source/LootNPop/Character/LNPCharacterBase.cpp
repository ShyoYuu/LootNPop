// Copyright Epic Games, Inc. All Rights Reserved.

#include "LNPCharacterBase.h"
#include "Movement/LNPCharacterMoverComponent.h"
#include "Character/LNPInputHandlerComponent.h"
#include "Gravity/LNPPawnGravityComponent.h"
#include "Player/LNPPlayerState.h"
#include "Item/LNPWeaponData.h"
#include "LNPGameplayTags.h"
#include "LootNPop.h"

#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "AbilitySystemComponent.h"
#include "MassAgentComponent.h"


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

	WeaponMeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
	WeaponMeshComponent->SetupAttachment(VisualMesh);
	WeaponMeshComponent->SetVisibility(false);

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
	return false;
}

void ALNPCharacterBase::PostInitializeComponents()
{
	Super::PostInitializeComponents();
}

void ALNPCharacterBase::BeginPlay()
{
	Super::BeginPlay();

	// 초기 상태: 맨손 태그 부여 + 맨손 AnimLayer 연결
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		CurrentWeaponTag  = TAG_Weapon_Unarmed;
		CurrentAimModeTag = TAG_AimMode_None;
		ASC->AddLooseGameplayTag(CurrentWeaponTag);
		ASC->AddLooseGameplayTag(CurrentAimModeTag);
	}

	if (UnarmedAnimLayerClass)
	{
		AnimSourceMesh->LinkAnimClassLayers(UnarmedAnimLayerClass);
	}
}

void ALNPCharacterBase::EquipWeapon(ULNPWeaponData* WeaponData)
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();

	// 기존 무기·조준모드 태그 제거
	if (ASC)
	{
		ASC->RemoveLooseGameplayTag(CurrentWeaponTag);
		ASC->RemoveLooseGameplayTag(CurrentAimModeTag);
	}

	ActiveWeaponData = WeaponData;

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
		WeaponMeshComponent->SetSkeletalMeshAsset(WeaponData->WeaponMesh);
		WeaponMeshComponent->AttachToComponent(VisualMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, WeaponData->AttachSocketName);
		WeaponMeshComponent->SetRelativeRotation(FRotator(0.0f, 90.0f, -4.0f));
		WeaponMeshComponent->SetVisibility(true);
	}
	else
	{
		WeaponMeshComponent->SetSkeletalMeshAsset(nullptr);
		WeaponMeshComponent->SetVisibility(false);
	}
}

void ALNPCharacterBase::EquipTestWeapon(int32 SlotIndex)
{
	ULNPWeaponData* Target = TestWeaponList.IsValidIndex(SlotIndex) ? TestWeaponList[SlotIndex].Get() : nullptr;
	EquipWeapon(Target);
}

void ALNPCharacterBase::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (InputHandlerComponent)
		InputHandlerComponent->SetupPlayerInputComponent(PlayerInputComponent);
}

