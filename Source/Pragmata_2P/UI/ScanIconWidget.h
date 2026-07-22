// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ScanIconWidget.generated.h"

/**
 * 
 */
UCLASS()
class PRAGMATA_2P_API UScanIconWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Pragmata|Scan Icon")
	void SetScanIconVisible(bool NewVisible);
	
	UFUNCTION(BlueprintCallable, Category="Pragmata|Scan Icon")
	void SetDistanceText(const FText& NewText);
	
protected:
	virtual void NativeConstruct() override;

private:
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<class UTextBlock> DistanceText = nullptr;
};
