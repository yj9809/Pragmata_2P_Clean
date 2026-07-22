#include "Components/DianaCleanseComponent.h"

#include "Animation/DianaAnimInstance.h"
#include "Animation/AnimSequenceBase.h"
#include "Characters/HughCharacter.h"
#include "Components/DecalComponent.h"
#include "Components/DianaHackComponent.h"
#include "Components/DianaNodeInteractionComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EngineUtils.h"
#include "Enemy/MonsterBase.h"
#include "Game/PragmataGameState.h"
#include "Game/PragmataPlayerController.h"
#include "Game/PragmataPlayerState.h"
#include "Game/PragmataTraceUtils.h"
#include "Pawns/DianaOperatorPawn.h"
#include "Components/DianaVoiceComponent.h"
#include "Props/DeadFilamentObstacle.h"
#include "Props/SpecialInteractableInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "UObject/SoftObjectPath.h"

namespace
{
	const FName CleansePreviewApexParamName(TEXT("CleansePreviewApex"));
	const FName CleansePreviewForwardParamName(TEXT("CleansePreviewForward"));
	const FName CleansePreviewRightParamName(TEXT("CleansePreviewRight"));
	const FName CleansePreviewRangeParamName(TEXT("CleansePreviewRange"));
	const FName CleansePreviewTanHalfAngleParamName(TEXT("CleansePreviewTanHalfAngle"));
}

UDianaCleanseComponent::UDianaCleanseComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
	CleansePreviewMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/VFX/Diana/M_CleanseConePreview.M_CleanseConePreview")));
	CleanseStartAnimation = TSoftObjectPtr<UAnimSequenceBase>(FSoftObjectPath(TEXT("/Game/Character/Diana/Animations/Attack/AS_Diana_Attack_605_Clean_Up_Start_Lv2.AS_Diana_Attack_605_Clean_Up_Start_Lv2")));
	CleanseLoopAnimation = TSoftObjectPtr<UAnimSequenceBase>(FSoftObjectPath(TEXT("/Game/Character/Diana/Animations/Attack/AS_Diana_Attack_606_Clean_Up_Loop_Lv2.AS_Diana_Attack_606_Clean_Up_Loop_Lv2")));
	CleanseReleaseAnimation = TSoftObjectPtr<UAnimSequenceBase>(FSoftObjectPath(TEXT("/Game/Character/Diana/Animations/Attack/AS_Diana_Attack_633_Clean_Up_Release_Lv2.AS_Diana_Attack_633_Clean_Up_Release_Lv2")));
}

void UDianaCleanseComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UDianaCleanseComponent, bCanUseCleanse);
	DOREPLIFETIME(UDianaCleanseComponent, CleanseCooldownStartServerTime);
	DOREPLIFETIME(UDianaCleanseComponent, CleanseCooldownDurationSeconds);
}

void UDianaCleanseComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdateCleansePreview();
}

float UDianaCleanseComponent::GetCleanseCooldownProgress() const
{
	if (bCanUseCleanse || CleanseCooldownDurationSeconds <= UE_KINDA_SMALL_NUMBER)
	{
		return 1.f;
	}

	const float ElapsedSeconds = GetCurrentServerTimeSeconds() - CleanseCooldownStartServerTime;
	return FMath::Clamp(ElapsedSeconds / CleanseCooldownDurationSeconds, 0.f, 1.f);
}

void UDianaCleanseComponent::RequestCleanse()
{
	// 기존 Blueprint 직접 호출은 button tap과 같은 빠른 실행 경로로 처리
	RequestQuickCleanse();
}

void UDianaCleanseComponent::BeginPrecisionCleanse()
{
	// 정밀 조준은 hold 시작 intent만 보내고 실제 실행은 release에서 확정
	Server_BeginPrecisionCleanse();
}

void UDianaCleanseComponent::CommitPrecisionCleanse()
{
	// release 시점에 서버가 다시 조준/상태를 검증한 뒤 정화 실행
	Server_CommitPrecisionCleanse();
}

void UDianaCleanseComponent::CancelPrecisionCleanse()
{
	// Aim hold가 먼저 끊기면 preview만 정리하고 cooldown은 쓰지 않음
	Server_CancelPrecisionCleanse();
}

