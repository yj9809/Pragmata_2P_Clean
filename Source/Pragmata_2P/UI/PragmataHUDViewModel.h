// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "Engine/TimerHandle.h"
#include "PragmataHUDViewModel.generated.h"

class UOverDriveComponent;
class UPragmataPlayerStatsComponent;
class UPragmataHealthComponent;
class UDianaCleanseComponent;
class AHughCharacter;
class ADianaOperatorPawn;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FPragmataHUDLunafilamentGainedSignature, int32, Delta, int32, OldTotal, int32, NewTotal);

/**
 *
 */
UCLASS()
class PRAGMATA_2P_API UPragmataHUDViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

protected:
	virtual void BeginDestroy() override;

public:
	UFUNCTION(BlueprintPure, Category = "Pragmata|HUD")
	float GetOverDrivePercent() const { return OverDrivePercent; }

	UFUNCTION(BlueprintCallable, Category = "Pragmata|HUD")
	void SetOverDrivePercent(float NewPercent);

	UFUNCTION(BlueprintCallable, Category = "Pragmata|HUD")
	void BindOverDriveComponent(UOverDriveComponent* OverDriveComponent);

	UFUNCTION(BlueprintCallable, Category = "Pragmata|HUD")
	void UnbindOverDriveComponent();

	UFUNCTION(BlueprintPure, Category = "Pragmata|HUD")
	bool CanUseCleanse() const { return bCanUseCleanse; }

	UFUNCTION(BlueprintPure, Category = "Pragmata|HUD")
	float GetCleanseCooldownProgress() const { return CleanseCooldownProgress; }

	UFUNCTION(BlueprintCallable, Category = "Pragmata|HUD")
	void SetCleanseCooldownValues(bool bNewCanUseCleanse, float NewCooldownProgress);

	UFUNCTION(BlueprintCallable, Category = "Pragmata|HUD")
	void BindCleanseComponent(UDianaCleanseComponent* CleanseComponent);

	UFUNCTION(BlueprintCallable, Category = "Pragmata|HUD")
	void UnbindCleanseComponent();

	UFUNCTION(BlueprintCallable, Category = "Pragmata|HUD")
	void RefreshCleanseCooldownState();

	UFUNCTION(BlueprintCallable, Category = "Pragmata|HUD")
	void BindSharedHUDStats(AHughCharacter* HughCharacter, ADianaOperatorPawn* DianaPawn);

	UFUNCTION(BlueprintCallable, Category = "Pragmata|HUD")
	void UnbindSharedHUDStats();

	UFUNCTION(BlueprintPure, Category = "Pragmata|HUD")
	int32 GetLunafilament() const { return Lunafilament; }

	UFUNCTION(BlueprintPure, Category = "Pragmata|HUD")
	int32 GetLastLunafilamentGainDelta() const { return LastLunafilamentGainDelta; }

	UFUNCTION(BlueprintPure, Category = "Pragmata|HUD")
	int32 GetLastLunafilamentGainOldTotal() const { return LastLunafilamentGainOldTotal; }

	UFUNCTION(BlueprintPure, Category = "Pragmata|HUD")
	int32 GetLastLunafilamentGainNewTotal() const { return LastLunafilamentGainNewTotal; }

	UFUNCTION(BlueprintPure, Category = "Pragmata|HUD")
	int32 GetLunafilamentGainSequence() const { return LunafilamentGainSequence; }

	UFUNCTION(BlueprintPure, Category = "Pragmata|HUD")
	float GetLunafilamentGainMergeWindowSeconds() const { return 1.0f; }

	UFUNCTION(BlueprintPure, Category = "Pragmata|HUD")
	float GetPlayerDamageMultiplier() const { return PlayerDamageMultiplier; }

	UFUNCTION(BlueprintCallable, Category = "Pragmata|HUD")
	void SetLunafilament(int32 NewLunafilament);

	UFUNCTION(BlueprintCallable, Category = "Pragmata|HUD")
	void SetPlayerDamageMultiplier(float NewMultiplier);

	UFUNCTION(BlueprintCallable, Category = "Pragmata|HUD")
	void BindPlayerStatsComponent(UPragmataPlayerStatsComponent* PlayerStatsComponent);

	UFUNCTION(BlueprintCallable, Category = "Pragmata|HUD")
	void UnbindPlayerStatsComponent();

	UPROPERTY(BlueprintAssignable, Category = "Pragmata|HUD")
	FPragmataHUDLunafilamentGainedSignature OnLunafilamentGainedForUI;

	UFUNCTION(BlueprintPure, Category = "Pragmata|HUD")
	float GetBoostPercent() const { return BoostPercent; }

	UFUNCTION(BlueprintPure, Category = "Pragmata|HUD")
	bool IsBoostLocked() const { return bBoostLocked; }

	UFUNCTION(BlueprintCallable, Category = "Pragmata|HUD")
	void SetBoostValues(float NewCurrentStamina, float NewMaxStamina, bool bNewLocked);

	UFUNCTION(BlueprintCallable, Category = "Pragmata|HUD")
	void BindStaminaSource(AHughCharacter* HughCharacter);

	UFUNCTION(BlueprintCallable, Category = "Pragmata|HUD")
	void UnbindStaminaSource();

	UFUNCTION(BlueprintPure, Category = "Pragmata|HUD")
	FText GetHPText() const { return HPText; }

	UFUNCTION(BlueprintPure, Category = "Pragmata|HUD")
	float GetHPBarPercent() const { return HPBarPercent; }

	UFUNCTION(BlueprintCallable, Category = "Pragmata|HUD")
	void SetHPBarPercent(float NewPercent);

	UFUNCTION(BlueprintCallable, Category = "Pragmata|HUD")
	void SetCurrentHealth(float NewCurrentHealth);

	UFUNCTION(BlueprintCallable, Category = "Pragmata|HUD")
	void SetMaxHealth(float NewMaxHealth);

	UFUNCTION(BlueprintCallable, Category = "Pragmata|HUD")
	void SetHealthValues(float NewCurrentHealth, float NewMaxHealth);

	UFUNCTION(BlueprintCallable, Category = "Pragmata|HUD")
	void BindHealthComponent(UPragmataHealthComponent* HealthComponent);

	UFUNCTION(BlueprintCallable, Category = "Pragmata|HUD")
	void UnbindHealthComponent();

	UFUNCTION(BlueprintPure, Category = "Pragmata|HUD")
	int32 GetRecoveryItemCount() const { return RecoveryItemCount; }

	UFUNCTION(BlueprintPure, Category = "Pragmata|HUD")
	int32 GetMaxRecoveryItemCount() const { return MaxRecoveryItemCount; }

	UFUNCTION(BlueprintPure, Category = "Pragmata|HUD")
	float GetRecoveryHoldProgress() const { return RecoveryHoldProgress; }

	UFUNCTION(BlueprintPure, Category = "Pragmata|HUD")
	bool IsRecoveryHolding() const { return bRecoveryHolding; }

	UFUNCTION(BlueprintPure, Category = "Pragmata|HUD")
	bool IsRecoveryProhibited() const { return bRecoveryProhibited; }

	UFUNCTION(BlueprintPure, Category = "Pragmata|HUD")
	bool CanUseHeal() const { return bCanUseHeal; }

	UFUNCTION(BlueprintCallable, Category = "Pragmata|HUD")
	void SetRecoveryItemCount(int32 NewCurrentCount, int32 NewMaxCount);

	UFUNCTION(BlueprintCallable, Category = "Pragmata|HUD")
	void SetRecoveryHoldProgress(float NewProgress);

	UFUNCTION(BlueprintCallable, Category = "Pragmata|HUD")
	void SetRecoveryHolding(bool bNewHolding);

	UFUNCTION(BlueprintCallable, Category = "Pragmata|HUD")
	void SetRecoveryProhibited(bool bNewProhibited);

	UFUNCTION(BlueprintCallable, Category = "Pragmata|HUD")
	void SetCanUseHeal(bool bNewCanUseHeal);

