// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/HoldKeyPromptWidget.h"

#include "Components/Image.h"
#include "Engine/Texture2D.h"

void UHoldKeyPromptWidget::SetHoldProgress(float NewProgress)
{
	HoldProgress = FMath::Clamp(NewProgress, 0.f, 1.f);
	ApplyHoldProgress();
}

void UHoldKeyPromptWidget::SetHolding(bool bNewHolding)
{
	if (bHolding == bNewHolding)
	{
		return;
	}

	bHolding = bNewHolding;
	ApplyHoldingState();
	ApplyProhibitedState();
}

void UHoldKeyPromptWidget::SetProhibited(bool bNewProhibited)
{
	if (bProhibited == bNewProhibited)
	{
		return;
	}

	bProhibited = bNewProhibited;
	ApplyProhibitedState();
}

void UHoldKeyPromptWidget::SetKeyTexture(UTexture2D* NewTexture)
{
	KeyTexture = NewTexture;
	ApplyKeyTexture();
}

void UHoldKeyPromptWidget::ResetHoldProgress()
{
	SetHoldProgress(0.f);
	SetHolding(false);
}

void UHoldKeyPromptWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (HoldProgressCircle)
	{
		HoldProgressMaterial = HoldProgressCircle->GetDynamicMaterial();
	}

	// Actor나 Widget Blueprint에서 지정한 키 아이콘을 생성 시점에 반영
	ApplyKeyTexture();
	ApplyHoldProgress();
	ApplyHoldingState();
	ApplyProhibitedState();
}

void UHoldKeyPromptWidget::ApplyHoldProgress()
{
	if (HoldProgressMaterial)
	{
		HoldProgressMaterial->SetScalarParameterValue(ProgressParameterName, HoldProgress);
	}
}

void UHoldKeyPromptWidget::ApplyKeyTexture()
{
	if (KeyBox && KeyTexture)
	{
		KeyBox->SetBrushFromTexture(KeyTexture);
	}
}

void UHoldKeyPromptWidget::ApplyHoldingState()
{
	const FLinearColor NewTint = bHolding ? HoldingTint : IdleTint;

	if (BackgroundCircle)
	{
		BackgroundCircle->SetColorAndOpacity(NewTint);
	}

	if (HoldProgressCircle)
	{
		HoldProgressCircle->SetVisibility(bHolding ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void UHoldKeyPromptWidget::ApplyProhibitedState()
{
	if (Prohibition)
	{
		Prohibition->SetVisibility(bProhibited ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (HoldProgressCircle)
	{
		HoldProgressCircle->SetVisibility(
			!bProhibited && bHolding ? ESlateVisibility::Visible : ESlateVisibility::Collapsed
		);
	}
}
