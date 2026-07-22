#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BossHPWidget.generated.h"

class AActor;
class UBossHPViewModel;
class UEnemyGroggyGaugeWidget;
class UEnemyHPBarWidget;
class UTextBlock;
class UWidget;

UCLASS()
class PRAGMATA_2P_API UBossHPWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Pragmata|Boss HP")
	void SetBossTarget(AActor* TargetActor);

	UFUNCTION(BlueprintCallable, Category="Pragmata|Boss HP")
	void BindToViewModel(UBossHPViewModel* ViewModel);

	UFUNCTION(BlueprintCallable, Category="Pragmata|Boss HP")
	void UnbindFromViewModel();

	UFUNCTION(BlueprintCallable, Category="Pragmata|Boss HP")
	void SetHealthValue(float NewCurrentHealth, float NewMaxHealth);

	UFUNCTION(BlueprintCallable, Category="Pragmata|Boss HP")
	void SetHealthPercent(float NewPercent);

	UFUNCTION(BlueprintCallable, Category="Pragmata|Boss Groggy")
	void SetGroggyValue(float NewCurrentGroggy, float NewMaxGroggy);

	UFUNCTION(BlueprintCallable, Category="Pragmata|Boss Groggy")
	void SetGroggyPercent(float NewPercent);

	UFUNCTION(BlueprintCallable, Category="Pragmata|Boss HP")
	void SetBossDisplayName(const FText& NewDisplayName);

	UFUNCTION(BlueprintCallable, Category="Pragmata|Boss HP")
	void SetBossHPVisible(bool bVisible);

	UFUNCTION(BlueprintPure, Category="Pragmata|Boss HP")
	float GetHealthPercent() const { return HealthPercent; }

	UFUNCTION(BlueprintPure, Category="Pragmata|Boss Groggy")
	float GetGroggyPercent() const { return GroggyPercent; }

	UFUNCTION(BlueprintPure, Category="Pragmata|Boss HP")
	UBossHPViewModel* GetBossHPViewModel() const { return BoundBossHPViewModel.Get(); }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintImplementableEvent, Category="Pragmata|Boss HP")
	void OnBossTargetChanged(AActor* TargetActor);

	UFUNCTION(BlueprintImplementableEvent, Category="Pragmata|Boss HP")
	void OnBossHealthChanged(float NewCurrentHealth, float NewMaxHealth, float NewHealthPercent);

	UFUNCTION(BlueprintImplementableEvent, Category="Pragmata|Boss Groggy")
	void OnBossGroggyChanged(float NewCurrentGroggy, float NewMaxGroggy, float NewGroggyPercent);

	UFUNCTION(BlueprintImplementableEvent, Category="Pragmata|Boss HP")
	void OnBossHPVisibilityChanged(bool bVisible);

private:
	static const FName ViewModelName;

	UBossHPViewModel* ResolveBossHPViewModel();
	UEnemyHPBarWidget* ResolveHPBarWidget() const;
	UEnemyGroggyGaugeWidget* ResolveGroggyGaugeWidget() const;
	void SyncFromViewModel();
	void ApplyDisplayText();

	UFUNCTION()
	void HandleViewModelHealthChanged(float NewCurrentHealth, float NewMaxHealth, float NewHealthPercent);

	UFUNCTION()
	void HandleViewModelGroggyChanged(float NewCurrentGroggy, float NewMaxGroggy, float NewGroggyPercent);

	UFUNCTION()
	void HandleViewModelVisibilityChanged(bool bVisible);

	UFUNCTION()
	void HandleViewModelBossNameChanged(FText NewDisplayName);

private:
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UWidget> Root = nullptr;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UWidget> BossHPRoot = nullptr;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UEnemyHPBarWidget> HPBar = nullptr;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UEnemyHPBarWidget> BossHPBar = nullptr;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UEnemyGroggyGaugeWidget> BossGroggyBar = nullptr;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> BossNameText = nullptr;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> BossHPText = nullptr;

	UPROPERTY(EditAnywhere, Category="Pragmata|Boss HP")
	bool bHideWhenNoTarget = true;

	UPROPERTY(EditAnywhere, Category="Pragmata|Boss HP")
	bool bHideWhenDead = true;

	TWeakObjectPtr<UBossHPViewModel> BoundBossHPViewModel;
	FText BossDisplayName;
	float CachedCurrentHealth = 0.f;
	float CachedMaxHealth = 0.f;
	float HealthPercent = 0.f;
	float CachedCurrentGroggy = 0.f;
	float CachedMaxGroggy = 0.f;
	float GroggyPercent = 0.f;
	bool bLoggedMissingViewModel = false;
};
