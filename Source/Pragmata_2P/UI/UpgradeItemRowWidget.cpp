#include "UI/UpgradeItemRowWidget.h"

#include "Components/Image.h"
#include "Components/RichTextBlock.h"
#include "UI/HoldKeyPromptWidget.h"

namespace
{
	FString FormatUpgradeValue(float Value, int32 FractionalDigits)
	{
		const int32 ClampedDigits = FMath::Clamp(FractionalDigits, 0, 3);
		return FString::Printf(TEXT("%.*f"), ClampedDigits, Value);
	}
}

// 메뉴에서 계산한 강화 값을 지정된 소수 자리수로 RichText에 표시
void UUpgradeItemRowWidget::UpdateValueText(bool bSelected, float CurrentValue, float NextValue, int32 FractionalDigits)
{
	if (!ValueRichText)
	{
		return;
	}

	const FString CurrentValueText = FormatUpgradeValue(CurrentValue, FractionalDigits);
	const FString NextValueText = FormatUpgradeValue(NextValue, FractionalDigits);
	const FString ValueText = bSelected
		                          ? FString::Printf(
			                          TEXT("<CurrentValueText>%s</><NextValueText> >> %s</>"),
			                          *CurrentValueText, *NextValueText)
		                          : FString::Printf(TEXT("<CurrentValueText>%s</>"), *CurrentValueText);
	ValueRichText->SetText(FText::FromString(ValueText));
}

// 선택 상태에 맞춰 현재 레벨과 다음 레벨을 RichText 스타일로 분리 표시
void UUpgradeItemRowWidget::UpdateLevelDisplayText(bool bSelected, int32 CurrentLevel, int32 NextLevel)
{
	// 텍스트 유무와 무관하게 슬롯은 항상 현재 레벨에 맞춰 갱신
	UpdateLevelSlots(CurrentLevel);

	if (!LevelDisplayRichText)
	{
		return;
	}

	const FString RichText = bSelected
		                         ? FString::Printf(
			                         TEXT(
				                         "<LvLabel>Lv.</><LvCurrentValue>%d</><LvNextLabel> >> Lv.</><LvNextValue>%d</>")
			                         , CurrentLevel, NextLevel
		                         )
		                         : FString::Printf(TEXT("<LvLabel>Lv.</><LvCurrentValue>%d</>"), CurrentLevel);

	LevelDisplayRichText->SetText(FText::FromString(RichText));
}

// 인덱스가 현재 레벨보다 작은 슬롯만 OnColor로 켜고 나머지는 OffColor로 끔
void UUpgradeItemRowWidget::UpdateLevelSlots(int32 CurrentLevel)
{
	const TArray<TObjectPtr<UImage>> LevelSlots = {
		LevelSlot_1, LevelSlot_2, LevelSlot_3, LevelSlot_4, LevelSlot_5
	};

	for (int32 Index = 0; Index < LevelSlots.Num(); ++Index)
	{
		if (UImage* SlotImage = LevelSlots[Index])
		{
			SlotImage->SetColorAndOpacity(Index < CurrentLevel ? OnColor : OffColor);
		}
	}
}

UHoldKeyPromptWidget* UUpgradeItemRowWidget::GetHoldPrompt(bool bRefundSide) const
{
	return bRefundSide ? CancleKey : UpgradeKey;
}

// 홀드 시작: 불가 표시를 지우고 게이지를 0에서 켠다
void UUpgradeItemRowWidget::BeginHold(bool bRefundSide)
{
	if (UHoldKeyPromptWidget* Prompt = GetHoldPrompt(bRefundSide))
	{
		Prompt->SetProhibited(false);
		Prompt->SetHolding(true);
		Prompt->SetHoldProgress(0.f);
	}
}

void UUpgradeItemRowWidget::SetHoldGauge(bool bRefundSide, float Progress)
{
	if (UHoldKeyPromptWidget* Prompt = GetHoldPrompt(bRefundSide))
	{
		Prompt->SetHoldProgress(Progress);
	}
}

// 홀드 취소/완료: 게이지와 불가 표시를 함께 정리
void UUpgradeItemRowWidget::CancelHold(bool bRefundSide)
{
	if (UHoldKeyPromptWidget* Prompt = GetHoldPrompt(bRefundSide))
	{
		Prompt->ResetHoldProgress();
		Prompt->SetProhibited(false);
	}
}

void UUpgradeItemRowWidget::ShowHoldProhibited(bool bRefundSide)
{
	if (UHoldKeyPromptWidget* Prompt = GetHoldPrompt(bRefundSide))
	{
		Prompt->SetProhibited(true);
	}
}

// MAX면 강화(F) 버튼을, 레벨0이면 환급(C) 버튼을 숨기고, 비용 부족 시 F에 X 표시
void UUpgradeItemRowWidget::UpdateHoldButtons(int32 CurrentLevel, int32 MaxLevel, bool bAffordable)
{
	const bool bIsMax = CurrentLevel >= MaxLevel;
	const bool bShowUpgrade = !bIsMax;
	const bool bShowRefund = CurrentLevel > 0;

	if (UpgradeKeyPanel)
	{
		UpgradeKeyPanel->SetVisibility(bShowUpgrade ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (CancleKeyPanel)
	{
		CancleKeyPanel->SetVisibility(bShowRefund ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	// 강화 가능하지만 비용이 부족하면 F 프롬프트에 X 표시
	if (UpgradeKey && bShowUpgrade)
	{
		UpgradeKey->SetProhibited(!bAffordable);
	}
}

// 비용이 없는 상태에서는 이전 비용 텍스트가 남지 않도록 비움
void UUpgradeItemRowWidget::UpdateCostText(int32 Cost, bool bAffordable)
{
	if (!CostRichText)
	{
		return;
	}

	if (Cost <= 0)
	{
		CostRichText->SetText(FText::GetEmpty());
		return;
	}

	const TCHAR* StyleTag = bAffordable ? TEXT("CostAffordable") : TEXT("CostUnavailable");
	const FString RichText = FString::Printf(TEXT("<%s>%d</>"), StyleTag, Cost);

	CostRichText->SetText(FText::FromString(RichText));
}
