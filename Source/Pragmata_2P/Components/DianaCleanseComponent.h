#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TimerManager.h"
#include "UObject/SoftObjectPtr.h"
#include "DianaCleanseComponent.generated.h"

UENUM()
enum class EDianaCleanseMode : uint8
{
	None,
	PrecisionDianaCamera,
	QuickHughAim
};

UENUM()
enum class EDianaCleanseAnimationPhase : uint8
{
	Start,
	Loop,
	Release,
	Stop
};

class UDecalComponent;
class UAnimSequenceBase;
class UMaterialInstanceDynamic;
class UMaterialInterface;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDianaCleanseCooldownChangedSignature, bool, bCanUseCleanse, float, CooldownProgress);

UCLASS(ClassGroup=(Pragmata), meta=(BlueprintSpawnableComponent))
class PRAGMATA_2P_API UDianaCleanseComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDianaCleanseComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category="Pragmata|Diana|Cleanse")
	void RequestCleanse();

	void BeginPrecisionCleanse();
	void CommitPrecisionCleanse();
	void CancelPrecisionCleanse();
	void RequestQuickCleanse();
	bool IsCleansePreviewActive() const { return ActiveCleanseMode != EDianaCleanseMode::None; }
	bool IsServerCleanseExecuting() const { return bServerCleanseExecuting; }

	UFUNCTION(BlueprintPure, Category="Pragmata|Diana|Cleanse")
	bool CanUseCleanse() const { return bCanUseCleanse; }

	UFUNCTION(BlueprintPure, Category="Pragmata|Diana|Cleanse")
	float GetCleanseCooldownProgress() const;

	UPROPERTY(BlueprintAssignable, Category="Pragmata|Diana|Cleanse")
	FDianaCleanseCooldownChangedSignature OnCleanseCooldownChanged;