void UDianaCleanseComponent::RequestQuickCleanse()
{
	// 빠른 실행은 Hugh 조준 방향 preview를 짧게 보여준 뒤 서버 timer로 실행
	Server_RequestQuickCleanse();
}

void UDianaCleanseComponent::Server_BeginPrecisionCleanse_Implementation()
{
	UWorld* World = GetWorld();
	if (!World || ActiveCleanseMode != EDianaCleanseMode::None)
	{
		return;
	}

	FVector CleanseOrigin = FVector::ZeroVector;
	FVector CleanseDirection = FVector::ForwardVector;
	const float CurrentTime = World->GetTimeSeconds();
	if (!CanStartCleanse(CurrentTime, EDianaCleanseMode::PrecisionDianaCamera, CleanseOrigin, CleanseDirection))
	{
		return;
	}

	// hold 시작 승인 직후 요청해 release 발사와 대사 시점을 분리
	PlayCleanseVoiceOnInput();
	Multicast_SetCleansePreviewMode(EDianaCleanseMode::PrecisionDianaCamera, CleanseDirection);
	Multicast_PlayCleanseAnimation(EDianaCleanseAnimationPhase::Start);
	const UAnimSequenceBase* StartAnimation = CleanseStartAnimation.LoadSynchronous();
	const float StartDuration = StartAnimation ? StartAnimation->GetPlayLength() : 0.f;
	World->GetTimerManager().SetTimer(CleanseStartAnimationTimerHandle, this, &ThisClass::BeginCleanseLoopAnimation,
		FMath::Max(0.01f, StartDuration), false);
}

void UDianaCleanseComponent::Server_CommitPrecisionCleanse_Implementation()
{
	UWorld* World = GetWorld();
	if (!World || ActiveCleanseMode != EDianaCleanseMode::PrecisionDianaCamera)
	{
		return;
	}

	Multicast_PlayCleanseAnimation(EDianaCleanseAnimationPhase::Release);
	ApplyCleanseFromMode(EDianaCleanseMode::PrecisionDianaCamera, World->GetTimeSeconds());
}

void UDianaCleanseComponent::Server_CancelPrecisionCleanse_Implementation()
{
	if (ActiveCleanseMode == EDianaCleanseMode::PrecisionDianaCamera)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(CleanseStartAnimationTimerHandle);
		}
		Multicast_SetCleansePreviewMode(EDianaCleanseMode::None, FVector::ForwardVector);
		Multicast_PlayCleanseAnimation(EDianaCleanseAnimationPhase::Stop);
	}
}

void UDianaCleanseComponent::Server_RequestQuickCleanse_Implementation()
{
	UWorld* World = GetWorld();
	if (!World || ActiveCleanseMode != EDianaCleanseMode::None)
	{
		return;
	}

	FVector CleanseOrigin = FVector::ZeroVector;
	FVector CleanseDirection = FVector::ForwardVector;
	const float CurrentTime = World->GetTimeSeconds();
	if (!CanStartCleanse(CurrentTime, EDianaCleanseMode::QuickHughAim, CleanseOrigin, CleanseDirection))
	{
		return;
	}

	// quick IA 입력 승인 직후 요청해 timer 발사와 대사 시점을 분리
	PlayCleanseVoiceOnInput();
	Multicast_SetCleansePreviewMode(EDianaCleanseMode::QuickHughAim, CleanseDirection);
	World->GetTimerManager().SetTimer(QuickCleanseTimerHandle, this, &UDianaCleanseComponent::ExecutePendingQuickCleanse, QuickCleansePreviewSeconds, false);
}

void UDianaCleanseComponent::Multicast_SetCleansePreviewMode_Implementation(EDianaCleanseMode NewMode, FVector PreviewDirection)
{
	// preview mode는 양쪽 client가 같은 source actor와 방향으로 decal을 그리게 하는 presentation 상태
	ActiveCleanseMode = NewMode;
	ActiveCleansePreviewDirection = PreviewDirection.GetSafeNormal();
	if (ActiveCleansePreviewDirection.IsNearlyZero())
	{
		ActiveCleansePreviewDirection = FVector::ForwardVector;
	}

	if (ActiveCleanseMode == EDianaCleanseMode::None)
	{
		HideCleansePreview();
	}
}

