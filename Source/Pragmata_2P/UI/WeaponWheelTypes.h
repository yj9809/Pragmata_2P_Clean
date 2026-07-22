#pragma once

#include "CoreMinimal.h"
#include "Data/PragmataTypes.h"
#include "WeaponWheelTypes.generated.h"

class UTexture2D;

USTRUCT(BlueprintType)
struct PRAGMATA_2P_API FWeaponWheelSlotState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pragmata|Weapon Wheel")
	EWeaponType WeaponType = EWeaponType::Primary;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pragmata|Weapon Wheel")
	bool bHasWeapon = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pragmata|Weapon Wheel")
	bool bEquipped = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pragmata|Weapon Wheel", meta=(ClampMin="0"))
	int32 CurrentAmmo = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pragmata|Weapon Wheel", meta=(ClampMin="0"))
	int32 MaxAmmo = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pragmata|Weapon Wheel")
	bool bShowMaxAmmo = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pragmata|Weapon Wheel")
	TObjectPtr<UTexture2D> WeaponIcon = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pragmata|Weapon Wheel")
	FVector2D WeaponIconSize = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pragmata|Weapon Wheel")
	FText WeaponName;
};
