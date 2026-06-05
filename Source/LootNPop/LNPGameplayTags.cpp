// Copyright (c) 2026 LootNPop. All rights reserved.

#include "LNPGameplayTags.h"

UE_DEFINE_GAMEPLAY_TAG(TAG_Weapon_Unarmed,   "LNP.Weapon.Unarmed")
UE_DEFINE_GAMEPLAY_TAG(TAG_Weapon_Pistol,    "LNP.Weapon.Pistol")
UE_DEFINE_GAMEPLAY_TAG(TAG_Weapon_Rifle,     "LNP.Weapon.Rifle")
UE_DEFINE_GAMEPLAY_TAG(TAG_Weapon_LongSword, "LNP.Weapon.LongSword")

UE_DEFINE_GAMEPLAY_TAG(TAG_AimMode_None,    "LNP.AimMode.None")
UE_DEFINE_GAMEPLAY_TAG(TAG_AimMode_FreeAim, "LNP.AimMode.FreeAim")
UE_DEFINE_GAMEPLAY_TAG(TAG_AimMode_LockOn,  "LNP.AimMode.LockOn")

UE_DEFINE_GAMEPLAY_TAG(TAG_Action_Attacking,    "LNP.Action.Attacking")
UE_DEFINE_GAMEPLAY_TAG(TAG_Movement_Jumping,    "LNP.Movement.Jumping")
UE_DEFINE_GAMEPLAY_TAG(TAG_Block_MovementInput, "LNP.Block.MovementInput")

UE_DEFINE_GAMEPLAY_TAG(TAG_State_Guarding,    "LNP.State.Guarding")
UE_DEFINE_GAMEPLAY_TAG(TAG_State_ParryWindow, "LNP.State.ParryWindow")

UE_DEFINE_GAMEPLAY_TAG(TAG_GameplayCue_Guard_Block,    "GameplayCue.LNP.Guard.Block")
UE_DEFINE_GAMEPLAY_TAG(TAG_GameplayCue_Parry_Success,  "GameplayCue.LNP.Parry.Success")

UE_DEFINE_GAMEPLAY_TAG(TAG_GameplayEvent_Parry_Success, "LNP.GameplayEvent.Parry.Success")
UE_DEFINE_GAMEPLAY_TAG(TAG_GameplayEvent_Parry_Stagger, "LNP.GameplayEvent.Parry.Stagger")