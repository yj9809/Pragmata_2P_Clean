// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/WeaponWheelSlotWidget.h"

#include "Components/Image.h"
#include "Components/RichTextBlock.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"

void UWeaponWheelSlotWidget::SetSlotState(const FWeaponWheelSlotState& NewSlotState)
{
	SlotState = NewSlotState;
	SlotState.CurrentAmmo = FMath::Max(0, SlotState.CurrentAmmo);
	SlotState.MaxAmmo = FMath::Max(0, SlotState.MaxAmmo);
	SlotState.bEquipped = SlotState.bHasWeapon && SlotState.bEquipped;

	ApplySlotState();
}

void UWeaponWheelSlotWidget::NativeConstruct()
{
	Super::NativeConstruct();

	ApplySlotState();
}

void UWeaponWheelSlotWidget::ApplySlotState()
{
	if (WeaponIcon)
	{
		if (SlotState.bHasWeapon && SlotState.WeaponIcon)
		{
			WeaponIcon->SetBrushFromTexture(SlotState.WeaponIcon);
			if (!SlotState.WeaponIconSize.IsNearlyZero())
			{
				WeaponIcon->SetBrushSize(SlotState.WeaponIconSize);
				WeaponIcon->SetDesiredSizeOverride(SlotState.WeaponIconSize);
			}
			WeaponIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			WeaponIcon->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	if (AmmoText)
	{
		AmmoText->SetText(GetAmmoText());
		AmmoText->SetVisibility(SlotState.bHasWeapon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (EmptyImage)
	{
		EmptyImage->SetVisibility(SlotState.bHasWeapon ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}

	if (WeaponNameText)
	{
		WeaponNameText->SetText(SlotState.WeaponName);
		WeaponNameText->SetVisibility(SlotState.bHasWeapon && !SlotState.WeaponName.IsEmpty() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (EquippedHighlight)
	{
		EquippedHighlight->SetVisibility(SlotState.bEquipped ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

FText UWeaponWheelSlotWidget::GetAmmoText() const
{
	if (!SlotState.bHasWeapon)
	{
		return FText::GetEmpty();
	}

	if (SlotState.bShowMaxAmmo)
	{
		const FName CurrentAmmoStyleName = GetCurrentAmmoStyleName();
		return FText::Format(
			INVTEXT("<{0}>{1}</><AmmoMax> / {2}</>"),
			FText::FromName(CurrentAmmoStyleName),
			FText::AsNumber(SlotState.CurrentAmmo),
			FText::AsNumber(SlotState.MaxAmmo)
		);
	}

	return FText::Format(
		INVTEXT("<AmmoPartial>{0}</>"),
		FText::AsNumber(SlotState.CurrentAmmo)
	);
}

FName UWeaponWheelSlotWidget::GetCurrentAmmoStyleName() const
{
	if (SlotState.CurrentAmmo <= 1)
	{
		return TEXT("AmmoCritical");
	}

	if (SlotState.bShowMaxAmmo && SlotState.MaxAmmo > 0 && SlotState.CurrentAmmo >= SlotState.MaxAmmo)
	{
		return TEXT("AmmoFull");
	}

	return TEXT("AmmoPartial");
}
