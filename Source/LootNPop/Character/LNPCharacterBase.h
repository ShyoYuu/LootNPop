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
class UMotionWarpingComponent;
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

	/** 근접 공격 보정용 모션 워핑 컴포넌트. Mover가 InitializeComponent에서 어댑터를 자동 연결한다. */
	UFUNCTION(BlueprintPure, Category = "LNP|Combat")
	UMotionWarpingComponent* GetMotionWarpingComponent() const { return MotionWarpingComponent; }

	UFUNCTION(BlueprintPure, Category = "LNP|Gravity")
	FVector GetUpDirection() const;

	UFUNCTION(BlueprintPure, Category = "LNP|Movement")
	bool GetFaceMoveDirection() const;

	/**
	 * ADS(정조준) 중인가. 카메라 리그 디렉터(CDE_ThirdPerson)와 조준 감도가 읽는다.
	 *
	 * 로컬 전용 상태다 — 카메라는 각자의 머신에서만 렌더되므로 복제하지 않는다.
	 * 대시·질주 차단은 시뮬레이션 판정이라 별도로 InputCmd(FLNPModifierInputs::bWantsToADS)를 탄다.
	 * 입력 핸들러가 없는 적 NPC는 항상 false.
	 */
	UFUNCTION(BlueprintPure, Category = "LNP|Camera")
	bool IsADSActive() const;

	// IAbilitySystemInterface 구현
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UCapsuleComponent*      GetCapsule()      const { return CapsuleComponent; }
	USkeletalMeshComponent* GetWeaponMesh()   const { return WeaponMesh;       }
	UAnimInstance*          GetAnimInstance() const { return AnimSourceMesh ? AnimSourceMesh->GetAnimInstance() : nullptr; }

	/** 방향만 담는다 — 단위 벡터 또는 영벡터. 속도는 SetAIDesiredSpeed로 (사유: 입력 핸들러 주석). */
	void SetAIMoveInput(FVector InMoveInput);
	void SetAIOrientationIntent(FVector InOrientationIntent);
	void SetAIDesiredSpeed(float InSpeed);

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

	/**
	 * 현재 실행 중인 기본 공격 어빌리티를 취소한다. 서브클래스에서 오버라이드.
	 * public인 이유: 경직 발동(FLNPStaggerCommand)이 진행 중인 공격을 끊는 데 쓴다 —
	 * 몽타주만 덮어쓰면 GAS 상태가 남아 콤보·쿨다운이 어긋난다.
	 */
	virtual void CancelCurrentAttackAbility() {}

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
	 * 물리 랙돌로 전환하고 PopVelocity(cm/s)만큼 전 바디를 띄운다. 멱등 — 여러 번 불러도 안전.
	 *
	 * **각 머신이 로컬로 부르는 연출이다.** 물리 결과는 복제하지 않으므로 위치가 머신마다 갈라진다.
	 * 호출 순서에 의미가 있다 — 구현부 주석 참조.
	 */
	void EnterRagdoll(const FVector& PopVelocity);

	/** 랙돌을 되돌려 애니메이션 구동 상태로 복귀시킨다. 멱등 — Mass 표현 풀 재사용 경로가 매번 부른다. */
	void ExitRagdoll();

	bool IsRagdollActive() const { return bRagdollActive; }

	/** 랙돌 기준 본의 월드 위치. 랙돌이 아니면 액터 위치를 반환한다. (사망 카메라 추적용) */
	FVector GetRagdollAnchorLocation() const;

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
	 * 무기 장착을 **요청**한다 (의도 전달). nullptr이면 맨손.
	 * 실제 상태 변경은 서버 권위 경로가 수행하며, 비주얼은 그 결과가 되돌아올 때 갱신된다.
	 * 베이스는 no-op — 상태를 소유한 서브클래스가 오버라이드한다.
	 */
	virtual void RequestEquipWeapon(ULNPWeaponData* WeaponData) {}

	/**
	 * 파생 비주얼 상태를 적용한다. nullptr이면 맨손 상태로 전환.
	 * - ASC에 무기·조준모드 태그 부여
	 * - VisualMesh에 서브 AnimBP 레이어 연결
	 * - WeaponMesh 어태치 / bFaceMoveDirection 설정
	 *
	 * 원본(ULNPEquipmentComponent::WeaponSlot 또는 EnemyConfig)이 아니라 그 파생 **캐시**를 채운다.
	 * 멱등이므로 도착 순서를 모르는 여러 지점에서 마음대로 호출해도 된다.
	 * public인 이유: 원본을 소유한 ULNPEquipmentComponent가 Pawn에 밀어 넣는다.
	 */
	void ApplyWeaponVisuals(ULNPWeaponData* WeaponData);

	/**
	 * 테스트용: SlotIndex로 TestWeaponList에서 무기 장착 요청. 범위 초과 시 맨손.
	 * 플레이어는 가방에 해당 무기가 있으면 그 인스턴스를, 없으면 지급받은 새 인스턴스를 장착한다
	 * (ALNPPlayerCharacter::EquipWeaponOnServer) — 디버그 경로도 메뉴 장착과 같은 상태 기계를 탄다.
	 */
	void EquipTestWeapon(int32 SlotIndex);

