// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/PlayerBoostGaugeWidget.h"

#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/OverlaySlot.h"
#include "Components/Widget.h"

void UPlayerBoostGaugeWidget::SetBoostPercent(float NewPercent)
{
	NewPercent = FMath::Clamp(NewPercent, 0.f, 1.f);
	
	const float PreviousPercent = CurrentPercent;
	
	if (NewPercent < PreviousPercent)
	{
		CurrentPercent = NewPercent;
		ApplyBoostLayout(CurrentPercent);

		// 단발 소모(점프 등)만 잔상 표시. 호버 같은 연속 소모는 임계값 미만이라 잔상 생략.
		if ((PreviousPercent - NewPercent) >= GhostMinDropThreshold)
		{
			ShowBoostGhost(CurrentPercent, PreviousPercent);
		}
		else if (bGhostActive)
		{
			// 연속 소모(호버 등): 점프 때 생긴 기존 잔상이 줄어든 실제 값을 가리지 않도록 제거.
			HideBoostGhost();
		}
	}
	else if (NewPercent > PreviousPercent)
	{
		CurrentPercent = NewPercent;
		HideBoostGhost();
		ApplyBoostLayout(CurrentPercent);
	}
	else
	{
		CurrentPercent = NewPercent;
		ApplyBoostLayout(CurrentPercent);
	}
}

void UPlayerBoostGaugeWidget::SetBoostLocked(bool bNewLocked)
{
	if (bBoostLocked == bNewLocked)
	{
		return;
	}

	bBoostLocked = bNewLocked;

	// 색만 갱신(레이아웃/잔상 진행 상태는 유지).
	ApplyBoostFillColor(CurrentPercent);

	if (bGhostActive && BoostGhostImage)
	{
		BoostGhostImage->SetColorAndOpacity(bBoostLocked ? LowBoostColor : GhostBoostColor);
	}
}

void UPlayerBoostGaugeWidget::NativeConstruct()
{
	Super::NativeConstruct();
	
	if (BoostFillRegion)
	{
		BoostFillRegion->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	}
	
	if (LeftBoostLine)
	{
		LeftBoostLine->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	}
	
	if (RightBoostLine)
	{
		RightBoostLine->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	}
	
	ApplyRegionImageSlot(BoostFillImage, FVector2D::ZeroVector);
	ApplyRegionImageSlot(BoostGhostImage, FVector2D::ZeroVector);
	
	if (BoostGhostImage)
	{
		BoostGhostImage->SetColorAndOpacity(GhostBoostColor);
	}
	
	HideBoostGhost();
	ApplyBoostLayout(CurrentPercent);
}

void UPlayerBoostGaugeWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	
	if (bPendingLayoutApply)
	{
		ApplyBoostLayout(CurrentPercent);
		
		if (bGhostActive)
		{
			ApplyBoostGhostLayout();
		}
	}
	
	if (bGhostActive)
	{
		if (GhostHoldTimer > 0.f)
		{
			GhostHoldTimer -= InDeltaTime;
			ApplyBoostGhostLayout();
			return;
		}
		
		const float ShrinkSpeed = 1.f / FMath::Max(GhostShrinkDuration, UE_KINDA_SMALL_NUMBER);
		GhostDisplayPercent = FMath::FInterpConstantTo(GhostDisplayPercent, GhostTargetPercent, InDeltaTime, ShrinkSpeed);
		
		if (GhostDisplayPercent <= CurrentPercent + UE_KINDA_SMALL_NUMBER)
		{
			HideBoostGhost();
			return;
		}
		
		ApplyBoostGhostLayout();
	}
}

float UPlayerBoostGaugeWidget::GetTrackWidth() const
{
	if (!BoostTrack)
	{
		return 0.f;
	}
	
	float SizeX = BoostTrack->GetCachedGeometry().GetLocalSize().X;
	
	if (SizeX > 0.f)
	{
		return SizeX;
	}
	
	const UCanvasPanelSlot* TrackSlot = Cast<UCanvasPanelSlot>(BoostTrack->Slot);
	
	if (TrackSlot)
	{
		SizeX = TrackSlot->GetSize().X;
		
		if (SizeX > 0.f)
		{
			return SizeX;
		}
	}
	
	return FallbackTrackWidth > 0.f ? FallbackTrackWidth : 0.f;
}

