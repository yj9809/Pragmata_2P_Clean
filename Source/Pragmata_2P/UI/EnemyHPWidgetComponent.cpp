#include "UI/EnemyHPWidgetComponent.h"

#include "Components/PragmataHealthComponent.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Actor.h"
#include "UI/EnemyHPWidget.h"

UEnemyHPWidgetComponent::UEnemyHPWidgetComponent()
{
	// Screen space는 UWidgetComponent::TickComponent()의 AddWidgetToScreen() 경로로만 화면에 등록되므로 tick을 끄면 아예 렌더링되지 않음
	SetWidgetSpace(EWidgetSpace::Screen);
	SetDrawAtDesiredSize(true);
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

// WidgetComponent가 생성한 WBP를 owner enemy의 HealthComponent에 연결
void UEnemyHPWidgetComponent::BeginPlay()
{
	Super::BeginPlay();

	InitWidget();

	// Screen space는 tick에서만 AddWidgetToScreen()이 호출되므로, BP에서 Start with Tick Enabled가
	// 꺼져 있어도(또는 실수로 꺼진 경우에도) 항상 화면에 등록되도록 강제
	SetComponentTickEnabled(true);

	BindWidgetToOwnerHealth();
	ApplyOwnerTargetedVisibility();
}

// component가 사라질 때 delegate binding이 owner에 남지 않게 정리
void UEnemyHPWidgetComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UEnemyHPWidget* EnemyHPWidget = Cast<UEnemyHPWidget>(GetUserWidgetObject()))
	{
		EnemyHPWidget->UnbindFromHealth();
	}

	Super::EndPlay(EndPlayReason);
}

void UEnemyHPWidgetComponent::SetOwnerTargetedVisible(bool bShouldBeVisible)
{
	bOwnerTargetedVisible = bShouldBeVisible;
	ApplyOwnerTargetedVisibility();
}

// 같은 actor의 HealthComponent를 찾아 head widget WBP에 전달
void UEnemyHPWidgetComponent::BindWidgetToOwnerHealth() const
{
	UEnemyHPWidget* EnemyHPWidget = Cast<UEnemyHPWidget>(GetUserWidgetObject());
	if (!EnemyHPWidget)
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		EnemyHPWidget->UnbindFromHealth();
		return;
	}

	EnemyHPWidget->BindToHealth(Owner->FindComponentByClass<UPragmataHealthComponent>());
}

// Widget 생성 순서와 무관하게 마지막 target 상태를 WBP에 적용
void UEnemyHPWidgetComponent::ApplyOwnerTargetedVisibility() const
{
	if (UEnemyHPWidget* EnemyHPWidget = Cast<UEnemyHPWidget>(GetUserWidgetObject()))
	{
		EnemyHPWidget->SetEnemyHPVisible(bOwnerTargetedVisible);
	}
}
