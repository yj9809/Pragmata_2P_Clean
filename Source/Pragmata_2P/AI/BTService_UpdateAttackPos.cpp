// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/BTService_UpdateAttackPos.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "MonsterBBKey.h"
#include "Enemy/MonsterBase.h"
#include "DrawDebugHelpers.h"


UBTService_UpdateAttackPos::UBTService_UpdateAttackPos()
{
	NodeName = TEXT("UpdateAPos");

	Interval = 0.1f;
}

void UBTService_UpdateAttackPos::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

	// 몬스터 자신 Pawn
	APawn* ControllingPawn = OwnerComp.GetAIOwner()->GetPawn();
	if (!ControllingPawn)
	{
		return;
	}

	// Blackboard target은 Pawn이 아닌 공격 가능한 Actor 계약으로 유지
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!Blackboard)
	{
		return;
	}

	AActor* Target = Cast<AActor>(Blackboard->GetValueAsObject(BBKEY_TARGET));
	if (!Target)
	{
		return;
	}
	
	// 공격 사거리 가져오기
	AMonsterBase* Monster = Cast<AMonsterBase>(ControllingPawn);
	if (!Monster)
	{
		return;
	}

	if (!Monster->HasSelectedAttackPattern())
	{
		return;
	}

	// 벡터 구하기
	FVector MonsterLoc = Monster->GetActorLocation();
	FVector TargetLoc = Target->GetActorLocation();

	// 방향 벡터 구하기
	FVector DirFromTargetToMonster = MonsterLoc - TargetLoc;
	DirFromTargetToMonster.Z = 0.0f;
	DirFromTargetToMonster.Normalize();

	// 공격 사거리 불러오기
	float DesiredDistance = Monster->GetAttackRange();

	// AttackPos 설정
	FVector AttackPos = TargetLoc + DirFromTargetToMonster * DesiredDistance;
	AttackPos.Z = MonsterLoc.Z;

	/* 몬스터 공격 감지 범위 + 공격 사거리 시각 디버깅 코드 -> 지우지 마세요! */
	/*
	// 빨강 = 몬스터 위치(GetActorLocation)
	DrawDebugSphere(
		GetWorld(),
		MonsterLoc,
		40.f,
		12,
		FColor::Red,
		false,
		5.0f
	);

	// 초록 = 타겟 위치
	DrawDebugSphere(
		GetWorld(),
		TargetLoc,
		40.f,
		12,
		FColor::Green,
		false,
		5.0f
	);

	// 파랑 = 계산된 AttackPos
	DrawDebugSphere(
		GetWorld(),
		AttackPos,
		40.f,
		12,
		FColor::Blue,
		false,
		5.0f
	);

	// 선 연결
	DrawDebugLine(
		GetWorld(),
		MonsterLoc,
		TargetLoc,
		FColor::Yellow,
		false,
		5.0f,
		0,
		3.f
	);

	DrawDebugLine(
		GetWorld(),
		TargetLoc,
		AttackPos,
		FColor::Cyan,
		false,
		5.0f,
		0,
		3.f
	);
	*/

	// BB에 AttackPos 설정
	Blackboard->SetValueAsVector(BBKEY_ATTACKPOS, AttackPos);
}
