#include "Enemy/MonsterTerritoryActor.h"

#include "AI/MonsterAIController.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PragmataFactionComponent.h"
#include "Components/PragmataHealthComponent.h"
#include "DefaultLevelSequenceInstanceData.h"
#include "DrawDebugHelpers.h"
#include "Enemy/MonsterBase.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Game/PragmataGameState.h"
#include "Interfaces/TerritoryCombatLockable.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "MovieSceneSequencePlaybackSettings.h"
#include "TimerManager.h"

AMonsterTerritoryActor::AMonsterTerritoryActor()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	SetRootComponent(TriggerBox);

	TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerBox->SetCollisionObjectType(ECC_WorldDynamic);
	TriggerBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	TriggerBox->SetGenerateOverlapEvents(true);

	// TriggerBox 디버깅용
	TriggerBox->ShapeColor = FColor::Red;
}

void AMonsterTerritoryActor::BeginPlay()
{
	Super::BeginPlay();

	bTerritoryTriggerActivated = !bRequireMapGimmickActivation;

	TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &AMonsterTerritoryActor::HandleBeginOverlap);
	TriggerBox->OnComponentEndOverlap.AddDynamic(this, &AMonsterTerritoryActor::HandleEndOverlap);

	for (AMonsterBase* Monster : PlacedMonsters)
	{
		RegisterMonster(Monster);
		const FMonsterTerritoryPlacedMonsterInfo* PlacedMonsterOverrideInfo = FindPlacedMonsterOverrideInfo(Monster);
		UAnimMontage* AppearanceMontageOverride = PlacedMonsterOverrideInfo
			? PlacedMonsterOverrideInfo->AppearanceMontageOverride.Get()
			: nullptr;
		const bool bDelayForFirstEntrySequence = IsFirstEntrySequenceMonster(Monster);
		if (ShouldDelayPlacedMonsterUntilTerritoryEntry(
			Monster,
			AppearanceMontageOverride,
			bDelayForFirstEntrySequence))
		{
			SuspendMonsterUntilTerritoryTrigger(Monster);
		}
	}

	for (const FMonsterTerritoryPlacedMonsterInfo& PlacedMonsterInfo : PlacedMonsterOverrides)
	{
		AMonsterBase* Monster = PlacedMonsterInfo.Monster;
		const bool bAlreadyRegistered = PlacedMonsters.ContainsByPredicate(
			[Monster](const TObjectPtr<AMonsterBase>& PlacedMonster)
			{
				return PlacedMonster == Monster;
			});
		if (!IsValid(Monster) || bAlreadyRegistered)
		{
			continue;
		}

		RegisterMonster(Monster);
		if (ShouldDelayPlacedMonsterUntilTerritoryEntry(
			Monster,
			PlacedMonsterInfo.AppearanceMontageOverride,
			IsFirstEntrySequenceMonster(Monster)))
		{
			SuspendMonsterUntilTerritoryTrigger(Monster);
		}
	}

	for (AMonsterBase* Monster : FirstEntrySequenceMonsters)
	{
		if (!IsValid(Monster) || IsPlacedMonsterConfigured(Monster))
		{
			continue;
		}

		RegisterMonster(Monster);
		SuspendMonsterUntilTerritoryTrigger(Monster);
	}

	RemoveInvalidMonsters();
}

void AMonsterTerritoryActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// ReturnHome 중인 monster의 안전한 재진입 상태를 서버에서 갱신
	if (HasAuthority())
	{
		ResolvePendingControllerTerritories();
		UpdateReturningMonsters();
	}

//#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
//	if (TriggerBox)
//	{
//		DrawDebugBox(
//			GetWorld(),
//			TriggerBox->GetComponentLocation(),
//			TriggerBox->GetScaledBoxExtent(),
//			TriggerBox->GetComponentQuat(),
//			FColor::Red,
//			false,
//			0.f,
//			0,
//			3.f
//		);
//	}
//#endif
}

// Territory에 플레이어 출입할 때
void AMonsterTerritoryActor::HandleBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult
)
{
	if (!HasAuthority())
	{
		return;
	}

	// 플레이어일 때 처리
	if (IsTargetActor(OtherActor))
	{
		CurrentTarget = OtherActor;
		OnPlayerEnteredTerritory.Broadcast(OtherActor);
		TriggerFirstPlayerEntry(OtherActor);
		for (AMonsterBase* Monster : ActiveMonsters)
		{
			if (IsValid(Monster))
			{
				Monster->ForgetPerceivedActor(OtherActor);
			}
		}
		return;
	}

	// 몬스터일 때 처리
	if (AMonsterBase* Monster = Cast<AMonsterBase>(OtherActor))
	{
		// 자세 animation을 따라 움직이는 머리·몸통·다리 collision은 Territory 출입 판정에서 제외
		if (OtherComp != Monster->GetCapsuleComponent())
		{
			return;
		}

		// Territory 매핑된 몬스터 아니면 return;
		if (!ActiveMonsters.Contains(Monster))
		{
			return;
		}
		
		const TWeakObjectPtr<AMonsterBase> MonsterPtr(Monster);
		if (AMonsterAIController* AIController = Cast<AMonsterAIController>(Monster->GetController()))
		{
			// 이탈 기록이 있으면 경계 overlap만으로 전투 분기를 다시 열지 않음
			if (!ReturningMonsters.Contains(MonsterPtr) || IsMonsterInsideReentryBounds(Monster))
			{
				AIController->SetMonsterTerritory(true);
				ReturningMonsters.Remove(MonsterPtr);
			}
		}
		return;
	}
}

