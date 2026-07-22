#pragma once

#include "CoreMinimal.h"
#include "Data/PragmataTypes.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TimerManager.h"
#include "PragmataMultiplayerSessionSubsystem.generated.h"

class FOnlineSessionSearch;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPragmataRoomCodeSignature, const FString&, RoomCode);

UENUM(BlueprintType)
enum class EPragmataSessionPhase : uint8
{
	Idle,
	Creating,
	HostingReady,
	Searching,
	Joining,
	Connected,
	Failed
};

USTRUCT(BlueprintType)
struct PRAGMATA_2P_API FPragmataSessionStatus
{
	GENERATED_BODY()

	// Blueprint가 표시할 세션 진행 상태
	UPROPERTY(BlueprintReadOnly, Category="Pragmata|Session")
	bool bWasSuccessful = false;

	UPROPERTY(BlueprintReadOnly, Category="Pragmata|Session")
	FString Message;

	UPROPERTY(BlueprintReadOnly, Category="Pragmata|Session")
	FString RoomCode;

	UPROPERTY(BlueprintReadOnly, Category="Pragmata|Session")
	EPragmataSessionPhase Phase = EPragmataSessionPhase::Idle;

	UPROPERTY(BlueprintReadOnly, Category="Pragmata|Session")
	bool bIsInProgress = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPragmataSessionStatusSignature, const FPragmataSessionStatus&, Status);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPragmataLegacySessionStatusSignature, bool, bWasSuccessful, const FString&, Message);

USTRUCT()
struct FPragmataLobbyRoleHandoff
{
	GENERATED_BODY()

	UPROPERTY()
	EPragmataPlayerRole PlayerRole = EPragmataPlayerRole::None;

	UPROPERTY()
	FName SkinId = NAME_None;
};

UCLASS()
class PRAGMATA_2P_API UPragmataMultiplayerSessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category="Pragmata|Session")
	void HostHughSession(const FString& MapPath);

	UFUNCTION(BlueprintCallable, Category="Pragmata|Session")
	void HostLobbySession(const FString& MapPath);

	void HostLobbySessionWithPreferredRole(const FString& MapPath, EPragmataPlayerRole PreferredRole);

	UFUNCTION(BlueprintCallable, Category="Pragmata|Session")
	void JoinDianaSessionByCode(const FString& RoomCode);

	UFUNCTION(BlueprintCallable, Category="Pragmata|Session")
	void JoinLobbySessionByCode(const FString& RoomCode);

	void JoinLobbySessionByCodeWithPreferredRole(const FString& RoomCode, EPragmataPlayerRole PreferredRole);

	UFUNCTION(BlueprintCallable, Category="Pragmata|Session")
	void CancelSessionSearch();

	UFUNCTION(BlueprintCallable, Category="Pragmata|Session")
	void DestroyPragmataSession();

	void SetSessionAdvertisingEnabled(bool bShouldAdvertise);

	UFUNCTION(BlueprintPure, Category="Pragmata|Session")
	FString GetCurrentRoomCode() const { return CurrentRoomCode; }

	void ClearLobbyRoleHandoff();
	void RegisterLobbyRoleHandoff(int32 PlayerId, int32 LobbyPlayerIndex, EPragmataPlayerRole PlayerRole, FName SkinId = NAME_None);
	bool FindLobbyRoleHandoff(int32 PlayerId, int32 LobbyPlayerIndex, EPragmataPlayerRole& OutPlayerRole, FName* OutSkinId = nullptr) const;
	bool ConsumeNextLobbyRoleHandoff(EPragmataPlayerRole& OutPlayerRole, FName* OutSkinId = nullptr);

	UPROPERTY(BlueprintAssignable, Category="Pragmata|Session")
	FPragmataRoomCodeSignature OnRoomCodeCreated;

	UPROPERTY(BlueprintAssignable, Category="Pragmata|Session")
	FPragmataSessionStatusSignature OnSessionStatusChanged;

	UPROPERTY(BlueprintAssignable, Category="Pragmata|Session")
	FPragmataLegacySessionStatusSignature OnLegacySessionStatusChanged;

private:
	enum class EPendingTravelMode : uint8
	{
		Game,
		Lobby
	};

	void CreatePragmataSession(const FString& MapPath, EPendingTravelMode TravelMode);
	void FindPragmataSessionByCode(const FString& RoomCode, EPendingTravelMode TravelMode);
	void StartFindSessionSearchAttempt();
	void ScheduleFindSessionRetry();
	void OnCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	void OnFindSessionsComplete(bool bWasSuccessful);
	void OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	void OnDestroySessionComplete(FName SessionName, bool bWasSuccessful);

	IOnlineSessionPtr GetSessionInterface() const;
	void BroadcastStatus(EPragmataSessionPhase Phase, bool bWasSuccessful, const FString& Message);
	void ClearSessionDelegates();
	void ClearFindSessionRetry();
	static bool IsInProgressPhase(EPragmataSessionPhase Phase);

	FString PendingHostMapPath;
	FString PendingJoinRoomCode;
	FString CurrentRoomCode;
	EPragmataSessionPhase CurrentSessionPhase = EPragmataSessionPhase::Idle;
	EPragmataPlayerRole PendingPreferredLobbyRole = EPragmataPlayerRole::None;
	TMap<int32, FPragmataLobbyRoleHandoff> LobbyRoleByPlayerId;
	TMap<int32, FPragmataLobbyRoleHandoff> LobbyRoleByLobbyIndex;
	TArray<FPragmataLobbyRoleHandoff> OrderedLobbyTravelRoles;
	int32 NextOrderedLobbyTravelRoleIndex = 0;
	int32 RemainingFindSessionRetries = 0;
	bool bCreateSessionAfterDestroy = false;
	EPendingTravelMode PendingHostTravelMode = EPendingTravelMode::Game;
	EPendingTravelMode PendingJoinTravelMode = EPendingTravelMode::Game;

	FDelegateHandle CreateSessionCompleteHandle;
	FDelegateHandle FindSessionsCompleteHandle;
	FDelegateHandle JoinSessionCompleteHandle;
	FDelegateHandle DestroySessionCompleteHandle;

	TSharedPtr<FOnlineSessionSearch> SessionSearch;
	FTimerHandle FindSessionRetryTimerHandle;
};
