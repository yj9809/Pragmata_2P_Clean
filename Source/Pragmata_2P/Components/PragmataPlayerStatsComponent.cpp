#include "Components/PragmataPlayerStatsComponent.h"

#include "Components/PragmataHealthComponent.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

UPragmataPlayerStatsComponent::UPragmataPlayerStatsComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UPragmataPlayerStatsComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UPragmataPlayerStatsComponent, Lunafilament);
	DOREPLIFETIME(UPragmataPlayerStatsComponent, PlayerDamageMultiplier);
	DOREPLIFETIME(UPragmataPlayerStatsComponent, RepairCartridge);
	DOREPLIFETIME(UPragmataPlayerStatsComponent, MaxRepairCartridge);
	DOREPLIFETIME(UPragmataPlayerStatsComponent, RepairCartridgeRecoveryAmount);
	DOREPLIFETIME(UPragmataPlayerStatsComponent, SuitUpgradeLevel);
	DOREPLIFETIME(UPragmataPlayerStatsComponent, WeaponUpgradeLevel);
}

void UPragmataPlayerStatsComponent::BeginPlay()
{
	Super::BeginPlay();

	if (HasServerAuthority())
	{
		RefreshUpgradeEffects();
	}
}

int32 UPragmataPlayerStatsComponent::GetUpgradeLevel(EPragmataUpgradeType UpgradeType) const
{
	switch (UpgradeType)
	{
	case EPragmataUpgradeType::Suit:
		return SuitUpgradeLevel;
	case EPragmataUpgradeType::Weapon:
		return WeaponUpgradeLevel;
	default:
		return 0;
	}
}

int32 UPragmataPlayerStatsComponent::GetMaxUpgradeLevel(EPragmataUpgradeType UpgradeType) const
{
	return UpgradeType == EPragmataUpgradeType::Suit || UpgradeType == EPragmataUpgradeType::Weapon
		? FMath::Max(0, MaxUpgradeLevel)
		: 0;
}

int32 UPragmataPlayerStatsComponent::GetUpgradeCost(EPragmataUpgradeType UpgradeType) const
{
	return UpgradeType == EPragmataUpgradeType::Suit || UpgradeType == EPragmataUpgradeType::Weapon
		? FMath::Max(0, LunafilamentCostPerUpgrade)
		: 0;
}

bool UPragmataPlayerStatsComponent::CanUpgrade(EPragmataUpgradeType UpgradeType) const
{
	const int32 UpgradeCost = GetUpgradeCost(UpgradeType);
	return CanApplyUpgradeType(UpgradeType)
		&& GetUpgradeLevel(UpgradeType) < GetMaxUpgradeLevel(UpgradeType)
		&& UpgradeCost > 0
		&& Lunafilament >= UpgradeCost;
}

bool UPragmataPlayerStatsComponent::CanRefundUpgrade(EPragmataUpgradeType UpgradeType) const
{
	return CanApplyUpgradeType(UpgradeType) && GetUpgradeLevel(UpgradeType) > 0;
}

bool UPragmataPlayerStatsComponent::AddLunafilament(int32 Amount)
{
	// 재화 획득은 서버 총액과 UI 연출용 delta를 분리해서 알림
	if (!HasServerAuthority() || Amount <= 0)
	{
		return false;
	}

	const int32 OldTotal = Lunafilament;
	const int64 RequestedTotal = static_cast<int64>(Lunafilament) + static_cast<int64>(Amount);
	const int32 ClampedTotal = static_cast<int32>(FMath::Clamp<int64>(RequestedTotal, 0, MAX_int32));
	if (Lunafilament == ClampedTotal)
	{
		return false;
	}

	Lunafilament = ClampedTotal;
	OnLunafilamentChanged.Broadcast(Lunafilament);
	BroadcastLunafilamentGained(OldTotal, Lunafilament);
	return true;
}

bool UPragmataPlayerStatsComponent::SpendLunafilament(int32 Amount)
{
	// 강화/구매는 서버 보유 재화가 충분할 때만 통과
	if (!HasServerAuthority() || Amount <= 0 || Lunafilament < Amount)
	{
		return false;
	}

	Lunafilament -= Amount;
	OnLunafilamentChanged.Broadcast(Lunafilament);
	return true;
}

void UPragmataPlayerStatsComponent::SetPlayerDamageMultiplier(float NewMultiplier)
{
	// 음수 배율이 damage/heal 의미를 뒤집지 않게 서버에서 보정
	if (!HasServerAuthority())
	{
		return;
	}

	const float ClampedMultiplier = FMath::Max(0.f, NewMultiplier);
	if (FMath::IsNearlyEqual(PlayerDamageMultiplier, ClampedMultiplier))
	{
		return;
	}

	PlayerDamageMultiplier = ClampedMultiplier;
	OnDamageMultiplierChanged.Broadcast(PlayerDamageMultiplier);
}

