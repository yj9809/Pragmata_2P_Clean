// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/WaistArmComponent.h"

#include "Animation/WaistArmAnimInstance.h"
#include "Components/SkeletalMeshComponent.h"

// Sets default values for this component's properties
UWaistArmComponent::UWaistArmComponent()
{
	WaistArmMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WaistArmMesh"));
	WaistArmMesh->SetupAttachment(this);
	WaistArmMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WaistArmMesh->SetGenerateOverlapEvents(false);
}


// Called when the game starts
void UWaistArmComponent::BeginPlay()
{
	Super::BeginPlay();
}


// Called every frame
void UWaistArmComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

// Hugh aim 상태를 로봇팔 AnimInstance에 전달해 손/무기 attach와 pose를 맞춤
void UWaistArmComponent::SetArmState(EWaistArmState State)
{
	CurrentState = State;

	if (WaistArmMesh)
	{
		UWaistArmAnimInstance* AnimInstance = Cast<UWaistArmAnimInstance>(WaistArmMesh->GetAnimInstance());
		if (AnimInstance)
		{
			AnimInstance->SetArmState(State);
		}
	}
}

// BP CDO template parent가 남은 child mesh를 실제 component instance 밑으로 되돌림
void UWaistArmComponent::EnsureWaistArmMeshAttachment()
{
	if (IsTemplate() || !WaistArmMesh || WaistArmMesh->IsTemplate())
	{
		return;
	}

	if (WaistArmMesh->GetAttachParent() != this)
	{
		WaistArmMesh->SetupAttachment(this);
	}
}

// aim 전환 montage를 로봇팔 mesh anim instance에서 재생
float UWaistArmComponent::PlayArm_AimEndMontage(float InPlayRate) const
{
	if (WaistArmMesh && AimEndMontage)
	{
		UAnimInstance* AnimInstance = WaistArmMesh->GetAnimInstance();
		if (AnimInstance)
		{
			//재생 중인 경우 끊고 다시 재생
			if (AnimInstance->Montage_IsPlaying(AimEndMontage))
				AnimInstance->Montage_Stop(1.f, AimEndMontage);

			return AnimInstance->Montage_Play(AimEndMontage, InPlayRate);
		}
	}
	return 0.f;
}
