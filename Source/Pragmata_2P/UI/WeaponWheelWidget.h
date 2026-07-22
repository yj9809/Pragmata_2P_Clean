// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "WeaponWheelWidget.generated.h"

class AHughCharacter;
class AWeaponBase;
class UWeaponWheelViewModel;
class UWeaponWheelSlotWidget;

UCLASS()
class PRAGMATA_2P_API UWeaponWheelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintCallable, Category="Pragmata|Weapon Wheel")
	void SetWeaponWheelVisible(bool bNewVisible);

	UFUNCTION(BlueprintCallable, Category="Pragmata|Weapon Wheel")
	void BindWeaponSource(AHughCharacter* HughCharacter);

	UFUNCTION(BlueprintCallable, Category="Pragmata|Weapon Wheel")
	void UnbindWeaponSource();

private:
	void BindWeapon(AWeaponBase* Weapon);
	void UnbindWeapon();
	void RefreshWeaponAmmo();
	void RefreshWeaponInventory();
	UWeaponWheelViewModel* ResolveWeaponWheelViewModel() const;

	UFUNCTION()
	void HandleCurrentWeaponChanged(AWeaponBase* NewWeapon);

	UFUNCTION()
	void HandleWeaponInventoryChanged();

	UFUNCTION()
	void HandleAmmoChanged(int32 CurAmmo, int32 MaxAmmo);

private:
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional, AllowPrivateAccess="true"))
	TObjectPtr<UWeaponWheelSlotWidget> TopSlot = nullptr;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional, AllowPrivateAccess="true"))
	TObjectPtr<UWeaponWheelSlotWidget> LeftSlot = nullptr;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional, AllowPrivateAccess="true"))
	TObjectPtr<UWeaponWheelSlotWidget> RightSlot = nullptr;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional, AllowPrivateAccess="true"))
	TObjectPtr<UWeaponWheelSlotWidget> BottomSlot = nullptr;

	TWeakObjectPtr<AHughCharacter> BoundHughCharacter;
	TWeakObjectPtr<AWeaponBase> BoundWeapon;
};
