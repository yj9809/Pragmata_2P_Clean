// Fill out your copyright notice in the Description page of Project Settings.


#include "Animation/MonsterAnimInstance.h"

#include "Enemy/MonsterBase.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"


UMonsterAnimInstance::UMonsterAnimInstance()
{
	// 이동 판단 기준 값
	MovingThreshold = 3.0f;

	// 점프 중인지 판단할 기준 값.
	JumpingThreshold = 100.0f;
}

void UMonsterAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	// 애니메이션 소유 캐릭터 저장
	Owner = Cast<ACharacter>(GetOwningActor());

	// 캐릭터 무브먼트 컴포넌트 저장.
	if (Owner)
	{
		Movement = Owner->GetCharacterMovement();
	}
}

void UMonsterAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	// 애니메이션 재생에 사용할 값 설정.
	if (Movement && Owner)
	{
		// 현재 속도.
		Velocity = Movement->Velocity;

		// 지면에서 이동하는 속력(빠르기).
		GroundSpeed = Velocity.Size2D();

		// 이동/정지 상태 설정.
		bIsIdle = GroundSpeed < MovingThreshold;

		// 공중에 떠 있는지 확인.
		bIsFalling = Movement->IsFalling();

		// Z축 속도 저장
		VelocityZ = Velocity.Z;

		// 점프 중인지 판단.
		bIsJumping = (bIsFalling && (VelocityZ > JumpingThreshold));
		
		AMonsterBase* Monster = Cast<AMonsterBase>(Owner);
		if (!Monster)
		{
			return;
		}

		// replicated monster 사망 상태와 상황 분기를 AnimBP 변수로 전달
		bIsDeath = Monster->IsDead();
		DeathAnimation = Monster->GetDeathAnimation();

		//is attacking target?
		IsAttacking = Monster->GetIsAttacking();
		Angle = Monster->GetAngle();

	}
}
