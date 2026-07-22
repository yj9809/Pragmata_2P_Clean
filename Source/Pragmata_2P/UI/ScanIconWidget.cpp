// Fill out your copyright notice in the Description page of Project Settings.


#include "ScanIconWidget.h"

#include "Components/TextBlock.h"

void UScanIconWidget::SetScanIconVisible(bool NewVisible)
{
	const ESlateVisibility NewVisibility = NewVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
	SetVisibility(NewVisibility);
}

void UScanIconWidget::SetDistanceText(const FText& NewText)
{
	if (DistanceText)
	{
		DistanceText->SetText(NewText);
	}
}

void UScanIconWidget::NativeConstruct()
{
	Super::NativeConstruct();
	
	SetVisibility(ESlateVisibility::Collapsed);
}
