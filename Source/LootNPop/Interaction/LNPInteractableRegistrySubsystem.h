// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "LNPInteractableRegistrySubsystem.generated.h"

/**
 * 월드에 살아 있는 ILNPInteractable Actor 레지스트리.
 *
 * 상호작용 탐색용 — SmartObject 공간 쿼리는 Mass Representation의 액터 풀링(재사용·텔레포트)과
 * 충돌해 폐기했고(등록 시점 위치가 파티션에 고정됨), 월드 전체 액터 순회(TActorIterator)는 비효율적이라
 * 대상 Actor가 BeginPlay/EndPlay에 스스로 등록/해제하는 목록을 쓴다. 등록 대상은 항상 소수다
 * (High LOD Pod는 근접 시에만 스폰, Dice는 소수·단명, 월드 장치는 수십 개 고정).
 *
 * 타입 분기는 2026-09-18에 ILNPInteractable로 승격됐다 — 그래플 앵커·스프링 런처가 들어오며
 * 대상이 4종이 됐고, 무엇보다 앵커의 탐색 반경(수십 m)이 컴포넌트의 단일 컷과 충돌했다.
 * 저장소는 AActor 그대로 둔다 — 등록/해제 경로를 바꿀 이유가 없고 소비처가 Cast 한 번으로 해결한다.
 */
UCLASS()
class LOOTNPOP_API ULNPInteractableRegistrySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	void RegisterInteractable(AActor* Actor);
	void UnregisterInteractable(AActor* Actor);

	/** 살아 있는 인터랙터블 Actor 목록. 풀에 반납된(Hidden) 액터가 섞여 있을 수 있으므로 사용처에서 걸러낸다. */
	const TArray<TWeakObjectPtr<AActor>>& GetInteractables() const { return Interactables; }

private:
	TArray<TWeakObjectPtr<AActor>> Interactables;
};
