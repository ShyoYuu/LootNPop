// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"

class AActor;
class UWorld;

/**
 * 팀 — 누가 누구를 해칠 수 있는가의 단일 출처.
 *
 * 팀은 `uint8` ID다. **0은 NPC 전용이고 NPC는 언제나 한 팀이다.** 플레이어는 1 이상이며 기본값은
 * 모두 `DefaultPlayerTeam`(한 팀 — 플레이어끼리 서로 맞지 않는다). 플레이어 팀은 서버가
 * `ALNPPlayerState`에서 런타임에 바꾸고 복제로 퍼진다(`LNP.Team.Set`).
 *
 * ⚠️ 예전에는 `ELNPInstigatorTeam`(Enemy/Player) 두 값과 `ULNPSettings::bFriendlyFire`로 표현했다.
 *    아군 사격 켜짐은 "플레이어마다 각자 팀", 꺼짐은 "플레이어 전원 한 팀"과 같으므로 팀 배정으로 흡수했다.
 *
 * ⚠️ 지금 팀을 따르는 것은 **피격 판정뿐이다.** 락온 대상·근접 조준 보정·LootPod 협동 루팅·적 HP 바는
 *    아직 "플레이어는 모두 아군"으로 동작한다 (→ Idea_Backlog.md "팀이 다른 플레이어를 적으로 다루기").
 */
namespace LNPTeam
{
	constexpr uint8 NPCTeam           = 0;
	constexpr uint8 DefaultPlayerTeam = 1;

	/**
	 * 공격이 대상에 닿는가. **닿으면 막히고(소멸) 피해를 준다 — 둘은 갈리지 않는다.**
	 * 투사체·폭발·근접·화염·조준점 깊이가 모두 이 하나로 대상 후보를 고른다.
	 *
	 * 자기 자신 제외는 팀 규칙이 아니라 각 판정 루프가 따로 한다 — 다만 자기 자신은 언제나 같은 팀이라
	 * 폭발처럼 자기 제외가 없는 경로도 이 술어만으로 자기를 거른다.
	 *
	 * ⚠️ 예전에는 규칙이 판정 경로마다 if문으로 흩어져 있었고, 서버 투사체만 "적 탄은 다른 적 몸에
	 *    막히지만 피해는 없다"는 예외를 가졌다. 클라 예측은 그 예외를 몰라 게스트 화면의 탄이 적을
	 *    관통했고, 순수 엔티티끼리는 서버 착탄 큐도 나가지 않아 끝내 정정되지 않았다.
	 */
	inline bool CanHit(uint8 AttackerTeam, uint8 TargetTeam)
	{
		return AttackerTeam != TargetTeam;
	}

	/**
	 * 액터의 현재 팀. 플레이어 폰은 PlayerState의 값, 그 밖(적 NPC·null)은 NPCTeam이다.
	 * PlayerState가 아직 붙지 않은 플레이어 폰(클라 접속 직후)은 DefaultPlayerTeam으로 본다 —
	 * NPC로 보면 그 사이 적 탄이 그 플레이어를 통과한다.
	 *
	 * 워커 스레드 판정에서도 부른다. 포인터와 1바이트 값만 읽으며, 판정 프로세서가 이미 같은 경로로
	 * 사망 여부·핑을 읽고 있다.
	 */
	LOOTNPOP_API uint8 GetTeam(const AActor* Actor);

	/**
	 * 이 머신에서 화면을 보는 로컬 플레이어의 팀 — 탄 색처럼 **보는 사람 기준** 연출용이다. 게임 스레드 전용.
	 * 로컬 플레이어가 없으면(데디케이티드 서버) DefaultPlayerTeam.
	 */
	LOOTNPOP_API uint8 GetLocalViewerTeam(const UWorld* World);
}
