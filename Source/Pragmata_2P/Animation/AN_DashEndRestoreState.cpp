// Fill out your copyright notice in the Description page of Project Settings.


#include "Animation/AN_DashEndRestoreState.h"

#include "Characters/HughCharacter.h"

void UAN_DashEndRestoreState::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
                                     const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
	
	AHughCharacter* HughCharacter = Cast<AHughCharacter>(MeshComp->GetOwner());
	if (HughCharacter)
	{
		HughCharacter->SetIsDashing(false);
	}
}
