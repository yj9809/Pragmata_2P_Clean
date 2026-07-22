// Fill out your copyright notice in the Description page of Project Settings.

#include "Props/BGMRegionActor.h"

#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "Characters/HughCharacter.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Pawns/DianaOperatorPawn.h"
#include "Sound/SoundBase.h"

ABGMRegionActor::ABGMRegionActor()
{
	PrimaryActorTick.bCanEverTick = false;
	SetReplicates(false);

	RegionTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("RegionTrigger"));
	RootComponent = RegionTrigger;
	RegionTrigger->SetBoxExtent(FVector(500.0, 500.0, 200.0));
	ConfigureRegionTriggerCollision();

	MusicAudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("MusicAudioComponent"));
	MusicAudioComponent->SetupAttachment(RootComponent);
	MusicAudioComponent->SetAutoActivate(false);
}

void ABGMRegionActor::BeginPlay()
{
	Super::BeginPlay();

	if (RegionTrigger)
	{
		ConfigureRegionTriggerCollision();
		RegionTrigger->OnComponentBeginOverlap.AddDynamic(this, &ABGMRegionActor::HandleRegionBeginOverlap);
		RegionTrigger->OnComponentEndOverlap.AddDynamic(this, &ABGMRegionActor::HandleRegionEndOverlap);
	}

	if (MusicAudioComponent && BackgroundMusic)
	{
		MusicAudioComponent->SetSound(BackgroundMusic);
	}
	ResetMusicComponentForTriggerPlayback();

	if (!PlayMusicForExistingLocalOverlap())
	{
		StartInitialOverlapRetry();
	}
}

void ABGMRegionActor::PlayRegionMusic()
{
	CancelScheduledMusicPlayback();

	if (!MusicAudioComponent || !BackgroundMusic)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BGMRegionActor] BackgroundMusic is not set. Actor=%s"), *GetNameSafe(this));
		return;
	}

	MusicAudioComponent->SetSound(BackgroundMusic);
	MusicAudioComponent->SetVolumeMultiplier(1.0f);

	if (MusicAudioComponent->IsPlaying())
	{
		if (!bRestartWhenReenter)
		{
			return;
		}

		MusicAudioComponent->Stop();
	}

	MusicAudioComponent->FadeIn(FadeInDuration, TargetVolume, 0.0f);
}

void ABGMRegionActor::StopRegionMusic()
{
	CancelScheduledMusicPlayback();

	if (!MusicAudioComponent || !MusicAudioComponent->IsPlaying())
	{
		return;
	}

	MusicAudioComponent->FadeOut(FadeOutDuration, 0.0f);
}

void ABGMRegionActor::HandleRegionBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	(void)OverlappedComponent;
	(void)OtherComp;
	(void)OtherBodyIndex;
	(void)bFromSweep;
	(void)SweepResult;

	if (!IsLocalControlledPragmataPlayer(OtherActor))
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ExitRecheckTimerHandle);
	}
	ScheduleRegionMusicPlayback();
}

void ABGMRegionActor::HandleRegionEndOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex)
{
	(void)OverlappedComponent;
	(void)OtherComp;
	(void)OtherBodyIndex;

	if (!IsLocalControlledPragmataPlayer(OtherActor))
	{
		return;
	}

	// 지연 중 영역을 떠나면 재생 예약을 취소해 바깥에서 BGM이 시작되지 않게 보장
	CancelScheduledMusicPlayback();

	if (!bStopOnExit)
	{
		return;
	}

	ScheduleStopIfOutsideRegion();
}

// 기존 BP에 저장된 collision 기본값이 달라도 런타임에서 BGM 트리거로 동작하게 보정
void ABGMRegionActor::ConfigureRegionTriggerCollision() const
{
	if (!RegionTrigger)
	{
		return;
	}

	RegionTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	RegionTrigger->SetCollisionObjectType(ECC_WorldDynamic);
	RegionTrigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	RegionTrigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	RegionTrigger->SetGenerateOverlapEvents(true);
}

// BP에 남은 자동 재생 상태가 Trigger 판정을 우회하지 못하게 보정
void ABGMRegionActor::ResetMusicComponentForTriggerPlayback() const
{
	if (!MusicAudioComponent)
	{
		return;
	}

	MusicAudioComponent->SetAutoActivate(false);
	MusicAudioComponent->SetVolumeMultiplier(1.0f);
	if (MusicAudioComponent->IsPlaying())
	{
		MusicAudioComponent->Stop();
	}
}

