#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PlayerTeleportTrigger.generated.h"

class UBoxComponent;
class UPrimitiveComponent;

UCLASS()
class PRAGMATA_2P_API APlayerTeleportTrigger : public AActor
{
	GENERATED_BODY()

public:
	APlayerTeleportTrigger();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UBoxComponent> TeleportTrigger = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pragmata|Teleport")
	FVector TargetLocation = FVector::ZeroVector;

private:
	UFUNCTION()
	void HandleTriggerBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult
	);

	bool CanTeleportActor(AActor* OtherActor) const;
	void TeleportActorToTarget(AActor* TargetActor) const;
};
