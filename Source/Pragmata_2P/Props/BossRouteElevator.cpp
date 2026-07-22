#include "Props/BossRouteElevator.h"

#include "Components/PragmataHealthComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Enemy/MonsterBase.h"
#include "Net/UnrealNetwork.h"
#include "Props/PuzzleInteractable.h"

ABossRouteElevator::ABossRouteElevator()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bReplicates = true;
	SetReplicateMovement(true);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	ElevatorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ElevatorMesh"));
	ElevatorMesh->SetupAttachment(SceneRoot);
}

void ABossRouteElevator::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		CollectAttachedPuzzleActors();
		ApplyRepeatHackSettingsToControlledPuzzleActors();
		BindControlledPuzzleActors();

		const bool bInitialPuzzleAvailable = bStartPuzzleAvailable && Stages.Num() > 0;
		TargetLocation = GetActorLocation();
		bPuzzleAvailable = bInitialPuzzleAvailable;
		ElevatorState = bInitialPuzzleAvailable
			? EBossRouteElevatorState::WaitingForPuzzle
			: EBossRouteElevatorState::Completed;
		ApplyPuzzleAvailabilityToControlledActors();
	}

	SetActorTickEnabled(ElevatorState == EBossRouteElevatorState::Moving);
	OnPuzzleAvailabilityChanged(bPuzzleAvailable);
	OnElevatorStateChanged(ElevatorState);
}

void ABossRouteElevator::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// 엘리베이터 실제 이동은 서버가 계산하고 movement replication으로 client에 전달
	if (!HasAuthority() || ElevatorState != EBossRouteElevatorState::Moving)
	{
		SetActorTickEnabled(false);
		return;
	}

	MoveElapsedTime += DeltaSeconds;
	const float Alpha = ActiveMoveDuration <= KINDA_SMALL_NUMBER ? 1.f : FMath::Clamp(MoveElapsedTime / ActiveMoveDuration, 0.f, 1.f);
	SetActorLocation(FMath::Lerp(MoveStartLocation, MoveTargetLocation, Alpha));

	if (Alpha >= 1.f)
	{
		FinishCurrentStageMove();
	}
}

void ABossRouteElevator::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABossRouteElevator, ElevatorState);
	DOREPLIFETIME(ABossRouteElevator, CurrentStageIndex);
	DOREPLIFETIME(ABossRouteElevator, TargetLocation);
	DOREPLIFETIME(ABossRouteElevator, bPuzzleAvailable);
}

void ABossRouteElevator::OnPuzzleTriggered_Implementation()
{
	// puzzle node 완료 신호는 다음 이동 stage를 여는 서버 권한 입력으로만 사용
	if (!HasAuthority() || ElevatorState != EBossRouteElevatorState::WaitingForPuzzle || !bPuzzleAvailable)
	{
		return;
	}

	StartCurrentStageMove();
}

void ABossRouteElevator::NotifyCombatCleared()
{
	// wave manager 또는 BP가 전투 종료를 확정하면 다음 puzzle 단계로 전환
	if (!HasAuthority() || ElevatorState != EBossRouteElevatorState::WaitingForCombat)
	{
		return;
	}

	OnCombatCleared(CurrentStageIndex);
	FinishCurrentStage();
}

void ABossRouteElevator::SetTerritoryCombatLocked_Implementation(bool bLocked)
{
	// Territory 전투 상태는 progression의 monster 감시 대신 다음 stage 진행 신호로만 사용
	if (!HasAuthority() || bTerritoryCombatLocked == bLocked)
	{
		return;
	}

	bTerritoryCombatLocked = bLocked;
	if (!bTerritoryCombatLocked)
	{
		NotifyCombatCleared();
	}
}

void ABossRouteElevator::OnRep_ElevatorState()
{
	ApplyReplicatedLocationState();
	OnElevatorStateChanged(ElevatorState);
}

void ABossRouteElevator::OnRep_CurrentStageIndex()
{
	OnElevatorStateChanged(ElevatorState);
}