bool UPragmataPlayerStatsComponent::ApplyDecodeWeaponDamageBonus(float Multiplier, float DurationSeconds)
{
	// Decode 보너스는 server가 Hugh 무기 피해에만 적용하고 재획득 시 지속시간을 갱신
	if (!HasServerAuthority() || Multiplier <= 0.f || DurationSeconds <= 0.f)
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	DecodeWeaponDamageMultiplier = Multiplier;
	World->GetTimerManager().SetTimer(
		DecodeWeaponDamageBonusTimerHandle,
		this,
		&ThisClass::ClearDecodeWeaponDamageBonus,
		DurationSeconds,
		false);
	return true;
}

void UPragmataPlayerStatsComponent::ClearDecodeWeaponDamageBonus()
{
	// 만료 뒤 영구 weapon upgrade 배율은 보존하고 Decode 몫만 제거
	DecodeWeaponDamageMultiplier = 1.f;
}

void UPragmataPlayerStatsComponent::SetRepairCartridge(int32 NewCount)
{
	// 회복 아이템 수량은 최대 슬롯 범위 안에서만 서버가 확정
	if (!HasServerAuthority())
	{
		return;
	}

	const int32 ClampedMaxCount = FMath::Clamp(MaxRepairCartridge, 0, 7);
	const int32 ClampedCount = FMath::Clamp(NewCount, 0, ClampedMaxCount);
	if (RepairCartridge == ClampedCount && MaxRepairCartridge == ClampedMaxCount)
	{
		return;
	}

	MaxRepairCartridge = ClampedMaxCount;
	RepairCartridge = ClampedCount;
	BroadcastRepairCartridgeChanged();
}

bool UPragmataPlayerStatsComponent::AddRepairCartridge(int32 Amount)
{
	// 획득량이 0 이하이면 inventory 상태를 건드리지 않음
	if (!HasServerAuthority() || Amount <= 0)
	{
		return false;
	}

	const int32 PreviousCount = RepairCartridge;
	SetRepairCartridge(RepairCartridge + Amount);
	return RepairCartridge != PreviousCount;
}

bool UPragmataPlayerStatsComponent::ConsumeRepairCartridge()
{
	// 회복이 실제로 적용될 때만 cartridge를 소모해 full HP 낭비를 막음
	if (!HasServerAuthority() || RepairCartridge <= 0)
	{
		return false;
	}

	AActor* Owner = GetOwner();
	UPragmataHealthComponent* HealthComponent = Owner ? Owner->FindComponentByClass<UPragmataHealthComponent>() : nullptr;
	if (!HealthComponent || !HealthComponent->RecoverHP(RepairCartridgeRecoveryAmount))
	{
		return false;
	}

	RepairCartridge = FMath::Clamp(RepairCartridge - 1, 0, FMath::Clamp(MaxRepairCartridge, 0, 7));
	BroadcastRepairCartridgeChanged();
	return true;
}

bool UPragmataPlayerStatsComponent::TryUpgrade(EPragmataUpgradeType UpgradeType)
{
	// 강화는 서버에서 비용과 레벨 상한을 같은 틱에 확정
	if (!HasServerAuthority() || !CanUpgrade(UpgradeType))
	{
		return false;
	}

	const int32 UpgradeCost = GetUpgradeCost(UpgradeType);
	if (!SpendLunafilament(UpgradeCost))
	{
		return false;
	}

	if (UpgradeType == EPragmataUpgradeType::Suit)
	{
		ResolveBaseMaxHP();
	}

	const int32 NextLevel = GetUpgradeLevel(UpgradeType) + 1;
	if (!SetUpgradeLevel(UpgradeType, NextLevel))
	{
		AddLunafilament(UpgradeCost);
		return false;
	}

	ApplyUpgradeEffect(UpgradeType);
	BroadcastUpgradeLevelChanged(UpgradeType);
	return true;
}

bool UPragmataPlayerStatsComponent::TryRefundUpgrade(EPragmataUpgradeType UpgradeType)
{
	// 환급은 강화 레벨을 먼저 낮춘 뒤 같은 비용을 루나필라멘트로 되돌림
	if (!HasServerAuthority() || !CanRefundUpgrade(UpgradeType))
	{
		return false;
	}

	const int32 PreviousLevel = GetUpgradeLevel(UpgradeType);
	if (UpgradeType == EPragmataUpgradeType::Suit)
	{
		ResolveBaseMaxHP();
	}

	if (!SetUpgradeLevel(UpgradeType, PreviousLevel - 1))
	{
		return false;
	}

	ApplyUpgradeEffect(UpgradeType);
	AddLunafilament(GetUpgradeCost(UpgradeType));
	BroadcastUpgradeLevelChanged(UpgradeType);
	return true;
}

void UPragmataPlayerStatsComponent::RefreshUpgradeEffects()
{
	// Hugh 기본 HP가 BeginPlay에서 다시 고정된 뒤 강화 수치를 재적용
	if (!HasServerAuthority())
	{
		return;
	}

	if (SuitUpgradeLevel == 0)
	{
		CachedBaseMaxHP = 0;
	}
	if (CachedBaseMaxHP <= 0)
	{
		const AActor* Owner = GetOwner();
		const UPragmataHealthComponent* HealthComponent = Owner ? Owner->FindComponentByClass<UPragmataHealthComponent>() : nullptr;
		CachedBaseMaxHP = HealthComponent ? HealthComponent->GetMaxHP() : 0;
	}

	ApplyUpgradeEffect(EPragmataUpgradeType::Suit);
	ApplyUpgradeEffect(EPragmataUpgradeType::Weapon);
}

