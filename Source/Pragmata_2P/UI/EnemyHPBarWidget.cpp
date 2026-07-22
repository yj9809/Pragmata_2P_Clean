#include "UI/EnemyHPBarWidget.h"

#include "Components/Image.h"
#include "Materials/MaterialInstanceDynamic.h"

// HealthComponent 값에서 Enemy HP material percent를 계산
void UEnemyHPBarWidget::SetHealthValue(float NewCurrentHealth, float NewMaxHealth)
{
	CurrentHealth = FMath::Max(0.f, NewCurrentHealth);
	MaxHealth = FMath::Max(0.f, NewMaxHealth);

	const float NewPercent = MaxHealth > 0.f
		? CurrentHealth / MaxHealth
		: 0.f;

	SetHealthPercent(NewPercent);
}

// 외부 테스트나 MVVM binding에서 직접 비율만 밀어 넣을 때 사용
void UEnemyHPBarWidget::SetHealthPercent(float NewPercent)
{
	HealthPercent = FMath::Clamp(NewPercent, 0.f, 1.f);
	ApplyHealthPercent();
}

// WBP Image brush에서 Dynamic Material을 확보하고 현재 값을 반영
void UEnemyHPBarWidget::NativeConstruct()
{
	Super::NativeConstruct();

	EnsureMaterialInstance();
	ApplyHealthPercent();
}

void UEnemyHPBarWidget::EnsureMaterialInstance()
{
	if (!HPBar || HPBarMID)
	{
		return;
	}

	HPBarMID = HPBar->GetDynamicMaterial();
}

// Enemy HP 마테리얼의 채움 비율만 C++ 상태로 갱신
void UEnemyHPBarWidget::ApplyHealthPercent()
{
	EnsureMaterialInstance();

	if (HPBarMID && !PercentParameterName.IsNone())
	{
		HPBarMID->SetScalarParameterValue(PercentParameterName, HealthPercent);
	}
}
