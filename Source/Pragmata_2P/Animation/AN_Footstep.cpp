// Fill out your copyright notice in the Description page of Project Settings.


#include "Animation/AN_Footstep.h"

#include "Kismet/GameplayStatics.h"

void UAN_Footstep::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
                          const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
	
	if (!MeshComp) return;
	
	FVector SoundLocation = MeshComp->GetBoneLocation(FootstepSocketName);
	
	if (SoundBase)
	{
		// 1번 대화에서 다룬 'Fire-and-Forget' 방식으로 발 위치에 소리 재생
		UGameplayStatics::PlaySoundAtLocation(
			MeshComp->GetWorld(), 
			SoundBase, 
			SoundLocation,
			FRotator::ZeroRotator,
			1.0f, // 볼륨
			1.0f  // 피치
		);
	}
}
