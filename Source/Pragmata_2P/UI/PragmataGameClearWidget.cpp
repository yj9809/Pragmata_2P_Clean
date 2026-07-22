#include "UI/PragmataGameClearWidget.h"

#include "Brushes/SlateColorBrush.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Game/PragmataPlayerController.h"

// ending illustration과 종료 문구는 Widget Blueprint가 소유하고 C++는 공통 fade/input 상태만 초기화
void UPragmataGameClearWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetIsFocusable(true);
	SetRenderOpacity(0.f);

	// 종료 페이드는 Blueprint 슬롯 값과 무관하게 모든 엔딩 문구 위를 덮어야 한다.
	if (FadeImage)
	{
		FadeImage->SetBrush(FSlateColorBrush(FLinearColor::Black));

		if (UCanvasPanelSlot* FadeImageSlot = Cast<UCanvasPanelSlot>(FadeImage->Slot))
		{
			FadeImageSlot->SetZOrder(100);
		}
	}

	ApplyExitFadeAlpha(0.f);

	if (FinText)
	{
		FinText->SetText(FText::FromString(TEXT("FIN.")));
	}
	if (PressAnyButtonText)
	{
		PressAnyButtonText->SetText(FText::FromString(TEXT("Press Any Button")));
	}
}

// black camera fade가 끝난 뒤 ending widget 전체를 천천히 드러냄
void UPragmataGameClearWidget::BeginGameClearPresentation(float FadeSeconds)
{
	PresentationFadeDuration = FMath::Max(0.01f, FadeSeconds);
	PresentationFadeElapsed = 0.f;
	bPresentationFadeActive = true;
	bExitFadeActive = false;
	SetRenderOpacity(0.f);
	ApplyExitFadeAlpha(0.f);
}

// Title 이동 전에는 illustration 위의 FadeImage만 다시 검게 만들어 화면 전환을 숨김
void UPragmataGameClearWidget::BeginGameClearExit(float FadeSeconds)
{
	ExitFadeDuration = FMath::Max(0.01f, FadeSeconds);
	ExitFadeElapsed = 0.f;
	bExitFadeActive = true;
	ApplyExitFadeAlpha(0.f);
}

// presentation과 exit fade를 분리해 Widget Blueprint animation이 없어도 최소 clear 연출을 유지
void UPragmataGameClearWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (bPresentationFadeActive)
	{
		PresentationFadeElapsed = FMath::Min(PresentationFadeElapsed + InDeltaTime, PresentationFadeDuration);
		SetRenderOpacity(PresentationFadeElapsed / PresentationFadeDuration);
		bPresentationFadeActive = PresentationFadeElapsed < PresentationFadeDuration;
	}

	if (bExitFadeActive)
	{
		ExitFadeElapsed = FMath::Min(ExitFadeElapsed + InDeltaTime, ExitFadeDuration);
		ApplyExitFadeAlpha(ExitFadeElapsed / ExitFadeDuration);
		bExitFadeActive = ExitFadeElapsed < ExitFadeDuration;
	}
}

// FullScreenClickTarget을 누른 경우에도 controller의 server 검증 경로로 Title 복귀 요청을 통일
FReply UPragmataGameClearWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	RequestReturnToTitle();
	return FReply::Handled();
}

// mouse 입력은 Widget에서, keyboard/gamepad 입력은 PlayerController::InputKey에서 같은 요청으로 합류
void UPragmataGameClearWidget::RequestReturnToTitle()
{
	if (APragmataPlayerController* PragmataController = GetOwningPlayer<APragmataPlayerController>())
	{
		PragmataController->RequestReturnToTitleFromGameClear();
	}
}

// FadeImage는 illustration 위에 배치하되 클릭을 막지 않는 overlay로만 사용
void UPragmataGameClearWidget::ApplyExitFadeAlpha(float Alpha) const
{
	if (!FadeImage)
	{
		return;
	}

	FLinearColor FadeColor = FLinearColor::Black;
	FadeColor.A = FMath::Clamp(Alpha, 0.f, 1.f);
	FadeImage->SetColorAndOpacity(FadeColor);
	FadeImage->SetVisibility(Alpha > 0.f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}
