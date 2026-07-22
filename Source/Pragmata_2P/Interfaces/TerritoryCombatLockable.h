// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "TerritoryCombatLockable.generated.h"

UINTERFACE(BlueprintType, MinimalAPI)
class UTerritoryCombatLockable : public UInterface
{
	GENERATED_BODY()
};

class PRAGMATA_2P_API ITerritoryCombatLockable
{
	GENERATED_BODY()

public:
	// Territory 전투 시작과 종료에 맞춰 map gimmick의 잠금 상태를 변경
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Territory|Combat")
	void SetTerritoryCombatLocked(bool bLocked);

};
