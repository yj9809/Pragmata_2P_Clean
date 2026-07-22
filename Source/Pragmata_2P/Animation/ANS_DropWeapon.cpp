// Fill out your copyright notice in the Description page of Project Settings.


#include "Animation/ANS_DropWeapon.h"

#include "Characters/HughCharacter.h"
#include "Components/SkeletalMeshComponent.h"

void UANS_DropWeapon::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration,
                                  const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	AHughCharacter* HughCharacter = Cast<AHughCharacter>(MeshComp->GetOwner());
	if (!HughCharacter) return;

	HughCharacter->DropWeapon();
}
