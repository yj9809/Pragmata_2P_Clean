#pragma once

#include "Blueprint/UserWidget.h"
#include "PragmataGameClearWidget.generated.h"

class UImage;
class UTextBlock;

UCLASS()
class PRAGMATA_2P_API UPragmataGameClearWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void BeginGameClearPresentation(float FadeSeconds);
	void BeginGameClearExit(float FadeSeconds);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
	void RequestReturnToTitle();
	void ApplyExitFadeAlpha(float Alpha) const;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UImage> FadeImage = nullptr;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UImage> EndingIllustration = nullptr;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> FinText = nullptr;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> PressAnyButtonText = nullptr;

	float PresentationFadeDuration = 0.f;
	float PresentationFadeElapsed = 0.f;
	float ExitFadeDuration = 0.f;
	float ExitFadeElapsed = 0.f;
	bool bPresentationFadeActive = false;
	bool bExitFadeActive = false;
};