private:
	void UpdateHPText();

	void RecalculateHPBarPercent();

	void QueueLunafilamentGainForUI(int32 Delta, int32 OldTotal, int32 NewTotal);
	void StartLunafilamentGainBatchTimer();
	void ClearLunafilamentGainBatchTimer();
	void FlushLunafilamentGainForUI();
	void ResetPendingLunafilamentGainForUI();

	UFUNCTION()
	void OnOverDriveChanged_Handler(float NewPercent);

	UFUNCTION()
	void OnCleanseCooldownChanged_Handler(bool bNewCanUseCleanse, float NewCooldownProgress);

	UFUNCTION()
	void OnHealthChanged_Handler(float NewCurrentHealth, float NewMaxHealth);

	UFUNCTION()
	void OnStaminaChanged_Handler(float NewCurrentStamina, float NewMaxStamina, bool bNewLocked);

	UFUNCTION()
	void OnHealAvailabilityChanged_Handler(bool bNewCanUseHeal, int32 RemainingHealCartridges);

	UFUNCTION()
	void OnHealHoldChanged_Handler(float HoldProgress, bool bHolding);

	UFUNCTION()
	void OnLunafilamentChanged_Handler(int32 NewLunafilament);

	UFUNCTION()
	void OnLunafilamentGained_Handler(int32 Delta, int32 OldTotal, int32 NewTotal);

	UFUNCTION()
	void OnDamageMultiplierChanged_Handler(float NewMultiplier);

	UFUNCTION()
	void OnRepairCartridgeChanged_Handler(int32 NewRepairCartridge, int32 NewMaxRepairCartridge);

private:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Pragmata|HUD", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "1.0"))
	float OverDrivePercent = 0.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Pragmata|HUD", meta = (AllowPrivateAccess = "true"))
	bool bCanUseCleanse = true;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Pragmata|HUD", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "1.0"))
	float CleanseCooldownProgress = 1.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Pragmata|HUD", meta = (AllowPrivateAccess = "true", ClampMin = "0"))
	int32 Lunafilament = 0;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Pragmata|HUD", meta = (AllowPrivateAccess = "true", ClampMin = "0"))
	int32 LastLunafilamentGainDelta = 0;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Pragmata|HUD", meta = (AllowPrivateAccess = "true", ClampMin = "0"))
	int32 LastLunafilamentGainOldTotal = 0;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Pragmata|HUD", meta = (AllowPrivateAccess = "true", ClampMin = "0"))
	int32 LastLunafilamentGainNewTotal = 0;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Pragmata|HUD", meta = (AllowPrivateAccess = "true", ClampMin = "0"))
	int32 LunafilamentGainSequence = 0;

	int32 PendingLunafilamentUIDelta = 0;
	int32 PendingLunafilamentUIOldTotal = 0;
	int32 PendingLunafilamentUINewTotal = 0;
	bool bHasPendingLunafilamentUIBatch = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Pragmata|HUD", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float PlayerDamageMultiplier = 1.f;

	// 부스트(스태미나) 게이지 비율. 스태미나는 가득 찬 상태로 시작.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Pragmata|HUD", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "1.0"))
	float BoostPercent = 1.f;

	// 스태미나 잠금(빨강 표시) 상태. 캐릭터의 IsStaminaConsumable을 그대로 전달.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Pragmata|HUD", meta = (AllowPrivateAccess = "true"))
	bool bBoostLocked = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Pragmata|HUD", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "1.0"))
	float HPBarPercent = 0.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Pragmata|HUD", meta = (AllowPrivateAccess = "true"))
	float CurrentHealth = 0.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Pragmata|HUD", meta = (AllowPrivateAccess = "true"))
	float MaxHealth = 0.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Pragmata|HUD", meta = (AllowPrivateAccess = "true"))
	FText HPText;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "Pragmata|HUD", meta = (AllowPrivateAccess = "true", ClampMin = "0"))
	int32 RecoveryItemCount = 0;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "Pragmata|HUD", meta = (AllowPrivateAccess = "true", ClampMin = "0"))
	int32 MaxRecoveryItemCount = 7;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Pragmata|HUD", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "1.0"))
	float RecoveryHoldProgress = 0.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Pragmata|HUD", meta = (AllowPrivateAccess = "true"))
	bool bRecoveryHolding = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Pragmata|HUD", meta = (AllowPrivateAccess = "true"))
	bool bRecoveryProhibited = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Pragmata|HUD", meta = (AllowPrivateAccess = "true"))
	bool bCanUseHeal = false;

	TWeakObjectPtr<UOverDriveComponent> BoundOverDriveComponent;
	TWeakObjectPtr<UDianaCleanseComponent> BoundCleanseComponent;
	TWeakObjectPtr<UPragmataHealthComponent> BoundHealthComponent;
	TWeakObjectPtr<UPragmataPlayerStatsComponent> BoundPlayerStatsComponent;
	TWeakObjectPtr<AHughCharacter> BoundHughCharacter;
	FTimerHandle LunafilamentGainBatchTimerHandle;
};
