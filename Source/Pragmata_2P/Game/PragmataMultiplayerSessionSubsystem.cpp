#include "Game/PragmataMultiplayerSessionSubsystem.h"

#include "Data/PragmataGameFlowConfig.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Game/PragmataSessionUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Online/OnlineSessionNames.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"

namespace
{
	const FName PragmataSessionName = NAME_GameSession;
	const FName PragmataRoomCodeSettingName(TEXT("PragmataRoomCode"));
	constexpr float FindSessionRetryIntervalSeconds = 0.5f;
	constexpr int32 MaxFindSessionRetryAttempts = 6;

	FString ResolveLobbyMapPath(const UObject* WorldContextObject)
	{
		const UPragmataGameFlowConfig* GameFlowConfig = UPragmataGameFlowConfig::Get(WorldContextObject);
		return GameFlowConfig ? GameFlowConfig->GetLobbyMapPath() : FString();
	}

	FString ResolveGameplayGameModePath(const UObject* WorldContextObject)
	{
		const UPragmataGameFlowConfig* GameFlowConfig = UPragmataGameFlowConfig::Get(WorldContextObject);
		return GameFlowConfig ? GameFlowConfig->GetGameplayGameModePath() : FString();
	}

	// Null subsystem이 port 0을 광고한 상태면 ClientTravel 전에 실패로 돌림
	bool HasInvalidResolvedGamePort(const FString& ConnectInfo)
	{
		return ConnectInfo.EndsWith(TEXT(":0")) || ConnectInfo.Contains(TEXT(":0/"));
	}
}

// subsystem 종료 시 timer와 OSS delegate를 먼저 정리
void UPragmataMultiplayerSessionSubsystem::Deinitialize()
{
	ClearFindSessionRetry();
	ClearSessionDelegates();
	Super::Deinitialize();
}

// direct game smoke용 legacy host 경로
void UPragmataMultiplayerSessionSubsystem::HostHughSession(const FString& MapPath)
{
	const FString ResolvedMapPath = MapPath;
	if (ResolvedMapPath.IsEmpty())
	{
		BroadcastStatus(EPragmataSessionPhase::Failed, false, TEXT("게임 맵 설정을 확인하세요"));
		return;
	}

	CreatePragmataSession(ResolvedMapPath, EPendingTravelMode::Game);
}

// 기존 Blueprint 호출은 preselect 없이 lobby host만 시작
void UPragmataMultiplayerSessionSubsystem::HostLobbySession(const FString& MapPath)
{
	HostLobbySessionWithPreferredRole(MapPath, EPragmataPlayerRole::None);
}

// Host 버튼은 network 생성만 담당하고 gameplay role은 preselect option으로 전달
void UPragmataMultiplayerSessionSubsystem::HostLobbySessionWithPreferredRole(const FString& MapPath, EPragmataPlayerRole PreferredRole)
{
	PendingPreferredLobbyRole = PreferredRole;
	const FString ResolvedMapPath = MapPath.IsEmpty() ? ResolveLobbyMapPath(this) : MapPath;
	if (ResolvedMapPath.IsEmpty())
	{
		BroadcastStatus(EPragmataSessionPhase::Failed, false, TEXT("로비 맵 설정을 확인하세요"));
		return;
	}

	CreatePragmataSession(ResolvedMapPath, EPendingTravelMode::Lobby);
}

// direct game smoke용 legacy join 경로
void UPragmataMultiplayerSessionSubsystem::JoinDianaSessionByCode(const FString& RoomCode)
{
	FindPragmataSessionByCode(RoomCode, EPendingTravelMode::Game);
}

// 기존 Blueprint 호출은 preselect 없이 lobby join만 시작
void UPragmataMultiplayerSessionSubsystem::JoinLobbySessionByCode(const FString& RoomCode)
{
	JoinLobbySessionByCodeWithPreferredRole(RoomCode, EPragmataPlayerRole::None);
}

// Join 버튼은 session 검색만 담당하고 선택 role은 서버 lobby가 다시 확정
void UPragmataMultiplayerSessionSubsystem::JoinLobbySessionByCodeWithPreferredRole(const FString& RoomCode, EPragmataPlayerRole PreferredRole)
{
	PendingPreferredLobbyRole = PreferredRole;
	FindPragmataSessionByCode(RoomCode, EPendingTravelMode::Lobby);
}

