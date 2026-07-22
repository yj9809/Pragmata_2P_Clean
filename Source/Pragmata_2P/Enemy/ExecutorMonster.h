// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Enemy/MonsterBase.h"
#include "ExecutorMonster.generated.h"

class UMaterialInterface;

/**
 * 
 */
UCLASS()
class PRAGMATA_2P_API AExecutorMonster : public AMonsterBase
{
	GENERATED_BODY()
	
public:
	AExecutorMonster();

protected:
	virtual void BeginPlay() override;
	virtual void HandleHackStateChanged() override;

private:
	void CacheNormalHackMaterialsIfNeeded();
	void ApplyHackMaterialsForCurrentState();

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInterface>> NormalHackSlotMaterials;

	bool bCachedNormalHackMaterials = false;
};
