#include "UI/PragmataRoleLobbyWidget.h"

#include "Components/Button.h"
#include "Components/CircularThrobber.h"
#include "Components/EditableTextBox.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Data/LobbyCharacterSkinSet.h"
#include "Data/PragmataGameFlowConfig.h"
#include "Engine/GameInstance.h"
#include "Game/PragmataLobbyGameState.h"
#include "Game/PragmataLobbyPlayerController.h"
#include "Game/PragmataPlayerState.h"
#include "HAL/PlatformApplicationMisc.h"
#include "InputCoreTypes.h"
#include "UI/PragmataLobbyCharacterSelectWidget.h"
#include "UI/PragmataLobbyMatchmakingWidget.h"

// WBP 위젯 바인딩과 세션 delegate를 연결
void UPragmataRoleLobbyWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
	ResolveSplitWidgetBindings();
	BindWidgetEvents();
	BindSessionSubsystem();
	RefreshLobbyView();
}

// widget 제거 시 세션과 버튼 delegate를 해제해 PIE 재시작 누수를 방지
void UPragmataRoleLobbyWidget::NativeDestruct()
{
	UnbindSessionSubsystem();
	UnbindWidgetEvents();
	Super::NativeDestruct();
}

// lobby state와 local prediction 표시가 늦지 않도록 매 frame UI를 동기화
void UPragmataRoleLobbyWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	RefreshLobbyView();
}

