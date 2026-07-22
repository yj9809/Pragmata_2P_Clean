#include "Components/DianaHackComponent.h"

#include "Characters/HughCharacter.h"
#include "Components/PragmataPlayerStatsComponent.h"
#include "Game/PragmataGameState.h"
#include "Game/PragmataPlayerController.h"
#include "Game/PragmataPlayerState.h"
#include "Game/PragmataTraceUtils.h"
#include "Interfaces/PragmataHackable.h"
#include "Net/UnrealNetwork.h"
#include "Puzzle/EnemyHackingPuzzleComponent.h"

#include "Components/OverDriveComponent.h"
#include "Pawns/DianaOperatorPawn.h"
#include "Components/DianaVoiceComponent.h"

UDianaHackComponent::UDianaHackComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

// 해킹 피해 업그레이드는 서버 값만 gameplay 판정에 사용하도록 복제
void UDianaHackComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UDianaHackComponent, HackDamageUpgradeLevel);
}

void UDianaHackComponent::RequestHack(AActor* TargetActor)
{
	// Diana 클라이언트 입력은 서버 검증 RPC로만 해킹 판정을 시작
	if (!GetOwner())
	{
		return;
	}

	Server_RequestHack(TargetActor);
}

void UDianaHackComponent::RequestHackDirection(EHackDirection Direction)
{
	if (IsEnemyHackActive())
	{
		Server_ProcessEnemyHackDirection(Direction);
	}
}

void UDianaHackComponent::RequestCancelHack(AActor* ExpectedTarget)
{
	if (IsEnemyHackActive())
	{
		Server_CancelEnemyHack(ExpectedTarget);
	}
}

bool UDianaHackComponent::IsEnemyHackActive() const
{
	return GetActiveEnemyHackTarget() != nullptr;
}

AActor* UDianaHackComponent::GetActiveEnemyHackTarget() const
{
	// owner client는 local active state, 다른 화면은 GameState 복제 state를 기준으로 표시
	if (ActivePuzzleComponent && ActiveHackTarget)
	{
		return ActiveHackTarget;
	}

	const UWorld* World = GetWorld();
	const APragmataGameState* GameState = World ? World->GetGameState<APragmataGameState>() : nullptr;
	return GameState ? GameState->GetActiveEnemyHackTarget() : nullptr;
}

float UDianaHackComponent::GetHackDamage() const
{
	return BaseHackDamage + HackDamagePerUpgrade * HackDamageUpgradeLevel;
}

void UDianaHackComponent::SetHackDamageUpgradeLevel(int32 NewLevel)
{
	// 업그레이드 단계는 권한 서버에서만 올려 클라이언트 피해 조작을 막음
	if (AActor* Owner = GetOwner(); Owner && Owner->HasAuthority())
	{
		HackDamageUpgradeLevel = FMath::Max(0, NewLevel);
	}
}

void UDianaHackComponent::Server_RequestHack_Implementation(AActor* TargetActor)
{
	// 서버가 target, 시점, 거리, 시야를 모두 재검증한 뒤 puzzle을 시작
	FVector CameraOrigin = FVector::ZeroVector;
	FVector CameraDirection = FVector::ForwardVector;
	if (!ValidateHackTarget(TargetActor, CameraOrigin, CameraDirection))
	{
		return;
	}

	UEnemyHackingPuzzleComponent* PuzzleComponent = TargetActor->FindComponentByClass<UEnemyHackingPuzzleComponent>();
	if (!PuzzleComponent)
	{
		return;
	}

	ClearActiveEnemyHack(true);
	PuzzleComponent->SetBaseHackDamage(GetHackDamage());
	if (!PuzzleComponent->StartHack())
	{
		return;
	}

	if (ADianaOperatorPawn* DianaPawn = GetOwner<ADianaOperatorPawn>())
	{
		DianaPawn->GetVoiceComponent()->PlayVoiceAction(EDianaVoiceAction::HackingAim);
	}

	PuzzleComponent->OnHackCompleted.RemoveAll(this);
	PuzzleComponent->OnHackCompleted.AddUObject(this, &UDianaHackComponent::HandleEnemyHackCompleted);
	PuzzleComponent->OnDecodeNodeVisited.RemoveAll(this);
	PuzzleComponent->OnDecodeNodeVisited.AddUObject(this, &UDianaHackComponent::HandleDecodeNodeVisited);

	ActiveHackTarget = TargetActor;
	ActivePuzzleComponent = PuzzleComponent;
	SetReplicatedEnemyHackTarget(TargetActor);
}

