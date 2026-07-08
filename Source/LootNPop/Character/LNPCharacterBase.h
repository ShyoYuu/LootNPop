// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "AbilitySystemInterface.h"
#include "MassAgentComponent.h"
#include "GameplayTagContainer.h"
#include "HitDetection/LNPProjectileMassTypes.h"
#include "LNPCharacterBase.generated.h"

class UCapsuleComponent;
class USkeletalMeshComponent;
class UAbilitySystemComponent;
class UMassAgentComponent;
class UAnimMontage;
class UGameplayAbility;
class UChooserTable;
class ULNPMontageChooserContext;

class ULNPCharacterMoverComponent;
class ULNPInputHandlerComponent;
class ULNPPawnGravityComponent;
class ALNPLootPod;
class ULNPWeaponData;
class UAbilitySystemComponent;


/**
 * 플레이어·Enemy가 공유하는 캐릭터 Pawn 베이스.
 *
 * GAS 연동(ASC는 PlayerState 소유), 무기 장착·AnimBP 레이어 교체, 콤보 상태 관리,
 * 히트리액트·HitStop·넉백 피드백, 그리고 원거리 Ghost 발사체/패링 반사의 Multicast 재현을 담당한다.
 * 구형 중력(GetUpDirection)과 Mover 기반 이동을 각 컴포넌트에 위임한다.
 * 실제 공격 발동(TryActivateAttack_Impl)은 파생 클래스가 구현한다.
 */
