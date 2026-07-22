#include "UI/PlayerHPBarWidget.h"

#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"

void UPlayerHPBarWidget::SetHealthPercent(float NewPercent)
{
	NewPercent = FMath::Clamp(NewPercent, 0.f, 1.f);

	float PreviousPercent = CurrentPercent;
	
	if (NewPercent < PreviousPercent)
	{
		const bool bGhostActive = GhostTimer > 0.f;
		
		if (bGhostActive)
		{
			GhostEndPercent = FMath::Max(GhostEndPercent, PreviousPercent);
		}
		else
		{
			GhostEndPercent = PreviousPercent;
		}
		
		GhostStartPercent = NewPercent;
		CurrentPercent = NewPercent;
		ApplyHealthLayout(CurrentPercent);
		ShowDamageGhost(GhostStartPercent, GhostEndPercent);
	}
	else if (NewPercent > PreviousPercent)
	{
		CurrentPercent = NewPercent;
		HideDamageGhost();
		ApplyHealthLayout(CurrentPercent);
	}
	// 동일한 경우.
	// 크게 쓸 일은 없지만 제대로 Percent처리가 되지 않을 경우를 위한 보강 코드.
	else
	{
		CurrentPercent = NewPercent;
		ApplyHealthLayout(CurrentPercent);
	}
}

void UPlayerHPBarWidget::SetHPText(FText NewText)
{
	if (!HPValueText)
	{
		return;
	}
	
	HPValueText->SetText(NewText);
}

void UPlayerHPBarWidget::NativeConstruct()
{
	Super::NativeConstruct();
	
	if (HealthFillRegion)
	{
		HealthFillRegion->SetRenderTransformPivot(FVector2D(0.f, 0.5f));
	}
	
	if (CurrentHealthLine)
	{
		CurrentHealthLine->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	}
	
	HideDamageGhost();
	ApplyHealthLayout(CurrentPercent);
}

void UPlayerHPBarWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	
	if (bPendingLayoutApply)
	{
		ApplyHealthLayout(CurrentPercent);
		
		if (GhostTimer > 0.f)
		{
			ApplyDamageGhostLayout();
		}
	}
	
	if (GhostTimer > 0.f)
	{
		GhostTimer -= InDeltaTime;
		
		if (GhostTimer <= 0.f)
		{
			HideDamageGhost();
		}
	}
}

float UPlayerHPBarWidget::GetTrackWidth() const
{
	if (!HealthTrack)
	{
		return 0.f;
	}
	
	float SizeX = HealthTrack->GetCachedGeometry().GetLocalSize().X;
	
	if (SizeX > 0.f)
	{
		return SizeX;
	}
	
	const UCanvasPanelSlot* TrackSlot = Cast<UCanvasPanelSlot>(HealthTrack->Slot);
	
	if (TrackSlot)
	{
		SizeX = TrackSlot->GetSize().X;
		
		if (SizeX > 0.f)
		{
			return SizeX;
		}
	}
	
	if (FallbackTrackWidth > 0.f)
	{
		return FallbackTrackWidth;
	}
	
	return 0.f;
}

void UPlayerHPBarWidget::ApplyHealthLayout(float Percent)
{
	Percent = FMath::Clamp(Percent, 0.f, 1.f);
	
	ApplyFillRegion(Percent);
	ApplyCurrentHealthLine(Percent);
}

void UPlayerHPBarWidget::ApplyFillRegion(float Percent)
{
	if (!HealthFillRegion)
	{
		return;
	}
	
	const float TrackWidth = GetTrackWidth();
	
	if (TrackWidth <= 0.f)
	{
		bPendingLayoutApply = true;
		return;
	}
	
	UCanvasPanelSlot* FillSlot = Cast<UCanvasPanelSlot>(HealthFillRegion->Slot);
	
	if (!FillSlot)
	{
		return;
	}
	
	// 좌우로 늘린(stretch) 앵커에서는 Slot의 Size가 아니라 Offset(Left/Right 인셋)이 실제 폭을 결정한다.
	// 왼쪽 인셋은 그대로 두고, 오른쪽 인셋을 (1 - Percent)만큼 늘려 오른쪽에서 잘라낸다.
	FMargin Offsets = FillSlot->GetOffsets();
	Offsets.Right = TrackWidth * (1.f - Percent);
	FillSlot->SetOffsets(Offsets);

	bPendingLayoutApply = false;
}

void UPlayerHPBarWidget::ShowDamageGhost(float LowPercent, float HighPercent)
{
	if (!DamageGhostRegion)
	{
		return;
	}
	
	LowPercent = FMath::Clamp(LowPercent, 0.f, 1.f);
	HighPercent = FMath::Clamp(HighPercent, 0.f, 1.f);
	
	if (LowPercent >= HighPercent)
	{
		HideDamageGhost();
		return;
	}
	
	GhostStartPercent = LowPercent;
	GhostEndPercent = HighPercent;
	
	GhostTimer = GhostVisibleTime;
	
	DamageGhostRegion->SetVisibility(ESlateVisibility::Visible);
	ApplyDamageGhostLayout();
}

void UPlayerHPBarWidget::HideDamageGhost()
{
	GhostTimer = 0.f;
	
	if (DamageGhostRegion)
	{
		DamageGhostRegion->SetVisibility(ESlateVisibility::Collapsed);
	}
	
	GhostStartPercent = CurrentPercent;
	GhostEndPercent = CurrentPercent;
}

void UPlayerHPBarWidget::ApplyDamageGhostLayout()
{
	if (!DamageGhostRegion)
	{
		return;
	}
	
	if (GhostTimer <= 0.f)
	{
		HideDamageGhost();
		return;
	}
	
	const float TrackWidth = GetTrackWidth();
	
	if (TrackWidth <= 0.f)
	{
		bPendingLayoutApply = true;
		return;
	}
	
	UCanvasPanelSlot* GhostSlot = Cast<UCanvasPanelSlot>(DamageGhostRegion->Slot);
	
	if (!GhostSlot)
	{
		HideDamageGhost();
		return;
	}
	
	// Fill Region과 동일하게 stretch 앵커 기준 Offset(Left/Right 인셋)으로 구간을 배치.
	// 왼쪽 인셋 = GhostStart 지점, 오른쪽 인셋 = (1 - GhostEnd) 지점.
	FMargin Offsets = GhostSlot->GetOffsets();
	Offsets.Left = TrackWidth * GhostStartPercent;
	Offsets.Right = TrackWidth * (1.f - GhostEndPercent);
	GhostSlot->SetOffsets(Offsets);
}

void UPlayerHPBarWidget::ApplyCurrentHealthLine(float Percent)
{
	if (!CurrentHealthLine)
	{
		return;
	}
	
	Percent = FMath::Clamp(Percent, 0.f, 1.f);
	
	if (FMath::IsNearlyEqual(Percent, 1.f))
	{
		CurrentHealthLine->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	
	CurrentHealthLine->SetVisibility(ESlateVisibility::Visible);
	
	const float TrackWidth = GetTrackWidth();
	
	if (TrackWidth <= 0.f)
	{
		bPendingLayoutApply = true;
		return;
	}
	
	CurrentHealthLine->SetRenderTranslation(FVector2D(TrackWidth * Percent, 0.f));
}
