// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/BTTask_ExecuteSelectedAttack.h"

#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "Enemy/MonsterBase.h"

UBTTask_ExecuteSelectedAttack::UBTTask_ExecuteSelectedAttack()
{
	NodeName = TEXT("Do Attack");
}

EBTNodeResult::Type UBTTask_ExecuteSelectedAttack::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	Super::ExecuteTask(OwnerComp, NodeMemory);

	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController)
	{
		return EBTNodeResult::Failed;
	}

	APawn* ControllingPawn = AIController->GetPawn();
	if (!ControllingPawn)
	{
		return EBTNodeResult::Failed;
	}

	AMonsterBase* Monster = Cast<AMonsterBase>(ControllingPawn);
	if (!Monster)
	{
		return EBTNodeResult::Failed;
	}

	// 선택된 공격 패턴 사용 -> 몽타주 재생
	// 캐릭터가 공격을 종료할 때 실행될 델리게이트.
	FMonsterAttackFinished OnAttackFinished;
	TWeakObjectPtr<UBehaviorTreeComponent> WeakOwnerComp(&OwnerComp);

	OnAttackFinished.BindWeakLambda(
		this,
		[this, WeakOwnerComp]()
		{
			if (UBehaviorTreeComponent* BehaviorTree = WeakOwnerComp.Get())
			{
				FinishLatentTask(*BehaviorTree, EBTNodeResult::Succeeded);
			}
		}
	);

	Monster->SetAIAttackDelegate(OnAttackFinished);

	if (!Monster->AttackByAI())
	{
		Monster->ClearAIAttackDelegate();
		return EBTNodeResult::Failed;
	}

	return EBTNodeResult::InProgress;
}

EBTNodeResult::Type UBTTask_ExecuteSelectedAttack::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	if (AAIController* AIController = OwnerComp.GetAIOwner())
	{
		if (AMonsterBase* Monster = Cast<AMonsterBase>(AIController->GetPawn()))
		{
			Monster->ClearAIAttackDelegate();
		}
	}

	return Super::AbortTask(OwnerComp, NodeMemory);
}
