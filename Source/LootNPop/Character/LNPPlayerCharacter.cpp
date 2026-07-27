// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Character/LNPPlayerCharacter.h"
#include "Player/LNPPlayerState.h"
#include "Item/LNPEquipmentComponent.h"
#include "Item/LNPInventoryComponent.h"
#include "Item/LNPBuffData.h"
#include "Item/LNPItemInstance.h"
#include "LootDice/LNPLootDice.h"
#include "Item/LNPWeaponData.h"
#include "Item/LNPItemDefinitionBase.h"
#include "Item/LNPInventoryItemInstance.h"
#include "GAS/Abilities/LNPGameplayAbility.h"
#include "Interaction/LNPInteractionComponent.h"
#include "Camera/LNPLockOnComponent.h"
#include "Camera/LNPControlRotationComponent.h"
#include "Movement/LNPCharacterMoverComponent.h"
#include "LootNPop.h"

#include "AbilitySystemComponent.h"
#include "GameFramework/GameplayCameraComponent.h"
#include "MoverDataModelTypes.h"

#include "GAS/Attributes/LNPBaseAttributeSet.h"
#include "LootPod/LNPLootPodMassTypes.h"
#include "MassAgentComponent.h"
#include "MassEntityManager.h"
#include "MassEntityUtils.h"
#include "MassCommands.h"

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
		UAbilitySystemComponent* ASC = PS->GetAbilitySystemComponent();
		ASC->InitAbilityActorInfo(PS, this);

		// LootSpeed Attribute 변경 → 플레이어 엔티티 Fragment 동기화 (서버 전용 경로 — PossessedBy는 서버에서만 실행)
		ASC->GetGameplayAttributeValueChangeDelegate(ULNPBaseAttributeSet::GetLootSpeedAttribute())
			.AddWeakLambda(this, [this](const FOnAttributeChangeData& Data)
			{
				PushLootSpeedToEntity(Data.NewValue);
			});

		// EqComp::BeginPlay가 DefaultWeapon GAS 부여를 완료한 경우 비주얼·EquippedWeaponData 동기화
		if (ULNPEquipmentComponent* EqComp = PS->GetEquipmentComponent())
			if (ULNPWeaponData* WeaponDef = EqComp->GetWeaponSlot().Definition.Get())
				ALNPCharacterBase::EquipWeapon(WeaponDef);
	}

	if (GameplayCamera)
		GameplayCamera->ActivateCameraForPlayerController(Cast<APlayerController>(NewController));
}