// map gimmick이 봉쇄·연출 준비를 끝낸 뒤 Territory 전투 시작을 허용
void AMonsterTerritoryActor::ActivateTerritoryTrigger()
{
	if (!HasAuthority())
	{
		return;
	}

	bTerritoryTriggerActivated = true;

	if (!HasTriggeringPlayerInsideTerritory())
	{
		CurrentTarget = FindTriggeringPlayerInsideTerritory();
	}

	if (HasTriggeringPlayerInsideTerritory())
	{
		TriggerFirstPlayerEntry(CurrentTarget);
	}
}

// Territory 첫 player 진입을 map event와 설정된 spawn·sequence 순서로 한 번만 실행
void AMonsterTerritoryActor::TriggerFirstPlayerEntry(AActor* PlayerActor)
{
	if (!HasAuthority()
		|| bFirstPlayerEntryTriggered
		|| !bTerritoryTriggerActivated
		|| !IsTriggeringPlayer(PlayerActor))
	{
		return;
	}

	bFirstPlayerEntryTriggered = true;
	OnFirstPlayerEntered.Broadcast(PlayerActor);
	LockCombatTargets();

	StartFirstEntrySequenceOrGameplay();
}

// 첫 진입 확정 후 sequence 지연 시간이 끝날 때까지 spawn과 배치 monster 시작을 보류
void AMonsterTerritoryActor::StartFirstEntrySequenceOrGameplay()
{
	if (!HasAuthority())
	{
		return;
	}

	if (!FirstEntrySequence)
	{
		StartFirstEntryGameplay();
		return;
	}

	const float SequenceDelay = FMath::Max(0.f, FirstEntrySequenceDelay);
	if (SequenceDelay > 0.f)
	{
		GetWorldTimerManager().SetTimer(
			FirstEntrySequenceDelayTimerHandle,
			this,
			&ThisClass::HandleFirstEntrySequenceDelayElapsed,
			SequenceDelay,
			false);
		return;
	}

	HandleFirstEntrySequenceDelayElapsed();
}

// delay가 끝난 뒤에도 sequence를 monster 시작보다 먼저 재생
void AMonsterTerritoryActor::HandleFirstEntrySequenceDelayElapsed()
{
	if (!HasAuthority() || !FirstEntrySequence)
	{
		return;
	}

	MulticastPlayFirstEntrySequence(FirstEntrySequence);
	// player 생성 실패로 OnFinished를 받을 수 없으면 전투 진행이 막히지 않게 즉시 fallback
	if (HasAuthority() && !FirstEntrySequencePlayer)
	{
		StartFirstEntryGameplay();
	}
}

// Player 진영으로 바뀐 monster가 첫 진입 event를 오발하지 않도록 실제 player-controlled Pawn만 허용
bool AMonsterTerritoryActor::IsTriggeringPlayer(AActor* Actor) const
{
	const APawn* Pawn = Cast<APawn>(Actor);
	return IsTargetActor(Actor) && Pawn && Pawn->IsPlayerControlled();
}

AActor* AMonsterTerritoryActor::FindTriggeringPlayerInsideTerritory() const
{
	if (!TriggerBox)
	{
		return nullptr;
	}

	TArray<AActor*> OverlappingActors;
	TriggerBox->GetOverlappingActors(OverlappingActors);
	for (AActor* Actor : OverlappingActors)
	{
		if (IsTriggeringPlayer(Actor))
		{
			return Actor;
		}
	}
	return nullptr;
}

bool AMonsterTerritoryActor::HasTriggeringPlayerInsideTerritory() const
{
	return IsTriggeringPlayer(CurrentTarget) && IsActorInsideTerritory(CurrentTarget);
}

// first entry sequence 이후 실제 monster 시작과 runtime spawn을 한 곳에서 처리
void AMonsterTerritoryActor::StartFirstEntryGameplay()
{
	if (!HasAuthority())
	{
		return;
	}

	StartPlacedMonstersForEncounter();
	SpawnRuntimeMonsters();
	bEncounterMonstersPrepared = true;
	CheckTerritoryCombatClear();
	CheckTerritoryGameClear();
}

