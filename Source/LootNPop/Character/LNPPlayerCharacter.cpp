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
#include "Character/LNPInputHandlerComponent.h"
#include "Movement/LNPCharacterMoverComponent.h"
#include "GameMode/LNPGameMode.h"
#include "Player/LNPPlayerController.h"
#include "Config/LNPSettings.h"
#include "LootNPop.h"

#include "AbilitySystemComponent.h"
#include "GameFramework/GameplayCameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
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

	// bSyncInputsForSimProxy는 보간 프록시 전용 우회책이라 제거했다 (엔진에도 임시 옵션이라 명시돼 있다).
	// SimulatedProxyNetworkLOD=ForwardPredict에서는 시뮬레이티드 프록시도 실제로 시뮬레이션되므로
	// UMoverComponent가 일반 경로에서 CachedLastUsedInputCmd를 채운다 —
	// GetBaseAimRotation 오버라이드가 읽는 GetLastInputCmd()의 ControlRotation이 그대로 유효하다.
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

		// 사망 판정도 같은 자리에서 건다 — PossessedBy는 서버에서만 실행되므로 이 바인딩 자체가 권위 보장이다.
		// AttributeSet(PostGameplayEffectExecute)에 넣지 않는 이유: 그쪽은 플레이어와 Enemy가 공유하는데
		// Enemy는 이미 Mass 경로(ULNPHealthProcessor)로 사망을 처리하므로 이중 처리가 된다.
		ASC->GetGameplayAttributeValueChangeDelegate(ULNPBaseAttributeSet::GetHealthAttribute())
			.AddWeakLambda(this, [this](const FOnAttributeChangeData& Data)
			{
				if (Data.NewValue <= 0.f)
					HandleDeathOnServer();
			});

		// 기본 무기 지급·장착은 여기서 한다 — PlayerState의 연결이 완전히 성립한 뒤라야
		// 가방 인스턴스(복제 서브오브젝트)가 원격 클라이언트에 제대로 도달한다.
		// EqComp::BeginPlay에서 하면 게스트 가방이 빈 채로 남는다 (EnsureDefaultWeapon 주석 참조).
		if (ULNPEquipmentComponent* EqComp = PS->GetEquipmentComponent())
			EqComp->EnsureDefaultWeapon();
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
		UE_LOG(LogLootNPop, Log, TEXT("[LootPod] %s LootSpeed sync skipped — entity handle not ready"), *GetName());
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

// 장착 요청은 항상 서버 권위 경로로만 흐른다. 로컬 선반영(예측)은 하지 않는다 —
// 비주얼은 ULNPEquipmentComponent가 WeaponSlot 적용 후 밀어 넣는다(서버 즉시 / 클라 OnRep).
void ALNPPlayerCharacter::RequestEquipWeapon(ULNPWeaponData* WeaponData)
{
	if (!HasAuthority())
	{
		Server_EquipWeapon(WeaponData);
		return;
	}

	EquipWeaponOnServer(WeaponData);
}

void ALNPPlayerCharacter::Server_EquipWeapon_Implementation(ULNPWeaponData* WeaponData)
{
	EquipWeaponOnServer(WeaponData);
}

