#pragma once

#include "CoreMinimal.h"
#include "Props/ItemBoxInteractable.h"
#include "UObject/SoftObjectPtr.h"
#include "CleanseSkillAcquisitionProp.generated.h"

class UTutorialStepData;

UCLASS()
class PRAGMATA_2P_API ACleanseSkillAcquisitionProp : public AItemBoxInteractable
{
	GENERATED_BODY()

  public:
	ACleanseSkillAcquisitionProp();
	virtual void BeginPlay() override;
	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool ShouldShowInteractionPromptOnDianaFocus() const override;
	void SetVicinityIndicatorVisible(bool bVisible, float DistanceCentimeters);

  private:
	UFUNCTION()
	void OnRep_Acquired();

	UPROPERTY(ReplicatedUsing = OnRep_Acquired, VisibleInstanceOnly, Category = "Pragmata|Cleanse")
	bool bAcquired = false;
	UPROPERTY(EditDefaultsOnly, Category = "Pragmata|Cleanse")
	TSoftObjectPtr<UTutorialStepData> CleanseTutorialStep;
};
