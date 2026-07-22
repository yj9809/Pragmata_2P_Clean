// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/DamageTextWidget.h"
#include "Components/TextBlock.h"

void UDamageTextWidget::SetDamageText(float Damage)
{
	if (DamageText)
	{
		DamageText->SetText(FText::AsNumber(FMath::RoundToInt(Damage)));
	}
}

void UDamageTextWidget::PlayFloatAndFadeAnimation()
{
	if (FloatAndFade)
	{
		PlayAnimation(FloatAndFade, 0.f, 1, EUMGSequencePlayMode::Forward, 1.f);
	}
}
