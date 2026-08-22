// Copyright (c) 2026 LootNPop. All rights reserved.

#include "LNPGameplayTags.h"

UE_DEFINE_GAMEPLAY_TAG(TAG_Weapon_Unarmed,   "LNP.Weapon.Unarmed")
UE_DEFINE_GAMEPLAY_TAG(TAG_Weapon_Pistol,    "LNP.Weapon.Pistol")
UE_DEFINE_GAMEPLAY_TAG(TAG_Weapon_Rifle,     "LNP.Weapon.Rifle")
UE_DEFINE_GAMEPLAY_TAG(TAG_Weapon_Shotgun,   "LNP.Weapon.Shotgun")
UE_DEFINE_GAMEPLAY_TAG(TAG_Weapon_LongSword, "LNP.Weapon.LongSword")

UE_DEFINE_GAMEPLAY_TAG(TAG_AimMode_None,    "LNP.AimMode.None")
UE_DEFINE_GAMEPLAY_TAG(TAG_AimMode_FreeAim, "LNP.AimMode.FreeAim")
UE_DEFINE_GAMEPLAY_TAG(TAG_AimMode_LockOn,  "LNP.AimMode.LockOn")

UE_DEFINE_GAMEPLAY_TAG(TAG_Ability_HitEffect_Knockback, "LNP.Ability.HitEffect.Knockback")
UE_DEFINE_GAMEPLAY_TAG(TAG_Ability_HitEffect_Parry,     "LNP.Ability.HitEffect.Parry")

UE_DEFINE_GAMEPLAY_TAG(TAG_Action_Attacking,    "LNP.Action.Attacking")
UE_DEFINE_GAMEPLAY_TAG(TAG_Movement_Jumping,    "LNP.Movement.Jumping")
UE_DEFINE_GAMEPLAY_TAG(TAG_Block_MovementInput, "LNP.Block.MovementInput")

UE_DEFINE_GAMEPLAY_TAG(TAG_State_Guarding,    "LNP.State.Guarding")
UE_DEFINE_GAMEPLAY_TAG(TAG_State_ParryWindow, "LNP.State.ParryWindow")

UE_DEFINE_GAMEPLAY_TAG(TAG_Block_AttackInput, "LNP.Block.AttackInput")
UE_DEFINE_GAMEPLAY_TAG(TAG_State_ComboWindow, "LNP.State.ComboWindow")

// 주의: GameplayCue 태그는 반드시 "GameplayCue." 루트로 시작해야 한다 (GameplayCueNotify 에셋 태그 매칭 규칙).
UE_DEFINE_GAMEPLAY_TAG(TAG_GameplayCue_Guard_Block,          "GameplayCue.LNP.Guard.Block")
UE_DEFINE_GAMEPLAY_TAG(TAG_GameplayCue_Parry_Success,        "GameplayCue.LNP.Parry.Success")
UE_DEFINE_GAMEPLAY_TAG(TAG_GameplayCue_Character_HitReact,   "GameplayCue.LNP.Character.HitReact")
UE_DEFINE_GAMEPLAY_TAG(TAG_GameplayCue_Melee_Impact,         "GameplayCue.LNP.Melee.Impact")
UE_DEFINE_GAMEPLAY_TAG(TAG_GameplayCue_Projectile_Impact,    "GameplayCue.LNP.Projectile.Impact")
UE_DEFINE_GAMEPLAY_TAG(TAG_GameplayCue_Melee_AttackerHitStop,"GameplayCue.LNP.Melee.AttackerHitStop")

UE_DEFINE_GAMEPLAY_TAG(TAG_GameplayEvent_Parry_Success, "LNP.GameplayEvent.Parry.Success")
UE_DEFINE_GAMEPLAY_TAG(TAG_GameplayEvent_Parry_Stagger, "LNP.GameplayEvent.Parry.Stagger")

UE_DEFINE_GAMEPLAY_TAG(TAG_Montage_Situation_Attack,       "LNP.Montage.Situation.Attack")
UE_DEFINE_GAMEPLAY_TAG(TAG_Montage_Situation_HitReaction,  "LNP.Montage.Situation.HitReaction")
UE_DEFINE_GAMEPLAY_TAG(TAG_Montage_Situation_ParrySuccess, "LNP.Montage.Situation.ParrySuccess")
UE_DEFINE_GAMEPLAY_TAG(TAG_Montage_Situation_Block,        "LNP.Montage.Situation.Block")
UE_DEFINE_GAMEPLAY_TAG(TAG_Montage_Situation_Dash,	       "LNP.Montage.Situation.Dash")

UE_DEFINE_GAMEPLAY_TAG(TAG_Montage_Value_Direction_Front, "LNP.Montage.Value.Direction.Front")
UE_DEFINE_GAMEPLAY_TAG(TAG_Montage_Value_Direction_Back,  "LNP.Montage.Value.Direction.Back")
UE_DEFINE_GAMEPLAY_TAG(TAG_Montage_Value_Direction_Left,  "LNP.Montage.Value.Direction.Left")
UE_DEFINE_GAMEPLAY_TAG(TAG_Montage_Value_Direction_Right, "LNP.Montage.Value.Direction.Right")

UE_DEFINE_GAMEPLAY_TAG(TAG_Montage_Value_Parry_Parrier, "LNP.Montage.Value.Parry.Parrier")
UE_DEFINE_GAMEPLAY_TAG(TAG_Montage_Value_Parry_Parried, "LNP.Montage.Value.Parry.Parried")

UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Level, "LNP.Item.Level")

UE_DEFINE_GAMEPLAY_TAG(TAG_GE_Data_Stat_MaxHealth,    "LNP.GE.Data.Stat.MaxHealth")
UE_DEFINE_GAMEPLAY_TAG(TAG_GE_Data_Stat_AttackPower,  "LNP.GE.Data.Stat.AttackPower")
UE_DEFINE_GAMEPLAY_TAG(TAG_GE_Data_Stat_AttackSpeed,  "LNP.GE.Data.Stat.AttackSpeed")
UE_DEFINE_GAMEPLAY_TAG(TAG_GE_Data_Stat_DefensePower, "LNP.GE.Data.Stat.DefensePower")
UE_DEFINE_GAMEPLAY_TAG(TAG_GE_Data_Stat_MoveSpeed,    "LNP.GE.Data.Stat.MoveSpeed")
UE_DEFINE_GAMEPLAY_TAG(TAG_GE_Data_Stat_LootSpeed,    "LNP.GE.Data.Stat.LootSpeed")