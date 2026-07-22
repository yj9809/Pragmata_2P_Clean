#pragma once

#include "CoreMinimal.h"
#include "Data/PragmataTypes.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "TutorialTriggerActor.generated.h"

class APragmataPlayerController;
class UBoxComponent;
class UPrimitiveComponent;
class UTutorialStepData;

UENUM(BlueprintType)
enum class ETutorialTriggerDismissPolicy : uint8
{
	UntilReplaced,
	Timed
};

UCLASS(Blueprintable)
class PRAGMATA_2P_API ATutorialTriggerActor : public AActor
{
	GENERATED_BODY()

public:
	ATutorialTriggerActor();

	// client graph 호출을 server로 전달하지 않으므로 authority Event에서만 사용
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Pragmata|Tutorial Trigger", meta=(ToolTip="서버 authority Blueprint graph에서만 호출. Client 호출을 server RPC로 전달하지 않음"))
	void TriggerConfiguredTutorial(AActor* TriggeringActor);

	// 이 actor가 마지막으로 발행한 presentation만 닫음
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Pragmata|Tutorial Trigger", meta=(ToolTip="서버 authority Blueprint graph에서만 호출. 이 trigger의 마지막 presentation만 닫음"))
	void HideConfiguredTutorial();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Pragmata|Tutorial Trigger")
	TObjectPtr<UBoxComponent> TriggerBox = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pragmata|Tutorial Trigger")
	TObjectPtr<UTutorialStepData> TutorialStep = nullptr;

	// SaveGame 키이므로 배포 후 이름을 바꾸거나 다른 tutorial과 공유하지 않음
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pragmata|Tutorial Trigger")
	FName TutorialId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pragmata|Tutorial Trigger")
	EPragmataPlayerRole ActivationRole = EPragmataPlayerRole::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pragmata|Tutorial Trigger")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pragmata|Tutorial Trigger")
	ETutorialTriggerDismissPolicy DismissPolicy = ETutorialTriggerDismissPolicy::UntilReplaced;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pragmata|Tutorial Trigger", meta=(EditCondition="DismissPolicy == ETutorialTriggerDismissPolicy::Timed", EditConditionHides, ClampMin="0.1"))
	float DisplayDurationSeconds = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pragmata|Tutorial Trigger|Objective Alert")
	bool bShowObjectiveAlertOnExit = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pragmata|Tutorial Trigger|Objective Alert", meta=(EditCondition="bShowObjectiveAlertOnExit", EditConditionHides))
	FText ExitObjectiveAlertText;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pragmata|Tutorial Trigger|Objective Alert", meta=(EditCondition="bShowObjectiveAlertOnExit", EditConditionHides, ClampMin="0.1"))
	float ExitObjectiveAlertDisplaySeconds = 10.f;

private:
	UFUNCTION()
	void HandleTriggerBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void HandleTriggerEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex);

	void HandleDismissTimerElapsed();
	APragmataPlayerController* GetValidTriggeringController(AActor* TriggeringActor) const;

	uint32 LatestPresentationSerial = 0;
	bool bExitObjectiveAlertShown = false;
	FTimerHandle DismissTimerHandle;
};
