#include "Animation/DianaAnimInstance.h"

#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "Characters/HughCharacter.h"
#include "Characters/WaistArmComponent.h"
#include "Components/PragmataHealthComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EngineUtils.h"
#include "Game/PragmataGameState.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Pawns/DianaOperatorPawn.h"
#include "UObject/UnrealType.h"

namespace DianaAnimNames
{
	static const FName SkillBodySlot(TEXT("DianaSkillBody"));
	static const FName SkillFaceSlot(TEXT("DianaSkillFace"));
	static const FName LeftToeIKLocation(TEXT("LeftToeIKLocation"));
	static const FName RightToeIKLocation(TEXT("RightToeIKLocation"));
	static const FName LeftToeIKAlpha(TEXT("LeftToeIKAlpha"));
	static const FName RightToeIKAlpha(TEXT("RightToeIKAlpha"));
	static const FName LeftFootIKLocation(TEXT("LeftFootIKLocation"));
	static const FName RightFootIKLocation(TEXT("RightFootIKLocation"));
	static const FName LeftFootIKAlpha(TEXT("LeftFootIKAlpha"));
	static const FName RightFootIKAlpha(TEXT("RightFootIKAlpha"));
	static const FName LeftHandIKLocation(TEXT("LeftHandIKLocation"));
	static const FName RightHandIKLocation(TEXT("RightHandIKLocation"));
	static const FName LeftHandIKAlpha(TEXT("LeftHandIKAlpha"));
	static const FName RightHandIKAlpha(TEXT("RightHandIKAlpha"));
}

UDianaAnimInstance::UDianaAnimInstance()
{
	IdleFaceAnimation = TSoftObjectPtr<UAnimSequenceBase>(FSoftObjectPath(
		TEXT("/Game/Character/Diana/Animations/Facial/Emotion/AS_Diana_Face_Emotion_Calm_Basic_Loop.AS_Diana_Face_Emotion_Calm_Basic_Loop")));
}

float UDianaAnimInstance::PlaySkillBodyAnimation(UAnimSequenceBase* Animation, bool bLoop)
{
	// skill body는 기존 locomotion pose 위 dedicated slot으로만 합성
	UAnimMontage* Montage = Animation
		? PlaySlotAnimationAsDynamicMontage(Animation, DianaAnimNames::SkillBodySlot, 0.08f, 0.12f, 1.f, bLoop ? MAX_int32 : 1)
		: nullptr;
	return Montage ? Montage->GetPlayLength() : 0.f;
}

float UDianaAnimInstance::PlaySkillFaceAnimation(UAnimSequenceBase* Animation, bool bLoop)
{
	// face slot은 Neck부터 blend되어 body/IK pose를 덮지 않음
	UAnimMontage* Montage = Animation
		? PlaySlotAnimationAsDynamicMontage(Animation, DianaAnimNames::SkillFaceSlot, 0.08f, 0.12f, 1.f, bLoop ? MAX_int32 : 1)
		: nullptr;
	return Montage ? Montage->GetPlayLength() : 0.f;
}

void UDianaAnimInstance::PlayIdleFaceAnimation()
{
	// 기본 표정은 face slot만 점유해 locomotion과 body skill을 보존
	PlaySkillFaceAnimation(IdleFaceAnimation.LoadSynchronous(), true);
}

void UDianaAnimInstance::StopSkillBodyAnimation(float BlendOutTime)
{
	StopSlotAnimation(BlendOutTime, DianaAnimNames::SkillBodySlot);
}

void UDianaAnimInstance::StopSkillFaceAnimation(float BlendOutTime)
{
	StopSlotAnimation(BlendOutTime, DianaAnimNames::SkillFaceSlot);
}

// AnimBP가 mount mesh 소유자인 Hugh 상태를 바로 읽을 수 있게 초기 캐시
void UDianaAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	CacheHughCharacter();
	PlayIdleFaceAnimation();
}

// Hugh/Diana runtime 상태를 AnimBP 전이 변수와 IK target으로 변환
void UDianaAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (!HughCharacter)
	{
		CacheHughCharacter();
	}

	UpdateLayerMode();
	UpdateDeathState();
	UpdateFollowState(DeltaSeconds);
	UpdateCombatState(DeltaSeconds);
	UpdateHandIKTargetMode();
	UpdateIKTargets();
}

void UDianaAnimInstance::CacheHughCharacter()
{
	// Diana mesh는 Hugh actor에 붙은 mesh라 owning actor를 Hugh로 취급
	HughCharacter = Cast<AHughCharacter>(GetOwningActor());
	HughMovement = HughCharacter ? HughCharacter->GetCharacterMovement() : nullptr;
}

