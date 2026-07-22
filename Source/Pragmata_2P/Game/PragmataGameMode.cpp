#include "Game/PragmataGameMode.h"

#include "Characters/HughCharacter.h"
#include "Components/PragmataHealthComponent.h"
#include "Engine/GameInstance.h"
#include "Game/PragmataGameState.h"
#include "Game/PragmataMultiplayerSessionSubsystem.h"
#include "Game/PragmataPlayerController.h"
#include "Game/PragmataPlayerState.h"
#include "Game/PragmataSessionUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Pawns/DianaOperatorPawn.h"
#include "Props/CheckPoint.h"
#include "GameFramework/CharacterMovementComponent.h"

// gameplay map은 role별 pawn을 직접 고르므로 기본 pawn class를 비워 둠
APragmataGameMode::APragmataGameMode()
{
	GameStateClass = APragmataGameState::StaticClass();
	PlayerStateClass = APragmataPlayerState::StaticClass();
	PlayerControllerClass = APragmataPlayerController::StaticClass();
	DefaultPawnClass = nullptr;
}

void APragmataGameMode::SetActiveCheckpoint(ACheckPoint* Checkpoint)
{
	if (HasAuthority() && IsValid(Checkpoint))
	{
		ActiveCheckpoint = Checkpoint;
	}
}

void APragmataGameMode::ScheduleTeamRespawn()
{
	if (!HasAuthority() || GetWorldTimerManager().IsTimerActive(TeamRespawnTimerHandle))
	{
		return;
	}

	GetWorldTimerManager().SetTimer(TeamRespawnTimerHandle, this, &ThisClass::ExecuteTeamRespawn, 10.f, false);
}

void APragmataGameMode::OpenCheckpointUpgradeSession()
{
	// checkpoint가 연 세션만 stat 변경과 양 player UI lock을 소유
	if (!HasAuthority() || bCheckpointUpgradeSessionActive)
	{
		return;
	}

	AHughCharacter* HughCharacter = SpawnedHughCharacter;
	if (!HughCharacter)
	{
		return;
	}
	bCheckpointUpgradeSessionActive = true;

	if (UPragmataHealthComponent* HealthComponent = HughCharacter->GetHealthComponent())
	{
		HealthComponent->SetDamageInvulnerable(true);
	}

	if (APragmataGameState* PragmataGameState = GetGameState<APragmataGameState>())
	{
		PragmataGameState->SetDianaActionsLocked(true);
	}

	if (APragmataPlayerController* HughController = Cast<APragmataPlayerController>(HughCharacter->GetController()))
	{
		HughController->Client_SetCheckpointUpgradeMenuOpen(true, true);
	}
	if (ADianaOperatorPawn* DianaPawn = SpawnedDianaOperatorPawn)
	{
		if (APragmataPlayerController* DianaController = Cast<APragmataPlayerController>(DianaPawn->GetController()))
		{
			DianaController->Client_SetCheckpointUpgradeMenuOpen(true, false);
		}
	}
}

void APragmataGameMode::CloseCheckpointUpgradeSession()
{
	// close/abnormal cleanup가 중복되어도 invincibility와 input을 한 번만 복구
	if (!HasAuthority() || !bCheckpointUpgradeSessionActive)
	{
		return;
	}
	bCheckpointUpgradeSessionActive = false;

	if (AHughCharacter* HughCharacter = SpawnedHughCharacter)
	{
		if (UPragmataHealthComponent* HealthComponent = HughCharacter->GetHealthComponent())
		{
			HealthComponent->SetDamageInvulnerable(false);
		}
		if (APragmataPlayerController* HughController = Cast<APragmataPlayerController>(HughCharacter->GetController()))
		{
			HughController->Client_SetCheckpointUpgradeMenuOpen(false, false);
		}
	}
	if (APragmataGameState* PragmataGameState = GetGameState<APragmataGameState>())
	{
		PragmataGameState->SetDianaActionsLocked(false);
	}
	if (ADianaOperatorPawn* DianaPawn = SpawnedDianaOperatorPawn)
	{
		if (APragmataPlayerController* DianaController = Cast<APragmataPlayerController>(DianaPawn->GetController()))
		{
			DianaController->Client_SetCheckpointUpgradeMenuOpen(false, false);
		}
	}
}

