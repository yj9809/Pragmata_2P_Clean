#pragma once

#include "CoreMinimal.h"
#include "Data/LobbyPreviewAnimationSet.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "LobbyPresentationDirector.generated.h"

class UAnimationAsset;
class UCameraComponent;
class USceneComponent;
class USkeletalMeshComponent;
class ULobbyCharacterSkinSet;

USTRUCT(BlueprintType)
struct PRAGMATA_2P_API FLobbyCameraFocusSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pragmata|Lobby")
	float OrbitAngleDegrees = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pragmata|Lobby")
	float Radius = 470.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pragmata|Lobby")
	float Height = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pragmata|Lobby")
	float FieldOfView = 58.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pragmata|Lobby")
	FVector LookAtOffset = FVector(0.0, 0.0, 120.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pragmata|Lobby")
	float ScreenRightOffset = 0.0f;
};

UCLASS()
class PRAGMATA_2P_API ALobbyPresentationDirector : public AActor
{
	GENERATED_BODY()

public:
	ALobbyPresentationDirector();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category="Pragmata|Lobby")
	void FocusRole(EPragmataPlayerRole NewFocusedRole, int32 Direction);

	UFUNCTION(BlueprintCallable, Category="Pragmata|Lobby")
	void SetPreviewPresentationEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category="Pragmata|Lobby")
	void SetPreviewSkin(EPragmataPlayerRole PreviewRole, FName SkinId);

	UCameraComponent* GetCameraComponent() const { return CameraComponent; }

private:
	void ResolvePreviewMeshes();
	void ApplyPreviewMeshVisibility(USkeletalMeshComponent* MeshComponent, bool bVisible) const;
	USkeletalMeshComponent* ResolvePreviewMeshByTag(FName ActorTag) const;
	FLobbyCameraFocusSettings GetFocusSettings(EPragmataPlayerRole TargetRole) const;
	float ResolveTargetAngle(float StartAngle, float TargetAngle, int32 Direction, EPragmataPlayerRole TargetRole) const;
	void StartCameraTransition(EPragmataPlayerRole TargetRole, int32 Direction);
	void ApplyCameraPose(float AngleDegrees, float Radius, float Height, float FieldOfView, EPragmataPlayerRole TargetRole);
	FVector ResolveLookAtLocation(EPragmataPlayerRole TargetRole, const FLobbyCameraFocusSettings& FocusSettings) const;
	void PlayPreviewSequence(EPragmataPlayerRole TargetRole);
	void PlayPreviewSequenceForMesh(EPragmataPlayerRole PreviewRole, EPragmataPlayerRole TargetRole, USkeletalMeshComponent* MeshComponent, FTimerHandle& LoopTimerHandle, TObjectPtr<UAnimationAsset>& LoopAnimationStorage);
	bool ResolveAnimationSequence(EPragmataPlayerRole PreviewRole, EPragmataPlayerRole TargetRole, FLobbyPreviewAnimationSequence& OutSequence) const;
	FLobbyPreviewAnimationSequence GetFallbackAnimationSequence(EPragmataPlayerRole PreviewRole, EPragmataPlayerRole TargetRole) const;
	ULobbyCharacterSkinSet* LoadLobbySkinSet();
	void PreloadSkinMeshes();
	UAnimationAsset* LoadAnimation(const TSoftObjectPtr<UAnimationAsset>& Animation, const TCHAR* Context) const;
	float GetAnimationLength(const UAnimationAsset* Animation) const;
	void PlayHughLoopAnimation();
	void PlayDianaLoopAnimation();

	UPROPERTY(VisibleAnywhere, Category="Pragmata|Lobby")
	TObjectPtr<USceneComponent> SceneRoot = nullptr;

	UPROPERTY(VisibleAnywhere, Category="Pragmata|Lobby")
	TObjectPtr<UCameraComponent> CameraComponent = nullptr;

	UPROPERTY(EditAnywhere, Category="Pragmata|Lobby|Camera")
	FVector OrbitCenter = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category="Pragmata|Lobby|Camera")
	FLobbyCameraFocusSettings HughFocusSettings;

	UPROPERTY(EditAnywhere, Category="Pragmata|Lobby|Camera")
	FLobbyCameraFocusSettings DianaFocusSettings;

	UPROPERTY(EditAnywhere, Category="Pragmata|Lobby|Camera", meta=(ClampMin="0.05"))
	float TransitionDuration = 0.65f;

	UPROPERTY(EditAnywhere, Category="Pragmata|Lobby|Actors")
	FName HughPreviewTag = TEXT("LobbyPreview.Hugh");

	UPROPERTY(EditAnywhere, Category="Pragmata|Lobby|Actors")
	FName DianaPreviewTag = TEXT("LobbyPreview.Diana");

	UPROPERTY(EditAnywhere, Category="Pragmata|Lobby|Animation")
	TSoftObjectPtr<ULobbyPreviewAnimationSet> AnimationSet;

	UPROPERTY(EditAnywhere, Category="Pragmata|Lobby|Skin")
	TSoftObjectPtr<ULobbyCharacterSkinSet> SkinSet;

	UPROPERTY(Transient)
	TObjectPtr<ULobbyPreviewAnimationSet> LoadedAnimationSet = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<ULobbyCharacterSkinSet> LoadedSkinSet = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> HughMeshComponent = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> DianaMeshComponent = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UAnimationAsset> HughLoopAnimation = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UAnimationAsset> DianaLoopAnimation = nullptr;

	FTimerHandle HughLoopTimerHandle;
	FTimerHandle DianaLoopTimerHandle;

	EPragmataPlayerRole FocusedRole = EPragmataPlayerRole::None;
	EPragmataPlayerRole TransitionTargetRole = EPragmataPlayerRole::None;
	FName CurrentHughSkinId = NAME_None;
	FName CurrentDianaSkinId = NAME_None;

	bool bCameraInitialized = false;
	bool bCameraTransitionActive = false;
	bool bPreviewPresentationEnabled = false;
	bool bPreviewAnimationInvalidatedBySkinChange = false;
	float TransitionElapsed = 0.0f;
	float TransitionStartAngle = 0.0f;
	float TransitionTargetAngle = 0.0f;
	float TransitionStartRadius = 0.0f;
	float TransitionTargetRadius = 0.0f;
	float TransitionStartHeight = 0.0f;
	float TransitionTargetHeight = 0.0f;
	float TransitionStartFOV = 0.0f;
	float TransitionTargetFOV = 0.0f;
	float CurrentAngle = 0.0f;
	float CurrentRadius = 0.0f;
	float CurrentHeight = 0.0f;
	float CurrentFOV = 58.0f;
};