void UPragmataPlayerStatsComponent::OnRep_Lunafilament(int32 PreviousLunafilament)
{
	// client UI도 복제 전후 차이로 획득 연출 이벤트를 복원
	OnLunafilamentChanged.Broadcast(Lunafilament);
	BroadcastLunafilamentGained(PreviousLunafilament, Lunafilament);
}

void UPragmataPlayerStatsComponent::OnRep_PlayerDamageMultiplier()
{
	OnDamageMultiplierChanged.Broadcast(PlayerDamageMultiplier);
}

void UPragmataPlayerStatsComponent::OnRep_RepairCartridgeSnapshot()
{
	BroadcastRepairCartridgeChanged();
}

void UPragmataPlayerStatsComponent::OnRep_SuitUpgradeLevel()
{
	BroadcastUpgradeLevelChanged(EPragmataUpgradeType::Suit);
}

void UPragmataPlayerStatsComponent::OnRep_WeaponUpgradeLevel()
{
	BroadcastUpgradeLevelChanged(EPragmataUpgradeType::Weapon);
}

bool UPragmataPlayerStatsComponent::HasServerAuthority() const
{
	const AActor* Owner = GetOwner();
	return Owner && Owner->HasAuthority();
}

void UPragmataPlayerStatsComponent::BroadcastLunafilamentGained(int32 OldTotal, int32 NewTotal)
{
	const int32 Delta = NewTotal - OldTotal;
	if (Delta <= 0)
	{
		return;
	}

	OnLunafilamentGained.Broadcast(Delta, OldTotal, NewTotal);
}

void UPragmataPlayerStatsComponent::BroadcastRepairCartridgeChanged()
{
	OnRepairCartridgeChanged.Broadcast(RepairCartridge, MaxRepairCartridge);
}

void UPragmataPlayerStatsComponent::BroadcastUpgradeLevelChanged(EPragmataUpgradeType UpgradeType)
{
	OnUpgradeLevelChanged.Broadcast(UpgradeType, GetUpgradeLevel(UpgradeType));
}

bool UPragmataPlayerStatsComponent::SetUpgradeLevel(EPragmataUpgradeType UpgradeType, int32 NewLevel)
{
	const int32 ClampedLevel = FMath::Clamp(NewLevel, 0, GetMaxUpgradeLevel(UpgradeType));
	switch (UpgradeType)
	{
	case EPragmataUpgradeType::Suit:
		if (SuitUpgradeLevel == ClampedLevel)
		{
			return false;
		}
		SuitUpgradeLevel = ClampedLevel;
		return true;
	case EPragmataUpgradeType::Weapon:
		if (WeaponUpgradeLevel == ClampedLevel)
		{
			return false;
		}
		WeaponUpgradeLevel = ClampedLevel;
		return true;
	default:
		return false;
	}
}

bool UPragmataPlayerStatsComponent::CanApplyUpgradeType(EPragmataUpgradeType UpgradeType) const
{
	if (UpgradeType == EPragmataUpgradeType::Weapon)
	{
		return true;
	}

	if (UpgradeType != EPragmataUpgradeType::Suit)
	{
		return false;
	}

	const AActor* Owner = GetOwner();
	return Owner && Owner->FindComponentByClass<UPragmataHealthComponent>() != nullptr;
}

void UPragmataPlayerStatsComponent::ApplyUpgradeEffect(EPragmataUpgradeType UpgradeType)
{
	switch (UpgradeType)
	{
	case EPragmataUpgradeType::Suit:
	{
		AActor* Owner = GetOwner();
		UPragmataHealthComponent* HealthComponent = Owner ? Owner->FindComponentByClass<UPragmataHealthComponent>() : nullptr;
		if (!HealthComponent)
		{
			return;
		}

		const int32 NewMaxHP = ResolveBaseMaxHP() + SuitUpgradeLevel * FMath::Max(0, MaxHPPerSuitUpgrade);
		HealthComponent->SetMaxHPFromUpgrade(NewMaxHP);
		break;
	}
	case EPragmataUpgradeType::Weapon:
		SetPlayerDamageMultiplier(1.f + WeaponUpgradeLevel * FMath::Max(0.f, DamageMultiplierPerWeaponUpgrade));
		break;
	default:
		break;
	}
}

int32 UPragmataPlayerStatsComponent::ResolveBaseMaxHP()
{
	if (CachedBaseMaxHP > 0)
	{
		return CachedBaseMaxHP;
	}

	const AActor* Owner = GetOwner();
	const UPragmataHealthComponent* HealthComponent = Owner ? Owner->FindComponentByClass<UPragmataHealthComponent>() : nullptr;
	if (!HealthComponent)
	{
		return 0;
	}

	CachedBaseMaxHP = FMath::Max(0, HealthComponent->GetMaxHP() - SuitUpgradeLevel * FMath::Max(0, MaxHPPerSuitUpgrade));
	return CachedBaseMaxHP;
}
