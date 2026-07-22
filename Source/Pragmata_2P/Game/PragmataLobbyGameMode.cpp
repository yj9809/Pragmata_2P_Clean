#include "Game/PragmataLobbyGameMode.h"

#include "Data/PragmataGameFlowConfig.h"
#include "Data/LobbyCharacterSkinSet.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Game/PragmataLobbyGameState.h"
#include "Game/PragmataLobbyPlayerController.h"
#include "Game/PragmataMultiplayerSessionSubsystem.h"
#include "Game/PragmataPlayerState.h"
#include "Game/PragmataSessionUtils.h"
#include "Kismet/GameplayStatics.h"

// lobby map은 pawn 없이 spectator controller와 replicated PlayerState만 사용
APragmataLobbyGameMode::APragmataLobbyGameMode()
{
	GameStateClass = APragmataLobbyGameState::StaticClass();
	PlayerStateClass = APragmataPlayerState::StaticClass();
	PlayerControllerClass = APragmataLobbyPlayerController::StaticClass();
	DefaultPawnClass = nullptr;
	bStartPlayersAsSpectators = true;
	bUseSeamlessTravel = true;
	LobbySkinSet = TSoftObjectPtr<ULobbyCharacterSkinSet>(FSoftObjectPath(TEXT("/Game/Data/DA_LobbyCharacterSkinSet.DA_LobbyCharacterSkinSet")));
}

// host listen URL의 room code를 lobby 입장 검증 기준으로 저장
void APragmataLobbyGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	ExpectedRoomCode = PragmataSessionUtils::NormalizeRoomCode(UGameplayStatics::ParseOption(Options, TEXT("RoomCode")));
}

// room code 유무로 session setup과 waiting phase를 구분
void APragmataLobbyGameMode::InitGameState()
{
	Super::InitGameState();

	if (APragmataLobbyGameState* LobbyGameState = GetGameState<APragmataLobbyGameState>())
	{
		LobbyGameState->SetRoomCode(ExpectedRoomCode);
		LobbyGameState->SetLobbyPhase(ExpectedRoomCode.IsEmpty() ? EPragmataLobbyPhase::SessionSetup : EPragmataLobbyPhase::WaitingForPlayer);
	}
}

// lobby는 같은 room code의 두 명까지만 입장 허용
void APragmataLobbyGameMode::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
	if (!ErrorMessage.IsEmpty())
	{
		return;
	}

	if (!IsRoomCodeValidForLogin(Options))
	{
		ErrorMessage = TEXT("pragmata_invalid_lobby_room_code");
		return;
	}

	if (GetLobbyPlayerCount() >= 2)
	{
		ErrorMessage = TEXT("pragmata_lobby_full");
	}
}

// 하위 호환용 travel option이 있으면 PlayerState 초기 선택값으로 저장
FString APragmataLobbyGameMode::InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId, const FString& Options, const FString& Portal)
{
	const FString Result = Super::InitNewPlayer(NewPlayerController, UniqueId, Options, Portal);

	const EPragmataPlayerRole PreferredRole = ParsePreferredRoleOption(Options);
	if (APragmataPlayerState* PragmataPlayerState = NewPlayerController ? NewPlayerController->GetPlayerState<APragmataPlayerState>() : nullptr)
	{
		if (PreferredRole == EPragmataPlayerRole::Hugh || PreferredRole == EPragmataPlayerRole::Diana)
		{
			PragmataPlayerState->SetLobbySelectedRole(PreferredRole);
		}
	}

	return Result;
}

// 새 player가 들어오면 lobby slot과 phase를 서버 기준으로 갱신
void APragmataLobbyGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	AssignLobbyDefaults(NewPlayer);
	if (!ExpectedRoomCode.IsEmpty() && GetLobbyPlayerCount() >= 2)
	{
		BeginPlayerJoiningPhase();
		return;
	}

	RefreshLobbyPhaseAndWarning();
}

