#include "Components/PragmataHealthComponent.h"

#include "Net/UnrealNetwork.h"

UPragmataHealthComponent::UPragmataHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UPragmataHealthComponent::BeginPlay()
{
	Super::BeginPlay();

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		// BP 또는 actor constructor가 지정한 MaxHealth를 새 actor의 시작 체력에 반영
		MaxHealth = FMath::Max(0.f, MaxHealth);
		CurrentHealth = MaxHealth;
		bIsDead = CurrentHealth <= 0.f;
	}
}

void UPragmataHealthComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UPragmataHealthComponent, MaxHealth);
	DOREPLIFETIME(UPragmataHealthComponent, CurrentHealth);
	DOREPLIFETIME(UPragmataHealthComponent, bIsDead);
	DOREPLIFETIME(UPragmataHealthComponent, bDamageInvulnerable);
}

bool UPragmataHealthComponent::ReceiveDamage(const FPragmataDamageSpec& DamageSpec)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || bIsDead || bDamageInvulnerable || DamageSpec.DamageAmount <= 0.f)
	{
		return false;
	}

	CurrentHealth = FMath::Clamp(CurrentHealth - DamageSpec.DamageAmount, 0.f, MaxHealth);
	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);

	if (CurrentHealth <= 0.f && !bIsDead)
	{
		bIsDead = true;
		OnDeath.Broadcast();
	}

	return true;
}

void UPragmataHealthComponent::Heal(float HealAmount)
{
	ApplyRecovery(HealAmount);
}

bool UPragmataHealthComponent::RecoverHP(int32 RecoveryAmount)
{
	// 아이템/강화 회복량은 정수 HP 단위로만 받아 기존 float health source에 반영
	if (RecoveryAmount <= 0)
	{
		return false;
	}

	return ApplyRecovery(static_cast<float>(RecoveryAmount));
}

// actor별 기본 HP를 health component source에 반영
void UPragmataHealthComponent::SetDefaultHealth(int32 NewMaxHP)
{
	const float ClampedMaxHealth = static_cast<float>(FMath::Max(0, NewMaxHP));
	MaxHealth = ClampedMaxHealth;
	CurrentHealth = ClampedMaxHealth;
	bIsDead = CurrentHealth <= 0.f;
}

// 강화 환급 시 현재 HP는 보존하고, 강화 시 증가한 MaxHP만큼 현재 HP도 올림
bool UPragmataHealthComponent::SetMaxHPFromUpgrade(int32 NewMaxHP)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return false;
	}

	const float ClampedMaxHealth = static_cast<float>(FMath::Max(0, NewMaxHP));
	if (FMath::IsNearlyEqual(MaxHealth, ClampedMaxHealth))
	{
		return false;
	}

	const float PreviousMaxHealth = MaxHealth;
	MaxHealth = ClampedMaxHealth;

	if (!bIsDead)
	{
		const float HealthDelta = MaxHealth - PreviousMaxHealth;
		const float NewCurrentHealth = HealthDelta > 0.f
			? CurrentHealth + HealthDelta
			: CurrentHealth;
		CurrentHealth = FMath::Clamp(NewCurrentHealth, 0.f, MaxHealth);
		bIsDead = CurrentHealth <= 0.f;
	}

	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
	return true;
}

bool UPragmataHealthComponent::RestoreFullHealth()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return false;
	}

	bIsDead = false;
	CurrentHealth = MaxHealth;
	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
	return true;
}

void UPragmataHealthComponent::SetDamageInvulnerable(bool bNewInvulnerable)
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		bDamageInvulnerable = bNewInvulnerable;
	}
}

// 회복도 damage처럼 서버 health source에서만 확정
bool UPragmataHealthComponent::ApplyRecovery(float RecoveryAmount)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || bIsDead || RecoveryAmount <= 0.f || CurrentHealth >= MaxHealth)
	{
		return false;
	}

	CurrentHealth = FMath::Clamp(CurrentHealth + RecoveryAmount, 0.f, MaxHealth);
	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
	return true;
}

void UPragmataHealthComponent::OnRep_HealthSnapshot()
{
	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
}

void UPragmataHealthComponent::OnRep_IsDead()
{
	if (bIsDead)
	{
		OnDeath.Broadcast();
	}
}
