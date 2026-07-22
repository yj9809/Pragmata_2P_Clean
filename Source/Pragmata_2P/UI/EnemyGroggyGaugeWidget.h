#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "EnemyGroggyGaugeWidget.generated.h"

class UImage;
class UMaterialInstanceDynamic;

UCLASS()
class PRAGMATA_2P_API UEnemyGroggyGaugeWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Pragmata|Enemy Groggy")
	void SetGroggyValue(float NewCurrentGroggy, float NewMaxGroggy);

	UFUNCTION(BlueprintCallable, Category="Pragmata|Enemy Groggy")
	void SetGroggyPercent(float NewPercent);

	UFUNCTION(BlueprintPure, Category="Pragmata|Enemy Groggy")
	float GetGroggyPercent() const { return GroggyPercent; }

protected:
	virtual void NativeConstruct() override;

private:
	UImage* ResolveGroggyImage() const;
	void EnsureMaterialInstance();
	void ApplyGroggyPercent();

private:
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UImage> GroggyBar = nullptr;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UImage> GroggyGauge = nullptr;

	UPROPERTY(EditAnywhere, Category="Pragmata|Enemy Groggy")
	FName PercentParameterName = TEXT("Percent");

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> GroggyMID = nullptr;

	float CurrentGroggy = 0.f;
	float MaxGroggy = 0.f;
	float GroggyPercent = 0.f;
};