// LAN session 생성과 lobby/game travel mode를 같은 상태 머신으로 관리
void UPragmataMultiplayerSessionSubsystem::CreatePragmataSession(const FString& MapPath, EPendingTravelMode TravelMode)
{
	ClearFindSessionRetry();

	IOnlineSessionPtr SessionInterface = GetSessionInterface();
	if (!SessionInterface.IsValid())
	{
		BroadcastStatus(EPragmataSessionPhase::Failed, false, TEXT("온라인 세션을 사용할 수 없습니다"));
		return;
	}

	PendingHostMapPath = MapPath;
	PendingHostTravelMode = TravelMode;
	if (CurrentRoomCode.IsEmpty())
	{
		CurrentRoomCode = PragmataSessionUtils::GenerateRoomCode();
	}
	BroadcastStatus(EPragmataSessionPhase::Creating, true, TEXT("세션 생성 중"));

	if (SessionInterface->GetNamedSession(PragmataSessionName))
	{
		bCreateSessionAfterDestroy = true;
		DestroyPragmataSession();
		return;
	}

	FOnlineSessionSettings SessionSettings;
	SessionSettings.NumPublicConnections = 2;
	SessionSettings.bIsLANMatch = true;
	SessionSettings.bShouldAdvertise = true;
	SessionSettings.bAllowJoinInProgress = true;
	SessionSettings.bAllowJoinViaPresence = false;
	SessionSettings.bUsesPresence = false;
	SessionSettings.Set(PragmataRoomCodeSettingName, CurrentRoomCode, EOnlineDataAdvertisementType::ViaOnlineService);
	SessionSettings.Set(SETTING_MAPNAME, PendingHostMapPath, EOnlineDataAdvertisementType::ViaOnlineService);

	CreateSessionCompleteHandle = SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(
		FOnCreateSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnCreateSessionComplete));

	if (!SessionInterface->CreateSession(0, PragmataSessionName, SessionSettings))
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteHandle);
		CreateSessionCompleteHandle.Reset();
		bCreateSessionAfterDestroy = false;
		BroadcastStatus(EPragmataSessionPhase::Failed, false, TEXT("세션 생성을 시작하지 못했습니다"));
	}
}

// LAN 검색은 초대 코드 일치 session 하나만 join 후보로 인정
void UPragmataMultiplayerSessionSubsystem::FindPragmataSessionByCode(const FString& RoomCode, EPendingTravelMode TravelMode)
{
	IOnlineSessionPtr SessionInterface = GetSessionInterface();
	if (!SessionInterface.IsValid())
	{
		BroadcastStatus(EPragmataSessionPhase::Failed, false, TEXT("온라인 세션을 사용할 수 없습니다"));
		return;
	}

	if (CurrentSessionPhase == EPragmataSessionPhase::Searching || CurrentSessionPhase == EPragmataSessionPhase::Joining || FindSessionsCompleteHandle.IsValid())
	{
		BroadcastStatus(CurrentSessionPhase, false, TEXT("이미 세션을 검색 중입니다"));
		return;
	}

	PendingJoinTravelMode = TravelMode;
	PendingJoinRoomCode = PragmataSessionUtils::NormalizeRoomCode(RoomCode);
	if (!PragmataSessionUtils::IsValidRoomCode(PendingJoinRoomCode))
	{
		BroadcastStatus(EPragmataSessionPhase::Failed, false, TEXT("초대 코드는 영문/숫자 6자리여야 합니다"));
		return;
	}

	ClearFindSessionRetry();
	RemainingFindSessionRetries = MaxFindSessionRetryAttempts;
	StartFindSessionSearchAttempt();
}

// OnlineSubsystemNull LAN 검색을 짧게 반복하기 위한 단일 시도
void UPragmataMultiplayerSessionSubsystem::StartFindSessionSearchAttempt()
{
	IOnlineSessionPtr SessionInterface = GetSessionInterface();
	if (!SessionInterface.IsValid())
	{
		BroadcastStatus(EPragmataSessionPhase::Failed, false, TEXT("온라인 세션을 사용할 수 없습니다"));
		return;
	}

	SessionSearch = MakeShared<FOnlineSessionSearch>();
	SessionSearch->bIsLanQuery = true;
	SessionSearch->MaxSearchResults = 32;
	SessionSearch->PingBucketSize = 50;

	FindSessionsCompleteHandle = SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(
		FOnFindSessionsCompleteDelegate::CreateUObject(this, &ThisClass::OnFindSessionsComplete));

	if (!SessionInterface->FindSessions(0, SessionSearch.ToSharedRef()))
	{
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteHandle);
		FindSessionsCompleteHandle.Reset();
		SessionSearch.Reset();
		ScheduleFindSessionRetry();
	}
	else
	{
		BroadcastStatus(EPragmataSessionPhase::Searching, true, TEXT("세션 검색 중"));
	}
}