// 외부 기믹, sequence 지정, appearance로 지연된 배치 monster만 첫 전투 시작 대상으로 처리
void AMonsterTerritoryActor::StartPlacedMonstersForEncounter()
{
	TSet<TWeakObjectPtr<AMonsterBase>> StartedMonsters;
	for (AMonsterBase* Monster : PlacedMonsters)
	{
		if (!IsValid(Monster))
		{
			continue;
		}

		const FMonsterTerritoryPlacedMonsterInfo* PlacedMonsterOverrideInfo = FindPlacedMonsterOverrideInfo(Monster);
		UAnimMontage* AppearanceMontageOverride = PlacedMonsterOverrideInfo
			? PlacedMonsterOverrideInfo->AppearanceMontageOverride.Get()
			: nullptr;
		const bool bDelayForFirstEntrySequence = IsFirstEntrySequenceMonster(Monster);
		if (!ShouldDelayPlacedMonsterUntilTerritoryEntry(
			Monster,
			AppearanceMontageOverride,
			bDelayForFirstEntrySequence))
		{
			continue;
		}

		BeginMonsterEncounter(Monster, AppearanceMontageOverride);
		StartedMonsters.Add(TWeakObjectPtr<AMonsterBase>(Monster));
	}

	for (const FMonsterTerritoryPlacedMonsterInfo& PlacedMonsterInfo : PlacedMonsterOverrides)
	{
		AMonsterBase* Monster = PlacedMonsterInfo.Monster;
		const TWeakObjectPtr<AMonsterBase> MonsterPtr(Monster);
		if (!IsValid(Monster) || StartedMonsters.Contains(MonsterPtr))
		{
			continue;
		}

		if (!ShouldDelayPlacedMonsterUntilTerritoryEntry(
			Monster,
			PlacedMonsterInfo.AppearanceMontageOverride,
			IsFirstEntrySequenceMonster(Monster)))
		{
			continue;
		}

		BeginMonsterEncounter(Monster, PlacedMonsterInfo.AppearanceMontageOverride);
		StartedMonsters.Add(MonsterPtr);
	}

	for (AMonsterBase* Monster : FirstEntrySequenceMonsters)
	{
		const TWeakObjectPtr<AMonsterBase> MonsterPtr(Monster);
		if (!IsValid(Monster) || StartedMonsters.Contains(MonsterPtr))
		{
			continue;
		}

		BeginMonsterEncounter(Monster);
		StartedMonsters.Add(MonsterPtr);
	}
}

// appearance가 있으면 연출 종료 후, 없으면 즉시 AI를 시작
void AMonsterTerritoryActor::BeginMonsterEncounter(
	AMonsterBase* Monster,
	UAnimMontage* AppearanceMontageOverride,
	bool bUseFallingAppearance,
	FName FallingEndSectionName,
	float FallingEndTransitionDistance)
{
	if (!HasAuthority() || !IsValid(Monster) || !ActiveMonsters.Contains(Monster))
	{
		return;
	}

	Monster->SetAutoRunAIOnPossess(false);
	Monster->OnSpawnAppearanceFinished.RemoveAll(this);
	if (Monster->HasSpawnAppearanceMontage(AppearanceMontageOverride))
	{
		Monster->OnSpawnAppearanceFinished.AddUObject(
			this,
			&ThisClass::HandleMonsterSpawnAppearanceFinished);
		if (Monster->TryPlaySpawnAppearance(
			AppearanceMontageOverride,
			bUseFallingAppearance,
			FallingEndSectionName,
			FallingEndTransitionDistance))
		{
			PendingAppearanceStartMonsters.Add(TWeakObjectPtr<AMonsterBase>(Monster));
			ScheduleSpawnAppearanceFallback(Monster, AppearanceMontageOverride, bUseFallingAppearance);
			return;
		}
		Monster->OnSpawnAppearanceFinished.RemoveAll(this);
	}

	StartMonsterAIWithCurrentTarget(Monster);
}

// 비하강 appearance 종료 delegate가 누락돼도 전투 시작이 대기 상태에 갇히지 않게 보정
void AMonsterTerritoryActor::ScheduleSpawnAppearanceFallback(
	AMonsterBase* Monster,
	UAnimMontage* AppearanceMontageOverride,
	bool bUseFallingAppearance)
{
	if (!HasAuthority() || !IsValid(Monster) || bUseFallingAppearance)
	{
		return;
	}

	const float PlayLength = Monster->GetSpawnAppearanceMontagePlayLength(AppearanceMontageOverride);
	if (PlayLength <= 0.f)
	{
		return;
	}

	FTimerHandle FallbackTimerHandle;
	FTimerDelegate FallbackDelegate;
	FallbackDelegate.BindUObject(
		this,
		&ThisClass::HandleSpawnAppearanceFallback,
		TWeakObjectPtr<AMonsterBase>(Monster));
	GetWorldTimerManager().SetTimer(FallbackTimerHandle, FallbackDelegate, PlayLength + 0.25f, false);
}

