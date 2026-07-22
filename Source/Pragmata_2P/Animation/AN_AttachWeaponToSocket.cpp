// Fill out your copyright notice in the Description page of Project Settings.


#include "Animation/AN_AttachWeaponToSocket.h"

#include "Characters/HughCharacter.h"
#include "Components/SkeletalMeshComponent.h"

void UAN_AttachWeaponToSocket::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
                                      const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
	
	AHughCharacter* HughCharacter = Cast<AHughCharacter>(MeshComp->GetOwner());
	if (!HughCharacter) return;
	
	HughCharacter->AttachWeaponToSocket(SocketName);
}
