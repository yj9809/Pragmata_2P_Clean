// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/MonsterAIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BlackboardData.h"
#include "AI/MonsterBBKey.h"
#include "Enemy/MonsterTerritoryActor.h"
#include "Enemy/MonsterBase.h"
#include "Engine/World.h"
#include "Game/PragmataGameState.h"
#include "Weapon/Decoy.h"

AMonsterAIController::AMonsterAIController()
{
}

void AMonsterAIController::RunAI()
{
	// 서버 권한으로만 동작
	if (!HasAuthority())
	{
		return;
	}

	// 블랙보드 컴포넌트 포인터 가져오기.
	UBlackboardComponent* BlackboardPtr = Blackboard.Get();
	UBlackboardData* BlackboardAsset = BTAsset ? BTAsset->GetBlackboardAsset() : nullptr;
	if (!BTAsset || !BlackboardAsset || !GetPawn())
	{
		UE_LOG(LogTemp, Warning, TEXT("[MonsterAIController::RunAI] Missing BT or Blackboard for %s"), *GetNameSafe(GetPawn()));
		return;
	}

	// BT가 선언한 Blackboard를 schema source로 사용
	if (UseBlackboard(BlackboardAsset, BlackboardPtr))
	{
		// 블랙보드 HomePos 스폰 위치로 저장.
		Blackboard->SetValueAsVector(BBKEY_HOMEPOS, GetPawn()->GetActorLocation());

		if (const AMonsterBase* Monster = Cast<AMonsterBase>(GetPawn()))
		{
			Blackboard->SetValueAsBool(BBKEY_BCANATTACK, Monster->CanAttack());
		}

		// 비헤이비어 트리 실행.
		bool Result = RunBehaviorTree(BTAsset);

		// 예외 처리.
		ensureAlways(Result);
	}
}

void AMonsterAIController::StopAI()
{
	// 서버 권한으로만 동작
	if (!HasAuthority())
	{
		return;
	}

	StopMovement();
	UnregisterAggroPawn();

	UBehaviorTreeComponent* BTComponent = Cast<UBehaviorTreeComponent>(BrainComponent);

	if (BTComponent)
	{
		BTComponent->StopTree(EBTStopMode::Forced);
	}
}

bool AMonsterAIController::SetTarget(AActor* NewTarget)
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[DamageAggro][SetTarget] Blocked: no authority Controller=%s Target=%s"), *GetNameSafe(this), *GetNameSafe(NewTarget));
		return false;
	}
	if (!Blackboard)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DamageAggro][SetTarget] Blocked: Blackboard missing Controller=%s Target=%s"), *GetNameSafe(this), *GetNameSafe(NewTarget));
		return false;
	}
	if (!NewTarget)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DamageAggro][SetTarget] Blocked: Target is null Controller=%s"), *GetNameSafe(this));
		return false;
	}

	// Target이 Territory 내부에 있을때만 Blackboard Key를 바꾸도록 예외 처리 
	// + decoy인 경우 예외처리
	ADecoy* Decoy = Cast<ADecoy>(NewTarget);
	if (!Decoy)
	{
		if (TerritoryActor && !TerritoryActor->IsActorInsideTerritory(NewTarget))
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("[DamageAggro][SetTarget] Blocked: Target outside territory Controller=%s Pawn=%s Target=%s Territory=%s"),
				*GetNameSafe(this),
				*GetNameSafe(GetPawn()),
				*GetNameSafe(NewTarget),
				*GetNameSafe(TerritoryActor.Get()));
			return false;
		}
	}
	

	Blackboard->SetValueAsObject(BBKEY_TARGET, NewTarget);
	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[DamageAggro][SetTarget] Success Controller=%s Pawn=%s Target=%s Territory=%s"),
		*GetNameSafe(this),
		*GetNameSafe(GetPawn()),
		*GetNameSafe(NewTarget),
		*GetNameSafe(TerritoryActor.Get()));
	if (APragmataGameState* GameState = GetWorld() ? GetWorld()->GetGameState<APragmataGameState>() : nullptr)
	{
		GameState->RegisterEnemyAggro(GetPawn(), NewTarget);
	}
	return true;
}

