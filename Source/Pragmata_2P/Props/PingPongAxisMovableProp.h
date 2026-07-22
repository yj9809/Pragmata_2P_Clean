#pragma once

#include "CoreMinimal.h"
#include "Props/AxisMovableProp.h"
#include "TimerManager.h"
#include "PingPongAxisMovableProp.generated.h"

UCLASS()
class PRAGMATA_2P_API APingPongAxisMovableProp : public AAxisMovableProp
{
	GENERATED_BODY()

public:
	APingPongAxisMovableProp();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintCallable, Category="Pragmata|Movable Prop|Auto")
	void StartAutoMove();

	UFUNCTION(BlueprintCallable, Category="Pragmata|Movable Prop|Auto")
	void StopAutoMove();

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pragmata|Movable Prop|Auto")
	bool bAutoMoveOnBeginPlay = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pragmata|Movable Prop|Auto", meta=(ClampMin="0"))
	float InitialMoveDelay = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pragmata|Movable Prop|Auto", meta=(ClampMin="0"))
	float WaitAtEndsDuration = 0.f;

private:
	void HandleAutoMoveTimer();
	void ScheduleNextAutoMove(float DelaySeconds);

	FTimerHandle AutoMoveTimerHandle;
};
