// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "LNPGuardParryTypes.generated.h"

/**
 * 패링 가능 엔티티(액터 상태)에 부착하는 Fragment.
 * 현재는 Player 엔티티에 사용; 향후 Enemy Actor 패링 지원 시 Enemy 엔티티에도 추가.
 *
 * bIsParrying / bIsGuarding 관리:
 *   - Player: ULNPInputHandlerComponent::OnGuardStarted/Released 에서 ASC 태그와 동시에 갱신
 *   - Enemy Actor: StateTree 또는 GA에서 갱신 (향후 지원)
 */
USTRUCT()
struct LOOTNPOP_API FLNPParryStateFragment : public FMassFragment
{
	GENERATED_BODY()

	bool  bIsParrying   = false;   // 패링 창 활성 여부 (TAG_State_ParryWindow 미러)
	bool  bIsGuarding   = false;   // 가드 중 여부 (TAG_State_Guarding 미러)
	float ParryAngleCos = 0.707f;  // cos(45°)
	float GuardAngleCos = 0.5f;    // cos(60°)
};