void ALNPPlayerCharacter::EquipWeaponOnServer(ULNPWeaponData* WeaponData)
{
	ALNPPlayerState* PS = GetPlayerState<ALNPPlayerState>();
	if (PS == nullptr)
		return;

	ULNPInventoryComponent* Inventory = PS->GetInventoryComponent();
	ULNPEquipmentComponent* EqComp = PS->GetEquipmentComponent();
	if (Inventory == nullptr || EqComp == nullptr)
		return;

	// nullptr = 맨손 전환 요청 (EquipTestWeapon의 범위 밖 슬롯).
	if (WeaponData == nullptr)
	{
		EqComp->UnequipWeapon();
		return;
	}

	// 정의로 장착하더라도 실제 인스턴스를 거친다 — 그래야 인벤토리 UI·bEquipped·드랍 가드가
	// 메뉴에서 장착한 무기와 완전히 같은 상태 기계를 탄다 (경로별 특수 케이스 없음).
	ULNPInventoryItemInstance* Instance = Inventory->FindBagInstanceByDefinition(WeaponData);
	if (Instance == nullptr)
	{
		// 보유하지 않은 무기를 요청했다 — 캐릭터의 TestWeaponList에 있을 때만 디버그 지급을 허용한다.
		// 이 검증이 없으면 클라이언트가 임의의 ULNPWeaponData 에셋을 지목해 장착할 수 있다.
		if (!TestWeaponList.Contains(WeaponData))
		{
			UE_LOG(LogLootNPop, Warning, TEXT("[Equip] %s requested unowned weapon %s — not in TestWeaponList, rejected"),
				*GetNameSafe(this), *GetNameSafe(WeaponData));
			return;
		}

		Instance = Inventory->AddItemInstance(WeaponData);
		if (Instance == nullptr)
			return;

		UE_LOG(LogLootNPop, Log, TEXT("[Equip] %s test weapon %s granted to bag"),
			*GetNameSafe(this), *GetNameSafe(WeaponData));
	}

	EqComp->EquipWeaponInstance(Instance);
}

void ALNPPlayerCharacter::RequestEquipWeaponInstance(ULNPInventoryItemInstance* Instance)
{
	if (Instance == nullptr)
		return;

	if (!HasAuthority())
	{
		// 인스턴스 포인터가 아니라 ItemId를 보낸다 — 서버가 소유 인벤토리에서 직접 조회·검증한다.
		Server_EquipWeaponInstance(Instance->GetItemId());
		return;
	}

	if (ALNPPlayerState* PS = GetPlayerState<ALNPPlayerState>())
		if (ULNPEquipmentComponent* EqComp = PS->GetEquipmentComponent())
			EqComp->EquipWeaponInstance(Instance);
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

	// 서버 검증: 요청자가 실제로 보유한 인스턴스인지 확인한다.
	ULNPInventoryItemInstance* Instance = Inventory->FindItemInstance(ItemId);
	if (Instance == nullptr)
		return;

	EqComp->EquipWeaponInstance(Instance);  // bEquipped 표시 + GAS 부여 + 비주얼 푸시
}

void ALNPPlayerCharacter::RequestMergeItem(const FGuid& ItemId)
{
	if (HasAuthority())
		MergeItemOnServer(ItemId);
	else
		Server_MergeItem(ItemId);
}

void ALNPPlayerCharacter::Server_MergeItem_Implementation(FGuid ItemId)
{
	MergeItemOnServer(ItemId);
}

void ALNPPlayerCharacter::MergeItemOnServer(const FGuid& ItemId)
{
	const ALNPPlayerState* PS = GetPlayerState<ALNPPlayerState>();
	if (!ItemId.IsValid() || PS == nullptr)
		return;

	// 소유 인벤토리에서만 조회한다 — 남의 아이템 ID를 보내도 찾히지 않는다.
	// 재료 수량·최대 레벨·장착 여부 검증은 전부 TryMergeItem 안에서 다시 이뤄진다.
	if (ULNPInventoryComponent* Inventory = PS->GetInventoryComponent())
		Inventory->TryMergeItem(ItemId);
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

	// 캐릭터 전방에 "작은 Pop"으로 스폰 — 이후 소멸·획득 규칙은 LootPod 보상과 완전 동일 (공용 경로)
	const FVector DropLocation = GetActorLocation()
		+ GetActorForwardVector() * 150.0f
		+ GetActorUpVector() * 80.0f;

	RemoveAndSpawnDice(*Inventory, ItemId, bIsBuff, DropLocation, /*ImpulseScale=*/0.4f);
}

