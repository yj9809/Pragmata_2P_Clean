#include "AI/BTTask_TurnToTarget.h"

#include "AI/MonsterBBKey.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Enemy/MonsterBase.h"

UBTTask_TurnToTarget::UBTTask_TurnToTarget()
{
	NodeName = TEXT("Turn To Target");
	INIT_TASK_NODE_NOTIFY_FLAGS();
}

EBTNodeResult::Type UBTTask_TurnToTarget::ExecuteTask(
	UBehaviorTreeComponent& OwnerComp,
	uint8* NodeMemory
)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController)
	{
		return EBTNodeResult::Failed;
	}

	AIController->StopMovement();
	return UpdateTurn(OwnerComp, 0.f) ? EBTNodeResult::InProgress : EBTNodeResult::Failed;
}

void UBTTask_TurnToTarget::TickTask(
	UBehaviorTreeComponent& OwnerComp,
	uint8* NodeMemory,
	float DeltaSeconds
)
{
	Super::TickTask(OwnerComp, NodeMemory, DeltaSeconds);

	if (!UpdateTurn(OwnerComp, DeltaSeconds))
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
	}
}

bool UBTTask_TurnToTarget::UpdateTurn(
	UBehaviorTreeComponent& OwnerComp,
	float DeltaSeconds
) const
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	AMonsterBase* Monster = AIController ? Cast<AMonsterBase>(AIController->GetPawn()) : nullptr;
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	AActor* Target = Blackboard ? Cast<AActor>(Blackboard->GetValueAsObject(BBKEY_TARGET)) : nullptr;
	if (!Monster || !Monster->HasAuthority() || !Target)
	{
		return false;
	}

	// 선택된 뒤쪽 조건 공격은 BT 회전으로 현재 방향을 바꾸면 후방 이동 animation이 어긋남
	if (Monster->ShouldKeepCurrentFacingForSelectedAttackPattern())
	{
		return true;
	}

	if (!Monster->CanTrackTargetDuringAttack())
	{
		return true;
	}

	FVector LookDirection = Target->GetActorLocation() - Monster->GetActorLocation();
	LookDirection.Z = 0.f;
	if (!LookDirection.Normalize())
	{
		return true;
	}

	const FRotator CurrentRotation = Monster->GetActorRotation();
	const FRotator TargetRotation(0.f, LookDirection.Rotation().Yaw, 0.f);
	Monster->SetActorRotation(FMath::RInterpConstantTo(
		CurrentRotation,
		TargetRotation,
		DeltaSeconds,
		TurnRateDegreesPerSecond
	));
	return true;
}
