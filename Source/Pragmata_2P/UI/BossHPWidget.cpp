#include "UI/BossHPWidget.h"

#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "MVVMSubsystem.h"
#include "UI/BossHPViewModel.h"
#include "UI/EnemyGroggyGaugeWidget.h"
#include "UI/EnemyHPBarWidget.h"
#include "View/MVVMView.h"

const FName UBossHPWidget::ViewModelName = TEXT("BossHPViewModel");

void UBossHPWidget::SetBossTarget(AActor* TargetActor)
{
	UBossHPViewModel* ViewModel = BoundBossHPViewModel.Get();
	if (!ViewModel)
	{
		ViewModel = ResolveBossHPViewModel();
	}

	if (ViewModel)
	{
		BindToViewModel(ViewModel);
		ViewModel->SetBossTarget(TargetActor);
	}
	else if (bHideWhenNoTarget)
	{
		SetBossHPVisible(false);
	}

	OnBossTargetChanged(TargetActor);
}

void UBossHPWidget::BindToViewModel(UBossHPViewModel* ViewModel)
{
	if (BoundBossHPViewModel.Get() == ViewModel)
	{
		SyncFromViewModel();
		return;
	}

	UnbindFromViewModel();

	BoundBossHPViewModel = ViewModel;
	if (!ViewModel)
	{
		if (bHideWhenNoTarget)
		{
			SetBossHPVisible(false);
		}
		return;
	}

	// Composite widget은 ViewModel 출력만 듣고 gameplay component delegate는 직접 구독하지 않는다.
	ViewModel->OnBossHealthChangedForUI.AddDynamic(this, &UBossHPWidget::HandleViewModelHealthChanged);
	ViewModel->OnBossGroggyChangedForUI.AddDynamic(this, &UBossHPWidget::HandleViewModelGroggyChanged);
	ViewModel->OnBossHPVisibilityChangedForUI.AddDynamic(this, &UBossHPWidget::HandleViewModelVisibilityChanged);
	ViewModel->OnBossNameChangedForUI.AddDynamic(this, &UBossHPWidget::HandleViewModelBossNameChanged);
	SyncFromViewModel();
}

void UBossHPWidget::UnbindFromViewModel()
{
	if (BoundBossHPViewModel.IsValid())
	{
		BoundBossHPViewModel->OnBossHealthChangedForUI.RemoveDynamic(this, &UBossHPWidget::HandleViewModelHealthChanged);
		BoundBossHPViewModel->OnBossGroggyChangedForUI.RemoveDynamic(this, &UBossHPWidget::HandleViewModelGroggyChanged);
		BoundBossHPViewModel->OnBossHPVisibilityChangedForUI.RemoveDynamic(this, &UBossHPWidget::HandleViewModelVisibilityChanged);
		BoundBossHPViewModel->OnBossNameChangedForUI.RemoveDynamic(this, &UBossHPWidget::HandleViewModelBossNameChanged);
	}

	BoundBossHPViewModel.Reset();
}

void UBossHPWidget::SetHealthValue(float NewCurrentHealth, float NewMaxHealth)
{
	CachedMaxHealth = FMath::Max(0.f, NewMaxHealth);
	CachedCurrentHealth = FMath::Clamp(NewCurrentHealth, 0.f, CachedMaxHealth);
	HealthPercent = CachedMaxHealth > UE_KINDA_SMALL_NUMBER
		? CachedCurrentHealth / CachedMaxHealth
		: 0.f;

	if (UEnemyHPBarWidget* HPBarWidget = ResolveHPBarWidget())
	{
		HPBarWidget->SetHealthValue(CachedCurrentHealth, CachedMaxHealth);
	}

	ApplyDisplayText();
	OnBossHealthChanged(CachedCurrentHealth, CachedMaxHealth, HealthPercent);
}

void UBossHPWidget::SetHealthPercent(float NewPercent)
{
	HealthPercent = FMath::Clamp(NewPercent, 0.f, 1.f);

	if (UEnemyHPBarWidget* HPBarWidget = ResolveHPBarWidget())
	{
		HPBarWidget->SetHealthPercent(HealthPercent);
	}

	OnBossHealthChanged(CachedCurrentHealth, CachedMaxHealth, HealthPercent);
}

void UBossHPWidget::SetGroggyValue(float NewCurrentGroggy, float NewMaxGroggy)
{
	CachedMaxGroggy = FMath::Max(0.f, NewMaxGroggy);
	CachedCurrentGroggy = FMath::Clamp(NewCurrentGroggy, 0.f, CachedMaxGroggy);
	GroggyPercent = CachedMaxGroggy > UE_KINDA_SMALL_NUMBER
		? CachedCurrentGroggy / CachedMaxGroggy
		: 0.f;

	if (UEnemyGroggyGaugeWidget* GroggyGaugeWidget = ResolveGroggyGaugeWidget())
	{
		GroggyGaugeWidget->SetGroggyValue(CachedCurrentGroggy, CachedMaxGroggy);
	}

	OnBossGroggyChanged(CachedCurrentGroggy, CachedMaxGroggy, GroggyPercent);
}

void UBossHPWidget::SetGroggyPercent(float NewPercent)
{
	GroggyPercent = FMath::Clamp(NewPercent, 0.f, 1.f);

	if (UEnemyGroggyGaugeWidget* GroggyGaugeWidget = ResolveGroggyGaugeWidget())
	{
		GroggyGaugeWidget->SetGroggyPercent(GroggyPercent);
	}

	OnBossGroggyChanged(CachedCurrentGroggy, CachedMaxGroggy, GroggyPercent);
}