void ALNPPlayerCharacter::HandleDeathOnServer()
{
	// 한 프레임에 여러 피해가 겹치면 델리게이트가 여러 번 울린다 — 최초 1회만 통과시킨다.
	if (!HasAuthority() || bIsDead)
		return;
	bIsDead = true;

	ALNPPlayerState* PS = GetPlayerState<ALNPPlayerState>();

	// ASC는 PlayerState 소유라 폰이 죽어도 살아남는다 — 진행 중인 어빌리티를 명시적으로 끊어 준다.
	if (PS)
	{
		if (UAbilitySystemComponent* ASC = PS->GetAbilitySystemComponent())
			ASC->CancelAllAbilities();
	}

	// 연출보다 먼저 드랍한다 — 무기 해제가 WeaponSlot 복제로 흘러 각 클라이언트의 WeaponMesh를 알아서 감춘다.
	DropAllItemsOnDeath();

	Multicast_OnDeath(GetUpDirection() * DeathPopSpeed);

	// 리스폰 타이머는 곧 파괴될 폰이 아니라 GameMode가 들고 있어야 한다.
	if (ALNPGameMode* GM = GetWorld()->GetAuthGameMode<ALNPGameMode>())
		GM->ScheduleRespawn(GetController(), GetDefault<ULNPSettings>()->PlayerRespawnDelay);

	UE_LOG(LogLootNPop, Log, TEXT("[Death] %s died — respawning in %.1fs"),
		*GetNameSafe(PS), GetDefault<ULNPSettings>()->PlayerRespawnDelay);
}

void ALNPPlayerCharacter::Multicast_OnDeath_Implementation(FVector PopVelocity)
{
	// 리슨 서버에서는 이 구현부가 로컬로도 실행된다 — 서버 권위 처리는 전부 HandleDeathOnServer에 있으므로
	// 여기는 순수 연출뿐이다. 전용 플래그로 멱등을 보장한다 (bIsDead는 서버에서 이미 켜져 있어 쓸 수 없다).
	if (bDeathFxPlayed)
		return;
	bDeathFxPlayed = true;
	bIsDead = true;

	if (IsLocallyControlled())
	{
		// ⚠ SetGameplayInputEnabled(false)가 아니라 이쪽이다 — 그쪽은 매핑 컨텍스트를 통째로 떼어
		// Look까지 죽이므로 사망 중 카메라를 돌릴 수 없게 된다. 여기서는 Look만 살린다.
		if (InputHandlerComponent)
			InputHandlerComponent->SetGameplayInputBlocked(true);

		// 죽은 뒤에도 락온이 살아 있으면 카메라가 적에게 끌려가고 대상 머리 위 표식도 남는다.
		if (LockOnComponent)
		{
			if (LockOnComponent->IsLockOnActive())
				LockOnComponent->ToggleLockOn();
			LockOnComponent->SetComponentTickEnabled(false);
		}

		// 시체가 주변 LootDice를 줍지 않도록.
		if (InteractionComponent)
			InteractionComponent->SetComponentTickEnabled(false);

		// 카메라 회전 기준점을 시체 쪽으로 내리는 일은 카메라 리그가 한다
		// (ULNPRagdollPivotOffsetCameraNode — 튜닝 값은 CR_ThirdPerson에 있다).
		BeginDeathCameraFollow();

		if (ALNPPlayerController* PC = Cast<ALNPPlayerController>(GetController()))
			PC->ShowDeathScreen(GetDefault<ULNPSettings>()->PlayerRespawnDelay);
	}

	EnterRagdoll(PopVelocity);
}

