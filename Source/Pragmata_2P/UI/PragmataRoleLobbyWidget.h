#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Data/PragmataTypes.h"
#include "Game/PragmataMultiplayerSessionSubsystem.h"
#include "PragmataRoleLobbyWidget.generated.h"

class APragmataLobbyGameState;
class UButton;
class UCircularThrobber;
class UEditableTextBox;
class UImage;
class ULobbyCharacterSkinSet;
class UPragmataLobbyCharacterSelectWidget;
class UPragmataLobbyMatchmakingWidget;
class UPragmataMultiplayerSessionSubsystem;
class UTextBlock;
class UWidget;

UCLASS()
class PRAGMATA_2P_API UPragmataRoleLobbyWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	UFUNCTION(BlueprintCallable, Category="Pragmata|Lobby", meta=(DeprecatedFunction, DeprecationMessage="Use HostLobbySession; role selection starts after both lobby players join."))
	void HostAsHugh(const FString& MapPath);

	UFUNCTION(BlueprintCallable, Category="Pragmata|Lobby")
	void HostLobbySession(const FString& MapPath);

	UFUNCTION(BlueprintCallable, Category="Pragmata|Lobby", meta=(DeprecatedFunction, DeprecationMessage="Use JoinLobbySession; role selection starts after both lobby players join."))
	void JoinAsDiana(const FString& RoomCode);

	UFUNCTION(BlueprintCallable, Category="Pragmata|Lobby")
	void JoinLobbySession(const FString& RoomCode);

	UFUNCTION(BlueprintCallable, Category="Pragmata|Lobby")
	void CancelJoinSearch();

	UFUNCTION(BlueprintCallable, Category="Pragmata|Lobby")
	void DestroyLobbySession();

	UFUNCTION(BlueprintCallable, Category="Pragmata|Lobby")
	void CopyRoomCodeToClipboard();

	UFUNCTION(BlueprintCallable, Category="Pragmata|Lobby")
	void RefreshLobbyView();

	UFUNCTION(BlueprintCallable, Category="Pragmata|Lobby|View")
	void TriggerTitleView();

	UFUNCTION(BlueprintCallable, Category="Pragmata|Lobby|View")
	void TriggerSessionSetupView();

	UFUNCTION(BlueprintCallable, Category="Pragmata|Lobby|View")
	void TriggerWaitingForPlayerView();

	UFUNCTION(BlueprintCallable, Category="Pragmata|Lobby|View")
	void TriggerPlayerJoiningView();

	UFUNCTION(BlueprintCallable, Category="Pragmata|Lobby|View")
	void TriggerCharacterSelectView();

	UFUNCTION(BlueprintCallable, Category="Pragmata|Lobby|View")
	void TriggerStartingGameView();

	UFUNCTION(BlueprintCallable, Category="Pragmata|Lobby|View")
	void TriggerSessionFoundJoiningView();

	UFUNCTION(BlueprintCallable, Category="Pragmata|Lobby|View")
	void TriggerCharacterRevealView();

	UFUNCTION(BlueprintPure, Category="Pragmata|Lobby")
	FString GetLastRoomCode() const { return LastRoomCode; }

	UFUNCTION(BlueprintPure, Category="Pragmata|Lobby")
	FString GetLastStatusMessage() const { return LastStatusMessage; }

	UFUNCTION(BlueprintPure, Category="Pragmata|Lobby")
	bool WasLastSessionActionSuccessful() const { return bLastSessionActionSuccessful; }

protected:
	UFUNCTION(BlueprintImplementableEvent, Category="Pragmata|Lobby")
	void OnRoomCodeChanged(const FString& RoomCode);

	UFUNCTION(BlueprintImplementableEvent, Category="Pragmata|Lobby")
	void OnSessionStatusChanged(bool bWasSuccessful, const FString& Message);

	UFUNCTION(BlueprintImplementableEvent, Category="Pragmata|Lobby")
	void OnLobbyPhaseChanged(EPragmataLobbyPhase LobbyPhase);

