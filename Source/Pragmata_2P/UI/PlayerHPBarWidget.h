#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PlayerHPBarWidget.generated.h"

class UImage;
class UTextBlock;
class UWidget;

UCLASS()
class PRAGMATA_2P_API UPlayerHPBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Pragmata|HP")
	void SetHealthPercent(float NewPercent);
	
	UFUNCTION(BlueprintCallable, Category="Pragmata|HP")
	void SetHPText(FText NewText);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	// Tick 기준 폭을 얻기 위한 함수.
	float GetTrackWidth() const;

	// Fill Region과 CurrentHealthLine을 한 번에 갱신.
	void ApplyHealthLayout(float Percent);

	// HealthFillRegion의 Canvas Slot X를 조절.
	void ApplyFillRegion(float Percent);

	void ApplyCurrentHealthLine(float Percent);

	// 피해 잔상 구간을 표시하고 Timer 리셋.
	void ShowDamageGhost(float LowPercent, float HighPercent);

	// DamageGhostRegion을 숨김.
	void HideDamageGhost();

	// 저장된 Ghost 구간을 현재 TrackWidth 기준으로 다시 배치.
	void ApplyDamageGhostLayout();

private:
	// TrackWidth 기준 위젯.
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UWidget> HealthTrack = nullptr;

	// 현재 Fill 마스크 영역.
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UWidget> HealthFillRegion = nullptr;

	// WBP 바인딩용 Scale/material 직접 조작 금지.
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UImage> HealthFillImage = nullptr;

	// 현재 HP 끝 위치를 표시하는 라인.
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UWidget> CurrentHealthLine = nullptr;
	
	// 피해 잔상 마스크 영역, Canvas Slot 위치/폭 조절 대상.
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UWidget> DamageGhostRegion = nullptr;
	
	// WBP 바인딩용 Scale/material 직접 조작 금지.
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UImage> DamageGhostImage = nullptr;
	
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> HPValueText = nullptr;
	
private:
	// 피해 잔상 유지 시간을 WBP/에디터에서 조정할 수 있도록 UPROPERTY로 노출.
	UPROPERTY(EditAnywhere, Category="Pragmata|HP", meta=(ClampMin="0.0"))
	float GhostVisibleTime = 1.f;
	
	// geometry가 0일 때 쓸 선택적 폭.
	UPROPERTY(EditAnywhere, Category="Pragmata|HP", meta=(ClampMin="0.0"))
	float FallbackTrackWidth = 0.f;
	
private:
	float CurrentPercent = 1.f;
	
	float GhostStartPercent = 1.f;
	
	float GhostEndPercent = 1.f;
	
	float GhostTimer = 0.f;
	
	bool bPendingLayoutApply = false;
};
