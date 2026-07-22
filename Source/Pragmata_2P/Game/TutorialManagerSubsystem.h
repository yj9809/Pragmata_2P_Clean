#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TimerManager.h"
#include "UObject/SoftObjectPtr.h"
#include "TutorialManagerSubsystem.generated.h"

class UTutorialPopupWidget;
class UTutorialProgressSaveGame;
class UTutorialStepData;
class UWorld;

UCLASS(Blueprintable, Config = Game)
class PRAGMATA_2P_API UTutorialManagerSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "Pragmata|Tutorial")
	void ShowTutorialStep(const UTutorialStepData* StepData);

	UFUNCTION(BlueprintCallable, Category = "Pragmata|Tutorial")
	void HideTutorial();

	void ShowTutorialPresentation(const UTutorialStepData* StepData, uint32 PresentationSerial);
	bool ShowTutorialOnce(FName TutorialId, const UTutorialStepData* StepData, uint32 PresentationSerial);
	void HideTutorialPresentation(uint32 ExpectedPresentationSerial);

private:
	bool TryShowTutorialStep(const UTutorialStepData* StepData);
	uint32 AllocateLocalPresentationSerial();
	void ScheduleTutorialDismiss(const UTutorialStepData* StepData, uint32 ExpectedPresentationSerial);
	void HandleTutorialDismissTimerElapsed(uint32 ExpectedPresentationSerial);
	void HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);
	void LoadTutorialProgress();
	void SaveTutorialProgress() const;
	bool IsRunningInPIE() const;

	// 하드 참조(TSubclassOf)로 두면 에디터 부팅 시 Config 해석이 WBP를 조기 강제 로드해 MVVM 바인딩 데이터가 유실됨 -> 소프트 참조 + 사용 시점 로드
	UPROPERTY(Config, EditDefaultsOnly, Category = "Pragmata|Tutorial")
	TSoftClassPtr<UTutorialPopupWidget> TutorialWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UTutorialPopupWidget> ActiveTutorialWidget;

	UPROPERTY(Transient)
	uint32 ActivePresentationSerial = 0;

	UPROPERTY(Transient)
	TObjectPtr<UTutorialProgressSaveGame> TutorialProgress;

	FTimerHandle TutorialDismissTimerHandle;
	uint32 NextLocalPresentationSerial = MAX_uint32;
	bool bUseTransientTutorialProgress = false;
};