void ABossRouteElevator::OnRep_TargetLocation()
{
	ApplyReplicatedLocationState();
}

void ABossRouteElevator::OnRep_PuzzleAvailable()
{
	OnPuzzleAvailabilityChanged(bPuzzleAvailable);
}

void ABossRouteElevator::HandleControlledPuzzleActivated(AActor* Interactor)
{
	// child puzzle가 target actor 설정 없이 완료돼도 elevator progression으로 연결
	(void)Interactor;
	OnPuzzleTriggered_Implementation();
}

void ABossRouteElevator::HandleRequiredEnemyDestroyed(AActor* DestroyedActor)
{
	// stage enemy가 제거될 때마다 남은 필수 enemy 수를 다시 계산
	if (!HasAuthority() || ElevatorState != EBossRouteElevatorState::WaitingForCombat)
	{
		return;
	}

	ActiveRequiredEnemies.RemoveAll(
		[DestroyedActor](const TObjectPtr<AMonsterBase>& EnemyActor)
		{
			return EnemyActor.Get() == DestroyedActor;
		}
	);
	CheckCombatClearCondition();
}

void ABossRouteElevator::HandleRequiredEnemyDeath()
{
	// 사망 판정 시점에 전투 종료 조건을 확인해 Destroy 지연에 묶이지 않게 함
	if (!HasAuthority() || ElevatorState != EBossRouteElevatorState::WaitingForCombat)
	{
		return;
	}

	CheckCombatClearCondition();
}

void ABossRouteElevator::SetElevatorState(EBossRouteElevatorState NewState)
{
	if (ElevatorState == NewState)
	{
		return;
	}

	ElevatorState = NewState;
	OnElevatorStateChanged(ElevatorState);
}

void ABossRouteElevator::SetPuzzleAvailable(bool bNewAvailable)
{
	if (bPuzzleAvailable == bNewAvailable)
	{
		return;
	}

	bPuzzleAvailable = bNewAvailable;
	OnPuzzleAvailabilityChanged(bPuzzleAvailable);
	ApplyPuzzleAvailabilityToControlledActors();
}

void ABossRouteElevator::StartCurrentStageMove()
{
	if (!Stages.IsValidIndex(CurrentStageIndex))
	{
		SetPuzzleAvailable(false);
		SetElevatorState(EBossRouteElevatorState::Completed);
		OnElevatorCompleted();
		ForceNetUpdate();
		return;
	}

	const FBossRouteElevatorStage& Stage = Stages[CurrentStageIndex];
	MoveStartLocation = GetActorLocation();
	MoveTargetLocation = MoveStartLocation + GetStageMoveOffset(Stage);
	TargetLocation = MoveTargetLocation;
	MoveElapsedTime = 0.f;
	ActiveMoveDuration = Stage.MoveDuration;

	SetPuzzleAvailable(false);
	SetElevatorState(EBossRouteElevatorState::Moving);
	OnStageMovementStarted(CurrentStageIndex);
	ForceNetUpdate();

	if (ActiveMoveDuration <= KINDA_SMALL_NUMBER || MoveStartLocation.Equals(MoveTargetLocation))
	{
		SetActorLocation(MoveTargetLocation);
		FinishCurrentStageMove();
		return;
	}

	SetActorTickEnabled(true);
}

void ABossRouteElevator::FinishCurrentStageMove()
{
	SetActorLocation(MoveTargetLocation);
	SetActorTickEnabled(false);
	OnStageMovementFinished(CurrentStageIndex);

	if (Stages.IsValidIndex(CurrentStageIndex) && Stages[CurrentStageIndex].bRequiresCombatClear)
	{
		SetElevatorState(EBossRouteElevatorState::WaitingForCombat);
		StartStageCombat();
		return;
	}

	FinishCurrentStage();
}

