// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AN_Footstep.generated.h"

/**
 * 
 */
UCLASS()
class PRAGMATA_2P_API UAN_Footstep : public UAnimNotify
{
	GENERATED_BODY()
public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	TObjectPtr<USoundBase> SoundBase;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	FName FootstepSocketName = TEXT("R_Foot");
};

