#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PragmataLobbyCharacterSelectWidget.generated.h"

class UButton;
class UTextBlock;
class UWidget;

UCLASS()
class PRAGMATA_2P_API UPragmataLobbyCharacterSelectWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UButton* GetReadyButton() const { return ReadyButton; }
	UTextBlock* GetReadyButtonLabel() const { return ReadyButtonLabel; }
	UTextBlock* GetHughRoleStatusText() const { return HughRoleStatusText; }
	UTextBlock* GetDianaRoleStatusText() const { return DianaRoleStatusText; }
	UTextBlock* GetRoleDescriptionTitleText() const { return RoleDescriptionTitleText; }
	UTextBlock* GetRoleDescriptionBodyText() const { return RoleDescriptionBodyText; }
	UTextBlock* GetSkinNameText() const { return SkinNameText; }
	UTextBlock* GetSkinCodeLabelText() const { return SkinCodeLabelText; }
	UWidget* GetRoleStatusDescriptionPanel() const { return RoleStatusDescriptionPanel; }
	UWidget* GetSkinLabelPanel() const { return SkinLabelPanel; }

private:
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UButton> ReadyButton = nullptr;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> ReadyButtonLabel = nullptr;

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
};
