// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/BTTask_ChangeMoveSpeed.h"
#include "AIController.h"
#include "Enemy/MonsterBase.h"
#include "GameFramework/CharacterMovementComponent.h"


UBTTask_ChangeMoveSpeed::UBTTask_ChangeMoveSpeed()
{
	NodeName = TEXT("ChangeMoveSpeed");
}

EBTNodeResult::Type UBTTask_ChangeMoveSpeed::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	Super::ExecuteTask(OwnerComp, NodeMemory);

	APawn* ControllingPawn = OwnerComp.GetAIOwner()->GetPawn();
	if (!ControllingPawn)
	{
		return EBTNodeResult::Failed;
	}

	AMonsterBase* Monster = Cast<AMonsterBase>(ControllingPawn);
	if (!Monster)
	{
		return EBTNodeResult::Failed;
	}

	Monster->SetAIMoveSpeedMode(MoveSpeedMode);
	if (bOverrideMoveSpeed)
	{
		// 상태별 기본값을 먼저 적용한 뒤 이 BT 노드에만 필요한 속도로 덮어씀
		if (UCharacterMovementComponent* MovementComponent = Monster->GetCharacterMovement())
		{
			MovementComponent->MaxWalkSpeed = CustomMoveSpeed;
		}
	}

	return EBTNodeResult::Succeeded;
}