void UDianaCleanseComponent::Multicast_StartCleanseCooldown_Implementation(float CooldownSeconds)
{
	// server가 정화 스킬 cooldown 소비를 확정하면 양쪽 화면 preview를 같은 시간 동안 숨김
	ActiveCleanseMode = EDianaCleanseMode::None;
	StartLocalCleansePreviewCooldown(CooldownSeconds);
}

void UDianaCleanseComponent::Multicast_PlayCleanseAnimation_Implementation(EDianaCleanseAnimationPhase Phase)
{
	PlayCleanseAnimation(Phase);
}

void UDianaCleanseComponent::OnRep_CleanseCooldownState()
{
	// 복제된 cooldown readiness를 ViewModel delegate 경로로 전달
	BroadcastCleanseCooldownChanged();
}

bool UDianaCleanseComponent::CanStartCleanse(float CurrentTime, EDianaCleanseMode CleanseMode, FVector& OutCleanseOrigin, FVector& OutCleanseDirection) const
{
	// 서버가 Diana role, action lock, active hack 상태와 mode별 조준 기준을 재검증
	const UWorld* World = GetWorld();
	const APragmataGameState* GameState = World ? World->GetGameState<APragmataGameState>() : nullptr;
	if (!GameState || GameState->AreDianaActionsLocked())
	{
		return false;
	}

	if (!bCanUseCleanse || CurrentTime < NextAllowedCleanseTime)
	{
		return false;
	}

	const ADianaOperatorPawn* DianaPawn = GetOwner<ADianaOperatorPawn>();
	if (!DianaPawn)
	{
		return false;
	}

	if (CleanseMode == EDianaCleanseMode::PrecisionDianaCamera && !DianaPawn->GetIsAiming())
	{
		return false;
	}

	const EDianaOperatorMode OperatorMode = DianaPawn->GetOperatorMode();
	if (OperatorMode != EDianaOperatorMode::Roaming && OperatorMode != EDianaOperatorMode::Combat)
	{
		return false;
	}

	const APragmataPlayerController* PlayerController = Cast<APragmataPlayerController>(DianaPawn->GetController());
	const APragmataPlayerState* PlayerState = PlayerController ? PlayerController->GetPlayerState<APragmataPlayerState>() : nullptr;
	if (!PlayerState || PlayerState->GetAssignedRole() != EPragmataPlayerRole::Diana || !PlayerState->IsCleanseUnlocked())
	{
		return false;
	}

	if (const UDianaHackComponent* HackComponent = DianaPawn->FindComponentByClass<UDianaHackComponent>())
	{
		// enemy hack UI가 열린 동안은 정화 스킬이 같은 Diana 입력 흐름을 끊지 않게 거부
		if (HackComponent->IsEnemyHackActive())
		{
			return false;
		}
	}

	if (const UDianaNodeInteractionComponent* NodeInteractionComponent = DianaPawn->FindComponentByClass<UDianaNodeInteractionComponent>())
	{
		// map hack puzzle 입력 중에는 정화 스킬이 방향 입력/취소 흐름과 섞이지 않게 거부
		if (NodeInteractionComponent->IsMapHackActive())
		{
			return false;
		}
	}

	return ResolveCleanseModeAim(CleanseMode, OutCleanseOrigin, OutCleanseDirection);
}

