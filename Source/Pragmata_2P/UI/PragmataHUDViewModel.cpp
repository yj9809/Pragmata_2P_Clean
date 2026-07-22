// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/PragmataHUDViewModel.h"
#include "Characters/HughCharacter.h"
#include "Components/DianaCleanseComponent.h"
#include "Components/OverDriveComponent.h"
#include "Components/PragmataHealthComponent.h"
#include "Components/PragmataPlayerStatsComponent.h"
#include "Engine/World.h"
#include "Pawns/DianaOperatorPawn.h"
#include "TimerManager.h"

void UPragmataHUDViewModel::BeginDestroy()
{
	// ViewModel 파괴 뒤 batch timer가 delegate를 재호출하지 않게 정리
	ClearLunafilamentGainBatchTimer();

	Super::BeginDestroy();
}

void UPragmataHUDViewModel::SetOverDrivePercent(float NewPercent)
{
	UE_MVVM_SET_PROPERTY_VALUE(OverDrivePercent, FMath::Clamp(NewPercent, 0.f, 1.f));
}

void UPragmataHUDViewModel::BindOverDriveComponent(UOverDriveComponent* OverDriveComponent)
{
	if (BoundOverDriveComponent.IsValid())
	{
		BoundOverDriveComponent->OnOverDriveChanged.RemoveDynamic(this, &UPragmataHUDViewModel::OnOverDriveChanged_Handler);
	}

	BoundOverDriveComponent = OverDriveComponent;

	if (!OverDriveComponent)
	{
		SetOverDrivePercent(0.f);
		return;
	}

	OverDriveComponent->OnOverDriveChanged.AddDynamic(this, &UPragmataHUDViewModel::OnOverDriveChanged_Handler);
	SetOverDrivePercent(OverDriveComponent->GetOverDrivePercent());
}

void UPragmataHUDViewModel::UnbindOverDriveComponent()
{
	if (BoundOverDriveComponent.IsValid())
	{
		BoundOverDriveComponent->OnOverDriveChanged.RemoveDynamic(this, &UPragmataHUDViewModel::OnOverDriveChanged_Handler);
	}

	BoundOverDriveComponent.Reset();
	SetOverDrivePercent(0.f);
}

void UPragmataHUDViewModel::SetCleanseCooldownValues(bool bNewCanUseCleanse, float NewCooldownProgress)
{
	UE_MVVM_SET_PROPERTY_VALUE(bCanUseCleanse, bNewCanUseCleanse);
	UE_MVVM_SET_PROPERTY_VALUE(CleanseCooldownProgress, FMath::Clamp(NewCooldownProgress, 0.f, 1.f));
}

void UPragmataHUDViewModel::BindCleanseComponent(UDianaCleanseComponent* CleanseComponent)
{
	if (BoundCleanseComponent.IsValid())
	{
		BoundCleanseComponent->OnCleanseCooldownChanged.RemoveDynamic(this, &UPragmataHUDViewModel::OnCleanseCooldownChanged_Handler);
	}

	BoundCleanseComponent = CleanseComponent;

	if (!CleanseComponent)
	{
		SetCleanseCooldownValues(true, 1.f);
		return;
	}

	// 정화 스킬 cooldown 복제 상태를 HUD용 FieldNotify 값으로 전달
	CleanseComponent->OnCleanseCooldownChanged.AddDynamic(this, &UPragmataHUDViewModel::OnCleanseCooldownChanged_Handler);
	RefreshCleanseCooldownState();
}

void UPragmataHUDViewModel::UnbindCleanseComponent()
{
	if (BoundCleanseComponent.IsValid())
	{
		BoundCleanseComponent->OnCleanseCooldownChanged.RemoveDynamic(this, &UPragmataHUDViewModel::OnCleanseCooldownChanged_Handler);
	}

	BoundCleanseComponent.Reset();
	SetCleanseCooldownValues(true, 1.f);
}

