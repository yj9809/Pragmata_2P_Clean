// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/RecoveryItemContainerWidget.h"

#include "RecoveryItemIcon.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/PanelSlot.h"
#include "Components/PanelWidget.h"

void URecoveryItemContainerWidget::SetRecoveryItemCount(int32 NewCurrentCount, int32 NewMaxCount)
{
	NewMaxCount = FMath::Clamp(NewMaxCount, 0, MaxRecoveryItemSlot);
	NewCurrentCount = FMath::Clamp(NewCurrentCount, 0, NewMaxCount);

	if (CurrentRecoveryItemCount == NewCurrentCount &&
		MaxRecoveryItemCount == NewMaxCount)
	{
		return;
	}

	CurrentRecoveryItemCount = NewCurrentCount;
	MaxRecoveryItemCount = NewMaxCount;

	ApplyRecoveryItemLayout(false);
}

void URecoveryItemContainerWidget::SetCurrentRecoveryItemCount(int32 NewCurrentCount)
{
	NewCurrentCount = FMath::Clamp(NewCurrentCount, 0, MaxRecoveryItemCount);

	if (CurrentRecoveryItemCount == NewCurrentCount)
	{
		return;
	}

	CurrentRecoveryItemCount = NewCurrentCount;

	ApplyRecoveryItemLayout(false);
}

void URecoveryItemContainerWidget::SetMaxRecoveryItemCount(int32 NewMaxCount)
{
	NewMaxCount = FMath::Clamp(NewMaxCount, 0, MaxRecoveryItemSlot);

	if (MaxRecoveryItemCount == NewMaxCount)
	{
		return;
	}

	MaxRecoveryItemCount = NewMaxCount;
	CurrentRecoveryItemCount = FMath::Clamp(CurrentRecoveryItemCount, 0, MaxRecoveryItemCount);

	ApplyRecoveryItemLayout(false);
}

void URecoveryItemContainerWidget::NativeConstruct()
{
	Super::NativeConstruct();

	RebuildRecoveryItemIcons();

	CachedIconState.Init(ERecoveryItemIconState::Hidden, MaxRecoveryItemSlot);

	ApplyRecoveryItemLayout(true);
}

void URecoveryItemContainerWidget::RebuildRecoveryItemIcons()
{
	RecoveryItemIcons.Reset(MaxRecoveryItemSlot);

	if (!RecoveryItemPanel)
	{
		return;
	}

	RecoveryItemPanel->ClearChildren();

	if (!RecoveryItemIconClass)
	{
		return;
	}

	for (int32 Index = 0; Index < MaxRecoveryItemSlot; ++Index)
	{
		URecoveryItemIcon* Icon = CreateWidget<URecoveryItemIcon>(this, RecoveryItemIconClass);
		if (!Icon)
		{
			continue;
		}

		UPanelSlot* AddedSlot = RecoveryItemPanel->AddChild(Icon);

		if (UHorizontalBoxSlot* HorizontalBoxSlot = Cast<UHorizontalBoxSlot>(AddedSlot))
		{
			HorizontalBoxSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			HorizontalBoxSlot->SetPadding(IconPadding);
			HorizontalBoxSlot->SetHorizontalAlignment(HAlign_Center);
			HorizontalBoxSlot->SetVerticalAlignment(VAlign_Center);
		}

		RecoveryItemIcons.Add(Icon);
	}
}

void URecoveryItemContainerWidget::ApplyRecoveryItemLayout(bool bForce)
{
	for (int32 Index = 0; Index < RecoveryItemIcons.Num(); Index++)
	{
		URecoveryItemIcon* Icon = RecoveryItemIcons[Index];
		if (!Icon)
		{
			continue;
		}

		const ERecoveryItemIconState NewState = GetIconStateForIndex(Index);

		if (!bForce && CachedIconState.IsValidIndex(Index) && CachedIconState[Index] == NewState)
		{
			continue;
		}

		CachedIconState[Index] = NewState;
		ApplyIconState(Icon, NewState);
	}
}

void URecoveryItemContainerWidget::ApplyIconState(URecoveryItemIcon* Icon, ERecoveryItemIconState NewState)
{
	if (!Icon)
	{
		return;
	}

	switch (NewState)
	{
	case ERecoveryItemIconState::Hidden:
		Icon->SetVisibility(ESlateVisibility::Collapsed);
		break;

	case ERecoveryItemIconState::Available:
		Icon->SetVisibility(ESlateVisibility::Visible);
		Icon->SetUsed(false);
		break;

	case ERecoveryItemIconState::Used:
		Icon->SetVisibility(ESlateVisibility::Visible);
		Icon->SetUsed(true);
		break;

	default:
		break;
	}
}

ERecoveryItemIconState URecoveryItemContainerWidget::GetIconStateForIndex(int32 Index) const
{
	if (Index >= MaxRecoveryItemCount)
	{
		return ERecoveryItemIconState::Hidden;
	}

	if (Index < CurrentRecoveryItemCount)
	{
		return ERecoveryItemIconState::Available;
	}

	return ERecoveryItemIconState::Used;
}
