// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Character/LNPCharacterBase.h"
#include "LNPPlayerCharacter.generated.h"

class UGameplayCameraComponent;
class ULNPItemDefinitionBase;
class ULNPInventoryComponent;
class ULNPInventoryItemInstance;
class ULNPInteractionComponent;
class ULNPLockOnComponent;
class ULNPControlRotationComponent;

/**
 * Player가 조종하는 캐릭터 클래스.
 */
UCLASS()
class LOOTNPOP_API ALNPPlayerCharacter : public ALNPCharacterBase
{
	GENERATED_BODY()

public:
	ALNPPlayerCharacter(const FObjectInitializer& ObjectInitializer);
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;

	virtual void RequestEquipWeapon(ULNPWeaponData* WeaponData) override;
	virtual const ULNPWeaponData* GetActiveWeaponDef() const override;

	/**
	 * 시선 회전(에임) 반환. 로컬 제어 폰은 기존처럼 Control Rotation을 쓰고,
	 * 서버의 원격 폰과 시뮬레이티드 프록시는 Mover InputCmd에 실려 온 소유 클라이언트의
	 * ControlRotation을 사용한다 — 원거리 발사 피치와 Aim Offset(상체 자세) 동기화의 단일 데이터 소스.
	 * (서버: Network Prediction 입력 복제 / 프록시: bSyncInputsForSimProxy로 SyncState에 동봉)
	 */
	virtual FRotator GetBaseAimRotation() const override;

	UFUNCTION(BlueprintPure, Category = "LNP|Interaction")
	TArray<AActor*> GetInteractionCandidates() const;

	UFUNCTION(BlueprintPure, Category = "LNP|Interaction")
	AActor* GetInteractionCandidate() const;

	/** 아이템 인스턴스를 캐릭터 전방에 LootDice로 드랍한다 (파티원 양도용). UI 진입점 — ItemId로 대상 지정. */
	void DropItem(const FGuid& ItemId);

	/** 가방 무기 인스턴스 장착을 요청한다 (UI 진입점). 서버 권위 — 결과는 WeaponSlot 복제로 되돌아온다. */
	void RequestEquipWeaponInstance(ULNPInventoryItemInstance* Instance);

	/** 같은 종류·같은 레벨 재료를 소모해 이 아이템의 레벨을 올린다 (UI 진입점). 서버 권위. */
	void RequestMergeItem(const FGuid& ItemId);

	bool IsDead() const { return bIsDead; }

protected:
	virtual void Tick(float DeltaSeconds) override;
	virtual bool TryActivateAttack_Impl() override;
	virtual void CancelCurrentAttackAbility() override;
	virtual ULNPWeaponData* ResolveWeaponDefForVisuals() const override;

	/** 서버 전용: LootSpeed Attribute 값을 플레이어 Mass 엔티티의 FLNPPlayerLootingFragment에 반영한다.
	 *  버프 GE가 Attribute를 변조하면 델리게이트(PossessedBy에서 바인딩)를 통해 자동 호출된다. */
	void PushLootSpeedToEntity(float NewLootSpeed);

	/** 원격 클라이언트의 정의 기반 장착(디버그 키)을 서버에 전달한다. 검증은 EquipWeaponOnServer가 수행. */
	UFUNCTION(Server, Reliable)
	void Server_EquipWeapon(ULNPWeaponData* WeaponData);

	/**
	 * 서버 전용: 정의로 장착한다 — 가방에서 같은 정의의 인스턴스를 찾고, 없으면 지급 후 장착한다.
	 * **보유하지 않은 무기는 `TestWeaponList`에 있을 때만 지급한다** — 이 검증이 없으면
	 * 클라이언트가 임의의 `ULNPWeaponData` 에셋을 지목해 장착할 수 있다.
	 * nullptr이면 맨손으로 전환한다.
	 */
	void EquipWeaponOnServer(ULNPWeaponData* WeaponData);

	/** 원격 클라이언트의 가방 무기 장착을 서버에 전달한다 (ItemId로 인스턴스 조회). */
	UFUNCTION(Server, Reliable)
	void Server_EquipWeaponInstance(FGuid ItemId);