// host 광고 직후 join 검색 타이밍 차이를 흡수
void UPragmataMultiplayerSessionSubsystem::ScheduleFindSessionRetry()
{
	if (RemainingFindSessionRetries <= 0)
	{
		BroadcastStatus(EPragmataSessionPhase::Failed, false, TEXT("해당 초대 코드의 세션을 찾지 못했습니다"));
		return;
	}

	--RemainingFindSessionRetries;
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (!World)
	{
		BroadcastStatus(EPragmataSessionPhase::Failed, false, TEXT("세션 검색을 다시 시도할 수 없습니다"));
		return;
	}

	BroadcastStatus(EPragmataSessionPhase::Searching, true, TEXT("세션 검색 중"));
	World->GetTimerManager().SetTimer(
		FindSessionRetryTimerHandle,
		this,
		&ThisClass::StartFindSessionSearchAttempt,
		FindSessionRetryIntervalSeconds,
		false);
}

// join retry timer와 OSS delegate를 함께 정리해 UI cancel을 즉시 반영
void UPragmataMultiplayerSessionSubsystem::CancelSessionSearch()
{
	ClearFindSessionRetry();

	IOnlineSessionPtr SessionInterface = GetSessionInterface();
	if (SessionInterface.IsValid())
	{
		SessionInterface->CancelFindSessions();
		if (FindSessionsCompleteHandle.IsValid())
		{
			SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteHandle);
			FindSessionsCompleteHandle.Reset();
		}
	}

	SessionSearch.Reset();
	RemainingFindSessionRetries = 0;
	BroadcastStatus(EPragmataSessionPhase::Idle, false, TEXT("세션 검색을 취소했습니다"));
}

// 새 host 세션을 만들기 전 기존 GameSession을 비우는 공통 경로
void UPragmataMultiplayerSessionSubsystem::DestroyPragmataSession()
{
	ClearFindSessionRetry();

	IOnlineSessionPtr SessionInterface = GetSessionInterface();
	if (!SessionInterface.IsValid())
	{
		return;
	}

	DestroySessionCompleteHandle = SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(
		FOnDestroySessionCompleteDelegate::CreateUObject(this, &ThisClass::OnDestroySessionComplete));

	if (!SessionInterface->DestroySession(PragmataSessionName))
	{
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteHandle);
		DestroySessionCompleteHandle.Reset();
		bCreateSessionAfterDestroy = false;
		BroadcastStatus(EPragmataSessionPhase::Failed, false, TEXT("세션 종료를 시작하지 못했습니다"));
	}
}

// 두 역할 접속 완료 후 같은 room code가 계속 검색되지 않게 광고만 끔
void UPragmataMultiplayerSessionSubsystem::SetSessionAdvertisingEnabled(bool bShouldAdvertise)
{
	IOnlineSessionPtr SessionInterface = GetSessionInterface();
	if (!SessionInterface.IsValid())
	{
		return;
	}

	if (FOnlineSessionSettings* SessionSettings = SessionInterface->GetSessionSettings(PragmataSessionName))
	{
		SessionSettings->bShouldAdvertise = bShouldAdvertise;
		SessionInterface->UpdateSession(PragmataSessionName, *SessionSettings);
	}
}

// lobby에서 확정한 역할 handoff를 새 travel 시작 전에 초기화
void UPragmataMultiplayerSessionSubsystem::ClearLobbyRoleHandoff()
{
	LobbyRoleByPlayerId.Reset();
	LobbyRoleByLobbyIndex.Reset();
	OrderedLobbyTravelRoles.Reset();
	NextOrderedLobbyTravelRoleIndex = 0;
}

