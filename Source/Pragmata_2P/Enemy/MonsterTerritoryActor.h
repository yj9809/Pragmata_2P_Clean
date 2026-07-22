#pragma once

#include "CoreMinimal.h"
#include "Data/PragmataTypes.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "MonsterTerritoryActor.generated.h"

class AMonsterBase;
class UBoxComponent;
class ULevelSequence;
class ULevelSequencePlayer;
class UAnimMontage;
class UPrimitiveComponent;

// 첫 player 진입 시 map gimmick이 server gameplay를 연결할 Blueprint event
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FMonsterTerritoryFirstPlayerEnteredSignature,
	AActor*,
	PlayerActor);

USTRUCT(BlueprintType)
struct FMonsterTerritorySpawnInfo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory")
	TSubclassOf<AMonsterBase> MonsterClass = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory")
	TObjectPtr<AActor> SpawnPoint = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Falling Appearance")
	bool bUseFallingAppearance = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Falling Appearance", meta=(EditCondition="bUseFallingAppearance", EditConditionHides, ClampMin="0.0", UIMin="0.0"))
	float FallingSpawnZOffset = 1200.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Falling Appearance", meta=(EditCondition="bUseFallingAppearance", EditConditionHides))
	FName FallingAppearanceEndSectionName = TEXT("End");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Falling Appearance", meta=(EditCondition="bUseFallingAppearance", EditConditionHides, ClampMin="0.0", UIMin="0.0"))
	float FallingEndTransitionDistance = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory")
	TObjectPtr<UAnimMontage> AppearanceMontageOverride = nullptr;
};

USTRUCT(BlueprintType)
struct FMonsterTerritoryPlacedMonsterInfo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory")
	TObjectPtr<AMonsterBase> Monster = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory")
	TObjectPtr<UAnimMontage> AppearanceMontageOverride = nullptr;
};

UCLASS()
class PRAGMATA_2P_API AMonsterTerritoryActor : public AActor
{
	GENERATED_BODY()

public:
	AMonsterTerritoryActor();

	// map actor가 전투 공간 봉쇄 같은 gameplay를 연결할 첫 진입 event
	UPROPERTY(BlueprintAssignable, Category="Territory|Events")
	FMonsterTerritoryFirstPlayerEnteredSignature OnFirstPlayerEntered;

	UPROPERTY(BlueprintAssignable, Category="Territory|Events")
	FMonsterTerritoryFirstPlayerEnteredSignature OnPlayerEnteredTerritory;

	UFUNCTION(BlueprintPure, Category="Territory")
	bool IsActorInsideTerritory(AActor* Actor) const;

