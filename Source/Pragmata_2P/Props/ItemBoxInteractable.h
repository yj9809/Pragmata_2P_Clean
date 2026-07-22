#pragma once

#include "CoreMinimal.h"
#include "Props/AnimatedPropInteraction.h"

#include "ItemBoxInteractable.generated.h"

UCLASS()
class PRAGMATA_2P_API AItemBoxInteractable : public AAnimatedPropInteraction
{
	GENERATED_BODY()

public:
	AItemBoxInteractable();

	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FPragmataScanResult GetPragmataScanResult_Implementation(AActor* ScanRequester) const override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool ShouldShowInteractionPromptOnDianaFocus() const override;

	UFUNCTION(BlueprintPure, Category = "Item Box")
	bool IsOpened() const { return bIsOpened; }

protected:
	virtual bool ShouldCreateInteractionPromptWidget() const override;

private:
	bool IsDianaInteractor(const AActor* Interactor) const;

	UPROPERTY(SaveGame, Replicated, BlueprintReadOnly, Category = "Item Box|State", meta = (AllowPrivateAccess = "true"))
	bool bIsOpened = false;
	
	UPROPERTY(SaveGame, Replicated, BlueprintReadWrite, Category = "Item Box|State", meta = (AllowPrivateAccess = "true"))
	int32 Amount = 100;
};
