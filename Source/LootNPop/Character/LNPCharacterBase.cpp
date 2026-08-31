// Copyright (c) 2026 LootNPop. All rights reserved.

#include "LNPCharacterBase.h"
#include "Movement/LNPCharacterMoverComponent.h"
#include "Character/LNPInputHandlerComponent.h"
#include "Gravity/LNPPawnGravityComponent.h"
#include "MotionWarpingComponent.h"
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

	// Mover가 InitializeComponent에서 이 컴포넌트를 찾아 UMotionWarpingMoverAdapter를 붙인다
	// (MoverComponent.cpp의 "Optional motion warping support"). 별도 배선은 필요 없다.
	MotionWarpingComponent = CreateDefaultSubobject<UMotionWarpingComponent>(TEXT("MotionWarpingComponent"));

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

bool ALNPCharacterBase::IsADSActive() const
{
	return InputHandlerComponent ? InputHandlerComponent->IsADSActive() : false;
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

void ALNPCharacterBase::SetAIDesiredSpeed(float InSpeed)
{
	if (InputHandlerComponent)
		InputHandlerComponent->SetAIDesiredSpeed(InSpeed);
}

bool ALNPCharacterBase::TryActivateAttack()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!ASC)
		return false;

	// 경직 중에는 어떤 경로로도 공격이 나가지 않는다.
	// ⚠️ 콤보 창 분기보다 **앞**이어야 한다 — 그 분기는 Block.AttackInput을 보지 않고 곧바로 재발동하므로,
	// 그로기 진입 시점에 콤보 창 태그가 한 프레임이라도 남아 있으면 차단을 통째로 건너뛴다.
	// (NPC의 FLNPEnemyAttackTask는 매 프레임 이 함수를 두드리므로 그 한 프레임을 정확히 집어낸다.)
	if (ASC->HasMatchingGameplayTag(TAG_State_Staggered))
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

	// 콤보 상태는 실제로 공격이 발동됐을 때만 확정한다.
	// 블락 구간과 콤보 창 사이의 사각지대(콤보 전환 직후 ANS가 아직 태그를 걸지 않은 1프레임 포함)에서는
	// 발동이 쿨다운·중복 활성화로 실패하는데, 이때 인덱스만 0으로 떨어뜨리면
	// 다음 콤보 입력이 다시 0→1이 되어 2타만 무한 반복된다.
	const int32 PrevComboIndex = CurrentComboIndex;
	ResetCombo();

	// ⚠ 이 RPC는 반드시 TryActivateAttack_Impl() **앞**에서 보내야 한다.
	// _Impl() 안의 TryActivateAbility가 ServerTryActivateAbility RPC를 PlayerState(ASC) 채널로 보내는데,
	// 뒤에서 보내면 서버가 어빌리티 활성화를 먼저 처리해 ActivateAbility가 낡은 CurrentComboIndex를 읽는다
	// → 서버가 Section_2를 재생하고 그 몽타주가 복제되어, 원격 화면에서만 1타가 2타로 보인다.
	// 발동이 실패해 아래에서 인덱스를 원복하더라도, 모든 활성화 경로가 직전에 절대값을 다시 보내므로
	// 서버 인덱스는 다음 공격 시점에 항상 재동기화된다.
	if (!HasAuthority() && PrevComboIndex != 0)
		Server_SetComboIndex(0); // 인덱스가 실제로 바뀔 때만 전송 (연사 입력 RPC 스팸 방지)

	if (!TryActivateAttack_Impl())
	{
		CurrentComboIndex = PrevComboIndex;
		return false;
	}
	return true;
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

	// 무기 원본(EquipmentComponent::WeaponSlot / EnemyConfig)이 이 시점에 이미 도착해 있을 수 있으므로
	// 무조건 맨손으로 링크하지 않고 현재 값을 존중한다 (풀 방향). 아직 없으면 nullptr = 맨손 레이어 링크.
	ApplyWeaponVisuals(ResolveWeaponDefForVisuals());
}

void ALNPCharacterBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	TickRagdollGravity();
}

void ALNPCharacterBase::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	InitAbilitySystem();
	RefreshWeaponVisuals();
}

void ALNPCharacterBase::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	InitAbilitySystem();
	RefreshWeaponVisuals();
}