void ALNPPlayerCharacter::DropAllItemsOnDeath()
{
	ALNPPlayerState* PS = GetPlayerState<ALNPPlayerState>();
	if (PS == nullptr)
		return;

	ULNPInventoryComponent* Inventory = PS->GetInventoryComponent();
	ULNPEquipmentComponent* Equipment = PS->GetEquipmentComponent();
	if (Inventory == nullptr || Equipment == nullptr)
		return;

	// ① 장착 해제를 **먼저**. 장착본이 bEquipped인 채로 제거되면 WeaponSlot.SourceInstance가 댕글링이 되고
	//    무기 GAS 부여(어빌리티·스텟 GE)가 회수되지 않는다. UnequipWeapon이 둘 다 처리한다.
	Equipment->UnequipWeapon();

	// ② ItemId·정의·레벨을 먼저 스냅샷한다. 제거 루프가 FastArray와 등록 서브오브젝트를 건드리므로
	//    인스턴스 포인터를 들고 순회하면 안 된다. (GetBagInstances는 장착본도 포함해 반환한다.)
	TArray<TPair<FGuid, bool>> DropTargets;   // <ItemId, bIsBuff>
	for (const ULNPInventoryItemInstance* Instance : Inventory->GetBagInstances())
	{
		if (Instance)
			DropTargets.Emplace(Instance->GetItemId(), false);
	}
	for (const ULNPInventoryItemInstance* Instance : Inventory->GetActiveBuffInstances())
	{
		if (Instance)
			DropTargets.Emplace(Instance->GetItemId(), true);
	}

	// ③ 사망 지점 한 곳에서 쏟는다 — SpawnDice가 Up 기준 원뿔 랜덤 임펄스 + 랜덤 축 회전을 주므로
	//    같은 자리에서 스폰해도 자연히 흩어진다 (SpawnPodRewards와 같은 전략).
	const FVector DropOrigin = GetActorLocation() + GetUpDirection() * 80.0f;
	for (const TPair<FGuid, bool>& Target : DropTargets)
	{
		RemoveAndSpawnDice(*Inventory, Target.Key, Target.Value, DropOrigin, /*ImpulseScale=*/1.0f);
	}

	UE_LOG(LogLootNPop, Log, TEXT("[Death] %s dropped %d items"), *GetNameSafe(this), DropTargets.Num());
}

void ALNPPlayerCharacter::BeginDeathCameraFollow()
{
	if (bDeathCameraFollowActive || GameplayCamera == nullptr)
		return;

	// AnimSourceMesh에서 뗀다 — UMoverComponent::FinalizeFrame이 매 프레임 PrimaryVisualComponent의
	// 상대 트랜스폼을 되돌리므로, 폰 계층에 붙어 있는 한 손으로 옮겨도 원위치된다.
	GameplayCamera->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	DeathCameraSmoothedLocation = GameplayCamera->GetComponentLocation();
	bDeathCameraFollowActive = true;
}

void ALNPPlayerCharacter::TickDeathCameraFollow(float DeltaSeconds)
{
	if (!bDeathCameraFollowActive || GameplayCamera == nullptr)
		return;

	// UGameplayCameraComponentBase는 자기 컴포넌트 트랜스폼을 카메라 포즈의 원점으로 쓴다 —
	// 월드 위치만 옮기면 리그가 그대로 따라온다. **회전은 건드리지 않는다** (시체 회전을 따라가면 화면이 요동친다).
	DeathCameraSmoothedLocation = FMath::VInterpTo(DeathCameraSmoothedLocation, GetRagdollAnchorLocation(),
		DeltaSeconds, DeathCameraFollowSpeed);
	GameplayCamera->SetWorldLocation(DeathCameraSmoothedLocation);
}

void ALNPPlayerCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	TickDeathCameraFollow(DeltaSeconds);
}