void UPlayerBoostGaugeWidget::ApplyBoostLayout(float Percent)
{
	Percent = FMath::Clamp(Percent, 0.f, 1.f);
	
	ApplyFillRegion(Percent);
	ApplyCurrentBoostLines(Percent);
	ApplyBoostFillColor(Percent);
}

void UPlayerBoostGaugeWidget::ApplyFillRegion(float Percent)
{
	if (!BoostFillRegion)
	{
		return;
	}
	
	const float TrackWidth = GetTrackWidth();
	
	if (TrackWidth <= 0.f)
	{
		bPendingLayoutApply = true;
		return;
	}
	
	UCanvasPanelSlot* FillSlot = Cast<UCanvasPanelSlot>(BoostFillRegion->Slot);
	
	if (!FillSlot)
	{
		return;
	}
	
	const float NewWidth = TrackWidth * Percent;

	// 좌우로 늘린(stretch) 앵커에서 중앙 확장 → Left/Right 인셋을 동일하게 줘서 트랙 중앙 기준 대칭으로 잘라낸다.
	// (매 프레임 점앵커로 덮어쓰지 않음 — WBP의 stretch 앵커를 그대로 존중.)
	const float SideInset = TrackWidth * (1.f - Percent) * 0.5f;
	FMargin Offsets = FillSlot->GetOffsets();
	Offsets.Left = SideInset;
	Offsets.Right = SideInset;
	FillSlot->SetOffsets(Offsets);

	const float RegionHeight = BoostFillRegion->GetCachedGeometry().GetLocalSize().Y;
	ApplyRegionImageSlot(BoostFillImage, FVector2D(NewWidth, RegionHeight));

	bPendingLayoutApply = false;
}

void UPlayerBoostGaugeWidget::ApplyCurrentBoostLines(float Percent)
{
	if (!LeftBoostLine && !RightBoostLine)
	{
		return;
	}
	
	Percent = FMath::Clamp(Percent, 0.f, 1.f);
	const bool bShowLines = !FMath::IsNearlyEqual(Percent, 1.f);
	
	if (!bShowLines)
	{
		if (LeftBoostLine)
		{
			LeftBoostLine->SetVisibility(ESlateVisibility::Collapsed);
		}
		
		if (RightBoostLine)
		{
			RightBoostLine->SetVisibility(ESlateVisibility::Collapsed);
		}
		
		return;
	}
	
	const float TrackWidth = GetTrackWidth();
	
	if (TrackWidth <= 0.f)
	{
		bPendingLayoutApply = true;
		return;
	}
	
	const float FillWidth = TrackWidth * Percent;
	const float LeftX = FillWidth * -0.5f;
	const float RightX = FillWidth * 0.5f;
	
	auto ApplyBoostLine = [](UWidget* BoostLine, float PositionX)
	{
		if (!BoostLine)
		{
			return;
		}
		
		BoostLine->SetVisibility(ESlateVisibility::Visible);
		
		UCanvasPanelSlot* LineSlot = Cast<UCanvasPanelSlot>(BoostLine->Slot);
		
		if (!LineSlot)
		{
			BoostLine->SetRenderTranslation(FVector2D(PositionX, 0.f));
			return;
		}
		
		const FVector2D CurrentPosition = LineSlot->GetPosition();
		LineSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		LineSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		LineSlot->SetPosition(FVector2D(PositionX, CurrentPosition.Y));
		BoostLine->SetRenderTranslation(FVector2D::ZeroVector);
	};
	
	if (LeftBoostLine)
	{
		ApplyBoostLine(LeftBoostLine, LeftX);
	}
	
	if (RightBoostLine)
	{
		ApplyBoostLine(RightBoostLine, RightX);
	}
}

void UPlayerBoostGaugeWidget::ApplyBoostFillColor(float Percent)
{
	if (!BoostFillImage)
	{
		return;
	}
	
	// 잔여량 10% 미만이거나 스태미나 잠금 상태면 빨간색.
	BoostFillImage->SetColorAndOpacity((bBoostLocked) ? LowBoostColor : NormalBoostColor);
}

