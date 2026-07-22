// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Characters/WaistArmComponent.h"
#include "WaistArmAnimInstance.generated.h"

enum class EWaistArmState : uint8;
/**
 * 
 */
UCLASS()
class PRAGMATA_2P_API UWaistArmAnimInstance : public UAnimInstance
{
	GENERATED_BODY()
public:
	FORCEINLINE void SetArmState(EWaistArmState State) { WaistArmState = State;}
protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="State")
	EWaistArmState WaistArmState = EWaistArmState::Idle;
};