bool UDianaCleanseComponent::ResolveCleanseModeAim(EDianaCleanseMode CleanseMode, FVector& OutCleanseOrigin, FVector& OutCleanseDirection) const
{
	// 실행 mode에 따라 gameplay cone이 참조할 조준 원점을 분리
	OutCleanseOrigin = FVector::ZeroVector;
	OutCleanseDirection = FVector::ForwardVector;

	const UWorld* World = GetWorld();
	const APragmataGameState* GameState = World ? World->GetGameState<APragmataGameState>() : nullptr;

	if (CleanseMode == EDianaCleanseMode::PrecisionDianaCamera)
	{
		const ADianaOperatorPawn* DianaPawn = GetOwner<ADianaOperatorPawn>();
		if (!DianaPawn)
		{
			return false;
		}

		OutCleanseOrigin = DianaPawn->GetAuthoritativeCameraOrigin();
		OutCleanseDirection = DianaPawn->GetAuthoritativeCameraDirection().GetSafeNormal();
		return !OutCleanseDirection.IsNearlyZero();
	}

	if (CleanseMode == EDianaCleanseMode::QuickHughAim)
	{
		const AHughCharacter* HughCharacter = GameState ? GameState->GetHughCharacter() : nullptr;
		if (!HughCharacter)
		{
			return false;
		}

		OutCleanseOrigin = HughCharacter->GetAuthoritativeAimOrigin();
		OutCleanseDirection = HughCharacter->GetAuthoritativeAimDirection().GetSafeNormal();
		return !OutCleanseDirection.IsNearlyZero();
	}

	return false;
}

AActor* UDianaCleanseComponent::ResolveCleansePreviewSourceActor(EDianaCleanseMode CleanseMode) const
{
	// preview fan의 발밑 surface는 정밀/빠른 실행 mode별 주체 아래에서 찾음
	if (CleanseMode == EDianaCleanseMode::PrecisionDianaCamera)
	{
		return GetOwner<ADianaOperatorPawn>();
	}

	if (CleanseMode == EDianaCleanseMode::QuickHughAim)
	{
		const UWorld* World = GetWorld();
		const APragmataGameState* GameState = World ? World->GetGameState<APragmataGameState>() : nullptr;
		return GameState ? GameState->GetHughCharacter() : nullptr;
	}

	return nullptr;
}

FVector UDianaCleanseComponent::ResolveCleansePreviewDirection() const
{
	// quick preview는 Hugh controller가 없는 client도 서버가 보낸 조준 방향을 그대로 사용
	if (ActiveCleanseMode == EDianaCleanseMode::QuickHughAim)
	{
		return ActiveCleansePreviewDirection;
	}

	FVector AimOrigin = FVector::ZeroVector;
	FVector AimDirection = FVector::ForwardVector;
	return ResolveCleanseModeAim(ActiveCleanseMode, AimOrigin, AimDirection) ? AimDirection : FVector::ZeroVector;
}

void UDianaCleanseComponent::ExecutePendingQuickCleanse()
{
	// quick mode timer가 만료되면 release 입력 없이 Hugh 조준 기준으로 실행
	UWorld* World = GetWorld();
	if (!World || ActiveCleanseMode != EDianaCleanseMode::QuickHughAim)
	{
		return;
	}

	// quick discharge는 loop 없이 release animation 시작 frame에서 gameplay cast를 확정
	Multicast_PlayCleanseAnimation(EDianaCleanseAnimationPhase::Release);
	ApplyCleanseFromMode(EDianaCleanseMode::QuickHughAim, World->GetTimeSeconds());
}

void UDianaCleanseComponent::BeginCleanseLoopAnimation()
{
	// start가 끝난 뒤에도 정밀 조준이 유지될 때만 loop로 전환
	if (GetOwner() && GetOwner()->HasAuthority() && ActiveCleanseMode == EDianaCleanseMode::PrecisionDianaCamera)
	{
		Multicast_PlayCleanseAnimation(EDianaCleanseAnimationPhase::Loop);
	}
}

void UDianaCleanseComponent::PlayCleanseAnimation(EDianaCleanseAnimationPhase Phase)
{
	// mount mesh AnimBP slot을 통해 body pose를 재생해 IK와 face layer를 유지
	const APragmataGameState* GameState = GetWorld() ? GetWorld()->GetGameState<APragmataGameState>() : nullptr;
	const AHughCharacter* HughCharacter = GameState ? GameState->GetHughCharacter() : nullptr;
	USkeletalMeshComponent* DianaMesh = HughCharacter ? HughCharacter->GetDianaAvatarMesh() : nullptr;
	UDianaAnimInstance* DianaAnim = DianaMesh ? Cast<UDianaAnimInstance>(DianaMesh->GetAnimInstance()) : nullptr;
	if (!DianaAnim)
	{
		return;
	}

	switch (Phase)
	{
	case EDianaCleanseAnimationPhase::Start:
		DianaAnim->PlaySkillBodyAnimation(CleanseStartAnimation.LoadSynchronous());
		break;
	case EDianaCleanseAnimationPhase::Loop:
		DianaAnim->PlaySkillBodyAnimation(CleanseLoopAnimation.LoadSynchronous(), true);
		break;
	case EDianaCleanseAnimationPhase::Release:
		DianaAnim->PlaySkillBodyAnimation(CleanseReleaseAnimation.LoadSynchronous());
		break;
	case EDianaCleanseAnimationPhase::Stop:
		DianaAnim->StopSkillBodyAnimation();
		break;
	default:
		break;
	}
}

