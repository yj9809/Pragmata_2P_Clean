// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_ExecuteSelectedAttack.generated.h"

UCLASS()
class PRAGMATA_2P_API UBTTask_ExecuteSelectedAttack : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_ExecuteSelectedAttack();

protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};