void ABossRouteElevator::FinishCurrentStage()
{
	++CurrentStageIndex;

	if (!Stages.IsValidIndex(CurrentStageIndex))
	{
		SetPuzzleAvailable(false);
		SetElevatorState(EBossRouteElevatorState::Completed);
		OnElevatorCompleted();
		ForceNetUpdate();
		return;
	}

	ResetControlledPuzzleActorsForNextStage();
	SetElevatorState(EBossRouteElevatorState::WaitingForPuzzle);
	SetPuzzleAvailable(true);
	ForceNetUpdate();
}

void ABossRouteElevator::StartStageCombat()
{
	// stage 진입 연출만 유지하고 monster 처치 판정은 Territory가 담당
	OnCombatStarted(CurrentStageIndex);
	ForceNetUpdate();
}

void ABossRouteElevator::ClearStageEnemyBindings()
{
	if (!HasAuthority())
	{
		return;
	}

	for (const TObjectPtr<AMonsterBase>& EnemyActorPtr : ActiveRequiredEnemies)
	{
		AMonsterBase* EnemyActor = EnemyActorPtr.Get();
		if (IsValid(EnemyActor))
		{
			EnemyActor->OnDestroyed.RemoveDynamic(this, &ABossRouteElevator::HandleRequiredEnemyDestroyed);

			if (UPragmataHealthComponent* HealthComponent = EnemyActor->GetHealthComponent())
			{
				HealthComponent->OnDeath.RemoveDynamic(this, &ABossRouteElevator::HandleRequiredEnemyDeath);
			}
		}
	}

	ActiveRequiredEnemies.Reset();
}

void ABossRouteElevator::CheckCombatClearCondition()
{
	// 이미 제거된 actor를 제외하고 남은 필수 enemy가 없으면 전투 완료
	if (!HasAuthority() || ElevatorState != EBossRouteElevatorState::WaitingForCombat)
	{
		return;
	}

	for (int32 EnemyIndex = ActiveRequiredEnemies.Num() - 1; EnemyIndex >= 0; --EnemyIndex)
	{
		AMonsterBase* EnemyActor = ActiveRequiredEnemies[EnemyIndex].Get();
		if (IsValid(EnemyActor) && !EnemyActor->IsActorBeingDestroyed() && !EnemyActor->IsDead())
		{
			continue;
		}

		if (IsValid(EnemyActor))
		{
			EnemyActor->OnDestroyed.RemoveDynamic(this, &ABossRouteElevator::HandleRequiredEnemyDestroyed);

			if (UPragmataHealthComponent* HealthComponent = EnemyActor->GetHealthComponent())
			{
				HealthComponent->OnDeath.RemoveDynamic(this, &ABossRouteElevator::HandleRequiredEnemyDeath);
			}
		}

		ActiveRequiredEnemies.RemoveAtSwap(EnemyIndex);
	}

	if (ActiveRequiredEnemies.Num() == 0)
	{
		NotifyCombatCleared();
	}
}

void ABossRouteElevator::CollectRequiredCombatMonsters(AActor* CombatActor)
{
	// stage 설정에는 monster 자신이나 monster를 담은 박스 actor를 둘 다 허용
	if (!IsValid(CombatActor) || CombatActor->IsActorBeingDestroyed())
	{
		return;
	}

	RegisterRequiredCombatMonster(Cast<AMonsterBase>(CombatActor));

	TArray<AActor*> AttachedActors;
	CombatActor->GetAttachedActors(AttachedActors, true, true);

	for (AActor* AttachedActor : AttachedActors)
	{
		RegisterRequiredCombatMonster(Cast<AMonsterBase>(AttachedActor));
	}
}

