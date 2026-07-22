// Fill out your copyright notice in the Description page of Project Settings.


#include "Animation/ANS_AttachWeaponToSocket.h"

#include "Animation/AnimNotifyLibrary.h"
#include "Characters/HughCharacter.h"
#include "Components/SkeletalMeshComponent.h"

void UANS_AttachWeaponToSocket::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	
	AHughCharacter* HughCharacter = Cast<AHughCharacter>(MeshComp->GetOwner());
	if (!HughCharacter) return;
	
	//정한 타이밍에 오른손 소켓에 무기 부착하기
	HughCharacter->AttachWeaponToSocket(HandSocketName);
}

void UANS_AttachWeaponToSocket::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
                                          const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);
	
	AHughCharacter* HughCharacter = Cast<AHughCharacter>(MeshComp->GetOwner());
	if (!HughCharacter) return;
	
	
	
	// 노티파이 상태가 정해진 타임라인 끝에 자연스럽게 도달했는지 확인
	bool bReachedEnd = UAnimNotifyLibrary::NotifyStateReachedEnd(EventReference);

	if (bReachedEnd)
	{
		// [정상 종료] 몽타주가 멈추지 않고 노티파이 영역의 끝까지 정상 재생됨
		//정한 타이밍에 왼쪽 등뒤 소켓에 무기 부착하기
		UE_LOG(LogTemp, Warning, TEXT("shotgun ANS_Attachweapon NotifyEnd FINSISHED"));
		HughCharacter->AttachWeaponToSocket(LeftBackSocketName);
	}
	else
	{
		// [중단/스톱] Montage_Stop 호출, 섹션 변경, 또는 다른 몽타주에 의해 
		// 애니메이션이 도중에 끊겨서 노티파이 영역을 다 채우지 못하고 종료됨
		//pickup montage, draw from waist montage 재생 중 Montage_stop에 의해 중단된 경우
		//정한 타이밍에 왼쪽 등뒤 소켓에 무기 부착하기
		UE_LOG(LogTemp, Warning, TEXT("shotgun ANS_Attachweapon NotifyEnd INTERRUPTED"));
		HughCharacter->AttachWeaponToSocket(HandSocketName);
	}
}
