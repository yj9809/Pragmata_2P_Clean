#include "AI/BTTask_OrbitAroundTarget.h"

#include "AI/MonsterBBKey.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Enemy/MonsterBase.h"
#include "GameFramework/CharacterMovementComponent.h"

UBTTask_OrbitAroundTarget::UBTTask_OrbitAroundTarget()
{
	NodeName = TEXT("Orbit Around Target");
	bCreateNodeInstance = true;
	bNotifyTick = true;
	bNotifyTaskFinished = true;
}

EBTNodeResult::Type UBTTask_OrbitAroundTarget::ExecuteTask(UBehaviorTreeComponent& OwnerComp,uint8* NodeMemory)
{
	Super::ExecuteTask(OwnerComp, NodeMemory);

	AAIController* AIController = OwnerComp.GetAIOwner();
	AMonsterBase* Monster = AIController ? Cast<AMonsterBase>(AIController->GetPawn()) : nullptr;
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	AActor* Target = Blackboard ? Cast<AActor>(Blackboard->GetValueAsObject(BBKEY_TARGET)) : nullptr;
	if (!AIController || !Monster || !Monster->HasAuthority() || !Blackboard || !Target)
	{
		return EBTNodeResult::Failed;
	}

	UCharacterMovementComponent* Movement = Monster->GetCharacterMovement();
	if (!Movement || Monster->GetAIAttackDetectRange() <= KINDA_SMALL_NUMBER)
	{
		return EBTNodeResult::Failed;
	}

	ActiveMonster = Monster;
	bPreviousOrientRotationToMovement = Movement->bOrientRotationToMovement;
	bPreviousUseControllerDesiredRotation = Movement->bUseControllerDesiredRotation;
	PreviousRotationRate = Movement->RotationRate;

	AIController->StopMovement();
	Movement->bOrientRotationToMovement = false;
	Movement->bUseControllerDesiredRotation = true;
	Movement->RotationRate.Yaw = FacingRotationRate; // 회전 속도 제한

	if (!Blackboard->GetValueAsBool(BBKEY_BHASORBITDIRECTION))
	{
		const FVector LocalMonsterLocation = Target->GetActorTransform().InverseTransformPosition(
			Monster->GetActorLocation()
		);

		if (LocalMonsterLocation.Y > SideDeadZone)
		{
			bOrbitClockwise = true;
		}
		else if (LocalMonsterLocation.Y < -SideDeadZone)
		{
			bOrbitClockwise = false;
		}
		else
		{
			bOrbitClockwise = FMath::RandBool();
		}

		Blackboard->SetValueAsBool(BBKEY_BORBITCLOCKWISE, bOrbitClockwise);
		Blackboard->SetValueAsBool(BBKEY_BHASORBITDIRECTION, true);
	}
	else
	{
		bOrbitClockwise = Blackboard->GetValueAsBool(BBKEY_BORBITCLOCKWISE);
	}

	AIController->SetFocus(Target);
	return EBTNodeResult::InProgress;
}

void UBTTask_OrbitAroundTarget::TickTask(UBehaviorTreeComponent& OwnerComp,uint8* NodeMemory,float DeltaSeconds)
{
	Super::TickTask(OwnerComp, NodeMemory, DeltaSeconds);

	AAIController* AIController = OwnerComp.GetAIOwner();
	AMonsterBase* Monster = ActiveMonster.Get();
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	AActor* Target = Blackboard ? Cast<AActor>(Blackboard->GetValueAsObject(BBKEY_TARGET)) : nullptr;
	if (!AIController || !Monster || !Target)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	FVector RadialDirection = Monster->GetActorLocation() - Target->GetActorLocation();
	RadialDirection.Z = 0.f;
	const float CurrentRadius = RadialDirection.Size();
	if (!RadialDirection.Normalize())
	{
		return;
	}

	const float AttackDetectRange = Monster->GetAIAttackDetectRange();
	if (AttackDetectRange <= KINDA_SMALL_NUMBER)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	const float DesiredOrbitRadius = FMath::Min(
		FMath::Max(MinimumOrbitRadius, AttackDetectRange * OrbitRadiusScale),
		AttackDetectRange * 0.95f
	);
	const FVector TangentDirection = bOrbitClockwise
		? FVector::CrossProduct(RadialDirection, FVector::UpVector)
		: FVector::CrossProduct(FVector::UpVector, RadialDirection);

	const float RadiusErrorRatio = (CurrentRadius - DesiredOrbitRadius) / DesiredOrbitRadius;
	const float RadialCorrectionAmount = FMath::Clamp(
		RadiusErrorRatio * RadialCorrectionStrength,
		-MaximumRadialCorrection,
		MaximumRadialCorrection
	);
	const FVector MoveDirection = (TangentDirection - RadialDirection * RadialCorrectionAmount).GetSafeNormal();

	Monster->AddMovementInput(MoveDirection);
	AIController->SetFocus(Target);
}

EBTNodeResult::Type UBTTask_OrbitAroundTarget::AbortTask(UBehaviorTreeComponent& OwnerComp,uint8* NodeMemory)
{
	RestoreMovement(OwnerComp);
	return Super::AbortTask(OwnerComp, NodeMemory);
}

void UBTTask_OrbitAroundTarget::OnTaskFinished(
	UBehaviorTreeComponent& OwnerComp,
	uint8* NodeMemory,
	EBTNodeResult::Type TaskResult
)
{
	RestoreMovement(OwnerComp);
	Super::OnTaskFinished(OwnerComp, NodeMemory, TaskResult);
}

void UBTTask_OrbitAroundTarget::RestoreMovement(UBehaviorTreeComponent& OwnerComp)
{
	AMonsterBase* Monster = ActiveMonster.Get();
	if (!Monster)
	{
		return;
	}

	AAIController* AIController = OwnerComp.GetAIOwner();
	if (AIController)
	{
		AIController->ClearFocus(EAIFocusPriority::Gameplay);
		AIController->StopMovement();
	}

	if (UCharacterMovementComponent* Movement = Monster->GetCharacterMovement())
	{
		Movement->bOrientRotationToMovement = bPreviousOrientRotationToMovement;
		Movement->bUseControllerDesiredRotation = bPreviousUseControllerDesiredRotation;
		Movement->RotationRate = PreviousRotationRate;
		Movement->StopMovementImmediately();
	}

	ActiveMonster.Reset();
}