void ALNPPlayerCharacter::PushLootSpeedToEntity(float NewLootSpeed)
{
	const UMassAgentComponent* AgentComponent = FindComponentByClass<UMassAgentComponent>();
	if (AgentComponent == nullptr)
		return;

	const FMassEntityHandle PlayerEntity = AgentComponent->GetEntityHandle();
	if (!PlayerEntity.IsValid())
	{
		// 퍼펫 핸드셰이크 전이면 스킵 — 이후 상호작용 시점 캐싱(StartLootingOnServer)이 커버한다
		UE_LOG(LogLootNPop, Verbose, TEXT("[LootPod] %s LootSpeed sync skipped — entity handle not ready"), *GetName());
		return;
	}

	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(*GetWorld());
	EntityManager.Defer().PushCommand<FMassDeferredSetCommand>([PlayerEntity, NewLootSpeed](FMassEntityManager& Manager)
	{
		if (!Manager.IsEntityValid(PlayerEntity))
			return;

		if (FLNPPlayerLootingFragment* Fragment = Manager.GetFragmentDataPtr<FLNPPlayerLootingFragment>(PlayerEntity))
		{
			Fragment->BuffedLootSpeed = NewLootSpeed;
		}
		else
		{
			FLNPPlayerLootingFragment Payload;
			Payload.BuffedLootSpeed = NewLootSpeed;
			Manager.Defer().PushCommand<FMassCommandAddFragmentInstances<FLNPPlayerLootingFragment>>(PlayerEntity, Payload);
		}
	});

	UE_LOG(LogLootNPop, Log, TEXT("[LootPod] %s LootSpeed -> %.2f (entity fragment sync)"), *GetName(), NewLootSpeed);
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

void ALNPPlayerCharacter::EquipWeaponInstance(ULNPInventoryItemInstance* Instance)
{
	if (Instance == nullptr)
		return;

	ULNPWeaponData* WeaponData = Cast<ULNPWeaponData>(Instance->GetDefinition());
	if (WeaponData == nullptr)
		return;

	Super::EquipWeapon(WeaponData);  // 비주얼·태그·EquippedWeaponData(서버만)

	if (ALNPPlayerState* PS = GetPlayerState<ALNPPlayerState>())
	{
		if (ULNPEquipmentComponent* EqComp = PS->GetEquipmentComponent())
		{
			// 클라이언트: 슬롯이 인스턴스를 즉시 참조(예측) 후 서버에 GAS 부여·bEquipped 요청
			EqComp->EquipWeaponInstance(Instance);
			if (!HasAuthority())
				Server_EquipWeaponInstance(Instance->GetItemId());
		}
	}
}

void ALNPPlayerCharacter::Server_EquipWeaponInstance_Implementation(FGuid ItemId)
{
	ALNPPlayerState* PS = GetPlayerState<ALNPPlayerState>();
	if (PS == nullptr)
		return;

	ULNPInventoryComponent* Inventory = PS->GetInventoryComponent();
	ULNPEquipmentComponent* EqComp = PS->GetEquipmentComponent();
	if (Inventory == nullptr || EqComp == nullptr)
		return;

	ULNPInventoryItemInstance* Instance = Inventory->FindItemInstance(ItemId);
	if (Instance == nullptr)
		return;

	if (ULNPWeaponData* WeaponData = Cast<ULNPWeaponData>(Instance->GetDefinition()))
		ALNPCharacterBase::EquipWeapon(WeaponData);  // 서버 비주얼·복제 마킹

	EqComp->EquipWeaponInstance(Instance);  // 서버 권위: bEquipped 표시 + GAS 부여
}

void ALNPPlayerCharacter::DropItem(const FGuid& ItemId)
{
	if (HasAuthority())
		DropItemOnServer(ItemId);
	else
		Server_DropItem(ItemId);
}

void ALNPPlayerCharacter::Server_DropItem_Implementation(FGuid ItemId)
{
	DropItemOnServer(ItemId);
}

void ALNPPlayerCharacter::DropItemOnServer(const FGuid& ItemId)
{
	ALNPPlayerState* PS = GetPlayerState<ALNPPlayerState>();
	if (!ItemId.IsValid() || PS == nullptr)
		return;

	ULNPInventoryComponent* Inventory = PS->GetInventoryComponent();
	if (Inventory == nullptr)
		return;

	// ItemId로 인스턴스를 찾는다 — 가방 우선, 없으면 활성 버프. 인스턴스 정체성이라 사본을 정확히 구분한다
	// (구 DA_Pistol 오검출은 정의 포인터 비교 탓이었고, 이제 인스턴스 ItemId로 근본 해소).
	bool bIsBuff = false;
	ULNPInventoryItemInstance* Instance = Inventory->FindItemInstance(ItemId);
	if (Instance == nullptr)
	{
		Instance = Inventory->FindBuffInstance(ItemId);
		bIsBuff = (Instance != nullptr);
	}
	if (Instance == nullptr)
	{
		UE_LOG(LogLootNPop, Log, TEXT("[LootDice] Drop rejected — ItemId %s not owned"), *ItemId.ToString());
		return;
	}
	if (Instance->IsEquipped())
	{
		UE_LOG(LogLootNPop, Log, TEXT("[LootDice] Drop rejected — %s is equipped"), *GetNameSafe(Instance->GetDefinition()));
		return;
	}

	ULNPItemDefinitionBase* ItemDef = Instance->GetDefinition();

	// 인벤토리 제거 성공 전에는 스폰하지 않는다 (아이템 복제 방지).
	// 버프는 잔여 지속 시간을 회수해 페이로드에 싣는다 — 양도받은 파티원이 이어서 쓴다.
	float DiceRemainingDuration = 0.0f;
	if (bIsBuff)
		DiceRemainingDuration = Inventory->RemoveBuffInstance(ItemId);
	else
		Inventory->RemoveItemInstance(ItemId);

	// 캐릭터 전방에 "작은 Pop"으로 스폰 — 이후 소멸·획득 규칙은 LootPod 보상과 완전 동일 (공용 경로)
	const FVector DropLocation = GetActorLocation()
		+ GetActorForwardVector() * 150.0f
		+ GetActorUpVector() * 80.0f;
	ALNPLootDice::SpawnDice(*GetWorld(), DropLocation, ItemDef, DiceRemainingDuration, /*ImpulseScale=*/0.4f);

	UE_LOG(LogLootNPop, Log, TEXT("[LootDice] %s dropped — %s (buff remaining %.1fs)"),
		*GetNameSafe(this), *GetNameSafe(ItemDef), DiceRemainingDuration);
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