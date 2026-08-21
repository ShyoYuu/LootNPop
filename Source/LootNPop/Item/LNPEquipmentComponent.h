// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Item/LNPItemInstance.h"
#include "LNPEquipmentComponent.generated.h"

class ULNPItemDefinitionBase;
class ULNPWeaponData;
class ULNPSkillData;
class ULNPInventoryItemInstance;
class UAbilitySystemComponent;

/** 무기 슬롯이 바뀌었을 때(서버 적용·클라이언트 OnRep 모두) 브로드캐스트된다. UI 갱신용. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FLNPEquipmentChanged);

/**
 * 장비 상태의 단일 원본. PlayerState에 붙어 있고 WeaponSlot이 복제된다.
 *
 * **쓰기는 서버 권위 전용이다.** 클라이언트는 ALNPPlayerCharacter의 Server_Equip* RPC로
 * 요청만 보내고, 결과는 OnRep_WeaponSlot으로 되돌아온다. 로컬 선반영(예측)은 하지 않는다 —
 * 예측 쓰기와 OnRep 쓰기가 공존하면 두 값이 갈라지는 것이 이 구조 이전의 버그였다.
 *
 * Pawn(ALNPCharacterBase)의 무기 비주얼은 여기서 파생되는 캐시다. PlayerState와 Pawn의
 * 복제 순서는 보장되지 않으므로 푸시(PushWeaponToPawn)와 풀(Pawn의 OnRep_PlayerState/BeginPlay)을
 * 양방향으로 걸어 어느 쪽이 먼저 도착해도 수렴시킨다. 두 방향 모두 멱등이다.
 */
