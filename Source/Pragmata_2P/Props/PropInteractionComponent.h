// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PropInteractionComponent.generated.h"


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class PRAGMATA_2P_API UPropInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UPropInteractionComponent();

protected:

public:
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void TriggerInteraction(AActor* Interactor);
};