// hold 시작 또는 IA 입력이 server에서 유효할 때만 Cleanse 대사를 요청
void UDianaCleanseComponent::PlayCleanseVoiceOnInput()
{
	if (ADianaOperatorPawn* DianaPawn = GetOwner<ADianaOperatorPawn>())
	{
		if (UDianaVoiceComponent* VoiceComponent = DianaPawn->GetVoiceComponent())
		{
			VoiceComponent->PlayVoiceAction(EDianaVoiceAction::Cleanse);
		}
	}
}

void UDianaCleanseComponent::ApplyCleanseFromMode(EDianaCleanseMode CleanseMode, float CurrentTime)
{
	// cooldown 소비와 실제 대상 처리는 server 검증을 통과한 실행 시점에만 수행
	FVector CleanseOrigin = FVector::ZeroVector;
	FVector CleanseDirection = FVector::ForwardVector;
	if (!CanStartCleanse(CurrentTime, CleanseMode, CleanseOrigin, CleanseDirection))
	{
		Multicast_SetCleansePreviewMode(EDianaCleanseMode::None, FVector::ForwardVector);
		return;
	}

	StartCleanseCooldown(CurrentTime);
	Multicast_StartCleanseCooldown(CleanseCooldownSeconds);

	TArray<AActor*> CleanseTargets;
	CollectCleanseTargets(CleanseOrigin, CleanseDirection, CleanseTargets);

	// DeadFilamentObstacle이 검증된 정화 스킬 적용인지 구분하는 짧은 guard
	bServerCleanseExecuting = true;
	for (AActor* TargetActor : CleanseTargets)
	{
		ApplyCleanseToTarget(TargetActor);
	}
	bServerCleanseExecuting = false;

	UE_LOG(LogTemp, Verbose, TEXT("[DianaCleanseComponent::ApplyCleanseFromMode] Applied target count: %d"), CleanseTargets.Num());
}

void UDianaCleanseComponent::StartCleanseCooldown(float CurrentTime)
{
	// 서버 cooldown 상태를 복제해 양쪽 HUD가 같은 readiness와 progress 기준을 사용
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}

	NextAllowedCleanseTime = CurrentTime + CleanseCooldownSeconds;
	bCanUseCleanse = false;
	CleanseCooldownStartServerTime = GetCurrentServerTimeSeconds();
	CleanseCooldownDurationSeconds = CleanseCooldownSeconds;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CleanseCooldownTimerHandle);
		World->GetTimerManager().SetTimer(CleanseCooldownTimerHandle, this, &UDianaCleanseComponent::HandleCleanseCooldownElapsed, CleanseCooldownSeconds, false);
	}

	BroadcastCleanseCooldownChanged();
	Owner->ForceNetUpdate();
}

void UDianaCleanseComponent::HandleCleanseCooldownElapsed()
{
	// cooldown 만료 시 UI readiness만 복구하고 gameplay 검증은 기존 CanStartCleanse가 유지
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}

	NextAllowedCleanseTime = 0.f;
	bCanUseCleanse = true;
	CleanseCooldownStartServerTime = 0.f;
	CleanseCooldownDurationSeconds = CleanseCooldownSeconds;

	BroadcastCleanseCooldownChanged();
	Owner->ForceNetUpdate();
}

void UDianaCleanseComponent::BroadcastCleanseCooldownChanged()
{
	OnCleanseCooldownChanged.Broadcast(bCanUseCleanse, GetCleanseCooldownProgress());
}

