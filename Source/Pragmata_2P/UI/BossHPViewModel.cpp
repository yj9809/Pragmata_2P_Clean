#include "UI/BossHPViewModel.h"

#include "Components/PragmataHealthComponent.h"
#include "GameFramework/Actor.h"

void UBossHPViewModel::BeginDestroy()
{
	if (BoundHealthComponent.IsValid())
	{
		BoundHealthComponent->OnHealthChanged.RemoveDynamic(this, &UBossHPViewModel::OnBossHealthChanged_Handler);
		BoundHealthComponent->OnDeath.RemoveDynamic(this, &UBossHPViewModel::OnBossDeath_Handler);
		BoundHealthComponent.Reset();
	}

	Super::BeginDestroy();
}

void UBossHPViewModel::SetBossTarget(AActor* NewBossTarget)
{
	if (BossTarget.Get() == NewBossTarget)
	{
		if (NewBossTarget && !BoundHealthComponent.IsValid())
		{
			BindHealthComponent(NewBossTarget->FindComponentByClass<UPragmataHealthComponent>());
		}
		else if (BoundHealthComponent.IsValid())
		{
			SetBossHealthValues(BoundHealthComponent->GetCurrentHealth(), BoundHealthComponent->GetMaxHealth());
			SetBossHPVisible(!BoundHealthComponent->IsDead());
		}
		return;
	}

	BossTarget = NewBossTarget;

	if (!NewBossTarget)
	{
		SetBossNameText(FText::GetEmpty());
		UnbindHealthComponent();
		SetBossGroggyPercent(0.f);
		SetBossHPVisible(false);
		return;
	}

	SetBossNameText(FText::FromString(NewBossTarget->GetName()));
	BindHealthComponent(NewBossTarget->FindComponentByClass<UPragmataHealthComponent>());
}

void UBossHPViewModel::BindHealthComponent(UPragmataHealthComponent* HealthComponent)
{
	if (BoundHealthComponent.Get() == HealthComponent)
	{
		if (HealthComponent)
		{
			SetBossHealthValues(HealthComponent->GetCurrentHealth(), HealthComponent->GetMaxHealth());
			SetBossHPVisible(!HealthComponent->IsDead());
		}
		return;
	}

	UnbindHealthComponent();

	BoundHealthComponent = HealthComponent;
	if (!HealthComponent)
	{
		SetBossHealthValues(0.f, 0.f);
		SetBossHPVisible(false);
		return;
	}

	// 보스 HUD용 ViewModel이 gameplay component delegate를 구독하고 WBP에는 FieldNotify 값만 노출한다.
	HealthComponent->OnHealthChanged.AddDynamic(this, &UBossHPViewModel::OnBossHealthChanged_Handler);
	HealthComponent->OnDeath.AddDynamic(this, &UBossHPViewModel::OnBossDeath_Handler);
	SetBossHealthValues(HealthComponent->GetCurrentHealth(), HealthComponent->GetMaxHealth());
	SetBossHPVisible(!HealthComponent->IsDead());
}

void UBossHPViewModel::UnbindHealthComponent()
{
	if (BoundHealthComponent.IsValid())
	{
		BoundHealthComponent->OnHealthChanged.RemoveDynamic(this, &UBossHPViewModel::OnBossHealthChanged_Handler);
		BoundHealthComponent->OnDeath.RemoveDynamic(this, &UBossHPViewModel::OnBossDeath_Handler);
	}

	BoundHealthComponent.Reset();
	SetBossHealthValues(0.f, 0.f);
}

void UBossHPViewModel::SetBossHealthValues(float NewCurrentHealth, float NewMaxHealth)
{
	const float ClampedMaxHealth = FMath::Max(0.f, NewMaxHealth);
	const float ClampedCurrentHealth = FMath::Clamp(NewCurrentHealth, 0.f, ClampedMaxHealth);

	UE_MVVM_SET_PROPERTY_VALUE(BossMaxHealth, ClampedMaxHealth);
	UE_MVVM_SET_PROPERTY_VALUE(BossCurrentHealth, ClampedCurrentHealth);

	RecalculateBossHealthPercent();
	UpdateBossHPText();
	BroadcastBossHealth();
}

