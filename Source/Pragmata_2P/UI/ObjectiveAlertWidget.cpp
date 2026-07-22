#include "UI/ObjectiveAlertWidget.h"

#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Engine/World.h"

void UObjectiveAlertWidget::NativeConstruct()
{
	Super::NativeConstruct();

	ApplyObjectiveVisibility(ESlateVisibility::Collapsed);
}

void UObjectiveAlertWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const UWorld* World = GetWorld();
	if (bObjectiveVisible && World && HideAtWorldSeconds > 0.f && World->GetTimeSeconds() >= HideAtWorldSeconds)
	{
		HideObjective();
	}
}

void UObjectiveAlertWidget::ShowObjective(const FText& InObjectiveText, float DisplaySeconds)
{
	if (InObjectiveText.IsEmptyOrWhitespace())
	{
		HideObjective();
		return;
	}

	if (ObjectiveText)
	{
		ObjectiveText->SetText(InObjectiveText);
	}

	const float ResolvedDisplaySeconds = DisplaySeconds > 0.f ? DisplaySeconds : DefaultDisplaySeconds;
	const UWorld* World = GetWorld();
	HideAtWorldSeconds = World ? World->GetTimeSeconds() + ResolvedDisplaySeconds : 0.f;
	bObjectiveVisible = true;
	ApplyObjectiveVisibility(ESlateVisibility::HitTestInvisible);
}

void UObjectiveAlertWidget::HideObjective()
{
	bObjectiveVisible = false;
	HideAtWorldSeconds = 0.f;
	ApplyObjectiveVisibility(ESlateVisibility::Collapsed);
}

void UObjectiveAlertWidget::ApplyObjectiveVisibility(ESlateVisibility NewVisibility)
{
	SetVisibility(NewVisibility);
	if (Root)
	{
		Root->SetVisibility(NewVisibility);
	}
}
