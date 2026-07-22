#include "UI/EnemyHPWidget.h"

#include "Components/PragmataHealthComponent.h"
#include "Components/Widget.h"
#include "GameFramework/Actor.h"
#include "UI/EnemyGroggyGaugeWidget.h"
#include "UI/EnemyHPBarWidget.h"

// target actor에서 HealthComponent를 찾아 상위 enemy HP 위젯에 연결
void UEnemyHPWidget::SetEnemyTarget(AActor* TargetActor)
{
	if (!TargetActor)
	{
		UnbindFromHealth();
		return;
	}

	BindToHealth(TargetActor->FindComponentByClass<UPragmataHealthComponent>());
}

// HealthComponent delegate를 WBP bar 갱신 경로에 연결
void UEnemyHPWidget::BindToHealth(UPragmataHealthComponent* HealthComp)
{
	UnbindFromHealth();

	if (!HealthComp)
	{
		return;
	}

	BoundHealthComp = HealthComp;
	HealthComp->OnHealthChanged.AddDynamic(this, &UEnemyHPWidget::HandleHealthChanged);
	HealthComp->OnDeath.AddDynamic(this, &UEnemyHPWidget::HandleDeath);

	SyncFromHealth();
	SetEnemyHPVisible(!bHideWhenDead || !HealthComp->IsDead());
}

// target 교체나 위젯 파괴 때 delegate와 표시 상태를 정리
void UEnemyHPWidget::UnbindFromHealth()
{
	if (BoundHealthComp.IsValid())
	{
		BoundHealthComp->OnHealthChanged.RemoveDynamic(this, &UEnemyHPWidget::HandleHealthChanged);
		BoundHealthComp->OnDeath.RemoveDynamic(this, &UEnemyHPWidget::HandleDeath);
	}

	BoundHealthComp.Reset();
	CachedCurrentHealth = 0.f;
	CachedMaxHealth = 0.f;
	SetHealthValue(CachedCurrentHealth, CachedMaxHealth);
	SetGroggyPercent(0.f);

	if (bHideWhenNoTarget)
	{
		SetEnemyHPVisible(false);
	}
}

// HP 하위 WBP에 현재 체력과 최대 체력을 전달
void UEnemyHPWidget::SetHealthValue(float NewCurrentHealth, float NewMaxHealth)
{
	CachedCurrentHealth = FMath::Max(0.f, NewCurrentHealth);
	CachedMaxHealth = FMath::Max(0.f, NewMaxHealth);

	if (UEnemyHPBarWidget* HPBarWidget = ResolveHPBarWidget())
	{
		HPBarWidget->SetHealthValue(CachedCurrentHealth, CachedMaxHealth);
	}
}

// groggy 하위 WBP에 현재 groggy와 최대 groggy를 전달
void UEnemyHPWidget::SetGroggyValue(float NewCurrentGroggy, float NewMaxGroggy)
{
	if (UEnemyGroggyGaugeWidget* GroggyGaugeWidget = ResolveGroggyGaugeWidget())
	{
		GroggyGaugeWidget->SetGroggyValue(NewCurrentGroggy, NewMaxGroggy);
	}
}

// groggy component가 없을 때도 비율만 직접 갱신할 수 있게 제공
void UEnemyHPWidget::SetGroggyPercent(float NewPercent)
{
	if (UEnemyGroggyGaugeWidget* GroggyGaugeWidget = ResolveGroggyGaugeWidget())
	{
		GroggyGaugeWidget->SetGroggyPercent(NewPercent);
	}
}

// WBP root가 있으면 root만 숨기고, 없으면 위젯 자체를 숨김
void UEnemyHPWidget::SetEnemyHPVisible(bool bVisible)
{
	const ESlateVisibility NewVisibility = bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;

	if (Root)
	{
		Root->SetVisibility(NewVisibility);
		return;
	}

	if (EnemyHPRoot)
	{
		EnemyHPRoot->SetVisibility(NewVisibility);
		return;
	}

	SetVisibility(NewVisibility);
}

// BP preview와 WidgetComponent 생성 직후에는 기본 bar 모양을 먼저 보여줌
void UEnemyHPWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SyncFromHealth();
}

// 위젯 제거 시 HealthComponent delegate 중복 binding을 남기지 않음
void UEnemyHPWidget::NativeDestruct()
{
	UnbindFromHealth();

	Super::NativeDestruct();
}

void UEnemyHPWidget::HandleHealthChanged(float NewCurrentHealth, float NewMaxHealth)
{
	// HealthComponent delegate 값을 두 하위 bar WBP 상태로 전달
	SetHealthValue(NewCurrentHealth, NewMaxHealth);
}

// death event 이후 bar를 0으로 만들고 표시 정책을 적용
void UEnemyHPWidget::HandleDeath()
{
	SetHealthValue(0.f, CachedMaxHealth);

	if (bHideWhenDead)
	{
		SetEnemyHPVisible(false);
	}
}

UEnemyHPBarWidget* UEnemyHPWidget::ResolveHPBarWidget() const
{
	if (HPBar)
	{
		return HPBar;
	}

	return EnemyHPBar;
}

UEnemyGroggyGaugeWidget* UEnemyHPWidget::ResolveGroggyGaugeWidget() const
{
	if (GroggyBar)
	{
		return GroggyBar;
	}

	return GroggyGauge;
}

// 현재 HealthComponent 상태를 child bar에 즉시 반영
void UEnemyHPWidget::SyncFromHealth()
{
	if (!BoundHealthComp.IsValid())
	{
		return;
	}

	SetHealthValue(BoundHealthComp->GetCurrentHealth(), BoundHealthComp->GetMaxHealth());

	if (bHideWhenDead && BoundHealthComp->IsDead())
	{
		SetEnemyHPVisible(false);
	}
}
