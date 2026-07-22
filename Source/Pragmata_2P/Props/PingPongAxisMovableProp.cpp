#include "Props/PingPongAxisMovableProp.h"

#include "Engine/World.h"

namespace
{
constexpr float MinimumAutoMoveInterval = 0.05f;
}

APingPongAxisMovableProp::APingPongAxisMovableProp()
{
}

void APingPongAxisMovableProp::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority() && bAutoMoveOnBeginPlay)
	{
		StartAutoMove();
	}
}

void APingPongAxisMovableProp::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopAutoMove();

	Super::EndPlay(EndPlayReason);
}

void APingPongAxisMovableProp::StartAutoMove()
{
	if (!HasAuthority())
	{
		return;
	}

	StopAutoMove();
	if (InitialMoveDelay <= KINDA_SMALL_NUMBER)
	{
		HandleAutoMoveTimer();
		return;
	}

	ScheduleNextAutoMove(InitialMoveDelay);
}

void APingPongAxisMovableProp::StopAutoMove()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AutoMoveTimerHandle);
	}
}

void APingPongAxisMovableProp::HandleAutoMoveTimer()
{
	if (!HasAuthority())
	{
		return;
	}

	// 서버가 목표 상태만 왕복 토글하고 실제 보간/복제는 부모 이동 경로를 재사용
	SetMoved(!IsMoved());
	ScheduleNextAutoMove(MoveDuration + WaitAtEndsDuration);
}

void APingPongAxisMovableProp::ScheduleNextAutoMove(float DelaySeconds)
{
	if (!HasAuthority())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float SafeDelay = FMath::Max(DelaySeconds, MinimumAutoMoveInterval);
	World->GetTimerManager().SetTimer(
		AutoMoveTimerHandle,
		this,
		&APingPongAxisMovableProp::HandleAutoMoveTimer,
		SafeDelay,
		false
	);
}