// 퇴장한 lobby slot을 비워 다음 joiner가 같은 index를 받을 수 있게 함
void APragmataLobbyGameMode::Logout(AController* Exiting)
{
	if (APragmataPlayerState* PragmataPlayerState = Exiting ? Exiting->GetPlayerState<APragmataPlayerState>() : nullptr)
	{
		PragmataPlayerState->SetLobbyReady(false);
		PragmataPlayerState->SetLobbyPlayerIndex(0);
		PragmataPlayerState->SetLobbySelectedRole(EPragmataPlayerRole::None);
		PragmataPlayerState->SetLobbySelectedSkinId(NAME_None);
	}

	Super::Logout(Exiting);
	if (GetLobbyPlayerCount() < 2)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(PlayerJoiningTimerHandle);
		}
	}
	RefreshLobbyPhaseAndWarning();
}

// 선택 변경은 서버 PlayerState를 수정하고 ready를 풀어 충돌 상태를 재평가
void APragmataLobbyGameMode::RequestSelectionStep(APragmataLobbyPlayerController* PlayerController, int32 Direction)
{
	if (!PlayerController || Direction == 0)
	{
		return;
	}

	APragmataLobbyGameState* LobbyGameState = GetGameState<APragmataLobbyGameState>();
	if (!LobbyGameState
		|| !LobbyGameState->HasEnoughPlayersForSelection()
		|| (LobbyGameState->GetLobbyPhase() != EPragmataLobbyPhase::CharacterSelect && LobbyGameState->GetLobbyPhase() != EPragmataLobbyPhase::ReadyToStart))
	{
		return;
	}

	APragmataPlayerState* PragmataPlayerState = PlayerController->GetPlayerState<APragmataPlayerState>();
	if (!PragmataPlayerState)
	{
		return;
	}
	if (PragmataPlayerState->GetLobbyPlayerIndex() < 1 || PragmataPlayerState->GetLobbyPlayerIndex() > 2)
	{
		LobbyGameState->SetWarningMessage(TEXT("로비가 가득 찼습니다"));
		return;
	}

	const EPragmataPlayerRole NextRole = GetNextSelectableRole(PragmataPlayerState->GetLobbySelectedRole(), Direction);
	PragmataPlayerState->SetLobbySelectedRole(NextRole);
	PragmataPlayerState->SetLobbySelectedSkinId(GetDefaultSkinForRole(NextRole));
	PragmataPlayerState->SetLobbyReady(false);
	RefreshLobbyPhaseAndWarning();
}

// skin 변경은 현재 선택 role 안에서만 서버 검증 후 확정
void APragmataLobbyGameMode::RequestSkinSelectionStep(APragmataLobbyPlayerController* PlayerController, int32 Direction)
{
	if (!PlayerController || Direction == 0)
	{
		return;
	}

	APragmataLobbyGameState* LobbyGameState = GetGameState<APragmataLobbyGameState>();
	if (!LobbyGameState
		|| !LobbyGameState->HasEnoughPlayersForSelection()
		|| (LobbyGameState->GetLobbyPhase() != EPragmataLobbyPhase::CharacterSelect && LobbyGameState->GetLobbyPhase() != EPragmataLobbyPhase::ReadyToStart))
	{
		return;
	}

	APragmataPlayerState* PragmataPlayerState = PlayerController->GetPlayerState<APragmataPlayerState>();
	if (!PragmataPlayerState)
	{
		return;
	}

	const EPragmataPlayerRole SelectedRole = PragmataPlayerState->GetLobbySelectedRole();
	const FName NextSkinId = GetNextSelectableSkin(SelectedRole, PragmataPlayerState->GetLobbySelectedSkinId(), Direction);
	if (NextSkinId == NAME_None)
	{
		return;
	}

	PragmataPlayerState->SetLobbySelectedSkinId(NextSkinId);
	PragmataPlayerState->SetLobbyReady(false);
	RefreshLobbyPhaseAndWarning();
}