// lobby key 입력은 실제 character select phase에서만 controller가 처리
FReply UPragmataRoleLobbyWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (APragmataLobbyPlayerController* LobbyPlayerController = GetOwningPlayer<APragmataLobbyPlayerController>())
	{
		const FKey PressedKey = InKeyEvent.GetKey();
		if (!LobbyPlayerController->CanHandleCharacterSelectionInput())
		{
			return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
		}
		if (PressedKey == EKeys::A || PressedKey == EKeys::Left)
		{
			LobbyPlayerController->RequestLobbySelectionStep(-1);
			return FReply::Handled();
		}
		if (PressedKey == EKeys::D || PressedKey == EKeys::Right)
		{
			LobbyPlayerController->RequestLobbySelectionStep(1);
			return FReply::Handled();
		}
		if (PressedKey == EKeys::W || PressedKey == EKeys::Up)
		{
			LobbyPlayerController->RequestLobbySkinSelectionStep(-1);
			return FReply::Handled();
		}
		if (PressedKey == EKeys::S || PressedKey == EKeys::Down)
		{
			LobbyPlayerController->RequestLobbySkinSelectionStep(1);
			return FReply::Handled();
		}
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

// title map이 click-to-start를 소유하므로 lobby mouse 입력은 기본 widget 처리에 맡김
FReply UPragmataRoleLobbyWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

// Deprecated wrapper: role 선택은 두 player 입장 후 lobby에서 처리
void UPragmataRoleLobbyWidget::HostAsHugh(const FString& MapPath)
{
	HostLobbySession(MapPath);
}

// 세션 생성 요청은 subsystem에 위임해 UI가 network backend를 직접 몰라도 되게 함
void UPragmataRoleLobbyWidget::HostLobbySession(const FString& MapPath)
{
	if (UPragmataMultiplayerSessionSubsystem* SessionSubsystem = GetSessionSubsystem())
	{
		SessionSubsystem->HostLobbySession(ResolveHostMapPath(MapPath));
	}
}

// Deprecated wrapper: role 선택은 두 player 입장 후 lobby에서 처리
void UPragmataRoleLobbyWidget::JoinAsDiana(const FString& RoomCode)
{
	JoinLobbySession(RoomCode);
}

// 입력된 room code로 lobby session 검색과 join을 시작
void UPragmataRoleLobbyWidget::JoinLobbySession(const FString& RoomCode)
{
	if (UPragmataMultiplayerSessionSubsystem* SessionSubsystem = GetSessionSubsystem())
	{
		SessionSubsystem->JoinLobbySessionByCode(RoomCode);
	}
}

// join 검색 중 사용자가 title/session setup으로 되돌아갈 수 있게 함
void UPragmataRoleLobbyWidget::CancelJoinSearch()
{
	if (UPragmataMultiplayerSessionSubsystem* SessionSubsystem = GetSessionSubsystem())
	{
		SessionSubsystem->CancelSessionSearch();
	}
}

// host가 만든 room code를 OS clipboard로 복사하고 status text를 즉시 갱신
void UPragmataRoleLobbyWidget::CopyRoomCodeToClipboard()
{
	const FString RoomCode = ResolveDisplayedRoomCode();
	if (RoomCode.IsEmpty())
	{
		return;
	}

	FPlatformApplicationMisc::ClipboardCopy(*RoomCode);
	LastStatusMessage = TEXT("초대 코드가 복사되었습니다");
	if (StatusText)
	{
		StatusText->SetText(FText::FromString(LastStatusMessage));
	}
}

// GameState phase, room code, ready 상태, 예측 선택값을 현재 UI 표시로 변환
void UPragmataRoleLobbyWidget::RefreshLobbyView()
{
	const APragmataLobbyGameState* LobbyGameState = GetLobbyGameState();
	EPragmataLobbyPhase LobbyPhase = LobbyGameState ? LobbyGameState->GetLobbyPhase() : EPragmataLobbyPhase::SessionSetup;

	if (LobbyPhase != LastObservedLobbyPhase)
	{
		HandleLobbyPhaseTransition(LobbyPhase);
		LastObservedLobbyPhase = LobbyPhase;
		OnLobbyPhaseChanged(LobbyPhase);
	}
	UpdateCharacterRevealTransition();

	const bool bShowTitle = LobbyPhase == EPragmataLobbyPhase::Title;
	const bool bShowSessionSetup = LobbyPhase == EPragmataLobbyPhase::SessionSetup;
	const bool bShowWaiting = LobbyPhase == EPragmataLobbyPhase::WaitingForPlayer;
	const bool bShowPlayerJoining = LobbyPhase == EPragmataLobbyPhase::PlayerJoining;
	const bool bShowSelection = LobbyPhase == EPragmataLobbyPhase::CharacterSelect || LobbyPhase == EPragmataLobbyPhase::ReadyToStart;
	const bool bShowStartingGame = LobbyPhase == EPragmataLobbyPhase::StartingGame;
	const bool bShowSessionFoundJoining = IsSessionFoundJoiningOverlayVisible(LobbyPhase);
	const bool bShowCharacterReveal = IsCharacterRevealTransitionActive();
	const bool bBlockLobbyControls = bShowPlayerJoining || bShowSessionFoundJoining || bShowStartingGame || bShowCharacterReveal;
	const EPragmataPlayerRole DisplayedRole = ResolveDisplayedRole();
	const bool bSessionActionBusy = bLastSessionInProgress || LastSessionPhase == EPragmataSessionPhase::HostingReady;
	const bool bJoinActionInProgress = bLastSessionInProgress
		&& (LastSessionPhase == EPragmataSessionPhase::Searching || LastSessionPhase == EPragmataSessionPhase::Joining);

	const FString RoomCode = ResolveDisplayedRoomCode();
	const bool bShowRoomCodeDuringSessionAction = !RoomCode.IsEmpty() && bSessionActionBusy;
	const FString WarningMessage = LobbyGameState ? LobbyGameState->GetWarningMessage() : FString();

	if (bShowCharacterReveal)
	{
		TriggerCharacterRevealView();
	}
	else if (bShowStartingGame)
	{
		TriggerStartingGameView();
	}
	else if (bShowSessionFoundJoining)
	{
		TriggerSessionFoundJoiningView();
	}
	else if (bShowPlayerJoining)
	{
		TriggerPlayerJoiningView();
	}
	else if (bShowSelection)
	{
		TriggerCharacterSelectView();
	}
	else if (bShowWaiting)
	{
		TriggerWaitingForPlayerView();
	}
	else if (bShowSessionSetup)
	{
		TriggerSessionSetupView();
	}
	else if (bShowTitle)
	{
		TriggerTitleView();
	}

	const bool bShowRoomCodeText = !bBlockLobbyControls && (bShowRoomCodeDuringSessionAction || bShowWaiting || bShowSelection);
	SetWidgetVisible(RoomCodeText, bShowRoomCodeText);
	SetWidgetVisible(CopyButton, ((bShowRoomCodeDuringSessionAction && !RoomCode.IsEmpty()) || (bShowWaiting && !RoomCode.IsEmpty()))
		&& !bBlockLobbyControls
		&& !bJoinActionInProgress);
	SetWidgetVisible(WarningText, !WarningMessage.IsEmpty() && !bBlockLobbyControls);

	if (HostButton)
	{
		HostButton->SetIsEnabled(!bSessionActionBusy);
	}
	if (JoinButton)
	{
		JoinButton->SetIsEnabled(!bSessionActionBusy);
	}
	if (CancelButton)
	{
		CancelButton->SetIsEnabled(bJoinActionInProgress);
	}
	if (CopyButton)
	{
		CopyButton->SetIsEnabled(!RoomCode.IsEmpty());
	}

	if (RoomCodeText)
	{
		RoomCodeText->SetText(FText::FromString(RoomCode));
	}
	if (WarningText)
	{
		WarningText->SetText(FText::FromString(WarningMessage));
	}
	if (TitlePromptText)
	{
		TitlePromptText->SetText(FText::FromString(TEXT("아무 버튼이나 누르세요")));
	}

	FString StatusMessage = LastStatusMessage;
	if (bShowTitle)
	{
		StatusMessage = TEXT("아무 버튼이나 누르세요");
	}
	else if (bShowSessionSetup)
	{
		if (!LastStatusMessage.IsEmpty() && (bLastSessionInProgress || LastSessionPhase == EPragmataSessionPhase::HostingReady || LastSessionPhase == EPragmataSessionPhase::Failed))
		{
			StatusMessage = LastStatusMessage;
		}
		else
		{
			StatusMessage = TEXT("세션을 만들거나 초대 코드로 참가하세요");
		}
	}
	else if (bShowWaiting)
	{
		StatusMessage = !LastStatusMessage.IsEmpty() ? LastStatusMessage : (RoomCode.IsEmpty() ? TEXT("세션 생성 중") : TEXT("2번 플레이어를 기다리는 중"));
	}
	else if (bShowPlayerJoining)
	{
		StatusMessage = TEXT("플레이어가 참가 중입니다...");
	}
	else if (bShowSelection)
	{
		StatusMessage = BuildSelectionStatusText();
	}
	else if (bShowStartingGame)
	{
		StatusMessage = TEXT("세션에 참가 중입니다...");
	}
	if (bShowSessionFoundJoining)
	{
		StatusMessage = TEXT("세션을 찾았습니다. 참가 중입니다...");
	}
	if (bShowCharacterReveal)
	{
		StatusMessage.Reset();
	}

	if (StatusText)
	{
		const bool bShowStatusText = !bShowStartingGame && !bShowCharacterReveal && !bShowPlayerJoining && !bShowSessionFoundJoining;
		SetStatusTextVisible(bShowStatusText);
		StatusText->SetText(FText::FromString(StatusMessage));
	}
	if (SessionOverlayStatusText)
	{
		const bool bShowOverlayStatusText = (bShowPlayerJoining || bShowSessionFoundJoining || bShowStartingGame) && !bShowCharacterReveal;
		SetSessionOverlayStatusVisible(bShowOverlayStatusText);
		SetSessionOverlayMessage(StatusMessage);
	}

	if (ReadyButton)
	{
		const APragmataLobbyPlayerController* LobbyPlayerController = GetOwningPlayer<APragmataLobbyPlayerController>();
		const APragmataPlayerState* PragmataPlayerState = LobbyPlayerController ? LobbyPlayerController->GetPlayerState<APragmataPlayerState>() : nullptr;
		const bool bCanHostStart = LobbyPlayerController && LobbyPlayerController->IsHostLobbyPlayer() && LobbyPhase == EPragmataLobbyPhase::ReadyToStart;
		const bool bIsReady = PragmataPlayerState && PragmataPlayerState->IsLobbyReady();
		ReadyButton->SetIsEnabled(bCanHostStart || !bIsReady);
	}

	if (HughRoleStatusText)
	{
		FLinearColor HughStatusColor;
		const FString HughStatus = BuildRoleStatusText(EPragmataPlayerRole::Hugh, HughStatusColor);
		HughRoleStatusText->SetText(FText::FromString(HughStatus));
		HughRoleStatusText->SetColorAndOpacity(FSlateColor(HughStatusColor));
	}
	if (DianaRoleStatusText)
	{
		FLinearColor DianaStatusColor;
		const FString DianaStatus = BuildRoleStatusText(EPragmataPlayerRole::Diana, DianaStatusColor);
		DianaRoleStatusText->SetText(FText::FromString(DianaStatus));
		DianaRoleStatusText->SetColorAndOpacity(FSlateColor(DianaStatusColor));
	}
	UpdateRoleDescription(DisplayedRole);
	UpdateSkinDescription(DisplayedRole);
	UpdateReadyButtonLabel(LobbyPhase);
	UpdateSessionMatchOverlay(LobbyPhase);

	// character select에서는 child 상태를 유지하고 matchmaking parent만 숨김
	if (bShowSelection)
	{
		SetMatchmakingVisible(false);
	}
}

// lobby/session을 닫아 host와 join search 상태를 모두 정리
void UPragmataRoleLobbyWidget::DestroyLobbySession()
{
	if (UPragmataMultiplayerSessionSubsystem* SessionSubsystem = GetSessionSubsystem())
	{
		SessionSubsystem->DestroyPragmataSession();
	}
}

// subsystem이 생성한 room code를 캐시해 GameState 복제 전에도 화면에 표시
void UPragmataRoleLobbyWidget::HandleRoomCodeCreated(const FString& RoomCode)
{
	LastRoomCode = RoomCode;
	if (RoomCodeText)
	{
		RoomCodeText->SetText(FText::FromString(RoomCode));
	}
	OnRoomCodeChanged(RoomCode);
}

// session 작업 결과를 status text와 Blueprint event 양쪽에 전달
void UPragmataRoleLobbyWidget::HandleSessionStatusChanged(const FPragmataSessionStatus& Status)
{
	bLastSessionActionSuccessful = Status.bWasSuccessful;
	LastStatusMessage = Status.Message;
	LastSessionPhase = Status.Phase;
	bLastSessionInProgress = Status.bIsInProgress;
	if (!Status.RoomCode.IsEmpty())
	{
		LastRoomCode = Status.RoomCode;
	}

	if (StatusText)
	{
		StatusText->SetText(FText::FromString(Status.Message));
	}

	OnSessionStatusChanged(Status.bWasSuccessful, Status.Message);
	RefreshLobbyView();
}

// host 버튼 클릭을 기본 map path 기반 세션 생성으로 연결
void UPragmataRoleLobbyWidget::HandleHostClicked()
{
	HostLobbySession(FString());
}

// join 버튼 클릭 시 입력칸의 room code로 세션 검색을 시작
void UPragmataRoleLobbyWidget::HandleJoinClicked()
{
	JoinLobbySession(RoomCodeInput ? RoomCodeInput->GetText().ToString() : FString());
}

// cancel 버튼은 진행 중인 join 검색을 중단
void UPragmataRoleLobbyWidget::HandleCancelClicked()
{
	CancelJoinSearch();
}

// destroy 버튼은 현재 lobby session을 명시적으로 닫음
void UPragmataRoleLobbyWidget::HandleDestroyClicked()
{
	DestroyLobbySession();
}

// copy 버튼은 화면에 표시 중인 room code를 clipboard로 보냄
void UPragmataRoleLobbyWidget::HandleCopyClicked()
{
	CopyRoomCodeToClipboard();
}

// ready 버튼은 phase에 따라 ready 또는 start game 요청으로 해석
void UPragmataRoleLobbyWidget::HandleReadyClicked()
{
	if (APragmataLobbyPlayerController* LobbyPlayerController = GetOwningPlayer<APragmataLobbyPlayerController>())
	{
		LobbyPlayerController->RequestLobbyConfirm();
	}
}

// widget이 속한 GameInstance에서 session subsystem을 안전하게 찾음
UPragmataMultiplayerSessionSubsystem* UPragmataRoleLobbyWidget::GetSessionSubsystem() const
{
	UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<UPragmataMultiplayerSessionSubsystem>() : nullptr;
}

// lobby phase와 player array를 읽기 위한 typed GameState 접근점
APragmataLobbyGameState* UPragmataRoleLobbyWidget::GetLobbyGameState() const
{
	UWorld* World = GetWorld();
	return World ? World->GetGameState<APragmataLobbyGameState>() : nullptr;
}

// WBP 버튼을 C++ lobby command에 연결
void UPragmataRoleLobbyWidget::BindWidgetEvents()
{
	if (HostButton)
	{
		HostButton->OnClicked.AddDynamic(this, &ThisClass::HandleHostClicked);
	}
	if (JoinButton)
	{
		JoinButton->OnClicked.AddDynamic(this, &ThisClass::HandleJoinClicked);
	}
	if (CancelButton)
	{
		CancelButton->OnClicked.AddDynamic(this, &ThisClass::HandleCancelClicked);
	}
	if (DestroyButton)
	{
		DestroyButton->OnClicked.AddDynamic(this, &ThisClass::HandleDestroyClicked);
	}
	if (CopyButton)
	{
		CopyButton->OnClicked.AddDynamic(this, &ThisClass::HandleCopyClicked);
	}
	if (ReadyButton)
	{
		ReadyButton->OnClicked.AddDynamic(this, &ThisClass::HandleReadyClicked);
	}
}

// widget destroy 전 동적 delegate를 해제해 재생성 시 중복 호출을 방지
void UPragmataRoleLobbyWidget::UnbindWidgetEvents()
{
	if (HostButton)
	{
		HostButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleHostClicked);
	}
	if (JoinButton)
	{
		JoinButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleJoinClicked);
	}
	if (CancelButton)
	{
		CancelButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleCancelClicked);
	}
	if (DestroyButton)
	{
		DestroyButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleDestroyClicked);
	}
	if (CopyButton)
	{
		CopyButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleCopyClicked);
	}
	if (ReadyButton)
	{
		ReadyButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleReadyClicked);
	}
}

