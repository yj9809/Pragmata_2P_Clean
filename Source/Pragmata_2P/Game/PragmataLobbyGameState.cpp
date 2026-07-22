#include "Game/PragmataLobbyGameState.h"

#include "Game/PragmataPlayerState.h"
#include "Net/UnrealNetwork.h"

// lobby phase, room code, warning은 모든 client UI가 같은 화면을 보게 하는 기준
void APragmataLobbyGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(APragmataLobbyGameState, LobbyPhase);
	DOREPLIFETIME(APragmataLobbyGameState, RoomCode);
	DOREPLIFETIME(APragmataLobbyGameState, WarningMessage);
}

// 서버가 session setup, waiting, joining, character select, start 가능 상태를 전환
void APragmataLobbyGameState::SetLobbyPhase(EPragmataLobbyPhase NewPhase)
{
	if (!HasAuthority() || LobbyPhase == NewPhase)
	{
		return;
	}

	LobbyPhase = NewPhase;
}

// host가 만든 초대 코드를 joiner와 lobby UI에 복제
void APragmataLobbyGameState::SetRoomCode(const FString& NewRoomCode)
{
	if (!HasAuthority() || RoomCode == NewRoomCode)
	{
		return;
	}

	RoomCode = NewRoomCode;
}

// 서버 검증 실패 사유를 lobby widget에 표시
void APragmataLobbyGameState::SetWarningMessage(const FString& NewWarningMessage)
{
	if (!HasAuthority() || WarningMessage == NewWarningMessage)
	{
		return;
	}

	WarningMessage = NewWarningMessage;
}

// 두 lobby slot이 같은 캐릭터를 고르면 ready/start를 막음
bool APragmataLobbyGameState::HasSelectionConflict() const
{
	EPragmataPlayerRole FirstSelectedRole = EPragmataPlayerRole::None;

	for (APlayerState* PlayerState : PlayerArray)
	{
		const APragmataPlayerState* PragmataPlayerState = Cast<APragmataPlayerState>(PlayerState);
		if (!PragmataPlayerState || PragmataPlayerState->GetLobbyPlayerIndex() < 1 || PragmataPlayerState->GetLobbyPlayerIndex() > 2)
		{
			continue;
		}

		const EPragmataPlayerRole SelectedRole = PragmataPlayerState->GetLobbySelectedRole();
		if (SelectedRole != EPragmataPlayerRole::Hugh && SelectedRole != EPragmataPlayerRole::Diana)
		{
			continue;
		}

		if (FirstSelectedRole == EPragmataPlayerRole::None)
		{
			FirstSelectedRole = SelectedRole;
			continue;
		}

		if (FirstSelectedRole == SelectedRole)
		{
			return true;
		}
	}

	return false;
}

// 두 플레이어가 서로 다른 role로 ready 되었을 때만 host start 허용
bool APragmataLobbyGameState::CanStartGame() const
{
	if (!HasEnoughPlayersForSelection() || HasSelectionConflict())
	{
		return false;
	}

	int32 ReadyCount = 0;
	for (APlayerState* PlayerState : PlayerArray)
	{
		const APragmataPlayerState* PragmataPlayerState = Cast<APragmataPlayerState>(PlayerState);
		if (!PragmataPlayerState || PragmataPlayerState->GetLobbyPlayerIndex() < 1 || PragmataPlayerState->GetLobbyPlayerIndex() > 2)
		{
			continue;
		}

		if (!PragmataPlayerState->IsLobbyReady())
		{
			return false;
		}

		++ReadyCount;
	}

	return ReadyCount >= 2;
}

// lobby slot 1, 2가 모두 채워진 뒤에만 selection/ready phase로 이동
bool APragmataLobbyGameState::HasEnoughPlayersForSelection() const
{
	int32 LobbyPlayerCount = 0;
	for (APlayerState* PlayerState : PlayerArray)
	{
		const APragmataPlayerState* PragmataPlayerState = Cast<APragmataPlayerState>(PlayerState);
		if (PragmataPlayerState && PragmataPlayerState->GetLobbyPlayerIndex() >= 1 && PragmataPlayerState->GetLobbyPlayerIndex() <= 2)
		{
			++LobbyPlayerCount;
		}
	}

	return LobbyPlayerCount >= 2;
}