// ready 요청은 두 player 존재와 선택 충돌을 서버에서 최종 검증
void APragmataLobbyGameMode::RequestReady(APragmataLobbyPlayerController* PlayerController)
{
	APragmataLobbyGameState* LobbyGameState = GetGameState<APragmataLobbyGameState>();
	APragmataPlayerState* PragmataPlayerState = PlayerController ? PlayerController->GetPlayerState<APragmataPlayerState>() : nullptr;
	if (!LobbyGameState || !PragmataPlayerState)
	{
		return;
	}
	if (LobbyGameState->GetLobbyPhase() == EPragmataLobbyPhase::StartingGame)
	{
		return;
	}
	if (PragmataPlayerState->GetLobbyPlayerIndex() < 1 || PragmataPlayerState->GetLobbyPlayerIndex() > 2)
	{
		LobbyGameState->SetWarningMessage(TEXT("로비가 가득 찼습니다"));
		return;
	}

	if (!LobbyGameState->HasEnoughPlayersForSelection())
	{
		LobbyGameState->SetWarningMessage(TEXT("두 번째 플레이어를 기다리는 중입니다"));
		return;
	}

	if (LobbyGameState->HasSelectionConflict())
	{
		LobbyGameState->SetWarningMessage(TEXT("두 플레이어가 같은 캐릭터를 선택할 수 없습니다"));
		PragmataPlayerState->SetLobbyReady(false);
		return;
	}

	EnsureValidSkinSelection(PragmataPlayerState);
	PragmataPlayerState->SetLobbyReady(true);
	RefreshLobbyPhaseAndWarning();
}

// host start는 AssignedRole 확정과 session handoff 저장을 완료한 뒤 Demo로 travel
void APragmataLobbyGameMode::RequestStartGame(APragmataLobbyPlayerController* PlayerController)
{
	APragmataLobbyGameState* LobbyGameState = GetGameState<APragmataLobbyGameState>();
	APragmataPlayerState* RequestingPlayerState = PlayerController ? PlayerController->GetPlayerState<APragmataPlayerState>() : nullptr;
	if (!LobbyGameState || !RequestingPlayerState)
	{
		return;
	}
	if (LobbyGameState->GetLobbyPhase() == EPragmataLobbyPhase::StartingGame)
	{
		return;
	}

	if (!IsHostLobbyPlayer(RequestingPlayerState))
	{
		LobbyGameState->SetWarningMessage(TEXT("1번 플레이어만 게임을 시작할 수 있습니다"));
		return;
	}

	if (!LobbyGameState->CanStartGame())
	{
		LobbyGameState->SetWarningMessage(TEXT("두 플레이어가 서로 다른 캐릭터로 준비해야 합니다"));
		return;
	}

	APragmataPlayerState* PlayerOneState = nullptr;
	APragmataPlayerState* PlayerTwoState = nullptr;
	if (!FindLobbyPlayers(PlayerOneState, PlayerTwoState))
	{
		LobbyGameState->SetWarningMessage(TEXT("로비 플레이어 2명이 필요합니다"));
		return;
	}

	if (PlayerOneState->GetLobbySelectedRole() == PlayerTwoState->GetLobbySelectedRole())
	{
		LobbyGameState->SetWarningMessage(TEXT("두 플레이어는 서로 다른 캐릭터를 선택해야 합니다"));
		PlayerOneState->SetLobbyReady(false);
		PlayerTwoState->SetLobbyReady(false);
		return;
	}

	for (APragmataPlayerState* PragmataPlayerState : { PlayerOneState, PlayerTwoState })
	{
		EnsureValidSkinSelection(PragmataPlayerState);
		PragmataPlayerState->SetAssignedRole(PragmataPlayerState->GetLobbySelectedRole());
	}

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UPragmataMultiplayerSessionSubsystem* SessionSubsystem = GameInstance->GetSubsystem<UPragmataMultiplayerSessionSubsystem>())
		{
			SessionSubsystem->ClearLobbyRoleHandoff();
			for (APragmataPlayerState* PragmataPlayerState : { PlayerOneState, PlayerTwoState })
			{
				SessionSubsystem->RegisterLobbyRoleHandoff(
					PragmataPlayerState->GetPlayerId(),
					PragmataPlayerState->GetLobbyPlayerIndex(),
					PragmataPlayerState->GetLobbySelectedRole(),
					PragmataPlayerState->GetLobbySelectedSkinId());
			}
			SessionSubsystem->SetSessionAdvertisingEnabled(false);
		}
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		LobbyGameState->SetWarningMessage(TEXT("게임을 시작할 수 없습니다"));
		return;
	}

	LobbyGameState->SetWarningMessage(FString());
	// Lobby 선택 역할은 seamless travel PlayerState copy로 gameplay GameMode에 전달
	const UPragmataGameFlowConfig* GameFlowConfig = UPragmataGameFlowConfig::Get(this);
	const FString GameMapPath = GameFlowConfig ? GameFlowConfig->GetLobbyGameMapPath() : FString();
	const FString GameplayGameModePath = GameFlowConfig ? GameFlowConfig->GetGameplayGameModePath() : FString();
	if (GameMapPath.IsEmpty() || GameplayGameModePath.IsEmpty())
	{
		LobbyGameState->SetWarningMessage(TEXT("게임 흐름 설정을 확인하세요"));
		UE_LOG(LogTemp, Warning, TEXT("[PragmataLobbyGameMode::RequestStartGame] Lobby game map or gameplay GameMode is empty"));
		return;
	}

	PendingGameplayTravelURL = PragmataSessionUtils::BuildGameTravelURL(GameMapPath, ExpectedRoomCode, GameplayGameModePath);
	LobbyGameState->SetLobbyPhase(EPragmataLobbyPhase::StartingGame);
	World->GetTimerManager().ClearTimer(StartGameTravelTimerHandle);
	// StartingGame phase가 양쪽 client UI에 복제될 시간을 확보한 뒤 travel
	World->GetTimerManager().SetTimer(StartGameTravelTimerHandle, this, &ThisClass::TravelToGameplayMap, StartGameTravelDelay, false);
}

