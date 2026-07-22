// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PlayerBoostGaugeWidget.generated.h"

class UImage;
class UWidget;

/**
 * 
 */
UCLASS()
class PRAGMATA_2P_API UPlayerBoostGaugeWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintCallable, Category="Pragmata|Boost")
	void SetBoostPercent(float NewPercent);

	// 스태미나 잠금 상태. true면 게이지를 빨간색(LowBoostColor)으로 강제 표시.
	UFUNCTION(BlueprintCallable, Category="Pragmata|Boost")
	void SetBoostLocked(bool bNewLocked);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	
private:
	float GetTrackWidth() const;
	
	void ApplyBoostLayout(float Percent);
	void ApplyFillRegion(float Percent);
	void ApplyCurrentBoostLines(float Percent);
	void ApplyBoostFillColor(float Percent);
	void ApplyRegionImageSlot(UImage* Image, const FVector2D& RegionSize) const;
	
	void ShowBoostGhost(float LowPercent, float HighPercent);
	void HideBoostGhost();
	void ApplyBoostGhostLayout();
	
private:
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UWidget> BoostTrack = nullptr;

	// 현재 Fill 마스크 영역.
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UWidget> BoostFillRegion = nullptr;

	// WBP 바인딩용 Scale/material 직접 조작 금지.
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UImage> BoostFillImage = nullptr;

	// 현재 HP 끝 위치를 표시하는 라인.
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UWidget> LeftBoostLine = nullptr;
	
	// 피해 잔상 마스크 영역, Canvas Slot 위치/폭 조절 대상.
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UWidget> RightBoostLine = nullptr;
	
	// WBP 바인딩용 Scale/material 직접 조작 금지.
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UWidget> BoostGhostRegion = nullptr;
	
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UImage> BoostGhostImage = nullptr;
	
	UPROPERTY(EditAnywhere, Category="Pragmata|Boost")
	FLinearColor NormalBoostColor = FLinearColor::White;
	
	UPROPERTY(EditAnywhere, Category="Pragmata|Boost")
	FLinearColor LowBoostColor = FLinearColor(1.f, 0.f, 0.f, 1.f);
	
	UPROPERTY(EditAnywhere, Category="Pragmata|Boost")
	FLinearColor GhostBoostColor = FLinearColor(1.f, 0.35f, 0.f, 1.f);
	
private:
	UPROPERTY(EditAnywhere, Category="Pragmata|Boost", meta=(ClampMin = "0.0"))
	float GhostHoldTime = 0.35f;
	
	UPROPERTY(EditAnywhere, Category="Pragmata|Boost", meta=(ClampMin = "0.01"))
	float GhostShrinkDuration = 0.35f;
	
	UPROPERTY(EditAnywhere, Category="Pragmata|Boost", meta=(ClampMin = "0.0"))
	float FallbackTrackWidth = 0.f;

	// 이 값 이상으로 한 번에 줄어들 때만 잔상(ghost)을 표시. 호버 등 연속 소모(매 프레임 미세 감소)는 잔상 생략.
	UPROPERTY(EditAnywhere, Category="Pragmata|Boost", meta=(ClampMin = "0.0", ClampMax = "1.0"))
	float GhostMinDropThreshold = 0.03f;

private:
	float CurrentPercent = 1.f;
	float GhostDisplayPercent = 1.f;
	float GhostTargetPercent = 1.f;
	float GhostHoldTimer = 0.f;
	bool bPendingLayoutApply = false;
	bool bGhostActive = false;
	bool bBoostLocked = false;
};
