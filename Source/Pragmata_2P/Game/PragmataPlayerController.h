#pragma once

#include "CoreMinimal.h"
#include "Data/PragmataTypes.h"
#include "GameFramework/PlayerController.h"
#include "UObject/SoftObjectPtr.h"
#include "PragmataPlayerController.generated.h"

class UInputMappingContext;
class UPragmataHUDWidget;
class UUserWidget;
class APragmataPlayerState;
class APragmataGameState;
class AHughCharacter;
class ADianaOperatorPawn;
class AActor;
class APostProcessVolume;
class UWidgetComponent;
class UPragmataUpgradeMenuWidget;
class UPragmataGameClearWidget;
class UTutorialStepData;
class ACleanseSkillAcquisitionProp;
struct FInputKeyEventArgs;

UCLASS()
class PRAGMATA_2P_API APragmataPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	APragmataPlayerController();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void AcknowledgePossession(APawn* P) override;
	virtual void OnRep_PlayerState() override;
	virtual void PlayerTick(float DeltaTime) override;
	virtual bool InputKey(const FInputKeyEventArgs& Params) override;

	UFUNCTION(BlueprintPure, Category="Pragmata|Player")
	EPragmataPlayerRole GetAssignedRole() const;

	UFUNCTION(BlueprintPure, Category="Pragmata|Player")
	EPragmataPlayerRole GetActiveControlRole() const;

	UFUNCTION(BlueprintPure, Category="Pragmata|Input")
	bool IsGamepadInput() const { return bIsGamepadInput; }

	void SetAssignedRole(EPragmataPlayerRole NewRole);
	void SetCrosshairVisibility(bool bIsVisible);

	UFUNCTION(BlueprintCallable, Category="Pragmata|Objective")
	void ShowObjectiveAlert(const FText& ObjectiveText, float DisplaySeconds = 4.f);

	// 로컬 owning HUD에만 대사를 표시
	UFUNCTION(BlueprintCallable, Category="Pragmata|Dialogue")
	void PlayDialogueLine(const FDialogueLine& Line, float DisplaySeconds = -1.f);

	UFUNCTION(BlueprintCallable, Category="Pragmata|Dialogue")
	void HideDialogue();

	void ShowGameClearPresentation();
	void ReturnToTitleFromGameClear();

	UFUNCTION(BlueprintCallable, Category="Pragmata|Game Clear")
	void RequestReturnToTitleFromGameClear();

	UFUNCTION(Client, Reliable)
	void Client_ShowPOIScanResults(const TArray<FPragmataScanResult>& ScanResults, float ScanDuration);

	UFUNCTION(Client, Reliable)
	void Client_ShowObjectiveAlert(const FText& ObjectiveText, float DisplaySeconds);

	// 서버가 특정 클라이언트의 로컬 HUD에 대사 표시를 지시
	// Level BP 등에서 node로 호출할 수 있게 BlueprintCallable 유지
	UFUNCTION(BlueprintCallable, Client, Reliable, Category="Pragmata|Dialogue")
	void Client_PlayDialogueLine(const FDialogueLine& Line, float DisplaySeconds);

	// 서버가 특정 클라이언트의 현재 대사를 닫도록 지시
	UFUNCTION(BlueprintCallable, Client, Reliable, Category="Pragmata|Dialogue")
	void Client_HideDialogue();

	UFUNCTION(Client, Reliable)
	void Client_PlayDianaScanPulse(FVector ScanOrigin, float PulseRadius, float PulseDuration);

	UFUNCTION(Client, Reliable)
	void Client_ShowTutorialStep(UTutorialStepData* StepData);

	UFUNCTION(Client, Reliable)
	void Client_SetCheckpointUpgradeMenuOpen(bool bOpen, bool bInteractive);

	UFUNCTION(BlueprintCallable, Category="Pragmata|Upgrade")
	void RequestUpgrade(EPragmataUpgradeType UpgradeType);

	UFUNCTION(BlueprintCallable, Category="Pragmata|Upgrade")
	void RequestRefundUpgrade(EPragmataUpgradeType UpgradeType);
	void RequestCheckpointUpgradeSelection(EPragmataUpgradeType UpgradeType);
	void RequestCheckpointUpgradeHold(float Progress, bool bRefund);
	void RequestCloseCheckpointUpgradeMenu();

	UPROPERTY(BlueprintReadOnly, Category="Pragmata|Input")
	bool bIsGamepadInput = false;