// Target이 BT 시작 직후 지정된 경우 루트 분기를 즉시 Chase 조건으로 다시 평가
void AMonsterAIController::RestartBehaviorAfterTargetAssigned()
{
	if (!HasAuthority())
	{
		return;
	}

	if (UBehaviorTreeComponent* BTComponent = Cast<UBehaviorTreeComponent>(BrainComponent))
	{
		BTComponent->RestartTree();
	}
}

void AMonsterAIController::ClearTargetIfSame(AActor* TargetActor)
{
	if (!HasAuthority() || !Blackboard || !TargetActor)
	{
		return;
	}

	AActor* CurrentTarget = Cast<AActor>(Blackboard->GetValueAsObject(BBKEY_TARGET));
	if (CurrentTarget == TargetActor)
	{
		Blackboard->ClearValue(BBKEY_TARGET);
		if (APragmataGameState* GameState = GetWorld() ? GetWorld()->GetGameState<APragmataGameState>() : nullptr)
		{
			GameState->UnregisterEnemyAggro(GetPawn());
		}
	}
}

void AMonsterAIController::UnregisterAggroPawn()
{
	if (!HasAuthority())
	{
		return;
	}

	if (APragmataGameState* GameState = GetWorld() ? GetWorld()->GetGameState<APragmataGameState>() : nullptr)
	{
		GameState->UnregisterEnemyAggro(GetPawn());
	}
}

void AMonsterAIController::SetTerritory(AMonsterTerritoryActor* NewTerritory)
{
	TerritoryActor = NewTerritory;
}

bool AMonsterAIController::IsActorInsideAssignedTerritory(AActor* Actor) const
{
	const bool bInsideTerritory = TerritoryActor && TerritoryActor->IsActorInsideTerritory(Actor);
	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[DamageAggro][IsActorInsideAssignedTerritory] Controller=%s Pawn=%s Actor=%s Territory=%s Inside=%d"),
		*GetNameSafe(this),
		*GetNameSafe(GetPawn()),
		*GetNameSafe(Actor),
		*GetNameSafe(TerritoryActor.Get()),
		bInsideTerritory ? 1 : 0);
	return bInsideTerritory;
}

void AMonsterAIController::SetMonsterTerritory(bool bInsideTerritory)
{
	if (!HasAuthority() || !Blackboard)
	{
		return;
	}

	Blackboard->SetValueAsBool(BBKEY_BINSIDETERRITORY, bInsideTerritory);
}

void AMonsterAIController::SetCanAttack(bool bCanAttack)
{
	if (!HasAuthority() || !Blackboard)
	{
		return;
	}

	Blackboard->SetValueAsBool(BBKEY_BCANATTACK, bCanAttack);
}

void AMonsterAIController::ResetOrbitDirection()
{
	if (!HasAuthority() || !Blackboard)
	{
		return;
	}

	Blackboard->ClearValue(BBKEY_BORBITCLOCKWISE);
	Blackboard->ClearValue(BBKEY_BHASORBITDIRECTION);
}

void AMonsterAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (AMonsterBase* Monster = Cast<AMonsterBase>(InPawn))
	{
		// 사용할 BT 애셋 로드
		BTAsset = Monster->GetBTAsset();

		if (Monster->ShouldAutoRunAIOnPossess())
		{
			// Territory 연출 대기 monster는 Territory가 직접 시작 시점을 정함
			RunAI();
		}
	}
}

void AMonsterAIController::OnUnPossess()
{
	UnregisterAggroPawn();
	Super::OnUnPossess();
}

void AMonsterAIController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnregisterAggroPawn();
	Super::EndPlay(EndPlayReason);
}
