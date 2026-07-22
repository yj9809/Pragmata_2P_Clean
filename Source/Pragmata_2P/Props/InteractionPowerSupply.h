// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Props/PuzzleInteractable.h"
#include "InteractionPowerSupply.generated.h"

/**
 * 
 */
UCLASS()
class PRAGMATA_2P_API AInteractionPowerSupply : public APuzzleInteractable
{
	GENERATED_BODY()
	
public:
	void Interact_Implementation(AActor* Interactor) override;
	
	virtual void CompletePuzzle() override;

	UFUNCTION(BlueprintPure, Category="Pragmata|Interaction")
	bool IsInteractionComplete() const { return bIsActivated; }

private:
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Pragmata|Interaction", meta=(AllowPrivateAccess="true"))
	bool bIsActivated = false;
};
