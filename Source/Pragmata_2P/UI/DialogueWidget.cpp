#include "UI/DialogueWidget.h"

#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Engine/World.h"

void UDialogueWidget::NativeConstruct()
{
	Super::NativeConstruct();

	ApplyDialogueVisibility(ESlateVisibility::Collapsed);
}

void UDialogueWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const UWorld* World = GetWorld();
	if (bDialogueVisible && World && HideAtWorldSeconds > 0.f && World->GetTimeSeconds() >= HideAtWorldSeconds)
	{
		// 자연 종료는 숨긴 뒤 시퀀스 진행 신호를 보냄
		const FDialogueLine FinishedLine = CurrentLine;
		HideDialogue();
		OnLineFinished.Broadcast(FinishedLine);
	}
}

void UDialogueWidget::PlayLine(const FDialogueLine& InLine, float DisplaySeconds)
{
	if (InLine.Line.IsEmptyOrWhitespace())
	{
		HideDialogue();
		return;
	}

	CurrentLine = InLine;

	if (SpeakerText)
	{
		SpeakerText->SetText(InLine.Speaker);
		// 화자가 없으면 나레이션으로 보고 화자 라인을 접음
		SpeakerText->SetVisibility(InLine.Speaker.IsEmptyOrWhitespace()
			? ESlateVisibility::Collapsed
			: ESlateVisibility::HitTestInvisible);
	}

	if (LineText)
	{
		LineText->SetText(InLine.Line);
	}

	const float ResolvedDisplaySeconds = DisplaySeconds > 0.f ? DisplaySeconds : DefaultDisplaySeconds;
	const UWorld* World = GetWorld();
	HideAtWorldSeconds = World ? World->GetTimeSeconds() + ResolvedDisplaySeconds : 0.f;
	bDialogueVisible = true;
	ApplyDialogueVisibility(ESlateVisibility::HitTestInvisible);

	OnLinePlayed(InLine);
}

void UDialogueWidget::HideDialogue()
{
	bDialogueVisible = false;
	HideAtWorldSeconds = 0.f;
	ApplyDialogueVisibility(ESlateVisibility::Collapsed);
}

void UDialogueWidget::ApplyDialogueVisibility(ESlateVisibility NewVisibility)
{
	SetVisibility(NewVisibility);
	if (Root)
	{
		Root->SetVisibility(NewVisibility);
	}
}