void APragmataGameMode::ExecuteTeamRespawn()
{
	// death respawn은 UI lock과 invincibility를 남기지 않고 시작
	CloseCheckpointUpgradeSession();
	if (APragmataGameState* PragmataGameState = GetGameState<APragmataGameState>())
	{
		// upgrade session이 없던 death도 forced cue action lock을 명시적으로 해제
		PragmataGameState->SetDianaActionsLocked(false);
	}
	AHughCharacter* HughCharacter = SpawnedHughCharacter;
	ADianaOperatorPawn* DianaPawn = SpawnedDianaOperatorPawn;
	if (!HughCharacter)
	{
		return;
	}

	FTransform RespawnTransform = HughCharacter->GetActorTransform();
	if (ActiveCheckpoint.IsValid())
	{
		RespawnTransform = ActiveCheckpoint->GetRespawnTransform();
	}
	else if (AActor* PlayerStart = FindPlayerStart(HughCharacter->GetController()))
	{
		RespawnTransform = PlayerStart->GetActorTransform();
	}
	HughCharacter->SetActorLocationAndRotation(RespawnTransform.GetLocation(), RespawnTransform.GetRotation());
	HughCharacter->ResetForRespawn();
	HughCharacter->ReplenishCheckpointResources();
	HughCharacter->SetDianaAvatarDeathAttachment(false);

	if (DianaPawn)
	{
		// Diana pawn과 view rotation을 같은 checkpoint anchor로 복구
		DianaPawn->SetActorLocationAndRotation(RespawnTransform.GetLocation(), RespawnTransform.GetRotation());
		if (AController* DianaController = DianaPawn->GetController())
		{
			DianaController->SetControlRotation(RespawnTransform.Rotator());
			// remote Diana owning client도 server와 같은 checkpoint view rotation으로 복구
			DianaController->ClientSetRotation(RespawnTransform.Rotator(), true);
		}
		DianaPawn->SetAimingState(false);
		DianaPawn->SetOperatorMode(EDianaOperatorMode::Roaming);
	}
}

// travel URL의 room code를 gameplay login 검증과 GameState 표시의 기준값으로 저장
void APragmataGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	ExpectedRoomCode = PragmataSessionUtils::NormalizeRoomCode(UGameplayStatics::ParseOption(Options, TEXT("RoomCode")));
}

// 클라이언트 UI가 gameplay map에서도 같은 room code를 읽도록 GameState에 반영
void APragmataGameMode::InitGameState()
{
	Super::InitGameState();

	if (APragmataGameState* PragmataGameState = GetGameState<APragmataGameState>())
	{
		PragmataGameState->SetRoomCode(ExpectedRoomCode);
	}
}

// PlayerController 생성 전에 room code와 role 충돌을 걸러 잘못된 spawn을 막음
void APragmataGameMode::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
	if (!ErrorMessage.IsEmpty())
	{
		return;
	}

	// 초대 코드와 role은 PlayerController 생성 전에 검증
	ValidateLoginOptions(Options, ErrorMessage);
}

// PlayerController가 생긴 직후 gameplay role을 확정해 spawn 경로가 한 값만 보게 함
FString APragmataGameMode::InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId, const FString& Options, const FString& Portal)
{
	const FString ErrorMessage = Super::InitNewPlayer(NewPlayerController, UniqueId, Options, Portal);
	if (!ErrorMessage.IsEmpty())
	{
		return ErrorMessage;
	}

	FString RoleErrorMessage;
	if (EnsureAssignedRoleForController(NewPlayerController, Options, RoleErrorMessage) == EPragmataPlayerRole::None)
	{
		if (ShouldDeferRoleAssignmentFailure(RoleErrorMessage))
		{
			UE_LOG(LogTemp, Warning, TEXT("[PragmataGameMode::InitNewPlayer] Deferring lobby role assignment: %s"), *RoleErrorMessage);
			return FString();
		}

		return RoleErrorMessage.IsEmpty() ? TEXT("pragmata_invalid_role") : RoleErrorMessage;
	}

	return FString();
}