	UFUNCTION(BlueprintPure, Category="Territory")
	AActor* GetCurrentTarget() const { return CurrentTarget; }

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Territory|Activation")
	void ActivateTerritoryTrigger();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	UFUNCTION()
	void HandleBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult
	);

	UFUNCTION()
	void HandleEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex
	);

	UFUNCTION()
	void HandleMonsterDestroyed(AActor* DestroyedActor);

	UFUNCTION()
	void HandleMonsterDeath();

	// server 첫 진입을 모든 peer의 sequence 재생으로 전달
	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlayFirstEntrySequence(ULevelSequence* Sequence);

	UFUNCTION()
	void HandleFirstEntrySequenceFinished();

	void HandleMonsterSpawnAppearanceFinished(AMonsterBase* Monster);
	void TriggerFirstPlayerEntry(AActor* PlayerActor);
	void StartFirstEntrySequenceOrGameplay();
	void HandleFirstEntrySequenceDelayElapsed();
	void StartFirstEntryGameplay();
	void StartPlacedMonstersForEncounter();
	void BeginMonsterEncounter(
		AMonsterBase* Monster,
		UAnimMontage* AppearanceMontageOverride = nullptr,
		bool bUseFallingAppearance = false,
		FName FallingEndSectionName = NAME_None,
		float FallingEndTransitionDistance = 0.f);
	void ScheduleSpawnAppearanceFallback(
		AMonsterBase* Monster,
		UAnimMontage* AppearanceMontageOverride,
		bool bUseFallingAppearance);
	void HandleSpawnAppearanceFallback(TWeakObjectPtr<AMonsterBase> MonsterPtr);
	void StartMonsterAIWithCurrentTarget(AMonsterBase* Monster);
	void SuspendMonsterUntilTerritoryTrigger(AMonsterBase* Monster) const;
	bool ShouldDelayPlacedMonsterUntilTerritoryEntry(
		AMonsterBase* Monster,
		UAnimMontage* AppearanceMontageOverride,
		bool bDelayForFirstEntrySequence) const;
	const FMonsterTerritoryPlacedMonsterInfo* FindPlacedMonsterOverrideInfo(AMonsterBase* Monster) const;
	bool IsFirstEntrySequenceMonster(AMonsterBase* Monster) const;
	bool IsPlacedMonsterConfigured(AMonsterBase* Monster) const;
	void SpawnRuntimeMonsters();
	void PlayFirstEntrySequence(ULevelSequence* Sequence);
	ULevelSequencePlayer* CreateTerritorySequencePlayer(ULevelSequence* Sequence, bool bUseTransformOrigin);
	bool IsTriggeringPlayer(AActor* Actor) const;
	AActor* FindTriggeringPlayerInsideTerritory() const;
	bool HasTriggeringPlayerInsideTerritory() const;
	void RegisterMonster(AMonsterBase* Monster);
	void LockCombatTargets();
	void UnlockCombatTargets();
	void CheckTerritoryCombatClear();
	void CheckTerritoryGameClear();
	void RequestGameClearPresentation();
	AMonsterBase* SpawnMonster(const FMonsterTerritorySpawnInfo& SpawnInfo);
	bool AssignCurrentTargetToMonster(AMonsterBase* Monster) const;
	void ClearMonsterTargetIfSame(AMonsterBase* Monster, AActor* TargetActor) const;
	void ResolvePendingControllerTerritories();
	void UpdateReturningMonsters();
	bool IsMonsterInsideReentryBounds(const AMonsterBase* Monster) const;
	bool IsTargetActor(AActor* Actor) const;
	void RemoveInvalidMonsters();

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UBoxComponent> TriggerBox = nullptr;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Territory|Settings", meta=(AllowPrivateAccess="true"))
	TArray<TObjectPtr<AMonsterBase>> PlacedMonsters;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Territory|Settings", meta=(AllowPrivateAccess="true"))
	TArray<FMonsterTerritoryPlacedMonsterInfo> PlacedMonsterOverrides;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Territory|First Entry", meta=(AllowPrivateAccess="true"))
	TArray<TObjectPtr<AMonsterBase>> FirstEntrySequenceMonsters;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Territory|Settings", meta=(AllowPrivateAccess="true"))
	TArray<FMonsterTerritorySpawnInfo> RuntimeSpawns;

	// 첫 player 진입 때 모든 peer에서 재생할 Territory 전용 sequence
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Territory|First Entry", meta=(AllowPrivateAccess="true"))
	TObjectPtr<ULevelSequence> FirstEntrySequence = nullptr;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Territory|First Entry", meta=(AllowPrivateAccess="true", EditCondition="FirstEntrySequence != nullptr", EditConditionHides, ClampMin="0.0", UIMin="0.0", Units="s"))
	float FirstEntrySequenceDelay = 0.f;

	// absolute transform track을 Territory 위치·회전 기준 local 연출로 재사용
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Territory|First Entry", meta=(AllowPrivateAccess="true", EditCondition="FirstEntrySequence != nullptr", EditConditionHides))
	bool bUseTerritoryAsSequenceTransformOrigin = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Settings", meta=(AllowPrivateAccess="true"))
	EPragmataFaction TargetFaction = EPragmataFaction::Player;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Territory|Activation", meta=(AllowPrivateAccess="true"))
	bool bRequireMapGimmickActivation = false;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Territory|Combat Lock", meta=(AllowPrivateAccess="true"))
	bool bLockMapUntilMonstersCleared = false;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Territory|Combat Lock", meta=(AllowPrivateAccess="true", EditCondition="bLockMapUntilMonstersCleared", EditConditionHides))
	TArray<TObjectPtr<AActor>> CombatLockTargets;

	// 체크한 Territory만 등록한 monster를 모두 처치했을 때 game clear를 요청하며 기본값은 일반 전투와 같게 비활성
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Territory|Game Clear", meta=(AllowPrivateAccess="true"))
	bool bTriggerGameClearWhenMonstersDefeated = false;

	// 보스 Territory의 마지막 monster 사망 뒤 clear UI를 시작하기 전 대기 시간
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Territory|Game Clear", meta=(AllowPrivateAccess="true", ClampMin="0.0", UIMin="0.0", Units="s", DisplayName="Game Clear Delay", EditCondition="bTriggerGameClearWhenMonstersDefeated", EditConditionHides))
	float GameClearPresentationDelay = 0.f;

	// 경계 재진입 직후 전투 분기로 되돌아가지 않도록 안쪽 판정 여유 거리
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Settings", meta=(AllowPrivateAccess="true", ClampMin="0.0"))
	float MonsterReentryMargin = 150.f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Territory|Runtime", meta=(AllowPrivateAccess="true"))
	TArray<TObjectPtr<AMonsterBase>> ActiveMonsters;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Territory|Runtime", meta=(AllowPrivateAccess="true"))
	TObjectPtr<AActor> CurrentTarget = nullptr;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Territory|Runtime", meta=(AllowPrivateAccess="true"))
	bool bFirstPlayerEntryTriggered = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Territory|Runtime", meta=(AllowPrivateAccess="true"))
	bool bTerritoryTriggerActivated = true;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Territory|Runtime", meta=(AllowPrivateAccess="true"))
	bool bCombatLockActive = false;

	bool bEncounterMonstersPrepared = false;
	bool bCombatLockReleased = false;
	bool bGameClearEncounterRegistered = false;
	bool bGameClearPresentationScheduled = false;
	bool bGameClearRequested = false;

	bool bRuntimeSpawnsTriggered = false;

	UPROPERTY(Transient)
	TObjectPtr<ULevelSequencePlayer> FirstEntrySequencePlayer = nullptr;

	FTimerHandle FirstEntrySequenceDelayTimerHandle;
	FTimerHandle GameClearPresentationDelayTimerHandle;

	TSet<TWeakObjectPtr<AMonsterBase>> ReturningMonsters;
	TSet<TWeakObjectPtr<AMonsterBase>> PendingTerritoryControllerMonsters;
	TSet<TWeakObjectPtr<AMonsterBase>> PendingSpawnTargetMonsters;
	TSet<TWeakObjectPtr<AMonsterBase>> PendingAppearanceStartMonsters;
};