// appearance가 정상 종료되지 않은 경우 상태를 정리하고 Territory AI 시작 흐름을 계속 진행
void AMonsterTerritoryActor::HandleSpawnAppearanceFallback(TWeakObjectPtr<AMonsterBase> MonsterPtr)
{
	if (!HasAuthority() || !PendingAppearanceStartMonsters.Contains(MonsterPtr))
	{
		return;
	}

	AMonsterBase* Monster = MonsterPtr.Get();
	if (!IsValid(Monster))
	{
		PendingAppearanceStartMonsters.Remove(MonsterPtr);
		return;
	}

	if (!Monster->ForceFinishSpawnAppearance() && PendingAppearanceStartMonsters.Contains(MonsterPtr))
	{
		Monster->OnSpawnAppearanceFinished.RemoveAll(this);
		PendingAppearanceStartMonsters.Remove(MonsterPtr);
		StartMonsterAIWithCurrentTarget(Monster);
	}
}

// appearance 종료 시점에 Target과 BT를 연결해 바로 Chase로 진입 가능하게 함
void AMonsterTerritoryActor::HandleMonsterSpawnAppearanceFinished(AMonsterBase* Monster)
{
	if (!HasAuthority() || !IsValid(Monster))
	{
		return;
	}

	Monster->OnSpawnAppearanceFinished.RemoveAll(this);
	PendingAppearanceStartMonsters.Remove(TWeakObjectPtr<AMonsterBase>(Monster));
	StartMonsterAIWithCurrentTarget(Monster);
}

// Territory가 확정한 player를 blackboard target으로 넣고 BT를 시작
void AMonsterTerritoryActor::StartMonsterAIWithCurrentTarget(AMonsterBase* Monster)
{
	if (!HasAuthority() || !IsValid(Monster))
	{
		return;
	}

	Monster->SetAutoRunAIOnPossess(true);
	AMonsterAIController* AIController = Cast<AMonsterAIController>(Monster->GetController());
	if (AIController)
	{
		AIController->RunAI();
		AIController->SetMonsterTerritory(true);
	}
	if (AssignCurrentTargetToMonster(Monster))
	{
		PendingSpawnTargetMonsters.Remove(TWeakObjectPtr<AMonsterBase>(Monster));
		if (AIController)
		{
			AIController->RestartBehaviorAfterTargetAssigned();
		}
	}
	else if (HasTriggeringPlayerInsideTerritory())
	{
		PendingSpawnTargetMonsters.Add(TWeakObjectPtr<AMonsterBase>(Monster));
	}
}

// Territory 첫 전투 연출 전에는 배치 monster가 Patrol/Perception으로 먼저 움직이지 않게 정지
void AMonsterTerritoryActor::SuspendMonsterUntilTerritoryTrigger(AMonsterBase* Monster) const
{
	if (!HasAuthority() || !IsValid(Monster))
	{
		return;
	}

	Monster->SetAutoRunAIOnPossess(false);
	if (AMonsterAIController* AIController = Cast<AMonsterAIController>(Monster->GetController()))
	{
		AIController->StopAI();
	}
}

// 명시된 sequence 지연 또는 appearance로 첫 전투 시작 시점이 정해진 배치 monster는 먼저 BT를 돌리지 않음
bool AMonsterTerritoryActor::ShouldDelayPlacedMonsterUntilTerritoryEntry(
	AMonsterBase* Monster,
	UAnimMontage* AppearanceMontageOverride,
	bool bDelayForFirstEntrySequence) const
{
	return bRequireMapGimmickActivation
		|| bDelayForFirstEntrySequence
		|| (IsValid(Monster) && Monster->HasSpawnAppearanceMontage(AppearanceMontageOverride));
}

// Territory 인스턴스에서 배치 monster별 지연 시작 설정과 appearance override를 한 곳에서 찾음
const FMonsterTerritoryPlacedMonsterInfo* AMonsterTerritoryActor::FindPlacedMonsterOverrideInfo(AMonsterBase* Monster) const
{
	if (!IsValid(Monster))
	{
		return nullptr;
	}

	for (const FMonsterTerritoryPlacedMonsterInfo& PlacedMonsterInfo : PlacedMonsterOverrides)
	{
		if (PlacedMonsterInfo.Monster == Monster)
		{
			return &PlacedMonsterInfo;
		}
	}
	return nullptr;
}

bool AMonsterTerritoryActor::IsFirstEntrySequenceMonster(AMonsterBase* Monster) const
{
	return IsValid(Monster)
		&& FirstEntrySequenceMonsters.ContainsByPredicate(
			[Monster](const TObjectPtr<AMonsterBase>& FirstEntrySequenceMonster)
			{
				return FirstEntrySequenceMonster == Monster;
			});
}