// pawn spawn 직전에 최종 role과 pawn class를 로그로 남겨 PIE host/client 분기를 추적
void APragmataGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	FString RoleErrorMessage;
	if (EnsureAssignedRoleForController(NewPlayer, OptionsString, RoleErrorMessage) == EPragmataPlayerRole::None)
	{
		const APragmataPlayerState* PragmataPlayerState = NewPlayer ? NewPlayer->GetPlayerState<APragmataPlayerState>() : nullptr;
		UE_LOG(LogTemp, Warning, TEXT("[PragmataGameMode::HandleStartingNewPlayer] Missing assigned role: %s Controller=%s PlayerId=%d LobbyIndex=%d Map=%s RoomCode=%s"),
			*RoleErrorMessage,
			*GetNameSafe(NewPlayer),
			PragmataPlayerState ? PragmataPlayerState->GetPlayerId() : INDEX_NONE,
			PragmataPlayerState ? PragmataPlayerState->GetLobbyPlayerIndex() : 0,
			GetWorld() ? *GetWorld()->GetMapName() : TEXT("None"),
			*ExpectedRoomCode);
		return;
	}

	const APragmataPlayerState* PragmataPlayerState = NewPlayer ? NewPlayer->GetPlayerState<APragmataPlayerState>() : nullptr;
	UE_LOG(LogTemp, Log, TEXT("[PragmataGameMode::HandleStartingNewPlayer] Starting Controller=%s AssignedRole=%d PlayerId=%d LobbyIndex=%d PawnClass=%s"),
		*GetNameSafe(NewPlayer),
		static_cast<int32>(PragmataPlayerState ? PragmataPlayerState->GetAssignedRole() : EPragmataPlayerRole::None),
		PragmataPlayerState ? PragmataPlayerState->GetPlayerId() : INDEX_NONE,
		PragmataPlayerState ? PragmataPlayerState->GetLobbyPlayerIndex() : 0,
		*GetNameSafe(GetDefaultPawnClassForController(NewPlayer)));

	Super::HandleStartingNewPlayer_Implementation(NewPlayer);
}

// gameplay spawn은 PlayerState에 확정된 AssignedRole만 기준으로 pawn class 선택
UClass* APragmataGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
	const EPragmataPlayerRole PlayerRole = GetAssignedRoleForController(InController);
	if (PlayerRole == EPragmataPlayerRole::Hugh)
	{
		UE_LOG(LogTemp, Log, TEXT("[PragmataGameMode::GetDefaultPawnClassForController] Hugh pawn class: %s"), *GetNameSafe(HughCharacterClass));
		return HughCharacterClass;
	}

	if (PlayerRole == EPragmataPlayerRole::Diana)
	{
		UE_LOG(LogTemp, Log, TEXT("[PragmataGameMode::GetDefaultPawnClassForController] Diana pawn class: %s"), *GetNameSafe(DianaOperatorPawnClass));
		return DianaOperatorPawnClass;
	}

	UE_LOG(LogTemp, Warning, TEXT("[PragmataGameMode::GetDefaultPawnClassForController] No gameplay role; pawn spawn blocked"));
	return nullptr;
}

