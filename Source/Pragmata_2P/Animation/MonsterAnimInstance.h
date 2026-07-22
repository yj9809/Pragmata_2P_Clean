// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "MonsterAnimInstance.generated.h"

class UAnimSequenceBase;

/**
 * 
 */
UCLASS()
class PRAGMATA_2P_API UMonsterAnimInstance : public UAnimInstance
{
	GENERATED_BODY()
public:
	UMonsterAnimInstance();

protected:
	// 애니메이션 초기화 될 때 호출되는 함수.
	virtual void NativeInitializeAnimation() override;

	// 애니메이션을 업데이트 할 때마다(틱 마다) 실행되는 함수.
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

protected:
	// 오너를 저장해두고 재활용.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character")
	TObjectPtr<class ACharacter> Owner;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character")
	TObjectPtr<class UCharacterMovementComponent> Movement;

	// Locomotion
	// Idle - Move 전환을 위해 사용할 속도.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character")
	FVector Velocity;

	// 이동 빠르기 (블랜드 스페이스에 적용).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character")
	float GroundSpeed;

	// 이동하는지 멈춰있는지 확인하는 변수.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character")
	uint8 bIsIdle : 1;

	// 이동 여부를 판단할 때 사용할 문턱 값.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character")
	float MovingThreshold;

	// Fall
	// 떨어지고 있는지 확인하는 변수.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character")
	uint8 bIsFalling : 1;

	// Jump
	// 점프해서 위로 솟구치던 속도
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character")
	float VelocityZ;

	// 점프 하고 있는 중인지 확인하는 변수
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character")
	uint8 bIsJumping : 1;

	// 점프 여부를 판단할 때 사용할 문턱 값.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character")
	float JumpingThreshold;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character")
	uint8 IsAttacking : 1;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character")
	float Angle;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character")
	uint8 bIsDeath : 1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Character")
	TObjectPtr<UAnimSequenceBase> DeathAnimation = nullptr;
};
