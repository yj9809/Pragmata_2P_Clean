#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UpgradeItemRowWidget.generated.h"

class URichTextBlock;
class UImage;
class UHoldKeyPromptWidget;

UCLASS()
class PRAGMATA_2P_API UUpgradeItemRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Pragmata|Upgrade")
	void UpdateValueText(bool bSelected, float CurrentValue, float NextValue, int32 FractionalDigits);

	UFUNCTION(BlueprintCallable, Category="Pragmata|Upgrade")
	void UpdateLevelDisplayText(bool bSelected, int32 CurrentLevel, int32 NextLevel);

	UFUNCTION(BlueprintCallable, Category="Pragmata|Upgrade")
	void UpdateCostText(int32 Cost, bool bAffordable);

	// MAX면 강화(F) 숨김, 레벨0이면 환급(C) 숨김, 비용 부족이면 F에 X 표시
	UFUNCTION(BlueprintCallable, Category="Pragmata|Upgrade")
	void UpdateHoldButtons(int32 CurrentLevel, int32 MaxLevel, bool bAffordable);

	// 강화(F)/환급(C) 홀드 게이지 전달 통로 — bRefundSide=false는 UpgradeKey, true는 CancleKey
	UFUNCTION(BlueprintCallable, Category="Pragmata|Upgrade|Hold")
	void BeginHold(bool bRefundSide);

	UFUNCTION(BlueprintCallable, Category="Pragmata|Upgrade|Hold")
	void SetHoldGauge(bool bRefundSide, float Progress);

	UFUNCTION(BlueprintCallable, Category="Pragmata|Upgrade|Hold")
	void CancelHold(bool bRefundSide);

	UFUNCTION(BlueprintCallable, Category="Pragmata|Upgrade|Hold")
	void ShowHoldProhibited(bool bRefundSide);

private:
	// bRefundSide에 따라 CancleKey/UpgradeKey 프롬프트 반환
	UHoldKeyPromptWidget* GetHoldPrompt(bool bRefundSide) const;

	// 현재 레벨만큼 슬롯을 OnColor로, 나머지는 OffColor로 칠함
	void UpdateLevelSlots(int32 CurrentLevel);

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<URichTextBlock> ValueRichText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<URichTextBlock> LevelDisplayRichText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<URichTextBlock> CostRichText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> LevelSlot_1;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> LevelSlot_2;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> LevelSlot_3;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> LevelSlot_4;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> LevelSlot_5;

	// 강화(F) 홀드 프롬프트
	UPROPERTY(BlueprintReadOnly, Category="Pragmata|Upgrade|Hold", meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UHoldKeyPromptWidget> UpgradeKey;

	// 환급(C) 홀드 프롬프트
	UPROPERTY(BlueprintReadOnly, Category="Pragmata|Upgrade|Hold", meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UHoldKeyPromptWidget> CancleKey;

	// 강화(F) 버튼 전체(아이콘+라벨) 패널 — MAX 시 숨김
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> UpgradeKeyPanel;

	// 환급(C) 버튼 전체 패널 — 레벨0 시 숨김
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> CancleKeyPanel;

	// 켜진 슬롯 색상 (에디터에서 조정 가능)
	UPROPERTY(EditAnywhere, Category="Pragmata|Upgrade")
	FLinearColor OnColor = FLinearColor::White;

	// 꺼진 슬롯 색상 (에디터에서 조정 가능)
	UPROPERTY(EditAnywhere, Category="Pragmata|Upgrade")
	FLinearColor OffColor = FLinearColor(0.25f, 0.25f, 0.25f, 1.0f);
};
