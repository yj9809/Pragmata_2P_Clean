// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/WeaponWheelWidget.h"

#include "MVVMSubsystem.h"
#include "Characters/HughCharacter.h"
#include "Data/WeaponDataAsset.h"
#include "UI/WeaponWheelViewModel.h"
#include "View/MVVMView.h"
#include "Weapon/WeaponBase.h"

void UWeaponWheelWidget::NativeDestruct()
{
	UnbindWeaponSource();

	Super::NativeDestruct();
}

void UWeaponWheelWidget::SetWeaponWheelVisible(bool bNewVisible)
{
	SetVisibility(bNewVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

// Hugh role pointer 기준으로 weapon ammo source를 구독
void UWeaponWheelWidget::BindWeaponSource(AHughCharacter* HughCharacter)
{
	if (BoundHughCharacter.IsValid())
	{
		BoundHughCharacter->OnCurrentWeaponChanged.RemoveDynamic(this, &UWeaponWheelWidget::HandleCurrentWeaponChanged);
		BoundHughCharacter->OnWeaponInventoryChanged.RemoveDynamic(this, &UWeaponWheelWidget::HandleWeaponInventoryChanged);
	}

	BoundHughCharacter = HughCharacter;

	if (!HughCharacter)
	{
		UnbindWeapon();
		return;
	}

	HughCharacter->OnCurrentWeaponChanged.AddUniqueDynamic(this, &UWeaponWheelWidget::HandleCurrentWeaponChanged);
	HughCharacter->OnWeaponInventoryChanged.AddUniqueDynamic(this, &UWeaponWheelWidget::HandleWeaponInventoryChanged);
	BindWeapon(HughCharacter->GetCurrentWeapon());
	RefreshWeaponInventory();
}

void UWeaponWheelWidget::UnbindWeaponSource()
{
	if (BoundHughCharacter.IsValid())
	{
		BoundHughCharacter->OnCurrentWeaponChanged.RemoveDynamic(this, &UWeaponWheelWidget::HandleCurrentWeaponChanged);
		BoundHughCharacter->OnWeaponInventoryChanged.RemoveDynamic(this, &UWeaponWheelWidget::HandleWeaponInventoryChanged);
	}

	BoundHughCharacter.Reset();
	UnbindWeapon();
}

// 현재 weapon이 바뀔 때마다 ammo delegate를 새 actor로 갈아탐
void UWeaponWheelWidget::BindWeapon(AWeaponBase* Weapon)
{
	UnbindWeapon();
	BoundWeapon = Weapon;

	if (!Weapon)
	{
		return;
	}

	// 무기를 획득/장착하는 순간 무기의 DataAsset을 읽어 휠 슬롯에 등록(아이콘/이름/타입) + 장착 하이라이트
	if (UWeaponWheelViewModel* ViewModel = ResolveWeaponWheelViewModel())
	{
		if (UWeaponDataAsset* Data = Weapon->GetWeaponData())
		{
			ViewModel->AcquireWeaponFromData(Data, Weapon->GetCurAmmo(), /*bEquipWeapon*/ true);
		}
	}

	Weapon->OnAmmoChange.AddUniqueDynamic(this, &UWeaponWheelWidget::HandleAmmoChanged);
	RefreshWeaponAmmo();  // 라이브 탄약으로 한 번 더 정확히 맞춤
}

void UWeaponWheelWidget::UnbindWeapon()
{
	if (BoundWeapon.IsValid())
	{
		BoundWeapon->OnAmmoChange.RemoveDynamic(this, &UWeaponWheelWidget::HandleAmmoChanged);
	}

	BoundWeapon.Reset();
}

void UWeaponWheelWidget::RefreshWeaponAmmo()
{
	AWeaponBase* Weapon = BoundWeapon.Get();
	UWeaponWheelViewModel* ViewModel = ResolveWeaponWheelViewModel();
	if (!Weapon || !ViewModel)
	{
		return;
	}

	const EWeaponType WeaponType = Weapon->GetWeaponType();

	// 탄약이 바닥난 비-Primary 무기는 슬롯을 비워 EmptyImage가 다시 보이게 한다.
	// SetWeaponAmmoValues가 이미 비워진 슬롯을 아이콘 없는 0/N 유령으로 되살리는 것을 막음
	// (RefreshWeaponInventory와 동일한 노출 규칙)
	if (WeaponType != EWeaponType::Primary && Weapon->GetCurAmmo() <= 0)
	{
		ViewModel->ClearWeaponSlot(WeaponType);
		return;
	}

	ViewModel->SetWeaponAmmoValues(WeaponType, Weapon->GetCurAmmo(), Weapon->GetMaxAmmo());
	ViewModel->SetEquippedWeaponSlot(WeaponType);
}

// weapon wheel slot은 inventory와 ammo가 있는 weapon만 표시
void UWeaponWheelWidget::RefreshWeaponInventory()
{
	AHughCharacter* HughCharacter = BoundHughCharacter.Get();
	UWeaponWheelViewModel* ViewModel = ResolveWeaponWheelViewModel();
	if (!HughCharacter || !ViewModel)
	{
		return;
	}

	const TArray<AWeaponBase*>& WeaponInventory = HughCharacter->GetWeaponInventory();
	const EWeaponType SlotTypes[] =
	{
		EWeaponType::Primary,
		EWeaponType::Attack,
		EWeaponType::Tactical,
		EWeaponType::Defense
	};

	for (EWeaponType SlotType : SlotTypes)
	{
		const int32 SlotIndex = static_cast<int32>(SlotType);
		AWeaponBase* Weapon = WeaponInventory.IsValidIndex(SlotIndex) ? WeaponInventory[SlotIndex] : nullptr;
		const bool bPrimarySlot = SlotType == EWeaponType::Primary;
		const bool bVisibleSlot = IsValid(Weapon) && (bPrimarySlot || Weapon->GetCurAmmo() > 0);

		if (bVisibleSlot)
		{
			// DataAsset의 아이콘/이름/타입을 슬롯에 등록한다.
			// 아직 해당 아이콘으로 등록되지 않은 슬롯만 갱신해 매 ammo 갱신마다 재설정/브로드캐스트되는 것을 막음
			if (UWeaponDataAsset* Data = Weapon->GetWeaponData())
			{
				if (ViewModel->GetWeaponSlotState(SlotType).WeaponIcon != Data->GetWeaponIcon())
				{
					ViewModel->SetWeaponSlotFromData(Data, Weapon->GetCurAmmo());
				}
			}

			// 탄약 수치는 라이브 무기 액터 기준으로 반영 (DataAsset MaxAmmo가 비어 있어도 정확)
			ViewModel->SetWeaponAmmoValues(SlotType, Weapon->GetCurAmmo(), Weapon->GetMaxAmmo());
		}
		else if (!bPrimarySlot)
		{
			ViewModel->ClearWeaponSlot(SlotType);
		}
	}

	if (AWeaponBase* CurrentWeapon = HughCharacter->GetCurrentWeapon())
	{
		const bool bPrimaryWeapon = CurrentWeapon->GetWeaponType() == EWeaponType::Primary;
		if (bPrimaryWeapon || CurrentWeapon->GetCurAmmo() > 0)
		{
			ViewModel->SetEquippedWeaponSlot(CurrentWeapon->GetWeaponType());
		}
	}
}

UWeaponWheelViewModel* UWeaponWheelWidget::ResolveWeaponWheelViewModel() const
{
	UMVVMView* View = UMVVMSubsystem::GetViewFromUserWidget(this);
	if (!View)
	{
		return nullptr;
	}

	return Cast<UWeaponWheelViewModel>(View->GetViewModel(TEXT("WeaponWheelViewModel")).GetObject());
}

void UWeaponWheelWidget::HandleCurrentWeaponChanged(AWeaponBase* NewWeapon)
{
	BindWeapon(NewWeapon);
	RefreshWeaponInventory();
}

void UWeaponWheelWidget::HandleWeaponInventoryChanged()
{
	RefreshWeaponInventory();
}

void UWeaponWheelWidget::HandleAmmoChanged(int32 /*CurAmmo*/, int32 /*MaxAmmo*/)
{
	RefreshWeaponInventory();
}
