// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Item/LNPGameplayTagStack.h"
#include "LNPInventoryItemInstance.generated.h"

class ULNPItemDefinitionBase;

/**
 * 인벤토리 아이템의 런타임 인스턴스 (Lyra ULyraInventoryItemInstance식 하이브리드의 UObject 축).
 *
 * 가방·활성 버프가 모두 이 타입으로 표현되며, ULNPInventoryComponent의 FastArray 리스트에
 * 담겨 등록 서브오브젝트로 소유자-전용 복제된다. 정체성(ItemId)·데이터 주도 스탯(StatTags)을
 * 담아 "같은 DataAsset의 서로 다른 사본"을 구분할 수 있다 (정의 포인터 비교의 모호성 해소).
 *
 * 서버 전용 상태(GAS 이펙트 핸들 등)는 이 인스턴스에 싣지 않는다 — 컴포넌트의 서버 사이드
 * 테이블이 ItemId로 보관한다 (복제 대역폭·보안).
 */
UCLASS(BlueprintType)
class LOOTNPOP_API ULNPInventoryItemInstance : public UObject
{
	GENERATED_BODY()

public:
	ULNPInventoryItemInstance();

	//~UObject 네트워킹
	virtual bool IsSupportedForNetworking() const override { return true; }
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~End

	/** 서버: 스폰 직후 1회 초기화 — Definition 지정 및 ItemId 발급. */
	void Init(ULNPItemDefinitionBase* InDefinition);

	UFUNCTION(BlueprintPure, Category = "LNP|Inventory")
	ULNPItemDefinitionBase* GetDefinition() const { return Definition; }

	const FGuid& GetItemId() const { return ItemId; }

	bool IsEquipped() const { return bEquipped; }

	/** 서버: 장착 상태 변경 + 소유 컴포넌트에 인벤토리 변경 통지(호스트 UI 즉시 갱신). */
	void SetEquipped(bool bInEquipped);

	float GetRemainingDuration() const { return RemainingDuration; }
	void SetRemainingDuration(float InSeconds) { RemainingDuration = InSeconds; }

	// --- 데이터 주도 스탯 (태그스택) ---

	/** 서버 권위. */
	void AddStatTagStack(FGameplayTag Tag, int32 StackCount) { StatTags.AddStack(Tag, StackCount); }
	void RemoveStatTagStack(FGameplayTag Tag, int32 StackCount) { StatTags.RemoveStack(Tag, StackCount); }

	UFUNCTION(BlueprintPure, Category = "LNP|Inventory")
	int32 GetStatTagStackCount(FGameplayTag Tag) const { return StatTags.GetStackCount(Tag); }

	UFUNCTION(BlueprintPure, Category = "LNP|Inventory")
	bool HasStatTag(FGameplayTag Tag) const { return StatTags.ContainsTag(Tag); }

	/** 스탯 스냅샷 읽기/복원 (드랍→픽업 라운드트립용). */
	const FLNPGameplayTagStackContainer& GetStatTags() const { return StatTags; }

private:
	/** 이 인스턴스가 실체화한 아이템 정의 (에셋 참조). */
	UPROPERTY(Replicated)
	TObjectPtr<ULNPItemDefinitionBase> Definition = nullptr;

	/** 인스턴스 고유 식별자 — 장착본/보관본 구분 및 RPC 참조 키. */
	UPROPERTY(Replicated)
	FGuid ItemId;

	/** 데이터 주도 스탯 (레벨·랜덤 스탯 등). GAS 배율 연동은 후속. */
	UPROPERTY(Replicated)
	FLNPGameplayTagStackContainer StatTags;

	/** 현재 장착 중 여부 — 가방 UI는 false만 노출(장착/보관 분리). 변경 시 소유 클라 UI를 재필터한다. */
	UPROPERTY(ReplicatedUsing = OnRep_InstanceChanged)
	bool bEquipped = false;

	/** 버프 인스턴스의 잔여 지속 시간 스냅샷(초). 변경 시에만 갱신, 라이브 카운트다운은 UI 폴리시. */
	UPROPERTY(Replicated)
	float RemainingDuration = 0.0f;

	/** 소유 클라: bEquipped 복제 도착 시 인벤토리 UI를 재필터하도록 통지한다. */
	UFUNCTION()
	void OnRep_InstanceChanged();

	/** 소유 컴포넌트(Outer)의 OnInventoryChanged를 브로드캐스트한다 (서버/클라 공용). */
	void NotifyOwnerInventoryChanged() const;
};
