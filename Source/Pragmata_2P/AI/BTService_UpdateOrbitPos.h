// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "BTService_UpdateOrbitPos.generated.h"

class AMonsterBase;

UCLASS()
class PRAGMATA_2P_API UBTService_UpdateOrbitPos : public UBTService
{
	GENERATED_BODY()

public:
	UBTService_UpdateOrbitPos();

protected:
	virtual void OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual void OnCeaseRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI|Orbit", meta=(AllowPrivateAccess="true", ClampMin="0.1", ClampMax="0.95"))
	float OrbitRadiusScale = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI|Orbit", meta=(AllowPrivateAccess="true", ClampMin="0.0"))
	float MinimumOrbitRadius = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI|Orbit", meta=(AllowPrivateAccess="true", ClampMin="1.0", ClampMax="90.0"))
	float OrbitStepAngle = 90.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI|Orbit", meta=(AllowPrivateAccess="true", ClampMin="0.0"))
	float SideDeadZone = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI|Orbit", meta=(AllowPrivateAccess="true"))
	FVector NavProjectionExtent = FVector(200.f, 200.f, 500.f);

	TWeakObjectPtr<AMonsterBase> ActiveMonster;
	bool bOrbitClockwise = false;
	bool bPreviousOrientRotationToMovement = true;
	bool bPreviousUseControllerDesiredRotation = false;
};