// 비어 있는 1P/2P slot을 찾아 lobby index를 안정적으로 배정
int32 APragmataLobbyGameMode::ResolveNextLobbyPlayerIndex() const
{
	bool bHasPlayerOne = false;
	bool bHasPlayerTwo = false;

	if (const APragmataLobbyGameState* LobbyGameState = GetGameState<APragmataLobbyGameState>())
	{
		for (APlayerState* PlayerState : LobbyGameState->PlayerArray)
		{
			const APragmataPlayerState* PragmataPlayerState = Cast<APragmataPlayerState>(PlayerState);
			if (!PragmataPlayerState)
			{
				continue;
			}

			bHasPlayerOne |= PragmataPlayerState->GetLobbyPlayerIndex() == 1;
			bHasPlayerTwo |= PragmataPlayerState->GetLobbyPlayerIndex() == 2;
		}
	}

	if (!bHasPlayerOne)
	{
		return 1;
	}

	return bHasPlayerTwo ? 0 : 2;
}

// 첫 player는 Hugh, 두 번째 player는 Diana를 기본 선택값으로 사용
EPragmataPlayerRole APragmataLobbyGameMode::GetDefaultRoleForLobbyIndex(int32 LobbyPlayerIndex) const
{
	return LobbyPlayerIndex == 2 ? EPragmataPlayerRole::Diana : EPragmataPlayerRole::Hugh;
}

// v1은 Hugh/Diana 두 role만 토글하므로 direction은 none 상태의 첫 선택에만 사용
EPragmataPlayerRole APragmataLobbyGameMode::GetNextSelectableRole(EPragmataPlayerRole CurrentRole, int32 Direction) const
{
	if (CurrentRole != EPragmataPlayerRole::Hugh && CurrentRole != EPragmataPlayerRole::Diana)
	{
		return Direction < 0 ? EPragmataPlayerRole::Diana : EPragmataPlayerRole::Hugh;
	}

	return CurrentRole == EPragmataPlayerRole::Hugh ? EPragmataPlayerRole::Diana : EPragmataPlayerRole::Hugh;
}

