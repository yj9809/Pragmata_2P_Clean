// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Props/DoorBase.h"
#include "PoweredResponBox.generated.h"

class AMonsterTerritoryActor;

/**
 *
 */
UCLASS()
class PRAGMATA_2P_API APoweredResponBox : public ADoorBase
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;
	virtual void UpdateDoorState() override;
	virtual void OpenDoor() override;
	virtual void OnAnimationFinished() override;

	// 델리게이트에 바인딩할 콜백 함수
	UFUNCTION()
	void HandlePowerStateChanged(bool bIsPowerOn);

	// 오버랩 이벤트를 직접 관리하여 상태를 추적합니다.
	virtual void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	virtual void OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	void ActivateLinkedTerritoryTriggers();
	void RemoveAfterOpenDelay();

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Pragmata|Powered Response Box|Territory")
	TArray<TObjectPtr<AMonsterTerritoryActor>> TerritoriesToActivateOnOpen;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pragmata|Powered Response Box|Open Cleanup", meta=(DisplayName="Remove After Opening"))
	bool bRemoveAfterOpening = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pragmata|Powered Response Box|Open Cleanup", meta=(EditCondition="bRemoveAfterOpening", EditConditionHides, ClampMin="0.0", UIMin="0.0", DisplayName="Removal Delay After Opening"))
	float RemovalDelayAfterOpening = 0.f;

	FTimerHandle RemoveAfterOpenTimerHandle;
	bool bHasActivatedLinkedTerritories = false;
	bool bHasScheduledRemovalAfterOpen = false;
	bool bIsPlayerNearby = false; // 현재 플레이어가 트리거 내부에 있는지 추적
	
protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pragmata|Tutorial")
	TObjectPtr<class UTutorialStepData> HackingEnemyTutorialData;
};