bool AMonsterTerritoryActor::IsPlacedMonsterConfigured(AMonsterBase* Monster) const
{
	if (!IsValid(Monster))
	{
		return false;
	}

	if (PlacedMonsters.ContainsByPredicate(
		[Monster](const TObjectPtr<AMonsterBase>& PlacedMonster)
		{
			return PlacedMonster == Monster;
		}))
	{
		return true;
	}

	return FindPlacedMonsterOverrideInfo(Monster) != nullptr;
}

// RuntimeSpawns 배열이 설정된 Territory만 첫 진입 시 monster 생성
void AMonsterTerritoryActor::SpawnRuntimeMonsters()
{
	if (!HasAuthority() || bRuntimeSpawnsTriggered)
	{
		return;
	}
	bRuntimeSpawnsTriggered = true;

	for (const FMonsterTerritorySpawnInfo& SpawnInfo : RuntimeSpawns)
	{
		SpawnMonster(SpawnInfo);
	}

	RemoveInvalidMonsters();
}

void AMonsterTerritoryActor::MulticastPlayFirstEntrySequence_Implementation(ULevelSequence* Sequence)
{
	PlayFirstEntrySequence(Sequence);
}

// Territory에 sequence가 지정된 경우 peer별 local player로 재생
void AMonsterTerritoryActor::PlayFirstEntrySequence(ULevelSequence* Sequence)
{
	FirstEntrySequencePlayer = CreateTerritorySequencePlayer(
		Sequence,
		bUseTerritoryAsSequenceTransformOrigin);
	if (FirstEntrySequencePlayer)
	{
		if (HasAuthority())
		{
			FirstEntrySequencePlayer->OnFinished.AddDynamic(
				this,
				&ThisClass::HandleFirstEntrySequenceFinished);
		}
		FirstEntrySequencePlayer->Play();
	}
}

// first entry sequence가 종료 후 마지막 Transform 상태를 유지하도록 player를 생성
// Territory transform origin을 쓰면 같은 sequence를 Territory 기준 좌표로 평가
ULevelSequencePlayer* AMonsterTerritoryActor::CreateTerritorySequencePlayer(
	ULevelSequence* Sequence,
	bool bUseTransformOrigin)
{
	if (!Sequence || !GetWorld())
	{
		return nullptr;
	}

	ALevelSequenceActor* SequenceActor = nullptr;
	FMovieSceneSequencePlaybackSettings PlaybackSettings;
	PlaybackSettings.FinishCompletionStateOverride = EMovieSceneCompletionModeOverride::ForceKeepState;
	ULevelSequencePlayer* SequencePlayer = ULevelSequencePlayer::CreateLevelSequencePlayer(
		this,
		Sequence,
		PlaybackSettings,
		SequenceActor);
	if (SequenceActor && bUseTransformOrigin)
	{
		// peer마다 생성한 sequence actor가 같은 Territory transform을 absolute track 원점으로 사용
		SequenceActor->bOverrideInstanceData = true;
		if (UDefaultLevelSequenceInstanceData* InstanceData = Cast<UDefaultLevelSequenceInstanceData>(
			SequenceActor->DefaultInstanceData))
		{
			InstanceData->TransformOriginActor = this;
		}
	}

	return SequencePlayer;
}

// sequence 완료를 server 기준으로 확정한 뒤 지연된 runtime monster를 한 번만 생성
void AMonsterTerritoryActor::HandleFirstEntrySequenceFinished()
{
	if (HasAuthority())
	{
		StartFirstEntryGameplay();
	}
}

// Territory에서 플레이어가 나갈 때
void AMonsterTerritoryActor::HandleEndOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex
)
{
	if (!HasAuthority())
	{
		return;
	}

	// 플레이어일 때 처리
	if (IsTargetActor(OtherActor))
	{
		for (AMonsterBase* Monster : ActiveMonsters)
		{
			ClearMonsterTargetIfSame(Monster, OtherActor);
		}

		if (CurrentTarget == OtherActor)
		{
			CurrentTarget = nullptr;
		}
	}

	// 몬스터일 때 처리
	if (AMonsterBase* Monster = Cast<AMonsterBase>(OtherActor))
	{
		// 루트 Capsule이 실제로 나갈 때만 ReturnHome 상태로 전환
		if (OtherComp != Monster->GetCapsuleComponent())
		{
			return;
		}

		// Territory 매핑된 몬스터 아니면 return;
		if (!ActiveMonsters.Contains(Monster))
		{
			return;
		}

		if (AMonsterAIController* AIController = Cast<AMonsterAIController>(Monster->GetController()))
		{
			// 재진입 여유 거리 통과 전까지 ReturnHome 상태 유지
			ReturningMonsters.Add(TWeakObjectPtr<AMonsterBase>(Monster));
			AIController->SetMonsterTerritory(false);
		}
		return;
	}
}

