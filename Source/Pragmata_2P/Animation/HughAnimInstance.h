// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "HughAnimInstance.generated.h"

enum class EPragmataHitDirection : uint8;
enum class EMovementState : uint8;
enum class EActionState : uint8;
enum class EWeaponType : uint8;
/**
 * 
 */
UCLASS()
class PRAGMATA_2P_API UHughAnimInstance : public UAnimInstance
{
	GENERATED_BODY()
public:
	UHughAnimInstance();
	void SetActionState(EActionState ActionState);
	FORCEINLINE void SetIsDead(bool bNewIsDead = true) { IsDead = bNewIsDead; };
	FORCEINLINE void SetHitDirection(EPragmataHitDirection Direction){ HitDirection = Direction; };
protected:
	// anim owner 캐시
	virtual void NativeInitializeAnimation() override;
	// 복제 snapshot 기반 애니메이션 파라미터 갱신
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
protected:
	//for property access (cache player info)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="State")
	class AHughCharacter* HughCharacter;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="State")
	TObjectPtr<class UCharacterMovementComponent> Movement;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	EActionState CurrentActionState;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	EMovementState CurrentMovementState;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	float MovementSpeed = 0.f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	float Angle = 0.f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	float LeanAmount = 0.f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	float Acceleration = 0.f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	bool IsAiming = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	bool IsFalling = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	bool IsJumpInput = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	bool IsHovering = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	bool bEnemyCombatActive = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	bool IsDead = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	EPragmataHitDirection HitDirection;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	EWeaponType CurrentWeaponType;
};
