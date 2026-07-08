// Copyright (c) 2026 LootNPop. All rights reserved.

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
#include "Character/LNPMassAgentComponent.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "ChooserFunctionLibrary.h"
#include "Net/UnrealNetwork.h"

#include "HitDetection/LNPGhostProjectileSubsystem.h"


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
	// bVisible과 별개 플래그로 게임 중 렌더링을 차단한다. Mass 풀 재활성화(SetActorHiddenInGame(false))나
	// 재귀 SetVisibility(true) 계열이 bVisible을 다시 켜도 이 플래그는 건드리지 않으므로 노출되지 않는다.
	// 자식(VisualMesh)에는 전파되지 않음 — 렌더링은 VisualMesh가 담당.
	AnimSourceMesh->SetHiddenInGame(true);
	VisualMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("VisualMesh"));
	VisualMesh->SetupAttachment(AnimSourceMesh);

	// ULNPMassAgentComponent: 에이전트 경로의 NetID 캐싱 타이밍 갭 보정 (Phase 6.5 — 클래스 주석 참조)
	MassAgentComponent = CreateDefaultSubobject<ULNPMassAgentComponent>(TEXT("MassAgentComponent"));

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

void ALNPCharacterBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ALNPCharacterBase, EquippedWeaponData);
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
		if (!HasAuthority())
			Server_SetComboIndex(CurrentComboIndex); // 서버 어빌리티가 같은 몽타주 섹션을 재생하도록 동기화
		CancelCurrentAttackAbility();
		return TryActivateAttack_Impl();
	}

	if (ASC->HasMatchingGameplayTag(TAG_Block_AttackInput))
		return false;

	const int32 PrevComboIndex = CurrentComboIndex;
	ResetCombo();
	if (!HasAuthority() && PrevComboIndex != 0)
		Server_SetComboIndex(0); // 인덱스가 실제로 바뀔 때만 전송 (연사 입력 RPC 스팸 방지)
	return TryActivateAttack_Impl();
}

void ALNPCharacterBase::Server_SetComboIndex_Implementation(int32 NewComboIndex)
{
	const ULNPWeaponData* WeaponDef = GetActiveWeaponDef();
	const int32 MaxCombo = WeaponDef ? WeaponDef->MaxComboCount : 5;
	CurrentComboIndex = FMath::Clamp(NewComboIndex, 0, FMath::Max(0, MaxCombo - 1));
}

bool ALNPCharacterBase::TryActivateAttack_Impl()
{
	return false;
}

void ALNPCharacterBase::IncrementComboIndex()
{
	const ULNPWeaponData* WeaponDef = GetActiveWeaponDef();
	const int32 MaxCombo = WeaponDef ? WeaponDef->MaxComboCount : 5;
	CurrentComboIndex = (CurrentComboIndex + 1) % MaxCombo;
}

void ALNPCharacterBase::ResetCombo()
{
	CurrentComboIndex = 0;
}

void ALNPCharacterBase::BeginPlay()
{
	Super::BeginPlay();

	MontageCtx = NewObject<ULNPMontageChooserContext>(this);

	// EquippedWeaponData가 BeginPlay 이전에 이미 리플리케이트됐을 수 있으므로(원격 관전 시점의 초기 스폰),
	// 무조건 Unarmed로 링크하지 않고 EquipWeapon을 거쳐 현재 값을 존중한다.
	// virtual 우회(OnRep_CurrentWeapon과 동일 패턴) — 소유하지 않은 액터에서 Server RPC가 호출되는 것을 방지.
	ALNPCharacterBase::EquipWeapon(EquippedWeaponData);
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

	if (HasAuthority())
	{
		for (const TSubclassOf<UGameplayAbility>& AbilityClass : DefaultAbilities)
		{
			if (AbilityClass)
				ASC->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1, INDEX_NONE, this));
		}
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
		WeaponMesh->SetRelativeLocation(WeaponData->WeaponMeshRelativeLocation);
		WeaponMesh->SetRelativeRotation(WeaponData->WeaponMeshRelativeRotation);
		WeaponMesh->SetVisibility(true);
	}
	else
	{
		WeaponMesh->SetSkeletalMeshAsset(nullptr);
		WeaponMesh->SetVisibility(false);
	}

	if (HasAuthority())
		EquippedWeaponData = WeaponData;
}

void ALNPCharacterBase::OnRep_CurrentWeapon()
{
	// 비주얼·태그만 갱신 (GAS 부여는 서버가 처리) — virtual 우회로 RPC 루프 방지
	ALNPCharacterBase::EquipWeapon(EquippedWeaponData);
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

void ALNPCharacterBase::ApplyLocalHitFeedback()
{
	ApplyHitStop(0.2f);
}

void ALNPCharacterBase::ApplyKnockback(const FVector HitFromDirection, const float Strength)
{
	if (MoverComponent)
		MoverComponent->ApplyKnockback(HitFromDirection, Strength);
}

void ALNPCharacterBase::Multicast_SpawnGhostProjectiles_Implementation(FLNPProjectileSharedFragment SharedData, FVector SpawnPos,
	const TArray<FVector>& Velocities, float ProjectileLifetime, ELNPInstigatorTeam InstigatorTeam,
	int32 KeyOrSalvo, int32 InstigatorPlayerID, float UpstreamDelaySeconds)
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() != NM_Client)
		return; // 서버/리슨호스트 자신은 이미 authoritative 엔티티를 갖고 있음 — 중복 방지.

	if (IsLocallyControlled())
		return; // 자기 자신이 쏜 발사체 — 이미 예측 Ghost가 있음(ULNPAbility_RangedAttack::SpawnProjectile).

	if (ULNPGhostProjectileSubsystem* GhostSub = World->GetSubsystem<ULNPGhostProjectileSubsystem>())
		GhostSub->SpawnSpectatorGhosts(SharedData, SpawnPos, Velocities, ProjectileLifetime,
			InstigatorTeam, InstigatorPlayerID, KeyOrSalvo, UpstreamDelaySeconds);
}

void ALNPCharacterBase::Multicast_RespawnReflectedGhost_Implementation(FLNPProjectileSharedFragment SharedData, FVector SpawnPos,
	FVector NewVelocity, float LifetimeRemaining, ELNPInstigatorTeam NewTeam,
	int32 OldInstigatorPlayerID, int32 OldKeyOrSalvo, uint8 OldSpawnIndex,
	int32 NewInstigatorPlayerID, int32 NewKeyOrSalvo)
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() != NM_Client)
		return; // 서버/리슨호스트 자신은 이미 authoritative 엔티티에 직접 반영됨.

	ULNPGhostProjectileSubsystem* GhostSub = World->GetSubsystem<ULNPGhostProjectileSubsystem>();
	if (!GhostSub)
		return;

	// 구 Ghost 소멸 — 공격자 클라이언트가 오예측(패링을 모른 채 히트 판정)으로 이미 파괴했어도 no-op으로 안전.
	GhostSub->DestroyGhost({ OldInstigatorPlayerID, OldKeyOrSalvo, OldSpawnIndex });

	// 서버 확정 반사 지점·속도로 새 Ghost 스폰 — 발사 방송과 동일한 공용 경로 (Dead Reckoning 포함).
	// 반사는 서버에서 발원하므로 업스트림 지연은 0.
	const FVector Velocity = NewVelocity;
	GhostSub->SpawnSpectatorGhosts(SharedData, SpawnPos, MakeArrayView(&Velocity, 1), LifetimeRemaining,
		NewTeam, NewInstigatorPlayerID, NewKeyOrSalvo, 0.f);
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