void UPragmataHUDViewModel::RefreshCleanseCooldownState()
{
	if (!BoundCleanseComponent.IsValid())
	{
		return;
	}

	SetCleanseCooldownValues(BoundCleanseComponent->CanUseCleanse(), BoundCleanseComponent->GetCleanseCooldownProgress());
}

void UPragmataHUDViewModel::BindSharedHUDStats(AHughCharacter* HughCharacter, ADianaOperatorPawn* DianaPawn)
{
	// 양쪽 root HUD가 GameState role pointer 기준으로 같은 gameplay source를 구독
	BindHealthComponent(HughCharacter ? HughCharacter->GetHealthComponent() : nullptr);
	BindStaminaSource(HughCharacter);
	BindOverDriveComponent(DianaPawn ? DianaPawn->GetOverDriveComponent() : nullptr);
	BindCleanseComponent(DianaPawn ? DianaPawn->GetCleanseComponent() : nullptr);
	BindPlayerStatsComponent(HughCharacter ? HughCharacter->GetPlayerStatsComponent() : nullptr);
}

void UPragmataHUDViewModel::UnbindSharedHUDStats()
{
	UnbindHealthComponent();
	UnbindStaminaSource();
	UnbindOverDriveComponent();
	UnbindCleanseComponent();
	UnbindPlayerStatsComponent();
}

void UPragmataHUDViewModel::SetLunafilament(int32 NewLunafilament)
{
	UE_MVVM_SET_PROPERTY_VALUE(Lunafilament, FMath::Max(0, NewLunafilament));
}

void UPragmataHUDViewModel::SetPlayerDamageMultiplier(float NewMultiplier)
{
	UE_MVVM_SET_PROPERTY_VALUE(PlayerDamageMultiplier, FMath::Max(0.f, NewMultiplier));
}

void UPragmataHUDViewModel::BindPlayerStatsComponent(UPragmataPlayerStatsComponent* PlayerStatsComponent)
{
	if (BoundPlayerStatsComponent.IsValid())
	{
		BoundPlayerStatsComponent->OnLunafilamentChanged.RemoveDynamic(this, &UPragmataHUDViewModel::OnLunafilamentChanged_Handler);
		BoundPlayerStatsComponent->OnLunafilamentGained.RemoveDynamic(this, &UPragmataHUDViewModel::OnLunafilamentGained_Handler);
		BoundPlayerStatsComponent->OnDamageMultiplierChanged.RemoveDynamic(this, &UPragmataHUDViewModel::OnDamageMultiplierChanged_Handler);
		BoundPlayerStatsComponent->OnRepairCartridgeChanged.RemoveDynamic(this, &UPragmataHUDViewModel::OnRepairCartridgeChanged_Handler);
		ClearLunafilamentGainBatchTimer();
	}

	BoundPlayerStatsComponent = PlayerStatsComponent;
	ResetPendingLunafilamentGainForUI();

	if (!PlayerStatsComponent)
	{
		SetLunafilament(0);
		SetPlayerDamageMultiplier(1.f);
		SetRecoveryItemCount(0, 7);
		return;
	}

	// shared stat component 복제값을 root HUD ViewModel에 연결
	PlayerStatsComponent->OnLunafilamentChanged.AddDynamic(this, &UPragmataHUDViewModel::OnLunafilamentChanged_Handler);
	PlayerStatsComponent->OnLunafilamentGained.AddDynamic(this, &UPragmataHUDViewModel::OnLunafilamentGained_Handler);
	PlayerStatsComponent->OnDamageMultiplierChanged.AddDynamic(this, &UPragmataHUDViewModel::OnDamageMultiplierChanged_Handler);
	PlayerStatsComponent->OnRepairCartridgeChanged.AddDynamic(this, &UPragmataHUDViewModel::OnRepairCartridgeChanged_Handler);
	SetLunafilament(PlayerStatsComponent->GetLunafilament());
	SetPlayerDamageMultiplier(PlayerStatsComponent->GetPlayerDamageMultiplier());
	SetRecoveryItemCount(PlayerStatsComponent->GetRepairCartridge(), PlayerStatsComponent->GetMaxRepairCartridge());
}

