// Fill out your copyright notice in the Description page of Project Settings.


#include "Animation/ANS_BlockWeaponFire.h"

#include "Characters/HughCharacter.h"
#include "Components/SkeletalMeshComponent.h"

void UANS_BlockWeaponFire::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
                                       float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	
	AHughCharacter* HughCharacter = Cast<AHughCharacter>(MeshComp->GetOwner());
	if (HughCharacter)
	{
		HughCharacter->SetIsWeaponTriggerBlocked(true);
	}
}

void UANS_BlockWeaponFire::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
                                     const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);
	AHughCharacter* HughCharacter = Cast<AHughCharacter>(MeshComp->GetOwner());
	if (HughCharacter)
	{
		HughCharacter->SetIsWeaponTriggerBlocked(false);
	}
}