float UDianaCleanseComponent::GetCurrentServerTimeSeconds() const
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	return GameState ? GameState->GetServerWorldTimeSeconds() : (World ? World->GetTimeSeconds() : 0.f);
}

void UDianaCleanseComponent::CollectCleanseTargets(const FVector& CameraOrigin, const FVector& CameraDirection, TArray<AActor*>& OutTargets) const
{
	// 정화는 사용 순간에만 월드 후보를 훑고 거리/cone/LOS로 실제 대상 확정
	OutTargets.Reset();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (TActorIterator<ADeadFilamentObstacle> It(World); It; ++It)
	{
		ADeadFilamentObstacle* Obstacle = *It;
		if (IsTargetInCleanseCone(Obstacle, CameraOrigin, CameraDirection) && HasLineOfSightToTarget(Obstacle, CameraOrigin))
		{
			OutTargets.Add(Obstacle);
		}
	}

	for (TActorIterator<AMonsterBase> It(World); It; ++It)
	{
		AMonsterBase* Monster = *It;
		if (Monster
			&& Monster->IsTainted()
			&& !Monster->IsDead()
			&& IsTargetInCleanseCone(Monster, CameraOrigin, CameraDirection)
			&& HasLineOfSightToTarget(Monster, CameraOrigin))
		{
			OutTargets.Add(Monster);
		}
	}
}

bool UDianaCleanseComponent::IsTargetInCleanseCone(AActor* TargetActor, const FVector& CameraOrigin, const FVector& CameraDirection) const
{
	// 화면 pixel radius가 아니라 camera forward와 target 방향의 3D 각도 사용
	if (!TargetActor)
	{
		return false;
	}

	const FVector TargetLocation = TargetActor->GetActorLocation();
	const float CleanseRangeSquared = CleanseRange * CleanseRange;
	if (FVector::DistSquared(CameraOrigin, TargetLocation) > CleanseRangeSquared)
	{
		return false;
	}

	const FVector ToTarget = (TargetLocation - CameraOrigin).GetSafeNormal();
	if (ToTarget.IsNearlyZero())
	{
		return false;
	}

	const float Dot = FVector::DotProduct(CameraDirection, ToTarget);
	const float MinimumDot = FMath::Cos(FMath::DegreesToRadians(CleanseHalfAngleDegrees));
	return Dot >= MinimumDot;
}

bool UDianaCleanseComponent::HasLineOfSightToTarget(AActor* TargetActor, const FVector& CameraOrigin) const
{
	// 정화 대상 직접 hit는 허용하고 앞쪽 blocker만 차단
	UWorld* World = GetWorld();
	if (!World || !TargetActor)
	{
		return false;
	}

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DianaCleanseLineOfSight), false, GetOwner());
	const bool bHit = World->LineTraceSingleByChannel(Hit, CameraOrigin, TargetActor->GetActorLocation(), CleanseTraceChannel, Params);

	return !bHit || PragmataTraceUtils::IsHitActorWithinCandidate(Hit, TargetActor);
}

void UDianaCleanseComponent::ApplyCleanseToTarget(AActor* TargetActor)
{
	// 장애물 제거와 enemy 오염 해제를 같은 서버 cast 결과로 처리
	if (!TargetActor)
	{
		return;
	}

	if (TargetActor->GetClass()->ImplementsInterface(USpecialInteractableInterface::StaticClass()))
	{
		// DeadFilamentObstacle은 interface 내부에서 정화 스킬 guard를 한 번 더 확인
		ISpecialInteractableInterface::Execute_Interact(TargetActor, GetOwner());
		return;
	}

	if (AMonsterBase* Monster = Cast<AMonsterBase>(TargetActor))
	{
		Monster->CleanseTaint(GetOwner());
	}
}