// Hugh/Diana가 같은 PlayerStart를 쓸 때 겹치지 않도록 역할별 보정 위치로 spawn
void APragmataGameMode::RestartPlayerAtPlayerStart(AController* NewPlayer, AActor* StartSpot)
{
	if (NewPlayer && StartSpot)
	{
		FTransform SpawnTransform = StartSpot->GetActorTransform();
		SpawnTransform.AddToTranslation(GetPlayerStartOffsetForRole(GetAssignedRoleForController(NewPlayer), StartSpot));
		RestartPlayerAtTransform(NewPlayer, SpawnTransform);
		return;
	}

	Super::RestartPlayerAtPlayerStart(NewPlayer, StartSpot);
}

// possess가 끝난 pawn을 GameState에 등록해 반대편 클라이언트가 role pawn을 찾게 함
void APragmataGameMode::FinishRestartPlayer(AController* NewPlayer, const FRotator& StartRotation)
{
	Super::FinishRestartPlayer(NewPlayer, StartRotation);

	RegisterSpawnedPawnForRole(NewPlayer);
}

// 연결 종료 시 해당 role의 connected flag와 pawn pointer를 비워 재접속을 허용
void APragmataGameMode::Logout(AController* Exiting)
{
	// 어느 player가 나가도 shared checkpoint session을 즉시 해제
	CloseCheckpointUpgradeSession();
	ClearRoleState(GetAssignedRoleForController(Exiting));

	Super::Logout(Exiting);
}

// URL option에서 명시 role을 파싱해 lobby 없는 smoke 흐름도 같은 검증을 통과
EPragmataPlayerRole APragmataGameMode::ParseRequestedRole(const FString& Options) const
{
	const FString RoleString = UGameplayStatics::ParseOption(Options, TEXT("PragmataRole"));
	if (RoleString.Equals(TEXT("Hugh"), ESearchCase::IgnoreCase))
	{
		return EPragmataPlayerRole::Hugh;
	}

	if (RoleString.Equals(TEXT("Diana"), ESearchCase::IgnoreCase))
	{
		return EPragmataPlayerRole::Diana;
	}

	return EPragmataPlayerRole::None;
}

// 명시 role, map option, 빈 방 fallback 순서로 접속 요청 role을 해석
EPragmataPlayerRole APragmataGameMode::ResolveRequestedRole(const FString& Options) const
{
	if (const EPragmataPlayerRole LoginRole = ParseRequestedRole(Options); LoginRole != EPragmataPlayerRole::None)
	{
		return LoginRole;
	}

	// PIE smoke와 listen host는 map URL option을 fallback으로 사용
	if (const EPragmataPlayerRole TravelRole = ParseRequestedRole(OptionsString); TravelRole != EPragmataPlayerRole::None)
	{
		return TravelRole;
	}

	if (ExpectedRoomCode.IsEmpty())
	{
		if (IsRoleAvailable(EPragmataPlayerRole::Hugh))
		{
			return EPragmataPlayerRole::Hugh;
		}

		if (IsRoleAvailable(EPragmataPlayerRole::Diana))
		{
			return EPragmataPlayerRole::Diana;
		}
	}

	return EPragmataPlayerRole::None;
}

// AssignedRole 확정 전 단계에서 lobby 선택, handoff, URL fallback을 한 곳에서 소비
EPragmataPlayerRole APragmataGameMode::ResolveGameplayRoleForController(AController* Controller, const FString& Options)
{
	if (const APragmataPlayerState* PragmataPlayerState = Controller ? Controller->GetPlayerState<APragmataPlayerState>() : nullptr)
	{
		if (PragmataPlayerState->GetAssignedRole() != EPragmataPlayerRole::None)
		{
			return PragmataPlayerState->GetAssignedRole();
		}

		if (PragmataPlayerState->GetLobbySelectedRole() != EPragmataPlayerRole::None)
		{
			// gameplay 진입 전 lobby 선택값 소비
			return PragmataPlayerState->GetLobbySelectedRole();
		}
	}

	if (const APragmataPlayerController* PragmataPlayerController = Cast<APragmataPlayerController>(Controller))
	{
		if (PragmataPlayerController->GetAssignedRole() != EPragmataPlayerRole::None)
		{
			return PragmataPlayerController->GetAssignedRole();
		}
	}

	if (const EPragmataPlayerRole HandoffRole = ResolveLobbyHandoffRoleForController(Controller); HandoffRole != EPragmataPlayerRole::None)
	{
		if (IsRoleAvailable(HandoffRole, Controller))
		{
			// seamless travel handoff 역할 소비
			return HandoffRole;
		}
	}

	if (const EPragmataPlayerRole RequestedRole = ResolveRequestedRole(Options); RequestedRole != EPragmataPlayerRole::None)
	{
		return RequestedRole;
	}

	return EPragmataPlayerRole::None;
}

