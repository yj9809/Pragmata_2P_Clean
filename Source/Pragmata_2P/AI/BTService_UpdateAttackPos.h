// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "BTService_UpdateAttackPos.generated.h"

/**
 * 
 */
UCLASS()
class PRAGMATA_2P_API UBTService_UpdateAttackPos : public UBTService
{
	GENERATED_BODY()

public:
	UBTService_UpdateAttackPos();

protected:
	virtual void TickNode(
		UBehaviorTreeComponent& OwnerComp,
		uint8* NodeMemory,
		float DeltaSeconds) override;
	
};
