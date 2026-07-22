// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Engine/TimerHandle.h"
#include "LunaFilamentWidget.generated.h"

class UTextBlock;

UCLASS()
class PRAGMATA_2P_API ULunaFilamentWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

public:
	UFUNCTION(BlueprintCallable, Category = "Pragmata|HUD")
	void SetLunaFilamentAmount(int32 NewAmount);

	UFUNCTION(BlueprintCallable, Category = "Pragmata|HUD")
	void CommitPendingLunaFilamentAmount();

	UFUNCTION(BlueprintPure, Category = "Pragmata|HUD")
	int32 GetLunaFilamentAmount() const { return LunaFilamentAmount; }

private:
	void UpdateAmountText();
	void UpdateGainAmountText();
	void PlayGainPopup();
	void ClearGainPopup();
	void StartGainPopupDisplayTimer();
	void ClearGainPopupTimers();
	void HandleGainPopupDisplayElapsed();
	void StartGainPopupScale(float TargetScale);
	void UpdateGainPopupScale(float InDeltaTime);
	void ApplyGainPopupScale(float NewScale);

	enum class EGainPopupState : uint8
	{
		Hidden,
		ScalingIn,
		Visible,
		ScalingOut
	};

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> AmountText = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> GainAmountText = nullptr;

	int32 LunaFilamentAmount = 0;
	int32 PendingLunaFilamentAmount = 0;
	int32 PendingGainAmount = 0;
	bool bHasInitializedAmount = false;
	EGainPopupState GainPopupState = EGainPopupState::Hidden;
	float GainPopupCurrentScale = 0.f;
	float GainPopupStartScale = 0.f;
	float GainPopupTargetScale = 0.f;
	float GainPopupScaleElapsed = 0.f;
	FTimerHandle GainPopupDisplayTimerHandle;
};
