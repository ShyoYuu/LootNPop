#pragma once

#include "Character/LNPCharacterBase.h"
#include "Components/ActorComponent.h"
#include "Core/CameraEvaluationContext.h"

namespace LNPCamera
{
	/**
	 * 이 평가 컨텍스트를 소유한 LNP 캐릭터를 얻는다. 못 찾으면 nullptr.
	 *
	 * ⚠ **`Context->GetPlayerController()->GetPawnOrSpectator()`를 쓰면 안 된다.**
	 * 2인 PIE 실측(2026-08-21)에서 카메라 컨텍스트가 플레이어당 2개씩 총 4개 만들어지는데,
	 * 그 방식은 각각 자기 폰 / `None` / **다른 플레이어의 폰**을 집어왔다.
	 * 컨트롤러↔폰 관계는 컨텍스트 생성 시점에 확정돼 있지 않다.
	 *
	 * 컨텍스트의 Owner는 `UGameplayCameraComponentBase` 자신이므로
	 * (`GameplayCameraComponentBase.cpp` — `Params.Owner = this`),
	 * 그 컴포넌트의 소유 액터가 곧 이 카메라가 따라다니는 폰이다. 컨텍스트당 정확하다.
	 *
	 * **캐싱하지 말 것.** 캐스트 두 번이라 매 프레임 호출해도 비용이 없고,
	 * `OnInitialize`에서 캐싱하면 리스폰 후 파괴된 폰을 가리킨 채로 남는다.
	 */
	inline const ALNPCharacterBase* ResolveOwningCharacter(
		const TSharedPtr<const UE::Cameras::FCameraEvaluationContext>& Context)
	{
		UObject* ContextOwner = Context ? Context->GetOwner() : nullptr;
		if (const UActorComponent* OwnerComponent = Cast<UActorComponent>(ContextOwner))
			return Cast<ALNPCharacterBase>(OwnerComponent->GetOwner());

		return Cast<ALNPCharacterBase>(ContextOwner);
	}
}