void ALNPPlayerCharacter::RemoveAndSpawnDice(ULNPInventoryComponent& Inventory, const FGuid& ItemId, bool bIsBuff,
                                             const FVector& Location, float ImpulseScale)
{
	ULNPInventoryItemInstance* Instance = bIsBuff ? Inventory.FindBuffInstance(ItemId) : Inventory.FindItemInstance(ItemId);
	if (Instance == nullptr)
		return;

	// 제거하면 인스턴스가 사라지므로 페이로드에 실을 값은 먼저 읽어 둔다.
	ULNPItemDefinitionBase* ItemDef = Instance->GetDefinition();
	const int32 DiceItemLevel = Instance->GetItemLevel();

	// 인벤토리 제거 성공 전에는 스폰하지 않는다 (아이템 복제 방지).
	// 버프는 잔여 지속 시간을 회수해 페이로드에 싣는다 — 양도받은 파티원이 이어서 쓴다.
	float DiceRemainingDuration = 0.0f;
	if (bIsBuff)
	{
		DiceRemainingDuration = Inventory.RemoveBuffInstance(ItemId);
	}
	else if (!Inventory.RemoveItemInstance(ItemId))
	{
		return;
	}

	ALNPLootDice::SpawnDice(*GetWorld(), Location, ItemDef, DiceRemainingDuration, DiceItemLevel, ImpulseScale);

	UE_LOG(LogLootNPop, Log, TEXT("[LootDice] %s dropped — %s Lv.%d (buff remaining %.1fs)"),
		*GetNameSafe(this), *GetNameSafe(ItemDef), DiceItemLevel, DiceRemainingDuration);
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

// WeaponSlot이 복제되므로 시뮬레이티드 프록시를 포함한 모든 머신에서 이 값이 정확하다.
// (역할별 분기가 필요했던 시절은 WeaponSlot이 복제되지 않던 때의 이야기다.)
const ULNPWeaponData* ALNPPlayerCharacter::GetActiveWeaponDef() const
{
	return ResolveWeaponDefForVisuals();
}

ULNPWeaponData* ALNPPlayerCharacter::ResolveWeaponDefForVisuals() const
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
namespace
{
	/** 사망·리스폰 검증용 자살 커맨드. 리스폰 지점 랜덤성은 여러 번 죽여봐야 확인되므로 필요하다.
	 *  Usage: LNP.Debug.KillPlayer [PlayerIndex]
	 *   - 권위(호스트/서버) 콘솔에서만 동작한다. 인수 없으면 0번(호스트 자신).
	 *   - Health를 0으로 내려 정상 사망 경로(PossessedBy의 Health 델리게이트)를 그대로 탄다. */
	FAutoConsoleCommandWithWorldAndArgs GLNPDebugKillPlayer(
		TEXT("LNP.Debug.KillPlayer"),
		TEXT("Server-only: set a player's Health to 0 to trigger the death/respawn flow. Args: [PlayerIndex]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (World == nullptr)
				return;

			TArray<APlayerController*> Controllers;
			for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
			{
				if (APlayerController* PC = It->Get())
					Controllers.Add(PC);
			}

			const int32 PlayerIndex = (Args.Num() >= 1) ? FCString::Atoi(*Args[0]) : 0;
			if (!Controllers.IsValidIndex(PlayerIndex))
			{
				UE_LOG(LogLootNPop, Warning, TEXT("[LNP.Debug.KillPlayer] PlayerIndex %d out of range (0..%d)."),
					PlayerIndex, Controllers.Num() - 1);
				return;
			}

			ALNPPlayerState* PS = Controllers[PlayerIndex]->GetPlayerState<ALNPPlayerState>();
			UAbilitySystemComponent* ASC = PS ? PS->GetAbilitySystemComponent() : nullptr;
			if (ASC == nullptr)
			{
				UE_LOG(LogLootNPop, Warning, TEXT("[LNP.Debug.KillPlayer] No ASC on player %d."), PlayerIndex);
				return;
			}
			if (PS->GetLocalRole() != ROLE_Authority)
			{
				UE_LOG(LogLootNPop, Warning, TEXT("[LNP.Debug.KillPlayer] Must run on authority (listen host / server)."));
				return;
			}

			ASC->SetNumericAttributeBase(ULNPBaseAttributeSet::GetHealthAttribute(), 0.f);
			UE_LOG(LogLootNPop, Log, TEXT("[LNP.Debug.KillPlayer] Killed player %d (%s)."), PlayerIndex, *GetNameSafe(PS));
		}));
}
