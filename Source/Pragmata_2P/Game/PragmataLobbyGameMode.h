#pragma once

#include "CoreMinimal.h"
#include "Data/PragmataTypes.h"
#include "GameFramework/GameModeBase.h"
#include "TimerManager.h"
#include "PragmataLobbyGameMode.generated.h"

class APragmataLobbyPlayerController;
class APragmataPlayerState;
class ULobbyCharacterSkinSet;

UCLASS()
class PRAGMATA_2P_API APragmataLobbyGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	APragmataLobbyGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void InitGameState() override;
	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
	virtual FString InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId, const FString& Options, const FString& Portal) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

	void RequestSelectionStep(APragmataLobbyPlayerController* PlayerController, int32 Direction);
	void RequestSkinSelectionStep(APragmataLobbyPlayerController* PlayerController, int32 Direction);
	void RequestReady(APragmataLobbyPlayerController* PlayerController);
	void RequestStartGame(APragmataLobbyPlayerController* PlayerController);

private:
	int32 ResolveNextLobbyPlayerIndex() const;
	EPragmataPlayerRole GetDefaultRoleForLobbyIndex(int32 LobbyPlayerIndex) const;
	EPragmataPlayerRole GetNextSelectableRole(EPragmataPlayerRole CurrentRole, int32 Direction) const;
	FName GetDefaultSkinForRole(EPragmataPlayerRole PlayerRole);
	FName GetNextSelectableSkin(EPragmataPlayerRole PlayerRole, FName CurrentSkinId, int32 Direction);
	bool IsSkinValidForRole(EPragmataPlayerRole PlayerRole, FName SkinId);
	void EnsureValidSkinSelection(APragmataPlayerState* PragmataPlayerState);
	EPragmataPlayerRole ParsePreferredRoleOption(const FString& Options) const;
	int32 GetLobbyPlayerCount() const;
	void AssignLobbyDefaults(APlayerController* NewPlayer);
	void RefreshLobbyPhaseAndWarning();
	void BeginPlayerJoiningPhase();
	void CompletePlayerJoiningPhase();
	bool IsHostLobbyPlayer(const APragmataPlayerState* PlayerState) const;
	bool IsRoomCodeValidForLogin(const FString& Options) const;
	bool FindLobbyPlayers(APragmataPlayerState*& OutPlayerOneState, APragmataPlayerState*& OutPlayerTwoState) const;
	void TravelToGameplayMap();
	ULobbyCharacterSkinSet* LoadLobbySkinSet();

	UPROPERTY(EditDefaultsOnly, Category="Pragmata|Lobby", meta=(ClampMin="0.0"))
	float StartGameTravelDelay = 0.35f;

	UPROPERTY(EditDefaultsOnly, Category="Pragmata|Lobby", meta=(ClampMin="0.0"))
	float PlayerJoiningRevealDelay = 1.25f;

	UPROPERTY(EditDefaultsOnly, Category="Pragmata|Lobby|Skin")
	TSoftObjectPtr<ULobbyCharacterSkinSet> LobbySkinSet;

	UPROPERTY(Transient)
	TObjectPtr<ULobbyCharacterSkinSet> LoadedLobbySkinSet = nullptr;

	FString ExpectedRoomCode;
	FString PendingGameplayTravelURL;
	FTimerHandle StartGameTravelTimerHandle;
	FTimerHandle PlayerJoiningTimerHandle;
};
