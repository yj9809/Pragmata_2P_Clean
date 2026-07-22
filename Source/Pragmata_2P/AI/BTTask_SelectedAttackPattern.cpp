// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/BTTask_SelectedAttackPattern.h"

#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "Enemy/MonsterBase.h"
#include "Engine/Engine.h"

UBTTask_SelectedAttackPattern::UBTTask_SelectedAttackPattern()
{
	NodeName = TEXT("Select Pattern");
}

EBTNodeResult::Type UBTTask_SelectedAttackPattern::ExecuteTask(
	UBehaviorTreeComponent& OwnerComp,
	uint8* NodeMemory
)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	AMonsterBase* Monster = AIController ? Cast<AMonsterBase>(AIController->GetPawn()) : nullptr;
	if (!Monster || !Monster->EnsureSelectedAttackPattern())
	{
		return EBTNodeResult::Failed;
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	// Behavior Tree가 확정한 공격 패턴을 플레이 뷰포트에 표시
	// if (GEngine)
	// {
	// 	const FName PatternName = Monster->GetSelectedAttackPatternName();
	// 	const FString PatternLabel = PatternName.IsNone() ? TEXT("Unnamed") : PatternName.ToString();
	// 	GEngine->AddOnScreenDebugMessage(
	// 		-1,
	// 		3.f,
	// 		FColor::Yellow,
	// 		FString::Printf(
	// 			TEXT("[SelectAttackPattern] %s -> %s"),
	// 			*GetNameSafe(Monster),
	// 			*PatternLabel
	// 		)
	// 	);
	// }
#endif

	return EBTNodeResult::Succeeded;
}
