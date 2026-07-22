#pragma once

#include "CoreMinimal.h"
#include "Data/PragmataTypes.h"
#include "Engine/DataAsset.h"
#include "WeaponDataAsset.generated.h"

class UTexture2D;

UCLASS(BlueprintType)
class PRAGMATA_2P_API UWeaponDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category="Pragmata|Weapon Data")
	EWeaponType GetWeaponType() const { return WeaponType; }

	UFUNCTION(BlueprintPure, Category="Pragmata|Weapon Data")
	FText GetWeaponName() const { return WeaponName; }

	UFUNCTION(BlueprintPure, Category="Pragmata|Weapon Data")
	UTexture2D* GetWeaponIcon() const { return WeaponIcon; }

	UFUNCTION(BlueprintPure, Category="Pragmata|Weapon Data")
	FVector2D GetWeaponIconSize() const { return WeaponIconSize; }

	UFUNCTION(BlueprintPure, Category="Pragmata|Weapon Data")
	int32 GetMaxAmmo() const { return MaxAmmo; }

	UFUNCTION(BlueprintPure, Category="Pragmata|Weapon Data")
	bool ShouldShowMaxAmmo() const { return bShowMaxAmmo; }

	UFUNCTION(BlueprintPure, Category="Pragmata|Weapon Data")
	int32 GetDefaultCurrentAmmo() const { return MaxAmmo > 0 ? FMath::Clamp(DefaultCurrentAmmo, 0, MaxAmmo) : FMath::Max(0, DefaultCurrentAmmo); }

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pragmata|Weapon Data", meta=(AllowPrivateAccess="true"))
	EWeaponType WeaponType = EWeaponType::Primary;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pragmata|Weapon Data", meta=(AllowPrivateAccess="true"))
	FText WeaponName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pragmata|Weapon Data", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UTexture2D> WeaponIcon = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pragmata|Weapon Data|UI", meta=(AllowPrivateAccess="true", ClampMin="0"))
	FVector2D WeaponIconSize = FVector2D(64.f, 64.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pragmata|Weapon Data", meta=(AllowPrivateAccess="true", ClampMin="0"))
	int32 MaxAmmo = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pragmata|Weapon Data", meta=(AllowPrivateAccess="true"))
	bool bShowMaxAmmo = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pragmata|Weapon Data", meta=(AllowPrivateAccess="true", ClampMin="0"))
	int32 DefaultCurrentAmmo = 0;
};
