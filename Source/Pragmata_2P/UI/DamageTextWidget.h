// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DamageTextWidget.generated.h"

/**
 *
 */
UCLASS()
class PRAGMATA_2P_API UDamageTextWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void SetDamageText(float Damage);
	void PlayFloatAndFadeAnimation();
	protected:
	UPROPERTY(Transient, meta = (BindWidgetAnim))
	class UWidgetAnimation* FloatAndFade;
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	class UTextBlock* DamageText;
};
