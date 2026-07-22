// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_SelectedAttackPattern.generated.h"

/**
 *
 */
UCLASS()
class PRAGMATA_2P_API UBTTask_SelectedAttackPattern : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_SelectedAttackPattern();

protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};
