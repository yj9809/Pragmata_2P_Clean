#pragma once

#include "CoreMinimal.h"
#include "Components/WidgetComponent.h"
#include "EnemyHPWidgetComponent.generated.h"

UCLASS(ClassGroup=(Pragmata), meta=(BlueprintSpawnableComponent))
class PRAGMATA_2P_API UEnemyHPWidgetComponent : public UWidgetComponent
{
	GENERATED_BODY()

public:
	UEnemyHPWidgetComponent();

	// owner monster의 authoritative target 상태를 화면 HP bar 표시로 반영
	void SetOwnerTargetedVisible(bool bShouldBeVisible);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void BindWidgetToOwnerHealth() const;
	void ApplyOwnerTargetedVisibility() const;

	bool bOwnerTargetedVisible = false;
};
