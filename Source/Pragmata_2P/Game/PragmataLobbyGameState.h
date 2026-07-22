#pragma once

#include "CoreMinimal.h"
#include "Data/PragmataTypes.h"
#include "GameFramework/GameStateBase.h"
#include "PragmataLobbyGameState.generated.h"

UCLASS()
class PRAGMATA_2P_API APragmataLobbyGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category="Pragmata|Lobby")
	EPragmataLobbyPhase GetLobbyPhase() const { return LobbyPhase; }

	void SetLobbyPhase(EPragmataLobbyPhase NewPhase);

	UFUNCTION(BlueprintPure, Category="Pragmata|Lobby")
	FString GetRoomCode() const { return RoomCode; }

	void SetRoomCode(const FString& NewRoomCode);

	UFUNCTION(BlueprintPure, Category="Pragmata|Lobby")
	FString GetWarningMessage() const { return WarningMessage; }

	void SetWarningMessage(const FString& NewWarningMessage);

	UFUNCTION(BlueprintPure, Category="Pragmata|Lobby")
	bool HasSelectionConflict() const;

	UFUNCTION(BlueprintPure, Category="Pragmata|Lobby")
	bool CanStartGame() const;

	UFUNCTION(BlueprintPure, Category="Pragmata|Lobby")
	bool HasEnoughPlayersForSelection() const;

private:
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Pragmata|Lobby", meta=(AllowPrivateAccess="true"))
	EPragmataLobbyPhase LobbyPhase = EPragmataLobbyPhase::SessionSetup;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="Pragmata|Lobby", meta=(AllowPrivateAccess="true"))
	FString RoomCode;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="Pragmata|Lobby", meta=(AllowPrivateAccess="true"))
	FString WarningMessage;
};