// PlayerId, lobby index, ordered fallback을 모두 저장해 travel 복구 경로를 늘림
void UPragmataMultiplayerSessionSubsystem::RegisterLobbyRoleHandoff(int32 PlayerId, int32 LobbyPlayerIndex, EPragmataPlayerRole PlayerRole, FName SkinId)
{
	if (PlayerRole != EPragmataPlayerRole::Hugh && PlayerRole != EPragmataPlayerRole::Diana)
	{
		return;
	}

	FPragmataLobbyRoleHandoff Handoff;
	Handoff.PlayerRole = PlayerRole;
	Handoff.SkinId = SkinId;

	if (PlayerId >= 0)
	{
		LobbyRoleByPlayerId.FindOrAdd(PlayerId) = Handoff;
	}

	if (LobbyPlayerIndex >= 1 && LobbyPlayerIndex <= 2)
	{
		LobbyRoleByLobbyIndex.FindOrAdd(LobbyPlayerIndex) = Handoff;

		const int32 RoleIndex = LobbyPlayerIndex - 1;
		if (OrderedLobbyTravelRoles.Num() <= RoleIndex)
		{
			OrderedLobbyTravelRoles.SetNum(RoleIndex + 1);
		}
		OrderedLobbyTravelRoles[RoleIndex] = Handoff;
	}
}

// seamless travel에서 남아 있는 PlayerState 식별자로 lobby 선택 role을 찾음
bool UPragmataMultiplayerSessionSubsystem::FindLobbyRoleHandoff(int32 PlayerId, int32 LobbyPlayerIndex, EPragmataPlayerRole& OutPlayerRole, FName* OutSkinId) const
{
	if (PlayerId >= 0)
	{
		if (const FPragmataLobbyRoleHandoff* FoundHandoff = LobbyRoleByPlayerId.Find(PlayerId))
		{
			OutPlayerRole = FoundHandoff->PlayerRole;
			if (OutSkinId)
			{
				*OutSkinId = FoundHandoff->SkinId;
			}
			return true;
		}
	}

	if (LobbyPlayerIndex >= 1 && LobbyPlayerIndex <= 2)
	{
		if (const FPragmataLobbyRoleHandoff* FoundHandoff = LobbyRoleByLobbyIndex.Find(LobbyPlayerIndex))
		{
			OutPlayerRole = FoundHandoff->PlayerRole;
			if (OutSkinId)
			{
				*OutSkinId = FoundHandoff->SkinId;
			}
			return true;
		}
	}

	return false;
}

// PIE non-seamless fallback은 lobby 입장 순서대로 role을 한 번씩 소비
bool UPragmataMultiplayerSessionSubsystem::ConsumeNextLobbyRoleHandoff(EPragmataPlayerRole& OutPlayerRole, FName* OutSkinId)
{
	while (NextOrderedLobbyTravelRoleIndex < OrderedLobbyTravelRoles.Num())
	{
		const FPragmataLobbyRoleHandoff Handoff = OrderedLobbyTravelRoles[NextOrderedLobbyTravelRoleIndex];
		++NextOrderedLobbyTravelRoleIndex;

		if (Handoff.PlayerRole == EPragmataPlayerRole::Hugh || Handoff.PlayerRole == EPragmataPlayerRole::Diana)
		{
			OutPlayerRole = Handoff.PlayerRole;
			if (OutSkinId)
			{
				*OutSkinId = Handoff.SkinId;
			}
			return true;
		}
	}

	return false;
}

// listen travel URL에 room code를 실어 host lobby로 이동
void UPragmataMultiplayerSessionSubsystem::OnCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	IOnlineSessionPtr SessionInterface = GetSessionInterface();
	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteHandle);
	}
	CreateSessionCompleteHandle.Reset();

	if (!bWasSuccessful)
	{
		BroadcastStatus(EPragmataSessionPhase::Failed, false, TEXT("세션 생성에 실패했습니다"));
		return;
	}

	OnRoomCodeCreated.Broadcast(CurrentRoomCode);
	BroadcastStatus(EPragmataSessionPhase::HostingReady, true, TEXT("2번 플레이어를 기다리는 중"));

	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (!World)
	{
		BroadcastStatus(EPragmataSessionPhase::Failed, false, TEXT("로비로 이동할 수 없습니다"));
		return;
	}

	FString TravelOptions;
	if (PendingHostTravelMode == EPendingTravelMode::Lobby)
	{
		TravelOptions = PragmataSessionUtils::BuildLobbyListenOpenLevelOptions(CurrentRoomCode, PendingPreferredLobbyRole);
	}
	else
	{
		const FString GameplayGameModePath = ResolveGameplayGameModePath(this);
		if (GameplayGameModePath.IsEmpty())
		{
			BroadcastStatus(EPragmataSessionPhase::Failed, false, TEXT("게임 모드 설정을 확인하세요"));
			return;
		}

		TravelOptions = PragmataSessionUtils::BuildHughListenOpenLevelOptions(CurrentRoomCode, GameplayGameModePath);
	}

	// 최초 host 전환은 아직 listen NetDriver가 없으므로 ServerTravel 대신 local listen travel 사용
	UGameplayStatics::OpenLevel(this, FName(*PendingHostMapPath), true, TravelOptions);
}

