#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Props/SpecialInteractableInterface.h"

#include "DeadFilamentObstacle.generated.h"

class UStaticMeshComponent;

UCLASS()
class PRAGMATA_2P_API ADeadFilamentObstacle : public AActor, public ISpecialInteractableInterface
{
	GENERATED_BODY()

public:
	ADeadFilamentObstacle();

	virtual void Interact_Implementation(AActor* Interactor) override;

	UFUNCTION(BlueprintPure, Category="Pragmata|DeadFilament")
	bool CanBeCleansedBy(AActor* Interactor) const;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> DeadFilamentMesh = nullptr;
};
