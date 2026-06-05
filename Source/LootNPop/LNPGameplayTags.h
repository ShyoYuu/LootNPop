// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "NativeGameplayTags.h"

// 무기 장착 태그
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Weapon_Unarmed)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Weapon_Pistol)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Weapon_Rifle)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Weapon_LongSword)

// 조준 모드 태그
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_AimMode_None)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_AimMode_FreeAim)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_AimMode_LockOn)

// 액션·시스템 제어 태그
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Action_Attacking)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Movement_Jumping)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Block_MovementInput)

// Guard / Parry 상태 태그
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Guarding)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_ParryWindow)

// Guard / Parry GameplayCue 태그
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_GameplayCue_Guard_Block)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_GameplayCue_Parry_Success)

// Guard / Parry GameplayEvent 태그 (GA 몽타주 트리거용)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_GameplayEvent_Parry_Success) // 방어자에게 전송 → GA_ParrySuccess 트리거
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_GameplayEvent_Parry_Stagger) // 공격자에게 전송 → GA_Stagger 트리거
