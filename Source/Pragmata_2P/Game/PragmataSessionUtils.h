#pragma once

#include "CoreMinimal.h"
#include "Data/PragmataTypes.h"

namespace PragmataSessionUtils
{
	inline constexpr int32 RoomCodeLength = 6;

	PRAGMATA_2P_API FString GenerateRoomCode();
	PRAGMATA_2P_API FString NormalizeRoomCode(const FString& RoomCode);
	PRAGMATA_2P_API bool IsValidRoomCode(const FString& RoomCode);
	PRAGMATA_2P_API FString BuildHughListenTravelURL(const FString& MapPath, const FString& RoomCode, const FString& GameplayGameModePath);
	PRAGMATA_2P_API FString BuildHughListenOpenLevelOptions(const FString& RoomCode, const FString& GameplayGameModePath);
	PRAGMATA_2P_API FString BuildDianaJoinTravelURL(const FString& ConnectInfo, const FString& RoomCode);
	PRAGMATA_2P_API FString BuildLobbyListenTravelURL(const FString& MapPath, const FString& RoomCode, EPragmataPlayerRole PreferredRole = EPragmataPlayerRole::None);
	PRAGMATA_2P_API FString BuildLobbyListenOpenLevelOptions(const FString& RoomCode, EPragmataPlayerRole PreferredRole = EPragmataPlayerRole::None);
	PRAGMATA_2P_API FString BuildLobbyJoinTravelURL(const FString& ConnectInfo, const FString& RoomCode, EPragmataPlayerRole PreferredRole = EPragmataPlayerRole::None);
	PRAGMATA_2P_API FString BuildGameTravelURL(const FString& MapPath, const FString& RoomCode, const FString& GameplayGameModePath);
}