// -- 몬스터 삭제&영역 삭제는 아직 미완성!
void AMonsterTerritoryActor::HandleMonsterDestroyed(AActor* DestroyedActor)
{
	AMonsterBase* DestroyedMonster = Cast<AMonsterBase>(DestroyedActor);
	if (DestroyedMonster)
	{
		DestroyedMonster->OnSpawnAppearanceFinished.RemoveAll(this);

		if (UPragmataHealthComponent* HealthComponent = DestroyedMonster->GetHealthComponent())
		{
			HealthComponent->OnDeath.RemoveDynamic(this, &AMonsterTerritoryActor::HandleMonsterDeath);
		}
	}

	ReturningMonsters.Remove(TWeakObjectPtr<AMonsterBase>(DestroyedMonster));
	PendingTerritoryControllerMonsters.Remove(TWeakObjectPtr<AMonsterBase>(DestroyedMonster));
	PendingSpawnTargetMonsters.Remove(TWeakObjectPtr<AMonsterBase>(DestroyedMonster));
	PendingAppearanceStartMonsters.Remove(TWeakObjectPtr<AMonsterBase>(DestroyedMonster));

	ActiveMonsters.RemoveAll(
		[DestroyedActor](AMonsterBase* Monster)
		{
			return !IsValid(Monster) || Monster == DestroyedActor;
		}
	);

	// actor 삭제 전에 전투 종료를 확정해 map gimmick이 잠긴 채 사라지지 않도록 보장
	CheckTerritoryCombatClear();
	CheckTerritoryGameClear();

	// 첫 진입 전에 배치 monster가 제거돼도 runtime spawn과 map event를 받을 Territory는 유지
	if (ActiveMonsters.Num() == 0
		&& bFirstPlayerEntryTriggered
		&& (bRuntimeSpawnsTriggered || RuntimeSpawns.IsEmpty()))
	{
		if (!bGameClearPresentationScheduled && !bGameClearRequested)
		{
			Destroy();
		}
	}
}

void AMonsterTerritoryActor::HandleMonsterDeath()
{
	// death animation의 lifespan을 기다리지 않고 마지막 monster 사망 시 즉시 봉쇄 해제
	CheckTerritoryCombatClear();
	CheckTerritoryGameClear();
}

void AMonsterTerritoryActor::RegisterMonster(AMonsterBase* Monster)
{
	if (!HasAuthority() || !IsValid(Monster))
	{
		return;
	}

	if (bRequireMapGimmickActivation && !bFirstPlayerEntryTriggered)
	{
		Monster->SetAutoRunAIOnPossess(false);
	}

	ActiveMonsters.AddUnique(Monster);
	bGameClearEncounterRegistered |= bTriggerGameClearWhenMonstersDefeated;
	Monster->OnDestroyed.AddDynamic(this, &AMonsterTerritoryActor::HandleMonsterDestroyed);

	if (UPragmataHealthComponent* HealthComponent = Monster->GetHealthComponent())
	{
		HealthComponent->OnDeath.AddUniqueDynamic(this, &AMonsterTerritoryActor::HandleMonsterDeath);
	}

	if (AMonsterAIController* AIController = Cast<AMonsterAIController>(Monster->GetController()))
	{
		AIController->SetTerritory(this);
	}
	else
	{
		// 배치 monster는 Territory BeginPlay 시점에 AIController가 아직 없을 수 있음
		PendingTerritoryControllerMonsters.Add(TWeakObjectPtr<AMonsterBase>(Monster));
	}
}

// 첫 전투 확정 시점에 등록한 map gimmick만 한 번 봉쇄
void AMonsterTerritoryActor::LockCombatTargets()
{
	if (!HasAuthority() || !bLockMapUntilMonstersCleared || bCombatLockActive)
	{
		return;
	}

	bCombatLockActive = true;
	bCombatLockReleased = false;

	for (AActor* TargetActor : CombatLockTargets)
	{
		if (IsValid(TargetActor)
			&& TargetActor->GetClass()->ImplementsInterface(UTerritoryCombatLockable::StaticClass()))
		{
			ITerritoryCombatLockable::Execute_SetTerritoryCombatLocked(TargetActor, true);
		}
	}
}

// 마지막 monster 처치 뒤 같은 target에 해제 신호를 한 번만 전달
void AMonsterTerritoryActor::UnlockCombatTargets()
{
	if (!HasAuthority() || !bCombatLockActive || bCombatLockReleased)
	{
		return;
	}

	bCombatLockReleased = true;
	bCombatLockActive = false;

	for (AActor* TargetActor : CombatLockTargets)
	{
		if (IsValid(TargetActor)
			&& TargetActor->GetClass()->ImplementsInterface(UTerritoryCombatLockable::StaticClass()))
		{
			ITerritoryCombatLockable::Execute_SetTerritoryCombatLocked(TargetActor, false);
		}
	}
}