protected:
	/**
	 * 이 캐릭터의 무기 원본을 조회한다 — 비주얼 재적용(풀 방향)에 쓴다.
	 * 플레이어는 EquipmentComponent::WeaponSlot, 적은 EnemyConfig에서 읽는다.
	 */
	virtual ULNPWeaponData* ResolveWeaponDefForVisuals() const { return nullptr; }

	/** 원본에서 무기를 다시 읽어 비주얼을 강제 재적용한다 (풀 방향). InitAbilitySystem() 뒤에서만 호출할 것. */
	void RefreshWeaponVisuals();

	/** 실제 공격 발동 로직. 서브클래스에서 오버라이드. */
	virtual bool TryActivateAttack_Impl();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Combat", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMotionWarpingComponent> MotionWarpingComponent;

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

	/**
	 * 현재 비주얼에 반영된 무기 데이터 — **파생 캐시이며 복제되지 않는다.**
	 * 원본은 ULNPEquipmentComponent::WeaponSlot(플레이어) / EnemyConfig(적)이다.
	 * ApplyWeaponVisuals()의 멱등 조기 반환 판정에만 쓴다.
	 */
	UPROPERTY()
	TObjectPtr<ULNPWeaponData> CachedWeaponDef;

	/** ApplyWeaponVisuals()가 한 번이라도 돌았는지 — 최초 맨손 레이어 링크를 조기 반환이 삼키지 않도록. */
	bool bWeaponVisualsApplied = false;

	/** 항상 보유할 기본 어빌리티 목록. BP 서브클래스에서 지정. */
	UPROPERTY(EditDefaultsOnly, Category = "LNP|Abilities")
	TArray<TSubclassOf<UGameplayAbility>> DefaultAbilities;

	/** 랙돌 위치의 기준이 되는 본 (카메라 추적·앵커 조회). */
	UPROPERTY(EditDefaultsOnly, Category = "LNP|Death")
	FName RagdollAnchorBoneName = TEXT("pelvis");

	/** 랙돌 진입 시 부여할 랜덤 축 각속도 (rad/s). 0이면 텀블링 없음. */
	UPROPERTY(EditDefaultsOnly, Category = "LNP|Death")
	float RagdollSpinRadPerSec = 6.0f;

private:
	void InitAbilitySystem();

	/** 랙돌 바디에 구형 중력을 매 틱 주입한다 (Chaos는 프로젝트의 커스텀 중력을 모른다). */
	void TickRagdollGravity();

	/** 랙돌 진입 전 VisualMesh의 콜리전 프로필 — ExitRagdoll 복원용. */
	FName CachedVisualMeshProfile = NAME_None;

	bool bRagdollActive = false;

	FGameplayTag CurrentWeaponTag;
	FGameplayTag CurrentAimModeTag;

	int32 CurrentComboIndex = 0;

	// EvaluateMontage() 호출마다 재할당을 피하기 위해 캐싱
	UPROPERTY(Transient)
	TObjectPtr<ULNPMontageChooserContext> MontageCtx;
};
