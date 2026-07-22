// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/BTService_UpdateOrbitPos.h"

#include "AI/MonsterBBKey.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Enemy/MonsterBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NavigationSystem.h"

UBTService_UpdateOrbitPos::UBTService_UpdateOrbitPos()
{
	NodeName = TEXT("Update Orbit Pos");
	bCreateNodeInstance = true;
	INIT_SERVICE_NODE_NOTIFY_FLAGS();
	Interval = 0.15f;
	RandomDeviation = 0.f;
}

void UBTService_UpdateOrbitPos::OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	Super::OnBecomeRelevant(OwnerComp, NodeMemory);

	AAIController* AIController = OwnerComp.GetAIOwner();
	AMonsterBase* Monster = AIController ? Cast<AMonsterBase>(AIController->GetPawn()) : nullptr;
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	AActor* Target = Blackboard ? Cast<AActor>(Blackboard->GetValueAsObject(BBKEY_TARGET)) : nullptr;
	if (!Monster || !Monster->HasAuthority() || !Target)
	{
		return;
	}

	UCharacterMovementComponent* Movement = Monster->GetCharacterMovement();
	if (!Movement)
	{
		return;
	}

	ActiveMonster = Monster;
	bPreviousOrientRotationToMovement = Movement->bOrientRotationToMovement;
	bPreviousUseControllerDesiredRotation = Movement->bUseControllerDesiredRotation;

	Movement->bOrientRotationToMovement = false;
	Movement->bUseControllerDesiredRotation = true;

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

	TickNode(OwnerComp, NodeMemory, 0.f);
}

void UBTService_UpdateOrbitPos::TickNode(
	UBehaviorTreeComponent& OwnerComp,
	uint8* NodeMemory,
	float DeltaSeconds
)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

	AAIController* AIController = OwnerComp.GetAIOwner();
	AMonsterBase* Monster = AIController ? Cast<AMonsterBase>(AIController->GetPawn()) : nullptr;
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	AActor* Target = Blackboard ? Cast<AActor>(Blackboard->GetValueAsObject(BBKEY_TARGET)) : nullptr;
	if (!Monster || !Monster->HasAuthority() || !Blackboard || !Target)
	{
		return;
	}

	FVector RadialDirection = Monster->GetActorLocation() - Target->GetActorLocation();
	RadialDirection.Z = 0.f;
	if (!RadialDirection.Normalize())
	{
		RadialDirection = -Monster->GetActorForwardVector();
		RadialDirection.Z = 0.f;
		RadialDirection.Normalize();
	}

	const float AttackDetectRange = Monster->GetAIAttackDetectRange();
	const float DesiredOrbitRadius = FMath::Max(MinimumOrbitRadius, AttackDetectRange * OrbitRadiusScale);
	const float OrbitRadius = FMath::Min(DesiredOrbitRadius, AttackDetectRange * 0.95f);
	const float DirectionSign = bOrbitClockwise ? -1.f : 1.f;
	const FVector NextOrbitDirection = RadialDirection.RotateAngleAxis(
		DirectionSign * OrbitStepAngle,
		FVector::UpVector
	);

	const FVector DesiredOrbitPos = Target->GetActorLocation() + NextOrbitDirection * OrbitRadius;
	UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetNavigationSystem(Monster->GetWorld());
	if (!NavSystem)
	{
		return;
	}

	FNavLocation ProjectedOrbitPos;
	if (!NavSystem->ProjectPointToNavigation(DesiredOrbitPos, ProjectedOrbitPos, NavProjectionExtent))
	{
		return;
	}

	Blackboard->SetValueAsVector(BBKEY_ORBITPOS, ProjectedOrbitPos.Location);
	AIController->SetFocus(Target);
}

void UBTService_UpdateOrbitPos::OnCeaseRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	Super::OnCeaseRelevant(OwnerComp, NodeMemory);

	AAIController* AIController = OwnerComp.GetAIOwner();
	if (AIController)
	{
		AIController->ClearFocus(EAIFocusPriority::Gameplay);
	}

	if (AMonsterBase* Monster = ActiveMonster.Get())
	{
		if (UCharacterMovementComponent* Movement = Monster->GetCharacterMovement())
		{
			Movement->bOrientRotationToMovement = bPreviousOrientRotationToMovement;
			Movement->bUseControllerDesiredRotation = bPreviousUseControllerDesiredRotation;
		}
	}

	if (UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent())
	{
		Blackboard->ClearValue(BBKEY_ORBITPOS);
	}

	ActiveMonster.Reset();
}
