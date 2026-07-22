// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/WeaponWheelTypes.h"
#include "WeaponWheelSlotWidget.generated.h"

class UImage;
class URichTextBlock;
class UTextBlock;
class UWidget;

UCLASS()
class PRAGMATA_2P_API UWeaponWheelSlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Pragmata|Weapon Wheel")
	void SetSlotState(const FWeaponWheelSlotState& NewSlotState);

	UFUNCTION(BlueprintPure, Category="Pragmata|Weapon Wheel")
	FWeaponWheelSlotState GetSlotState() const { return SlotState; }

protected:
	virtual void NativeConstruct() override;

private:
	void ApplySlotState();

	FText GetAmmoText() const;

	FName GetCurrentAmmoStyleName() const;

private:
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UImage> WeaponIcon = nullptr;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<URichTextBlock> AmmoText = nullptr;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UImage> EmptyImage = nullptr;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> WeaponNameText = nullptr;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UWidget> EquippedHighlight = nullptr;

	FWeaponWheelSlotState SlotState;
};
