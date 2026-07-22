// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "ANS_AttachWeaponToSocket.generated.h"

/**
 * 
 */
UCLASS()
class PRAGMATA_2P_API UANS_AttachWeaponToSocket : public UAnimNotifyState
{
	GENERATED_BODY()
protected:
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Socket")
	FName HandSocketName = TEXT("WeaponSocket");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Socket")
	FName LeftBackSocketName = TEXT("LeftWeaponSocket");
};