// seamless travel 중 PlayerState 식별자가 흔들려도 lobby handoff role을 회수
EPragmataPlayerRole APragmataGameMode::ResolveLobbyHandoffRoleForController(AController* Controller)
{
	APragmataPlayerState* PragmataPlayerState = Controller ? Controller->GetPlayerState<APragmataPlayerState>() : nullptr;
	if (!PragmataPlayerState)
	{
		return EPragmataPlayerRole::None;
	}

	UGameInstance* GameInstance = GetGameInstance();
	UPragmataMultiplayerSessionSubsystem* SessionSubsystem = GameInstance ? GameInstance->GetSubsystem<UPragmataMultiplayerSessionSubsystem>() : nullptr;
	if (!SessionSubsystem)
	{
		return EPragmataPlayerRole::None;
	}

	EPragmataPlayerRole HandoffRole = EPragmataPlayerRole::None;
	FName HandoffSkinId = NAME_None;
	if (SessionSubsystem->FindLobbyRoleHandoff(PragmataPlayerState->GetPlayerId(), PragmataPlayerState->GetLobbyPlayerIndex(), HandoffRole, &HandoffSkinId))
	{
		if (HandoffSkinId != NAME_None)
		{
			PragmataPlayerState->SetLobbySelectedSkinId(HandoffSkinId);
		}
		return HandoffRole;
	}

	if (!ExpectedRoomCode.IsEmpty())
	{
		while (SessionSubsystem->ConsumeNextLobbyRoleHandoff(HandoffRole, &HandoffSkinId))
		{
			if (IsRoleAvailable(HandoffRole, Controller))
			{
				if (HandoffSkinId != NAME_None)
				{
					PragmataPlayerState->SetLobbySelectedSkinId(HandoffSkinId);
				}
				return HandoffRole;
			}
		}
	}

	return EPragmataPlayerRole::None;
}

// spawn/possess 전에 role, room, 중복 점유, pawn class를 모두 검증하고 확정
EPragmataPlayerRole APragmataGameMode::EnsureAssignedRoleForController(APlayerController* PlayerController, const FString& Options, FString& OutErrorMessage)
{
	const EPragmataPlayerRole PlayerRole = ResolveGameplayRoleForController(PlayerController, Options);
	if (PlayerRole == EPragmataPlayerRole::None)
	{
		OutErrorMessage = TEXT("pragmata_invalid_role");
		return EPragmataPlayerRole::None;
	}

	if (!IsRoomCodeValidForLogin(PlayerRole, Options))
	{
		OutErrorMessage = TEXT("pragmata_invalid_room_code");
		return EPragmataPlayerRole::None;
	}

	if (!IsRoleAvailable(PlayerRole, PlayerController))
	{
		OutErrorMessage = TEXT("pragmata_role_unavailable");
		return EPragmataPlayerRole::None;
	}

	if (!IsPawnClassConfiguredForRole(PlayerRole, OutErrorMessage))
	{
		return EPragmataPlayerRole::None;
	}

	if (APragmataPlayerState* PragmataPlayerState = PlayerController ? PlayerController->GetPlayerState<APragmataPlayerState>() : nullptr)
	{
		// spawn/possess 전 최종 역할 확정
		PragmataPlayerState->SetAssignedRole(PlayerRole);
		if (PragmataPlayerState->GetLobbySelectedRole() == EPragmataPlayerRole::None)
		{
			PragmataPlayerState->SetLobbySelectedRole(PlayerRole);
		}
	}

	if (APragmataPlayerController* PragmataPlayerController = Cast<APragmataPlayerController>(PlayerController))
	{
		PragmataPlayerController->SetAssignedRole(PlayerRole);
	}

	return PlayerRole;
}

