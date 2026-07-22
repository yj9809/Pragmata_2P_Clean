#pragma once

#include "CoreMinimal.h"
#include "Data/PragmataTypes.h"
#include "GameFramework/GameStateBase.h"
#include "TimerManager.h"
#include "PragmataGameState.generated.h"

class AHughCharacter;
class ADianaOperatorPawn;
class AActor;
class UTutorialStepData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPowerStateChanged, bool, bIsPowerOn);
DECLARE_MULTICAST_DELEGATE_OneParam(FPragmataActiveHackActorChangedSignature, AActor*);
DECLARE_MULTICAST_DELEGATE_OneParam(FPragmataActiveBossTargetChangedSignature, AActor*);
DECLARE_MULTICAST_DELEGATE_OneParam(FPragmataCombatStateChangedSignature, bool);
DECLARE_MULTICAST_DELEGATE_OneParam(FPragmataHughCharacterChangedSignature, AHughCharacter*);
DECLARE_MULTICAST_DELEGATE_OneParam(FPragmataDianaOperatorPawnChangedSignature, ADianaOperatorPawn*);
DECLARE_MULTICAST_DELEGATE_OneParam(FPragmataCheckpointUpgradeSelectionChangedSignature, EPragmataUpgradeType);
DECLARE_MULTICAST_DELEGATE_TwoParams(FPragmataCheckpointUpgradeHoldChangedSignature, float, bool);

UCLASS()
class PRAGMATA_2P_API APragmataGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "Power System")
	FOnPowerStateChanged OnPowerStateChanged;

	UFUNCTION(BlueprintCallable, Category = "Power System")
	void SetGlobalPowerState(bool bNewPowerState);

	UFUNCTION(BlueprintPure, Category = "Power System")
	bool GetGlobalPowerState() const { return bGlobalPowerOn; }