	/** 원격 클라이언트의 아이템 드랍을 서버에 전달한다 — 검증(미장착·보유)은 서버가 수행한다. */
	UFUNCTION(Server, Reliable)
	void Server_DropItem(FGuid ItemId);

	/** 서버 전용: 인스턴스 조회·미장착 검증 → 인벤토리 제거 → LootDice 스폰 (제거 성공 전 스폰 금지). */
	void DropItemOnServer(const FGuid& ItemId);

	/** 원격 클라이언트의 합성 요청을 서버에 전달한다 — 재료·최대 레벨 검증은 서버가 다시 한다. */
	UFUNCTION(Server, Reliable)
	void Server_MergeItem(FGuid ItemId);

	/** 서버 전용: 소유 인벤토리에서 ItemId를 조회해 합성한다. */
	void MergeItemOnServer(const FGuid& ItemId);

	/**
	 * 서버 전용: HP가 0 이하로 떨어진 최초 시점 1회. 아이템 전량 드랍 → 연출 방송 → 리스폰 예약.
	 * bIsDead 가드로 중복 진입을 막는다 (한 프레임에 여러 피해가 겹칠 수 있다).
	 */
	void HandleDeathOnServer();

	/**
	 * 사망 연출을 전 머신에 재현한다 — 랙돌·입력 차단·카메라 전환. 물리는 각 머신 로컬 시뮬이다.
	 * Reliable — 사망은 1회성 상태 전이라 유실되면 그 화면에서만 캐릭터가 계속 서 있게 된다
	 * (Multicast_SpawnGhostProjectiles와 같은 판단).
	 * 서버 권위 처리(드랍·타이머)는 HandleDeathOnServer에만 두어 리슨 서버 중복 실행을 원천 차단한다.
	 */
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_OnDeath(FVector PopVelocity);

	/** 서버 전용: 가방(장착본 포함)과 활성 버프를 전부 LootDice로 스폰한다. */
	void DropAllItemsOnDeath();

	/**
	 * 서버 전용 공용 경로: 인스턴스를 인벤토리에서 제거한 뒤 LootDice로 스폰한다.
	 * ⚠ 페이로드(정의·레벨)는 **제거 전에** 읽어야 한다 — 제거하면 인스턴스가 사라진다.
	 * 제거에 실패하면 스폰하지 않는다 (아이템 복제 방지).
	 */
	void RemoveAndSpawnDice(ULNPInventoryComponent& Inventory, const FGuid& ItemId, bool bIsBuff,
	                       const FVector& Location, float ImpulseScale);

	/** 로컬 제어 클라이언트 전용: 카메라를 폰 계층에서 떼어 랙돌을 따라가게 한다. */
	void BeginDeathCameraFollow();

	/** 사망 카메라를 랙돌 앵커로 보간 이동시킨다. Tick에서 호출. */
	void TickDeathCameraFollow(float DeltaSeconds);

	/** 사망 시 표면 Up 방향으로 부여할 Pop 속도 (cm/s). */
	UPROPERTY(EditDefaultsOnly, Category = "LNP|Death")
	float DeathPopSpeed = 2000.f;

	/** 사망 카메라가 랙돌을 따라가는 추종 속도. 클수록 즉각적. */
	UPROPERTY(EditDefaultsOnly, Category = "LNP|Death")
	float DeathCameraFollowSpeed = 8.f;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UGameplayCameraComponent> GameplayCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Interaction", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ULNPInteractionComponent> InteractionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ULNPLockOnComponent> LockOnComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ULNPControlRotationComponent> ControlRotationComponent;

	/** 서버 권위 사망 처리(드랍·타이머)의 1회 가드. 클라이언트에서는 방송 수신 표시로도 쓴다. */
	bool bIsDead = false;

	/** 사망 연출(랙돌·입력 차단·카메라)의 멱등 가드 — 서버에서 bIsDead와 별개여야 한다. */
	bool bDeathFxPlayed = false;

	bool bDeathCameraFollowActive = false;
	FVector DeathCameraSmoothedLocation = FVector::ZeroVector;
};