// sequence와 runtime spawn 준비가 끝난 뒤 Territory 등록 monster만 전멸 판정
void AMonsterTerritoryActor::CheckTerritoryCombatClear()
{
	if (!HasAuthority()
		|| !bCombatLockActive
		|| !bFirstPlayerEntryTriggered
		|| !bEncounterMonstersPrepared)
	{
		return;
	}

	for (const AMonsterBase* Monster : ActiveMonsters)
	{
		if (IsValid(Monster) && !Monster->IsActorBeingDestroyed() && !Monster->IsDead())
		{
			return;
		}
	}

	// 배치·runtime spawn을 모두 준비한 뒤 살아 있는 monster가 없을 때만 봉쇄 해제
	UnlockCombatTargets();
}

// Combat Lock 사용 여부와 무관하게 opt-in boss Territory의 등록 monster 전멸만 game clear 조건으로 판정
void AMonsterTerritoryActor::CheckTerritoryGameClear()
{
	if (!HasAuthority()
		|| !bTriggerGameClearWhenMonstersDefeated
		|| !bGameClearEncounterRegistered
		|| !bFirstPlayerEntryTriggered
		|| !bEncounterMonstersPrepared
		|| bGameClearPresentationScheduled
		|| bGameClearRequested)
	{
		return;
	}

	for (const AMonsterBase* Monster : ActiveMonsters)
	{
		if (IsValid(Monster) && !Monster->IsActorBeingDestroyed() && !Monster->IsDead())
		{
			return;
		}
	}

	bGameClearPresentationScheduled = true;
	const float DelaySeconds = FMath::Max(0.f, GameClearPresentationDelay);
	if (DelaySeconds > 0.f)
	{
		GetWorldTimerManager().SetTimer(
			GameClearPresentationDelayTimerHandle,
			this,
			&ThisClass::RequestGameClearPresentation,
			DelaySeconds,
			false);
		return;
	}

	RequestGameClearPresentation();
}

// 마지막 boss 처치 뒤 설정한 시간이 지난 후에만 모든 peer의 clear UI를 시작
void AMonsterTerritoryActor::RequestGameClearPresentation()
{
	if (!HasAuthority() || bGameClearRequested)
	{
		return;
	}

	bGameClearPresentationScheduled = false;
	if (APragmataGameState* PragmataGameState = GetWorld() ? GetWorld()->GetGameState<APragmataGameState>() : nullptr)
	{
		bGameClearRequested = true;
		PragmataGameState->BeginGameClearPresentation();
	}
}

