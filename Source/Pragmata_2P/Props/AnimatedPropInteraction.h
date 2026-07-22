#pragma once

#include "CoreMinimal.h"
#include "Props/PropInteractionBase.h"

#include "AnimatedPropInteraction.generated.h"

class UAnimationAsset;
class USkeletalMeshComponent;

UCLASS()
class PRAGMATA_2P_API AAnimatedPropInteraction : public APropInteractionBase
{
	GENERATED_BODY()

public:
	AAnimatedPropInteraction();

	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void PlayPresentationInteractionAnimation();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USkeletalMeshComponent> AnimatedMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Animation")
	TObjectPtr<UAnimationAsset> InteractionAnimation = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Animation", meta = (ClampMin = "0.01"))
	float InteractionAnimationPlayRate = 1.0f;

	UFUNCTION(BlueprintCallable, Category = "Interaction|Animation")
	virtual void PlayInteractionAnimation();

private:
	UFUNCTION()
	void OnRep_InteractionAnimationSerial();

	UPROPERTY(ReplicatedUsing = OnRep_InteractionAnimationSerial)
	int32 InteractionAnimationSerial = 0;
};
