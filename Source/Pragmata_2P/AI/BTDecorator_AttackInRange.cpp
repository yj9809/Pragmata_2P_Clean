// Fill out your copyright notice in the Description page of Project Settings.

#include "AI/BTDecorator_AttackInRange.h"
#include "Enemy/MonsterBase.h"
#include "AIController.h"
#include "AI/MonsterBBKey.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "DrawDebugHelpers.h"

UBTDecorator_AttackInRange::UBTDecorator_AttackInRange()
{
	NodeName = TEXT("AttackInRange");
}

bool UBTDecorator_AttackInRange::CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const
{
	bool bResult = Super::CalculateRawConditionValue(OwnerComp, NodeMemory);

	// 공격 범위 안에 있는지 판단.
	// 1. 폰 정보 가져오기.
	APawn* ControllingPawn = OwnerComp.GetAIOwner()->GetPawn();
	if (!ControllingPawn)
	{
		return false;
	}

	// 몬스터 클래스로 형변환.
	AMonsterBase* Monster = Cast<AMonsterBase>(ControllingPawn);
	if (!Monster)
	{
		return false;
	}

	if (!Monster->HasSelectedAttackPattern())
	{
		return false;
	}

	// 2. Blackboard target은 Pawn이 아닌 공격 가능한 Actor 계약으로 유지
	AActor* Target = Cast<AActor>(OwnerComp.GetBlackboardComponent()->GetValueAsObject(BBKEY_TARGET));
	if (!Target)
	{
		return false;
	}

	// 거리 계산. 3차원에서 2차원 거리로 변경(감지 이슈 관련)
	//float DistanceToTarget = ControllingPawn->GetDistanceTo(Target);
	float DistanceToTarget = FVector::Dist2D(ControllingPawn->GetActorLocation(), Target->GetActorLocation());

	// 계산한 결과가 공격 범위 안에 있는지 확인.
	const float MinimumAttackRange = Monster->GetMinimumAttackRange();
	const float AttackDetectRange = Monster->GetAIAttackDetectRange();
	bResult = Monster->IsSelectedAttackPatternInRange(Target);

	FVector MonsterLoc = ControllingPawn->GetActorLocation();
	FVector TargetLoc = Target->GetActorLocation();

	// 원통 변수
	float Radius = AttackDetectRange;
	float HalfHeight = 300.f; // 원통 높이 절반

	FVector Bottom = MonsterLoc - FVector(0, 0, HalfHeight);
	FVector Top = MonsterLoc + FVector(0, 0, HalfHeight);

	const FColor DebugColor = bResult ? FColor::Green : FColor::Red;
	const float DrawDuration = 0.1f;
	const float Thickness = 2.f;

	/* 감지 범위 확인 디버깅 코드 -> 지우지 마세요! */
	/*UWorld* World = ControllingPawn->GetWorld();
	if (World)
	{
		DrawDebugCylinder(
			World,
			Bottom,
			Top,
			Radius,
			32,
			DebugColor,
			false,
			DrawDuration,
			0,
			Thickness
		);

		if (MinimumAttackRange > 0.f)
		{
			DrawDebugCylinder(
				World,
				Bottom,
				Top,
				MinimumAttackRange,
				32,
				FColor::Yellow,
				false,
				DrawDuration,
				0,
				Thickness
			);
		}

		DrawDebugLine(
			World,
			MonsterLoc,
			TargetLoc,
			DebugColor,
			false,
			DrawDuration,
			0,
			Thickness
		);

		DrawDebugSphere(
			World,
			TargetLoc,
			25.f,
			12,
			DebugColor,
			false,
			DrawDuration
		);

		DrawDebugString(
			World,
			MonsterLoc + FVector(0, 0, HalfHeight + 50.f),
			FString::Printf(
				TEXT("AttackRange: %.0f - %.0f / Dist: %.0f"),
				MinimumAttackRange,
				AttackDetectRange,
				DistanceToTarget
			),
			nullptr,
			DebugColor,
			DrawDuration,
			true
		);
	}*/

	return bResult;
}
