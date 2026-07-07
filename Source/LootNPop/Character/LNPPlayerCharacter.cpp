// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Character/LNPPlayerCharacter.h"
#include "Player/LNPPlayerState.h"
#include "Item/LNPEquipmentComponent.h"
#include "Item/LNPItemInstance.h"
#include "Item/LNPWeaponData.h"
#include "Item/LNPItemDefinitionBase.h"
#include "GAS/Abilities/LNPGameplayAbility.h"
#include "Interaction/LNPInteractionComponent.h"
#include "Camera/LNPLockOnComponent.h"
#include "Camera/LNPControlRotationComponent.h"
#include "Movement/LNPCharacterMoverComponent.h"
#include "LootNPop.h"

#include "AbilitySystemComponent.h"
#include "GameFramework/GameplayCameraComponent.h"
#include "MoverDataModelTypes.h"

ALNPPlayerCharacter::ALNPPlayerCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	GameplayCamera = CreateDefaultSubobject<UGameplayCameraComponent>(TEXT("GameplayCamera"));
	GameplayCamera->SetupAttachment(AnimSourceMesh);

	InteractionComponent = CreateDefaultSubobject<ULNPInteractionComponent>(TEXT("InteractionComponent"));

	LockOnComponent = CreateDefaultSubobject<ULNPLockOnComponent>(TEXT("LockOnComponent"));

	ControlRotationComponent = CreateDefaultSubobject<ULNPControlRotationComponent>(TEXT("ControlRotationComponent"));

	// 시뮬레이티드 프록시에도 InputCmd(ControlRotation 포함)를 SyncState에 동봉해 전달한다.
	// GetBaseAimRotation 오버라이드가 이를 읽어 관전자 화면의 Aim Offset을 동기화한다.
	// 플레이어 폰에만 적용 — Enemy는 시선 동기화가 불필요해 대역폭을 아낀다.
	if (MoverComponent)
		MoverComponent->bSyncInputsForSimProxy = true;
}

void ALNPPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();
}

void ALNPPlayerCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}

void ALNPPlayerCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	if (ALNPPlayerState* PS = GetPlayerState<ALNPPlayerState>())
	{
		PS->GetAbilitySystemComponent()->InitAbilityActorInfo(PS, this);

		// EqComp::BeginPlay가 DefaultWeapon GAS 부여를 완료한 경우 비주얼·EquippedWeaponData 동기화
		if (ULNPEquipmentComponent* EqComp = PS->GetEquipmentComponent())
			if (ULNPWeaponData* WeaponDef = EqComp->GetWeaponSlot().Definition.Get())
				ALNPCharacterBase::EquipWeapon(WeaponDef);
	}

	if (GameplayCamera)
		GameplayCamera->ActivateCameraForPlayerController(Cast<APlayerController>(NewController));
}

void ALNPPlayerCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	if (ALNPPlayerState* PS = GetPlayerState<ALNPPlayerState>())
		PS->GetAbilitySystemComponent()->InitAbilityActorInfo(PS, this);
}

void ALNPPlayerCharacter::EquipWeapon(ULNPWeaponData* WeaponData)
{
	Super::EquipWeapon(WeaponData);  // 비주얼·태그·EquippedWeaponData(서버만)

	if (ALNPPlayerState* PS = GetPlayerState<ALNPPlayerState>())
	{
		if (ULNPEquipmentComponent* EqComp = PS->GetEquipmentComponent())
		{
			// 클라이언트: WeaponSlot.Definition 즉시 갱신 후 서버에 GAS 부여 요청
			// 서버:      직접 GAS 어빌리티 부여
			EqComp->EquipWeapon(WeaponData);
			if (!HasAuthority())
				Server_EquipWeapon(WeaponData);
		}
	}
}

void ALNPPlayerCharacter::Server_EquipWeapon_Implementation(ULNPWeaponData* WeaponData)
{
	// 서버: 비주얼·태그·EquippedWeaponData 복제 마킹
	ALNPCharacterBase::EquipWeapon(WeaponData);

	// 서버: GAS 어빌리티 부여 (EqComp가 HasAuthority라 실제로 Grant됨)
	if (ALNPPlayerState* PS = GetPlayerState<ALNPPlayerState>())
		if (ULNPEquipmentComponent* EqComp = PS->GetEquipmentComponent())
			EqComp->EquipWeapon(WeaponData);
}

