// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/RecoveryItemIcon.h"

#include "Components/Image.h"

void URecoveryItemIcon::SetUsed(bool bNewUsed)
{
	bUsed = bNewUsed;
	
	if (!RecoveryIcon)
	{
		return;
	}
	
	RecoveryIcon->SetColorAndOpacity(bUsed ? UsedTint : AvailableTint);
}

void URecoveryItemIcon::NativeConstruct()
{
	Super::NativeConstruct();
	
	SetUsed(bUsed);
}