// role별 skin 기본값은 DataAsset 또는 C++ fallback 정의를 사용
FName APragmataLobbyGameMode::GetDefaultSkinForRole(EPragmataPlayerRole PlayerRole)
{
	if (ULobbyCharacterSkinSet* SkinSet = LoadLobbySkinSet())
	{
		return SkinSet->ResolveDefaultSkinId(PlayerRole);
	}

	return NAME_None;
}

// Diana는 현재 단일 skin만 있으므로 같은 id가 되돌아옴
FName APragmataLobbyGameMode::GetNextSelectableSkin(EPragmataPlayerRole PlayerRole, FName CurrentSkinId, int32 Direction)
{
	if (ULobbyCharacterSkinSet* SkinSet = LoadLobbySkinSet())
	{
		return SkinSet->GetAdjacentSkinId(PlayerRole, CurrentSkinId, Direction);
	}

	return NAME_None;
}

bool APragmataLobbyGameMode::IsSkinValidForRole(EPragmataPlayerRole PlayerRole, FName SkinId)
{
	if (ULobbyCharacterSkinSet* SkinSet = LoadLobbySkinSet())
	{
		return SkinSet->IsValidSkin(PlayerRole, SkinId);
	}

	return false;
}

// role 변경, ready, travel 직전에는 잘못된 skin id를 기본값으로 복구
void APragmataLobbyGameMode::EnsureValidSkinSelection(APragmataPlayerState* PragmataPlayerState)
{
	if (!PragmataPlayerState)
	{
		return;
	}

	const EPragmataPlayerRole SelectedRole = PragmataPlayerState->GetLobbySelectedRole();
	const FName SkinId = PragmataPlayerState->GetLobbySelectedSkinId();
	if (!IsSkinValidForRole(SelectedRole, SkinId))
	{
		PragmataPlayerState->SetLobbySelectedSkinId(GetDefaultSkinForRole(SelectedRole));
	}
}

// URL option은 하위 호환용 preferred role이며 서버 lobby가 최종 검증
EPragmataPlayerRole APragmataLobbyGameMode::ParsePreferredRoleOption(const FString& Options) const
{
	const FString PreferredRole = UGameplayStatics::ParseOption(Options, TEXT("PreferredRole"));
	if (PreferredRole.Equals(TEXT("Hugh"), ESearchCase::IgnoreCase))
	{
		return EPragmataPlayerRole::Hugh;
	}
	if (PreferredRole.Equals(TEXT("Diana"), ESearchCase::IgnoreCase))
	{
		return EPragmataPlayerRole::Diana;
	}

	return EPragmataPlayerRole::None;
}

// 유효한 lobby slot을 가진 PlayerState만 player count로 인정
int32 APragmataLobbyGameMode::GetLobbyPlayerCount() const
{
	int32 LobbyPlayerCount = 0;
	if (const APragmataLobbyGameState* LobbyGameState = GetGameState<APragmataLobbyGameState>())
	{
		for (APlayerState* PlayerState : LobbyGameState->PlayerArray)
		{
			const APragmataPlayerState* PragmataPlayerState = Cast<APragmataPlayerState>(PlayerState);
			if (PragmataPlayerState && PragmataPlayerState->GetLobbyPlayerIndex() > 0)
			{
				++LobbyPlayerCount;
			}
		}
	}

	return LobbyPlayerCount;
}

