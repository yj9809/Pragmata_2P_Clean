#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "TutorialStepViewModel.generated.h"

class UTutorialStepData;
class UTexture2D;

UCLASS(BlueprintType)
class PRAGMATA_2P_API UTutorialStepViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Pragmata|Tutorial")
	FText GetTitle() const { return Title; }

	UFUNCTION(BlueprintPure, Category = "Pragmata|Tutorial")
	UTexture2D* GetPreviewImage() const { return PreviewImage; }

	UFUNCTION(BlueprintPure, Category = "Pragmata|Tutorial")
	FText GetDescription() const { return Description; }

	void InitFromStepData(const UTutorialStepData* StepData);

protected:
	void SetTitle(const FText& NewTitle);
	void SetPreviewImage(UTexture2D* NewPreviewImage);
	void SetDescription(const FText& NewDescription);

private:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Pragmata|Tutorial", meta = (AllowPrivateAccess = "true"))
	FText Title;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Pragmata|Tutorial", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTexture2D> PreviewImage;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Pragmata|Tutorial", meta = (AllowPrivateAccess = "true"))
	FText Description;
};
