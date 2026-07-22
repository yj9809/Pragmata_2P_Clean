#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/PragmataTypes.h"
#include "TimerManager.h"
#include "PragmataPlayerStatsComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPragmataLunafilamentChangedSignature, int32, Lunafilament);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FPragmataLunafilamentGainedSignature, int32, Delta, int32, OldTotal, int32, NewTotal);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPragmataDamageMultiplierChangedSignature, float, PlayerDamageMultiplier);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPragmataRepairCartridgeChangedSignature, int32, RepairCartridge, int32, MaxRepairCartridge);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPragmataUpgradeLevelChangedSignature, EPragmataUpgradeType, UpgradeType, int32, UpgradeLevel);

UCLASS(ClassGroup=(Pragmata), meta=(BlueprintSpawnableComponent))
class PRAGMATA_2P_API UPragmataPlayerStatsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// shared player stat component 생성
	UPragmataPlayerStatsComponent();

	// shared player stat replication 등록
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 시작 시 강화 기본값을 현재 HP source에 맞춤
	virtual void BeginPlay() override;

	UFUNCTION(BlueprintPure, Category="Pragmata|Player Stats")
	int32 GetLunafilament() const { return Lunafilament; }

	UFUNCTION(BlueprintPure, Category="Pragmata|Player Stats")
	float GetPlayerDamageMultiplier() const { return PlayerDamageMultiplier; }

	UFUNCTION(BlueprintPure, Category="Pragmata|Player Stats")
	float GetDecodeWeaponDamageMultiplier() const { return DecodeWeaponDamageMultiplier; }

	UFUNCTION(BlueprintPure, Category="Pragmata|Player Stats")
	int32 GetRepairCartridge() const { return RepairCartridge; }

	UFUNCTION(BlueprintPure, Category="Pragmata|Player Stats")
	int32 GetRemainingHealCartridges() const { return GetRepairCartridge(); }

	UFUNCTION(BlueprintPure, Category="Pragmata|Player Stats")
	int32 GetMaxRepairCartridge() const { return MaxRepairCartridge; }

	UFUNCTION(BlueprintPure, Category="Pragmata|Player Stats")
	int32 GetRepairCartridgeRecoveryAmount() const { return RepairCartridgeRecoveryAmount; }

	UFUNCTION(BlueprintPure, Category="Pragmata|Upgrade")
	int32 GetUpgradeLevel(EPragmataUpgradeType UpgradeType) const;

	UFUNCTION(BlueprintPure, Category="Pragmata|Upgrade")
	int32 GetMaxUpgradeLevel(EPragmataUpgradeType UpgradeType) const;

	UFUNCTION(BlueprintPure, Category="Pragmata|Upgrade")
	int32 GetUpgradeCost(EPragmataUpgradeType UpgradeType) const;

	UFUNCTION(BlueprintPure, Category="Pragmata|Upgrade")
	int32 GetMaxHPPerSuitUpgrade() const { return MaxHPPerSuitUpgrade; }

	UFUNCTION(BlueprintPure, Category="Pragmata|Upgrade")
	float GetDamageMultiplierPerWeaponUpgrade() const { return DamageMultiplierPerWeaponUpgrade; }

	UFUNCTION(BlueprintPure, Category="Pragmata|Upgrade")
	bool CanUpgrade(EPragmataUpgradeType UpgradeType) const;

	UFUNCTION(BlueprintPure, Category="Pragmata|Upgrade")
	bool CanRefundUpgrade(EPragmataUpgradeType UpgradeType) const;

	UFUNCTION(BlueprintCallable, Category="Pragmata|Player Stats")
	bool AddLunafilament(int32 Amount);

	UFUNCTION(BlueprintCallable, Category="Pragmata|Player Stats")
	bool SpendLunafilament(int32 Amount);

	UFUNCTION(BlueprintCallable, Category="Pragmata|Player Stats")
	void SetPlayerDamageMultiplier(float NewMultiplier);

	UFUNCTION(BlueprintCallable, Category="Pragmata|Player Stats")
	bool ApplyDecodeWeaponDamageBonus(float Multiplier = 1.1f, float DurationSeconds = 10.f);

	UFUNCTION(BlueprintCallable, Category="Pragmata|Player Stats")
	void SetRepairCartridge(int32 NewCount);

	UFUNCTION(BlueprintCallable, Category="Pragmata|Player Stats")
	bool AddRepairCartridge(int32 Amount);

	UFUNCTION(BlueprintCallable, Category="Pragmata|Player Stats")
	bool ConsumeRepairCartridge();

	UFUNCTION(BlueprintCallable, Category="Pragmata|Upgrade")
	bool TryUpgrade(EPragmataUpgradeType UpgradeType);

	UFUNCTION(BlueprintCallable, Category="Pragmata|Upgrade")
	bool TryRefundUpgrade(EPragmataUpgradeType UpgradeType);

	UFUNCTION(BlueprintCallable, Category="Pragmata|Upgrade")
	void RefreshUpgradeEffects();

	UPROPERTY(BlueprintAssignable, Category="Pragmata|Player Stats")
	FPragmataLunafilamentChangedSignature OnLunafilamentChanged;

	UPROPERTY(BlueprintAssignable, Category="Pragmata|Player Stats")
	FPragmataLunafilamentGainedSignature OnLunafilamentGained;

	UPROPERTY(BlueprintAssignable, Category="Pragmata|Player Stats")
	FPragmataDamageMultiplierChangedSignature OnDamageMultiplierChanged;

	UPROPERTY(BlueprintAssignable, Category="Pragmata|Player Stats")
	FPragmataRepairCartridgeChangedSignature OnRepairCartridgeChanged;

	UPROPERTY(BlueprintAssignable, Category="Pragmata|Upgrade")
	FPragmataUpgradeLevelChangedSignature OnUpgradeLevelChanged;

