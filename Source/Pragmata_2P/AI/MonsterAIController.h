// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "MonsterAIController.generated.h"

class AMonsterTerritoryActor;

UCLASS()
class PRAGMATA_2P_API AMonsterAIController : public AAIController
{
	GENERATED_BODY()

public:
	AMonsterAIController();

	// AI 실행 함수
	void RunAI();

	// AI 중단 함수.
	void StopAI();

public:
	// TargetActor 설정 함수.
	bool SetTarget(AActor* NewTarget);
	void ClearTargetIfSame(AActor* TargetActor);
	void SetTerritory(AMonsterTerritoryActor* NewTerritory);
	void RestartBehaviorAfterTargetAssigned();
	// 피격 감지는 시야와 별개라 연결된 Territory 안쪽 player만 허용
	bool IsActorInsideAssignedTerritory(AActor* Actor) const;

	void SetMonsterTerritory(bool bOutsideTerritory);
	void SetCanAttack(bool bCanAttack);
	void ResetOrbitDirection();

protected:
	// 컨트롤러가 폰에 빙의했을 때 호출되는 함수.
	// BT/BB 애셋 설정 및 실행 처리.
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void UnregisterAggroPawn();

protected:
	// 비헤이비어 트리 애셋.
	UPROPERTY()
	TObjectPtr<class UBehaviorTree> BTAsset;

	UPROPERTY()
	TObjectPtr<AMonsterTerritoryActor> TerritoryActor = nullptr;
};
