// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/EnemyViewModel.h"
#include "Components/PragmataHealthComponent.h"

#include "GameFramework/Actor.h"

void UEnemyViewModel::SetEnemyTarget(AActor* TargetActor)
{
	// 타겟이 없으면 해제
	if (!TargetActor)
	{
		UnbindFromHealth();
		return;
	}

	// 타겟 적의 HealthComponent를 찾아 바인딩. 없으면(체력 없는 액터 등) 해제.
	UPragmataHealthComponent* HealthComp = TargetActor->FindComponentByClass<UPragmataHealthComponent>();
	if (!HealthComp)
	{
		UnbindFromHealth();
		return;
	}

	BindToHealth(HealthComp);
}

void UEnemyViewModel::BindToHealth(UPragmataHealthComponent* HealthComp)
{
	UnbindFromHealth();

	if (!HealthComp) return;

	BoundHealthComp = HealthComp;

	HealthComp->OnHealthChanged.AddDynamic(this, &UEnemyViewModel::HandleHealthChanged);
	HealthComp->OnDeath.AddDynamic(this, &UEnemyViewModel::HandleDeath);

	SyncFromHealth();
}

void UEnemyViewModel::UnbindFromHealth()
{
	if (BoundHealthComp.IsValid())
	{
		BoundHealthComp->OnHealthChanged.RemoveDynamic(this, &UEnemyViewModel::HandleHealthChanged);
		BoundHealthComp->OnDeath.RemoveDynamic(this, &UEnemyViewModel::HandleDeath);
	}
	BoundHealthComp.Reset();

	SetCurrentHealth(0.f);
	SetMaxHealth(0.f);
	SetHealthRatio(0.f);
	SetIsDead(false);
	// 그로기는 외부 푸시값이라 타겟 해제 시 함께 초기화
	SetGroggyValue(0.f);
	SetMaxGroggy(0.f);
	SetGroggyRatio(0.f);
}

void UEnemyViewModel::UpdateGroggy(float InCurrent, float InMax)
{
	SetGroggyValue(InCurrent);
	SetMaxGroggy(InMax);
	UpdateGroggyRatio();
}

void UEnemyViewModel::HandleHealthChanged(float InCurrentHealth, float InMaxHealth)
{
	SetCurrentHealth(InCurrentHealth);
	SetMaxHealth(InMaxHealth);
	UpdateHealthRatio();
}

void UEnemyViewModel::HandleDeath()
{
	SetIsDead(true);
	SetCurrentHealth(0.f);
	SetHealthRatio(0.f);
}

void UEnemyViewModel::SyncFromHealth()
{
	if (!BoundHealthComp.IsValid()) return;

	SetCurrentHealth(BoundHealthComp->GetCurrentHealth());
	SetMaxHealth(BoundHealthComp->GetMaxHealth());
	UpdateHealthRatio();
	SetIsDead(BoundHealthComp->IsDead());
}

void UEnemyViewModel::SetCurrentHealth(float NewValue)
{
	UE_MVVM_SET_PROPERTY_VALUE(CurrentHealth, NewValue);
}

void UEnemyViewModel::SetMaxHealth(float NewValue)
{
	UE_MVVM_SET_PROPERTY_VALUE(MaxHealth, NewValue);
}

void UEnemyViewModel::SetHealthRatio(float NewValue)
{
	UE_MVVM_SET_PROPERTY_VALUE(HealthRatio, NewValue);
}

void UEnemyViewModel::SetIsDead(bool bNewValue)
{
	UE_MVVM_SET_PROPERTY_VALUE(bIsDead, bNewValue);
}

void UEnemyViewModel::SetGroggyValue(float NewValue)
{
	UE_MVVM_SET_PROPERTY_VALUE(GroggyValue, NewValue);
}

void UEnemyViewModel::SetMaxGroggy(float NewValue)
{
	UE_MVVM_SET_PROPERTY_VALUE(MaxGroggy, NewValue);
}

void UEnemyViewModel::SetGroggyRatio(float NewValue)
{
	UE_MVVM_SET_PROPERTY_VALUE(GroggyRatio, NewValue);
}

void UEnemyViewModel::UpdateHealthRatio()
{
	const float Ratio = (MaxHealth > 0.f)
		? FMath::Clamp(CurrentHealth / MaxHealth, 0.f, 1.f)
		: 0.f;
	SetHealthRatio(Ratio);
}

void UEnemyViewModel::UpdateGroggyRatio()
{
	const float Ratio = (MaxGroggy > 0.f)
		? FMath::Clamp(GroggyValue / MaxGroggy, 0.f, 1.f)
		: 0.f;
	SetGroggyRatio(Ratio);
}