ADianaOperatorPawn* UDianaAnimInstance::FindDianaOperatorPawn() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	const APragmataGameState* GameState = World->GetGameState<APragmataGameState>();
	if (ADianaOperatorPawn* GameStateDianaPawn = GameState ? GameState->GetDianaOperatorPawn() : nullptr)
	{
		return GameStateDianaPawn;
	}

	// GameState pointer가 없던 과거 빌드와 PIE 초기화 순서를 위한 fallback 조회
	for (TActorIterator<ADianaOperatorPawn> It(World); It; ++It)
	{
		return *It;
	}

	return nullptr;
}

ADianaOperatorPawn* UDianaAnimInstance::ResolveDianaOperatorPawn()
{
	// 캐시된 Diana pawn이 사라진 경우에만 느린 world scan fallback을 사용
	if (!DianaOperatorPawn || DianaOperatorPawn->IsActorBeingDestroyed())
	{
		DianaOperatorPawn = FindDianaOperatorPawn();
	}

	return DianaOperatorPawn;
}

EDianaOperatorMode UDianaAnimInstance::ResolveDianaOperatorMode()
{
	const ADianaOperatorPawn* ResolvedDianaPawn = ResolveDianaOperatorPawn();
	return ResolvedDianaPawn ? ResolvedDianaPawn->GetOperatorMode() : EDianaOperatorMode::Roaming;
}

// 강제 피격/사망 연출은 Diana 독립 조작보다 높은 layer 우선순위를 가짐
void UDianaAnimInstance::UpdateLayerMode()
{
	bForcedPairActive = false;

	const UWorld* World = GetWorld();
	const APragmataGameState* GameState = World ? World->GetGameState<APragmataGameState>() : nullptr;
	const EDianaOperatorMode OperatorMode = ResolveDianaOperatorMode();
	// combat flag는 gameplay lock 판단에만 사용하고 Diana idle pose 전환에는 사용하지 않음
	bEnemyCombatActive = false;

	bForcedPairActive = (GameState && GameState->AreDianaActionsLocked())
		|| OperatorMode == EDianaOperatorMode::HitReactLocked
		|| OperatorMode == EDianaOperatorMode::MountedWatch; // Diana 독립 조작을 끊는 강제 연출

	// 강제 동기화 montage가 일반 상태보다 우선
	if (bForcedPairActive)
	{
		LayerMode = EDianaAnimLayerMode::ForcedPair;
		return;
	}

	// combat operator mode는 입력·스킬 경계이며 Diana idle layer를 tense pose로 바꾸지 않음
	LayerMode = EDianaAnimLayerMode::FollowHugh;
}

// Diana mounted death pose는 Hugh의 사망 상태를 기준으로 전이
void UDianaAnimInstance::UpdateDeathState()
{
	if (!HughCharacter)
	{
		bDeathActive = false;
		return;
	}

	const UPragmataHealthComponent* HealthComponent = HughCharacter->GetHealthComponent();
	const bool bHughDead = HughCharacter->GetActionState() == EActionState::Death
		|| (HealthComponent && HealthComponent->IsDead());
	bDeathActive = bHughDead;
}

// Respawn 시 death montage와 native death/IK 상태를 locomotion 기준으로 복구
void UDianaAnimInstance::ResetDeathPresentation()
{
	Montage_Stop(0.f);
	bDeathActive = false;
	SetRightHandIKEnabled(true);
	if (HughCharacter)
	{
		HughCharacter->SetDianaAvatarDeathAttachment(false);
	}
}

