#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EnvironmentalHazard.generated.h"

class UBoxComponent;
class UPrimitiveComponent;
class USceneComponent;

UCLASS()
class PRAGMATA_2P_API AEnvironmentalHazard : public AActor
{
	GENERATED_BODY()

public:
	AEnvironmentalHazard();

protected:
	virtual void BeginPlay() override;

	UFUNCTION(BlueprintNativeEvent, Category="Pragmata|Hazard")
	bool CanAffectActor(AActor* OtherActor) const;
	virtual bool CanAffectActor_Implementation(AActor* OtherActor) const;

	virtual void ApplyHazardToActor(AActor* TargetActor, const FHitResult* ImpactHit);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<USceneComponent> RootScene = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UBoxComponent> HazardTrigger = nullptr;

private:
	UFUNCTION()
	void HandleHazardBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult
	);
};
