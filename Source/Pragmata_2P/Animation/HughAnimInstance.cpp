// Fill out your copyright notice in the Description page of Project Settings.


#include "Animation/HughAnimInstance.h"

#include "KismetAnimationLibrary.h"
#include "Characters/HughCharacter.h"

UHughAnimInstance::UHughAnimInstance()
{
	
}

void UHughAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	
	HughCharacter = Cast<AHughCharacter>(TryGetPawnOwner());
}

// listen server와 client가 같은 표시용 속도/이동 intent로 blend를 계산
void UHughAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	
	if (HughCharacter == nullptr)
		HughCharacter = Cast<AHughCharacter>(TryGetPawnOwner());

	if (HughCharacter)
	{
		MovementSpeed = HughCharacter->GetMovementSpeed();
		Angle = HughCharacter->GetAngle();
		Acceleration = HughCharacter->GetCharacterMovement()->GetCurrentAcceleration().Length();
		IsAiming = HughCharacter->GetIsAiming();
		IsFalling = HughCharacter->GetCharacterMovement()->IsFalling();
		IsJumpInput = HughCharacter->GetIsJumpInput();
		IsHovering = HughCharacter->GetIsHovering();
		LeanAmount = HughCharacter->GetLeanAmount();
		
		CurrentActionState = HughCharacter->GetActionState();
		CurrentMovementState = HughCharacter->GetMovementState();
		// combat 진입도 locomotion idle은 normal state를 유지. attack/aim montage는 별도 state로 처리
		bEnemyCombatActive = false;
		
		CurrentWeaponType = HughCharacter->GetCurrentWeapon() ? HughCharacter->GetCurrentWeapon()->GetWeaponType() : EWeaponType::Primary;
	}
}

void UHughAnimInstance::SetActionState(EActionState ActionState)
{
	CurrentActionState = ActionState;
}