void UDianaHackComponent::Server_ProcessEnemyHackDirection_Implementation(EHackDirection Direction)
{
	// 방향 입력은 서버가 보관한 active puzzle에만 적용
	if (ActivePuzzleComponent)
	{
		ActivePuzzleComponent->ProcessDirectionInput(Direction);
	}
}

void UDianaHackComponent::Server_CancelEnemyHack_Implementation(AActor* ExpectedTarget)
{
	// reticle 이탈 cancel은 target이 바뀐 뒤 늦게 도착하면 새 hack session을 닫지 않음
	if (ExpectedTarget && ExpectedTarget != ActiveHackTarget)
	{
		return;
	}

	ClearActiveEnemyHack(true);
}

void UDianaHackComponent::HandleEnemyHackCompleted()
{
	// 완료 콜백은 현재 target/puzzle 일치 시에만 damage와 보상을 적용
	AActor* TargetActor = ActiveHackTarget;
	UEnemyHackingPuzzleComponent* PuzzleComponent = ActivePuzzleComponent;
	if (!TargetActor || !PuzzleComponent || PuzzleComponent->GetOwner() != TargetActor)
	{
		ClearActiveEnemyHack(false);
		return;
	}

	const float HackDamage = PuzzleComponent->GetProjectedDamage();
	const float HackDurationSeconds = PuzzleComponent->GetResolvedHackDurationSeconds();
	const bool bHackSucceeded = IPragmataHackable::Execute_ApplySuccessfulHack(TargetActor, GetOwner(), HackDamage, HackDurationSeconds);
	if (!bHackSucceeded)
	{
		ClearActiveEnemyHack(false);
		return;
	}

	ADianaOperatorPawn* DianaPawn = GetOwner<ADianaOperatorPawn>();
	if (!DianaPawn)
	{
		ClearActiveEnemyHack(false);
		return;
	}

	UOverDriveComponent* OverDriveComponent = DianaPawn->GetOverDriveComponent();
	if (!OverDriveComponent)
	{
		ClearActiveEnemyHack(false);
		return;
	}

	OverDriveComponent->AddOverDrive(OverDrivePerHack);
	DianaPawn->GetVoiceComponent()->PlayVoiceAction(EDianaVoiceAction::Hacking);
	DianaPawn->NotifyEnemyHackSucceeded();
	ScheduleCompletedHackHudClear(TargetActor);
	ClearActiveEnemyHack(false, false);
}

void UDianaHackComponent::HandleDecodeNodeVisited()
{
	// Decode 통과는 서버가 등록한 Hugh stat에만 즉시 무기 피해 보너스를 부여
	APragmataGameState* GameState = GetWorld() ? GetWorld()->GetGameState<APragmataGameState>() : nullptr;
	AHughCharacter* HughCharacter = GameState ? GameState->GetHughCharacter() : nullptr;
	UPragmataPlayerStatsComponent* PlayerStatsComponent = HughCharacter ? HughCharacter->GetPlayerStatsComponent() : nullptr;
	if (PlayerStatsComponent)
	{
		PlayerStatsComponent->ApplyDecodeWeaponDamageBonus();
	}
}

void UDianaHackComponent::ClearActiveEnemyHack(bool bCancelPuzzle, bool bClearReplicatedTarget)
{
	if (bClearReplicatedTarget)
	{
		CancelCompletedHackHudClearTimer();
	}

	if (ActivePuzzleComponent)
	{
		ActivePuzzleComponent->OnHackCompleted.RemoveAll(this);
		ActivePuzzleComponent->OnDecodeNodeVisited.RemoveAll(this);
		if (bCancelPuzzle)
		{
			ActivePuzzleComponent->CancelHack();
		}
	}

	ActiveHackTarget = nullptr;
	ActivePuzzleComponent = nullptr;
	if (bClearReplicatedTarget)
	{
		SetReplicatedEnemyHackTarget(nullptr);
	}
}

void UDianaHackComponent::ScheduleCompletedHackHudClear(AActor* CompletedTarget)
{
	// 완료 UI 애니메이션이 재생될 시간을 준 뒤 GameState target을 정리
	CancelCompletedHackHudClearTimer();
	PendingCompletedHackTarget = CompletedTarget;

	UWorld* World = GetWorld();
	if (!World)
	{
		ClearCompletedHackHudTarget();
		return;
	}

	World->GetTimerManager().SetTimer(
		CompletedHackHudClearTimerHandle,
		this,
		&ThisClass::ClearCompletedHackHudTarget,
		CompletedHackHudHoldSeconds,
		false
	);
}