// 입장 직후 lobby index, 기본 선택, ready 초기값을 서버에서 확정
void APragmataLobbyGameMode::AssignLobbyDefaults(APlayerController* NewPlayer)
{
	APragmataPlayerState* PragmataPlayerState = NewPlayer ? NewPlayer->GetPlayerState<APragmataPlayerState>() : nullptr;
	if (!PragmataPlayerState || PragmataPlayerState->GetLobbyPlayerIndex() > 0)
	{
		return;
	}

	const int32 LobbyPlayerIndex = ResolveNextLobbyPlayerIndex();
	if (LobbyPlayerIndex < 1 || LobbyPlayerIndex > 2)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PragmataLobbyGameMode::AssignLobbyDefaults] 로비가 가득 찼습니다"));
		PragmataPlayerState->SetLobbyPlayerIndex(0);
		PragmataPlayerState->SetLobbySelectedRole(EPragmataPlayerRole::None);
		PragmataPlayerState->SetLobbySelectedSkinId(NAME_None);
		PragmataPlayerState->SetLobbyReady(false);
		return;
	}

	PragmataPlayerState->SetLobbyPlayerIndex(LobbyPlayerIndex);
	const EPragmataPlayerRole PreferredRole = PragmataPlayerState->GetLobbySelectedRole();
	const EPragmataPlayerRole SelectedRole =
		PreferredRole == EPragmataPlayerRole::Hugh || PreferredRole == EPragmataPlayerRole::Diana
			? PreferredRole
			: GetDefaultRoleForLobbyIndex(LobbyPlayerIndex);
	PragmataPlayerState->SetLobbySelectedRole(SelectedRole);
	PragmataPlayerState->SetLobbySelectedSkinId(GetDefaultSkinForRole(SelectedRole));
	PragmataPlayerState->SetLobbyReady(false);
}

// PlayerState 배열을 기준으로 현재 lobby phase와 warning을 재계산
void APragmataLobbyGameMode::RefreshLobbyPhaseAndWarning()
{
	APragmataLobbyGameState* LobbyGameState = GetGameState<APragmataLobbyGameState>();
	if (!LobbyGameState)
	{
		return;
	}
	if (LobbyGameState->GetLobbyPhase() == EPragmataLobbyPhase::StartingGame)
	{
		return;
	}
	if (LobbyGameState->GetLobbyPhase() == EPragmataLobbyPhase::PlayerJoining && GetLobbyPlayerCount() >= 2)
	{
		return;
	}

	if (ExpectedRoomCode.IsEmpty())
	{
		LobbyGameState->SetLobbyPhase(EPragmataLobbyPhase::SessionSetup);
		LobbyGameState->SetWarningMessage(FString());
		return;
	}

	if (GetLobbyPlayerCount() < 2)
	{
		LobbyGameState->SetLobbyPhase(EPragmataLobbyPhase::WaitingForPlayer);
		LobbyGameState->SetWarningMessage(FString());
		return;
	}

	LobbyGameState->SetLobbyPhase(LobbyGameState->CanStartGame() ? EPragmataLobbyPhase::ReadyToStart : EPragmataLobbyPhase::CharacterSelect);
	LobbyGameState->SetWarningMessage(LobbyGameState->HasSelectionConflict() ? TEXT("두 플레이어가 같은 캐릭터를 선택할 수 없습니다") : FString());
}

// 두 번째 player가 들어온 순간을 양쪽 UI에 먼저 보여준 뒤 선택 화면으로 전환
void APragmataLobbyGameMode::BeginPlayerJoiningPhase()
{
	APragmataLobbyGameState* LobbyGameState = GetGameState<APragmataLobbyGameState>();
	UWorld* World = GetWorld();
	if (!LobbyGameState || !World)
	{
		RefreshLobbyPhaseAndWarning();
		return;
	}

	LobbyGameState->SetWarningMessage(FString());
	LobbyGameState->SetLobbyPhase(EPragmataLobbyPhase::PlayerJoining);
	World->GetTimerManager().ClearTimer(PlayerJoiningTimerHandle);
	World->GetTimerManager().SetTimer(PlayerJoiningTimerHandle, this, &ThisClass::CompletePlayerJoiningPhase, PlayerJoiningRevealDelay, false);
}

// joining 안내와 black reveal 시간을 둔 뒤 실제 character select phase로 진입
void APragmataLobbyGameMode::CompletePlayerJoiningPhase()
{
	APragmataLobbyGameState* LobbyGameState = GetGameState<APragmataLobbyGameState>();
	if (!LobbyGameState)
	{
		return;
	}

	if (GetLobbyPlayerCount() < 2)
	{
		RefreshLobbyPhaseAndWarning();
		return;
	}

	LobbyGameState->SetLobbyPhase(LobbyGameState->CanStartGame() ? EPragmataLobbyPhase::ReadyToStart : EPragmataLobbyPhase::CharacterSelect);
	LobbyGameState->SetWarningMessage(LobbyGameState->HasSelectionConflict() ? TEXT("두 플레이어가 같은 캐릭터를 선택할 수 없습니다") : FString());
}