private:
	UFUNCTION()
	void OnRep_Lunafilament(int32 PreviousLunafilament);

	UFUNCTION()
	void OnRep_PlayerDamageMultiplier();

	UFUNCTION()
	void OnRep_RepairCartridgeSnapshot();

	UFUNCTION()
	void OnRep_SuitUpgradeLevel();

	UFUNCTION()
	void OnRep_WeaponUpgradeLevel();

	bool HasServerAuthority() const;
	void BroadcastLunafilamentGained(int32 OldTotal, int32 NewTotal);
	void BroadcastRepairCartridgeChanged();
	void BroadcastUpgradeLevelChanged(EPragmataUpgradeType UpgradeType);
	bool SetUpgradeLevel(EPragmataUpgradeType UpgradeType, int32 NewLevel);
	bool CanApplyUpgradeType(EPragmataUpgradeType UpgradeType) const;
	void ApplyUpgradeEffect(EPragmataUpgradeType UpgradeType);
	int32 ResolveBaseMaxHP();
	void ClearDecodeWeaponDamageBonus();

	UPROPERTY(ReplicatedUsing=OnRep_Lunafilament, VisibleInstanceOnly, Category="Pragmata|Player Stats", meta=(ClampMin="0"))
	int32 Lunafilament = 0;

	UPROPERTY(ReplicatedUsing=OnRep_PlayerDamageMultiplier, EditDefaultsOnly, Category="Pragmata|Player Stats", meta=(ClampMin="0.0"))
	float PlayerDamageMultiplier = 1.f;

	// Decode가 주는 일시 무기 피해 배율은 영구 weapon upgrade와 분리
	float DecodeWeaponDamageMultiplier = 1.f;

	UPROPERTY(ReplicatedUsing=OnRep_RepairCartridgeSnapshot, VisibleInstanceOnly, Category="Pragmata|Player Stats", meta=(ClampMin="0"))
	int32 RepairCartridge = 6;

	UPROPERTY(ReplicatedUsing=OnRep_RepairCartridgeSnapshot, EditDefaultsOnly, Category="Pragmata|Player Stats", meta=(ClampMin="0", ClampMax="7"))
	int32 MaxRepairCartridge = 7;

	UPROPERTY(Replicated, EditDefaultsOnly, Category="Pragmata|Player Stats", meta=(ClampMin="0"))
	int32 RepairCartridgeRecoveryAmount = 300;

	UPROPERTY(ReplicatedUsing=OnRep_SuitUpgradeLevel, VisibleInstanceOnly, Category="Pragmata|Upgrade", meta=(ClampMin="0"))
	int32 SuitUpgradeLevel = 0;

	UPROPERTY(ReplicatedUsing=OnRep_WeaponUpgradeLevel, VisibleInstanceOnly, Category="Pragmata|Upgrade", meta=(ClampMin="0"))
	int32 WeaponUpgradeLevel = 0;

	UPROPERTY(EditDefaultsOnly, Category="Pragmata|Upgrade", meta=(ClampMin="0"))
	int32 MaxUpgradeLevel = 5;

	UPROPERTY(EditDefaultsOnly, Category="Pragmata|Upgrade", meta=(ClampMin="0"))
	int32 LunafilamentCostPerUpgrade = 100;

	UPROPERTY(EditDefaultsOnly, Category="Pragmata|Upgrade|Suit", meta=(ClampMin="0"))
	int32 MaxHPPerSuitUpgrade = 100;

	UPROPERTY(EditDefaultsOnly, Category="Pragmata|Upgrade|Weapon", meta=(ClampMin="0.0"))
	float DamageMultiplierPerWeaponUpgrade = 0.1f;

	int32 CachedBaseMaxHP = 0;

	FTimerHandle DecodeWeaponDamageBonusTimerHandle;
};
