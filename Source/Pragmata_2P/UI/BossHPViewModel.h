#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "BossHPViewModel.generated.h"

class AActor;
class UPragmataHealthComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FBossHPGaugeChangedSignature, float, CurrentValue, float, MaxValue, float, Percent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBossHPVisibilityChangedSignature, bool, bVisible);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBossHPTextChangedSignature, FText, NewText);

UCLASS(BlueprintType)
class PRAGMATA_2P_API UBossHPViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	virtual void BeginDestroy() override;

	UFUNCTION(BlueprintCallable, Category="Pragmata|Boss HP")
	void SetBossTarget(AActor* NewBossTarget);

	UFUNCTION(BlueprintPure, Category="Pragmata|Boss HP")
	AActor* GetBossTarget() const { return BossTarget; }

	UFUNCTION(BlueprintCallable, Category="Pragmata|Boss HP")
	void BindHealthComponent(UPragmataHealthComponent* HealthComponent);

	UFUNCTION(BlueprintCallable, Category="Pragmata|Boss HP")
	void UnbindHealthComponent();

	UFUNCTION(BlueprintCallable, Category="Pragmata|Boss HP")
	void SetBossHealthValues(float NewCurrentHealth, float NewMaxHealth);

	UFUNCTION(BlueprintCallable, Category="Pragmata|Boss HP")
	void SetBossHealthPercent(float NewPercent);

	UFUNCTION(BlueprintCallable, Category="Pragmata|Boss Groggy")
	void SetBossGroggyValue(float NewCurrentGroggy, float NewMaxGroggy);

	UFUNCTION(BlueprintCallable, Category="Pragmata|Boss Groggy")
	void SetBossGroggyPercent(float NewPercent);

	UFUNCTION(BlueprintCallable, Category="Pragmata|Boss HP")
	void SetBossHPVisible(bool bNewVisible);

	UFUNCTION(BlueprintCallable, Category="Pragmata|Boss HP")
	void SetBossNameText(const FText& NewNameText);

	UFUNCTION(BlueprintPure, Category="Pragmata|Boss HP")
	float GetBossHealthPercent() const { return BossHealthPercent; }

	UFUNCTION(BlueprintPure, Category="Pragmata|Boss HP")
	float GetBossCurrentHealth() const { return BossCurrentHealth; }

	UFUNCTION(BlueprintPure, Category="Pragmata|Boss HP")
	float GetBossMaxHealth() const { return BossMaxHealth; }

	UFUNCTION(BlueprintPure, Category="Pragmata|Boss Groggy")
	float GetBossGroggyPercent() const { return BossGroggyPercent; }

	UFUNCTION(BlueprintPure, Category="Pragmata|Boss Groggy")
	float GetBossCurrentGroggy() const { return BossCurrentGroggy; }

	UFUNCTION(BlueprintPure, Category="Pragmata|Boss Groggy")
	float GetBossMaxGroggy() const { return BossMaxGroggy; }

	UFUNCTION(BlueprintPure, Category="Pragmata|Boss HP")
	bool IsBossHPVisible() const { return bBossHPVisible; }

	UFUNCTION(BlueprintPure, Category="Pragmata|Boss HP")
	FText GetBossHPText() const { return BossHPText; }

	UFUNCTION(BlueprintPure, Category="Pragmata|Boss HP")
	FText GetBossNameText() const { return BossNameText; }

	UPROPERTY(BlueprintAssignable, Category="Pragmata|Boss HP")
	FBossHPGaugeChangedSignature OnBossHealthChangedForUI;

	UPROPERTY(BlueprintAssignable, Category="Pragmata|Boss Groggy")
	FBossHPGaugeChangedSignature OnBossGroggyChangedForUI;

	UPROPERTY(BlueprintAssignable, Category="Pragmata|Boss HP")
	FBossHPVisibilityChangedSignature OnBossHPVisibilityChangedForUI;

	UPROPERTY(BlueprintAssignable, Category="Pragmata|Boss HP")
	FBossHPTextChangedSignature OnBossNameChangedForUI;

private:
	void RecalculateBossHealthPercent();
	void UpdateBossHPText();
	void BroadcastBossHealth();
	void BroadcastBossGroggy();

	UFUNCTION()
	void OnBossHealthChanged_Handler(float NewCurrentHealth, float NewMaxHealth);

	UFUNCTION()
	void OnBossDeath_Handler();

private:
	UPROPERTY(BlueprintReadOnly, Category="Pragmata|Boss HP", meta=(AllowPrivateAccess="true"))
	TObjectPtr<AActor> BossTarget = nullptr;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category="Pragmata|Boss HP", meta=(AllowPrivateAccess="true", ClampMin="0.0"))
	float BossCurrentHealth = 0.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category="Pragmata|Boss HP", meta=(AllowPrivateAccess="true", ClampMin="0.0"))
	float BossMaxHealth = 0.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category="Pragmata|Boss HP", meta=(AllowPrivateAccess="true", ClampMin="0.0", ClampMax="1.0"))
	float BossHealthPercent = 0.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category="Pragmata|Boss Groggy", meta=(AllowPrivateAccess="true", ClampMin="0.0"))
	float BossCurrentGroggy = 0.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category="Pragmata|Boss Groggy", meta=(AllowPrivateAccess="true", ClampMin="0.0"))
	float BossMaxGroggy = 0.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category="Pragmata|Boss Groggy", meta=(AllowPrivateAccess="true", ClampMin="0.0", ClampMax="1.0"))
	float BossGroggyPercent = 0.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category="Pragmata|Boss HP", meta=(AllowPrivateAccess="true"))
	bool bBossHPVisible = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category="Pragmata|Boss HP", meta=(AllowPrivateAccess="true"))
	FText BossHPText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category="Pragmata|Boss HP", meta=(AllowPrivateAccess="true"))
	FText BossNameText;

	TWeakObjectPtr<UPragmataHealthComponent> BoundHealthComponent;
};