UCLASS(Abstract)
class LOOTNPOP_API ALNPCharacterBase : public APawn, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ALNPCharacterBase(const FObjectInitializer& ObjectInitializer);

	// --- Component 접근자 ---
	UFUNCTION(BlueprintPure, Category = "LNP|Mover")
	ULNPCharacterMoverComponent* GetMoverComponent() const { return MoverComponent; }

	UFUNCTION(BlueprintPure, Category = "LNP|Gravity")
	FVector GetUpDirection() const;

	UFUNCTION(BlueprintPure, Category = "LNP|Movement")
	bool GetFaceMoveDirection() const;

	// IAbilitySystemInterface 구현
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UCapsuleComponent*      GetCapsule()      const { return CapsuleComponent; }
	USkeletalMeshComponent* GetWeaponMesh()   const { return WeaponMesh;       }
	UAnimInstance*          GetAnimInstance() const { return AnimSourceMesh ? AnimSourceMesh->GetAnimInstance() : nullptr; }

	void SetAIMoveInput(FVector InMoveInput);
	void SetAIOrientationIntent(FVector InOrientationIntent);

	/**
	 * 공격 입력 처리 공용 진입점. TAG_Block_AttackInput 체크 후 TryActivateAttack_Impl을 호출한다.
	 * true  = 공격이 발동됨
	 * false = 발동 실패 (블락 구간, GAS 쿨다운 등) — 호출자가 재시도 버퍼를 세울 수 있다.
	 *         블락 구간 + ComboWindow 활성 시에는 false를 반환하면서 콤보 버퍼도 동시에 설정한다.
	 */
	bool TryActivateAttack();

	/** 현재 콤보 인덱스 (0 = 첫 번째 공격 Attack_1). */
	int32 GetCurrentComboIndex() const { return CurrentComboIndex; }

	/** 콤보 인덱스를 MaxComboCount에 맞게 1 증가(순환). */
	void IncrementComboIndex();

	/**
	 * 원격 클라이언트의 콤보 인덱스를 서버에 동기화한다. CurrentComboIndex는 입력을 소유한 머신에서만
	 * 갱신되는데, 서버의 ULNPAbility_MeleeAttack::ActivateAbility가 몽타주 섹션을 서버 측 인덱스로 고르므로
	 * 동기화하지 않으면 서버(와 시뮬레이티드 프록시)는 항상 콤보 1 몽타주만 재생하고 히트 판정 타이밍도 어긋난다.
	 */
	UFUNCTION(Server, Reliable)
	void Server_SetComboIndex(int32 NewComboIndex);

	/** 콤보 상태를 초기값으로 리셋. */
	void ResetCombo();

	/** 피격 방향에 맞는 HitReact 몽타주 섹션을 재생한다. GameplayCue.LNP.Character.HitReact 노티파이에서 호출한다. */
	UFUNCTION(BlueprintCallable, Category = "LNP|Combat")
	void PlayHitReact(FVector HitFromWorldDir);

	/**
	 * MontageChooser를 평가해 조건에 맞는 몽타주를 반환한다.
	 * @param WeaponType     장착 무기 태그 (LNP.Weapon.*)
	 * @param SituationType  상황 태그    (LNP.Montage.Situation.*)
	 * @param Value          세부 값 태그 (LNP.Montage.Value.*, 생략 가능)
	 */
	UFUNCTION(BlueprintCallable, Category = "LNP|Animation")
	UAnimMontage* EvaluateMontage(FGameplayTag WeaponType, FGameplayTag SituationType, FGameplayTag Value) const;
	UAnimMontage* EvaluateMontage(FGameplayTag SituationType, FGameplayTag Value = FGameplayTag()) const;
	bool PlayMontage(FGameplayTag SituationType, FGameplayTag Value = FGameplayTag()) const;

	/** Duration 동안 CustomTimeDilation을 TimeDilation으로 낮춰 HitStop 효과를 준다. GameplayCue.LNP.Character.HitReact 노티파이에서 호출한다. */
	UFUNCTION(BlueprintCallable, Category = "LNP|Combat")
	void ApplyHitStop(float Duration, float TimeDilation = 0.1f);

	/** 클라이언트 예측 판정 전용 로컬 히트 피드백 (공격자 자신의 화면에서만 즉시 재생). 서버 확정 GameplayCue와 별개. */
	void ApplyLocalHitFeedback();

	/** HitFromDirection 방향으로 Strength 크기의 넉백 임펄스를 가한다. */
	void ApplyKnockback(const FVector HitFromDirection, const float Strength);

	/**
	 * 원거리 발사체를 시뮬레이티드 프록시(구경꾼) 화면에도 시각적으로만 재현한다.
	 * 서버에서만(HasAuthority) 호출하며, 발사자 본인 클라이언트와 서버/리슨호스트 자신은
	 * 수신 측에서 스스로 걸러낸다(Multicast_SpawnGhostProjectiles_Implementation 참조).
	 * Reliable — 손실 시 관전자가 발사체를 통째로 못 보게 되는 1회성 존재 이벤트이므로.
	 * UpstreamDelaySeconds: 서버 발신 시점 이전에 이미 흐른 지연(공격자 RTT/2) — 수신자 Dead Reckoning 외삽에 합산.
	 */
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_SpawnGhostProjectiles(FLNPProjectileSharedFragment SharedData, FVector SpawnPos,
		const TArray<FVector>& Velocities, float ProjectileLifetime, ELNPInstigatorTeam InstigatorTeam,
		int32 KeyOrSalvo, int32 InstigatorPlayerID, float UpstreamDelaySeconds);

	/**
	 * 서버 확정 원거리 패링 반사를 "구 Ghost 소멸 + 새 Ghost 스폰"으로 전 클라이언트에 재현한다.
	 * 발사 방송과 달리 아무도 반사를 예측하지 않았으므로 방어자 본인 클라이언트도 스폰한다
	 * (서버/리슨호스트 자신만 수신 측에서 걸러냄). 스폰 자체는 발사 방송과 동일한
	 * ULNPGhostProjectileSubsystem::SpawnSpectatorGhosts 공용 경로를 타므로 Dead Reckoning도 함께 적용된다.
	 */
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_RespawnReflectedGhost(FLNPProjectileSharedFragment SharedData, FVector SpawnPos,
		FVector NewVelocity, float LifetimeRemaining, ELNPInstigatorTeam NewTeam,
		int32 OldInstigatorPlayerID, int32 OldKeyOrSalvo, uint8 OldSpawnIndex,
		int32 NewInstigatorPlayerID, int32 NewKeyOrSalvo);

	/** 이 캐릭터에 현재 장착/설정된 무기 데이터를 반환한다. */
	virtual const ULNPWeaponData* GetActiveWeaponDef() const { return nullptr; }

	/**
	 * 무기를 장착한다. nullptr을 전달하면 맨손 상태로 전환.
	 * - ASC에 무기·조준모드 태그 부여
	 * - VisualMesh에 서브 AnimBP 레이어 연결
	 * - bFaceMoveDirection 자동 설정
	 */
	virtual void EquipWeapon(ULNPWeaponData* WeaponData);

	/** 테스트용: SlotIndex로 TestWeaponList에서 무기 장착. 범위 초과 시 맨손. */
	void EquipTestWeapon(int32 SlotIndex);

