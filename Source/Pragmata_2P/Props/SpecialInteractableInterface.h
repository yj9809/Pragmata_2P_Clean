#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "SpecialInteractableInterface.generated.h"

UINTERFACE(MinimalAPI)
class USpecialInteractableInterface : public UInterface
{
	GENERATED_BODY()
};

class PRAGMATA_2P_API ISpecialInteractableInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Special Interaction")
	void Interact(AActor* Interactor);
};
