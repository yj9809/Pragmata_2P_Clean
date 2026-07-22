#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/PragmataTypes.h"
#include "PragmataFactionComponent.generated.h"

UCLASS(ClassGroup=(Pragmata), meta=(BlueprintSpawnableComponent))
class PRAGMATA_2P_API UPragmataFactionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPragmataFactionComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category="Pragmata|Faction")
	EPragmataFaction GetFaction() const { return Faction; }

	UFUNCTION(BlueprintCallable, Category="Pragmata|Faction")
	void SetFaction(EPragmataFaction NewFaction);

	UFUNCTION(BlueprintPure, Category="Pragmata|Faction")
	bool IsEnemyFaction(EPragmataFaction OtherFaction) const;

private:
	UPROPERTY(EditDefaultsOnly, Replicated, Category="Pragmata|Faction")
	EPragmataFaction Faction = EPragmataFaction::Neutral;
};
