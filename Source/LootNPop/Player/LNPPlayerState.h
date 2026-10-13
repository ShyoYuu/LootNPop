// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystemInterface.h"
#include "Player/LNPTeam.h"
#include "LNPPlayerState.generated.h"

class UAbilitySystemComponent;
class UDataTable;
class ULNPBaseAttributeSet;
class ULNPEquipmentComponent;
class ULNPInventoryComponent;

UCLASS()
class LOOTNPOP_API ALNPPlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()
public:
	ALNPPlayerState();

	virtual void PostInitializeComponents() override;

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UFUNCTION(BlueprintPure, Category = "LNP|Equipment")
	ULNPEquipmentComponent* GetEquipmentComponent() const { return EquipmentComponent; }

	UFUNCTION(BlueprintPure, Category = "LNP|Inventory")
	ULNPInventoryComponent* GetInventoryComponent() const { return InventoryComponent; }

	/** 피격 판정 팀 (Player/LNPTeam.h). */
	uint8 GetTeamId() const { return TeamId; }

	/** 서버 전용. 이미 날아가는 탄·진행 중인 스윙은 발사 시점 팀을 그대로 쓴다 — 바꾼 뒤의 공격부터 적용된다. */
	void SetTeamId(uint8 NewTeamId);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	/** 0(LNPTeam::NPCTeam)은 NPC 전용이라 플레이어는 1 이상이다. 서버만 바꾸고 복제로 퍼진다. */
	UPROPERTY(Replicated)
	uint8 TeamId = LNPTeam::DefaultPlayerTeam;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Abilities", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<ULNPBaseAttributeSet> BaseAttributeSet;

	/**
	 * 기초 스탯 초기값 테이블. 행 구조체는 `AttributeMetaData`이고, 행 이름은
	 * `LNPBaseAttributeSet.<어트리뷰트명>` 형식이어야 한다 (예: `LNPBaseAttributeSet.MaxHealth`).
	 * 비워 두면 ULNPBaseAttributeSet 생성자 기본값을 그대로 쓴다.
	 *
	 * 공격 특화·방어 특화 같은 프리셋은 이 테이블 에셋을 여러 개 만들어 두고
	 * BP_LNPPlayerState 파생 클래스마다 다른 것을 지정하면 된다.
	 *
	 * ⚠️ 엔진이 실제로 읽는 열은 BaseValue 하나뿐이다. MinValue/MaxValue/bCanStack은
	 *    UAttributeSet::InitFromMetaDataTable이 읽지 않으므로 클램프 용도로 믿으면 안 된다.
	 *    값 제한은 ULNPBaseAttributeSet::PreAttributeChange가 담당한다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "LNP|Attributes")
	TObjectPtr<UDataTable> DefaultAttributeTable;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Equipment", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ULNPEquipmentComponent> EquipmentComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Inventory", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ULNPInventoryComponent> InventoryComponent;
};
