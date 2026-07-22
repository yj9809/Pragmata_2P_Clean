#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "UI/WeaponWheelTypes.h"
#include "WeaponWheelViewModel.generated.h"

class UTexture2D;
class UWeaponDataAsset;

UCLASS(BlueprintType)
class PRAGMATA_2P_API UWeaponWheelViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	UWeaponWheelViewModel();

	UFUNCTION(BlueprintPure, Category="Pragmata|Weapon Wheel")
	FWeaponWheelSlotState GetPrimarySlotState() const { return PrimarySlotState; }

	UFUNCTION(BlueprintPure, Category="Pragmata|Weapon Wheel")
	FWeaponWheelSlotState GetAttackSlotState() const { return AttackSlotState; }

	UFUNCTION(BlueprintPure, Category="Pragmata|Weapon Wheel")
	FWeaponWheelSlotState GetTacticalSlotState() const { return TacticalSlotState; }

	UFUNCTION(BlueprintPure, Category="Pragmata|Weapon Wheel")
	FWeaponWheelSlotState GetDefenseSlotState() const { return DefenseSlotState; }

	UFUNCTION(BlueprintPure, Category="Pragmata|Weapon Wheel")
	FWeaponWheelSlotState GetWeaponSlotState(EWeaponType WeaponType) const;

	UFUNCTION(BlueprintPure, Category="Pragmata|Weapon Wheel")
	EWeaponType GetEquippedWeaponType() const { return EquippedWeaponType; }

	UFUNCTION(BlueprintCallable, Category="Pragmata|Weapon Wheel")
	void InitializeSlotTypes();

	UFUNCTION(BlueprintCallable, Category="Pragmata|Weapon Wheel")
	void SetWeaponSlot(EWeaponType WeaponType, UTexture2D* WeaponIcon, int32 CurrentAmmo, int32 MaxAmmo = 0, bool bShowMaxAmmo = false);

	UFUNCTION(BlueprintCallable, Category="Pragmata|Weapon Wheel")
	void SetWeaponSlotWithName(EWeaponType WeaponType, FText WeaponName, UTexture2D* WeaponIcon, int32 CurrentAmmo, int32 MaxAmmo = 0, bool bShowMaxAmmo = false);

	UFUNCTION(BlueprintCallable, Category="Pragmata|Weapon Wheel")
	void SetWeaponSlotFromData(UWeaponDataAsset* WeaponData, int32 CurrentAmmo);

	UFUNCTION(BlueprintCallable, Category="Pragmata|Weapon Wheel")
	void AcquireWeaponFromData(UWeaponDataAsset* WeaponData, int32 CurrentAmmo, bool bEquipWeapon = true);

	UFUNCTION(BlueprintCallable, Category="Pragmata|Weapon Wheel")
	void AcquireWeaponFromDataWithDefaultAmmo(UWeaponDataAsset* WeaponData, bool bEquipWeapon = true);

	UFUNCTION(BlueprintCallable, Category="Pragmata|Weapon Wheel")
	void ClearWeaponSlot(EWeaponType WeaponType);

	UFUNCTION(BlueprintCallable, Category="Pragmata|Weapon Wheel")
	void SetEquippedWeaponSlot(EWeaponType WeaponType);

	UFUNCTION(BlueprintCallable, Category="Pragmata|Weapon Wheel")
	void SetWeaponAmmo(EWeaponType WeaponType, int32 CurrentAmmo);

	UFUNCTION(BlueprintCallable, Category="Pragmata|Weapon Wheel")
	void SetWeaponAmmoValues(EWeaponType WeaponType, int32 CurrentAmmo, int32 MaxAmmo);

private:
	FWeaponWheelSlotState& GetMutableSlotState(EWeaponType WeaponType);
	const FWeaponWheelSlotState& GetSlotState(EWeaponType WeaponType) const;
	void BroadcastSlotChanged(EWeaponType WeaponType);
	void BroadcastAllSlotsChanged();

private:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category="Pragmata|Weapon Wheel", meta=(AllowPrivateAccess="true"))
	FWeaponWheelSlotState PrimarySlotState;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category="Pragmata|Weapon Wheel", meta=(AllowPrivateAccess="true"))
	FWeaponWheelSlotState AttackSlotState;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category="Pragmata|Weapon Wheel", meta=(AllowPrivateAccess="true"))
	FWeaponWheelSlotState TacticalSlotState;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category="Pragmata|Weapon Wheel", meta=(AllowPrivateAccess="true"))
	FWeaponWheelSlotState DefenseSlotState;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category="Pragmata|Weapon Wheel", meta=(AllowPrivateAccess="true"))
	EWeaponType EquippedWeaponType = EWeaponType::Primary;
};
