 #include "UI/WeaponWheelViewModel.h"

#include "Data/WeaponDataAsset.h"

UWeaponWheelViewModel::UWeaponWheelViewModel()
{
	// 추후 ViewModel 주입 시 그립 건은 초기 장비로 Primary 슬롯에 세팅
	PrimarySlotState.WeaponType = EWeaponType::Primary;
	AttackSlotState.WeaponType = EWeaponType::Attack;
	TacticalSlotState.WeaponType = EWeaponType::Tactical;
	DefenseSlotState.WeaponType = EWeaponType::Defense;
}

FWeaponWheelSlotState UWeaponWheelViewModel::GetWeaponSlotState(EWeaponType WeaponType) const
{
	return GetSlotState(WeaponType);
}

void UWeaponWheelViewModel::InitializeSlotTypes()
{
	PrimarySlotState.WeaponType = EWeaponType::Primary;
	AttackSlotState.WeaponType = EWeaponType::Attack;
	TacticalSlotState.WeaponType = EWeaponType::Tactical;
	DefenseSlotState.WeaponType = EWeaponType::Defense;

	BroadcastAllSlotsChanged();
}

void UWeaponWheelViewModel::SetWeaponSlot(EWeaponType WeaponType, UTexture2D* WeaponIcon, int32 CurrentAmmo, int32 MaxAmmo, bool bShowMaxAmmo)
{
	SetWeaponSlotWithName(WeaponType, FText::GetEmpty(), WeaponIcon, CurrentAmmo, MaxAmmo, bShowMaxAmmo);
}

void UWeaponWheelViewModel::SetWeaponSlotWithName(EWeaponType WeaponType, FText WeaponName, UTexture2D* WeaponIcon, int32 CurrentAmmo, int32 MaxAmmo, bool bShowMaxAmmo)
{
	const int32 ClampedMaxAmmo = FMath::Max(0, MaxAmmo);

	FWeaponWheelSlotState& SlotState = GetMutableSlotState(WeaponType);
	SlotState.WeaponType = WeaponType;
	SlotState.bHasWeapon = true;
	SlotState.CurrentAmmo = bShowMaxAmmo && ClampedMaxAmmo > 0 ? FMath::Clamp(CurrentAmmo, 0, ClampedMaxAmmo) : FMath::Max(0, CurrentAmmo);
	SlotState.MaxAmmo = ClampedMaxAmmo;
	SlotState.bShowMaxAmmo = bShowMaxAmmo;
	SlotState.WeaponIcon = WeaponIcon;
	SlotState.WeaponIconSize = FVector2D::ZeroVector;
	SlotState.WeaponName = WeaponName;

	BroadcastSlotChanged(WeaponType);
}

void UWeaponWheelViewModel::SetWeaponSlotFromData(UWeaponDataAsset* WeaponData, int32 CurrentAmmo)
{
	if (!WeaponData)
	{
		return;
	}

	SetWeaponSlotWithName(
		WeaponData->GetWeaponType(),
		WeaponData->GetWeaponName(),
		WeaponData->GetWeaponIcon(),
		CurrentAmmo,
		WeaponData->GetMaxAmmo(),
		WeaponData->ShouldShowMaxAmmo()
	);

	FWeaponWheelSlotState& SlotState = GetMutableSlotState(WeaponData->GetWeaponType());
	SlotState.WeaponIconSize = WeaponData->GetWeaponIconSize();
	BroadcastSlotChanged(WeaponData->GetWeaponType());
}

void UWeaponWheelViewModel::AcquireWeaponFromData(UWeaponDataAsset* WeaponData, int32 CurrentAmmo, bool bEquipWeapon)
{
	if (!WeaponData)
	{
		return;
	}

	SetWeaponSlotFromData(WeaponData, CurrentAmmo);
	if (bEquipWeapon)
	{
		SetEquippedWeaponSlot(WeaponData->GetWeaponType());
	}
}

void UWeaponWheelViewModel::AcquireWeaponFromDataWithDefaultAmmo(UWeaponDataAsset* WeaponData, bool bEquipWeapon)
{
	if (!WeaponData)
	{
		return;
	}

	AcquireWeaponFromData(WeaponData, WeaponData->GetDefaultCurrentAmmo(), bEquipWeapon);
}