// 1P만 start game 권한을 갖는 lobby host로 취급
bool APragmataLobbyGameMode::IsHostLobbyPlayer(const APragmataPlayerState* PlayerState) const
{
	return PlayerState && PlayerState->GetLobbyPlayerIndex() == 1;
}

// 기본 맵 첫 입장만 code 없이 허용하고 이후 joiner는 code 일치를 요구
bool APragmataLobbyGameMode::IsRoomCodeValidForLogin(const FString& Options) const
{
	const FString ProvidedRoomCode = PragmataSessionUtils::NormalizeRoomCode(UGameplayStatics::ParseOption(Options, TEXT("RoomCode")));
	if (ExpectedRoomCode.IsEmpty())
	{
		return ProvidedRoomCode.IsEmpty() && GetLobbyPlayerCount() == 0;
	}

	if (ProvidedRoomCode.IsEmpty() && GetLobbyPlayerCount() == 0)
	{
		return true;
	}

	return ProvidedRoomCode.Equals(ExpectedRoomCode, ESearchCase::CaseSensitive);
}

// start game 직전 1P/2P PlayerState를 안정적으로 회수
bool APragmataLobbyGameMode::FindLobbyPlayers(APragmataPlayerState*& OutPlayerOneState, APragmataPlayerState*& OutPlayerTwoState) const
{
	OutPlayerOneState = nullptr;
	OutPlayerTwoState = nullptr;

	const APragmataLobbyGameState* LobbyGameState = GetGameState<APragmataLobbyGameState>();
	if (!LobbyGameState)
	{
		return false;
	}

	for (APlayerState* PlayerState : LobbyGameState->PlayerArray)
	{
		APragmataPlayerState* PragmataPlayerState = Cast<APragmataPlayerState>(PlayerState);
		if (!PragmataPlayerState)
		{
			continue;
		}

		if (PragmataPlayerState->GetLobbyPlayerIndex() == 1)
		{
			OutPlayerOneState = PragmataPlayerState;
		}
		else if (PragmataPlayerState->GetLobbyPlayerIndex() == 2)
		{
			OutPlayerTwoState = PragmataPlayerState;
		}
	}

	return OutPlayerOneState && OutPlayerTwoState;
}

// start loading UI 표시 후 저장해 둔 gameplay URL로 이동
void APragmataLobbyGameMode::TravelToGameplayMap()
{
	UWorld* World = GetWorld();
	if (!World || PendingGameplayTravelURL.IsEmpty())
	{
		if (APragmataLobbyGameState* LobbyGameState = GetGameState<APragmataLobbyGameState>())
		{
			LobbyGameState->SetWarningMessage(TEXT("게임을 시작할 수 없습니다"));
		}
		return;
	}

	const FString TravelURL = PendingGameplayTravelURL;
	PendingGameplayTravelURL.Reset();
	World->ServerTravel(TravelURL);
}

// DataAsset이 아직 없거나 stale editor DLL이면 fallback 객체로 같은 skin 정의를 제공
ULobbyCharacterSkinSet* APragmataLobbyGameMode::LoadLobbySkinSet()
{
	if (LoadedLobbySkinSet)
	{
		return LoadedLobbySkinSet;
	}

	if (!LobbySkinSet.IsNull())
	{
		LoadedLobbySkinSet = LobbySkinSet.LoadSynchronous();
	}

	if (!LoadedLobbySkinSet)
	{
		LoadedLobbySkinSet = NewObject<ULobbyCharacterSkinSet>(this, TEXT("RuntimeLobbyCharacterSkinSet"));
	}

	return LoadedLobbySkinSet;
}
