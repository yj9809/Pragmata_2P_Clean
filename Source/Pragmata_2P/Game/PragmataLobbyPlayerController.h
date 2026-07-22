#pragma once

#include "CoreMinimal.h"
#include "Data/PragmataTypes.h"
#include "GameFramework/PlayerController.h"
#include "PragmataLobbyPlayerController.generated.h"

class UUserWidget;
class UInputAction;
class UInputMappingContext;
class ALobbyPresentationDirector;
class APragmataPlayerState;
class ULobbyCharacterSkinSet;
struct FInputActionValue;

UCLASS()
class PRAGMATA_2P_API APragmataLobbyPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	APragmataLobbyPlayerController();

	virtual void SetupInputComponent() override;
	virtual void OnRep_PlayerState() override;

	UFUNCTION(BlueprintCallable, Category="Pragmata|Lobby")
	void RequestLobbySelectionStep(int32 Direction);

	UFUNCTION(BlueprintCallable, Category="Pragmata|Lobby")
	void RequestLobbySkinSelectionStep(int32 Direction);

	UFUNCTION(BlueprintCallable, Category="Pragmata|Lobby")
	void RequestLobbyConfirm();

	UFUNCTION(BlueprintPure, Category="Pragmata|Lobby")
	bool IsHostLobbyPlayer() const;

	// owning client는 서버 확정 전 예측 선택값을 UI/preview에 노출
	UFUNCTION(BlueprintPure, Category="Pragmata|Lobby")
	EPragmataPlayerRole GetDisplayedLobbySelectedRole() const;

	UFUNCTION(BlueprintPure, Category="Pragmata|Lobby")
	FName GetDisplayedLobbySelectedSkinId() const;

	UFUNCTION(BlueprintPure, Category="Pragmata|Lobby")
	bool CanHandleCharacterSelectionInput() const;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void ShowLobbyWidget();
	void ApplyLobbyInputMappingContext();
	void RemoveLobbyInputMappingContext();
	UInputMappingContext* LoadLobbyInputMappingContext() const;
	UInputAction* LoadLobbyInputAction(const TSoftObjectPtr<UInputAction>& InputAction) const;
	void SetupLobbyCameraView();
	ALobbyPresentationDirector* FindLobbyPresentationDirector() const;
	void UpdateLobbyPresentationFocus(int32 Direction);
	void RefreshLobbyPresentationSkins();
	void RefreshLobbyPresentationAvailability();
	void HandleConfirmPressed();
	void HandleLobbySelectAction(const FInputActionValue& Value);
	void HandleLobbySkinSelectAction(const FInputActionValue& Value);
	void HandleLeftStickHorizontal(float Value);
	void HandleLeftStickVertical(float Value);
	bool IsCharacterSelectionInputAllowed() const;
	void AdvanceLocalDisplayedRole(int32 Direction);
	void AdvanceLocalDisplayedSkin(int32 Direction);
	FName ResolveDefaultSkinForRole(EPragmataPlayerRole PlayerRole) const;
	FName ResolveAdjacentSkinForRole(EPragmataPlayerRole PlayerRole, FName CurrentSkinId, int32 Direction) const;
	ULobbyCharacterSkinSet* LoadLobbySkinSet() const;
	void BindLobbyPlayerState();
	void RebindObservedLobbyPlayerStates();
	void HandleLobbyStateChanged();

	UFUNCTION(Server, Reliable)
	void ServerRequestSelectionStep(int32 Direction);

	UFUNCTION(Server, Reliable)
	void ServerRequestSkinSelectionStep(int32 Direction);

	UFUNCTION(Server, Reliable)
	void ServerRequestReady();

	UFUNCTION(Server, Reliable)
	void ServerRequestStartGame();

	UPROPERTY(EditDefaultsOnly, Category="Pragmata|Lobby")
	TSoftClassPtr<UUserWidget> LobbyWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category="Pragmata|Lobby")
	FTransform FallbackCameraTransform = FTransform(FRotator(-12.0, 0.0, 0.0), FVector(-460.0, 0.0, 180.0), FVector::OneVector);

	UPROPERTY(EditDefaultsOnly, Category="Pragmata|Lobby|Input")
	TSoftObjectPtr<UInputMappingContext> LobbyInputMappingContext;

	UPROPERTY(EditDefaultsOnly, Category="Pragmata|Lobby|Input")
	TSoftObjectPtr<UInputAction> LobbySelectAction;

	UPROPERTY(EditDefaultsOnly, Category="Pragmata|Lobby|Input")
	TSoftObjectPtr<UInputAction> LobbySkinSelectAction;

	UPROPERTY(EditDefaultsOnly, Category="Pragmata|Lobby|Input")
	TSoftObjectPtr<UInputAction> LobbyConfirmAction;

	UPROPERTY(EditDefaultsOnly, Category="Pragmata|Lobby|Skin")
	TSoftObjectPtr<ULobbyCharacterSkinSet> LobbySkinSet;

	UPROPERTY(Transient)
	TObjectPtr<ULobbyCharacterSkinSet> LoadedLobbySkinSet = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> LobbyWidget;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> AppliedLobbyInputMappingContext = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<APragmataPlayerState> BoundLobbyPlayerState = nullptr;

	TArray<TWeakObjectPtr<APragmataPlayerState>> ObservedLobbyPlayerStates;

	UPROPERTY(Transient)
	TObjectPtr<ALobbyPresentationDirector> LobbyPresentationDirector = nullptr;

	float NextLeftStickStepTime = 0.0f;
	float NextLeftStickSkinStepTime = 0.0f;

	// 서버 OnRep 전까지 local player에게만 즉시 보이는 선택값
	bool bHasPredictedLobbySelection = false;
	EPragmataPlayerRole PredictedLobbySelectedRole = EPragmataPlayerRole::None;

	bool bHasPredictedLobbySkinSelection = false;
	FName PredictedLobbySelectedSkinId = NAME_None;
};
