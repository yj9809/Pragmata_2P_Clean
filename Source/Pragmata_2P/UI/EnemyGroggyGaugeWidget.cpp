#include "UI/EnemyGroggyGaugeWidget.h"

#include "Components/Image.h"
#include "Materials/MaterialInstanceDynamic.h"

// groggy 수치에서 material percent를 계산
void UEnemyGroggyGaugeWidget::SetGroggyValue(float NewCurrentGroggy, float NewMaxGroggy)
{
	CurrentGroggy = FMath::Max(0.f, NewCurrentGroggy);
	MaxGroggy = FMath::Max(0.f, NewMaxGroggy);

	const float NewPercent = MaxGroggy > 0.f
		? CurrentGroggy / MaxGroggy
		: 0.f;

	SetGroggyPercent(NewPercent);
}

// 외부 테스트나 groggy 시스템에서 직접 비율만 밀어 넣을 때 사용
void UEnemyGroggyGaugeWidget::SetGroggyPercent(float NewPercent)
{
	GroggyPercent = FMath::Clamp(NewPercent, 0.f, 1.f);
	ApplyGroggyPercent();
}

// WBP Image brush에서 Dynamic Material을 확보하고 현재 값을 반영
void UEnemyGroggyGaugeWidget::NativeConstruct()
{
	Super::NativeConstruct();

	EnsureMaterialInstance();
	ApplyGroggyPercent();
}

UImage* UEnemyGroggyGaugeWidget::ResolveGroggyImage() const
{
	if (GroggyBar)
	{
		return GroggyBar;
	}

	return GroggyGauge;
}

// groggy WBP가 선택한 Image 이름을 허용
void UEnemyGroggyGaugeWidget::EnsureMaterialInstance()
{
	if (GroggyMID)
	{
		return;
	}

	UImage* TargetImage = ResolveGroggyImage();
	if (!TargetImage)
	{
		return;
	}

	GroggyMID = TargetImage->GetDynamicMaterial();
}

// Enemy groggy 마테리얼의 채움 비율만 C++ 상태로 갱신
void UEnemyGroggyGaugeWidget::ApplyGroggyPercent()
{
	EnsureMaterialInstance();

	if (GroggyMID && !PercentParameterName.IsNone())
	{
		GroggyMID->SetScalarParameterValue(PercentParameterName, GroggyPercent);
	}
}