// room code가 0개면 retry, 2개 이상이면 ambiguous 실패로 처리
void UPragmataMultiplayerSessionSubsystem::OnFindSessionsComplete(bool bWasSuccessful)
{
	IOnlineSessionPtr SessionInterface = GetSessionInterface();
	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteHandle);
	}
	FindSessionsCompleteHandle.Reset();

	if (!bWasSuccessful || !SessionSearch.IsValid())
	{
		SessionSearch.Reset();
		ScheduleFindSessionRetry();
		return;
	}

	TArray<FOnlineSessionSearchResult> MatchingResults;
	for (const FOnlineSessionSearchResult& Result : SessionSearch->SearchResults)
	{
		FString FoundRoomCode;
		Result.Session.SessionSettings.Get(PragmataRoomCodeSettingName, FoundRoomCode);
		if (PragmataSessionUtils::NormalizeRoomCode(FoundRoomCode) == PendingJoinRoomCode)
		{
			MatchingResults.Add(Result);
		}
	}

	if (MatchingResults.Num() == 0)
	{
		SessionSearch.Reset();
		ScheduleFindSessionRetry();
		return;
	}

	if (MatchingResults.Num() > 1)
	{
		SessionSearch.Reset();
		BroadcastStatus(EPragmataSessionPhase::Failed, false, TEXT("같은 초대 코드의 세션이 여러 개 발견되었습니다"));
		return;
	}

	JoinSessionCompleteHandle = SessionInterface->AddOnJoinSessionCompleteDelegate_Handle(
		FOnJoinSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnJoinSessionComplete));

	if (!SessionInterface->JoinSession(0, PragmataSessionName, MatchingResults[0]))
	{
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteHandle);
		JoinSessionCompleteHandle.Reset();
		SessionSearch.Reset();
		BroadcastStatus(EPragmataSessionPhase::Failed, false, TEXT("세션 참가를 시작하지 못했습니다"));
		return;
	}

	RemainingFindSessionRetries = 0;
	BroadcastStatus(EPragmataSessionPhase::Joining, true, TEXT("세션을 찾았습니다. 참가 중입니다"));
}

// client travel URL에 room code를 붙여 lobby server로 입장
void UPragmataMultiplayerSessionSubsystem::OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	IOnlineSessionPtr SessionInterface = GetSessionInterface();
	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteHandle);
	}
	JoinSessionCompleteHandle.Reset();
	SessionSearch.Reset();

	if (Result != EOnJoinSessionCompleteResult::Success || !SessionInterface.IsValid())
	{
		BroadcastStatus(EPragmataSessionPhase::Failed, false, TEXT("세션 참가에 실패했습니다"));
		return;
	}

	FString ConnectInfo;
	if (!SessionInterface->GetResolvedConnectString(SessionName, ConnectInfo))
	{
		BroadcastStatus(EPragmataSessionPhase::Failed, false, TEXT("호스트 주소를 찾지 못했습니다"));
		return;
	}
	if (HasInvalidResolvedGamePort(ConnectInfo))
	{
		UE_LOG(LogTemp, Warning, TEXT("[PragmataMultiplayerSessionSubsystem::OnJoinSessionComplete] Invalid resolved connect string: %s"), *ConnectInfo);
		BroadcastStatus(EPragmataSessionPhase::Failed, false, TEXT("호스트 포트가 아직 준비되지 않았습니다"));
		return;
	}

	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	APlayerController* PlayerController = World ? World->GetFirstPlayerController() : nullptr;
	if (!PlayerController)
	{
		BroadcastStatus(EPragmataSessionPhase::Failed, false, TEXT("로컬 플레이어를 찾지 못했습니다"));
		return;
	}

	BroadcastStatus(EPragmataSessionPhase::Joining, true, TEXT("세션을 찾았습니다. 참가 중입니다"));
	const FString TravelURL = PendingJoinTravelMode == EPendingTravelMode::Lobby
		? PragmataSessionUtils::BuildLobbyJoinTravelURL(ConnectInfo, PendingJoinRoomCode, PendingPreferredLobbyRole)
		: PragmataSessionUtils::BuildDianaJoinTravelURL(ConnectInfo, PendingJoinRoomCode);
	PlayerController->ClientTravel(TravelURL, TRAVEL_Absolute);
}