void UPragmataHUDViewModel::UnbindPlayerStatsComponent()
{
	if (BoundPlayerStatsComponent.IsValid())
	{
		BoundPlayerStatsComponent->OnLunafilamentChanged.RemoveDynamic(this, &UPragmataHUDViewModel::OnLunafilamentChanged_Handler);
		BoundPlayerStatsComponent->OnLunafilamentGained.RemoveDynamic(this, &UPragmataHUDViewModel::OnLunafilamentGained_Handler);
		BoundPlayerStatsComponent->OnDamageMultiplierChanged.RemoveDynamic(this, &UPragmataHUDViewModel::OnDamageMultiplierChanged_Handler);
		BoundPlayerStatsComponent->OnRepairCartridgeChanged.RemoveDynamic(this, &UPragmataHUDViewModel::OnRepairCartridgeChanged_Handler);
		ClearLunafilamentGainBatchTimer();
	}

	BoundPlayerStatsComponent.Reset();
	ResetPendingLunafilamentGainForUI();
	SetLunafilament(0);
	SetPlayerDamageMultiplier(1.f);
	SetRecoveryItemCount(0, 7);
}

void UPragmataHUDViewModel::SetBoostValues(float NewCurrentStamina, float NewMaxStamina, bool bNewLocked)
{
	const float NewPercent = NewMaxStamina > UE_KINDA_SMALL_NUMBER ? NewCurrentStamina / NewMaxStamina : 0.f;

	UE_MVVM_SET_PROPERTY_VALUE(BoostPercent, FMath::Clamp(NewPercent, 0.f, 1.f));
	UE_MVVM_SET_PROPERTY_VALUE(bBoostLocked, bNewLocked);
}

void UPragmataHUDViewModel::BindStaminaSource(AHughCharacter* HughCharacter)
{
	if (BoundHughCharacter.IsValid())
	{
		BoundHughCharacter->OnStaminaChanged.RemoveDynamic(this, &UPragmataHUDViewModel::OnStaminaChanged_Handler);
		BoundHughCharacter->OnHealAvailabilityChanged.RemoveDynamic(this, &UPragmataHUDViewModel::OnHealAvailabilityChanged_Handler);
		BoundHughCharacter->OnHealHoldChanged.RemoveDynamic(this, &UPragmataHUDViewModel::OnHealHoldChanged_Handler);
	}

	BoundHughCharacter = HughCharacter;

	if (!HughCharacter)
	{
		SetBoostValues(0.f, 0.f, false);
		return;
	}

	HughCharacter->OnStaminaChanged.AddDynamic(this, &UPragmataHUDViewModel::OnStaminaChanged_Handler);
	HughCharacter->OnHealAvailabilityChanged.AddDynamic(this, &UPragmataHUDViewModel::OnHealAvailabilityChanged_Handler);
	HughCharacter->OnHealHoldChanged.AddDynamic(this, &UPragmataHUDViewModel::OnHealHoldChanged_Handler);
	// 잠금(빨강) = 소모 불가(!IsStaminaConsumable).
	SetBoostValues(HughCharacter->GetCurrentStamina(), HughCharacter->GetMaxStamina(), !HughCharacter->GetIsStaminaConsumable());
	SetCanUseHeal(HughCharacter->CanUseHeal());
	SetRecoveryHoldProgress(HughCharacter->GetHealHoldProgress());
}

void UPragmataHUDViewModel::UnbindStaminaSource()
{
	if (BoundHughCharacter.IsValid())
	{
		BoundHughCharacter->OnStaminaChanged.RemoveDynamic(this, &UPragmataHUDViewModel::OnStaminaChanged_Handler);
		BoundHughCharacter->OnHealAvailabilityChanged.RemoveDynamic(this, &UPragmataHUDViewModel::OnHealAvailabilityChanged_Handler);
		BoundHughCharacter->OnHealHoldChanged.RemoveDynamic(this, &UPragmataHUDViewModel::OnHealHoldChanged_Handler);
	}

	BoundHughCharacter.Reset();
	SetBoostValues(0.f, 0.f, false);
	SetCanUseHeal(false);
	SetRecoveryHolding(false);
	SetRecoveryHoldProgress(0.f);
}

