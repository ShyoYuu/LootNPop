// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "MassObserverProcessor.h"
#include "MassProcessor.h"
#include "Subsystems/WorldSubsystem.h"
#include "LNPLootPodCollisionProxy.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMesh;

/** 이 LootPod의 collision proxy 인스턴스가 이미 추가됐음을 표시한다. */
USTRUCT() struct LOOTNPOP_API FLNPLootPodCollisionProxyTag : public FMassTag { GENERATED_BODY() };

/**
 * LootPod collision proxy(D-047). 엔티티 수명에 묶인 충돌 전용 ISM을 서버·클라이언트가 각자 유지한다.
 * Mass 시각화 ISM과 승격 Actor는 LOD에 따라 생기고 사라지므로 충돌 소스로 쓰지 않는다.
 *
 * ISM은 RemoveAtSwap 모드다. 인스턴스를 제거하면 마지막 인스턴스가 빈 index로 옮겨지므로,
 * index→엔티티 표를 같은 방식으로 갱신하고 Generation을 올린다. exact hit의 Item(instance index)을
 * 엔티티로 해석할 때 Generation으로 표의 시점을 확인한다(D-037).
 * 엔티티 핸들은 머신 로컬 값이다. PodID는 서버에만 있으므로 필요하면 서버가 fragment에서 읽는다.
 */
UCLASS()
class LOOTNPOP_API ULNPLootPodCollisionProxySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	ULNPLootPodCollisionProxySubsystem();

	void AddPod(FMassEntityHandle Entity, const FTransform& PodTransform);
	void RemovePod(FMassEntityHandle Entity);

	/** instance index에 대응하는 Pod 엔티티. 범위 밖이면 무효 핸들. */
	FMassEntityHandle ResolveInstance(int32 InstanceIndex) const;

	/** proxy ISM. 아직 Pod가 없어 만들지 않았으면 nullptr. */
	UInstancedStaticMeshComponent* GetProxyComponent() const { return ProxyComponent; }

	/** index→엔티티 표가 바뀔 때마다 증가한다. */
	uint32 GetGeneration() const { return Generation; }

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	UInstancedStaticMeshComponent* GetOrCreateProxyComponent();

	/** 엔진 BasicShapes Sphere. */
	UPROPERTY()
	TObjectPtr<UStaticMesh> ProxyMesh;

	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> ProxyComponent;

	/** ISM instance index 순서와 같다. */
	TArray<FMassEntityHandle> InstanceToEntity;
	TMap<FMassEntityHandle, int32> EntityToInstance;

	uint32 Generation = 0;
};

/**
 * proxy가 없는 LootPod에 인스턴스를 추가한다. 생성 observer 대신 프로세서를 쓰는 이유는
 * 서버가 엔티티 생성 뒤에 transform을 채우기 때문이다(ULNPMassSpawnSubsystem::SetupSpawnedEntities).
 */
UCLASS()
class LOOTNPOP_API ULNPLootPodCollisionProxyAddProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	ULNPLootPodCollisionProxyAddProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery EntityQuery;
};

/** LootPod 엔티티가 파괴되면 proxy 인스턴스를 제거한다. 서버 Popped 파괴와 클라이언트 복제 제거 모두 이 경로다. */
UCLASS()
class LOOTNPOP_API ULNPLootPodCollisionProxyRemoveObserver : public UMassObserverProcessor
{
	GENERATED_BODY()

public:
	ULNPLootPodCollisionProxyRemoveObserver();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery EntityQuery;
};