// Region overlap으로 시작한 재생만 지연하고 Blueprint 직접 호출은 즉시 재생으로 유지
void ABGMRegionActor::ScheduleRegionMusicPlayback()
{
	if (PlaybackDelay <= 0.0f)
	{
		PlayRegionMusic();
		return;
	}

	UWorld* World = GetWorld();
	if (!World || World->GetTimerManager().IsTimerActive(MusicPlaybackDelayTimerHandle))
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		MusicPlaybackDelayTimerHandle,
		this,
		&ABGMRegionActor::HandleDelayedMusicPlayback,
		PlaybackDelay,
		false);
}

// delay가 끝난 시점에도 로컬 플레이어가 Region 안에 있을 때만 BGM을 시작
void ABGMRegionActor::HandleDelayedMusicPlayback()
{
	if (IsLocalPawnInsideRegion())
	{
		PlayRegionMusic();
	}
}

// 재생 대기 중에는 영역 밖으로 나간 뒤 뒤늦게 재생되는 것을 막기 위해 예약을 해제
void ABGMRegionActor::CancelScheduledMusicPlayback()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MusicPlaybackDelayTimerHandle);
	}
}

// 2P possess 타이밍이 늦어 BeginPlay의 overlap 검사를 놓치는 경우를 짧게 보정
// EndOverlap 순간 흔들림으로 BGM이 끊기지 않도록 짧게 재확인한 뒤 정지
void ABGMRegionActor::ScheduleStopIfOutsideRegion()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		StopRegionMusic();
		return;
	}

	World->GetTimerManager().ClearTimer(ExitRecheckTimerHandle);
	if (ExitRecheckDelay <= 0.0f)
	{
		HandleExitRecheck();
		return;
	}

	World->GetTimerManager().SetTimer(
		ExitRecheckTimerHandle,
		this,
		&ABGMRegionActor::HandleExitRecheck,
		ExitRecheckDelay,
		false
	);
}

void ABGMRegionActor::HandleExitRecheck()
{
	if (!IsLocalPawnInsideRegion())
	{
		CancelScheduledMusicPlayback();
		StopRegionMusic();
	}
}

void ABGMRegionActor::StartInitialOverlapRetry()
{
	if (InitialOverlapRetryCount <= 0 || InitialOverlapRetryInterval <= 0.0f)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	RemainingInitialOverlapRetryCount = InitialOverlapRetryCount;
	World->GetTimerManager().SetTimer(
		InitialOverlapRetryTimerHandle,
		this,
		&ABGMRegionActor::HandleInitialOverlapRetry,
		InitialOverlapRetryInterval,
		true
	);
}

// 로컬 Pawn이 뒤늦게 possess되면 기존 overlap 상태를 다시 확인해 BGM을 시작
void ABGMRegionActor::HandleInitialOverlapRetry()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (MusicAudioComponent && MusicAudioComponent->IsPlaying())
	{
		if (bStopOnExit && !IsLocalPawnInsideRegion())
		{
			StopRegionMusic();
		}
		World->GetTimerManager().ClearTimer(InitialOverlapRetryTimerHandle);
		return;
	}

	if (PlayMusicForExistingLocalOverlap())
	{
		World->GetTimerManager().ClearTimer(InitialOverlapRetryTimerHandle);
		return;
	}

	--RemainingInitialOverlapRetryCount;
	if (RemainingInitialOverlapRetryCount <= 0)
	{
		World->GetTimerManager().ClearTimer(InitialOverlapRetryTimerHandle);
	}
}

// 맵 로딩 시점에 이미 트리거 안에 있는 로컬 플레이어도 배경음을 시작할 수 있게 보정
bool ABGMRegionActor::PlayMusicForExistingLocalOverlap()
{
	if (IsLocalPawnInsideRegion())
	{
		ScheduleRegionMusicPlayback();
		return true;
	}

	return false;
}

// 현재 world의 로컬 플레이어 Pawn이 Trigger 안에 있을 때만 지역 BGM을 유지
bool ABGMRegionActor::IsLocalPawnInsideRegion() const
{
	if (!RegionTrigger)
	{
		return false;
	}

	TArray<AActor*> OverlappingActors;
	RegionTrigger->GetOverlappingActors(OverlappingActors, APawn::StaticClass());

	for (AActor* OverlappingActor : OverlappingActors)
	{
		if (IsLocalControlledPragmataPlayer(OverlappingActor))
		{
			return true;
		}
	}

	return false;
}

// 배경음은 네트워크 이벤트가 아니라 각 클라이언트의 로컬 청취 상태이므로 로컬 Pawn만 처리한다.
bool ABGMRegionActor::IsLocalControlledPragmataPlayer(const AActor* Actor) const
{
	const APawn* Pawn = Cast<APawn>(Actor);
	if (!Pawn || !Pawn->IsLocallyControlled())
	{
		return false;
	}

	return Actor->IsA<AHughCharacter>() || Actor->IsA<ADianaOperatorPawn>();
}