void UPragmataHUDViewModel::SetHPBarPercent(float NewPercent)
{
	UE_MVVM_SET_PROPERTY_VALUE(HPBarPercent, FMath::Clamp(NewPercent, 0.f, 1.f));
}

void UPragmataHUDViewModel::SetCurrentHealth(float NewCurrentHealth)
{
	const float ClampedHealth = FMath::Clamp(NewCurrentHealth, 0.f, MaxHealth);

	UE_MVVM_SET_PROPERTY_VALUE(CurrentHealth, ClampedHealth);

	RecalculateHPBarPercent();
	UpdateHPText();
}

void UPragmataHUDViewModel::SetMaxHealth(float NewMaxHealth)
{
	const float ClampedMaxHealth = FMath::Max(0.f, NewMaxHealth);
	const float ClampedCurrentHealth = FMath::Clamp(CurrentHealth, 0.f, ClampedMaxHealth);

	UE_MVVM_SET_PROPERTY_VALUE(MaxHealth, ClampedMaxHealth);
	UE_MVVM_SET_PROPERTY_VALUE(CurrentHealth, ClampedCurrentHealth);

	RecalculateHPBarPercent();
	UpdateHPText();
}

void UPragmataHUDViewModel::SetHealthValues(float NewCurrentHealth, float NewMaxHealth)
{
	const float ClampedMaxHealth = FMath::Max(0.f, NewMaxHealth);
	const float ClampedCurrentHealth = FMath::Clamp(NewCurrentHealth, 0.f, ClampedMaxHealth);

	UE_MVVM_SET_PROPERTY_VALUE(MaxHealth, ClampedMaxHealth);
	UE_MVVM_SET_PROPERTY_VALUE(CurrentHealth, ClampedCurrentHealth);

	RecalculateHPBarPercent();
	UpdateHPText();
}

void UPragmataHUDViewModel::BindHealthComponent(UPragmataHealthComponent* HealthComponent)
{
	if (BoundHealthComponent.IsValid())
	{
		BoundHealthComponent->OnHealthChanged.RemoveDynamic(this, &UPragmataHUDViewModel::OnHealthChanged_Handler);
	}

	BoundHealthComponent = HealthComponent;

	if (!HealthComponent)
	{
		SetHealthValues(0.f, 0.f);
		return;
	}

	HealthComponent->OnHealthChanged.AddDynamic(this, &UPragmataHUDViewModel::OnHealthChanged_Handler);
	SetHealthValues(HealthComponent->GetCurrentHealth(), HealthComponent->GetMaxHealth());
}

void UPragmataHUDViewModel::UnbindHealthComponent()
{
	if (BoundHealthComponent.IsValid())
	{
		BoundHealthComponent->OnHealthChanged.RemoveDynamic(this, &UPragmataHUDViewModel::OnHealthChanged_Handler);
	}

	BoundHealthComponent.Reset();
	SetHealthValues(0.f, 0.f);
}

void UPragmataHUDViewModel::SetRecoveryItemCount(int32 NewCurrentCount, int32 NewMaxCount)
{
	const int32 ClampedMaxCount = FMath::Clamp(NewMaxCount, 0, 7);
	const int32 ClampedCurrentCount = FMath::Clamp(NewCurrentCount, 0, ClampedMaxCount);

	UE_MVVM_SET_PROPERTY_VALUE(MaxRecoveryItemCount, ClampedMaxCount);
	UE_MVVM_SET_PROPERTY_VALUE(RecoveryItemCount, ClampedCurrentCount);
}

void UPragmataHUDViewModel::SetRecoveryHoldProgress(float NewProgress)
{
	UE_MVVM_SET_PROPERTY_VALUE(RecoveryHoldProgress, FMath::Clamp(NewProgress, 0.f, 1.f));
}

