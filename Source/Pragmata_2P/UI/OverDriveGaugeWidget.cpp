// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/OverDriveGaugeWidget.h"
#include "Components/Image.h"
#include "Components/Widget.h"
#include "Game/PragmataPlayerState.h"
#include "GameFramework/PlayerController.h"

void UOverDriveGaugeWidget::SetOverDrivePercent(float Percent)
{
	TargetFill = FMath::Clamp(Percent, 0.f, 1.f);
}

void UOverDriveGaugeWidget::SetPurificationPanelVisible(bool bNewVisible)
{
	if (bPurificationPanelVisible == bNewVisible)
	{
		return;
	}

	bPurificationPanelVisible = bNewVisible;
	ApplyPurificationPanelVisibility();
}

void UOverDriveGaugeWidget::SetPurificationSkillAvailable(bool bNewAvailable)
{
	if (bPurificationSkillAvailable == bNewAvailable)
	{
		return;
	}

	bPurificationSkillAvailable = bNewAvailable;
	ApplyPurificationAvailability();
}

void UOverDriveGaugeWidget::SetPurificationCooldownPercent(float Percent)
{
	// 정화 cooldown은 gameplay가 넘긴 0~1 표시값만 material에 전달
	PurificationCooldownPercent = FMath::Clamp(Percent, 0.f, 1.f);
	ApplyPurificationCooldownPercent();
}

void UOverDriveGaugeWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (OverDriveGauge)
	{
		GaugeMID = OverDriveGauge->GetDynamicMaterial();
	}

	if (CoolDownImage)
	{
		CoolDownMID = CoolDownImage->GetDynamicMaterial();
	}

	if (GaugeMID)
	{
		GaugeMID->SetVectorParameterValue(TEXT("ChargedColor"), ChargedColor);
		GaugeMID->SetScalarParameterValue(TEXT("Charged"), 0.f);
	}

	if (OverDriveMarker)
	{
		OverDriveMarker->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));

		if (TargetFill == 0.f)
		{
			OverDriveMarker->SetVisibility(ESlateVisibility::Hidden);
		}
	}

	auto InitGlow = [](UImage* Image)
	{
		if (Image)
		{
			Image->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
			Image->SetRenderOpacity(0.f);
			Image->SetVisibility(ESlateVisibility::Hidden);
		}
	};

	InitGlow(BurstGlow);
	SetChargedDotOverlayActive(false);

	ApplyFill(DisplayFill);
	ApplyPurificationCooldownPercent();
	ApplyPurificationPanelVisibility();
	ApplyPurificationAvailability();
	BindCleanseUnlockState();
}

void UOverDriveGaugeWidget::NativeDestruct()
{
	if (APragmataPlayerState* PlayerState = BoundCleansePlayerState.Get())
	{
		PlayerState->OnCleanseUnlockedChanged.RemoveAll(this);
	}
	BoundCleansePlayerState.Reset();
	Super::NativeDestruct();
}

void UOverDriveGaugeWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!BoundCleansePlayerState.IsValid())
	{
		BindCleanseUnlockState();
	}
	UpdateCleanseUnlockedFocus(InDeltaTime);

	if (!FMath::IsNearlyEqual(DisplayFill, TargetFill))
	{
		DisplayFill = FMath::FInterpConstantTo(DisplayFill, TargetFill, InDeltaTime, FillInterpSpeed);
		ApplyFill(DisplayFill);
	}

	const bool bIsFull = DisplayFill >= 1.f - UE_KINDA_SMALL_NUMBER;
	const bool bTargetReleased = TargetFill < 1.f - UE_KINDA_SMALL_NUMBER;

	if (bIsFull && !bWasFull && ChargeState == EChargeState::Idle)
	{
		EnterBurst();
	}
	else if ((bTargetReleased || !bIsFull) && ChargeState != EChargeState::Idle)
	{
		ExitCharged();
	}

	bWasFull = bIsFull;

	switch (ChargeState)
	{
	case EChargeState::Burst:
		UpdateBurst(InDeltaTime);
		break;
	case EChargeState::Charged:
		UpdateChargedPulse(InDeltaTime);
		break;
	default:
		break;
	}
}