bool APragmataGameMode::ShouldDeferRoleAssignmentFailure(const FString& RoleErrorMessage) const
{
	if (ExpectedRoomCode.IsEmpty())
	{
		return false;
	}

	// Seamless travel can call InitNewPlayer before lobby handoff data has reached PlayerState.
	return RoleErrorMessage.IsEmpty() || RoleErrorMessage == TEXT("pragmata_invalid_role");
}

// 이미 확정된 role만 읽어 spawn 이후 lobby 선택값이 gameplay를 흔들지 않게 함
EPragmataPlayerRole APragmataGameMode::GetAssignedRoleForController(const AController* Controller) const
{
	if (const APragmataPlayerState* PragmataPlayerState = Controller ? Controller->GetPlayerState<APragmataPlayerState>() : nullptr)
	{
		if (PragmataPlayerState->GetAssignedRole() != EPragmataPlayerRole::None)
		{
			return PragmataPlayerState->GetAssignedRole();
		}
	}

	if (const APragmataPlayerController* PragmataPlayerController = Cast<APragmataPlayerController>(Controller))
	{
		if (PragmataPlayerController->GetAssignedRole() != EPragmataPlayerRole::None)
		{
			return PragmataPlayerController->GetAssignedRole();
		}
	}

	return EPragmataPlayerRole::None;
}

// Controller 생성 전 login option이 방 코드와 역할 제약을 만족하는지 방어
bool APragmataGameMode::ValidateLoginOptions(const FString& Options, FString& OutErrorMessage) const
{
	const EPragmataPlayerRole RequestedRole = ResolveRequestedRole(Options);
	if (!IsRoomCodeValidForLogin(RequestedRole, Options))
	{
		OutErrorMessage = TEXT("pragmata_invalid_room_code");
		return false;
	}

	if (RequestedRole == EPragmataPlayerRole::None)
	{
		if (!ExpectedRoomCode.IsEmpty())
		{
			return true;
		}

		OutErrorMessage = TEXT("pragmata_invalid_role");
		return false;
	}

	if (!IsRoleAvailable(RequestedRole))
	{
		OutErrorMessage = TEXT("pragmata_role_unavailable");
		return false;
	}

	if (!IsPawnClassConfiguredForRole(RequestedRole, OutErrorMessage))
	{
		return false;
	}

	return true;
}

// 외부 접속자는 현재 match의 room code와 일치해야 gameplay map에 들어옴
bool APragmataGameMode::IsRoomCodeValidForLogin(EPragmataPlayerRole PlayerRole, const FString& Options) const
{
	const FString ProvidedRoomCode = ResolveRoomCodeForLogin(Options);
	if (ExpectedRoomCode.IsEmpty())
	{
		return ProvidedRoomCode.IsEmpty();
	}

	return ProvidedRoomCode.Equals(ExpectedRoomCode, ESearchCase::CaseSensitive);
}

// login URL과 travel URL 중 실제 비교에 쓸 room code를 정규화
FString APragmataGameMode::ResolveRoomCodeForLogin(const FString& Options) const
{
	const FString LoginRoomCode = PragmataSessionUtils::NormalizeRoomCode(UGameplayStatics::ParseOption(Options, TEXT("RoomCode")));
	if (!LoginRoomCode.IsEmpty())
	{
		return LoginRoomCode;
	}

	if (ParseRequestedRole(Options) != EPragmataPlayerRole::None)
	{
		return FString();
	}

	return PragmataSessionUtils::NormalizeRoomCode(UGameplayStatics::ParseOption(OptionsString, TEXT("RoomCode")));
}