void ABossRouteElevator::RegisterRequiredCombatMonster(AMonsterBase* MonsterActor)
{
	// 이미 죽은 monster는 현재 wave의 클리어 대기 대상에서 제외
	if (!IsValid(MonsterActor) || MonsterActor->IsActorBeingDestroyed() || MonsterActor->IsDead())
	{
		return;
	}

	const bool bAlreadyRegistered = ActiveRequiredEnemies.ContainsByPredicate(
		[MonsterActor](const TObjectPtr<AMonsterBase>& EnemyActor)
		{
			return EnemyActor.Get() == MonsterActor;
		}
	);
	if (bAlreadyRegistered)
	{
		return;
	}

	ActiveRequiredEnemies.Add(MonsterActor);
	MonsterActor->OnDestroyed.AddUniqueDynamic(this, &ABossRouteElevator::HandleRequiredEnemyDestroyed);

	if (UPragmataHealthComponent* HealthComponent = MonsterActor->GetHealthComponent())
	{
		HealthComponent->OnDeath.AddUniqueDynamic(this, &ABossRouteElevator::HandleRequiredEnemyDeath);
	}
}

FVector ABossRouteElevator::GetStageMoveOffset(const FBossRouteElevatorStage& Stage) const
{
	const FVector SafeDirection = MoveDirection.IsNearlyZero() ? FVector::UpVector : MoveDirection.GetSafeNormal();
	return SafeDirection * Stage.MoveDistance;
}

void ABossRouteElevator::ApplyReplicatedLocationState()
{
	// 상태 복제 지연 중에도 이동 종료 위치를 받은 client는 최종 위치를 맞춤
	if (!HasAuthority()
		&& (ElevatorState == EBossRouteElevatorState::WaitingForPuzzle
			|| ElevatorState == EBossRouteElevatorState::WaitingForCombat
			|| ElevatorState == EBossRouteElevatorState::Completed))
	{
		SetActorLocation(TargetLocation);
	}
}

void ABossRouteElevator::CollectAttachedPuzzleActors()
{
	// BP child actor로 붙인 puzzle node를 별도 배열 세팅 없이 progression 대상에 포함
	if (!HasAuthority())
	{
		return;
	}

	TArray<AActor*> AttachedActors;
	GetAttachedActors(AttachedActors, true, true);

	for (AActor* AttachedActor : AttachedActors)
	{
		if (APuzzleInteractable* PuzzleActor = Cast<APuzzleInteractable>(AttachedActor))
		{
			ControlledPuzzleActors.AddUnique(PuzzleActor);
		}
	}
}

void ABossRouteElevator::BindControlledPuzzleActors()
{
	// puzzle 완료 delegate를 elevator stage 시작 신호로 연결
	if (!HasAuthority())
	{
		return;
	}

	for (APuzzleInteractable* PuzzleActor : ControlledPuzzleActors)
	{
		if (IsValid(PuzzleActor))
		{
			PuzzleActor->OnInteractionActivated.AddUniqueDynamic(this, &ABossRouteElevator::HandleControlledPuzzleActivated);
		}
	}
}

// 월드 인스턴스 설정으로 child actor template의 고정값을 덮어쓰고 client에도 같은 정책을 복제
void ABossRouteElevator::ApplyRepeatHackSettingsToControlledPuzzleActors() const
{
	if (!HasAuthority())
	{
		return;
	}

	for (APuzzleInteractable* PuzzleActor : ControlledPuzzleActors)
	{
		if (IsValid(PuzzleActor))
		{
			PuzzleActor->ConfigureRepeatHacking(bAllowRepeatedPuzzleHacking, MaxPuzzleHackCount);
		}
	}
}

void ABossRouteElevator::ResetControlledPuzzleActorsForNextStage() const
{
	// 다음 stage 해킹을 다시 요구하기 위해 puzzle node를 미해결 animation 상태로 되돌림
	if (!HasAuthority())
	{
		return;
	}

	for (APuzzleInteractable* PuzzleActor : ControlledPuzzleActors)
	{
		if (IsValid(PuzzleActor))
		{
			PuzzleActor->ResetPuzzleForNextAttempt();
		}
	}
}

void ABossRouteElevator::ApplyPuzzleAvailabilityToControlledActors() const
{
	if (!HasAuthority())
	{
		return;
	}

	for (APuzzleInteractable* PuzzleActor : ControlledPuzzleActors)
	{
		if (IsValid(PuzzleActor))
		{
			PuzzleActor->SetPuzzleInteractionEnabled(bPuzzleAvailable);
		}
	}
}
