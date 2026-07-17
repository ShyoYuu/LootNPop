// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Interaction/LNPInteractableRegistrySubsystem.h"

void ULNPInteractableRegistrySubsystem::RegisterInteractable(AActor* Actor)
{
	if (Actor != nullptr)
	{
		Interactables.AddUnique(Actor);
	}
}

void ULNPInteractableRegistrySubsystem::UnregisterInteractable(AActor* Actor)
{
	Interactables.Remove(Actor);

	// 파괴 통지를 놓친 stale 엔트리도 이 기회에 정리
	Interactables.RemoveAll([](const TWeakObjectPtr<AActor>& Entry) { return !Entry.IsValid(); });
}
