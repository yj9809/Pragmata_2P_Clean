// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/LunaFilamentWidget.h"

#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "TimerManager.h"

namespace
{
constexpr float GainPopupScaleInSeconds = 0.1f;
constexpr float GainPopupDisplayHoldSeconds = 0.5f;
constexpr float GainPopupScaleOutSeconds = 0.1f;
}

void ULunaFilamentWidget::NativeConstruct()
{
	Super::NativeConstruct();

	ClearGainPopup();
	UpdateAmountText();
}

// 파괴 뒤 timer가 widget을 다시 호출하지 않게 정리
void ULunaFilamentWidget::NativeDestruct()
{
	ClearGainPopupTimers();

	Super::NativeDestruct();
}

void ULunaFilamentWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	UpdateGainPopupScale(InDeltaTime);
}

// 루나 필라멘트 획득 연출 동안 총액 표시를 이전 값에 고정
void ULunaFilamentWidget::SetLunaFilamentAmount(int32 NewAmount)
{
	const int32 ClampedAmount = FMath::Max(0, NewAmount);
	if (GainPopupState != EGainPopupState::Hidden && ClampedAmount > PendingLunaFilamentAmount)
	{
		CommitPendingLunaFilamentAmount();
	}

	if (!bHasInitializedAmount)
	{
		LunaFilamentAmount = ClampedAmount;
		PendingLunaFilamentAmount = ClampedAmount;
		bHasInitializedAmount = true;
		ClearGainPopupTimers();
		UpdateAmountText();
		ClearGainPopup();
		return;
	}

	// 소비/보정으로 latest total이 pending total보다 내려가면 획득 연출을 즉시 취소
	if (PendingGainAmount > 0 && ClampedAmount < PendingLunaFilamentAmount)
	{
		LunaFilamentAmount = ClampedAmount;
		PendingLunaFilamentAmount = ClampedAmount;
		PendingGainAmount = 0;
		ClearGainPopupTimers();
		UpdateAmountText();
		ClearGainPopup();
		return;
	}

	if (ClampedAmount <= LunaFilamentAmount)
	{
		LunaFilamentAmount = ClampedAmount;
		PendingLunaFilamentAmount = ClampedAmount;
		PendingGainAmount = 0;
		ClearGainPopupTimers();
		UpdateAmountText();
		ClearGainPopup();
		return;
	}

	PendingLunaFilamentAmount = ClampedAmount;
	PendingGainAmount = PendingLunaFilamentAmount - LunaFilamentAmount;
	UpdateGainAmountText();
	PlayGainPopup();
}

// 보류된 총액을 실제 표시값에 반영하고 팝업 상태 정리
void ULunaFilamentWidget::CommitPendingLunaFilamentAmount()
{
	ClearGainPopupTimers();
	LunaFilamentAmount = FMath::Max(0, PendingLunaFilamentAmount);
	PendingGainAmount = 0;
	UpdateAmountText();
	ClearGainPopup();
}

void ULunaFilamentWidget::UpdateAmountText()
{
	if (AmountText)
	{
		AmountText->SetText(FText::AsNumber(LunaFilamentAmount));
	}
}

// 수집된 획득량을 팝업 텍스트로 표시
void ULunaFilamentWidget::UpdateGainAmountText()
{
	if (!GainAmountText)
	{
		return;
	}

	GainAmountText->SetText(FText::Format(INVTEXT("+{0}"), FText::AsNumber(PendingGainAmount)));
	GainAmountText->SetRenderOpacity(1.f);
	GainAmountText->SetVisibility(ESlateVisibility::HitTestInvisible);
}

// batch 표시 시 scale-in, 표시 중 새 batch는 현재 팝업을 갱신
void ULunaFilamentWidget::PlayGainPopup()
{
	switch (GainPopupState)
	{
	case EGainPopupState::Hidden:
		ApplyGainPopupScale(0.f);
		StartGainPopupScale(1.f);
		break;
	case EGainPopupState::ScalingOut:
		StartGainPopupScale(1.f);
		break;
	case EGainPopupState::ScalingIn:
	case EGainPopupState::Visible:
	default:
		ApplyGainPopupScale(1.f);
		break;
	}
}

