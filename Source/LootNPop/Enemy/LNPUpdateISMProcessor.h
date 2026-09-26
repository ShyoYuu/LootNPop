// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "LNPUpdateISMProcessor.generated.h"

/**
 * 엔진 `UMassUpdateISMProcessor`의 대체 — ISM 표현 개체의 transform을 인스턴스 배치에 쌓는다.
 * 엔진 프로세서는 `DefaultMass.ini`에서 끈다.
 *
 * 대체하는 이유는 **인스턴스별 커스텀 데이터** 하나다. 커스텀 데이터는 한 ISM의 모든 인스턴스에 transform과 **같은 순서로**
 * 넣어야 하는데(`FMassLODSignificanceRange::AddBatchedCustomData` 주석), 그 순서는 transform을 쌓는 루프 안에서만 보장된다.
 * 별도 프로세서가 같은 청크를 다시 돌며 붙이면 청크 순서가 조금만 달라져도 값이 엉뚱한 인스턴스에 들어간다.
 *
 * 커스텀 데이터를 붙이는 개체:
 * - 비행 적(`FLNPEnemyFlyingTag`): [0] 센서 발광(살아 있으면 1, 행동 상태 `Dying`이면 0). 게스트는 복제된 행동 상태를 쓰므로
 *   네트워크 비용이 없다. 머티리얼 `M_EnemyDroneEye`가 `PerInstanceCustomData[0]`을 발광에 곱한다.
 *
 * 그 밖의 개체는 엔진 프로세서와 똑같이 transform만 쌓는다.
 */
UCLASS()
class LOOTNPOP_API ULNPUpdateISMProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	ULNPUpdateISMProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery EntityQuery;
};
