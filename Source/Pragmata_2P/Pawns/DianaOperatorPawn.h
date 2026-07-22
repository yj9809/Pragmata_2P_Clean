#pragma once

#include "CoreMinimal.h"
#include "Data/PragmataTypes.h"
#include "GameFramework/Pawn.h"
#include "UObject/SoftObjectPtr.h"
#include "DianaOperatorPawn.generated.h"

class UCameraComponent;
class UDianaHackComponent;
class UDianaNodeInteractionComponent;
class UDianaCleanseComponent;
class UDianaScanComponent;
class UDianaVoiceComponent;
class UInputAction;
class UPragmataHUDWidget;
class USceneComponent;
class USpringArmComponent;
struct FInputActionValue;

class UOverDriveComponent;

UCLASS()
class PRAGMATA_2P_API ADianaOperatorPawn : public APawn
{
	GENERATED_BODY()

public:
	ADianaOperatorPawn();

	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, Category="Pragmata|Diana")
	void AddCameraInput(FVector2D LookInput);

	UFUNCTION(BlueprintCallable, Category="Pragmata|Diana")
	void RequestScan();

	UFUNCTION(BlueprintCallable, Category="Pragmata|Diana")
	void RequestHack(AActor* TargetActor);

	UFUNCTION(BlueprintCallable, Category="Pragmata|Diana")
	void RequestNodeInteraction();

	UFUNCTION(BlueprintCallable, Category="Pragmata|Diana")
	void RequestCleanse();

	UFUNCTION(BlueprintCallable, Category="Pragmata|Diana")
	void BeginPrecisionCleanse();

	UFUNCTION(BlueprintCallable, Category="Pragmata|Diana")
	void CommitPrecisionCleanse();

	UFUNCTION(BlueprintCallable, Category="Pragmata|Diana")
	void CancelPrecisionCleanse();

	UFUNCTION(BlueprintCallable, Category="Pragmata|Diana")
	void RequestQuickCleanse();

	UFUNCTION(BlueprintCallable, Category="Pragmata|Diana")
	void SetAimingState(bool bNewAiming);

	UFUNCTION(BlueprintCallable, Category="Pragmata|Diana")
	void SetOperatorMode(EDianaOperatorMode NewMode);

	UFUNCTION(BlueprintPure, Category="Pragmata|Diana")
	EDianaOperatorMode GetOperatorMode() const { return OperatorMode; }

	UFUNCTION(BlueprintPure, Category="Pragmata|Diana")
	bool GetIsAiming() const { return bIsAiming; }

	UFUNCTION(BlueprintPure, Category="Pragmata|Diana|Animation")
	uint8 GetEnemyHackSuccessSerial() const { return EnemyHackSuccessSerial; }

	void NotifyEnemyHackSucceeded();

	UFUNCTION(BlueprintPure, Category="Pragmata|Diana")
	UOverDriveComponent* GetOverDriveComponent() const { return OverDriveComponent; }

	UFUNCTION(BlueprintPure, Category="Pragmata|Diana")
	UDianaCleanseComponent* GetCleanseComponent() const { return CleanseComponent; }

	UFUNCTION(BlueprintPure, Category="Pragmata|Diana")
	UDianaVoiceComponent* GetVoiceComponent() const { return VoiceComponent; }

	const TSoftClassPtr<UPragmataHUDWidget>& GetGameplayHUDClass() const { return GameplayHUDClass; }

	UFUNCTION(BlueprintCallable, Category="Pragmata|Objective")
	void ShowObjectiveAlert(float DisplaySeconds = 4.f);

	UFUNCTION(BlueprintCallable, Category="Pragmata|Objective")
	void ShowObjectiveAlertText(const FText& ObjectiveText, float DisplaySeconds = 4.f);

	FVector GetAuthoritativeCameraOrigin() const;
	FVector GetAuthoritativeCameraDirection() const;
	bool CanUseDianaActionIntent() const;

	UFUNCTION(BlueprintCallable, Category="Pragmata|OverDrive")
	void RequestOverDrive();

	bool IsHackActionActive() const;