void UPlayerBoostGaugeWidget::ApplyRegionImageSlot(UImage* Image, const FVector2D& RegionSize) const
{
	if (!Image)
	{
		return;
	}
	
	if (UOverlaySlot* OverlaySlot = Cast<UOverlaySlot>(Image->Slot))
	{
		OverlaySlot->SetHorizontalAlignment(HAlign_Fill);
		OverlaySlot->SetVerticalAlignment(VAlign_Fill);
		OverlaySlot->SetPadding(FMargin(0.f));
		return;
	}
	
	UCanvasPanelSlot* ImageSlot = Cast<UCanvasPanelSlot>(Image->Slot);
	
	if (!ImageSlot)
	{
		return;
	}
	
	ImageSlot->SetAnchors(FAnchors(0.5f, 0.5f));
	ImageSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	ImageSlot->SetPosition(FVector2D::ZeroVector);
	
	if (RegionSize.X > 0.f && RegionSize.Y > 0.f)
	{
		ImageSlot->SetSize(RegionSize);
	}
}

void UPlayerBoostGaugeWidget::ShowBoostGhost(float LowPercent, float HighPercent)
{
	if (!BoostGhostRegion)
	{
		return;
	}
	
	LowPercent = FMath::Clamp(LowPercent, 0.f, 1.f);
	HighPercent = FMath::Clamp(HighPercent, 0.f, 1.f);
	
	if (LowPercent >= HighPercent)
	{
		HideBoostGhost();
		return;
	}
	
	GhostTargetPercent = LowPercent;
	GhostDisplayPercent = bGhostActive ? FMath::Max(GhostDisplayPercent, HighPercent) : HighPercent;
	GhostHoldTimer = GhostHoldTime;
	bGhostActive = true;
	
	if (BoostGhostImage)
	{
		BoostGhostImage->SetColorAndOpacity(bBoostLocked ? LowBoostColor : GhostBoostColor);
	}
	
	BoostGhostRegion->SetVisibility(ESlateVisibility::Visible);
	
	ApplyBoostGhostLayout();
}

void UPlayerBoostGaugeWidget::HideBoostGhost()
{
	bGhostActive = false;
	GhostHoldTimer = 0.f;
	
	if (BoostGhostRegion)
	{
		BoostGhostRegion->SetVisibility(ESlateVisibility::Collapsed);
	}
	
	GhostDisplayPercent = CurrentPercent;
	GhostTargetPercent = CurrentPercent;
	ApplyCurrentBoostLines(CurrentPercent);
}

void UPlayerBoostGaugeWidget::ApplyBoostGhostLayout()
{
	if (!BoostGhostRegion)
	{
		return;
	}
	
	if (!bGhostActive)
	{
		HideBoostGhost();
		return;
	}
	
	const float TrackWidth = GetTrackWidth();
	
	if (TrackWidth <= 0.f)
	{
		bPendingLayoutApply = true;
		return;
	}
	
	UCanvasPanelSlot* GhostSlot = Cast<UCanvasPanelSlot>(BoostGhostRegion->Slot);
	
	if (!GhostSlot)
	{
		HideBoostGhost();
		return;
	}
	
	const float GhostWidth = TrackWidth * GhostDisplayPercent;

	// Fill과 동일하게 중앙 기준 대칭 인셋(Left=Right)으로 잔상 구간을 배치.
	const float SideInset = TrackWidth * (1.f - GhostDisplayPercent) * 0.5f;
	FMargin Offsets = GhostSlot->GetOffsets();
	Offsets.Left = SideInset;
	Offsets.Right = SideInset;
	GhostSlot->SetOffsets(Offsets);

	const float RegionHeight = BoostGhostRegion->GetCachedGeometry().GetLocalSize().Y;
	ApplyRegionImageSlot(BoostGhostImage, FVector2D(GhostWidth, RegionHeight));
	// 부스트 라인은 잔상이 아니라 항상 현재 게이지 끝을 따라가야 함.
	ApplyCurrentBoostLines(CurrentPercent);
}
