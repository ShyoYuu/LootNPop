// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayAbilitySpec.h"
#include "ActiveGameplayEffectHandle.h"
#include "LNPItemInstance.generated.h"

class ULNPWeaponData;
class ULNPSkillData;
class ULNPBuffData;
class ULNPInventoryItemInstance;

/**
 * 무기 장착 슬롯의 내용물. ULNPEquipmentComponent::WeaponSlot이 유일한 인스턴스이며
 * 장비 상태의 복제되는 단일 원본이다 (서버만 쓴다).
 * Definition만 복제되고 나머지는 서버 전용이다 — 아래 각 필드 주석 참조.
 */
USTRUCT(BlueprintType)
struct LOOTNPOP_API FLNPWeaponInstance
{
	GENERATED_BODY()

	/** 복제되는 유일한 필드. 모든 머신에서 "이 캐릭터가 무엇을 들고 있는가"의 정답. */
	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<ULNPWeaponData> Definition = nullptr;

	/**
	 * 이 장착을 유발한 가방 인스턴스 (bag-equipped면 유효, 기본/innate 무기면 null).
	 * NotReplicated — 가방 인스턴스는 COND_OwnerOnly 서브오브젝트라 비소유자 클라이언트에서
	 * 영원히 resolve되지 않는다. 복제하면 Iris가 상태를 계속 dirty로 잡아 재전송이 멈추지 않는다.
	 * 실제로 이 필드를 읽는 곳은 서버의 bEquipped 토글뿐이다.
	 */
	UPROPERTY(NotReplicated)
	TObjectPtr<ULNPInventoryItemInstance> SourceInstance = nullptr;

	/** NotReplicated — 서버 ASC의 스펙 핸들이라 원격에서 해석 불가. */
	UPROPERTY(NotReplicated)
	TArray<FGameplayAbilitySpecHandle> GrantedAbilities;

	/** NotReplicated — 서버 전용 핸들. */
	UPROPERTY(NotReplicated)
	TArray<FActiveGameplayEffectHandle> AppliedEffects;

	bool IsValid() const { return Definition != nullptr; }
	void Reset() { *this = FLNPWeaponInstance(); }
};

USTRUCT(BlueprintType)
struct LOOTNPOP_API FLNPSkillInstance
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<ULNPSkillData> Definition = nullptr;

	/** 이 장착을 유발한 가방 인스턴스 (bag-equipped면 유효). */
	UPROPERTY()
	TObjectPtr<ULNPInventoryItemInstance> SourceInstance = nullptr;

	UPROPERTY()
	TArray<FGameplayAbilitySpecHandle> GrantedAbilities;

	UPROPERTY()
	TArray<FActiveGameplayEffectHandle> AppliedEffects;

	bool IsValid() const { return Definition != nullptr; }
	void Reset() { *this = FLNPSkillInstance(); }
};