void UDianaAnimInstance::UpdateFollowState(float DeltaSeconds)
{
	// Hugh 이동 상태를 Diana mount용 상위 state로 압축해 AnimBP 전이 복잡도를 낮춤
	if (!HughCharacter || !HughMovement)
	{
		MovementSpeed = 0.f;
		FollowState = EDianaFollowAnimState::MountIdle;
		bWasHughFalling = false;
		bWasHughHovering = false;
		JumpEndRemainingTime = 0.f;
		HoverStartRemainingTime = 0.f;
		return;
	}

	MovementSpeed = HughCharacter->GetMovementSpeed();

	const bool bIsHughHovering = HughCharacter->GetIsHovering();
	// 호버 진입 pose 짧게 유지
	if (bIsHughHovering && !bWasHughHovering)
	{
		HoverStartRemainingTime = HoverStartHoldTime;
	}
	bWasHughHovering = bIsHughHovering;

	const bool bIsHughFalling = HughMovement->IsFalling();
	// 착지 pose 짧게 유지
	if (!bIsHughFalling && bWasHughFalling)
	{
		JumpEndRemainingTime = JumpEndHoldTime;
	}
	bWasHughFalling = bIsHughFalling;

	if (bIsHughHovering)
	{
		if (HoverStartRemainingTime > 0.f)
		{
			HoverStartRemainingTime = FMath::Max(0.f, HoverStartRemainingTime - DeltaSeconds);
			FollowState = EDianaFollowAnimState::HoverStart;
			return;
		}

		FollowState = EDianaFollowAnimState::SkyLoop;
		return;
	}

	HoverStartRemainingTime = 0.f;

	if (bIsHughFalling)
	{
		FollowState = HughCharacter->GetVelocity().Z > 0.f
			? EDianaFollowAnimState::JumpStart
			: EDianaFollowAnimState::JumpLoop;
		return;
	}

	if (JumpEndRemainingTime > 0.f)
	{
		JumpEndRemainingTime = FMath::Max(0.f, JumpEndRemainingTime - DeltaSeconds);
		FollowState = EDianaFollowAnimState::JumpEnd;
		return;
	}

	switch (HughCharacter->GetMovementState())
	{
	case EMovementState::Walk:
		FollowState = EDianaFollowAnimState::Walk;
		break;
	case EMovementState::Jog:
	case EMovementState::Run:
		FollowState = EDianaFollowAnimState::Jog;
		break;
	case EMovementState::Idle:
	default:
		FollowState = EDianaFollowAnimState::MountIdle;
		break;
	}
}

void UDianaAnimInstance::UpdateCombatState(float DeltaSeconds)
{
	// Diana aim 입력과 hack 성공 serial을 Hugh식 AnimBP 전이값으로 노출
	ADianaOperatorPawn* ResolvedDianaPawn = ResolveDianaOperatorPawn();
	const bool bIsDianaAiming = ResolvedDianaPawn && ResolvedDianaPawn->GetIsAiming();
	bHackAimActive = bIsDianaAiming;
	// aim과 skill body slot 모두 authored 오른손 pose를 Hugh socket IK보다 우선
	SetRightHandIKEnabled(!bIsDianaAiming && !IsSlotActive(DianaAnimNames::SkillBodySlot));

	if (ResolvedDianaPawn)
	{
		const uint8 CurrentSuccessSerial = ResolvedDianaPawn->GetEnemyHackSuccessSerial();
		if (ObservedHackSuccessPawn != ResolvedDianaPawn)
		{
			// 새 pawn 관측 시 기존 serial은 기준값으로만 저장해 과거 성공 animation 재생을 막음
			ObservedHackSuccessPawn = ResolvedDianaPawn;
			LastObservedEnemyHackSuccessSerial = CurrentSuccessSerial;
			HackSuccessSignalRemainingTime = 0.f;
		}
		else if (CurrentSuccessSerial != LastObservedEnemyHackSuccessSerial)
		{
			LastObservedEnemyHackSuccessSerial = CurrentSuccessSerial;
			HackSuccessSignalRemainingTime = HackSuccessSignalDuration;
		}
	}
	else
	{
		ObservedHackSuccessPawn = nullptr;
		HackSuccessSignalRemainingTime = 0.f;
	}
	if (HackSuccessSignalRemainingTime > 0.f)
	{
		HackSuccessSignalRemainingTime = FMath::Max(0.f, HackSuccessSignalRemainingTime - DeltaSeconds);
	}
	bHackSuccessRequested = HackSuccessSignalRemainingTime > 0.f;

	if (bIsDianaAiming)
	{
		CombatState = bWasDianaAiming ? EDianaCombatAnimState::HackLoop : EDianaCombatAnimState::HackStart;
	}
	else
	{
		CombatState = bWasDianaAiming ? EDianaCombatAnimState::HackEnd : EDianaCombatAnimState::Idle;
	}
	bWasDianaAiming = bIsDianaAiming;
}

