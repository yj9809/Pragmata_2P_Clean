// Fill out your copyright notice in the Description page of Project Settings.

#include "Props/InteractionPowerSupply.h"

#include "Game/PragmataGameState.h"

void AInteractionPowerSupply::Interact_Implementation(AActor* Interactor)
{
	Super::Interact_Implementation(Interactor);	
}

void AInteractionPowerSupply::CompletePuzzle()
{
	if (!HasAuthority() || !CanInteractWithPuzzle())
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		if (APragmataGameState* GS = World->GetGameState<APragmataGameState>())
		{
			bIsActivated = true;
			GS->SetGlobalPowerState(true);
		}
	}

	Super::CompletePuzzle();
}