protected:
	/** 실제 공격 발동 로직. 서브클래스에서 오버라이드. */
	virtual bool TryActivateAttack_Impl();

	/** 현재 실행 중인 기본 공격 어빌리티를 취소한다. 서브클래스에서 오버라이드. */
	virtual void CancelCurrentAttackAbility() {}
	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCapsuleComponent> CapsuleComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkeletalMeshComponent> AnimSourceMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkeletalMeshComponent> VisualMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mass", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMassAgentComponent> MassAgentComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Movement", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ULNPCharacterMoverComponent> MoverComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ULNPInputHandlerComponent> InputHandlerComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Gravity", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ULNPPawnGravityComponent> GravityComponent;

	/** 맨손 상태에서 사용할 서브 AnimBP 클래스. BP 서브클래스에서 지정. */
	UPROPERTY(EditDefaultsOnly, Category = "LNP|Animation")
	TSubclassOf<UAnimInstance> UnarmedAnimLayerClass;

	/**
	 * 몽타주 선택 테이블. EvaluateMontage()가 평가하는 단일 Chooser Table.
	 * 열 구성: WeaponType(Has Tag) / SituationType(Has Tag) / Value(Has Tag) → UAnimMontage*
	 */
	UPROPERTY(EditDefaultsOnly, Category = "LNP|Animation")
	TObjectPtr<UChooserTable> MontageChooser;

	/** 무기 스켈레탈 메시를 표시하는 컴포넌트. EquipWeapon()이 메시와 소켓을 동적으로 교체. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Weapon", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkeletalMeshComponent> WeaponMesh;

	/** 테스트용 무기 목록. BP에서 슬롯 순서대로 지정. (0=Pistol, 1=Rifle, 2=LongSword) */
	UPROPERTY(EditDefaultsOnly, Category = "LNP|Weapon|Test")
	TArray<TObjectPtr<ULNPWeaponData>> TestWeaponList;

	/** 현재 장착된 무기 데이터 — 서버가 쓰고 클라이언트가 OnRep으로 비주얼을 갱신. */
	UPROPERTY(ReplicatedUsing = OnRep_CurrentWeapon)
	TObjectPtr<ULNPWeaponData> EquippedWeaponData;

	UFUNCTION()
	void OnRep_CurrentWeapon();

	/** 항상 보유할 기본 어빌리티 목록. BP 서브클래스에서 지정. */
	UPROPERTY(EditDefaultsOnly, Category = "LNP|Abilities")
	TArray<TSubclassOf<UGameplayAbility>> DefaultAbilities;

private:
	void InitAbilitySystem();

	FGameplayTag CurrentWeaponTag;
	FGameplayTag CurrentAimModeTag;

	int32 CurrentComboIndex = 0;

	// EvaluateMontage() 호출마다 재할당을 피하기 위해 캐싱
	UPROPERTY(Transient)
	TObjectPtr<ULNPMontageChooserContext> MontageCtx;
};