private:
	UFUNCTION()
	void OnRep_GlobalPowerOn();

	UPROPERTY(ReplicatedUsing=OnRep_GlobalPowerOn)
	bool bGlobalPowerOn = false;

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category="Pragmata|Game")
	AHughCharacter* GetHughCharacter() const { return HughCharacter; }

	UFUNCTION(BlueprintPure, Category="Pragmata|Game")
	ADianaOperatorPawn* GetDianaOperatorPawn() const { return DianaOperatorPawn; }

	UFUNCTION(BlueprintPure, Category="Pragmata|Game")
	FString GetRoomCode() const { return RoomCode; }

	UFUNCTION(BlueprintPure, Category="Pragmata|Game")
	bool IsHughConnected() const { return bHughConnected; }

	UFUNCTION(BlueprintPure, Category="Pragmata|Game")
	bool IsDianaConnected() const { return bDianaConnected; }

	void SetHughCharacter(AHughCharacter* NewHughCharacter);
	void SetDianaOperatorPawn(ADianaOperatorPawn* NewDianaOperatorPawn);
	void SetRoomCode(const FString& NewRoomCode);
	void SetRoleConnected(EPragmataPlayerRole PlayerRole, bool bConnected);

	UFUNCTION(BlueprintPure, Category="Pragmata|Game Clear")
	bool IsGameClearPresentationActive() const { return bGameClearPresentationActive; }

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Pragmata|Game Clear")
	void BeginGameClearPresentation();

	FPragmataHughCharacterChangedSignature OnHughCharacterChanged;
	FPragmataDianaOperatorPawnChangedSignature OnDianaOperatorPawnChanged;

	UFUNCTION(BlueprintPure, Category="Pragmata|Hack")
	AActor* GetActiveEnemyHackTarget() const { return ActiveEnemyHackTarget; }

	UFUNCTION(BlueprintPure, Category="Pragmata|Hack")
	AActor* GetActiveMapPuzzleActor() const { return ActiveMapPuzzleActor; }

	void SetActiveEnemyHackTarget(AActor* NewTarget);
	void SetActiveMapPuzzleActor(AActor* NewPuzzleActor);

	FPragmataActiveHackActorChangedSignature OnActiveEnemyHackTargetChanged;
	FPragmataActiveHackActorChangedSignature OnActiveMapPuzzleActorChanged;

	UFUNCTION(BlueprintPure, Category="Pragmata|Boss")
	AActor* GetActiveBossTarget() const { return ActiveBossTarget; }

	UFUNCTION(BlueprintCallable, Category="Pragmata|Boss")
	void SetActiveBossTarget(AActor* NewBossTarget);

	FPragmataActiveBossTargetChangedSignature OnActiveBossTargetChanged;

	void RegisterScannablePOI(AActor* POIActor);
	void UnregisterScannablePOI(AActor* POIActor);
	void GetRegisteredScannablePOIs(TArray<AActor*>& OutPOIActors) const;

	UFUNCTION(BlueprintPure, Category="Pragmata|Game")
	bool AreDianaActionsLocked() const { return bDianaActionsLocked; }

	UFUNCTION(BlueprintPure, Category="Pragmata|Game")
	bool IsEnemyCombatActive() const { return bEnemyCombatActive; }

	void SetDianaActionsLocked(bool bNewLocked);
	EPragmataUpgradeType GetCheckpointUpgradeSelection() const { return CheckpointUpgradeSelection; }
	void SetCheckpointUpgradeSelection(EPragmataUpgradeType NewSelection);
	void SetCheckpointUpgradeHold(float NewProgress, bool bNewRefund);
	FPragmataCheckpointUpgradeSelectionChangedSignature OnCheckpointUpgradeSelectionChanged;
	FPragmataCheckpointUpgradeHoldChangedSignature OnCheckpointUpgradeHoldChanged;
	void RegisterEnemyAggro(AActor* EnemyActor, AActor* TargetActor);
	void UnregisterEnemyAggro(AActor* EnemyActor);

	FPragmataCombatStateChangedSignature OnEnemyCombatActiveChanged;

	// 서버가 한 번 호출하면 Hugh/Diana 양쪽 클라이언트가 각자 로컬 TutorialManagerSubsystem에서 표시/숨김을 실행
	UFUNCTION(NetMulticast, Reliable, BlueprintCallable, Category="Pragmata|Tutorial")
	void Multicast_ShowTutorialStep(UTutorialStepData* StepData);

	UFUNCTION(NetMulticast, Reliable, BlueprintCallable, Category="Pragmata|Tutorial")
	void Multicast_HideTutorial();

	uint32 BeginTutorialPresentation(UTutorialStepData* StepData);
	uint32 BeginTutorialPresentationOnce(FName TutorialId, UTutorialStepData* StepData);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_ShowTutorialPresentation(UTutorialStepData* StepData, uint32 PresentationSerial);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_ShowTutorialPresentationOnce(FName TutorialId, UTutorialStepData* StepData, uint32 PresentationSerial);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_HideTutorialPresentation(uint32 ExpectedPresentationSerial);

	UFUNCTION(NetMulticast, Reliable, BlueprintCallable, Category="Pragmata|Objective")
	void Multicast_ShowObjectiveAlert(const FText& ObjectiveText, float DisplaySeconds = 4.f);

	// 서버가 Hugh/Diana 양쪽 로컬 HUD에 같은 대사를 표시하는 co-op 공통 진입점
	UFUNCTION(NetMulticast, Reliable, BlueprintCallable, Category="Pragmata|Dialogue")
	void Multicast_PlayDialogueLine(const FDialogueLine& Line, float DisplaySeconds = -1.f);

	// 서버가 Hugh/Diana 양쪽 로컬 HUD의 현재 대사를 닫는 공통 진입점
	UFUNCTION(NetMulticast, Reliable, BlueprintCallable, Category="Pragmata|Dialogue")
	void Multicast_HideDialogue();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_ReturnPlayersToTitle();

