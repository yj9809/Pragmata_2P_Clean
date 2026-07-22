#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_TurnToTarget.generated.h"

UCLASS()
class PRAGMATA_2P_API UBTTask_TurnToTarget : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_TurnToTarget();

protected:
	virtual EBTNodeResult::Type ExecuteTask(
		UBehaviorTreeComponent& OwnerComp,
		uint8* NodeMemory
	) override;
	virtual void TickTask(
		UBehaviorTreeComponent& OwnerComp,
		uint8* NodeMemory,
		float DeltaSeconds
	) override;

private:
	bool UpdateTurn(UBehaviorTreeComponent& OwnerComp, float DeltaSeconds) const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI|Attack", meta=(AllowPrivateAccess="true", ClampMin="0.0"))
	float TurnRateDegreesPerSecond = 45.f;
};