void UBossHPViewModel::SetBossHealthPercent(float NewPercent)
{
	UE_MVVM_SET_PROPERTY_VALUE(BossHealthPercent, FMath::Clamp(NewPercent, 0.f, 1.f));
	BroadcastBossHealth();
}

void UBossHPViewModel::SetBossGroggyValue(float NewCurrentGroggy, float NewMaxGroggy)
{
	const float ClampedMaxGroggy = FMath::Max(0.f, NewMaxGroggy);
	const float ClampedCurrentGroggy = FMath::Clamp(NewCurrentGroggy, 0.f, ClampedMaxGroggy);
	const float NewPercent = ClampedMaxGroggy > UE_KINDA_SMALL_NUMBER
		? ClampedCurrentGroggy / ClampedMaxGroggy
		: 0.f;

	UE_MVVM_SET_PROPERTY_VALUE(BossMaxGroggy, ClampedMaxGroggy);
	UE_MVVM_SET_PROPERTY_VALUE(BossCurrentGroggy, ClampedCurrentGroggy);
	UE_MVVM_SET_PROPERTY_VALUE(BossGroggyPercent, FMath::Clamp(NewPercent, 0.f, 1.f));
	BroadcastBossGroggy();
}

void UBossHPViewModel::SetBossGroggyPercent(float NewPercent)
{
	UE_MVVM_SET_PROPERTY_VALUE(BossGroggyPercent, FMath::Clamp(NewPercent, 0.f, 1.f));
	BroadcastBossGroggy();
}

void UBossHPViewModel::SetBossHPVisible(bool bNewVisible)
{
	UE_MVVM_SET_PROPERTY_VALUE(bBossHPVisible, bNewVisible);
	OnBossHPVisibilityChangedForUI.Broadcast(bBossHPVisible);
}

void UBossHPViewModel::SetBossNameText(const FText& NewNameText)
{
	UE_MVVM_SET_PROPERTY_VALUE(BossNameText, NewNameText);
	OnBossNameChangedForUI.Broadcast(BossNameText);
}

void UBossHPViewModel::RecalculateBossHealthPercent()
{
	const float NewPercent = BossMaxHealth > UE_KINDA_SMALL_NUMBER
		? BossCurrentHealth / BossMaxHealth
		: 0.f;

	UE_MVVM_SET_PROPERTY_VALUE(BossHealthPercent, FMath::Clamp(NewPercent, 0.f, 1.f));
}

void UBossHPViewModel::UpdateBossHPText()
{
	const int32 RoundedCurrentHealth = FMath::RoundToInt(BossCurrentHealth);
	const int32 RoundedMaxHealth = FMath::RoundToInt(BossMaxHealth);
	UE_MVVM_SET_PROPERTY_VALUE(BossHPText, FText::Format(INVTEXT("{0} / {1}"), FText::AsNumber(RoundedCurrentHealth), FText::AsNumber(RoundedMaxHealth)));
}

void UBossHPViewModel::BroadcastBossHealth()
{
	OnBossHealthChangedForUI.Broadcast(BossCurrentHealth, BossMaxHealth, BossHealthPercent);
}

void UBossHPViewModel::BroadcastBossGroggy()
{
	OnBossGroggyChangedForUI.Broadcast(BossCurrentGroggy, BossMaxGroggy, BossGroggyPercent);
}

void UBossHPViewModel::OnBossHealthChanged_Handler(float NewCurrentHealth, float NewMaxHealth)
{
	SetBossHealthValues(NewCurrentHealth, NewMaxHealth);
}

void UBossHPViewModel::OnBossDeath_Handler()
{
	SetBossHealthValues(0.f, BossMaxHealth);
	SetBossHPVisible(false);
}
