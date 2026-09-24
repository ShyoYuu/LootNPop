// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "DynamicTerrain/LNPPlacementTypes.h"
#include "Mass/EntityHandle.h"
#include "Mass/ExternalSubsystemTraits.h"
#include "Subsystems/WorldSubsystem.h"
#include <atomic>
#include "LNPHitIdentityRegistry.generated.h"

class ULevel;
class UPrimitiveComponent;
struct FHitResult;

/** exact collision source의 수명주기. Terrain Contract의 profile 계열과 같다. Unknown은 미등록 hit(UnknownExactSurface)다. */
enum class ELNPExactSourceLifetime : uint8
{
	Unknown,
	Static,
	Dynamic,
	Destructible,
};

/** exact collision source의 역할 비트. */
namespace ELNPExactSourceRole
{
	inline constexpr uint8 None = 0;
	inline constexpr uint8 Support = 1 << 0;
	inline constexpr uint8 Blocker = 1 << 1;
}

/**
 * worker가 받는 exact hit 의미 결과(D-037). UObject 참조를 담지 않는 POD다.
 * FaceIndex·InstanceIndex는 Phase 4의 face/instance→Layer 표가 소비할 원본 identity로 그대로 전달한다.
 */
struct FLNPExactHitIdentity
{
	ELNPExactSourceLifetime Lifetime = ELNPExactSourceLifetime::Unknown;
	uint8 Roles = ELNPExactSourceRole::None;

	/** 옥탄트 slot. persistent level의 런타임 source는 INDEX_NONE. 마커 요소는 마커의 slot이다. */
	int8 Slot = INDEX_NONE;

	/** 마커로 배치한 요소의 MarkerId. (Slot, MarkerId)가 DynamicSupport ID다. 그 밖의 source는 무효 GUID. */
	FGuid MarkerId;

	/** complex(trimesh) collision의 face. 단순 shape는 INDEX_NONE. */
	int32 FaceIndex = INDEX_NONE;

	/** ISM·HISM instance index. 일반 component는 INDEX_NONE. */
	int32 InstanceIndex = INDEX_NONE;

	/** Mass 엔티티에 묶인 collision proxy의 엔티티(D-047). 머신 로컬 핸들이다. */
	FMassEntityHandle Entity;

	/** 해석에 쓴 snapshot의 generation. */
	uint32 RegistryGeneration = 0;

	bool IsKnown() const { return Lifetime != ELNPExactSourceLifetime::Unknown; }
};

/** component 단위 등록 정보. component 하나가 여러 Layer를 가질 수 있으므로 Layer는 여기 두지 않는다. */
struct FLNPExactSourceEntry
{
	ELNPExactSourceLifetime Lifetime = ELNPExactSourceLifetime::Unknown;
	uint8 Roles = ELNPExactSourceRole::None;
	int8 Slot = INDEX_NONE;
	bool bLootPodProxy = false;
	FGuid MarkerId;

	/** 등록 시점 collision geometry의 원점(구 중심) 최대 거리(cm). world collision envelope의 입력이다. */
	float MaxRadius = 0.f;
};

/**
 * weak component 키를 index·serial만으로 비교한다. 해시·비교 모두 UObject를 역참조하지 않아 worker에서 안전하다.
 * stale 키끼리를 같다고 보는 TWeakObjectPtr::operator==의 동작도 피한다.
 */
struct FLNPExactSourceKeyFuncs : TDefaultMapKeyFuncs<TWeakObjectPtr<UPrimitiveComponent>, FLNPExactSourceEntry, false>
{
	static bool Matches(KeyInitType A, KeyInitType B) { return A.HasSameIndexAndSerialNumber(B); }
	static uint32 GetKeyHash(KeyInitType Key) { return GetTypeHash(Key); }
};

/** 게임 스레드가 만들어 게시하는 불변 registry. 게시 뒤에는 수정하지 않는다. */
struct FLNPHitIdentitySnapshot
{
	TMap<TWeakObjectPtr<UPrimitiveComponent>, FLNPExactSourceEntry, FDefaultSetAllocator, FLNPExactSourceKeyFuncs> Sources;

	/** LootPod proxy ISM의 instance index→엔티티 표 사본. */
	TArray<FMassEntityHandle> LootPodProxyInstances;

	/**
	 * world collision envelope 최대 반지름(cm) — 등록된 source의 MaxRadius 최댓값. 이보다 바깥에는 exact geometry가 없다.
	 * 0이면 아직 source가 없다(옥탄트 생성 전). LootPod proxy는 지면 위라 포함하지 않는다.
	 */
	float WorldEnvelopeRadius = 0.f;

	uint32 Generation = 0;
};