// 같은 role을 두 컨트롤러가 동시에 점유하지 못하게 PlayerState 기준으로 검사
bool APragmataGameMode::IsRoleAvailable(EPragmataPlayerRole PlayerRole, const AController* ControllerToIgnore) const
{
	const APragmataGameState* PragmataGameState = GetGameState<APragmataGameState>();
	if (!PragmataGameState)
	{
		return true;
	}

	const APragmataPlayerState* IgnoredPlayerState = ControllerToIgnore ? ControllerToIgnore->GetPlayerState<APragmataPlayerState>() : nullptr;
	for (APlayerState* PlayerState : PragmataGameState->PlayerArray)
	{
		if (ControllerToIgnore && PlayerState == ControllerToIgnore->GetPlayerState<APlayerState>())
		{
			continue;
		}
		if (PlayerState && PlayerState->IsInactive())
		{
			continue;
		}

		const APragmataPlayerState* PragmataPlayerState = Cast<APragmataPlayerState>(PlayerState);
		if (IgnoredPlayerState && PragmataPlayerState)
		{
			const bool bSamePlayerId = IgnoredPlayerState->GetPlayerId() >= 0 && IgnoredPlayerState->GetPlayerId() == PragmataPlayerState->GetPlayerId();
			const bool bSameLobbyIndex = IgnoredPlayerState->GetLobbyPlayerIndex() > 0 && IgnoredPlayerState->GetLobbyPlayerIndex() == PragmataPlayerState->GetLobbyPlayerIndex();
			if (bSamePlayerId || bSameLobbyIndex)
			{
				continue;
			}
		}

		if (PragmataPlayerState && PragmataPlayerState->GetAssignedRole() == PlayerRole)
		{
			return false;
		}
	}

	return true;
}

// C++ spawn 경로에서 Blueprint pawn class 누락을 조기에 로그 가능한 오류로 변환
bool APragmataGameMode::IsPawnClassConfiguredForRole(EPragmataPlayerRole PlayerRole, FString& OutErrorMessage) const
{
	if (PlayerRole == EPragmataPlayerRole::Hugh && !HughCharacterClass)
	{
		OutErrorMessage = TEXT("pragmata_missing_hugh_class");
		return false;
	}

	if (PlayerRole == EPragmataPlayerRole::Diana && !DianaOperatorPawnClass)
	{
		OutErrorMessage = TEXT("pragmata_missing_diana_class");
		return false;
	}

	return true;
}

// Diana가 Hugh와 같은 PlayerStart에 갇히지 않도록 lateral offset만 적용
FVector APragmataGameMode::GetPlayerStartOffsetForRole(EPragmataPlayerRole PlayerRole, const AActor* StartSpot) const
{
	if (PlayerRole == EPragmataPlayerRole::Diana && StartSpot)
	{
		return StartSpot->GetActorRightVector() * DianaPlayerStartRightOffset;
	}

	return FVector::ZeroVector;
}

