#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "EnemyHPBarWidget.generated.h"

class UImage;
class UMaterialInstanceDynamic;

UCLASS()
class PRAGMATA_2P_API UEnemyHPBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Pragmata|Enemy HP")
	void SetHealthValue(float NewCurrentHealth, float NewMaxHealth);

	UFUNCTION(BlueprintCallable, Category="Pragmata|Enemy HP")
	void SetHealthPercent(float NewPercent);

	UFUNCTION(BlueprintPure, Category="Pragmata|Enemy HP")
	float GetHealthPercent() const { return HealthPercent; }

protected:
	virtual void NativeConstruct() override;

private:
	void EnsureMaterialInstance();
	void ApplyHealthPercent();

private:
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UImage> HPBar = nullptr;

	UPROPERTY(EditAnywhere, Category="Pragmata|Enemy HP")
	FName PercentParameterName = TEXT("Percent");

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> HPBarMID = nullptr;

	float CurrentHealth = 0.f;
	float MaxHealth = 0.f;
	float HealthPercent = 1.f;
};
