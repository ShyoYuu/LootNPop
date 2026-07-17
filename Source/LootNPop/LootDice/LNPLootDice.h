// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LNPLootDice.generated.h"

class ULNPItemDefinitionBase;
class UStaticMeshComponent;
class UWidgetComponent;
class UMaterialInstanceDynamic;

/**
 * LootDice — 보상 아이템의 월드 실체화 픽업 Actor (아이콘이 그려진 주사위 큐브).
 *
 * LootPod 루팅 완료 보상과 인벤토리 드랍(양도)이 모두 이 Actor로 스폰되며, 생성 이후
 * 규칙(물리·획득·소멸)은 동일하다. LootPod과 달리 Mass 엔티티를 쓰지 않는다 — 소수·단명이고
 * Chaos 리지드바디·페이로드 복제가 필요해 순수 Actor가 적합하다 (TechDesign_LootDice.md §2.1).
 *
 * 물리 동기화: 서버 권위 시뮬 + 표준 ReplicatedMovement(FRepMovement, 각속도 포함 — Iris
 * RepMovementNetSerializer 네이티브 지원). 정지(슬립) 후에는 델타가 없어 트래픽이 자연 소멸하고,
 * 모든 클라이언트가 같은 위치·같은 윗면으로 정지한 Dice를 본다 (분배 논의의 전제).
 */
UCLASS()
class LOOTNPOP_API ALNPLootDice : public AActor
{
	GENERATED_BODY()

public:
	ALNPLootDice();

	/**
	 * 공용 스폰 경로 — LootPod 보상·인벤토리 드랍이 모두 이 함수를 쓴다 (§2.7). 서버 전용.
	 * COND_InitialOnly 페이로드는 스폰 번치에만 실리므로 반드시 Deferred 스폰으로 대입 후 Finish한다.
	 * @param InRemainingDuration  버프 잔여 초 (0 = 신품/풀 지속시간)
	 * @param ImpulseScale         Pop 임펄스 배율 — Pod 보상 1.0, 인벤토리 드랍은 "작은 Pop"(0.4 권장)
	 */
	static ALNPLootDice* SpawnDice(UWorld& World, const FVector& Location, ULNPItemDefinitionBase* Item,
	                               float InRemainingDuration, float ImpulseScale = 1.0f);

	/** LootPod Popped 후처리 — 보상 테이블(LNPSettings)에서 PodID 보상을 조회해 N개 스폰. 서버 전용. */
	static void SpawnPodRewards(UWorld& World, int32 PodID, const FVector& PodLocation);

	/** 획득 가능 판정 — 거리만 체크한다 (주사위는 방향 개념이 없어 LootPod과 달리 각도 체크 없음) */
	bool CanInteract(const APawn* Interactor) const;

	/** 상호작용 프롬프트(키 아이콘) 표시 여부 — 로컬 플레이어의 ULNPInteractionComponent가 호출한다 */
	void SetInteractionPromptVisible(bool bVisible);

	ULNPItemDefinitionBase* GetItemDef() const { return ItemDef; }
	float GetRemainingDuration() const { return RemainingDuration; }