// destroy 후 재생성 요청이 있으면 같은 pending host 설정으로 이어서 생성
void UPragmataMultiplayerSessionSubsystem::OnDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
	IOnlineSessionPtr SessionInterface = GetSessionInterface();
	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteHandle);
	}
	DestroySessionCompleteHandle.Reset();

	if (bWasSuccessful && bCreateSessionAfterDestroy)
	{
		bCreateSessionAfterDestroy = false;
		CreatePragmataSession(PendingHostMapPath, PendingHostTravelMode);
		return;
	}

	bCreateSessionAfterDestroy = false;
	CurrentRoomCode.Reset();
	BroadcastStatus(
		bWasSuccessful ? EPragmataSessionPhase::Idle : EPragmataSessionPhase::Failed,
		bWasSuccessful,
		bWasSuccessful ? TEXT("세션을 종료했습니다") : TEXT("세션 종료에 실패했습니다"));
}

// PIE world별 OnlineSubsystemNull session interface를 조회
IOnlineSessionPtr UPragmataMultiplayerSessionSubsystem::GetSessionInterface() const
{
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	// PIE 월드별 OnlineSubsystem 조회
	IOnlineSubsystem* OnlineSubsystem = Online::GetSubsystem(World);
	return OnlineSubsystem ? OnlineSubsystem->GetSessionInterface() : nullptr;
}

// UI는 status delegate 하나로 room code, phase, busy state를 모두 갱신
void UPragmataMultiplayerSessionSubsystem::BroadcastStatus(EPragmataSessionPhase Phase, bool bWasSuccessful, const FString& Message)
{
	CurrentSessionPhase = Phase;

	FPragmataSessionStatus Status;
	Status.bWasSuccessful = bWasSuccessful;
	Status.Message = Message;
	Status.RoomCode = CurrentRoomCode.IsEmpty() ? PendingJoinRoomCode : CurrentRoomCode;
	Status.Phase = Phase;
	Status.bIsInProgress = IsInProgressPhase(Phase);

	OnSessionStatusChanged.Broadcast(Status);
	OnLegacySessionStatusChanged.Broadcast(bWasSuccessful, Message);

	UE_LOG(LogTemp, Log, TEXT("[PragmataMultiplayerSessionSubsystem] %s: %s"),
		bWasSuccessful ? TEXT("Success") : TEXT("Failure"),
		*Message);
}

// retry timer를 끊어 이전 join 시도가 다음 검색에 섞이지 않게 함
void UPragmataMultiplayerSessionSubsystem::ClearFindSessionRetry()
{
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (World)
	{
		World->GetTimerManager().ClearTimer(FindSessionRetryTimerHandle);
	}
	RemainingFindSessionRetries = 0;
}

// UI button enable 정책에 쓰는 진행 중 phase 판정
bool UPragmataMultiplayerSessionSubsystem::IsInProgressPhase(EPragmataSessionPhase Phase)
{
	return Phase == EPragmataSessionPhase::Creating
		|| Phase == EPragmataSessionPhase::Searching
		|| Phase == EPragmataSessionPhase::Joining;
}

// subsystem 종료나 재시작 전에 OSS delegate handle을 모두 해제
void UPragmataMultiplayerSessionSubsystem::ClearSessionDelegates()
{
	ClearFindSessionRetry();

	IOnlineSessionPtr SessionInterface = GetSessionInterface();
	if (!SessionInterface.IsValid())
	{
		return;
	}

	if (CreateSessionCompleteHandle.IsValid())
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteHandle);
		CreateSessionCompleteHandle.Reset();
	}
	if (FindSessionsCompleteHandle.IsValid())
	{
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteHandle);
		FindSessionsCompleteHandle.Reset();
	}
	if (JoinSessionCompleteHandle.IsValid())
	{
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteHandle);
		JoinSessionCompleteHandle.Reset();
	}
	if (DestroySessionCompleteHandle.IsValid())
	{
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteHandle);
		DestroySessionCompleteHandle.Reset();
	}
}
