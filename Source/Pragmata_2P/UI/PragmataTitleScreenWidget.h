#pragma once

#include "Blueprint/UserWidget.h"
#include "PragmataTitleScreenWidget.generated.h"

class UImage;
class UTextBlock;

UCLASS()
class PRAGMATA_2P_API UPragmataTitleScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void BeginTitleExit(float FadeSeconds);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
	void RequestTitleStart();
	void ApplyFadeAlpha(float Alpha) const;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UImage> FadeImage = nullptr;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> TitleText = nullptr;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> PressAnyButtonText = nullptr;

	float ExitFadeDuration = 0.0f;
	float ExitFadeElapsed = 0.0f;
	bool bExitFadeActive = false;
};
