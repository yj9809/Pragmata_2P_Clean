#pragma once

#include "CoreMinimal.h"
#include "Interfaces/TerritoryCombatLockable.h"
#include "Props/AxisMovableProp.h"

#include "Elevator.generated.h"

UCLASS()
class PRAGMATA_2P_API AElevator : public AAxisMovableProp, public ITerritoryCombatLockable
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void SetTerritoryCombatLocked_Implementation(bool bLocked) override;

protected:
	virtual bool IsMovementInteractionEnabled() const override { return !bTerritoryCombatLocked; }

	UFUNCTION()
	void OnRep_TerritoryCombatLocked();

	UFUNCTION(BlueprintImplementableEvent, Category="Territory|Combat")
	void OnTerritoryCombatLockStateChanged(bool bLocked);

	UPROPERTY(ReplicatedUsing=OnRep_TerritoryCombatLocked, BlueprintReadOnly, Category="Territory|Combat")
	bool bTerritoryCombatLocked = false;
};
