#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_OrbitAroundTarget.generated.h"

class AMonsterBase;

UCLASS()
class PRAGMATA_2P_API UBTTask_OrbitAroundTarget : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_OrbitAroundTarget();

protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void OnTaskFinished(
		UBehaviorTreeComponent& OwnerComp,
		uint8* NodeMemory,
		EBTNodeResult::Type TaskResult
	) override;

private:
	void RestoreMovement(UBehaviorTreeComponent& OwnerComp);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI|Orbit", meta=(AllowPrivateAccess="true", ClampMin="0.1", ClampMax="0.95"))
	float OrbitRadiusScale = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI|Orbit", meta=(AllowPrivateAccess="true", ClampMin="0.0"))
	float MinimumOrbitRadius = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI|Orbit", meta=(AllowPrivateAccess="true", ClampMin="0.0"))
	float RadialCorrectionStrength = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI|Orbit", meta=(AllowPrivateAccess="true", ClampMin="0.0", ClampMax="1.0"))
	float MaximumRadialCorrection = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI|Orbit", meta=(AllowPrivateAccess="true", ClampMin="0.0"))
	float SideDeadZone = 10.f;

	// 회전 속도 설정
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI|Orbit", meta=(AllowPrivateAccess="true", ClampMin="0.0"))
	float FacingRotationRate = 80.f;

	TWeakObjectPtr<AMonsterBase> ActiveMonster;
	bool bOrbitClockwise = false;
	bool bPreviousOrientRotationToMovement = true;
	bool bPreviousUseControllerDesiredRotation = false;
	FRotator PreviousRotationRate = FRotator::ZeroRotator;
};
