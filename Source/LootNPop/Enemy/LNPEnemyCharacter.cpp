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
#include "LootNPop.h"

#include "AbilitySystemComponent.h"
#include "Net/UnrealNetwork.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Blueprint/UserWidget.h"

ALNPEnemyCharacter::ALNPEnemyCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetNetUpdateFrequency(30.f);

	// ⚠️ **Actor 릴러번시는 반드시 버블 컬 거리(ULNPEnemyTrait::ReplicationCullDistance = 12,000)보다
	//    작아야 한다.** 이 Actor는 UMassAgentComponent로 Mass 엔티티에 퍼펫 링크되는데, 엔진은
	//    "Actor가 살아 있는 동안 엔티티는 유효하다"를 전제로 상태 일관성을 검사한다
	//    (UMassAgentComponent::DebugCheckStateConsistency — PuppetPaused 상태에서 엔티티 무효면 assert).
	//    릴러번시가 더 크면 그 사이 구간에서 **엔티티만 버블에서 빠지고 Actor는 남아** 퍼펫 핸들이 뜬다.
	//    엔진 기본값 15,000(150m)을 그대로 두면 120~150m가 정확히 그 구간이 된다.
	//    8,000은 표현 LOD의 Actor 스폰 거리(6,000)와 추격 반경(5,000)을 덮으면서
	//    버블 컬의 히스테리시스 하한(12,000×0.9 = 10,800)보다 충분히 아래다.
	SetNetCullDistanceSquared(8000.f * 8000.f);

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

	// 조준 Pitch는 서버에서만 굴리고 결과를 복제한다 — 게스트가 한 번 더 보간하면 두 화면이 갈라진다.
	if (HasAuthority())
		AimPitchDeg = FMath::FInterpTo(AimPitchDeg, TargetAimPitchDeg, DeltaTime, AimPitchInterpSpeed);

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
	// Actor는 Mass 표현 풀에서 재사용된다 — 직전에 시체였을 수 있으므로 매 활성화마다 되돌린다 (멱등).
	ExitRagdoll();

	if (AnimSourceMesh)
		AnimSourceMesh->SetVisibility(false);

	if (InputHandlerComponent)
	{
		InputHandlerComponent->SetAIMoveInput(FVector::ZeroVector);
		InputHandlerComponent->SetAIOrientationIntent(FVector::ZeroVector);
		// Actor는 표현 풀에서 재사용된다 — 직전 개체의 속도가 남지 않도록 함께 되돌린다.
		InputHandlerComponent->SetAIDesiredSpeed(0.f);
	}

	// 같은 이유로 조준 자세도 되돌린다 — 직전 개체가 위를 겨눈 채 LOD 강등됐다면 그 자세로 등장한다.
	AimPitchDeg = 0.f;
	TargetAimPitchDeg = 0.f;

	if (AttributeSet)
		AttributeSet->SetHealth(InHealth);

	if (MoverComponent)
		MoverComponent->LaunchWithVelocity(InVelocity);

	RefreshHpBar(InHealth, AttributeSet ? AttributeSet->GetMaxHealth() : 0.f);
}

void ALNPEnemyCharacter::TriggerRagdoll()
{
	if (!HasAuthority() || IsRagdollActive())
		return;

	Multicast_TriggerRagdoll(GetUpDirection() * RagdollPopSpeed);
}

void ALNPEnemyCharacter::Multicast_TriggerRagdoll_Implementation(FVector PopVelocity)
{
	// 리슨 서버는 이 구현부가 로컬로도 실행된다 — EnterRagdoll이 멱등이므로 그대로 둔다(호스트 화면에도 보여야 한다).
	// 데디케이티드 서버는 볼 사람이 없으므로 물리 바디 생성 비용을 아낀다.
	if (GetNetMode() == NM_DedicatedServer)
		return;

	if (HpBarComponent)
		HpBarComponent->SetVisibility(false);
	SetLockOnMarkerVisible(false);

	EnterRagdoll(PopVelocity);
}

void ALNPEnemyCharacter::SetLockOnMarkerVisible(bool bVisible)
{
	if (LockOnMarkerComponent)
		LockOnMarkerComponent->SetVisibility(bVisible);
}

void ALNPEnemyCharacter::SetAimTargetLocation(const FVector& InWorldTarget)
{
	// Config가 없으면 무기 어빌리티도 없어 어차피 쏘지 못한다 — 조준할 이유도 없다.
	if (!EnemyConfig)
		return;

	// 조준선의 기준점은 총구가 아니라 **캡슐 중심**이다. 총구 위치는 조준 자세에 따라 움직이므로
	// 총구를 기준으로 각도를 재면 자기참조가 된다. 총구는 캡슐 중심과 거의 같은 높이라
	// 실제 궤적이 어긋나는 양은 무기 그립의 좌우 오프셋뿐이며, 이는 Yaw에서 이미 감수하던 값이다.
	const FVector LocalDir = GetActorTransform().InverseTransformVectorNoScale(InWorldTarget - GetActorLocation());
	TargetAimPitchDeg = FMath::Clamp(static_cast<float>(LocalDir.Rotation().Pitch),
		EnemyConfig->MovementConfig.AimPitchMinDeg,
		EnemyConfig->MovementConfig.AimPitchMaxDeg);
}

void ALNPEnemyCharacter::ClearAimTarget()
{
	TargetAimPitchDeg = 0.f;
}

FRotator ALNPEnemyCharacter::GetBaseAimRotation() const
{
	const FVector LocalAimDir = FRotator(AimPitchDeg, 0.f, 0.f).Vector();
	return GetActorTransform().TransformVectorNoScale(LocalAimDir).Rotation();
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
	DOREPLIFETIME(ALNPEnemyCharacter, AimPitchDeg);
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
	// [HpDebug] 조사용 임시 계측 — 조사 종료 시 제거한다.
	// dist는 이 머신의 로컬 플레이어와의 거리(cm). 릴러번시 반경(cullRadius)과 나란히 보면
	// "갱신이 드문 것"이 빈도 문제인지 경계 문제인지 갈린다.
	float LocalDist = -1.f;
	if (const UWorld* DbgWorld = GetWorld())
	{
		if (const APlayerController* DbgPC = DbgWorld->GetFirstPlayerController())
		{
			if (const APawn* DbgPawn = DbgPC->GetPawn())
				LocalDist = FVector::Dist(DbgPawn->GetActorLocation(), GetActorLocation());
		}
	}
	UE_LOG(LogLootNPop, Log, TEXT("[HpDebug][Attr] frame=%llu t=%.3f auth=%d %s hp=%.1f dist=%.0f (cullRadius=%.0f)"),
		GFrameCounter, GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f,
		HasAuthority() ? 1 : 0, *GetName(), Data.NewValue,
		LocalDist, FMath::Sqrt(GetNetCullDistanceSquared()));

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