void UBossHPWidget::SetBossDisplayName(const FText& NewDisplayName)
{
	BossDisplayName = NewDisplayName;
	ApplyDisplayText();
}

void UBossHPWidget::SetBossHPVisible(bool bVisible)
{
	const ESlateVisibility NewVisibility = bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;

	if (Root)
	{
		Root->SetVisibility(NewVisibility);
	}
	else if (BossHPRoot)
	{
		BossHPRoot->SetVisibility(NewVisibility);
	}
	else
	{
		SetVisibility(NewVisibility);
	}

	OnBossHPVisibilityChanged(bVisible);
}

void UBossHPWidget::NativeConstruct()
{
	Super::NativeConstruct();

	BindToViewModel(ResolveBossHPViewModel());
	if (!BoundBossHPViewModel.IsValid() && bHideWhenNoTarget)
	{
		SetBossHPVisible(false);
	}
}

void UBossHPWidget::NativeDestruct()
{
	UnbindFromViewModel();

	Super::NativeDestruct();
}

UBossHPViewModel* UBossHPWidget::ResolveBossHPViewModel()
{
	UMVVMView* View = UMVVMSubsystem::GetViewFromUserWidget(this);
	if (!View)
	{
		if (!bLoggedMissingViewModel)
		{
			UE_LOG(LogTemp, Warning, TEXT("[UBossHPWidget::ResolveBossHPViewModel] Missing MVVM view on boss HP widget"));
			bLoggedMissingViewModel = true;
		}
		return nullptr;
	}

	UBossHPViewModel* ViewModel = Cast<UBossHPViewModel>(View->GetViewModel(ViewModelName).GetObject());
	if (!ViewModel && !bLoggedMissingViewModel)
	{
		UE_LOG(LogTemp, Warning, TEXT("[UBossHPWidget::ResolveBossHPViewModel] Add a BossHPViewModel entry named BossHPViewModel to WBP_BossHPWidget"));
		bLoggedMissingViewModel = true;
	}

	return ViewModel;
}

UEnemyHPBarWidget* UBossHPWidget::ResolveHPBarWidget() const
{
	if (HPBar)
	{
		return HPBar;
	}

	return BossHPBar;
}

UEnemyGroggyGaugeWidget* UBossHPWidget::ResolveGroggyGaugeWidget() const
{
	return BossGroggyBar;
}

void UBossHPWidget::SyncFromViewModel()
{
	if (!BoundBossHPViewModel.IsValid())
	{
		return;
	}

	SetBossDisplayName(BoundBossHPViewModel->GetBossNameText());
	if (BoundBossHPViewModel->GetBossMaxHealth() > UE_KINDA_SMALL_NUMBER)
	{
		SetHealthValue(BoundBossHPViewModel->GetBossCurrentHealth(), BoundBossHPViewModel->GetBossMaxHealth());
	}
	else
	{
		SetHealthPercent(BoundBossHPViewModel->GetBossHealthPercent());
	}

	if (BoundBossHPViewModel->GetBossMaxGroggy() > UE_KINDA_SMALL_NUMBER)
	{
		SetGroggyValue(BoundBossHPViewModel->GetBossCurrentGroggy(), BoundBossHPViewModel->GetBossMaxGroggy());
	}
	else
	{
		SetGroggyPercent(BoundBossHPViewModel->GetBossGroggyPercent());
	}
	SetBossHPVisible(BoundBossHPViewModel->IsBossHPVisible());
}

void UBossHPWidget::ApplyDisplayText()
{
	if (BossNameText)
	{
		BossNameText->SetText(BossDisplayName);
	}

	if (BossHPText)
	{
		const int32 RoundedCurrentHealth = FMath::RoundToInt(CachedCurrentHealth);
		const int32 RoundedMaxHealth = FMath::RoundToInt(CachedMaxHealth);
		BossHPText->SetText(FText::Format(INVTEXT("{0} / {1}"), FText::AsNumber(RoundedCurrentHealth), FText::AsNumber(RoundedMaxHealth)));
	}
}

void UBossHPWidget::HandleViewModelHealthChanged(float NewCurrentHealth, float NewMaxHealth, float NewHealthPercent)
{
	SetHealthValue(NewCurrentHealth, NewMaxHealth);

	if (bHideWhenDead && NewMaxHealth > UE_KINDA_SMALL_NUMBER && NewHealthPercent <= 0.f)
	{
		SetBossHPVisible(false);
	}
}

void UBossHPWidget::HandleViewModelGroggyChanged(float NewCurrentGroggy, float NewMaxGroggy, float NewGroggyPercent)
{
	if (NewMaxGroggy > UE_KINDA_SMALL_NUMBER)
	{
		SetGroggyValue(NewCurrentGroggy, NewMaxGroggy);
	}
	else
	{
		CachedCurrentGroggy = NewCurrentGroggy;
		CachedMaxGroggy = NewMaxGroggy;
		SetGroggyPercent(NewGroggyPercent);
	}
}

void UBossHPWidget::HandleViewModelVisibilityChanged(bool bVisible)
{
	SetBossHPVisible(bVisible);
}

void UBossHPWidget::HandleViewModelBossNameChanged(FText NewDisplayName)
{
	SetBossDisplayName(NewDisplayName);
}