// session subsystem event를 구독해 room code와 status를 UI에 반영
void UPragmataRoleLobbyWidget::BindSessionSubsystem()
{
	UPragmataMultiplayerSessionSubsystem* SessionSubsystem = GetSessionSubsystem();
	if (!SessionSubsystem || BoundSessionSubsystem == SessionSubsystem)
	{
		return;
	}

	UnbindSessionSubsystem();
	BoundSessionSubsystem = SessionSubsystem;
	BoundSessionSubsystem->OnRoomCodeCreated.AddDynamic(this, &ThisClass::HandleRoomCodeCreated);
	BoundSessionSubsystem->OnSessionStatusChanged.AddDynamic(this, &ThisClass::HandleSessionStatusChanged);
}

// 다른 world나 PIE 종료 후 subsystem delegate가 죽은 widget을 호출하지 않게 정리
void UPragmataRoleLobbyWidget::UnbindSessionSubsystem()
{
	if (!BoundSessionSubsystem)
	{
		return;
	}

	BoundSessionSubsystem->OnRoomCodeCreated.RemoveDynamic(this, &ThisClass::HandleRoomCodeCreated);
	BoundSessionSubsystem->OnSessionStatusChanged.RemoveDynamic(this, &ThisClass::HandleSessionStatusChanged);
	BoundSessionSubsystem = nullptr;
}