private:
	UFUNCTION(Server, Reliable)
	void Server_BeginPrecisionCleanse();

	UFUNCTION(Server, Reliable)
	void Server_CommitPrecisionCleanse();

	UFUNCTION(Server, Reliable)
	void Server_CancelPrecisionCleanse();

	UFUNCTION(Server, Reliable)
	void Server_RequestQuickCleanse();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_SetCleansePreviewMode(EDianaCleanseMode NewMode, FVector PreviewDirection);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_StartCleanseCooldown(float CooldownSeconds);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayCleanseAnimation(EDianaCleanseAnimationPhase Phase);

	UFUNCTION()
	void OnRep_CleanseCooldownState();

	bool CanStartCleanse(float CurrentTime, EDianaCleanseMode CleanseMode, FVector& OutCleanseOrigin, FVector& OutCleanseDirection) const;
	bool ResolveCleanseModeAim(EDianaCleanseMode CleanseMode, FVector& OutCleanseOrigin, FVector& OutCleanseDirection) const;
	AActor* ResolveCleansePreviewSourceActor(EDianaCleanseMode CleanseMode) const;
	FVector ResolveCleansePreviewDirection() const;
	void ExecutePendingQuickCleanse();
	void BeginCleanseLoopAnimation();
	void PlayCleanseAnimation(EDianaCleanseAnimationPhase Phase);
	void PlayCleanseVoiceOnInput();
	void ApplyCleanseFromMode(EDianaCleanseMode CleanseMode, float CurrentTime);
	void StartCleanseCooldown(float CurrentTime);
	void HandleCleanseCooldownElapsed();
	void BroadcastCleanseCooldownChanged();
	float GetCurrentServerTimeSeconds() const;
	void CollectCleanseTargets(const FVector& CameraOrigin, const FVector& CameraDirection, TArray<AActor*>& OutTargets) const;
	bool IsTargetInCleanseCone(AActor* TargetActor, const FVector& CameraOrigin, const FVector& CameraDirection) const;
	bool HasLineOfSightToTarget(AActor* TargetActor, const FVector& CameraOrigin) const;
	void ApplyCleanseToTarget(AActor* TargetActor);
	bool CanShowCleansePreviewBase(const class ADianaOperatorPawn* DianaPawn) const;
	bool CanShowCleansePreview(const class ADianaOperatorPawn* DianaPawn) const;
	bool IsLocalCleansePreviewCooldownActive(float CurrentTime) const;
	void StartLocalCleansePreviewCooldown(float CooldownSeconds);
	bool ResolveCleansePreviewSurface(const AActor* SourceActor, FHitResult& OutHit) const;
	void UpdateCleansePreview();
	void UpdateCleansePreviewMaterialParameters(const FVector& PreviewStart, const FVector& Forward, const FVector& SurfaceNormal);
	void HideCleansePreview();
	UDecalComponent* EnsureCleansePreviewComponent();

	UPROPERTY(EditDefaultsOnly, Category="Pragmata|Diana Cleanse", meta=(ClampMin="0.1", Units="s"))
	float CleanseCooldownSeconds = 5.f;

	UPROPERTY(EditDefaultsOnly, Category="Pragmata|Diana Cleanse", meta=(ClampMin="0.01", Units="s"))
	float QuickCleansePreviewSeconds = 0.5f;

	UPROPERTY(EditDefaultsOnly, Category="Pragmata|Diana Cleanse", meta=(ClampMin="0", Units="cm"))
	float CleanseRange = 3000.f;

	UPROPERTY(EditDefaultsOnly, Category="Pragmata|Diana Cleanse", meta=(ClampMin="0", ClampMax="180", Units="deg"))
	float CleanseHalfAngleDegrees = 30.f;

	UPROPERTY(EditDefaultsOnly, Category="Pragmata|Diana Cleanse")
	TEnumAsByte<ECollisionChannel> CleanseTraceChannel = ECC_Visibility;

	UPROPERTY(EditDefaultsOnly, Category="Pragmata|Diana Cleanse|Preview")
	TSoftObjectPtr<UMaterialInterface> CleansePreviewMaterial;

	UPROPERTY(EditDefaultsOnly, Category="Pragmata|Diana Cleanse|Animation")
	TSoftObjectPtr<UAnimSequenceBase> CleanseStartAnimation;

	UPROPERTY(EditDefaultsOnly, Category="Pragmata|Diana Cleanse|Animation")
	TSoftObjectPtr<UAnimSequenceBase> CleanseLoopAnimation;

	UPROPERTY(EditDefaultsOnly, Category="Pragmata|Diana Cleanse|Animation")
	TSoftObjectPtr<UAnimSequenceBase> CleanseReleaseAnimation;

	UPROPERTY(EditDefaultsOnly, Category="Pragmata|Diana Cleanse|Preview", meta=(ClampMin="0", Units="cm"))
	float CleansePreviewTraceUpDistance = 150.f;

	UPROPERTY(EditDefaultsOnly, Category="Pragmata|Diana Cleanse|Preview", meta=(ClampMin="0", Units="cm"))
	float CleansePreviewTraceDownDistance = 700.f;

	UPROPERTY(EditDefaultsOnly, Category="Pragmata|Diana Cleanse|Preview", meta=(ClampMin="1", Units="cm"))
	float CleansePreviewProjectionDepth = 256.f;

	UPROPERTY(EditDefaultsOnly, Category="Pragmata|Diana Cleanse|Preview", meta=(ClampMin="0", Units="cm"))
	float CleansePreviewSurfaceOffset = 5.f;

	UPROPERTY(Transient)
	TObjectPtr<UDecalComponent> CleansePreviewDecal = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> CleansePreviewMaterialInstance = nullptr;

	UPROPERTY(Transient)
	float LocalCleansePreviewHiddenUntilTime = 0.f;

	UPROPERTY(ReplicatedUsing=OnRep_CleanseCooldownState, BlueprintReadOnly, VisibleInstanceOnly, Category="Pragmata|Diana Cleanse", meta=(AllowPrivateAccess="true"))
	bool bCanUseCleanse = true;

	UPROPERTY(ReplicatedUsing=OnRep_CleanseCooldownState, VisibleInstanceOnly, Category="Pragmata|Diana Cleanse")
	float CleanseCooldownStartServerTime = 0.f;

	UPROPERTY(ReplicatedUsing=OnRep_CleanseCooldownState, VisibleInstanceOnly, Category="Pragmata|Diana Cleanse", meta=(ClampMin="0.0"))
	float CleanseCooldownDurationSeconds = 0.f;

	FTimerHandle QuickCleanseTimerHandle;
	FTimerHandle CleanseStartAnimationTimerHandle;
	FTimerHandle CleanseCooldownTimerHandle;
	float NextAllowedCleanseTime = 0.f;
	EDianaCleanseMode ActiveCleanseMode = EDianaCleanseMode::None;
	FVector ActiveCleansePreviewDirection = FVector::ForwardVector;
	bool bServerCleanseExecuting = false;
};