void ALNPCharacterBase::RefreshWeaponVisuals()
{
	// ⚠ 반드시 InitAbilitySystem() **뒤에** 호출해야 한다 —
	// InitAbilitySystem()이 CurrentWeaponTag를 Unarmed로 되돌리므로 순서가 뒤바뀌면 무기 태그가 덮인다.
	// 캐시를 무효화해 멱등 조기 반환을 건너뛰고 태그를 다시 부여시킨다.
	CachedWeaponDef = nullptr;
	bWeaponVisualsApplied = false;
	ApplyWeaponVisuals(ResolveWeaponDefForVisuals());
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

void ALNPCharacterBase::ApplyWeaponVisuals(ULNPWeaponData* WeaponData)
{
	// 멱등 조기 반환. 푸시(EquipmentComponent)와 풀(BeginPlay/OnRep_PlayerState)이 양방향으로
	// 들어오므로 중복 호출이 정상인데, 같은 무기로 LinkAnimClassLayers를 다시 부르면
	// 애님 레이어 상태가 리셋되어 포즈가 튄다.
	if (bWeaponVisualsApplied && CachedWeaponDef == WeaponData)
		return;

	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();

	// 조준 모드 전환 여부를 여기서 잡아 둔다 — 아래에서 CurrentAimModeTag를 덮어쓰기 때문.
	const FGameplayTag PrevAimModeTag = CurrentAimModeTag;

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

		// 가드·ADS는 같은 키를 조준 모드로 나눠 쓰므로, 모드가 바뀌면 눌린 입력을 정리해야 한다.
		// 라이플→피스톨처럼 모드가 그대로면 부르지 않는다 (ADS가 끊기면 안 된다).
		if (PrevAimModeTag != NewAimModeTag)
			InputHandlerComponent->NotifyAimModeChanged();
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

	CachedWeaponDef = WeaponData;
	bWeaponVisualsApplied = true;
}

void ALNPCharacterBase::EquipTestWeapon(int32 SlotIndex)
{
	ULNPWeaponData* Target = TestWeaponList.IsValidIndex(SlotIndex) ? TestWeaponList[SlotIndex].Get() : nullptr;
	RequestEquipWeapon(Target);
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

void ALNPCharacterBase::EnterRagdoll(const FVector& PopVelocity)
{
	if (bRagdollActive || VisualMesh == nullptr)
		return;

	if (VisualMesh->GetPhysicsAsset() == nullptr)
	{
		// 바디를 만들 근거가 없으면 아래 호출들이 전부 조용히 no-op이 된다 — 원인을 남긴다.
		UE_LOG(LogLootNPop, Warning, TEXT("EnterRagdoll: '%s' VisualMesh has no PhysicsAsset — ragdoll skipped"),
			*GetNameSafe(this));
		return;
	}

	bRagdollActive = true;

	// 몽타주만 끊는다. ⚠ AnimSourceMesh->SetActive(false)는 금지 — 컴포넌트 틱이 죽으면 포즈 갱신이 끊겨
	// 물리 바디가 없는 본이 굳는다. 어차피 아래에서 bBlendPhysics가 켜지면 애님 결과는 물리에 덮인다.
	if (UAnimInstance* AnimInst = GetAnimInstance())
		AnimInst->Montage_Stop(0.15f);

	// 랙돌이 자기 캡슐에 밀리지 않도록 먼저 끈다.
	if (CapsuleComponent)
		CapsuleComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// ① 콜리전 프로필을 **먼저** 건다. BP의 VisualMesh는 NoCollision이라
	//    UPrimitiveComponent::ShouldCreatePhysicsState()가 false → 물리 바디가 아예 생성돼 있지 않다.
	//    프로필 변경이 EnsurePhysicsStateCreated() → RecreatePhysicsState()로 바디를 만들어 준다.
	//    (순서를 뒤집으면 방금 세운 시뮬 플래그가 재생성에 날아간다 — 기존 코드의 버그였다.)
	CachedVisualMeshProfile = VisualMesh->GetCollisionProfileName();
	VisualMesh->SetCollisionProfileName(TEXT("Ragdoll"));

	// ② 그 다음 시뮬. SetAllBodiesSimulatePhysics가 아니라 SetSimulatePhysics를 써야 한다 —
	//    후자만 bBlendPhysics를 켜서 ShouldBlendPhysicsBones()를 통과시킨다.
	//    전자만 부르면 바디는 돌지만 애님 포즈가 100% 덮어써 랙돌이 눈에 보이지 않는다.
	VisualMesh->SetSimulatePhysics(true);

	// ②-b CCD(연속 충돌 판정) 활성화. Pop 속도 ±2000cm/s면 피직 1스텝(비동기 고정 스텝 기준 최대 33ms)에
	//     수십 cm를 이동한다 — 손·발처럼 작은 바디는 이산 판정이 지면을 통째로 건너뛰어 관통한다(터널링).
	//     CCD는 스텝 시작·끝 사이를 스윕해 첫 접촉을 잡으므로 낙하 속도와 무관하게 막힌다.
	//     ①②로 바디가 생성된 뒤에 불러야 한다 — SetAllUseCCD는 유효한 FBodyInstance에만 적용된다.
	//     ExitRagdoll에서 되돌리지 않는 이유는 그쪽 SetCollisionProfileName 주석 참조.
	VisualMesh->SetAllUseCCD(true);

	// ③ 내장 -Z 중력 차단 — 구형 중력은 TickRagdollGravity가 AddForce로 넣는다 (LootDice와 동일 전략).
	VisualMesh->SetEnableGravity(false);
	VisualMesh->WakeAllRigidBodies();

	// ④ Pop! — Mover 넉백이 아니라 랙돌 바디에 직접 준다. Mover 넉백은 캡슐만 날리고 메시는 제자리에 남는다.
	VisualMesh->SetAllPhysicsLinearVelocity(PopVelocity, /*bAddToCurrent=*/true);
	if (RagdollSpinRadPerSec > 0.f)
		VisualMesh->SetAllPhysicsAngularVelocityInRadians(FMath::VRand() * RagdollSpinRadPerSec);

	if (MoverComponent)
		MoverComponent->EnterDeadMode();
}

void ALNPCharacterBase::ExitRagdoll()
{
	if (!bRagdollActive || VisualMesh == nullptr)
		return;

	bRagdollActive = false;

	VisualMesh->SetSimulatePhysics(false);            // bBlendPhysics도 함께 내려간다
	VisualMesh->SetAllBodiesPhysicsBlendWeight(0.f);  // 잔여 블렌드 가중치 소거
	VisualMesh->SetEnableGravity(true);
	// 보통 NoCollision → ShouldCreatePhysicsState()가 false → TermArticulated()가 모든 FBodyInstance를
	// delete하고 Bodies를 비운다. ⚠ EnterRagdoll의 SetAllUseCCD(true)를 여기서 되돌리지 않는 근거가 이것이다 —
	// bUseCCD는 컴포넌트가 아니라 바디마다 붙어 있고(BodyInstance.cpp:4055), 재진입 시 바디는
	// CopyBodyInstancePropertiesFrom(PhysicsAsset DefaultInstance)로 다시 태어나 CCD가 꺼진 상태에서 시작한다.
	// (시뮬 여부와는 무관한 플래그다 — Chaos는 키네마틱 파티클도 CCD 경로를 탄다.)
	// 즉 이 프로필이 콜리전 있는 것으로 바뀌면 바디가 살아남아 bUseCCD=true가 남는다 — 그때는 명시적 해제가 필요하다.
	VisualMesh->SetCollisionProfileName(CachedVisualMeshProfile);

	if (CapsuleComponent)
		CapsuleComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

	if (MoverComponent)
		MoverComponent->ExitDeadMode();
}

FVector ALNPCharacterBase::GetRagdollAnchorLocation() const
{
	if (bRagdollActive && VisualMesh && VisualMesh->IsSimulatingPhysics())
		return VisualMesh->GetSocketLocation(RagdollAnchorBoneName);

	return GetActorLocation();
}

void ALNPCharacterBase::TickRagdollGravity()
{
	if (!bRagdollActive || VisualMesh == nullptr || GravityComponent == nullptr)
		return;

	// 잠든 바디를 매 틱 깨우지 않는다 — 시체가 영원히 미세 진동하며 잠들지 못한다 (LootDice §2.4와 같은 이유).
	if (!VisualMesh->IsSimulatingPhysics() || !VisualMesh->IsAnyRigidBodyAwake())
		return;

	// bAccelChange=true — Chaos 내장 중력과 동일하게 질량 무관 가속도로 적분된다.
	const FVector GravityAccel = -GetUpDirection() * GravityComponent->GetGravityStrength();
	VisualMesh->AddForceToAllBodiesBelow(GravityAccel, NAME_None, /*bAccelChange=*/true, /*bIncludeSelf=*/true);
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