/**
 * exact hit identity registry(D-037).
 *
 * source는 두 경로로 모인다.
 * - 옥탄트 slot Level: 월드 생성 완료 뒤 LNP collision profile로 분류해 일괄 등록한다. 생성이 다시 시작되면 제거한다.
 * - 런타임 source: 소유자가 BeginPlay·EndPlay에서 직접 등록·해제한다(서버 스폰 장치, 이후 동적 지형).
 * LootPod proxy는 ULNPLootPodCollisionProxySubsystem의 Generation 변화를 보고 표를 다시 복사한다.
 *
 * 변경은 dirty로만 표시하고, Tick에서 새 snapshot을 만들어 교체한다. Tickable 오브젝트는 TG_PostPhysics 완료 대기 뒤,
 * TG_PostUpdateWork 전에 실행된다(LevelTick.cpp). Mass phase는 PrePhysics~PostPhysics와 LastDemotable(FrameEnd)에
 * 있으므로 이 교체 시점에는 Mass 프로세서가 돌고 있지 않다. worker는 GetSnapshot으로 받은 불변 snapshot을 락 없이 읽는다.
 */
UCLASS()
class LOOTNPOP_API ULNPHitIdentitySubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * 게임 스레드 전용. component의 현재 collision profile로 분류한다. 분류할 수 없는 profile이면 경고하고 등록하지 않는다.
	 * @param Placement          마커로 배치한 요소의 (slot, MarkerId). hit identity에 그대로 실린다.
	 * @param MaxRadiusOverride  0보다 크면 등록 시점 자세 대신 쓴다. 움직이는 요소는 경로 swept 반지름을 넘긴다.
	 */
	void RegisterRuntimeSource(UPrimitiveComponent* Component, const FLNPPlacementId& Placement = FLNPPlacementId(), float MaxRadiusOverride = 0.f);
	void UnregisterRuntimeSource(UPrimitiveComponent* Component);

	/** 모든 스레드. 게시된 최신 snapshot. */
	TSharedRef<const FLNPHitIdentitySnapshot, ESPMode::ThreadSafe> GetSnapshot() const { return Snapshot; }

	/** 모든 스레드. 해석 실패 시 Lifetime=Unknown을 돌려주고 미해석 counter를 올린다. */
	FLNPExactHitIdentity ResolveHit(const FHitResult& Hit) const;
	static FLNPExactHitIdentity ResolveHit(const FLNPHitIdentitySnapshot& InSnapshot, const FHitResult& Hit);

	uint32 GetUnknownHitCount() const { return UnknownHitCount.load(std::memory_order_relaxed); }

	/** profile 이름을 수명주기·역할로 분류한다. LNPWorldExact에 응답하지 않거나 LNP profile이 아니면 false. */
	static bool ClassifyProfile(FName ProfileName, ELNPExactSourceLifetime& OutLifetime, uint8& OutRoles);

	/**
	 * 게임 스레드 전용. component collision geometry의 원점 최대 거리(cm).
	 * complex trimesh는 정점, 단순 shape는 AABB 꼭짓점, ISM은 instance별 mesh bounds 꼭짓점으로 잰다.
	 * 지각 한 장의 world bounds 꼭짓점은 반지름의 √3배라 안전망으로 쓸 수 없어 정점을 직접 본다.
	 */
	static float ComputeSourceMaxRadius(UPrimitiveComponent& Component);

	// UTickableWorldSubsystem
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Deinitialize() override;

private:
	/** 현재 slot Level 목록이 등록된 것과 다르면 slot source를 다시 모은다. */
	void RefreshSlotSources();
	void Publish();

	using FSourceMap = TMap<TWeakObjectPtr<UPrimitiveComponent>, FLNPExactSourceEntry, FDefaultSetAllocator, FLNPExactSourceKeyFuncs>;

	/** 게임 스레드 전용 작업본. */
	FSourceMap SlotSources;
	FSourceMap RuntimeSources;
	TArray<TWeakObjectPtr<ULevel>> RegisteredSlotLevels;
	uint32 SeenLootPodProxyGeneration = 0;
	bool bDirty = false;

	TSharedRef<const FLNPHitIdentitySnapshot, ESPMode::ThreadSafe> Snapshot = MakeShared<FLNPHitIdentitySnapshot, ESPMode::ThreadSafe>();

	mutable std::atomic<uint32> UnknownHitCount = 0;
};

/** worker가 ResolveHit·GetSnapshot만 호출한다. 게시는 Mass 실행 구간 밖이다. */
template<>
struct TMassExternalSubsystemTraits<ULNPHitIdentitySubsystem> final
{
	enum
	{
		GameThreadOnly = false,
		ThreadSafeWrite = false,
	};
};
