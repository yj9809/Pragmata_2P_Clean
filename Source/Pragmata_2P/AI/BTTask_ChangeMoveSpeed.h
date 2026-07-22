// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "Data/PragmataTypes.h"
#include "BTTask_ChangeMoveSpeed.generated.h"

/**
 * 
 */
UCLASS()
class PRAGMATA_2P_API UBTTask_ChangeMoveSpeed : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_ChangeMoveSpeed();

protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

protected:
	UPROPERTY(EditAnywhere, Category = "MoveType")
	EMonsterMoveSpeedMode MoveSpeedMode = EMonsterMoveSpeedMode::Default;

	UPROPERTY(EditAnywhere, Category = "MoveType", meta=(DisplayName="Override Move Speed"))
	bool bOverrideMoveSpeed = false;

	UPROPERTY(EditAnywhere, Category = "MoveType", meta=(EditCondition="bOverrideMoveSpeed", EditConditionHides, ClampMin="0.0", UIMin="0.0", DisplayName="Custom Move Speed"))
	float CustomMoveSpeed = 300.f;
};
