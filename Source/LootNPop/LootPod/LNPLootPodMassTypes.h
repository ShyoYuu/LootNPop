// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "MassEntityTraitBase.h"
#include "LNPLootPodMassTypes.generated.h"

// 1. LootPod 상태 정의
UENUM(BlueprintType)
enum class ELNPLootPodState : uint8
{
	Idle,       // 루팅 존 비활성 — Interaction Input으로만 활성화 가능
	Looting,    // 루팅 존 활성 — 루터가 있으면 게이지 증가, 전원 이탈 시 감쇠
	Popped      // 루팅 완료 (보상 스폰/정리 대기)
};

/** 루팅 존에 루터가 없을 때 게이지가 감쇠하는 속도 — MaxGauge 대비 초당 비율.
 *  이 값 하나로 감쇠 밸런스를 제어하며, 크게(1.0 이상) 올리면 사실상 "즉시 초기화" 룰로 동작한다.
 *  감쇠 끝에 게이지가 0이 되면 루팅 프로세스가 완전 취소된다 — 재개는 Interaction Input부터. */
constexpr float LNPLootPodGaugeDecayFractionPerSecond = 0.15f;

// --- Tag ---
/** Player를 기다리는 LootPod Tag */
USTRUCT() struct LOOTNPOP_API FLNPLootPodIdleTag : public FMassTag { GENERATED_BODY() };

/** 현재 루팅 중인 LootPod Tag */
USTRUCT() struct LOOTNPOP_API FLNPLootPodLootingTag : public FMassTag { GENERATED_BODY() };

/** 루팅 존 활성화 요청 1회성 Tag — Interaction Input 시 부여되고 ULNPIdleToLootingProcessor가 소비한다.
 *  루팅 기여 자체는 프레즌스 기반(FLNPPlayerTag + 존 범위)이므로 이 Tag는 활성화 트리거로만 쓰인다. */
USTRUCT() struct LOOTNPOP_API FLNPPlayerLootingTag : public FMassTag { GENERATED_BODY() };

/** 2. LootPod Data Fragment */
USTRUCT()
struct LOOTNPOP_API FLNPLootPodFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	ELNPLootPodState State = ELNPLootPodState::Idle;

	UPROPERTY(EditAnywhere, Category = "LNP|LootPod")
	float CurrentGauge = 0.0f;

	UPROPERTY(EditAnywhere, Category = "LNP|LootPod")
	float MaxGauge = 100.0f;

	/** 루팅 존 반경² — 게이지 기여·존 사수 판정 범위. ULNPLootPodTrait::LootingZoneRadius에서 주입된다. */
	UPROPERTY(EditAnywhere, Category = "LNP|LootPod")
	float LootableDistSquared = 250000.0f;

	UPROPERTY(EditAnywhere, Category = "LNP|LootPod")
	/**
	 * 보상 조회를 위한 고유 식별자. 트레잇은 템플릿을 만들어 모든 Pod가 공유하므로 여기서 채울 수 없고,
	 * 스폰 시 ULNPMassSpawnSubsystem::SetupSpawnedEntities가 엔티티마다 1부터 발급한다 (0 = 미발급).
	 * 조회가 서버에서만 일어나므로(ALNPLootDice::SpawnPodRewards) 복제하지 않는다.
	 */
	int32 PodID = 0;
};

/** 3. Player 루팅 속도 Fragment — 최초 상호작용 시 부착되어 상주한다. 없는 플레이어는 기본 속도 1.0으로 기여한다. */
USTRUCT()
struct LOOTNPOP_API FLNPPlayerLootingFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "LNP|LootPod")
	float BuffedLootSpeed = 1.0f;
};

// 4. LootPod 식별 Tag
USTRUCT()
struct LOOTNPOP_API FLNPLootPodTag : public FMassTag { GENERATED_BODY() };

class UMassReplicationTrait;

// 5. LootPod Entity Trait (Entity 트레이트)
UCLASS()
class LOOTNPOP_API ULNPLootPodTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	ULNPLootPodTrait();

public:
	/** 루팅 존 반경 (cm) — 게이지 기여·존 사수 판정 범위. Pod 종류별로 EntityConfig에서 조정한다.
	 *  Actor(BP)의 LootingZoneSphere 반경(존 표시·비주얼)과 일치시켜야 한다. */
	UPROPERTY(EditAnywhere, Category = "LNP|LootPod")
	float LootingZoneRadius = 500.0f;

	/** 게이지 총량 — 루팅 속도 합산 1.0 기준 채우는 데 걸리는 초와 같다. Pod 종류별로 EntityConfig에서 조정한다.
	 *  테스트 편의를 위해 10(≈10초)으로 낮춰둠 — 기획 밸런스 확정 시 재조정. */
	UPROPERTY(EditAnywhere, Category = "LNP|LootPod")
	float MaxGauge = 10.0f;

protected:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;

	/** LootPod MassReplication(Phase 7) — BubbleInfoClass/ReplicatorClass를 LNP 전용 클래스로 고정해 내부적으로 위임한다.
	 *  Standalone(NM_Standalone)에서는 UMassReplicationTrait::BuildTemplate 자체가 조기 반환하므로 별도 분기가 필요 없다. */
	UPROPERTY(VisibleAnywhere, Category = "LNP|LootPod", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMassReplicationTrait> ReplicationTrait;
};