	/** 서버: 획득 확정 여부 — RPC 직렬화가 선착순 1차 방어, 이 플래그가 Destroy 지연 프레임의 2차 방어 */
	bool IsClaimed() const { return bClaimed; }
	void SetClaimed() { bClaimed = true; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 구형 중력 방향 (단위 벡터). 구 내벽 세계는 "바깥쪽 = 아래" (RadialOutward —
	 *  LNPPawnGravityComponent.cpp GetUpDirection과 동일 부호, Origin=ZeroVector) */
	FVector ComputeGravityDir() const;

	/** 소멸 임박 깜빡임 — 클라이언트 로컬 계산 (SpawnServerTime + 수명 상수, 추가 복제 불필요 §2.9) */
	void UpdateExpiryBlink();

	/** 6면 아이콘 MID 구성 — IconTexture·CategoryColor 파라미터. 머티리얼/아이콘 부재 시 조용히 스킵 */
	void SetupIconMaterial();

	// --- 컴포넌트 ---

	/** 루트 = 물리 큐브. 시뮬레이트 중인 PrimitiveComponent가 루트여야 GatherCurrentMovement가
	 *  FRepMovement에 선속도·각속도까지 기록한다 (별도 SceneComponent 루트 금지). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	/** 상호작용 프롬프트 위젯 (스크린 스페이스, 기본 숨김) — WidgetClass 교체로 아트 적용 가능 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UWidgetComponent> InteractionPromptWidget;

	/** 아이콘·카테고리 색 구동용 동적 머티리얼 인스턴스 */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> IconMID;

	// --- 페이로드 (초기 1회 복제 — 스폰 후 불변) ---

	/** 이 Dice가 담고 있는 보상 아이템 정의 (에셋 참조) */
	UPROPERTY(Replicated)
	TObjectPtr<ULNPItemDefinitionBase> ItemDef;

	/** 버프 잔여 지속 시간(초) — 양도 시 유지된다. 0 = 신품(풀 지속시간) */
	UPROPERTY(Replicated)
	float RemainingDuration = 0.0f;

	/** 스폰 시점 서버 시간 — 소멸 임박 깜빡임의 클라이언트 로컬 계산용 */
	UPROPERTY(Replicated)
	float SpawnServerTime = 0.0f;

	// --- 튜닝 (BP CDO로 서버·클라이언트 동일 값 공유 — 복제 불필요) ---

	/** 미획득 시 소멸까지의 수명(초). 물리 동기화 대역폭 상한 + 월드 잔여물 방지 */
	UPROPERTY(EditDefaultsOnly, Category = "LNP|LootDice", meta = (ClampMin = "1.0"))
	float DiceLifetime = 60.0f;

	/** 소멸 전 깜빡임 경고 시간(초) */
	UPROPERTY(EditDefaultsOnly, Category = "LNP|LootDice", meta = (ClampMin = "0.0"))
	float BlinkWarnSeconds = 5.0f;

	/** 깜빡임 1주기(초) — 작을수록 빠르게 깜빡인다. 켜짐/꺼짐은 반주기씩 (소멸 경고 체감 조절) */
	UPROPERTY(EditDefaultsOnly, Category = "LNP|LootDice", meta = (ClampMin = "0.05"))
	float BlinkPeriod = 0.2f;

	/** 획득 허용 최대 거리 (cm) */
	UPROPERTY(EditAnywhere, Category = "LNP|Interaction")
	float InteractionRadius = 250.0f;

	/** 구형 중력 가속도 (cm/s²) — 폰 중력(LNPPawnGravityComponent::GravityStrength)과 일치시켜 낙하 체감 통일 */
	UPROPERTY(EditDefaultsOnly, Category = "LNP|Physics")
	float GravityAccel = 2000.0f;

	/** Pop 임펄스 속도 (cm/s, bVelChange) — 표면 Up 기준 원뿔 내 랜덤 방향 */
	UPROPERTY(EditDefaultsOnly, Category = "LNP|Physics")
	float PopImpulseSpeed = 600.0f;

	/** Pop 임펄스 원뿔 반각 (도) */
	UPROPERTY(EditDefaultsOnly, Category = "LNP|Physics", meta = (ClampMin = "0.0", ClampMax = "89.0"))
	float PopConeHalfAngleDeg = 25.0f;

	/** 스폰 각속도 (rad/s) — 공중에서 아이콘을 인지할 수 없을 만큼 높게 (주사위 굴림 연출 §2.5) */
	UPROPERTY(EditDefaultsOnly, Category = "LNP|Physics")
	float SpinSpeedRad = 20.0f;

	// --- 카테고리 색 (아이콘 머티리얼 CategoryColor 파라미터) ---

	UPROPERTY(EditDefaultsOnly, Category = "LNP|Visuals")
	FLinearColor WeaponCategoryColor = FLinearColor::Red;

	UPROPERTY(EditDefaultsOnly, Category = "LNP|Visuals")
	FLinearColor BuffCategoryColor = FLinearColor::Green;

	UPROPERTY(EditDefaultsOnly, Category = "LNP|Visuals")
	FLinearColor SkillCategoryColor = FLinearColor::Blue;

private:
	/** 서버: 획득 확정 플래그 (비복제 — Destroy 복제가 클라이언트 제거를 담당) */
	bool bClaimed = false;

	/** 깜빡임 토글의 현재 표시 상태 (SetVisibility 중복 호출 방지) */
	bool bBlinkVisible = true;
};