private:
	void BindPlayerStateRole();
	void HandleAssignedRoleChanged(EPragmataPlayerRole NewRole);
	void HandleActiveControlRoleChanged(EPragmataPlayerRole NewRole);
	void UpdateInputDeviceFromKeyEvent(const FInputKeyEventArgs& Params);
	void ApplyGameplayInputMode();
	void ApplyRoleInputAndCamera();
	void EnsureGameplayHUD();
	void EnsureHughCrosshair();
	void BindGameplayHackState();
	void HandleActiveEnemyHackTargetChanged(AActor* TargetActor);
	void HandleActiveMapPuzzleActorChanged(AActor* PuzzleActor);
	void HandleActiveBossTargetChanged(AActor* BossTarget);
	void HandleEnemyCombatActiveChanged(bool bNewActive);
	void HandleHughCharacterChanged(AHughCharacter* HughCharacter);
	void HandleDianaOperatorPawnChanged(ADianaOperatorPawn* DianaPawn);
	void RebindSharedHUDStats();
	bool ApplyUpgradeRequest(EPragmataUpgradeType UpgradeType, bool bRefund);
	AHughCharacter* ResolveSharedHughCharacter() const;
	void EnsureScanPulseVolume();
	void UpdateScanPulse();
	void StopScanPulse();
	void UpdateCleanseAcquisitionVicinityIndicators();
	void ShowScanIconForResult(const FPragmataScanResult& ScanResult);
	void HideActiveScanIcons();
	void SetCheckpointUpgradeMenuOpen(bool bOpen, bool bInteractive);
	void CreateGameClearWidget();
	void FinishGameClearPresentationFade();
	void DebugAddLunafilamentForUITest(int32 Amount);
	void ApplyDebugAddLunafilamentForUITest(int32 Amount);
	void DebugDealPlayerDamageForDeathTest(float DamageAmount, EPragmataHitDirection HitDirection);
	void ApplyDebugDealPlayerDamageForDeathTest(float DamageAmount, EPragmataHitDirection HitDirection);

	bool TryGetGameplayHUDClassForAssignedRole(TSoftClassPtr<UPragmataHUDWidget>& OutHUDClass) const;
	UInputMappingContext* LoadInputMappingContext(const TSoftObjectPtr<UInputMappingContext>& MappingContext) const;

	UFUNCTION(Server, Reliable)
	void Server_DebugAddLunafilamentForUITest(int32 Amount);

	UFUNCTION(Server, Reliable)
	void Server_DebugDealPlayerDamageForDeathTest(float DamageAmount, EPragmataHitDirection HitDirection);

	UFUNCTION(Server, Reliable)
	void Server_RequestUpgrade(EPragmataUpgradeType UpgradeType);

	UFUNCTION(Server, Reliable)
	void Server_RequestRefundUpgrade(EPragmataUpgradeType UpgradeType);

	UFUNCTION(Server, Reliable)
	void Server_RequestCheckpointUpgradeSelection(EPragmataUpgradeType UpgradeType);

	UFUNCTION(Server, Unreliable)
	void Server_RequestCheckpointUpgradeHold(float Progress, bool bRefund);

	UFUNCTION(Server, Reliable)
	void Server_RequestCloseCheckpointUpgradeMenu();
	UFUNCTION(Server, Reliable)
	void Server_RequestReturnToTitleFromGameClear();
	UPROPERTY(EditDefaultsOnly, Category="Pragmata|UI")
	TSoftClassPtr<UUserWidget> HughCrosshairWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category="Pragmata|Input")
	TSoftObjectPtr<UInputMappingContext> HughInputMappingContext;

	UPROPERTY(EditDefaultsOnly, Category="Pragmata|Input")
	TSoftObjectPtr<UInputMappingContext> DianaInputMappingContext;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> AppliedMappingContext = nullptr;

	bool bDebugDamageDirectionModifierHeld = false;

	UPROPERTY(Transient)
	TObjectPtr<APragmataPlayerState> BoundPragmataPlayerState = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<APragmataGameState> BoundPragmataGameState = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UPragmataHUDWidget> GameplayHUD = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UPragmataGameClearWidget> GameClearWidget = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> HughCrosshairWidget = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<APostProcessVolume> ScanPulseVolume = nullptr;

	EPragmataPlayerRole CachedAssignedRole = EPragmataPlayerRole::None;
	EPragmataPlayerRole CachedActiveControlRole = EPragmataPlayerRole::None;
	float ScanPulseStartTime = 0.f;
	float ScanPulseDuration = 0.f;
	bool bScanPulseActive = false;
	TMap<TObjectPtr<AActor>, FTimerHandle> ScanIconRevealTimers;

	// 현재 scan lifecycle에서 점등된 actor들과 마지막 reveal 기준 표시 시간
	TArray<TWeakObjectPtr<AActor>> ActiveScanIconActors;
	FTimerHandle ScanIconHideTimer;
	FTimerHandle GameClearPresentationTimerHandle;
	FTimerHandle GameClearReturnTimerHandle;
	float ActiveScanIconDisplayDuration = 5.f;
	bool bGameClearPresentationActive = false;
	bool bGameClearInputReady = false;
	bool bReturningToTitleFromGameClear = false;

	UPROPERTY(EditDefaultsOnly, Category="Pragmata|Upgrade")
	TSoftClassPtr<UPragmataUpgradeMenuWidget> UpgradeMenuWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UPragmataUpgradeMenuWidget> CheckpointUpgradeMenu = nullptr;
	
protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float ViewPitchLimit = 60.f;
};