void UOverDriveGaugeWidget::ApplyFill(float P)
{
	if (GaugeMID)
	{
		GaugeMID->SetScalarParameterValue(TEXT("FillAmount"), P);
	}

	if (OverDriveMarker)
	{
		OverDriveMarker->SetRenderTransformAngle(P * 360.f);

		const bool bShouldShowMarker = P > UE_KINDA_SMALL_NUMBER && P < 1.f - UE_KINDA_SMALL_NUMBER;
		OverDriveMarker->SetVisibility(bShouldShowMarker ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
	}
}

void UOverDriveGaugeWidget::EnterBurst()
{
	ChargeState = EChargeState::Burst;
	BurstTime = 0.f;

	if (GaugeMID)
	{
		GaugeMID->SetScalarParameterValue(TEXT("Charged"), 1.f);
	}

	SetChargedDotOverlayActive(true);

	if (BurstGlow)
	{
		BurstGlow->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

void UOverDriveGaugeWidget::UpdateBurst(float DeltaTime)
{
	BurstTime += DeltaTime;

	const float Alpha = FMath::Clamp(
		BurstTime / FMath::Max(BurstDuration, UE_KINDA_SMALL_NUMBER),
		0.f,
		1.f
	);

	const float Scale = FMath::Lerp(BurstStartScale, BurstEndScale, Alpha);
	const float Opacity = BurstOpacity * (1.f - Alpha) * (1.f - Alpha);
	SetGlow(BurstGlow, Scale, Opacity);

	if (Alpha >= 1.f)
	{
		EnterCharged();
	}
}

void UOverDriveGaugeWidget::EnterCharged()
{
	ChargeState = EChargeState::Charged;

	if (OverDriveGauge)
	{
		OverDriveGauge->SetRenderScale(FVector2D(1.f));
	}

	if (BurstGlow)
	{
		BurstGlow->SetRenderScale(FVector2D(PulseStartScale));
		BurstGlow->SetRenderOpacity(0.f);
		BurstGlow->SetVisibility(ESlateVisibility::Hidden);
	}

	SetChargedDotOverlayActive(true);

	PulsePhase = 0.f;
	PulseTimer = PulseInterval;
	bPulseActive = false;
}

void UOverDriveGaugeWidget::UpdateChargedPulse(float DeltaTime)
{
	if (!bPulseActive)
	{
		PulseTimer += DeltaTime;

		if (PulseTimer < PulseInterval)
		{
			return;
		}

		PulseTimer = 0.f;
		PulsePhase = 0.f;
		bPulseActive = true;

		if (BurstGlow)
		{
			BurstGlow->SetRenderScale(FVector2D(PulseStartScale));
			BurstGlow->SetRenderOpacity(PulseStartOpacity);
			BurstGlow->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
	}

	PulsePhase += PulseSpeed * DeltaTime;

	const float Phase = FMath::Clamp(PulsePhase, 0.f, 1.f);
	const float ExpandAlpha = 1.f - FMath::Pow(1.f - Phase, 3.f);
	const float Scale = FMath::Lerp(PulseStartScale, PulseEndScale, ExpandAlpha);

	float Opacity = PulseStartOpacity;

	if (Phase >= PulseBlinkStartPhase)
	{
		const float BlinkAlpha = (Phase - PulseBlinkStartPhase) / FMath::Max(1.f - PulseBlinkStartPhase, UE_KINDA_SMALL_NUMBER);
		const float BlinkCycle = FMath::Frac(BlinkAlpha * FMath::Max(PulseBlinkCount, 1.f));
		const float Blink = BlinkCycle < 0.5f ? 1.f : 0.25f;
		Opacity = PulseStartOpacity * Blink;

		if (BlinkAlpha >= 0.92f)
		{
			Opacity = 0.f;
		}
	}

	SetGlow(BurstGlow, Scale, Opacity);

	if (PulsePhase >= 1.f)
	{
		bPulseActive = false;
		PulsePhase = 0.f;

		if (BurstGlow)
		{
			BurstGlow->SetRenderOpacity(0.f);
			BurstGlow->SetVisibility(ESlateVisibility::Hidden);
		}
	}
}

void UOverDriveGaugeWidget::ExitCharged()
{
	ChargeState = EChargeState::Idle;

	if (GaugeMID)
	{
		GaugeMID->SetScalarParameterValue(TEXT("Charged"), 0.f);
	}

	SetChargedDotOverlayActive(false);

	PulseTimer = 0.f;
	PulsePhase = 0.f;
	bPulseActive = false;

	if (OverDriveGauge)
	{
		OverDriveGauge->SetRenderScale(FVector2D(1.f));
	}

	if (BurstGlow)
	{
		BurstGlow->SetRenderScale(FVector2D(PulseStartScale));
		BurstGlow->SetRenderOpacity(0.f);
		BurstGlow->SetVisibility(ESlateVisibility::Hidden);
	}
}

void UOverDriveGaugeWidget::SetGlow(UImage* Image, float Scale, float Opacity)
{
	if (!Image)
	{
		return;
	}

	Image->SetRenderScale(FVector2D(Scale));
	Image->SetRenderOpacity(Opacity);
}

void UOverDriveGaugeWidget::SetChargedDotOverlayActive(bool bActive)
{
	if (!ChargedDotOverlay)
	{
		return;
	}

	ChargedDotOverlay->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	ChargedDotOverlay->SetRenderOpacity(bActive ? 1.f : 0.f);
	ChargedDotOverlay->SetVisibility(bActive ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
}

void UOverDriveGaugeWidget::ApplyPurificationPanelVisibility()
{
	if (PurificationPanel)
	{
		PurificationPanel->SetVisibility(bPurificationPanelVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

// PlayerState delegate로 unlock 복제를 한 번만 받아 panel과 focus를 같은 frame에 갱신
void UOverDriveGaugeWidget::BindCleanseUnlockState()
{
	APragmataPlayerState* PlayerState = GetOwningPlayer() ? GetOwningPlayer()->GetPlayerState<APragmataPlayerState>() : nullptr;
	if (BoundCleansePlayerState.Get() == PlayerState)
	{
		return;
	}

	if (APragmataPlayerState* PreviousPlayerState = BoundCleansePlayerState.Get())
	{
		PreviousPlayerState->OnCleanseUnlockedChanged.RemoveAll(this);
	}
	BoundCleansePlayerState = PlayerState;
	if (PlayerState)
	{
		PlayerState->OnCleanseUnlockedChanged.AddUObject(this, &ThisClass::HandleCleanseUnlockedChanged);
		HandleCleanseUnlockedChanged(PlayerState->IsCleanseUnlocked());
	}
}

// 처음 true가 되는 순간만 widget blueprint focus animation을 재생
void UOverDriveGaugeWidget::HandleCleanseUnlockedChanged(bool bUnlocked)
{
	const bool bWasUnlocked = bHasReceivedCleanseUnlock;
	bHasReceivedCleanseUnlock = bUnlocked;
	SetPurificationPanelVisible(bUnlocked);
	if (bUnlocked && !bWasUnlocked)
	{
		PlayCleanseUnlockedFocus();
	}
}

// Blueprint override가 없어도 새 정화 UI가 한 번 확대되어 획득 지점을 안내
void UOverDriveGaugeWidget::PlayCleanseUnlockedFocus_Implementation()
{
	if (!PurificationPanel)
	{
		return;
	}
	CleanseUnlockFocusElapsed = 0.f;
	bCleanseUnlockFocusActive = true;
	PurificationPanel->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	PurificationPanel->SetRenderScale(FVector2D(CleanseUnlockFocusStartScale));
}

// focus pulse는 layout을 바꾸지 않고 render transform만 원래 크기로 복구
void UOverDriveGaugeWidget::UpdateCleanseUnlockedFocus(float DeltaTime)
{
	if (!bCleanseUnlockFocusActive || !PurificationPanel)
	{
		return;
	}
	CleanseUnlockFocusElapsed += DeltaTime;
	const float Alpha = FMath::Clamp(CleanseUnlockFocusElapsed / FMath::Max(CleanseUnlockFocusDuration, UE_KINDA_SMALL_NUMBER), 0.f, 1.f);
	const float EasedAlpha = 1.f - FMath::Pow(1.f - Alpha, 3.f);
	const float Scale = FMath::Lerp(CleanseUnlockFocusStartScale, 1.f, EasedAlpha);
	PurificationPanel->SetRenderScale(FVector2D(Scale));
	if (Alpha >= 1.f)
	{
		bCleanseUnlockFocusActive = false;
		PurificationPanel->SetRenderScale(FVector2D(1.f));
	}
}

void UOverDriveGaugeWidget::ApplyPurificationAvailability()
{
	// dot/cooldown 교체 때 text layout 튐을 막기 위해 공간은 유지
	const ESlateVisibility AvailableVisibility =
		bPurificationSkillAvailable ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden;
	const ESlateVisibility CoolDownVisibility =
		bPurificationSkillAvailable ? ESlateVisibility::Hidden : ESlateVisibility::HitTestInvisible;

	if (PurificationDot)
	{
		PurificationDot->SetVisibility(AvailableVisibility);
	}

	if (CoolDownPanel)
	{
		CoolDownPanel->SetVisibility(CoolDownVisibility);
	}
}

void UOverDriveGaugeWidget::ApplyPurificationCooldownPercent()
{
	if (CoolDownMID)
	{
		CoolDownMID->SetScalarParameterValue(
			CoolDownPercentParameterName,
			PurificationCooldownPercent
		);
	}
}
