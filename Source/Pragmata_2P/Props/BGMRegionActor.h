// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "BGMRegionActor.generated.h"

class UAudioComponent;
class UBoxComponent;
class UPrimitiveComponent;
class USoundBase;

struct FHitResult;

UCLASS(Blueprintable)
class PRAGMATA_2P_API ABGMRegionActor : public AActor
{
	GENERATED_BODY()

public:
	ABGMRegionActor();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pragmata|BGM")
	TObjectPtr<UBoxComponent> RegionTrigger;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pragmata|BGM")
	TObjectPtr<UAudioComponent> MusicAudioComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pragmata|BGM")
	TObjectPtr<USoundBase> BackgroundMusic;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pragmata|BGM", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float FadeInDuration = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pragmata|BGM", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float FadeOutDuration = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pragmata|BGM", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float TargetVolume = 1.0f;

	// 로컬 플레이어가 Region 안에 남아 있을 때만 적용하는 진입 후 재생 대기 시간
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pragmata|BGM", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s", DisplayName = "Playback Delay"))
	float PlaybackDelay = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pragmata|BGM")
	bool bRestartWhenReenter = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pragmata|BGM")
	bool bStopOnExit = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pragmata|BGM", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float InitialOverlapRetryInterval = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pragmata|BGM", meta = (ClampMin = "0", UIMin = "0"))
	int32 InitialOverlapRetryCount = 8;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pragmata|BGM", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float ExitRecheckDelay = 0.15f;

	UFUNCTION(BlueprintCallable, Category = "Pragmata|BGM")
	void PlayRegionMusic();

	UFUNCTION(BlueprintCallable, Category = "Pragmata|BGM")
	void StopRegionMusic();

private:
	UFUNCTION()
	void HandleRegionBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void HandleRegionEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex);

	void ConfigureRegionTriggerCollision() const;
	void ResetMusicComponentForTriggerPlayback() const;
	void ScheduleRegionMusicPlayback();
	void HandleDelayedMusicPlayback();
	void CancelScheduledMusicPlayback();
	void ScheduleStopIfOutsideRegion();
	void HandleExitRecheck();
	void StartInitialOverlapRetry();
	void HandleInitialOverlapRetry();
	bool PlayMusicForExistingLocalOverlap();
	bool IsLocalPawnInsideRegion() const;
	bool IsLocalControlledPragmataPlayer(const AActor* Actor) const;

	FTimerHandle InitialOverlapRetryTimerHandle;
	FTimerHandle ExitRecheckTimerHandle;
	FTimerHandle MusicPlaybackDelayTimerHandle;
	int32 RemainingInitialOverlapRetryCount = 0;
};
