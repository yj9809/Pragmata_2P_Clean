#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Data/PragmataTypes.h"
#include "UObject/SoftObjectPtr.h"
#include "DianaAnimInstance.generated.h"

class AHughCharacter;
class ADianaOperatorPawn;
class UAnimSequenceBase;
class UCharacterMovementComponent;
class USceneComponent;

UENUM(BlueprintType)
enum class EDianaAnimLayerMode : uint8
{
	FollowHugh,
	CombatIndependent,
	ForcedPair
};

UENUM(BlueprintType)
enum class EDianaFollowAnimState : uint8
{
	MountIdle,
	Idle,
	Walk,
	Jog,
	JumpStart,
	JumpLoop,
	JumpEnd,
	HoverStart,
	SkyLoop
};

UENUM(BlueprintType)
enum class EDianaCombatAnimState : uint8
{
	Idle,
	Aim,
	Scan,
	HackStart,
	HackLoop,
	HackEnd
};

UENUM(BlueprintType)
enum class EDianaHandIKTargetMode : uint8
{
	None,
	Handle,
	SideHandle
};

UCLASS()
class PRAGMATA_2P_API UDianaAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	UDianaAnimInstance();
	void ResetDeathPresentation();

	UFUNCTION(BlueprintCallable, Category="IK")
	void SetRightHandIKEnabled(bool bEnabled);

	// Diana 전용 body/face slot에 skill animation을 분리 재생
	float PlaySkillBodyAnimation(UAnimSequenceBase* Animation, bool bLoop = false);
	float PlaySkillFaceAnimation(UAnimSequenceBase* Animation, bool bLoop = false);
	void PlayIdleFaceAnimation();
	void StopSkillBodyAnimation(float BlendOutTime = 0.15f);
	void StopSkillFaceAnimation(float BlendOutTime = 0.15f);

protected:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

private:
	void CacheHughCharacter();
	ADianaOperatorPawn* FindDianaOperatorPawn() const;
	ADianaOperatorPawn* ResolveDianaOperatorPawn();
	EDianaOperatorMode ResolveDianaOperatorMode();
	void UpdateLayerMode();
	void UpdateDeathState();
	void UpdateFollowState(float DeltaSeconds);
	void UpdateCombatState(float DeltaSeconds);
	void UpdateHandIKTargetMode();
	void UpdateIKTargets();
	void ResetIKTargets();
	void UpdateSocketIKTarget(USceneComponent* TargetComponent, FName SocketName, FVector& OutLocation, float& OutAlpha);

	// Hugh 상태 참조
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="State", meta=(AllowPrivateAccess="true"))
	TObjectPtr<AHughCharacter> HughCharacter = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="State", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UCharacterMovementComponent> HughMovement = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<ADianaOperatorPawn> DianaOperatorPawn = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<ADianaOperatorPawn> ObservedHackSuccessPawn = nullptr;

	// AnimBP State Machine 전이값
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="State", meta=(AllowPrivateAccess="true"))
	EDianaAnimLayerMode LayerMode = EDianaAnimLayerMode::FollowHugh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="State", meta=(AllowPrivateAccess="true"))
	EDianaFollowAnimState FollowState = EDianaFollowAnimState::MountIdle;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="State", meta=(AllowPrivateAccess="true"))
	EDianaCombatAnimState CombatState = EDianaCombatAnimState::Idle;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="State", meta=(AllowPrivateAccess="true"))
	bool bHackSuccessRequested = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="State", meta=(AllowPrivateAccess="true"))
	bool bHackAimActive = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="State", meta=(AllowPrivateAccess="true"))
	float HackSuccessSignalDuration = 0.35f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="State", meta=(AllowPrivateAccess="true"))
	float MovementSpeed = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="State", meta=(AllowPrivateAccess="true"))
	bool bForcedPairActive = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="State", meta=(AllowPrivateAccess="true"))
	bool bDeathActive = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="State", meta=(AllowPrivateAccess="true"))
	bool bEnemyCombatActive = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="IK", meta=(AllowPrivateAccess="true"))
	EDianaHandIKTargetMode LeftHandIKTargetMode = EDianaHandIKTargetMode::Handle;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="IK", meta=(AllowPrivateAccess="true"))
	EDianaHandIKTargetMode RightHandIKTargetMode = EDianaHandIKTargetMode::Handle;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="IK", meta=(AllowPrivateAccess="true"))
	FVector LeftToeIKLocation = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="IK", meta=(AllowPrivateAccess="true"))
	FVector RightToeIKLocation = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="IK", meta=(AllowPrivateAccess="true"))
	float LeftToeIKAlpha = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="IK", meta=(AllowPrivateAccess="true"))
	float RightToeIKAlpha = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="IK", meta=(AllowPrivateAccess="true"))
	FVector LeftFootIKLocation = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="IK", meta=(AllowPrivateAccess="true"))
	FVector RightFootIKLocation = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="IK", meta=(AllowPrivateAccess="true"))
	float LeftFootIKAlpha = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="IK", meta=(AllowPrivateAccess="true"))
	float RightFootIKAlpha = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="IK", meta=(AllowPrivateAccess="true"))
	FVector LeftHandIKLocation = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="IK", meta=(AllowPrivateAccess="true"))
	FVector RightHandIKLocation = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="IK", meta=(AllowPrivateAccess="true"))
	float LeftHandIKAlpha = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="IK", meta=(AllowPrivateAccess="true"))
	float RightHandIKAlpha = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="IK", meta=(AllowPrivateAccess="true"))
	bool bRightHandIKEnabled = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="IK|Sockets", meta=(AllowPrivateAccess="true"))
	FName LeftToeIKSocketName = TEXT("DianaMountSocket_LeftPaddle_Front");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="IK|Sockets", meta=(AllowPrivateAccess="true"))
	FName RightToeIKSocketName = TEXT("DianaMountSocket_RightPaddle_Front");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="IK|Sockets", meta=(AllowPrivateAccess="true"))
	FName LeftFootIKSocketName = TEXT("DianaMountSocket_LeftPaddle_Back");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="IK|Sockets", meta=(AllowPrivateAccess="true"))
	FName RightFootIKSocketName = TEXT("DianaMountSocket_RightPaddle_Back");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="IK|Sockets", meta=(AllowPrivateAccess="true"))
	FName LeftHandIKSocketName = TEXT("DianaMountSocket_LeftHandle");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="IK|Sockets", meta=(AllowPrivateAccess="true"))
	FName RightHandIKSocketName = TEXT("DianaMountSocket_RightHandle");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="IK|Sockets", meta=(AllowPrivateAccess="true"))
	FName LeftSideHandIKSocketName = TEXT("DianaMountSocket_LeftSideHandle");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="IK|Sockets", meta=(AllowPrivateAccess="true"))
	FName RightSideHandIKSocketName = TEXT("DianaMountSocket_RightSideHandle");

	// 짧게 유지할 전이 pose 시간
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="State", meta=(AllowPrivateAccess="true", ClampMin="0"))
	float JumpEndHoldTime = 0.2f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="State", meta=(AllowPrivateAccess="true", ClampMin="0"))
	float HoverStartHoldTime = 0.2f;

	float JumpEndRemainingTime = 0.f;
	float HoverStartRemainingTime = 0.f;
	bool bWasHughFalling = false;
	bool bWasHughHovering = false;
	bool bWasDianaAiming = false;
	float HackSuccessSignalRemainingTime = 0.f;
	uint8 LastObservedEnemyHackSuccessSerial = 0;

	UPROPERTY(Transient)
	TSoftObjectPtr<UAnimSequenceBase> IdleFaceAnimation;
};