void UPragmataHUDViewModel::SetRecoveryHolding(bool bNewHolding)
{
	UE_MVVM_SET_PROPERTY_VALUE(bRecoveryHolding, bNewHolding);
}

void UPragmataHUDViewModel::SetRecoveryProhibited(bool bNewProhibited)
{
	UE_MVVM_SET_PROPERTY_VALUE(bRecoveryProhibited, bNewProhibited);
}

void UPragmataHUDViewModel::SetCanUseHeal(bool bNewCanUseHeal)
{
	UE_MVVM_SET_PROPERTY_VALUE(bCanUseHeal, bNewCanUseHeal);
	SetRecoveryProhibited(!bNewCanUseHeal);
}

void UPragmataHUDViewModel::UpdateHPText()
{
	const int32 RoundedCurrentHealth = FMath::RoundToInt(CurrentHealth);
	const int32 RoundedMaxHealth = FMath::RoundToInt(MaxHealth);

	UE_MVVM_SET_PROPERTY_VALUE(HPText, FText::Format(INVTEXT("{0} / {1}"), FText::AsNumber(RoundedCurrentHealth), FText::AsNumber(RoundedMaxHealth)));
}

void UPragmataHUDViewModel::RecalculateHPBarPercent()
{
	const float NewPercent = MaxHealth > UE_KINDA_SMALL_NUMBER ? CurrentHealth / MaxHealth : 0.f;

	UE_MVVM_SET_PROPERTY_VALUE(HPBarPercent, FMath::Clamp(NewPercent, 0.f, 1.f));
}

// UI-facing gain delegate는 1초 동안 raw gain을 모은 뒤 한 번만 broadcast
void UPragmataHUDViewModel::QueueLunafilamentGainForUI(int32 Delta, int32 OldTotal, int32 NewTotal)
{
	const int32 ClampedDelta = FMath::Max(0, Delta);
	if (ClampedDelta <= 0)
	{
		return;
	}

	if (!bHasPendingLunafilamentUIBatch)
	{
		PendingLunafilamentUIOldTotal = FMath::Max(0, OldTotal);
		PendingLunafilamentUIDelta = 0;
		bHasPendingLunafilamentUIBatch = true;
	}

	const int64 RequestedDelta = static_cast<int64>(PendingLunafilamentUIDelta) + static_cast<int64>(ClampedDelta);
	PendingLunafilamentUIDelta = static_cast<int32>(FMath::Clamp<int64>(RequestedDelta, 0, MAX_int32));
	PendingLunafilamentUINewTotal = FMath::Max(0, NewTotal);
	StartLunafilamentGainBatchTimer();
}

// 첫 raw gain부터 1초 뒤 UI-facing delegate를 한 번만 호출
void UPragmataHUDViewModel::StartLunafilamentGainBatchTimer()
{
	UWorld* World = BoundPlayerStatsComponent.IsValid() ? BoundPlayerStatsComponent->GetWorld() : nullptr;
	if (!World)
	{
		FlushLunafilamentGainForUI();
		return;
	}

	FTimerManager& TimerManager = World->GetTimerManager();
	if (TimerManager.IsTimerActive(LunafilamentGainBatchTimerHandle) || TimerManager.IsTimerPaused(LunafilamentGainBatchTimerHandle))
	{
		return;
	}

	TimerManager.SetTimer(
		LunafilamentGainBatchTimerHandle,
		this,
		&UPragmataHUDViewModel::FlushLunafilamentGainForUI,
		GetLunafilamentGainMergeWindowSeconds(),
		false);
}

// 바인딩 변경/파괴 시 기존 World timer를 정리
void UPragmataHUDViewModel::ClearLunafilamentGainBatchTimer()
{
	if (BoundPlayerStatsComponent.IsValid())
	{
		if (UWorld* World = BoundPlayerStatsComponent->GetWorld())
		{
			World->GetTimerManager().ClearTimer(LunafilamentGainBatchTimerHandle);
		}
	}
}