// root WBP가 child WBP를 조합할 때도 기존 lobby state 로직을 재사용
void UPragmataRoleLobbyWidget::ResolveSplitWidgetBindings()
{
	if (LobbyMatchmakingWidget)
	{
		if (UTextBlock* BoundTitlePromptText = LobbyMatchmakingWidget->GetTitlePromptText())
		{
			TitlePromptText = BoundTitlePromptText;
		}
		if (UTextBlock* BoundWarningText = LobbyMatchmakingWidget->GetWarningText())
		{
			WarningText = BoundWarningText;
		}
		if (UTextBlock* BoundStatusText = LobbyMatchmakingWidget->GetStatusText())
		{
			StatusText = BoundStatusText;
		}
		if (UTextBlock* BoundRoomCodeText = LobbyMatchmakingWidget->GetRoomCodeText())
		{
			RoomCodeText = BoundRoomCodeText;
		}
		if (UEditableTextBox* BoundRoomCodeInput = LobbyMatchmakingWidget->GetRoomCodeInput())
		{
			RoomCodeInput = BoundRoomCodeInput;
		}
		if (UButton* BoundHostButton = LobbyMatchmakingWidget->GetHostButton())
		{
			HostButton = BoundHostButton;
		}
		if (UButton* BoundJoinButton = LobbyMatchmakingWidget->GetJoinButton())
		{
			JoinButton = BoundJoinButton;
		}
		if (UButton* BoundCancelButton = LobbyMatchmakingWidget->GetCancelButton())
		{
			CancelButton = BoundCancelButton;
		}
		if (UButton* BoundDestroyButton = LobbyMatchmakingWidget->GetDestroyButton())
		{
			DestroyButton = BoundDestroyButton;
		}
		if (UButton* BoundCopyButton = LobbyMatchmakingWidget->GetCopyButton())
		{
			CopyButton = BoundCopyButton;
		}
	}

	if (LobbyCharacterSelectWidget)
	{
		if (UButton* BoundReadyButton = LobbyCharacterSelectWidget->GetReadyButton())
		{
			ReadyButton = BoundReadyButton;
		}
		if (UTextBlock* BoundReadyButtonLabel = LobbyCharacterSelectWidget->GetReadyButtonLabel())
		{
			ReadyButtonLabel = BoundReadyButtonLabel;
		}
		if (UTextBlock* BoundHughRoleStatusText = LobbyCharacterSelectWidget->GetHughRoleStatusText())
		{
			HughRoleStatusText = BoundHughRoleStatusText;
		}
		if (UTextBlock* BoundDianaRoleStatusText = LobbyCharacterSelectWidget->GetDianaRoleStatusText())
		{
			DianaRoleStatusText = BoundDianaRoleStatusText;
		}
		if (UTextBlock* BoundRoleDescriptionTitleText = LobbyCharacterSelectWidget->GetRoleDescriptionTitleText())
		{
			RoleDescriptionTitleText = BoundRoleDescriptionTitleText;
		}
		if (UTextBlock* BoundRoleDescriptionBodyText = LobbyCharacterSelectWidget->GetRoleDescriptionBodyText())
		{
			RoleDescriptionBodyText = BoundRoleDescriptionBodyText;
		}
		if (UTextBlock* BoundSkinNameText = LobbyCharacterSelectWidget->GetSkinNameText())
		{
			SkinNameText = BoundSkinNameText;
		}
		if (UTextBlock* BoundSkinCodeLabelText = LobbyCharacterSelectWidget->GetSkinCodeLabelText())
		{
			SkinCodeLabelText = BoundSkinCodeLabelText;
		}
		if (UWidget* BoundRoleStatusDescriptionPanel = LobbyCharacterSelectWidget->GetRoleStatusDescriptionPanel())
		{
			RoleStatusDescriptionPanel = BoundRoleStatusDescriptionPanel;
		}
		if (UWidget* BoundSkinLabelPanel = LobbyCharacterSelectWidget->GetSkinLabelPanel())
		{
			SkinLabelPanel = BoundSkinLabelPanel;
		}
	}
}

