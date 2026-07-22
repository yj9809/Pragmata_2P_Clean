#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TutorialPopupWidget.generated.h"

class UTutorialStepData;

UCLASS(Abstract)
class PRAGMATA_2P_API UTutorialPopupWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// WBP Designer의 View Models 패널에 Create Instance로 추가할 이름("TutorialStepViewModel")과 문자 그대로 일치해야 함
	static const FName ViewModelName;

	UFUNCTION(BlueprintCallable, Category = "Pragmata|Tutorial")
	bool InitializeStep(const UTutorialStepData* StepData);
};