FRotator ALNPPlayerCharacter::GetBaseAimRotation() const
{
	// 로컬 제어(소유 클라이언트·리슨호스트·스탠드얼론): 기존 동작 — Control Rotation
	if (Controller && IsLocallyControlled())
		return Controller->GetControlRotation();

	// 서버의 원격 폰 / 시뮬레이티드 프록시: 소유 클라이언트가 매 틱 InputCmd에 실어 보낸
	// ControlRotation(월드 공간, 축당 16비트 압축 복제)을 사용한다.
	if (MoverComponent)
	{
		if (const FCharacterDefaultInputs* Inputs = MoverComponent->GetLastInputCmd().InputCollection.FindDataByType<FCharacterDefaultInputs>())
		{
			// 아직 입력을 한 번도 받지 못한 초기 상태(ZeroRotator)는 폴백으로 넘긴다
			if (!Inputs->ControlRotation.IsNearlyZero())
				return Inputs->ControlRotation;
		}
	}

	return Super::GetBaseAimRotation();
}

const ULNPWeaponData* ALNPPlayerCharacter::GetActiveWeaponDef() const
{
	const ALNPPlayerState* PS = GetPlayerState<ALNPPlayerState>();
	if (!PS)
		return nullptr;

	const ULNPEquipmentComponent* EqComp = PS->GetEquipmentComponent();
	if (!EqComp)
		return nullptr;

	return EqComp->GetWeaponSlot().Definition;
}

bool ALNPPlayerCharacter::TryActivateAttack_Impl()
{
	const ALNPPlayerState* PS = GetPlayerState<ALNPPlayerState>();
	if (!PS)
		return false;

	UAbilitySystemComponent* ASC = PS->GetAbilitySystemComponent();
	if (!ASC)
		return false;

	const ULNPEquipmentComponent* EqComp = PS->GetEquipmentComponent();
	if (!EqComp)
		return false;

	const FLNPWeaponInstance& WeaponSlot = EqComp->GetWeaponSlot();
	if (!WeaponSlot.IsValid())
		return false;

	// 서버/리슨서버: 핸들 직접 사용
	if (WeaponSlot.GrantedAbilities.IsValidIndex(0))
		return ASC->TryActivateAbility(WeaponSlot.GrantedAbilities[0]);

	// 클라이언트: Mixed 모드 복제 스펙을 클래스로 탐색
	const ULNPItemDefinitionBase* Def = Cast<ULNPItemDefinitionBase>(WeaponSlot.Definition.Get());
	if (Def && Def->AbilitiesToGrant.IsValidIndex(0))
		if (UClass* AbilityClass = Def->AbilitiesToGrant[0])
			return ASC->TryActivateAbilityByClass(AbilityClass);

	return false;
}

void ALNPPlayerCharacter::CancelCurrentAttackAbility()
{
	const ALNPPlayerState* PS = GetPlayerState<ALNPPlayerState>();
	if (!PS)
		return;

	UAbilitySystemComponent* ASC = PS->GetAbilitySystemComponent();
	if (!ASC)
		return;

	const ULNPEquipmentComponent* EqComp = PS->GetEquipmentComponent();
	if (!EqComp)
		return;

	const FLNPWeaponInstance& WeaponSlot = EqComp->GetWeaponSlot();
	if (!WeaponSlot.IsValid())
		return;

	// 서버/리슨서버: 핸들 직접 사용
	if (WeaponSlot.GrantedAbilities.IsValidIndex(0))
	{
		ASC->CancelAbilityHandle(WeaponSlot.GrantedAbilities[0]);
		return;
	}

	// 클라이언트: Mixed 모드 복제 스펙을 클래스로 탐색 (TryActivateAttack_Impl과 동일한 폴백).
	// GrantedAbilities는 서버 전용이라 이 폴백이 없으면 원격 클라이언트의 콤보 진행 시
	// 이전 어빌리티가 취소되지 않아 재활성화가 항상 실패한다 (콤보 1 반복 버그).
	const ULNPItemDefinitionBase* Def = Cast<ULNPItemDefinitionBase>(WeaponSlot.Definition.Get());
	if (Def && Def->AbilitiesToGrant.IsValidIndex(0))
	{
		if (UClass* AbilityClass = Def->AbilitiesToGrant[0])
		{
			if (FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromClass(AbilityClass))
				ASC->CancelAbilityHandle(Spec->Handle);
		}
	}
}

TArray<AActor*> ALNPPlayerCharacter::GetInteractionCandidates() const
{
	return InteractionComponent ? InteractionComponent->GetInteractionCandidates() : TArray<AActor*>();
}

AActor* ALNPPlayerCharacter::GetInteractionCandidate() const
{
	return InteractionComponent ? InteractionComponent->GetFirstInteractionCandidate() : nullptr;
}