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

// 어빌리티 히트 이펙트 역할 태그 — AbilityTags에 부여해 ANS/외부 시스템이 어빌리티를 식별하는 데 사용
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ability_HitEffect_Knockback) // 넉백 정보를 제공하는 어빌리티
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ability_HitEffect_Parry)     // 패링 정보를 제공하는 어빌리티

// 액션·시스템 제어 태그
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Action_Attacking)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Movement_Jumping)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Block_MovementInput)

// Guard / Parry 상태 태그
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Guarding)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_ParryWindow)

// 공격 입력 제어 태그
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Block_AttackInput)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_ComboWindow)

// Guard / Parry / HitReact / Melee GameplayCue 태그
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_GameplayCue_Guard_Block)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_GameplayCue_Parry_Success)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_GameplayCue_Character_HitReact)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_GameplayCue_Melee_Impact)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_GameplayCue_Projectile_Impact)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_GameplayCue_Melee_AttackerHitStop)

// Guard / Parry GameplayEvent 태그 (GA 몽타주 트리거용)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_GameplayEvent_Parry_Success) // 방어자에게 전송 → GA_ParrySuccess 트리거
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_GameplayEvent_Parry_Stagger) // 공격자에게 전송 → GA_Stagger 트리거

// Montage Chooser: 시츄에이션 태그
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Montage_Situation_Attack)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Montage_Situation_HitReaction)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Montage_Situation_ParrySuccess)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Montage_Situation_Block)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Montage_Situation_Dash)

// Montage Chooser: 밸류 태그 — 피격 방향
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Montage_Value_Direction_Front)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Montage_Value_Direction_Back)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Montage_Value_Direction_Left)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Montage_Value_Direction_Right)

// Montage Chooser: 밸류 태그 — 패링 역할
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Montage_Value_Parry_Parrier)  // 패링 성공한 쪽 (방어자)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Montage_Value_Parry_Parried)  // 패링 당한 쪽 (원래 공격자)

// 인벤토리 아이템 스탯 태그 (인스턴스 StatTags 태그스택 키 — 값은 정수 스택 카운트)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Level)  // 아이템 레벨

// 스탯 GE의 SetByCaller 키 (LNPStat::ApplyModifiers가 스탯별 크기를 주입한다)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_GE_Data_Stat_MaxHealth)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_GE_Data_Stat_AttackPower)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_GE_Data_Stat_AttackSpeed)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_GE_Data_Stat_DefensePower)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_GE_Data_Stat_MoveSpeed)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_GE_Data_Stat_LootSpeed)