private:
	UFUNCTION()
	void HandleRoomCodeCreated(const FString& RoomCode);

	UFUNCTION()
	void HandleSessionStatusChanged(const FPragmataSessionStatus& Status);

	UFUNCTION()
	void HandleHostClicked();

	UFUNCTION()
	void HandleJoinClicked();

	UFUNCTION()
	void HandleCancelClicked();

	UFUNCTION()
	void HandleDestroyClicked();

	UFUNCTION()
	void HandleCopyClicked();

	UFUNCTION()
	void HandleReadyClicked();

	UPragmataMultiplayerSessionSubsystem* GetSessionSubsystem() const;
	APragmataLobbyGameState* GetLobbyGameState() const;
	void BindWidgetEvents();
	void UnbindWidgetEvents();
	void BindSessionSubsystem();
	void UnbindSessionSubsystem();
	void ResolveSplitWidgetBindings();
	void SetWidgetVisible(UWidget* Widget, bool bVisible) const;
	void SetMatchmakingVisible(bool bVisible) const;
	void SetSessionSetupControlsVisible(bool bVisible) const;
	void SetWaitingForPlayerControlsVisible(bool bVisible) const;
	void SetCharacterSelectControlsVisible(bool bVisible) const;
	void SetRoleInfoVisible(bool bVisible) const;
	void SetStatusTextVisible(bool bVisible) const;
	void SetSessionOverlayStatusVisible(bool bVisible) const;
	void SetSessionOverlayVisible(bool bVisible, bool bShowSpinner);
	void SetSessionOverlayMessage(const FString& Message) const;
	void HideAllLobbyModeWidgets();
	void HandleLobbyPhaseTransition(EPragmataLobbyPhase NewPhase);
	void UpdateCharacterRevealTransition();
	bool IsSessionFoundJoiningOverlayVisible(EPragmataLobbyPhase LobbyPhase) const;
	bool IsCharacterRevealTransitionActive() const;
	float ResolveCharacterRevealDimmerAlpha() const;
	void UpdateSessionMatchOverlay(EPragmataLobbyPhase LobbyPhase);
	bool ShouldShowSessionMatchOverlay(EPragmataLobbyPhase LobbyPhase) const;
	FString BuildSelectionStatusText() const;
	FString BuildRoleStatusText(EPragmataPlayerRole TargetRole, FLinearColor& OutColor) const;
	void UpdateRoleDescription(EPragmataPlayerRole DisplayedRole) const;
	void UpdateSkinDescription(EPragmataPlayerRole DisplayedRole) const;
	void UpdateReadyButtonLabel(EPragmataLobbyPhase LobbyPhase) const;
	EPragmataPlayerRole ResolveDisplayedRole() const;
	FName ResolveDisplayedSkinId() const;
	FText ResolveDisplayedSkinName(EPragmataPlayerRole DisplayedRole, FName SkinId, bool& bOutIsDefaultSkin) const;
	ULobbyCharacterSkinSet* LoadLobbySkinSet() const;
	FString ResolveDisplayedRoomCode() const;
	FString ResolveHostMapPath(const FString& RequestedMapPath) const;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UPragmataLobbyMatchmakingWidget> LobbyMatchmakingWidget = nullptr;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UImage> MatchmakingBackground = nullptr;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UPragmataLobbyCharacterSelectWidget> LobbyCharacterSelectWidget = nullptr;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> TitlePromptText = nullptr;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> WarningText = nullptr;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UButton> HostButton = nullptr;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UButton> JoinButton = nullptr;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UButton> CancelButton = nullptr;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UButton> DestroyButton = nullptr;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UButton> CopyButton = nullptr;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UButton> ReadyButton = nullptr;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> ReadyButtonLabel = nullptr;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UEditableTextBox> RoomCodeInput = nullptr;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> RoomCodeText = nullptr;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusText = nullptr;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> SessionOverlayStatusText = nullptr;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> HughRoleStatusText = nullptr;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> DianaRoleStatusText = nullptr;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> RoleDescriptionTitleText = nullptr;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> RoleDescriptionBodyText = nullptr;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> SkinNameText = nullptr;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> SkinCodeLabelText = nullptr;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UWidget> RoleStatusDescriptionPanel = nullptr;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UWidget> SkinLabelPanel = nullptr;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UImage> SessionMatchDimmer = nullptr;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UImage> SessionLoadingSpinner = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UPragmataMultiplayerSessionSubsystem> BoundSessionSubsystem = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<ULobbyCharacterSkinSet> LoadedLobbySkinSet = nullptr;

	UPROPERTY(BlueprintReadOnly, Category="Pragmata|Lobby", meta=(AllowPrivateAccess="true"))
	FString LastRoomCode;

	UPROPERTY(BlueprintReadOnly, Category="Pragmata|Lobby", meta=(AllowPrivateAccess="true"))
	FString LastStatusMessage;

	UPROPERTY(BlueprintReadOnly, Category="Pragmata|Lobby", meta=(AllowPrivateAccess="true"))
	bool bLastSessionActionSuccessful = false;

	EPragmataSessionPhase LastSessionPhase = EPragmataSessionPhase::Idle;
	bool bLastSessionInProgress = false;
	EPragmataLobbyPhase LastObservedLobbyPhase = EPragmataLobbyPhase::SessionSetup;
	bool bCharacterRevealTransitionActive = false;
	float CharacterRevealTransitionStartTime = 0.0f;
	float CharacterRevealFadeOutDuration = 0.18f;
	float CharacterRevealBlackHoldDuration = 0.22f;
	float CharacterRevealFadeInDuration = 0.55f;
};
