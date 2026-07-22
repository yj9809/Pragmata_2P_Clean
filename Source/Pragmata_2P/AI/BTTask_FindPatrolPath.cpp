// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/BTTask_FindPatrolPath.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "MonsterBBKey.h"
#include "NavigationSystem.h"
#include "AIController.h"

UBTTask_FindPatrolPath::UBTTask_FindPatrolPath()
{
	NodeName = TEXT("FindPatrolPos");

	// 정찰 반경 설정. (필요시 Monster쪽에서 수치 저장 후 받아오기)
	PatrolRadius = 800.0f;
}

EBTNodeResult::Type UBTTask_FindPatrolPath::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    // 1. 블랙보드의 HomePos위치를 기준으로 일정 반경 안에서 랜덤 위치 선택
	FVector Origin = OwnerComp.GetBlackboardComponent()->GetValueAsVector(BBKEY_HOMEPOS);

	APawn* ControllingPawn = OwnerComp.GetAIOwner()->GetPawn();
	if (!ControllingPawn)
	{
		return EBTNodeResult::Failed;
	}

	UWorld* World = ControllingPawn->GetWorld();
	if (!World)
	{
		return EBTNodeResult::Failed;
	}

	// 1.1 랜덤 위치 선택 시 네비게이션 기능 활용.
	UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetNavigationSystem(World);
	if (!NavSystem)
	{	
		return EBTNodeResult::Failed;
	}
	
	// 랜덤으로 선택된 위치를 저장할 변수.
	FNavLocation NextPatrolPos;
	bool bResult = NavSystem->GetRandomPointInNavigableRadius(
		Origin,
		PatrolRadius,
		NextPatrolPos
	);

	// 2. 선택된 위치를 블랙보드의 PatrolPos에 저장.
	if (bResult)
	{
		OwnerComp.GetBlackboardComponent()->SetValueAsVector(
			BBKEY_PATROLPOS,
			NextPatrolPos.Location
		);

		// 성공 반환.
		return EBTNodeResult::Succeeded;
	}

	return EBTNodeResult::Failed;
}