private:
	UFUNCTION()
	void OnRep_ActiveEnemyHackTarget();

	UFUNCTION()
	void OnRep_ActiveEnemyHackSerial();

	UFUNCTION()
	void OnRep_ActiveMapPuzzleActor();

	UFUNCTION()
	void OnRep_ActiveBossTarget();

	UFUNCTION()
	void OnRep_ActiveBossTargetSerial();

	UFUNCTION()
	void OnRep_EnemyCombatActive();

	UFUNCTION()
	void OnRep_GameClearPresentationActive();

	UFUNCTION()
	void OnRep_HughCharacter();

	UFUNCTION()
	void OnRep_DianaOperatorPawn();

	UFUNCTION()
	void OnRep_CheckpointUpgradeSelection();

	UFUNCTION()
	void OnRep_CheckpointUpgradeHold();

	void SetEnemyCombatActive(bool bNewActive);
	void HandleCombatExitGraceElapsed();
	void ShowGameClearPresentationOnLocalControllers();
	bool IsCombatTargetActor(const AActor* TargetActor) const;
	void CleanupInvalidAggroEnemies();

	UPROPERTY(ReplicatedUsing=OnRep_HughCharacter, BlueprintReadOnly, Category="Pragmata|Game", meta=(AllowPrivateAccess="true"))
	TObjectPtr<AHughCharacter> HughCharacter = nullptr;

	UPROPERTY(ReplicatedUsing=OnRep_DianaOperatorPawn, BlueprintReadOnly, Category="Pragmata|Game", meta=(AllowPrivateAccess="true"))
	TObjectPtr<ADianaOperatorPawn> DianaOperatorPawn = nullptr;

	UPROPERTY(ReplicatedUsing=OnRep_CheckpointUpgradeSelection)
	EPragmataUpgradeType CheckpointUpgradeSelection = EPragmataUpgradeType::Suit;

	UPROPERTY(ReplicatedUsing=OnRep_CheckpointUpgradeHold)
	float CheckpointUpgradeHoldProgress = 0.f;

	UPROPERTY(ReplicatedUsing=OnRep_CheckpointUpgradeHold)
	bool bCheckpointUpgradeRefundHold = false;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="Pragmata|Game", meta=(AllowPrivateAccess="true"))
	FString RoomCode;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="Pragmata|Game", meta=(AllowPrivateAccess="true"))
	bool bHughConnected = false;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="Pragmata|Game", meta=(AllowPrivateAccess="true"))
	bool bDianaConnected = false;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="Pragmata|Game", meta=(AllowPrivateAccess="true"))
	bool bDianaActionsLocked = false;

	UPROPERTY(ReplicatedUsing=OnRep_EnemyCombatActive, BlueprintReadOnly, Category="Pragmata|Game", meta=(AllowPrivateAccess="true"))
	bool bEnemyCombatActive = false;

	UPROPERTY(ReplicatedUsing=OnRep_GameClearPresentationActive, BlueprintReadOnly, Category="Pragmata|Game Clear", meta=(AllowPrivateAccess="true"))
	bool bGameClearPresentationActive = false;

	UPROPERTY(ReplicatedUsing=OnRep_ActiveEnemyHackTarget, BlueprintReadOnly, Category="Pragmata|Hack", meta=(AllowPrivateAccess="true"))
	TObjectPtr<AActor> ActiveEnemyHackTarget = nullptr;

	UPROPERTY(ReplicatedUsing=OnRep_ActiveEnemyHackSerial)
	uint8 ActiveEnemyHackSerial = 0;

	UPROPERTY(ReplicatedUsing=OnRep_ActiveMapPuzzleActor, BlueprintReadOnly, Category="Pragmata|Hack", meta=(AllowPrivateAccess="true"))
	TObjectPtr<AActor> ActiveMapPuzzleActor = nullptr;

	UPROPERTY(ReplicatedUsing=OnRep_ActiveBossTarget, BlueprintReadOnly, Category="Pragmata|Boss", meta=(AllowPrivateAccess="true"))
	TObjectPtr<AActor> ActiveBossTarget = nullptr;

	UPROPERTY(ReplicatedUsing=OnRep_ActiveBossTargetSerial)
	uint8 ActiveBossTargetSerial = 0;

	UPROPERTY(Transient)
	TSet<TObjectPtr<AActor>> ActiveAggroEnemies;

	UPROPERTY(Transient)
	TSet<TObjectPtr<AActor>> RegisteredScannablePOIs;

	UPROPERTY(EditDefaultsOnly, Category="Pragmata|Game|Combat", meta=(ClampMin="0"))
	float CombatExitGraceSeconds = 1.f;

	FTimerHandle CombatExitGraceTimerHandle;

	uint32 TutorialPresentationSerial = 0;
};
