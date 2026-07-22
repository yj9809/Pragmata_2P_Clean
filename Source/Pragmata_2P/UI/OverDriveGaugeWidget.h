// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "OverDriveGaugeWidget.generated.h"


class UImage;
class UMaterialInstanceDynamic;
class UWidget;
class APragmataPlayerState;
/**
 * 
 */
UCLASS()
class PRAGMATA_2P_API UOverDriveGaugeWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 목표값 설정 (0 ~ 1).
	UFUNCTION(BlueprintCallable, Category = "Pragmata|OverDrive")
	void SetOverDrivePercent(float Percent);

	UFUNCTION(BlueprintCallable, Category = "Pragmata|Purification")
	void SetPurificationPanelVisible(bool bNewVisible);

	UFUNCTION(BlueprintCallable, Category = "Pragmata|Purification")
	void SetPurificationSkillAvailable(bool bNewAvailable);

	UFUNCTION(BlueprintCallable, Category = "Pragmata|Purification")
	void SetPurificationCooldownPercent(float Percent);

	UFUNCTION(BlueprintNativeEvent, Category = "Pragmata|Purification")
	void PlayCleanseUnlockedFocus();
	virtual void PlayCleanseUnlockedFocus_Implementation();
	
protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	
private:
	enum class EChargeState : uint8
	{
		Idle,
		Burst,
		Charged
	};
	
	void ApplyFill(float P);
	
	void EnterBurst();
	void UpdateBurst(float DeltaTime);
	void EnterCharged();
	void UpdateChargedPulse(float DeltaTime);
	void ExitCharged();
	void SetChargedDotOverlayActive(bool bActive);
	void ApplyPurificationPanelVisibility();
	void ApplyPurificationAvailability();
	void ApplyPurificationCooldownPercent();
	void BindCleanseUnlockState();
	void HandleCleanseUnlockedChanged(bool bUnlocked);
	void UpdateCleanseUnlockedFocus(float DeltaTime);
	static void SetGlow(UImage* Image, float Scale, float Opacity);
	
	
private:	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UImage> OverDriveGauge = nullptr;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UImage> OverDriveMarker = nullptr;
	
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UImage> BurstGlow = nullptr;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UImage> ChargedDotOverlay = nullptr;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UWidget> PurificationPanel = nullptr;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UWidget> PurificationDot = nullptr;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UWidget> CoolDownPanel = nullptr;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UImage> CoolDownImage = nullptr;
	
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> GaugeMID = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> CoolDownMID = nullptr;
	
	UPROPERTY(EditAnywhere, Category = "Pragmata|OverDrive")
	float FillInterpSpeed = 0.1f;

	UPROPERTY(EditAnywhere, Category = "Pragmata|Purification")
	FName CoolDownPercentParameterName = TEXT("Percent");
	
	UPROPERTY(EditAnywhere, Category="Pragmata|OverDrive|Color")
	FLinearColor ChargedColor = FLinearColor(1.0f, 0.45f, 0.1f);

	UPROPERTY(EditAnywhere, Category = "Pragmata|OverDrive|Burst")
	float BurstDuration = 0.28f;

	UPROPERTY(EditAnywhere, Category = "Pragmata|OverDrive|Burst")
	float BurstStartScale = 0.9f;

	UPROPERTY(EditAnywhere, Category = "Pragmata|OverDrive|Burst")
	float BurstEndScale = 1.35f;

	UPROPERTY(EditAnywhere, Category = "Pragmata|OverDrive|Burst")
	float BurstOpacity = 0.45f;

	UPROPERTY(EditAnywhere, Category = "Pragmata|OverDrive|Pulse")
	float PulseSpeed = 2.f;
	
	UPROPERTY(EditAnywhere, Category = "Pragmata|OverDrive|Pulse")
	float PulseInterval = 1.2f;

	UPROPERTY(EditAnywhere, Category = "Pragmata|OverDrive|Pulse")
	float PulseStartScale = 0.95f;

	UPROPERTY(EditAnywhere, Category = "Pragmata|OverDrive|Pulse")
	float PulseEndScale = 3.f;

	UPROPERTY(EditAnywhere, Category = "Pragmata|OverDrive|Pulse")
	float PulseStartOpacity = 0.32f;
	
	UPROPERTY(EditAnywhere, Category= "Pragmata|OverDrive|Pulse")
	float PulseBlinkStartPhase = 0.68f;
	
	UPROPERTY(EditAnywhere, Category= "Pragmata|OverDrive|Pulse")
	float PulseBlinkCount = 10.f;
	
	float TargetFill = 0.f;
	float DisplayFill = 0.f;
	float PurificationCooldownPercent = 0.f;
	
	EChargeState ChargeState = EChargeState::Idle;
	float BurstTime = 0.f;
	float PulseTimer = 0.f;
	float PulsePhase = 0.f;
	
	bool bWasFull = false;
	bool bPulseActive = false;
	bool bPurificationPanelVisible = false;
	bool bPurificationSkillAvailable = false;
	bool bHasReceivedCleanseUnlock = false;
	bool bCleanseUnlockFocusActive = false;
	float CleanseUnlockFocusElapsed = 0.f;

	UPROPERTY(EditAnywhere, Category = "Pragmata|Purification|Focus", meta=(ClampMin="0.01", Units="s"))
	float CleanseUnlockFocusDuration = 1.f;

	UPROPERTY(EditAnywhere, Category = "Pragmata|Purification|Focus", meta=(ClampMin="1.0"))
	float CleanseUnlockFocusStartScale = 1.35f;
	TWeakObjectPtr<APragmataPlayerState> BoundCleansePlayerState;
};