// nullptr safe helper로 phase별 UI 표시 전환을 단순화
void UPragmataRoleLobbyWidget::SetWidgetVisible(UWidget* Widget, bool bVisible) const
{
	if (Widget)
	{
		Widget->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

// matchmaking panel과 전용 background가 서로 다른 phase에 남지 않게 함께 전환
void UPragmataRoleLobbyWidget::SetMatchmakingVisible(bool bVisible) const
{
	SetWidgetVisible(MatchmakingBackground, bVisible);
	SetWidgetVisible(LobbyMatchmakingWidget, bVisible);
}

// title 화면 진입 시 WBP가 필요한 최소 root만 보이게 함
void UPragmataRoleLobbyWidget::TriggerTitleView()
{
	HideAllLobbyModeWidgets();
	SetMatchmakingVisible(true);
	SetWidgetVisible(TitlePromptText, true);
	SetStatusTextVisible(true);
}

// 세션 생성/참가 입력을 받을 때 WBP controls를 노출
void UPragmataRoleLobbyWidget::TriggerSessionSetupView()
{
	HideAllLobbyModeWidgets();
	SetSessionSetupControlsVisible(true);
	SetStatusTextVisible(true);
}

// host가 room code를 보여주고 두 번째 player를 기다리는 화면
void UPragmataRoleLobbyWidget::TriggerWaitingForPlayerView()
{
	HideAllLobbyModeWidgets();
	SetWaitingForPlayerControlsVisible(true);
	SetStatusTextVisible(true);
}

// 상대가 발견되어 입장 중인 동안 일반 lobby controls를 덮음
void UPragmataRoleLobbyWidget::TriggerPlayerJoiningView()
{
	HideAllLobbyModeWidgets();
	SetSessionOverlayVisible(true, true);
	SetSessionOverlayStatusVisible(true);
	SetSessionOverlayMessage(TEXT("플레이어가 참가 중입니다..."));
}

// 두 player가 모두 들어온 뒤 character select WBP 영역을 노출
void UPragmataRoleLobbyWidget::TriggerCharacterSelectView()
{
	HideAllLobbyModeWidgets();
	SetCharacterSelectControlsVisible(true);
	SetRoleInfoVisible(true);
	SetStatusTextVisible(true);
}

// host가 게임 시작을 누른 뒤 모든 lobby UI를 dimmer와 spinner로 가림
void UPragmataRoleLobbyWidget::TriggerStartingGameView()
{
	HideAllLobbyModeWidgets();
	SetSessionOverlayVisible(true, true);
}

// joiner가 session을 찾고 실제 travel을 기다리는 화면
void UPragmataRoleLobbyWidget::TriggerSessionFoundJoiningView()
{
	HideAllLobbyModeWidgets();
	SetSessionOverlayVisible(true, true);
	SetSessionOverlayStatusVisible(true);
	SetSessionOverlayMessage(TEXT("세션을 찾았습니다. 참가 중입니다..."));
}

// PlayerJoining에서 character select로 넘어갈 때 검은 화면 reveal을 유지
void UPragmataRoleLobbyWidget::TriggerCharacterRevealView()
{
	HideAllLobbyModeWidgets();
	SetSessionOverlayVisible(true, false);
}

// 검색·참가 중 입력값은 유지하고 중복 요청 controls 대신 Cancel만 표시
void UPragmataRoleLobbyWidget::SetSessionSetupControlsVisible(bool bVisible) const
{
	const bool bJoinActionInProgress = bLastSessionInProgress
		&& (LastSessionPhase == EPragmataSessionPhase::Searching || LastSessionPhase == EPragmataSessionPhase::Joining);
	const bool bShowSessionEntryControls = bVisible && !bJoinActionInProgress;

	SetMatchmakingVisible(bVisible);
	SetWidgetVisible(RoomCodeInput, bShowSessionEntryControls);
	SetWidgetVisible(HostButton, bShowSessionEntryControls);
	SetWidgetVisible(JoinButton, bShowSessionEntryControls);
	SetWidgetVisible(DestroyButton, bVisible);
	SetWidgetVisible(CancelButton, bVisible && bJoinActionInProgress);
}

void UPragmataRoleLobbyWidget::SetWaitingForPlayerControlsVisible(bool bVisible) const
{
	SetMatchmakingVisible(bVisible);
	SetWidgetVisible(RoomCodeText, bVisible);
	SetWidgetVisible(CopyButton, bVisible);
	SetWidgetVisible(DestroyButton, bVisible);
}

void UPragmataRoleLobbyWidget::SetCharacterSelectControlsVisible(bool bVisible) const
{
	SetMatchmakingVisible(bVisible);
	SetWidgetVisible(LobbyCharacterSelectWidget, bVisible);
	SetWidgetVisible(RoomCodeText, bVisible);
	SetWidgetVisible(ReadyButton, bVisible);
}

void UPragmataRoleLobbyWidget::SetRoleInfoVisible(bool bVisible) const
{
	SetWidgetVisible(RoleStatusDescriptionPanel, bVisible);
	SetWidgetVisible(HughRoleStatusText, bVisible);
	SetWidgetVisible(DianaRoleStatusText, bVisible);
	SetWidgetVisible(RoleDescriptionTitleText, bVisible);
	SetWidgetVisible(RoleDescriptionBodyText, bVisible);
}

void UPragmataRoleLobbyWidget::SetStatusTextVisible(bool bVisible) const
{
	if (bVisible)
	{
		SetMatchmakingVisible(true);
	}
	SetWidgetVisible(StatusText, bVisible);
}

void UPragmataRoleLobbyWidget::SetSessionOverlayStatusVisible(bool bVisible) const
{
	if (SessionOverlayStatusText)
	{
		SessionOverlayStatusText->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UPragmataRoleLobbyWidget::SetSessionOverlayVisible(bool bVisible, bool bShowSpinner)
{
	if (SessionMatchDimmer)
	{
		SessionMatchDimmer->SetColorAndOpacity(FLinearColor(0.0f, 0.025f, 0.08f, bCharacterRevealTransitionActive ? ResolveCharacterRevealDimmerAlpha() : 0.92f));
		SessionMatchDimmer->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (SessionLoadingSpinner)
	{
		SessionLoadingSpinner->SetVisibility(bVisible && bShowSpinner ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UPragmataRoleLobbyWidget::SetSessionOverlayMessage(const FString& Message) const
{
	if (SessionOverlayStatusText)
	{
		SessionOverlayStatusText->SetText(FText::FromString(Message));
	}
}

void UPragmataRoleLobbyWidget::HideAllLobbyModeWidgets()
{
	SetWidgetVisible(TitlePromptText, false);
	SetSessionSetupControlsVisible(false);
	SetWaitingForPlayerControlsVisible(false);
	SetCharacterSelectControlsVisible(false);
	SetRoleInfoVisible(false);
	SetWidgetVisible(WarningText, false);
	SetStatusTextVisible(false);
	SetSessionOverlayStatusVisible(false);
	SetSessionOverlayVisible(false, false);
}

void UPragmataRoleLobbyWidget::HandleLobbyPhaseTransition(EPragmataLobbyPhase NewPhase)
{
	const bool bEnteringCharacterSelect = NewPhase == EPragmataLobbyPhase::CharacterSelect || NewPhase == EPragmataLobbyPhase::ReadyToStart;
	if (LastObservedLobbyPhase == EPragmataLobbyPhase::PlayerJoining && bEnteringCharacterSelect)
	{
		if (const UWorld* World = GetWorld())
		{
			bCharacterRevealTransitionActive = true;
			CharacterRevealTransitionStartTime = World->GetTimeSeconds();
		}
	}
}

void UPragmataRoleLobbyWidget::UpdateCharacterRevealTransition()
{
	if (!bCharacterRevealTransitionActive)
	{
		return;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		bCharacterRevealTransitionActive = false;
		return;
	}

	const float TotalDuration = CharacterRevealFadeOutDuration + CharacterRevealBlackHoldDuration + CharacterRevealFadeInDuration;
	if (World->GetTimeSeconds() - CharacterRevealTransitionStartTime >= TotalDuration)
	{
		bCharacterRevealTransitionActive = false;
	}
}

bool UPragmataRoleLobbyWidget::IsSessionFoundJoiningOverlayVisible(EPragmataLobbyPhase LobbyPhase) const
{
	return LobbyPhase == EPragmataLobbyPhase::SessionSetup
		&& LastSessionPhase == EPragmataSessionPhase::Joining
		&& bLastSessionInProgress;
}

bool UPragmataRoleLobbyWidget::IsCharacterRevealTransitionActive() const
{
	return bCharacterRevealTransitionActive;
}

float UPragmataRoleLobbyWidget::ResolveCharacterRevealDimmerAlpha() const
{
	if (!bCharacterRevealTransitionActive)
	{
		return 0.92f;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return 1.0f;
	}

	const float Elapsed = FMath::Max(World->GetTimeSeconds() - CharacterRevealTransitionStartTime, 0.0f);
	if (Elapsed <= CharacterRevealFadeOutDuration + CharacterRevealBlackHoldDuration)
	{
		return 1.0f;
	}

	const float FadeInElapsed = Elapsed - CharacterRevealFadeOutDuration - CharacterRevealBlackHoldDuration;
	const float FadeInAlpha = FMath::Clamp(FadeInElapsed / FMath::Max(CharacterRevealFadeInDuration, 0.01f), 0.0f, 1.0f);
	return FMath::Lerp(1.0f, 0.0f, FadeInAlpha);
}

// session 검색/입장 중에는 dimmer로 중복 클릭을 막고 spinner만 hit-test 제외
void UPragmataRoleLobbyWidget::UpdateSessionMatchOverlay(EPragmataLobbyPhase LobbyPhase)
{
	const bool bShowOverlay = ShouldShowSessionMatchOverlay(LobbyPhase);
	if (SessionMatchDimmer)
	{
		SessionMatchDimmer->SetColorAndOpacity(FLinearColor(0.0f, 0.025f, 0.08f, bCharacterRevealTransitionActive ? ResolveCharacterRevealDimmerAlpha() : 0.92f));
		SessionMatchDimmer->SetVisibility(bShowOverlay ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (SessionLoadingSpinner)
	{
		const bool bShowSpinner = bShowOverlay && !bCharacterRevealTransitionActive;
		SessionLoadingSpinner->SetVisibility(bShowSpinner ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

// room code 표시 이후나 character select 진입 후에는 matching overlay를 숨김
bool UPragmataRoleLobbyWidget::ShouldShowSessionMatchOverlay(EPragmataLobbyPhase LobbyPhase) const
{
	if (LobbyPhase == EPragmataLobbyPhase::StartingGame)
	{
		return true;
	}
	if (LobbyPhase == EPragmataLobbyPhase::PlayerJoining || bCharacterRevealTransitionActive)
	{
		return true;
	}
	if (IsSessionFoundJoiningOverlayVisible(LobbyPhase))
	{
		return true;
	}

	if (!ResolveDisplayedRoomCode().IsEmpty()
		&& (LastSessionPhase == EPragmataSessionPhase::Creating || LastSessionPhase == EPragmataSessionPhase::HostingReady))
	{
		return false;
	}

	if (LobbyPhase == EPragmataLobbyPhase::CharacterSelect || LobbyPhase == EPragmataLobbyPhase::ReadyToStart)
	{
		return false;
	}

	if (const APragmataLobbyGameState* LobbyGameState = GetLobbyGameState();
		LobbyGameState && LobbyGameState->HasEnoughPlayersForSelection())
	{
		return false;
	}

	const bool bIsBeforeLobbyTravel = LobbyPhase == EPragmataLobbyPhase::SessionSetup;
	return bIsBeforeLobbyTravel
		&& (LastSessionPhase == EPragmataSessionPhase::Creating
			|| LastSessionPhase == EPragmataSessionPhase::Searching
			|| LastSessionPhase == EPragmataSessionPhase::Joining);
}

// owning client의 선택 텍스트만 예측값으로 보여 입력 지연을 숨김
FString UPragmataRoleLobbyWidget::BuildSelectionStatusText() const
{
	const APragmataLobbyGameState* LobbyGameState = GetLobbyGameState();
	if (!LobbyGameState)
	{
		return TEXT("캐릭터를 선택하세요");
	}

	FString PlayerOneText = TEXT("1P: -");
	FString PlayerTwoText = TEXT("2P: -");
	const APragmataLobbyPlayerController* LobbyPlayerController = GetOwningPlayer<APragmataLobbyPlayerController>();
	const APragmataPlayerState* LocalPlayerState = LobbyPlayerController ? LobbyPlayerController->GetPlayerState<APragmataPlayerState>() : nullptr;
	for (APlayerState* PlayerState : LobbyGameState->PlayerArray)
	{
		const APragmataPlayerState* PragmataPlayerState = Cast<APragmataPlayerState>(PlayerState);
		if (!PragmataPlayerState)
		{
			continue;
		}

		// 로컬 플레이어만 예측 선택값 표시
		const EPragmataPlayerRole DisplayedRole = PragmataPlayerState == LocalPlayerState && LobbyPlayerController
			? LobbyPlayerController->GetDisplayedLobbySelectedRole()
			: PragmataPlayerState->GetLobbySelectedRole();
		const FString RoleText = DisplayedRole == EPragmataPlayerRole::Diana ? TEXT("Diana") : TEXT("Hugh");
		const FString ReadyText = PragmataPlayerState->IsLobbyReady() ? TEXT("준비 완료") : TEXT("선택 중");
		const FString PlayerText = FString::Printf(TEXT("%dP: %s %s"), PragmataPlayerState->GetLobbyPlayerIndex(), *RoleText, *ReadyText);
		if (PragmataPlayerState->GetLobbyPlayerIndex() == 1)
		{
			PlayerOneText = PlayerText;
		}
		else if (PragmataPlayerState->GetLobbyPlayerIndex() == 2)
		{
			PlayerTwoText = PlayerText;
		}
	}

	return FString::Printf(TEXT("%s | %s"), *PlayerOneText, *PlayerTwoText);
}

FString UPragmataRoleLobbyWidget::BuildRoleStatusText(EPragmataPlayerRole TargetRole, FLinearColor& OutColor) const
{
	// 내 선택은 파란색, 상대 선택은 빨간색, 충돌은 노란색, 나머지는 기본 흰색으로 구분
	static const FLinearColor LocalColor(0.25f, 0.6f, 1.0f);
	static const FLinearColor OtherColor(1.0f, 0.3f, 0.3f);
	static const FLinearColor ConflictColor(1.0f, 0.85f, 0.2f);
	static const FLinearColor NeutralColor(1.0f, 1.0f, 1.0f);
	OutColor = NeutralColor;

	const APragmataLobbyGameState* LobbyGameState = GetLobbyGameState();
	const APragmataLobbyPlayerController* LobbyPlayerController = GetOwningPlayer<APragmataLobbyPlayerController>();
	const APragmataPlayerState* LocalPlayerState = LobbyPlayerController ? LobbyPlayerController->GetPlayerState<APragmataPlayerState>() : nullptr;
	if (!LobbyGameState || !LobbyPlayerController)
	{
		return TEXT("선택 가능");
	}

	bool bSelectedByLocal = false;
	bool bSelectedByOther = false;
	bool bOtherHasChosen = false;
	bool bOtherReady = false;
	for (APlayerState* PlayerState : LobbyGameState->PlayerArray)
	{
		const APragmataPlayerState* PragmataPlayerState = Cast<APragmataPlayerState>(PlayerState);
		if (!PragmataPlayerState)
		{
			continue;
		}

		const bool bIsLocalPlayer = PragmataPlayerState == LocalPlayerState;
		const EPragmataPlayerRole DisplayedRole = bIsLocalPlayer
			? LobbyPlayerController->GetDisplayedLobbySelectedRole()
			: PragmataPlayerState->GetLobbySelectedRole();

		bSelectedByLocal |= bIsLocalPlayer && DisplayedRole == TargetRole;
		bSelectedByOther |= !bIsLocalPlayer && DisplayedRole == TargetRole;
		bOtherHasChosen |= !bIsLocalPlayer && (DisplayedRole == EPragmataPlayerRole::Hugh || DisplayedRole == EPragmataPlayerRole::Diana);
		bOtherReady |= !bIsLocalPlayer && PragmataPlayerState->IsLobbyReady();
	}

	if (bSelectedByLocal && bSelectedByOther)
	{
		OutColor = ConflictColor;
		return TEXT("선택 충돌");
	}
	if (bSelectedByLocal)
	{
		OutColor = LocalColor;
		return TEXT("내 선택");
	}
	if (bSelectedByOther)
	{
		OutColor = OtherColor;
		return bOtherReady ? TEXT("상대 준비 완료") : TEXT("상대가 선택");
	}
	if (bOtherHasChosen)
	{
		return TEXT("선택 가능");
	}

	return TEXT("상대 선택 대기");
}

void UPragmataRoleLobbyWidget::UpdateRoleDescription(EPragmataPlayerRole DisplayedRole) const
{
	// nested WBP를 다시 편집해도 현재 화면의 실제 title widget을 우선 갱신
	UTextBlock* ResolvedTitleText = LobbyCharacterSelectWidget
		? Cast<UTextBlock>(LobbyCharacterSelectWidget->GetWidgetFromName(TEXT("RoleDescriptionTitleText")))
		: nullptr;
	if (!ResolvedTitleText)
	{
		ResolvedTitleText = RoleDescriptionTitleText;
	}

	if (!ResolvedTitleText && !RoleDescriptionBodyText)
	{
		return;
	}

	if (DisplayedRole == EPragmataPlayerRole::Diana)
	{
		if (ResolvedTitleText)
		{
			ResolvedTitleText->SetText(FText::FromString(TEXT("Diana")));
		}
		if (RoleDescriptionBodyText)
		{
			RoleDescriptionBodyText->SetText(FText::FromString(TEXT("해킹과 스캔 중심의 전술 지원. 전장을 읽고 장치를 조작해 Hugh를 지원합니다.")));
		}
		return;
	}

	if (ResolvedTitleText)
	{
		ResolvedTitleText->SetText(FText::FromString(TEXT("Hugh")));
	}
	if (RoleDescriptionBodyText)
	{
		RoleDescriptionBodyText->SetText(FText::FromString(TEXT("전면 교전과 무기 운용 중심. 큰 체격으로 전장을 열고 Diana를 보호합니다.")));
	}
}

void UPragmataRoleLobbyWidget::UpdateSkinDescription(EPragmataPlayerRole DisplayedRole) const
{
	if (DisplayedRole != EPragmataPlayerRole::Hugh && DisplayedRole != EPragmataPlayerRole::Diana)
	{
		SetWidgetVisible(SkinLabelPanel, false);
		return;
	}

	const FName SkinId = ResolveDisplayedSkinId();
	bool bIsDefaultSkin = false;
	const FText SkinName = ResolveDisplayedSkinName(DisplayedRole, SkinId, bIsDefaultSkin);

	SetWidgetVisible(SkinLabelPanel, SkinNameText != nullptr || SkinCodeLabelText != nullptr);
	if (SkinNameText)
	{
		SkinNameText->SetText(SkinName);
	}
	if (SkinCodeLabelText)
	{
		SkinCodeLabelText->SetText(FText::FromString(bIsDefaultSkin ? TEXT("기본 스킨") : TEXT("선택 스킨")));
	}
}

void UPragmataRoleLobbyWidget::UpdateReadyButtonLabel(EPragmataLobbyPhase LobbyPhase) const
{
	if (!ReadyButtonLabel)
	{
		return;
	}

	const APragmataLobbyPlayerController* LobbyPlayerController = GetOwningPlayer<APragmataLobbyPlayerController>();
	const APragmataPlayerState* PragmataPlayerState = LobbyPlayerController ? LobbyPlayerController->GetPlayerState<APragmataPlayerState>() : nullptr;
	if (LobbyPlayerController && LobbyPlayerController->IsHostLobbyPlayer() && LobbyPhase == EPragmataLobbyPhase::ReadyToStart)
	{
		ReadyButtonLabel->SetText(FText::FromString(TEXT("게임 시작")));
		return;
	}

	ReadyButtonLabel->SetText(FText::FromString(PragmataPlayerState && PragmataPlayerState->IsLobbyReady() ? TEXT("준비 완료") : TEXT("준비")));
}

// character select 화면에서는 controller prediction을 우선 읽음
EPragmataPlayerRole UPragmataRoleLobbyWidget::ResolveDisplayedRole() const
{
	const APragmataLobbyPlayerController* LobbyPlayerController = GetOwningPlayer<APragmataLobbyPlayerController>();
	return LobbyPlayerController ? LobbyPlayerController->GetDisplayedLobbySelectedRole() : EPragmataPlayerRole::None;
}

FName UPragmataRoleLobbyWidget::ResolveDisplayedSkinId() const
{
	const APragmataLobbyPlayerController* LobbyPlayerController = GetOwningPlayer<APragmataLobbyPlayerController>();
	return LobbyPlayerController ? LobbyPlayerController->GetDisplayedLobbySelectedSkinId() : NAME_None;
}

FText UPragmataRoleLobbyWidget::ResolveDisplayedSkinName(EPragmataPlayerRole DisplayedRole, FName SkinId, bool& bOutIsDefaultSkin) const
{
	bOutIsDefaultSkin = false;
	const ULobbyCharacterSkinSet* SkinSet = LoadLobbySkinSet();
	if (const FLobbyCharacterSkinDefinition* Definition = SkinSet ? SkinSet->FindSkin(DisplayedRole, SkinId) : nullptr)
	{
		bOutIsDefaultSkin = Definition->bDefaultForRole;
		return Definition->DisplayName;
	}

	for (const FLobbyCharacterSkinDefinition& Definition : ULobbyCharacterSkinSet::MakeFallbackSkinDefinitions())
	{
		if (Definition.PlayerRole == DisplayedRole && Definition.SkinId == SkinId)
		{
			bOutIsDefaultSkin = Definition.bDefaultForRole;
			return Definition.DisplayName;
		}
	}

	return FText::FromString(TEXT("기본 스킨"));
}

ULobbyCharacterSkinSet* UPragmataRoleLobbyWidget::LoadLobbySkinSet() const
{
	if (LoadedLobbySkinSet)
	{
		return LoadedLobbySkinSet;
	}

	UPragmataRoleLobbyWidget* MutableThis = const_cast<UPragmataRoleLobbyWidget*>(this);
	const TSoftObjectPtr<ULobbyCharacterSkinSet> SkinSet(FSoftObjectPath(TEXT("/Game/Data/DA_LobbyCharacterSkinSet.DA_LobbyCharacterSkinSet")));
	MutableThis->LoadedLobbySkinSet = SkinSet.LoadSynchronous();
	if (!LoadedLobbySkinSet)
	{
		MutableThis->LoadedLobbySkinSet = NewObject<ULobbyCharacterSkinSet>(GetTransientPackage(), TEXT("WidgetRuntimeLobbySkinSet"));
	}

	return LoadedLobbySkinSet;
}

// subsystem 캐시와 GameState 복제값 중 현재 화면에 보여줄 room code를 선택
FString UPragmataRoleLobbyWidget::ResolveDisplayedRoomCode() const
{
	if (!LastRoomCode.IsEmpty())
	{
		return LastRoomCode;
	}

	if (const APragmataLobbyGameState* LobbyGameState = GetLobbyGameState())
	{
		return LobbyGameState->GetRoomCode();
	}

	return FString();
}

// Blueprint가 map path를 넘기지 않으면 GameFlow의 lobby map을 사용
FString UPragmataRoleLobbyWidget::ResolveHostMapPath(const FString& RequestedMapPath) const
{
	if (!RequestedMapPath.IsEmpty())
	{
		return RequestedMapPath;
	}

	const UPragmataGameFlowConfig* GameFlowConfig = UPragmataGameFlowConfig::Get(this);
	return GameFlowConfig ? GameFlowConfig->GetLobbyMapPath() : FString();
}