// possess된 pawn pointer와 connected flag를 GameState에 확정 반영
void APragmataGameMode::RegisterSpawnedPawnForRole(AController* NewPlayer)
{
	APawn* SpawnedPawn = NewPlayer ? NewPlayer->GetPawn() : nullptr;
	APragmataGameState* PragmataGameState = GetGameState<APragmataGameState>();
	if (!SpawnedPawn || !PragmataGameState)
	{
		return;
	}

	const EPragmataPlayerRole PlayerRole = GetAssignedRoleForController(NewPlayer);

	if (PlayerRole == EPragmataPlayerRole::Hugh)
	{
		// GameState에 Hugh pawn pointer 복제
		SpawnedHughCharacter = Cast<AHughCharacter>(SpawnedPawn);
		if (!SpawnedHughCharacter)
		{
			UE_LOG(LogTemp, Warning, TEXT("[PragmataGameMode::RegisterSpawnedPawnForRole] Hugh role spawned incompatible pawn: %s"), *GetNameSafe(SpawnedPawn));
			return;
		}

		PragmataGameState->SetHughCharacter(SpawnedHughCharacter);
	}
	else if (PlayerRole == EPragmataPlayerRole::Diana)
	{
		// GameState에 Diana pawn pointer 복제
		SpawnedDianaOperatorPawn = Cast<ADianaOperatorPawn>(SpawnedPawn);
		if (!SpawnedDianaOperatorPawn)
		{
			UE_LOG(LogTemp, Warning, TEXT("[PragmataGameMode::RegisterSpawnedPawnForRole] Diana role spawned incompatible pawn: %s"), *GetNameSafe(SpawnedPawn));
			return;
		}

		PragmataGameState->SetDianaOperatorPawn(SpawnedDianaOperatorPawn);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[PragmataGameMode::RegisterSpawnedPawnForRole] Spawned pawn has no gameplay role: %s"), *GetNameSafe(SpawnedPawn));
		return;
	}

	PragmataGameState->SetRoleConnected(PlayerRole, true);
	ApplyGameplaySkinSelections();

	CloseSessionIfBothPlayersConnected();
}

// Hugh 본체와 부착된 Diana avatar mesh는 Hugh pawn 하나에서 함께 적용
void APragmataGameMode::ApplyGameplaySkinSelections()
{
	if (!SpawnedHughCharacter)
	{
		return;
	}

	FName HughSkinId = NAME_None;
	FName DianaSkinId = NAME_None;
	if (const APragmataGameState* PragmataGameState = GetGameState<APragmataGameState>())
	{
		for (APlayerState* PlayerState : PragmataGameState->PlayerArray)
		{
			const APragmataPlayerState* PragmataPlayerState = Cast<APragmataPlayerState>(PlayerState);
			if (!PragmataPlayerState)
			{
				continue;
			}

			if (PragmataPlayerState->GetAssignedRole() == EPragmataPlayerRole::Hugh)
			{
				HughSkinId = PragmataPlayerState->GetLobbySelectedSkinId();
			}
			else if (PragmataPlayerState->GetAssignedRole() == EPragmataPlayerRole::Diana)
			{
				DianaSkinId = PragmataPlayerState->GetLobbySelectedSkinId();
			}
		}
	}

	SpawnedHughCharacter->SetGameplaySkinIds(HughSkinId, DianaSkinId);
}

// player 퇴장 시 role 점유와 GameState pawn pointer를 함께 정리
void APragmataGameMode::ClearRoleState(EPragmataPlayerRole PlayerRole)
{
	if (APragmataGameState* PragmataGameState = GetGameState<APragmataGameState>())
	{
		PragmataGameState->SetRoleConnected(PlayerRole, false);
		if (PlayerRole == EPragmataPlayerRole::Hugh)
		{
			SpawnedHughCharacter = nullptr;
			PragmataGameState->SetHughCharacter(nullptr);
		}
		else if (PlayerRole == EPragmataPlayerRole::Diana)
		{
			SpawnedDianaOperatorPawn = nullptr;
			PragmataGameState->SetDianaOperatorPawn(nullptr);
		}
	}
}

// 두 역할이 모두 들어오면 더 이상 matchmaking에 노출되지 않게 세션 광고 차단
void APragmataGameMode::CloseSessionIfBothPlayersConnected() const
{
	const APragmataGameState* PragmataGameState = GetGameState<APragmataGameState>();
	if (!PragmataGameState || !PragmataGameState->IsHughConnected() || !PragmataGameState->IsDianaConnected())
	{
		return;
	}

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UPragmataMultiplayerSessionSubsystem* SessionSubsystem = GameInstance->GetSubsystem<UPragmataMultiplayerSessionSubsystem>())
		{
			SessionSubsystem->SetSessionAdvertisingEnabled(false);
		}
	}
}
