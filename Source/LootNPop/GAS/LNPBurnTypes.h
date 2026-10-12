// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "LNPBurnTypes.generated.h"

/**
 * 적 화상 DoT 상태. **적 화상의 유일한 원본이다.**
 *
 * 적 ASC에 주기 GE를 거는 대신 이 프래그먼트를 쓰는 이유: Actor는 LOD에 따라 풀로 반납·재사용되므로
 * ASC에 건 GE는 강등 순간 사라지거나 다음 개체로 새어 나간다. 순수 엔티티에는 ASC 자체가 없다.
 *
 * 규칙 (GameDesign_Ability.md 화상 절)
 *   - 적중마다 확률로 1중첩씩 쌓이고 상한에서 멈춘다. 지속시간은 **모든 중첩이 공유**하며 걸릴 때마다 처음으로 돌아간다.
 *   - 틱 피해 = Stacks × DamagePerStack. 처음 걸릴 때만 틱 위상을 0으로 두고, 갱신은 위상을 건드리지 않는다 —
 *     계속 지지는 동안 틱이 밀려 한 번도 안 터지는 일이 없게 한다.
 *   - DamagePerStack·간격·GE 클래스는 마지막으로 성공한 적용값으로 덮어쓴다 (가해자는 기록하지 않는다).
 *
 * 서버는 ULNPBurnProcessor가 정산한다. 게스트는 `Stacks`만 버블(`FLNPReplicatedAgent::BurnStacks`)로 받아 채운다 —
 * 나머지 필드는 게스트에서 쓰이지 않는다. 같은 아키타입이어야 받을 자리가 있으므로 ULNPEnemyTrait::BuildTemplate가 무조건 붙인다.
 */
USTRUCT()
struct LOOTNPOP_API FLNPBurnFragment : public FMassFragment
{
	GENERATED_BODY()

	uint8 Stacks = 0;

	/** 공유 지속시간 잔여 (초). */
	float TimeRemaining = 0.f;

	/** 마지막 틱 이후 누적 시간 (초). */
	float TickAccum = 0.f;

	float TickInterval   = 0.5f;
	float DamagePerStack = 0.f;

	UPROPERTY()
	UClass* DamageEffectClass = nullptr;   // TObjectPtr는 프래그먼트의 trivially copyable 요건을 깬다
};

namespace LNPBurn
{
	/** `LNP.Burn.Log` — 적용·갱신·틱·만료·사망 정지와 게스트 수신값 변화를 남긴다. */
	LOOTNPOP_API bool IsLogEnabled();

	/**
	 * 적중 1회분의 화상 판정. 판정 Processor의 서버 구역에서 호출한다.
	 * @return 중첩이 걸렸거나 갱신됐으면 true.
	 */
	inline bool TryApply(FLNPBurnFragment* Burn, float Chance, uint8 MaxStacks, float Duration, float TickInterval,
		float DamagePerStack, UClass* DamageEffectClass)
	{
		if (!Burn || MaxStacks == 0 || Duration <= 0.f || TickInterval <= 0.f || FMath::FRand() >= Chance)
			return false;

		if (Burn->Stacks == 0)
			Burn->TickAccum = 0.f;

		Burn->Stacks            = FMath::Min<uint8>(Burn->Stacks + 1, MaxStacks);
		Burn->TimeRemaining     = Duration;
		Burn->TickInterval      = TickInterval;
		Burn->DamagePerStack    = DamagePerStack;
		Burn->DamageEffectClass = DamageEffectClass;
		return true;
	}
}
