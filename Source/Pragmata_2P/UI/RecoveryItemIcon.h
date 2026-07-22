// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RecoveryItemIcon.generated.h"

class UImage;

/**
 * 
 */
UCLASS()
class PRAGMATA_2P_API URecoveryItemIcon : public UUserWidget
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintCallable, Category="Pragmata|Recovery")
	void SetUsed(bool bNewUsed);
	
protected:
	virtual void NativeConstruct() override;
	
private:
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UImage> RecoveryIcon;
	
	UPROPERTY(EditAnywhere, Category="Pragmata|Recovery")
	FLinearColor AvailableTint = FLinearColor::White;
	
	UPROPERTY(EditAnywhere, Category="Pragmata|Recovery")
	FLinearColor UsedTint = FLinearColor(0.25f, 0.25f, 0.25f, 0.65f);
	
	bool bUsed = false;
};