void UWeaponWheelViewModel::ClearWeaponSlot(EWeaponType WeaponType)
{
	FWeaponWheelSlotState& SlotState = GetMutableSlotState(WeaponType);
	SlotState = FWeaponWheelSlotState();
	SlotState.WeaponType = WeaponType;

	BroadcastSlotChanged(WeaponType);
}

void UWeaponWheelViewModel::SetEquippedWeaponSlot(EWeaponType WeaponType)
{
	FWeaponWheelSlotState& NewEquippedSlot = GetMutableSlotState(WeaponType);
	if (!NewEquippedSlot.bHasWeapon)
	{
		return;
	}

	PrimarySlotState.bEquipped = false;
	AttackSlotState.bEquipped = false;
	TacticalSlotState.bEquipped = false;
	DefenseSlotState.bEquipped = false;

	NewEquippedSlot.bEquipped = true;
	EquippedWeaponType = WeaponType;

	BroadcastAllSlotsChanged();
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(EquippedWeaponType);
}

void UWeaponWheelViewModel::SetWeaponAmmo(EWeaponType WeaponType, int32 CurrentAmmo)
{
	FWeaponWheelSlotState& SlotState = GetMutableSlotState(WeaponType);
	if (!SlotState.bHasWeapon)
	{
		return;
	}

	SlotState.CurrentAmmo = SlotState.bShowMaxAmmo && SlotState.MaxAmmo > 0 ? FMath::Clamp(CurrentAmmo, 0, SlotState.MaxAmmo) : FMath::Max(0, CurrentAmmo);

	BroadcastSlotChanged(WeaponType);
}

// weapon data asset으로 세팅된 icon/name은 유지하고 서버 ammo 숫자만 반영
void UWeaponWheelViewModel::SetWeaponAmmoValues(EWeaponType WeaponType, int32 CurrentAmmo, int32 MaxAmmo)
{
	FWeaponWheelSlotState& SlotState = GetMutableSlotState(WeaponType);
	const int32 ClampedMaxAmmo = FMath::Max(0, MaxAmmo);

	SlotState.WeaponType = WeaponType;
	SlotState.bHasWeapon = true;
	SlotState.CurrentAmmo = ClampedMaxAmmo > 0 ? FMath::Clamp(CurrentAmmo, 0, ClampedMaxAmmo) : FMath::Max(0, CurrentAmmo);
	SlotState.MaxAmmo = ClampedMaxAmmo;
	SlotState.bShowMaxAmmo = ClampedMaxAmmo > 0;

	BroadcastSlotChanged(WeaponType);
}

FWeaponWheelSlotState& UWeaponWheelViewModel::GetMutableSlotState(EWeaponType WeaponType)
{
	switch (WeaponType)
	{
	case EWeaponType::Attack:
		return AttackSlotState;
	case EWeaponType::Tactical:
		return TacticalSlotState;
	case EWeaponType::Defense:
		return DefenseSlotState;
	case EWeaponType::Primary:
	default:
		return PrimarySlotState;
	}
}

const FWeaponWheelSlotState& UWeaponWheelViewModel::GetSlotState(EWeaponType WeaponType) const
{
	switch (WeaponType)
	{
	case EWeaponType::Attack:
		return AttackSlotState;
	case EWeaponType::Tactical:
		return TacticalSlotState;
	case EWeaponType::Defense:
		return DefenseSlotState;
	case EWeaponType::Primary:
	default:
		return PrimarySlotState;
	}
}

void UWeaponWheelViewModel::BroadcastSlotChanged(EWeaponType WeaponType)
{
	switch (WeaponType)
	{
	case EWeaponType::Attack:
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(AttackSlotState);
		break;
	case EWeaponType::Tactical:
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(TacticalSlotState);
		break;
	case EWeaponType::Defense:
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(DefenseSlotState);
		break;
	case EWeaponType::Primary:
	default:
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(PrimarySlotState);
		break;
	}
}

void UWeaponWheelViewModel::BroadcastAllSlotsChanged()
{
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(PrimarySlotState);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(AttackSlotState);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(TacticalSlotState);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(DefenseSlotState);
}