AMonsterBase* AMonsterTerritoryActor::SpawnMonster(const FMonsterTerritorySpawnInfo& SpawnInfo)
{
	if (!HasAuthority() || !SpawnInfo.MonsterClass || !SpawnInfo.SpawnPoint)
	{
		return nullptr;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	// 하강 appearance가 실제로 재생 가능한 항목만 착지 지점 위로 스폰
	const AMonsterBase* MonsterDefault = SpawnInfo.MonsterClass->GetDefaultObject<AMonsterBase>();
	const bool bUseFallingAppearance = SpawnInfo.bUseFallingAppearance
		&& MonsterDefault
		&& MonsterDefault->HasSpawnAppearanceMontage(SpawnInfo.AppearanceMontageOverride);

	FTransform SpawnTransform = SpawnInfo.SpawnPoint->GetActorTransform();
	if (bUseFallingAppearance)
	{
		SpawnTransform.AddToTranslation(FVector(0.f, 0.f, FMath::Max(0.f, SpawnInfo.FallingSpawnZOffset)));
	}

	AMonsterBase* SpawnedMonster = World->SpawnActorDeferred<AMonsterBase>(
		SpawnInfo.MonsterClass,
		SpawnTransform,
		this,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn
	);

	if (!SpawnedMonster)
	{
		return nullptr;
	}

	SpawnedMonster->SetAutoRunAIOnPossess(false);
	SpawnedMonster->FinishSpawning(SpawnTransform);
	RegisterMonster(SpawnedMonster);
	BeginMonsterEncounter(
		SpawnedMonster,
		SpawnInfo.AppearanceMontageOverride,
		bUseFallingAppearance,
		SpawnInfo.FallingAppearanceEndSectionName,
		SpawnInfo.FallingEndTransitionDistance);

	return SpawnedMonster;
}

// runtime spawn monster는 perception을 기다리지 않고 현재 Territory player를 target으로 사용
bool AMonsterTerritoryActor::AssignCurrentTargetToMonster(AMonsterBase* Monster) const
{
	if (!HasAuthority()
		|| !IsValid(Monster)
		|| !ActiveMonsters.Contains(Monster)
		|| !HasTriggeringPlayerInsideTerritory())
	{
		return false;
	}

	return Monster->SetTerritoryTarget(CurrentTarget);
}

void AMonsterTerritoryActor::ClearMonsterTargetIfSame(AMonsterBase* Monster, AActor* TargetActor) const
{
	if (!IsValid(Monster) || !TargetActor)
	{
		return;
	}

	AMonsterAIController* AIController = Cast<AMonsterAIController>(Monster->GetController());
	if (!AIController)
	{
		return;
	}

	// 블랙보드 Target 비우기
	AIController->ClearTargetIfSame(TargetActor);

	// MonsterBase CurrentTarget 비우기
	Monster->ClearTargetIfSame(TargetActor);
}

void AMonsterTerritoryActor::ResolvePendingControllerTerritories()
{
	for (auto Iterator = PendingTerritoryControllerMonsters.CreateIterator(); Iterator; ++Iterator)
	{
		AMonsterBase* Monster = Iterator->Get();
		if (!IsValid(Monster) || !ActiveMonsters.Contains(Monster))
		{
			PendingSpawnTargetMonsters.Remove(*Iterator);
			Iterator.RemoveCurrent();
			continue;
		}

		AMonsterAIController* AIController = Cast<AMonsterAIController>(Monster->GetController());
		if (!AIController)
		{
			continue;
		}

		AIController->SetTerritory(this);
		if (bRequireMapGimmickActivation && !bFirstPlayerEntryTriggered)
		{
			SuspendMonsterUntilTerritoryTrigger(Monster);
			Iterator.RemoveCurrent();
			continue;
		}
		if (PendingAppearanceStartMonsters.Contains(*Iterator))
		{
			Iterator.RemoveCurrent();
			continue;
		}
		if (bFirstPlayerEntryTriggered)
		{
			StartMonsterAIWithCurrentTarget(Monster);
			Iterator.RemoveCurrent();
			continue;
		}
		if (PendingSpawnTargetMonsters.Contains(*Iterator)
			&& (!HasTriggeringPlayerInsideTerritory() || AssignCurrentTargetToMonster(Monster)))
		{
			PendingSpawnTargetMonsters.Remove(*Iterator);
		}
		Iterator.RemoveCurrent();
	}
}

// ReturnHome 중인 monster가 경계 안쪽 여유 거리까지 들어오면 전투 분기 전환 허용
void AMonsterTerritoryActor::UpdateReturningMonsters()
{
	for (auto Iterator = ReturningMonsters.CreateIterator(); Iterator; ++Iterator)
	{
		AMonsterBase* Monster = Iterator->Get();
		if (!IsValid(Monster) || !ActiveMonsters.Contains(Monster))
		{
			Iterator.RemoveCurrent();
			continue;
		}

		if (!IsMonsterInsideReentryBounds(Monster))
		{
			continue;
		}

		if (AMonsterAIController* AIController = Cast<AMonsterAIController>(Monster->GetController()))
		{
			AIController->SetMonsterTerritory(true);
			Iterator.RemoveCurrent();
		}
	}
}

// 회전된 TriggerBox의 XY 경계에서 설정한 margin만큼 안쪽인지 확인
bool AMonsterTerritoryActor::IsMonsterInsideReentryBounds(const AMonsterBase* Monster) const
{
	if (!TriggerBox || !IsValid(Monster))
	{
		return false;
	}

	const FVector ScaledExtent = TriggerBox->GetScaledBoxExtent();
	const FVector LocalOffset = TriggerBox->GetComponentQuat().UnrotateVector(
		Monster->GetActorLocation() - TriggerBox->GetComponentLocation()
	);
	const float MaximumMargin = FMath::Max(0.f, FMath::Min(ScaledExtent.X, ScaledExtent.Y) - 1.f);
	const float ClampedMargin = FMath::Clamp(MonsterReentryMargin, 0.f, MaximumMargin);

	return FMath::Abs(LocalOffset.X) <= ScaledExtent.X - ClampedMargin
		&& FMath::Abs(LocalOffset.Y) <= ScaledExtent.Y - ClampedMargin
		&& FMath::Abs(LocalOffset.Z) <= ScaledExtent.Z;
}

bool AMonsterTerritoryActor::IsActorInsideTerritory(AActor* Actor) const
{
	return IsTargetActor(Actor) && TriggerBox && TriggerBox->IsOverlappingActor(Actor);
}

bool AMonsterTerritoryActor::IsTargetActor(AActor* Actor) const
{
	if (!Actor)
	{
		return false;
	}

	const UPragmataFactionComponent* FactionComponent = Actor->FindComponentByClass<UPragmataFactionComponent>();
	return FactionComponent && FactionComponent->GetFaction() == TargetFaction;
}

void AMonsterTerritoryActor::RemoveInvalidMonsters()
{
	ActiveMonsters.RemoveAll(
		[](AMonsterBase* Monster)
		{
			return !IsValid(Monster);
		}
	);
}
