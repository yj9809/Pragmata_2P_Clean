#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PragmataLobbyMatchmakingWidget.generated.h"

class UButton;
class UEditableTextBox;
class UTextBlock;

UCLASS()
class PRAGMATA_2P_API UPragmataLobbyMatchmakingWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UTextBlock* GetTitlePromptText() const { return TitlePromptText; }
	UTextBlock* GetWarningText() const { return WarningText; }
	UTextBlock* GetStatusText() const { return StatusText; }
	UTextBlock* GetRoomCodeText() const { return RoomCodeText; }
	UEditableTextBox* GetRoomCodeInput() const { return RoomCodeInput; }
	UButton* GetHostButton() const { return HostButton; }
	UButton* GetJoinButton() const { return JoinButton; }
	UButton* GetCancelButton() const { return CancelButton; }
	UButton* GetDestroyButton() const { return DestroyButton; }
	UButton* GetCopyButton() const { return CopyButton; }

private:
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> TitlePromptText = nullptr;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> WarningText = nullptr;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusText = nullptr;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> RoomCodeText = nullptr;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UEditableTextBox> RoomCodeInput = nullptr;

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
};
