// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "NativeGameplayTags.h"

// 무기 표현 세트 태그 (ULNPWeaponVisualSet::AnimSetTag) — 무기 식별자가 아니다. 표현을 공유하는 무기는 같은 값을 갖는다
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_VisualSet_Unarmed)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_VisualSet_Pistol)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_VisualSet_Rifle)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_VisualSet_Shotgun)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_VisualSet_LongSword)

// 조준 모드 태그
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_AimMode_None)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_AimMode_FreeAim)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_AimMode_LockOn)

// 어빌리티 히트 이펙트 역할 태그 — AbilityTags에 부여해 ANS/외부 시스템이 어빌리티를 식별하는 데 사용
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ability_HitEffect_Knockback) // 넉백 정보를 제공하는 어빌리티
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ability_HitEffect_Parry)     // 패링 정보를 제공하는 어빌리티
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ability_Reload)              // 재장전 어빌리티 — 경직·무기 교체가 이 태그로 취소한다

// 액션·시스템 제어 태그
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Action_Attacking)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Movement_Jumping)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Block_MovementInput)

// Guard / Parry 상태 태그
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Guarding)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_ParryWindow)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Staggered)   // 경직 중 — 이동·공격 입력을 막고 재경직을 차단한다 (GA_Stagger가 소유)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Reloading)   // 재장전 중 — 기본 공격 발동을 막는다 (GA_Reload가 소유)

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
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_GameplayCue_Character_Stagger)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_GameplayCue_Weapon_Reload)     // 무기 메시 재장전 애니 (WhileActive)

// Guard / Parry / 경직 GameplayEvent 태그 (GA 트리거용)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_GameplayEvent_Parry_Success) // 방어자에게 전송 → GA_ParrySuccess 트리거
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_GameplayEvent_Stagger_Light)  // 그로기 진입 → GA_Stagger 트리거 (패링도 경직도를 거쳐 여기로 들어온다)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_GameplayEvent_Stagger_Heavy)  // 다운(T2 도달) → GA_Stagger 트리거, 고정 시간

// Montage Chooser: 시츄에이션 태그
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Montage_Situation_Attack)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Montage_Situation_HitReaction)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Montage_Situation_ParrySuccess)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Montage_Situation_Block)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Montage_Situation_Dash)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Montage_Situation_Stagger)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Montage_Situation_Reload)

// Montage Chooser: 밸류 태그 — 피격 방향
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Montage_Value_Direction_Front)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Montage_Value_Direction_Back)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Montage_Value_Direction_Left)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Montage_Value_Direction_Right)

// Montage Chooser: 밸류 태그 — 패링 역할
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Montage_Value_Parry_Parrier)  // 패링 성공한 쪽 (방어자). 패링당한 쪽은 경직 시스템이 Value.Stagger.Parried로 처리한다

// Montage Chooser: 밸류 태그 — 경직 단계
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Montage_Value_Stagger_Light)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Montage_Value_Stagger_Heavy)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Montage_Value_Stagger_Parried) // 근접 패링으로 유발된 그로기 — 연출만 갈라 쓴다

// 인벤토리 아이템 스탯 태그 (인스턴스 StatTags 태그스택 키 — 값은 정수 스택 카운트)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Level)  // 아이템 레벨
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_AmmoSpent)  // 탄창에서 쓴 탄 수 — 남은 수가 아니라 소모량이라 부재(0) = 가득이다

// 스탯 GE의 SetByCaller 키 (LNPStat::ApplyModifiers가 스탯별 크기를 주입한다)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_GE_Data_Stat_MaxHealth)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_GE_Data_Stat_AttackPower)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_GE_Data_Stat_AttackSpeed)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_GE_Data_Stat_DefensePower)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_GE_Data_Stat_MoveSpeed)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_GE_Data_Stat_LootSpeed)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_GE_Data_Stat_PoiseResistance)