void UDianaHackComponent::ClearCompletedHackHudTarget()
{
	// 늦은 완료 timer가 새 hack session의 HUD target을 닫지 않게 target 일치 확인
	AActor* CompletedTarget = PendingCompletedHackTarget;
	PendingCompletedHackTarget = nullptr;

	UWorld* World = GetWorld();
	APragmataGameState* GameState = World ? World->GetGameState<APragmataGameState>() : nullptr;
	if (GameState && GameState->HasAuthority() && GameState->GetActiveEnemyHackTarget() == CompletedTarget)
	{
		GameState->SetActiveEnemyHackTarget(nullptr);
	}
}

void UDianaHackComponent::CancelCompletedHackHudClearTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CompletedHackHudClearTimerHandle);
	}
	PendingCompletedHackTarget = nullptr;
}

void UDianaHackComponent::SetReplicatedEnemyHackTarget(AActor* TargetActor) const
{
	UWorld* World = GetWorld();
	APragmataGameState* GameState = World ? World->GetGameState<APragmataGameState>() : nullptr;
	if (GameState && GameState->HasAuthority())
	{
		GameState->SetActiveEnemyHackTarget(TargetActor);
	}
}

bool UDianaHackComponent::ValidateHackTarget(AActor* TargetActor, FVector& OutCameraOrigin, FVector& OutCameraDirection) const
{
	// Diana aim과 Hugh 거리 기준을 서버가 함께 검증
	const UWorld* World = GetWorld();
	const APragmataGameState* GameState = World ? World->GetGameState<APragmataGameState>() : nullptr;
	if (!GameState || GameState->AreDianaActionsLocked())
	{
		return false;
	}

	const ADianaOperatorPawn* DianaPawn = GetOwner<ADianaOperatorPawn>();
	if (!DianaPawn)
	{
		return false;
	}

	const APragmataPlayerController* PlayerController = Cast<APragmataPlayerController>(DianaPawn->GetController());
	const APragmataPlayerState* PlayerState = PlayerController ? PlayerController->GetPlayerState<APragmataPlayerState>() : nullptr;
	if (!PlayerState || PlayerState->GetAssignedRole() != EPragmataPlayerRole::Diana)
	{
		return false;
	}

	if (!DianaPawn->GetIsAiming())
	{
		return false;
	}

	// 서버가 Diana 시점 intent를 재계산
	OutCameraOrigin = DianaPawn->GetAuthoritativeCameraOrigin();
	OutCameraDirection = DianaPawn->GetAuthoritativeCameraDirection().GetSafeNormal();
	if (OutCameraDirection.IsNearlyZero())
	{
		return false;
	}

	const AActor* HughActor = GameState->GetHughCharacter();
	if (!HughActor || FVector::Dist(OutCameraOrigin, HughActor->GetActorLocation()) > MaxCameraDistanceFromHugh)
	{
		return false;
	}

	if (!TargetActor || !GetOwner() || !TargetActor->GetClass()->ImplementsInterface(UPragmataHackable::StaticClass()))
	{
		return false;
	}

	if (!IPragmataHackable::Execute_CanBeHacked(TargetActor, GetOwner()))
	{
		return false;
	}

	if (FVector::Dist(OutCameraOrigin, TargetActor->GetActorLocation()) > MaxHackDistance)
	{
		return false;
	}

	const FVector ToTarget = (TargetActor->GetActorLocation() - OutCameraOrigin).GetSafeNormal();
	if (FVector::DotProduct(OutCameraDirection, ToTarget) < 0.25f)
	{
		return false;
	}

	return HasLineOfSightToTarget(TargetActor, OutCameraOrigin);
}

bool UDianaHackComponent::HasLineOfSightToTarget(AActor* TargetActor, FVector CameraOrigin) const
{
	// camera origin에서 target까지 visibility trace가 막히면 서버 hack을 취소
	UWorld* World = GetWorld();
	if (!World || !TargetActor)
	{
		return false;
	}

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DianaHackLineOfSight), false, GetOwner());
	const bool bHit = World->LineTraceSingleByChannel(Hit, CameraOrigin, TargetActor->GetActorLocation(), ECC_Visibility, Params);

	return !bHit || PragmataTraceUtils::IsHitActorWithinCandidate(Hit, TargetActor);
}