private:
	UFUNCTION(Server, Unreliable)
	void Server_SetViewRotation(FRotator NewViewRotation);

	UFUNCTION(Server, Reliable)
	void Server_SetAimingState(bool bNewAiming);

	UFUNCTION()
	void OnRep_ServerViewRotation();

	void ResolveInputActions();
	void OnTriggerLook(const FInputActionValue& Value);
	void OnStartAim(const FInputActionValue& Value);
	void OnCompleteAim(const FInputActionValue& Value);
	void OnStartScan(const FInputActionValue& Value);
	void OnStartInteract(const FInputActionValue& Value);
	void OnStartCleanse(const FInputActionValue& Value);
	void OnCompleteCleanse(const FInputActionValue& Value);
	void OnHackDirectionUp();
	void OnHackDirectionDown();
	void OnHackDirectionLeft();
	void OnHackDirectionRight();
	void OnOverDriveLeftStickPressed();
	void OnOverDriveRightStickPressed();
	void TryRequestOverDriveFromStick(bool bLeftStick);
	AActor* FindHackTargetAtReticle() const;
	AActor* ResolveHackableActor(AActor* Candidate) const;
	bool HasLineOfSightToHackTarget(AActor* Candidate, const FVector& TraceStart) const;
	void UpdateEnemyHackAim(float DeltaSeconds);
	void RequestHackDirection(EHackDirection Direction);
	void RequestCancelHack(AActor* ExpectedEnemyHackTarget = nullptr);
	void FollowHughAnchor();
	void UpdateAimCamera(float DeltaSeconds);
	FRotator ClampViewRotation(const FRotator& DesiredRotation) const;
	void ApplyViewRotation(const FRotator& NewViewRotation);
	bool IsOwnedByDianaPlayer() const;
	FVector GetCameraOrigin() const;
	FVector GetCameraDirection() const;
	bool CanUseScanOrHack() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Pragmata|Diana Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> SceneRoot = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Pragmata|Diana Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USpringArmComponent> CameraBoom = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Pragmata|Diana Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UCameraComponent> CameraComponent = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Pragmata|Diana Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UDianaHackComponent> HackComponent = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Pragmata|Diana Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UDianaScanComponent> ScanComponent = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Pragmata|Diana Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UDianaNodeInteractionComponent> NodeInteractionComponent = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Pragmata|Diana Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UDianaCleanseComponent> CleanseComponent = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Pragmata|Diana Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UOverDriveComponent> OverDriveComponent = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Pragmata|Diana Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UDianaVoiceComponent> VoiceComponent = nullptr;

	// BP_DianaOperatorPawn defaults에서 UI 디자이너가 교체할 Diana HUD widget
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Pragmata|UI", meta=(AllowPrivateAccess="true"))
	TSoftClassPtr<UPragmataHUDWidget> GameplayHUDClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pragmata|Objective", meta=(AllowPrivateAccess="true", MultiLine="true"))
	FText ObjectiveAlertText;

	UPROPERTY(Replicated, VisibleInstanceOnly, Category="Pragmata|Diana State")
	EDianaOperatorMode OperatorMode = EDianaOperatorMode::Roaming;

	UPROPERTY(Replicated, VisibleInstanceOnly, Category="Pragmata|Diana State")
	bool bIsAiming = false;

	UPROPERTY(Replicated, VisibleInstanceOnly, Category="Pragmata|Diana State|Animation")
	uint8 EnemyHackSuccessSerial = 0;

	// 서버가 검증에 사용할 Diana 시점
	UPROPERTY(ReplicatedUsing=OnRep_ServerViewRotation, VisibleInstanceOnly, Category="Pragmata|Diana State")
	FRotator ServerViewRotation = FRotator::ZeroRotator;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> LookAction = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> ScanAction = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> InteractAction = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> AimAction = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> CleanseAction = nullptr;

	UPROPERTY(EditDefaultsOnly, Category="Pragmata|Diana Camera", meta=(ClampMin="0"))
	float CameraYawSpeed = 5.f;

	UPROPERTY(EditDefaultsOnly, Category="Pragmata|Diana Camera", meta=(ClampMin="0"))
	float CameraPitchSpeed = 5.f;

	UPROPERTY(EditDefaultsOnly, Category="Pragmata|Diana Camera", meta=(ClampMin="-89", ClampMax="0"))
	float MinPitch = -70.f;

	UPROPERTY(EditDefaultsOnly, Category="Pragmata|Diana Camera", meta=(ClampMin="0", ClampMax="89"))
	float MaxPitch = 70.f;

	UPROPERTY(EditDefaultsOnly, Category="Pragmata|Diana Camera")
	FVector HughAnchorOffset = FVector::ZeroVector;

	UPROPERTY(EditDefaultsOnly, Category="Pragmata|Diana Camera", meta=(ClampMin="0"))
	float ReticleTraceDistance = 3000.f;

	UPROPERTY(EditDefaultsOnly, Category="Pragmata|Diana Aim Assist", meta=(ClampMin="0"))
	float EnemyHackAimSweepRadius = 160.f;

	UPROPERTY(EditDefaultsOnly, Category="Pragmata|Diana Aim Assist", meta=(ClampMin="0.05"))
	float EnemyHackAimRetryInterval = 0.35f;

	UPROPERTY(EditDefaultsOnly, Category="Pragmata|Diana Aim Assist", meta=(ClampMin="0"))
	float EnemyHackAimLostCancelDelay = 0.15f;

	UPROPERTY(EditDefaultsOnly, Category="Pragmata|Diana Camera|Aim", meta=(ClampMin="0"))
	float DefaultFOV = 90.f;

	UPROPERTY(EditDefaultsOnly, Category="Pragmata|Diana Camera|Aim", meta=(ClampMin="0"))
	float AimFOV = 60.f;

	UPROPERTY(EditDefaultsOnly, Category="Pragmata|Diana Camera|Aim", meta=(ClampMin="0"))
	float DefaultArmLength = 270.f;

	UPROPERTY(EditDefaultsOnly, Category="Pragmata|Diana Camera|Aim", meta=(ClampMin="0"))
	float AimArmLength = 200.f;

	UPROPERTY(EditDefaultsOnly, Category="Pragmata|Diana Camera|Aim", meta=(ClampMin="0"))
	float AimZoomSpeed = 10.f;

	float EnemyHackAimRetryTimer = 0.f;

	float LastOverDriveLeftStickPressTime = -BIG_NUMBER;
	float LastOverDriveRightStickPressTime = -BIG_NUMBER;

	UPROPERTY(EditDefaultsOnly, Category="Pragmata|OverDrive", meta=(ClampMin="0.0", Units="s"))
	float OverDriveStickPressWindow = 0.25f;
	float EnemyHackAimLostTimer = 0.f;

	UPROPERTY(Transient)
	TObjectPtr<AActor> LastRequestedEnemyHackTarget = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<AActor> PendingEnemyHackCancelTarget = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<AActor> LastObservedActiveEnemyHackTarget = nullptr;
};