// 획득 팝업을 다음 이벤트 전까지 숨김
void ULunaFilamentWidget::ClearGainPopup()
{
	GainPopupState = EGainPopupState::Hidden;
	GainPopupCurrentScale = 0.f;
	GainPopupStartScale = 0.f;
	GainPopupTargetScale = 0.f;
	GainPopupScaleElapsed = 0.f;

	if (GainAmountText)
	{
		GainAmountText->SetText(FText::GetEmpty());
		GainAmountText->SetRenderOpacity(0.f);
		GainAmountText->SetRenderScale(FVector2D::ZeroVector);
		GainAmountText->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void ULunaFilamentWidget::StartGainPopupDisplayTimer()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		StartGainPopupScale(0.f);
		return;
	}

	World->GetTimerManager().SetTimer(
		GainPopupDisplayTimerHandle,
		this,
		&ULunaFilamentWidget::HandleGainPopupDisplayElapsed,
		GainPopupDisplayHoldSeconds,
		false);
}

// display timer가 파괴 뒤 재호출되지 않게 정리
void ULunaFilamentWidget::ClearGainPopupTimers()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(GainPopupDisplayTimerHandle);
	}
}

// 짧은 표시 시간이 지난 뒤 총액 커밋을 위한 scale-out 시작
void ULunaFilamentWidget::HandleGainPopupDisplayElapsed()
{
	StartGainPopupScale(0.f);
}

// 현재 scale에서 목표 scale까지 C++ tick으로 보간
void ULunaFilamentWidget::StartGainPopupScale(float TargetScale)
{
	GainPopupStartScale = GainPopupCurrentScale;
	GainPopupTargetScale = FMath::Clamp(TargetScale, 0.f, 1.f);
	GainPopupScaleElapsed = 0.f;

	if (FMath::IsNearlyEqual(GainPopupStartScale, GainPopupTargetScale))
	{
		ApplyGainPopupScale(GainPopupTargetScale);
		GainPopupState = GainPopupTargetScale > 0.f ? EGainPopupState::Visible : EGainPopupState::Hidden;
		if (GainPopupState == EGainPopupState::Hidden && PendingGainAmount > 0)
		{
			CommitPendingLunaFilamentAmount();
		}
		return;
	}

	GainPopupState = GainPopupTargetScale > GainPopupStartScale
		? EGainPopupState::ScalingIn
		: EGainPopupState::ScalingOut;
}

void ULunaFilamentWidget::UpdateGainPopupScale(float InDeltaTime)
{
	if (GainPopupState != EGainPopupState::ScalingIn && GainPopupState != EGainPopupState::ScalingOut)
	{
		return;
	}

	const float Duration = GainPopupState == EGainPopupState::ScalingIn
		? GainPopupScaleInSeconds
		: GainPopupScaleOutSeconds;
	GainPopupScaleElapsed += FMath::Max(0.f, InDeltaTime);

	const float Alpha = Duration > UE_KINDA_SMALL_NUMBER
		? FMath::Clamp(GainPopupScaleElapsed / Duration, 0.f, 1.f)
		: 1.f;
	const float NewScale = FMath::Lerp(GainPopupStartScale, GainPopupTargetScale, Alpha);
	ApplyGainPopupScale(NewScale);

	if (Alpha < 1.f)
	{
		return;
	}

	if (GainPopupTargetScale > 0.f)
	{
		ApplyGainPopupScale(1.f);
		GainPopupState = EGainPopupState::Visible;
		StartGainPopupDisplayTimer();
		return;
	}

	CommitPendingLunaFilamentAmount();
}

void ULunaFilamentWidget::ApplyGainPopupScale(float NewScale)
{
	GainPopupCurrentScale = FMath::Clamp(NewScale, 0.f, 1.f);

	if (!GainAmountText)
	{
		return;
	}

	GainAmountText->SetRenderOpacity(1.f);
	GainAmountText->SetRenderScale(FVector2D(GainPopupCurrentScale, GainPopupCurrentScale));
	GainAmountText->SetVisibility(ESlateVisibility::HitTestInvisible);
}
