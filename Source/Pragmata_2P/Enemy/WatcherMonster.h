// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Enemy/MonsterBase.h"
#include "WatcherMonster.generated.h"

/**
 * 
 */
UCLASS()
class PRAGMATA_2P_API AWatcherMonster : public AMonsterBase
{
	GENERATED_BODY()
public:
	AWatcherMonster();
	void FireBullet(const FHitResult& HitResult);
protected:
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayFireEffects(const FHitResult& HitResult);
};
