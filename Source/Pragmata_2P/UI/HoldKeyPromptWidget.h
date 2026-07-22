// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HoldKeyPromptWidget.generated.h"

class UImage;
class UMaterialInstanceDynamic;
class UTexture2D;

/**
 *
 */
UCLASS()
class PRAGMATA_2P_API UHoldKeyPromptWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Pragmata|Hold Prompt")
	void SetHoldProgress(float NewProgress);

	UFUNCTION(BlueprintCallable, Category="Pragmata|Hold Prompt")
	void SetHolding(bool bNewHolding);

	UFUNCTION(BlueprintCallable, Category="Pragmata|Hold Prompt")
	void SetProhibited(bool bNewProhibited);

	UFUNCTION(BlueprintCallable, Category="Pragmata|Hold Prompt")
	void SetKeyTexture(UTexture2D* NewTexture);

	UFUNCTION(BlueprintCallable, Category="Pragmata|Hold Prompt")
	void ResetHoldProgress();

protected:
	virtual void NativeConstruct() override;

private:
	void ApplyHoldProgress();
	void ApplyHoldingState();
	void ApplyProhibitedState();
	void ApplyKeyTexture();

private:
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UImage> BackgroundCircle = nullptr;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UImage> HoldProgressCircle = nullptr;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UImage> KeyBox = nullptr;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UImage> Prohibition = nullptr;

	UPROPERTY(EditAnywhere, Category="Pragmata|Hold Prompt")
	FName ProgressParameterName = TEXT("Progress");

	UPROPERTY(EditAnywhere, Category="Pragmata|Hold Prompt")
	FLinearColor IdleTint = FLinearColor(1.f, 1.f, 1.f, 0.75f);

	UPROPERTY(EditAnywhere, Category="Pragmata|Hold Prompt")
	FLinearColor HoldingTint = FLinearColor(1.f, 1.f, 1.f, 1.f);

	UPROPERTY(EditAnywhere, Category="Pragmata|Hold Prompt")
	TObjectPtr<UTexture2D> KeyTexture = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> HoldProgressMaterial = nullptr;

	float HoldProgress = 0.f;

	bool bHolding = false;
	bool bProhibited = false;
};
