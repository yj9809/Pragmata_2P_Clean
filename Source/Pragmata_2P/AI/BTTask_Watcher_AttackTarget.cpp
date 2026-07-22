// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/BTTask_Watcher_AttackTarget.h"
#include "AIController.h"
#include "MonsterAIController.h"
#include "MonsterBBKey.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "DrawDebugHelpers.h"
#include "Enemy/WatcherMonster.h"

UBTTask_Watcher_AttackTarget::UBTTask_Watcher_AttackTarget()
{
	NodeName = TEXT("Watcher Attack Target");
	bNotifyTaskFinished = true;
	
	// Forces the tree to instantiate a unique object per AI instead of sharing it
	bCreateNodeInstance = true;
	bNotifyTick = true;
}

EBTNodeResult::Type UBTTask_Watcher_AttackTarget::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	Super::ExecuteTask(OwnerComp, NodeMemory);
	
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController)
	{
		UE_LOG(LogTemp, Warning, TEXT("[UBTTask_Watcher_AttackTarget::ExecuteTask] Failed: AIController missing"));
		return EBTNodeResult::Failed;
	}
	
	APawn* ControllingPawn = AIController->GetPawn();
	if (!ControllingPawn)
	{
		UE_LOG(LogTemp, Warning, TEXT("[UBTTask_Watcher_AttackTarget::ExecuteTask] Failed: Pawn missing Controller=%s"), *GetNameSafe(AIController));
		return EBTNodeResult::Failed;
	}
	
	AWatcherMonster* Monster = Cast<AWatcherMonster>(ControllingPawn);
	if (!Monster)
	{
		UE_LOG(LogTemp, Warning, TEXT("[UBTTask_Watcher_AttackTarget::ExecuteTask] Failed: Pawn is not Watcher Pawn=%s"), *GetNameSafe(ControllingPawn));
		return EBTNodeResult::Failed;
	}

	FTimerDelegate AttackDelegate;
	AttackDelegate.BindUObject(this, &UBTTask_Watcher_AttackTarget::AttackTarget, Monster);
	
	AIController->GetWorldTimerManager().SetTimer(AttackTimer, AttackDelegate, AttackInterval, true, 0.f);
	
	return EBTNodeResult::InProgress;
}

void UBTTask_Watcher_AttackTarget::OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory,
	EBTNodeResult::Type TaskResult)
{
	Super::OnTaskFinished(OwnerComp, NodeMemory, TaskResult);
	AAIController* Controller = OwnerComp.GetAIOwner();
	if (!Controller) return;
	Controller->GetWorldTimerManager().ClearTimer(AttackTimer);
}

void UBTTask_Watcher_AttackTarget::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickTask(OwnerComp, NodeMemory, DeltaSeconds);
	
	if (!bIsLaserVisible) return;
	
	AAIController* Controller = OwnerComp.GetAIOwner();
	if (!Controller || !Controller->GetPawn()) return;
	
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!Blackboard) return;

	AActor* TargetActor = Cast<AActor>(Blackboard->GetValueAsObject(BBKEY_TARGET));
	if (!TargetActor) return;
	
	UWorld* World = Controller->GetWorld();
	if (!World) return;
	
	//laser trajectory
	FVector StartPos = Controller->GetPawn()->GetActorLocation();
	FVector EndPos = TargetActor->GetActorLocation();
	
	FHitResult HitResult;
	FCollisionQueryParams CollisionParams;
	CollisionParams.AddIgnoredActor(Controller->GetPawn());
	
	FVector LaserEnd = EndPos;
	if (World->LineTraceSingleByChannel(HitResult, StartPos, EndPos, ECC_GameTraceChannel2, CollisionParams))
	{
		LaserEnd = HitResult.ImpactPoint;
	}
	
	DrawDebugLine(World, StartPos, LaserEnd, FColor::Red, false, 0.1f, 0, 3.f);
}


void UBTTask_Watcher_AttackTarget::AttackTarget(AWatcherMonster* Monster)
{
	if (IsValid(Monster))
	{
		AAIController* Controller = Monster->GetController<AAIController>();
		if (!Controller)
		{
			UE_LOG(LogTemp, Warning, TEXT("[UBTTask_Watcher_AttackTarget::AttackTarget] Blocked: Controller missing Monster=%s"), *GetNameSafe(Monster));
			return;
		}
		
		UBlackboardComponent* Blackboard = Controller->GetBlackboardComponent();
		if (!Blackboard)
		{
			UE_LOG(LogTemp, Warning, TEXT("[UBTTask_Watcher_AttackTarget::AttackTarget] Blocked: Blackboard missing Monster=%s Controller=%s"), *GetNameSafe(Monster), *GetNameSafe(Controller));
			return;
		}
		
		AActor* TargetActor = Cast<AActor>(Blackboard->GetValueAsObject(BBKEY_TARGET));
		if (!TargetActor)
		{
			UE_LOG(LogTemp, Warning, TEXT("[UBTTask_Watcher_AttackTarget::AttackTarget] Blocked: Target missing Monster=%s"), *GetNameSafe(Monster));
			return;
		}
		
		UWorld* World = Monster->GetWorld();
		if (!World)
		{
			UE_LOG(LogTemp, Warning, TEXT("[UBTTask_Watcher_AttackTarget::AttackTarget] Blocked: World missing Monster=%s"), *GetNameSafe(Monster));
			return;
		}
		
		FHitResult HitResult;
		FCollisionQueryParams CollisionParams;
		CollisionParams.AddIgnoredActor(Monster);
		
		FVector StartPos = Monster->GetActorLocation();
		FVector EndPos = TargetActor->GetActorLocation();
		
		const bool bHit = World->LineTraceSingleByChannel(HitResult, StartPos, EndPos, ECC_GameTraceChannel2, CollisionParams);
		const FVector LaserEnd = bHit ? HitResult.ImpactPoint : EndPos;
		DrawDebugLine(World, StartPos, LaserEnd, FColor::Red, false, 0.25f, 0, 3.f);

		if (!bHit)
		{
			// Treat a clear trace as a direct target hit when Projectile does not block Pawn
			FHitResult TargetHit(TargetActor, nullptr, EndPos, (StartPos - EndPos).GetSafeNormal());
			TargetHit.TraceStart = StartPos;
			TargetHit.TraceEnd = EndPos;
			Monster->FireBullet(TargetHit);
			return;
		}

		if (HitResult.GetActor() == TargetActor)
		{
			Monster->FireBullet(HitResult);
			return;
		}

		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[UBTTask_Watcher_AttackTarget::AttackTarget] Blocked by trace Monster=%s Target=%s HitActor=%s HitLocation=%s"),
			*GetNameSafe(Monster),
			*GetNameSafe(TargetActor),
			*GetNameSafe(HitResult.GetActor()),
			*HitResult.ImpactPoint.ToCompactString());
	}
}
