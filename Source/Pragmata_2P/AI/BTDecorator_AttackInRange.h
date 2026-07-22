// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTDecorator.h"
#include "BTDecorator_AttackInRange.generated.h"

/**
 * 
 */
UCLASS()
class PRAGMATA_2P_API UBTDecorator_AttackInRange : public UBTDecorator
{
	GENERATED_BODY()
public:
	UBTDecorator_AttackInRange();

protected:
	virtual bool CalculateRawConditionValue(
		UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory
	) const override;

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI|Attack", meta=(AllowPrivateAccess="true", ClampMin="0"))
	float AttackRange = 250.f;
};
