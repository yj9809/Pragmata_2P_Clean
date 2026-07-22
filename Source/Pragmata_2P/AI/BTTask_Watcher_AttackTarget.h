// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_Watcher_AttackTarget.generated.h"

class AWatcherMonster;
/**
 * 
 */
UCLASS()
class PRAGMATA_2P_API UBTTask_Watcher_AttackTarget : public UBTTaskNode
{
	GENERATED_BODY()
public:
	UBTTask_Watcher_AttackTarget();
protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	void AttackTarget(AWatcherMonster* Monster);
protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Timer")
	float AttackInterval = 5.f;
	UPROPERTY()
	FTimerHandle AttackTimer;
	UPROPERTY()
	bool bIsLaserVisible = true;
};