UCLASS(ClassGroup = (LNP), meta = (BlueprintSpawnableComponent))
class LOOTNPOP_API ULNPEquipmentComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	ULNPEquipmentComponent();
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 현재 무기를 해제한 후 WeaponDef를 장착하고 GA/GE를 부여한다 (기본/innate 무기 경로 — 가방 인스턴스 없음). 서버 전용. */
	void EquipWeapon(ULNPWeaponData* WeaponDef);

	/** 가방 인스턴스를 장착한다 — 슬롯이 인스턴스를 참조하고 bEquipped를 표시해 장착본/보관본을 구분한다. 서버 전용. */
	void EquipWeaponInstance(ULNPInventoryItemInstance* Instance);

	/** 서버 전용. */
	void UnequipWeapon();

	/**
	 * 장착 중인 무기의 GAS 부여를 현재 인스턴스 레벨로 다시 건다 (합성으로 레벨이 오른 직후). 서버 전용.
	 * ClearWeaponSlot→Equip 경로를 쓰지 않는 이유: bEquipped가 내려갔다 올라가 UI가 깜빡이고,
	 * EquipWeaponInstance의 "같은 인스턴스면 조기 반환"에 걸려 아무 일도 일어나지 않는다.
	 */
	void RefreshWeaponSlotGrants();

	/**
	 * 무기 슬롯이 비어 있고 Instance가 무기면 자동 장착한다. 서버 전용, 아이템 획득 경로가 호출한다.
	 * 기획상 맨손 상태는 존재하지 않아야 하므로(BeginPlay가 DefaultWeapon을 장착) 안전망 성격이다.
	 * 이미 장착 중이면 아무것도 하지 않는다 — 획득이 현재 장비를 갈아치우면 안 된다.
	 */
	void TryAutoEquipWeapon(ULNPInventoryItemInstance* Instance);

	/**
	 * DefaultWeapon의 가방 인스턴스를 보장하고, 맨손이면 장착한다. 서버 전용. 멱등.
	 *
	 * ⚠ **BeginPlay에서 부르면 안 된다.** 게스트의 PlayerState는 그 시점에 아직 자기 연결로
	 * 복제를 시작하기 전이라, 지금 등록한 서브오브젝트를 참조하는 FastArray 엔트리가
	 * 미해결 포인터인 채로 초기 번치에 실린다 — 엔트리는 도착하지만 `Instance`가 영원히 null이고
	 * FastArray는 "이미 보냄"으로 간주해 재전송하지 않아 게스트 가방이 빈 채로 남는다(2026-08-20 실측).
	 * 그래서 연결이 완전히 성립한 뒤인 `ALNPPlayerCharacter::PossessedBy`에서 호출한다 —
	 * 런타임 루팅과 같은, 검증된 복제 경로를 타게 된다.
	 *
	 * 이미 다른 무기를 장착 중이면 아무것도 하지 않는다 (리스폰 시 기본 무기로 되돌리면 안 된다).
	 */
	void EnsureDefaultWeapon();

	/** 무기 슬롯 변경 통지 (서버·클라이언트 공통). ULNPStatsTabWidget 등 UI가 구독한다. */
	UPROPERTY(BlueprintAssignable, Category = "LNP|Equipment")
	FLNPEquipmentChanged OnEquipmentChanged;

	/** 활성 스킬을 SlotIndex(0부터)에 장착한다. 이전 점유자를 해제한다. */
	UFUNCTION(BlueprintCallable, Category = "LNP|Equipment")
	void EquipActiveSkill(int32 SlotIndex, ULNPSkillData* SkillDef);

	UFUNCTION(BlueprintCallable, Category = "LNP|Equipment")
	void UnequipActiveSkill(int32 SlotIndex);

	/** 패시브 스킬을 추가한다 (슬롯 제한 없음). GA를 즉시 부여한다. */
	UFUNCTION(BlueprintCallable, Category = "LNP|Equipment")
	void AddPassiveSkill(ULNPSkillData* SkillDef);

	UFUNCTION(BlueprintCallable, Category = "LNP|Equipment")
	void RemovePassiveSkill(ULNPSkillData* SkillDef);

	const FLNPWeaponInstance& GetWeaponSlot() const { return WeaponSlot; }
	const TArray<FLNPSkillInstance>& GetActiveSkillSlots() const { return ActiveSkillSlots; }
	int32 GetMaxActiveSkillSlots() const { return MaxActiveSkillSlots; }

	/** ItemDef가 현재 장착 중(무기 슬롯·액티브 스킬 슬롯·패시브 스킬)인지 — 정의 포인터 비교(사본 구분 불가, 레거시). */
	UFUNCTION(BlueprintPure, Category = "LNP|Equipment")
	bool IsEquipped(const ULNPItemDefinitionBase* ItemDef) const;

	/** ItemId(인스턴스)가 현재 장착 중인지 — 사본을 정확히 구분한다 (DA_Pistol 오검출 해소의 핵심). */
	bool IsEquippedInstance(const FGuid& ItemId) const;

	/** BeginPlay 시 장착되는 기본 무기. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LNP|Equipment|Defaults")
	TObjectPtr<ULNPWeaponData> DefaultWeapon;

private:
	UAbilitySystemComponent* GetASC() const;

	/**
	 * 정의가 선언한 어빌리티·이펙트·스탯을 ASC에 건다.
	 * @param Level  아이템(무기) 레벨. 어빌리티 스펙 레벨이자 무기 레벨 테이블의 행 선택 기준이다.
	 *               레벨 개념이 없는 대상(스킬·기본 무기 폴백)은 1을 쓴다.
	 */
	void GrantItemImpl(ULNPItemDefinitionBase* Def,
	                   int32 Level,
	                   TArray<FGameplayAbilitySpecHandle>& OutAbilities,
	                   TArray<FActiveGameplayEffectHandle>& OutEffects);
	void RevokeItemImpl(TArray<FGameplayAbilitySpecHandle>& Abilities,
	                    TArray<FActiveGameplayEffectHandle>& Effects);

	/** 쓰기 진입점이 권위인지 검사한다. 아니면 ensure로 잡고 호출자를 막는다. */
	bool EnsureAuthority(const TCHAR* Context) const;


	/** 현재 슬롯을 비운다 (GAS 회수 + bEquipped 해제). 통지하지 않으므로 교체의 중간 단계로도 쓸 수 있다. */
	void ClearWeaponSlot();

	/** 슬롯 변경 후처리 — 서버의 직접 적용과 클라이언트의 OnRep이 공유하는 유일한 경로. */
	void OnWeaponSlotApplied();

	/** 소유 PlayerState의 Pawn에 무기 비주얼을 밀어 넣는다. Pawn이 아직 없으면 no-op (Pawn 쪽이 풀한다). */
	void PushWeaponToPawn() const;

	UFUNCTION()
	void OnRep_WeaponSlot();

	/** 장비 상태의 단일 원본. 서버만 쓰고 모든 클라이언트에 복제된다. */
	UPROPERTY(ReplicatedUsing = OnRep_WeaponSlot)
	FLNPWeaponInstance WeaponSlot;

	UPROPERTY()
	TArray<FLNPSkillInstance> ActiveSkillSlots;

	UPROPERTY()
	TArray<FLNPSkillInstance> PassiveSkillInstances;

	int32 MaxActiveSkillSlots = 4;
};
