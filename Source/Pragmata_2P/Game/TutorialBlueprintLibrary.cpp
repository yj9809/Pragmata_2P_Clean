#include "Game/TutorialBlueprintLibrary.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Game/PragmataGameState.h"
#include "Game/TutorialManagerSubsystem.h"

bool UTutorialBlueprintLibrary::ShowTutorialOnce(const UObject* WorldContextObject, FName TutorialId, UTutorialStepData* StepData)
{
	uint32 PresentationSerial = 0;
	return ShowTutorialOnceWithPresentationSerial(WorldContextObject, TutorialId, StepData, PresentationSerial);
}

// server 호출은 multicast presentation으로, client 호출은 해당 로컬 viewport 표시로 분기
bool UTutorialBlueprintLibrary::ShowTutorialOnceWithPresentationSerial(
	const UObject* WorldContextObject,
	FName TutorialId,
	UTutorialStepData* StepData,
	uint32& OutPresentationSerial)
{
	OutPresentationSerial = 0;
	if (!WorldContextObject || TutorialId == NAME_None || !StepData)
	{
		UE_LOG(LogTemp, Warning, TEXT("[UTutorialBlueprintLibrary::ShowTutorialOnce] WorldContext, TutorialId, StepData가 모두 필요함"));
		return false;
	}

	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("[UTutorialBlueprintLibrary::ShowTutorialOnce] World를 찾지 못함: %s"), *TutorialId.ToString());
		return false;
	}

	if (World->GetNetMode() != NM_Client)
	{
		APragmataGameState* PragmataGameState = World->GetGameState<APragmataGameState>();
		if (!PragmataGameState || !PragmataGameState->HasAuthority())
		{
			UE_LOG(LogTemp, Warning, TEXT("[UTutorialBlueprintLibrary::ShowTutorialOnce] server PragmataGameState를 찾지 못함: %s"), *TutorialId.ToString());
			return false;
		}

		OutPresentationSerial = PragmataGameState->BeginTutorialPresentationOnce(TutorialId, StepData);
		return OutPresentationSerial != 0;
	}

	UGameInstance* GameInstance = World->GetGameInstance();
	UTutorialManagerSubsystem* TutorialManager = GameInstance ? GameInstance->GetSubsystem<UTutorialManagerSubsystem>() : nullptr;
	if (!TutorialManager)
	{
		UE_LOG(LogTemp, Warning, TEXT("[UTutorialBlueprintLibrary::ShowTutorialOnce] TutorialManagerSubsystem을 찾지 못함: %s"), *TutorialId.ToString());
		return false;
	}

	return TutorialManager->ShowTutorialOnce(TutorialId, StepData, 0);
}
