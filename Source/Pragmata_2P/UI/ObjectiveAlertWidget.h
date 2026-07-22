#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ObjectiveAlertWidget.generated.h"

class UTextBlock;
class UWidget;

UCLASS()
class PRAGMATA_2P_API UObjectiveAlertWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Pragmata|Objective")
	void ShowObjective(const FText& InObjectiveText, float DisplaySeconds = -1.f);

	UFUNCTION(BlueprintCallable, Category="Pragmata|Objective")
	void HideObjective();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void ApplyObjectiveVisibility(ESlateVisibility NewVisibility);

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional, AllowPrivateAccess="true"))
	TObjectPtr<UWidget> Root = nullptr;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional, AllowPrivateAccess="true"))
	TObjectPtr<UTextBlock> ObjectiveText = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pragmata|Objective", meta=(AllowPrivateAccess="true", ClampMin="0.1", Units="s"))
	float DefaultDisplaySeconds = 4.f;

	float HideAtWorldSeconds = 0.f;
	bool bObjectiveVisible = false;
};
