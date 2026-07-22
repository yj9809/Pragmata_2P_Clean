#include "UI/PragmataTitleScreenWidget.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Game/PragmataTitlePlayerController.h"

// title copy와 black fade image를 C++ state 기준으로 초기화
void UPragmataTitleScreenWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetIsFocusable(true);
	ApplyFadeAlpha(0.0f);

	if (TitleText)
	{
		TitleText->SetText(FText::FromString(TEXT("PRAGMATA")));
	}
	if (PressAnyButtonText)
	{
		PressAnyButtonText->SetText(FText::FromString(TEXT("아무 버튼이나 누르세요")));
	}
}

// controller fade와 같은 시간으로 widget FadeImage alpha를 증가
void UPragmataTitleScreenWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bExitFadeActive || ExitFadeDuration <= 0.0f)
	{
		return;
	}

	ExitFadeElapsed = FMath::Min(ExitFadeElapsed + InDeltaTime, ExitFadeDuration);
	ApplyFadeAlpha(ExitFadeElapsed / ExitFadeDuration);
}

// FullScreenClickTarget 밖의 widget click도 같은 title start 경로로 처리
FReply UPragmataTitleScreenWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	RequestTitleStart();
	return FReply::Handled();
}

// title exit fade를 시작해 input과 travel은 controller가 계속 소유
void UPragmataTitleScreenWidget::BeginTitleExit(float FadeSeconds)
{
	ExitFadeDuration = FMath::Max(0.01f, FadeSeconds);
	ExitFadeElapsed = 0.0f;
	bExitFadeActive = true;
	ApplyFadeAlpha(0.0f);
}

// widget click은 owning title controller의 중복 방지 transition으로 위임
void UPragmataTitleScreenWidget::RequestTitleStart()
{
	if (APragmataTitlePlayerController* TitlePlayerController = GetOwningPlayer<APragmataTitlePlayerController>())
	{
		TitlePlayerController->RequestStartFromTitle();
	}
}

// FadeImage는 alpha가 있을 때만 hit-test를 막지 않는 overlay로 표시
void UPragmataTitleScreenWidget::ApplyFadeAlpha(float Alpha) const
{
	if (!FadeImage)
	{
		return;
	}

	FLinearColor FadeColor = FLinearColor::Black;
	FadeColor.A = FMath::Clamp(Alpha, 0.0f, 1.0f);
	FadeImage->SetColorAndOpacity(FadeColor);
	FadeImage->SetVisibility(Alpha > 0.0f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}
