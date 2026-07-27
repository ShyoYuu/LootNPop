// Copyright (c) 2026 LootNPop. All rights reserved.

#include "UI/LNPInventoryEntryWidget.h"
#include "Item/LNPInventoryItemInstance.h"
#include "Item/LNPItemDefinitionBase.h"
#include "Item/LNPBuffData.h"
#include "Item/LNPWeaponData.h"
#include "LNPGameplayTags.h"
#include "Character/LNPPlayerCharacter.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Engine/World.h"
#include "TimerManager.h"

void ULNPInventoryEntryWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 버튼 클릭 → 소유 폰의 드랍/장착. 버프 엔트리 BP에는 버튼이 없어 Optional로 null 허용.
	if (DropButton != nullptr)
		DropButton->OnClicked.AddDynamic(this, &ULNPInventoryEntryWidget::OnDropClicked);
	if (EquipButton != nullptr)
		EquipButton->OnClicked.AddDynamic(this, &ULNPInventoryEntryWidget::OnEquipClicked);
}

void ULNPInventoryEntryWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
		World->GetTimerManager().ClearTimer(CountdownTimerHandle);

	Super::NativeDestruct();
}

void ULNPInventoryEntryWidget::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	BoundInstance = Cast<ULNPInventoryItemInstance>(ListItemObject);
	ULNPItemDefinitionBase* Def = BoundInstance ? BoundInstance->GetDefinition() : nullptr;

	if (IconImage != nullptr)
		IconImage->SetBrushFromTexture(Def != nullptr ? Def->Icon.Get() : nullptr, false);

	if (NameText != nullptr)
	{
		// DisplayName 미설정 시 에셋명으로 폴백 — 이름이 빈칸으로 보이지 않게 한다.
		FText Name = (Def != nullptr) ? Def->DisplayName : FText::GetEmpty();
		if (Name.IsEmpty() && Def != nullptr)
			Name = FText::FromName(Def->GetFName());
		NameText->SetText(Name);
	}

	UpdateDetailText();

	// 버프 잔여 시간은 1초마다 다시 그린다 — 복제되는 값은 추가 시점 스냅샷이라 UI가 직접 센다.
	// ListView가 엔트리 위젯을 재사용하므로 항목이 바뀔 때마다 타이머를 다시 건다.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CountdownTimerHandle);

		const bool bIsFiniteBuff = BoundInstance != nullptr && Cast<ULNPBuffData>(Def) != nullptr
			&& BoundInstance->GetRemainingDuration() > 0.0f;
		if (bIsFiniteBuff)
		{
			World->GetTimerManager().SetTimer(CountdownTimerHandle, this,
				&ULNPInventoryEntryWidget::UpdateDetailText, 1.0f, /*bLoop=*/true);
		}
	}
}

void ULNPInventoryEntryWidget::UpdateDetailText()
{
	// DetailText: 버프는 잔여 시간, 그 외는 레벨(있으면). 둘 다 없으면 비운다.
	if (DetailText == nullptr)
		return;

	FText Detail = FText::GetEmpty();
	if (BoundInstance != nullptr)
	{
		if (Cast<ULNPBuffData>(BoundInstance->GetDefinition()) != nullptr)
		{
			// 올림 표시 — 30s에서 시작해 1s까지 세고, 0s 직후 서버가 만료시켜 목록에서 사라진다.
			const int32 Seconds = FMath::CeilToInt(BoundInstance->GetRemainingDurationLive());
			Detail = FText::FromString(FString::Printf(TEXT("%ds"), Seconds));
		}
		else
		{
			const int32 Level = BoundInstance->GetStatTagStackCount(TAG_Item_Level);
			if (Level > 0)
				Detail = FText::FromString(FString::Printf(TEXT("Lv.%d"), Level));
		}
	}
	DetailText->SetText(Detail);
}

void ULNPInventoryEntryWidget::OnDropClicked()
{
	if (BoundInstance == nullptr)
		return;

	if (ALNPPlayerCharacter* Character = Cast<ALNPPlayerCharacter>(GetOwningPlayerPawn()))
		Character->DropItem(BoundInstance->GetItemId());
}

void ULNPInventoryEntryWidget::OnEquipClicked()
{
	if (BoundInstance == nullptr)
		return;

	// 장착은 무기에 한해 동작 (스킬 슬롯 장착 UI는 후속). 그 외 타입이면 무시.
	if (Cast<ULNPWeaponData>(BoundInstance->GetDefinition()) != nullptr)
	{
		if (ALNPPlayerCharacter* Character = Cast<ALNPPlayerCharacter>(GetOwningPlayerPawn()))
			Character->EquipWeaponInstance(BoundInstance);
	}
}
