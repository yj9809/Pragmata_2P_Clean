// Fill out your copyright notice in the Description page of Project Settings.


#include "Animation/ANS_ChangeWeaponSocket.h"

#include "Characters/HughCharacter.h"
#include "Weapon/WeaponBase.h"

void UANS_ChangeWeaponSocket::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
                                          float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	
	if (!MeshComp) return;
	
	AHughCharacter* Character = Cast<AHughCharacter>(MeshComp->GetOwner());
	if (Character)
	{
		AWeaponBase* CurrentWeapon = Character->GetCurrentWeapon();
		if (CurrentWeapon)
		{
			CurrentWeapon->AttachToComponent(MeshComp, FAttachmentTransformRules::SnapToTargetNotIncludingScale, TargetSocketName);
		}
	}
}

void UANS_ChangeWeaponSocket::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	float FrameDeltaTime, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);
}

void UANS_ChangeWeaponSocket::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);
}