bool UDianaCleanseComponent::CanShowCleansePreviewBase(const ADianaOperatorPawn* DianaPawn) const
{
	// preview는 multicast mode와 복제된 actor 상태를 읽어 각 client가 같은 범위를 그림
	if (!DianaPawn || ActiveCleanseMode == EDianaCleanseMode::None)
	{
		return false;
	}

	if (ActiveCleanseMode == EDianaCleanseMode::PrecisionDianaCamera && !DianaPawn->GetIsAiming())
	{
		return false;
	}

	const UWorld* World = GetWorld();
	const APragmataGameState* GameState = World ? World->GetGameState<APragmataGameState>() : nullptr;
	if (GameState && GameState->AreDianaActionsLocked())
	{
		return false;
	}

	const EDianaOperatorMode OperatorMode = DianaPawn->GetOperatorMode();
	if (OperatorMode != EDianaOperatorMode::Roaming && OperatorMode != EDianaOperatorMode::Combat)
	{
		return false;
	}

	if (const UDianaHackComponent* HackComponent = DianaPawn->FindComponentByClass<UDianaHackComponent>())
	{
		if (HackComponent->IsEnemyHackActive())
		{
			return false;
		}
	}

	if (const UDianaNodeInteractionComponent* NodeInteractionComponent = DianaPawn->FindComponentByClass<UDianaNodeInteractionComponent>())
	{
		if (NodeInteractionComponent->IsMapHackActive())
		{
			return false;
		}
	}

	return true;
}

bool UDianaCleanseComponent::CanShowCleansePreview(const ADianaOperatorPawn* DianaPawn) const
{
	if (!CanShowCleansePreviewBase(DianaPawn))
	{
		return false;
	}

	const UWorld* World = GetWorld();
	const float CurrentTime = World ? World->GetTimeSeconds() : 0.f;
	return !IsLocalCleansePreviewCooldownActive(CurrentTime);
}

bool UDianaCleanseComponent::IsLocalCleansePreviewCooldownActive(float CurrentTime) const
{
	return CurrentTime < LocalCleansePreviewHiddenUntilTime;
}

void UDianaCleanseComponent::StartLocalCleansePreviewCooldown(float CooldownSeconds)
{
	// cooldown 중에는 범위가 다시 보이지 않게 local preview만 숨김
	if (CooldownSeconds <= 0.f)
	{
		return;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	LocalCleansePreviewHiddenUntilTime = World->GetTimeSeconds() + CooldownSeconds;
	HideCleansePreview();
}

bool UDianaCleanseComponent::ResolveCleansePreviewSurface(const AActor* SourceActor, FHitResult& OutHit) const
{
	// 정화 스킬 주체가 서 있는 가장 가까운 표면에 범위 표시를 투영
	UWorld* World = GetWorld();
	if (!World || !SourceActor)
	{
		return false;
	}

	const FVector SourceLocation = SourceActor->GetActorLocation();
	const FVector TraceStart = SourceLocation + FVector::UpVector * CleansePreviewTraceUpDistance;
	const FVector TraceEnd = SourceLocation - FVector::UpVector * CleansePreviewTraceDownDistance;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(DianaCleansePreviewSurface), false, SourceActor);
	return World->LineTraceSingleByChannel(OutHit, TraceStart, TraceEnd, CleanseTraceChannel, Params);
}

void UDianaCleanseComponent::UpdateCleansePreview()
{
	const ADianaOperatorPawn* DianaPawn = GetOwner<ADianaOperatorPawn>();
	if (!CanShowCleansePreview(DianaPawn))
	{
		HideCleansePreview();
		return;
	}

	AActor* SourceActor = ResolveCleansePreviewSourceActor(ActiveCleanseMode);
	const FVector AimDirection = ResolveCleansePreviewDirection();
	if (!SourceActor || AimDirection.IsNearlyZero())
	{
		HideCleansePreview();
		return;
	}

	FHitResult SurfaceHit;
	if (!ResolveCleansePreviewSurface(SourceActor, SurfaceHit))
	{
		HideCleansePreview();
		return;
	}

	UDecalComponent* PreviewDecal = EnsureCleansePreviewComponent();
	if (!PreviewDecal)
	{
		HideCleansePreview();
		return;
	}

	const FVector SurfaceNormal = SurfaceHit.ImpactNormal.GetSafeNormal();
	if (SurfaceNormal.IsNearlyZero())
	{
		HideCleansePreview();
		return;
	}

	// 정화 스킬 preview는 mode별 player surface hit에서 시작해 surface plane 위 전방으로 펼침
	FVector Forward = FVector::VectorPlaneProject(AimDirection, SurfaceNormal);
	if (!Forward.Normalize())
	{
		Forward = FVector::VectorPlaneProject(SourceActor->GetActorForwardVector(), SurfaceNormal);
		if (!Forward.Normalize())
		{
			HideCleansePreview();
			return;
		}
	}

	const float FarHalfWidth = CleanseRange * FMath::Tan(FMath::DegreesToRadians(CleanseHalfAngleDegrees));
	const FVector PreviewStart = SurfaceHit.ImpactPoint + SurfaceNormal * CleansePreviewSurfaceOffset;
	const FVector PreviewLocation = PreviewStart + Forward * (CleanseRange * 0.5f);
	const FRotator PreviewRotation = FRotationMatrix::MakeFromXZ(-SurfaceNormal, Forward).Rotator();

	PreviewDecal->DecalSize = FVector(CleansePreviewProjectionDepth, FarHalfWidth, CleanseRange * 0.5f);
	PreviewDecal->SetWorldLocationAndRotation(PreviewLocation, PreviewRotation);
	UpdateCleansePreviewMaterialParameters(PreviewStart, Forward, SurfaceNormal);
	PreviewDecal->SetHiddenInGame(false);
	PreviewDecal->SetVisibility(true);
}

