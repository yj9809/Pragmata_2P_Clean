#include "Components/DianaVoiceComponent.h"

#include "Characters/HughCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Data/DianaVoiceLineSet.h"
#include "Engine/World.h"
#include "Game/PragmataGameState.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

UDianaVoiceComponent::UDianaVoiceComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	VoiceLineSet = TSoftObjectPtr<UDianaVoiceLineSet>(FSoftObjectPath(TEXT("/Game/Sound/DianaVoice/DA_DianaVoiceLineSet.DA_DianaVoiceLineSet")));
}

// 음성 선택과 cooldown 소비는 server 한 곳에서만 처리해 두 player가 다른 line을 듣지 않게 함
void UDianaVoiceComponent::PlayVoiceAction(EDianaVoiceAction Action)
{
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !Owner->HasAuthority() || !World)
	{
		return;
	}

	const UDianaVoiceLineSet* LineSet = VoiceLineSet.LoadSynchronous();
	const FDianaVoiceActionPool* Pool = LineSet ? LineSet->FindPool(Action) : nullptr;
	if (!Pool || Pool->Lines.IsEmpty())
	{
		return;
	}

	const float CurrentTime = World->GetTimeSeconds();
	if (CurrentTime < ActiveVoiceUntilServerTime || CurrentTime < NextAllowedVoiceTimes.FindRef(Action))
	{
		return;
	}

	const int32 LineIndex = SelectLineIndex(Action, *Pool);
	if (!Pool->Lines.IsValidIndex(LineIndex))
	{
		return;
	}

	const FDianaVoiceLine& VoiceLine = Pool->Lines[LineIndex];
	USoundBase* Sound = VoiceLine.Sound.LoadSynchronous();
	if (!Sound)
	{
		return;
	}

	const float DisplaySeconds = VoiceLine.DisplaySeconds > 0.f ? VoiceLine.DisplaySeconds : Sound->GetDuration();
	ActiveVoiceUntilServerTime = CurrentTime + FMath::Max(DisplaySeconds, 0.1f);
	NextAllowedVoiceTimes.Add(Action, CurrentTime + FMath::Max(0.f, Pool->CooldownSeconds));

	Multicast_PlayVoiceLine(VoiceLine.Sound.ToSoftObjectPath());
	if (APragmataGameState* GameState = World->GetGameState<APragmataGameState>())
	{
		FDialogueLine DialogueLine;
		DialogueLine.Speaker = NSLOCTEXT("Pragmata", "DianaVoiceSpeaker", "Diana");
		DialogueLine.Line = VoiceLine.Subtitle;
		GameState->Multicast_PlayDialogueLine(DialogueLine, DisplaySeconds);
	}
}

// 2개 이상인 pool은 직전 line을 제외해 같은 행동의 즉시 반복을 막음
int32 UDianaVoiceComponent::SelectLineIndex(EDianaVoiceAction Action, const FDianaVoiceActionPool& Pool)
{
	const int32 LineCount = Pool.Lines.Num();
	if (LineCount <= 1)
	{
		LastLineIndices.Add(Action, 0);
		return 0;
	}

	const int32* PreviousIndex = LastLineIndices.Find(Action);
	if (!PreviousIndex)
	{
		const int32 FirstSelectedIndex = FMath::RandRange(0, LineCount - 1);
		LastLineIndices.Add(Action, FirstSelectedIndex);
		return FirstSelectedIndex;
	}

	int32 SelectedIndex = FMath::RandRange(0, LineCount - 2);
	if (SelectedIndex >= *PreviousIndex)
	{
		++SelectedIndex;
	}

	LastLineIndices.Add(Action, SelectedIndex);
	return SelectedIndex;
}

// 실제 SoundWave 재생은 각 client의 Diana avatar mesh에 붙여 3D 위치와 공용 UI 대사를 일치시킴
void UDianaVoiceComponent::Multicast_PlayVoiceLine_Implementation(FSoftObjectPath SoundPath)
{
	UWorld* World = GetWorld();
	APragmataGameState* GameState = World ? World->GetGameState<APragmataGameState>() : nullptr;
	AHughCharacter* HughCharacter = GameState ? GameState->GetHughCharacter() : nullptr;
	USkeletalMeshComponent* DianaMesh = HughCharacter ? HughCharacter->GetDianaAvatarMesh() : nullptr;
	USoundBase* Sound = Cast<USoundBase>(SoundPath.TryLoad());
	if (DianaMesh && Sound)
	{
		UGameplayStatics::SpawnSoundAttached(Sound, DianaMesh);
	}
}
