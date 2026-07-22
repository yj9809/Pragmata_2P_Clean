#pragma once

#include "CoreMinimal.h"
#include "Data/PragmataTypes.h"
#include "GameFramework/GameModeBase.h"
#include "PragmataGameMode.generated.h"

class ADianaOperatorPawn;
class AHughCharacter;
class APragmataPlayerController;
class APragmataPlayerState;
class ACheckPoint;

UCLASS()
class PRAGMATA_2P_API APragmataGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	APragmataGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void InitGameState() override;
	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
	virtual FString InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId, const FString& Options, const FString& Portal = TEXT("")) override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;
	virtual void RestartPlayerAtPlayerStart(AController* NewPlayer, AActor* StartSpot) override;
	virtual void FinishRestartPlayer(AController* NewPlayer, const FRotator& StartRotation) override;
	virtual void Logout(AController* Exiting) override;

	void SetActiveCheckpoint(ACheckPoint* Checkpoint);
	void ScheduleTeamRespawn();
	void OpenCheckpointUpgradeSession();
	void CloseCheckpointUpgradeSession();
	bool IsCheckpointUpgradeSessionActive() const { return bCheckpointUpgradeSessionActive; }

private:
	EPragmataPlayerRole ParseRequestedRole(const FString& Options) const;
	EPragmataPlayerRole ResolveRequestedRole(const FString& Options) const;
	EPragmataPlayerRole ResolveGameplayRoleForController(AController* Controller, const FString& Options);
	EPragmataPlayerRole ResolveLobbyHandoffRoleForController(AController* Controller);
	EPragmataPlayerRole EnsureAssignedRoleForController(APlayerController* PlayerController, const FString& Options, FString& OutErrorMessage);
	bool ShouldDeferRoleAssignmentFailure(const FString& RoleErrorMessage) const;
	EPragmataPlayerRole GetAssignedRoleForController(const AController* Controller) const;
	bool ValidateLoginOptions(const FString& Options, FString& OutErrorMessage) const;
	bool IsRoomCodeValidForLogin(EPragmataPlayerRole PlayerRole, const FString& Options) const;
	FString ResolveRoomCodeForLogin(const FString& Options) const;
	bool IsRoleAvailable(EPragmataPlayerRole PlayerRole, const AController* ControllerToIgnore = nullptr) const;
	bool IsPawnClassConfiguredForRole(EPragmataPlayerRole PlayerRole, FString& OutErrorMessage) const;
	FVector GetPlayerStartOffsetForRole(EPragmataPlayerRole PlayerRole, const AActor* StartSpot) const;
	void RegisterSpawnedPawnForRole(AController* NewPlayer);
	void ApplyGameplaySkinSelections();
	void ClearRoleState(EPragmataPlayerRole PlayerRole);
	void CloseSessionIfBothPlayersConnected() const;
	void ExecuteTeamRespawn();

	UPROPERTY(EditDefaultsOnly, Category="Pragmata|Game")
	TSubclassOf<AHughCharacter> HughCharacterClass;

	UPROPERTY(EditDefaultsOnly, Category="Pragmata|Game")
	TSubclassOf<ADianaOperatorPawn> DianaOperatorPawnClass;

	UPROPERTY(EditDefaultsOnly, Category="Pragmata|Game")
	float DianaPlayerStartRightOffset = 180.0f;

	UPROPERTY()
	TObjectPtr<AHughCharacter> SpawnedHughCharacter = nullptr;

	UPROPERTY()
	TObjectPtr<ADianaOperatorPawn> SpawnedDianaOperatorPawn = nullptr;

	// 서버 travel URL에서 검증할 초대 코드
	FString ExpectedRoomCode;

	TWeakObjectPtr<ACheckPoint> ActiveCheckpoint;
	FTimerHandle TeamRespawnTimerHandle;
	bool bCheckpointUpgradeSessionActive = false;
};