void UDianaCleanseComponent::UpdateCleansePreviewMaterialParameters(const FVector& PreviewStart, const FVector& Forward, const FVector& SurfaceNormal)
{
	// world-position mask가 decal UV 축과 무관하게 player 위치에서 fan을 시작하게 함
	if (!CleansePreviewMaterialInstance)
	{
		return;
	}

	const FVector Right = FVector::CrossProduct(Forward, SurfaceNormal).GetSafeNormal();
	if (Right.IsNearlyZero())
	{
		return;
	}

	CleansePreviewMaterialInstance->SetVectorParameterValue(CleansePreviewApexParamName, FLinearColor(PreviewStart.X, PreviewStart.Y, PreviewStart.Z, 1.f));
	CleansePreviewMaterialInstance->SetVectorParameterValue(CleansePreviewForwardParamName, FLinearColor(Forward.X, Forward.Y, Forward.Z, 0.f));
	CleansePreviewMaterialInstance->SetVectorParameterValue(CleansePreviewRightParamName, FLinearColor(Right.X, Right.Y, Right.Z, 0.f));
	CleansePreviewMaterialInstance->SetScalarParameterValue(CleansePreviewRangeParamName, CleanseRange);
	CleansePreviewMaterialInstance->SetScalarParameterValue(CleansePreviewTanHalfAngleParamName, FMath::Tan(FMath::DegreesToRadians(CleanseHalfAngleDegrees)));
}

void UDianaCleanseComponent::HideCleansePreview()
{
	if (CleansePreviewDecal)
	{
		CleansePreviewDecal->SetHiddenInGame(true);
		CleansePreviewDecal->SetVisibility(false);
	}
}

UDecalComponent* UDianaCleanseComponent::EnsureCleansePreviewComponent()
{
	// preview decal은 처음 표시할 때만 만들고 이후 tick에서는 재사용
	if (CleansePreviewDecal)
	{
		return CleansePreviewDecal;
	}

	UMaterialInterface* PreviewMaterial = CleansePreviewMaterial.LoadSynchronous();
	if (!PreviewMaterial)
	{
		return nullptr;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}

	CleansePreviewDecal = NewObject<UDecalComponent>(Owner, TEXT("CleansePreviewDecal"));
	if (!CleansePreviewDecal)
	{
		return nullptr;
	}

	CleansePreviewMaterialInstance = UMaterialInstanceDynamic::Create(PreviewMaterial, Owner);

	CleansePreviewDecal->SetIsReplicated(false);
	CleansePreviewDecal->SetHiddenInGame(true);
	CleansePreviewDecal->SetVisibility(false);
	CleansePreviewDecal->SetDecalMaterial(CleansePreviewMaterialInstance ? CleansePreviewMaterialInstance : PreviewMaterial);
	if (USceneComponent* RootComponent = Owner->GetRootComponent())
	{
		CleansePreviewDecal->SetupAttachment(RootComponent);
	}
	CleansePreviewDecal->RegisterComponent();
	return CleansePreviewDecal;
}
