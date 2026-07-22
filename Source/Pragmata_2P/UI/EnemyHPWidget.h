#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "EnemyHPWidget.generated.h"

class AActor;
class UEnemyGroggyGaugeWidget;
class UEnemyHPBarWidget;
class UPragmataHealthComponent;
class UWidget;

UCLASS()
class PRAGMATA_2P_API UEnemyHPWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Pragmata|Enemy HP")
	void SetEnemyTarget(AActor* TargetActor);

	UFUNCTION(BlueprintCallable, Category="Pragmata|Enemy HP")
	void BindToHealth(UPragmataHealthComponent* HealthComp);

	UFUNCTION(BlueprintCallable, Category="Pragmata|Enemy HP")
	void UnbindFromHealth();

	UFUNCTION(BlueprintCallable, Category="Pragmata|Enemy HP")
	void SetHealthValue(float NewCurrentHealth, float NewMaxHealth);

	UFUNCTION(BlueprintCallable, Category="Pragmata|Enemy Groggy")
	void SetGroggyValue(float NewCurrentGroggy, float NewMaxGroggy);

	UFUNCTION(BlueprintCallable, Category="Pragmata|Enemy Groggy")
	void SetGroggyPercent(float NewPercent);

	UFUNCTION(BlueprintCallable, Category="Pragmata|Enemy HP")
	void SetEnemyHPVisible(bool bVisible);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	UFUNCTION()
	void HandleHealthChanged(float NewCurrentHealth, float NewMaxHealth);

	UFUNCTION()
	void HandleDeath();

	UEnemyHPBarWidget* ResolveHPBarWidget() const;
	UEnemyGroggyGaugeWidget* ResolveGroggyGaugeWidget() const;
	void SyncFromHealth();

private:
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UWidget> Root = nullptr;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UWidget> EnemyHPRoot = nullptr;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UEnemyHPBarWidget> HPBar = nullptr;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UEnemyHPBarWidget> EnemyHPBar = nullptr;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UEnemyGroggyGaugeWidget> GroggyBar = nullptr;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UEnemyGroggyGaugeWidget> GroggyGauge = nullptr;

	UPROPERTY(EditAnywhere, Category="Pragmata|Enemy HP")
	bool bHideWhenNoTarget = true;

	UPROPERTY(EditAnywhere, Category="Pragmata|Enemy HP")
	bool bHideWhenDead = true;

	TWeakObjectPtr<UPragmataHealthComponent> BoundHealthComp;
	float CachedCurrentHealth = 0.f;
	float CachedMaxHealth = 0.f;
};