// 수집된 delta를 UI-facing delegate로 전송
void UPragmataHUDViewModel::FlushLunafilamentGainForUI()
{
	if (!bHasPendingLunafilamentUIBatch || PendingLunafilamentUIDelta <= 0)
	{
		ResetPendingLunafilamentGainForUI();
		return;
	}

	OnLunafilamentGainedForUI.Broadcast(
		PendingLunafilamentUIDelta,
		PendingLunafilamentUIOldTotal,
		PendingLunafilamentUINewTotal);
	ResetPendingLunafilamentGainForUI();
}

// 다음 batch가 이전 total을 섞지 않도록 pending 상태 초기화
void UPragmataHUDViewModel::ResetPendingLunafilamentGainForUI()
{
	PendingLunafilamentUIDelta = 0;
	PendingLunafilamentUIOldTotal = 0;
	PendingLunafilamentUINewTotal = 0;
	bHasPendingLunafilamentUIBatch = false;
}

void UPragmataHUDViewModel::OnOverDriveChanged_Handler(float NewPercent)
{
	SetOverDrivePercent(NewPercent);
}

void UPragmataHUDViewModel::OnCleanseCooldownChanged_Handler(bool bNewCanUseCleanse, float NewCooldownProgress)
{
	SetCleanseCooldownValues(bNewCanUseCleanse, NewCooldownProgress);
}

void UPragmataHUDViewModel::OnHealthChanged_Handler(float NewCurrentHealth, float NewMaxHealth)
{
	SetHealthValues(NewCurrentHealth, NewMaxHealth);
}

void UPragmataHUDViewModel::OnStaminaChanged_Handler(float NewCurrentStamina, float NewMaxStamina, bool bNewLocked)
{
	SetBoostValues(NewCurrentStamina, NewMaxStamina, bNewLocked);
}

void UPragmataHUDViewModel::OnHealAvailabilityChanged_Handler(bool bNewCanUseHeal, int32 RemainingHealCartridges)
{
	SetCanUseHeal(bNewCanUseHeal);
	SetRecoveryItemCount(RemainingHealCartridges, MaxRecoveryItemCount);
}

void UPragmataHUDViewModel::OnHealHoldChanged_Handler(float HoldProgress, bool bHolding)
{
	SetRecoveryHolding(bHolding);
	SetRecoveryHoldProgress(HoldProgress);
}

void UPragmataHUDViewModel::OnLunafilamentChanged_Handler(int32 NewLunafilament)
{
	SetLunafilament(NewLunafilament);
}

void UPragmataHUDViewModel::OnLunafilamentGained_Handler(int32 Delta, int32 OldTotal, int32 NewTotal)
{
	// WBP가 gameplay component를 직접 몰라도 획득 연출 이벤트를 구독할 수 있게 전달
	UE_MVVM_SET_PROPERTY_VALUE(LastLunafilamentGainDelta, FMath::Max(0, Delta));
	UE_MVVM_SET_PROPERTY_VALUE(LastLunafilamentGainOldTotal, FMath::Max(0, OldTotal));
	UE_MVVM_SET_PROPERTY_VALUE(LastLunafilamentGainNewTotal, FMath::Max(0, NewTotal));
	UE_MVVM_SET_PROPERTY_VALUE(LunafilamentGainSequence, LunafilamentGainSequence == MAX_int32 ? 1 : LunafilamentGainSequence + 1);
	QueueLunafilamentGainForUI(LastLunafilamentGainDelta, LastLunafilamentGainOldTotal, LastLunafilamentGainNewTotal);
}

void UPragmataHUDViewModel::OnDamageMultiplierChanged_Handler(float NewMultiplier)
{
	SetPlayerDamageMultiplier(NewMultiplier);
}

void UPragmataHUDViewModel::OnRepairCartridgeChanged_Handler(int32 NewRepairCartridge, int32 NewMaxRepairCartridge)
{
	SetRecoveryItemCount(NewRepairCartridge, NewMaxRepairCartridge);
}
