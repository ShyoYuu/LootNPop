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

	/** 복제된 잔여 시간 스냅샷(초). 라이브 표시는 GetRemainingDurationLive를 쓴다. */
	float GetRemainingDuration() const { return RemainingDuration; }

	/** 서버: 잔여 시간 스냅샷 지정 + 카운트다운 기준 시각 갱신. */
	void SetRemainingDuration(float InSeconds);

	/** 카운트다운 기준 시각을 지금(로컬 월드 시간)으로 잡는다 — 소유 클라는 스냅샷 수신 시점에 호출. */
	void MarkDurationStart();

	/**
	 * UI 표시용 라이브 잔여 시간(초) — 스냅샷에서 기준 시각 이후 경과분을 뺀 값.
	 * RemainingDuration <= 0(무한)이거나 기준 시각이 없으면 스냅샷을 그대로 반환한다.
	 * 권위 만료 판정은 서버의 FLNPBuffRuntime이 담당하며, 이 값은 어디까지나 표시용이다.
	 */
	float GetRemainingDurationLive() const;

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

	// --- 아이템 레벨 (TAG_Item_Level 스택의 얇은 래퍼) ---

	/** 아이템 레벨. 레벨이 없는 아이템(버프 등)도 1로 읽힌다 — 0레벨은 의미가 없다. */
	UFUNCTION(BlueprintPure, Category = "LNP|Inventory")
	int32 GetItemLevel() const;

	/** 서버: 레벨을 지정한다 (합성). StatTags는 복제되므로 소유 클라 UI가 따라온다. */
	void SetItemLevel(int32 InLevel);

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

	/**
	 * 서버가 UI에 보여야 할 값을 바꿀 때마다 올리는 카운터.
	 *
	 * 레벨은 이 인스턴스 자신의 StatTags에 있어 소유 컴포넌트의 BagList FastArray 콜백을 울리지 않고,
	 * StatTags 쪽 FastArray는 소유 컴포넌트를 모른다. 그 결과 원격 클라에서 "재료가 사라지는 통지"와
	 * "레벨이 오르는 복제"가 서로 다른 경로로 도착해 순서가 보장되지 않는다.
	 * 이미 UI를 재필터하는 OnRep_InstanceChanged에 같이 태워 레벨 변경도 확실히 통지한다.
	 */
	UPROPERTY(ReplicatedUsing = OnRep_InstanceChanged)
	uint8 ChangeCounter = 0;

	/** 버프 인스턴스의 잔여 지속 시간 스냅샷(초). 변경 시에만 갱신, 라이브 카운트다운은 UI 폴리시. */
	UPROPERTY(Replicated)
	float RemainingDuration = 0.0f;

	/**
	 * 위 스냅샷이 유효해진 로컬 월드 시각(초). 비복제 — 서버는 값 설정 시, 소유 클라는 복제 수신 시
	 * 각자의 시계로 찍는다(시계 동기화 불필요, 오차는 편도 지연 정도로 표시용에 충분).
	 */
	double DurationStartTime = 0.0;

	/** 소유 클라: bEquipped 복제 도착 시 인벤토리 UI를 재필터하도록 통지한다. */
	UFUNCTION()
	void OnRep_InstanceChanged();

	/** 소유 컴포넌트(Outer)의 OnInventoryChanged를 브로드캐스트한다 (서버/클라 공용). */
	void NotifyOwnerInventoryChanged() const;
};