void UDianaAnimInstance::UpdateHandIKTargetMode()
{
	// follow/combat layer에 따라 손잡이 socket을 바꿔 pose와 손 위치를 맞춤
	EDianaHandIKTargetMode TargetMode = EDianaHandIKTargetMode::SideHandle;

	if (!HughCharacter)
	{
		TargetMode = EDianaHandIKTargetMode::None;
	}
	else if (LayerMode == EDianaAnimLayerMode::CombatIndependent)
	{
		TargetMode = EDianaHandIKTargetMode::Handle;
	}
	else if (LayerMode == EDianaAnimLayerMode::FollowHugh)
	{
		switch (FollowState)
		{
		case EDianaFollowAnimState::MountIdle:
		case EDianaFollowAnimState::Idle:
			TargetMode = EDianaHandIKTargetMode::Handle;
			break;
		default:
			TargetMode = EDianaHandIKTargetMode::SideHandle;
			break;
		}
	}

	LeftHandIKTargetMode = TargetMode;
	RightHandIKTargetMode = TargetMode;
}

void UDianaAnimInstance::UpdateIKTargets()
{
	// AnimGraph가 native 변수만 읽도록 socket world 위치를 mesh local 위치로 변환
	ResetIKTargets();

	if (bDeathActive)
	{
		// 사망 pose를 Hugh socket IK가 덮지 않도록 모든 IK alpha를 0으로 유지
		return;
	}

	if (!HughCharacter)
	{
		return;
	}

	UWaistArmComponent* WaistArmComponent = HughCharacter->FindComponentByClass<UWaistArmComponent>();
	USkeletalMeshComponent* WaistArmMesh = WaistArmComponent ? WaistArmComponent->GetWaistArmMesh() : nullptr;
	UpdateSocketIKTarget(WaistArmMesh, LeftToeIKSocketName, LeftToeIKLocation, LeftToeIKAlpha);
	UpdateSocketIKTarget(WaistArmMesh, RightToeIKSocketName, RightToeIKLocation, RightToeIKAlpha);

	// Paddle_Back sockets are kept for the dormant foot FABRIK path, but current IK binds only the middle toes.

	USkeletalMeshComponent* HughMesh = HughCharacter->GetMesh();
	if (LeftHandIKTargetMode == EDianaHandIKTargetMode::Handle)
	{
		UpdateSocketIKTarget(HughMesh, LeftHandIKSocketName, LeftHandIKLocation, LeftHandIKAlpha);
	}
	else if (LeftHandIKTargetMode == EDianaHandIKTargetMode::SideHandle)
	{
		UpdateSocketIKTarget(HughMesh, LeftSideHandIKSocketName, LeftHandIKLocation, LeftHandIKAlpha);
	}

	if (RightHandIKTargetMode == EDianaHandIKTargetMode::Handle)
	{
		if (bRightHandIKEnabled)
		{
			UpdateSocketIKTarget(HughMesh, RightHandIKSocketName, RightHandIKLocation, RightHandIKAlpha);
		}
	}
	else if (RightHandIKTargetMode == EDianaHandIKTargetMode::SideHandle)
	{
		if (bRightHandIKEnabled)
		{
			UpdateSocketIKTarget(HughMesh, RightSideHandIKSocketName, RightHandIKLocation, RightHandIKAlpha);
		}
	}
}

void UDianaAnimInstance::ResetIKTargets()
{
	// socket 검증에 실패한 IK limb는 alpha 0으로 안전하게 꺼 둠
	LeftToeIKAlpha = 0.f;
	RightToeIKAlpha = 0.f;
	LeftFootIKAlpha = 0.f;
	RightFootIKAlpha = 0.f;
	LeftHandIKAlpha = 0.f;
	RightHandIKAlpha = 0.f;
}

void UDianaAnimInstance::SetRightHandIKEnabled(bool bEnabled)
{
	// hacking pose에서는 오른손 animation을 IK가 덮지 않도록 alpha를 native에서 차단
	bRightHandIKEnabled = bEnabled;
	if (!bRightHandIKEnabled)
	{
		RightHandIKAlpha = 0.f;
	}
}

void UDianaAnimInstance::UpdateSocketIKTarget(USceneComponent* TargetComponent, FName SocketName, FVector& OutLocation, float& OutAlpha)
{
	// target socket이 없으면 해당 IK만 비활성화해 AnimBP graph를 그대로 유지
	USkeletalMeshComponent* DianaMesh = GetSkelMeshComponent();
	if (!TargetComponent || !DianaMesh || SocketName.IsNone() || !TargetComponent->DoesSocketExist(SocketName))
	{
		OutAlpha = 0.f;
		return;
	}

	const FVector SocketWorldLocation = TargetComponent->GetSocketLocation(SocketName);
	OutLocation = DianaMesh->GetComponentTransform().InverseTransformPosition(SocketWorldLocation);
	OutAlpha = 1.f;
}
