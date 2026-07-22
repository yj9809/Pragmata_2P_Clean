#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/PragmataTypes.h"
#include "DianaVoiceComponent.generated.h"

class UDianaVoiceLineSet;

// 서버가 Diana 행동 음성을 선택하고, 양쪽 client에 같은 음성·자막을 전달하는 component
UCLASS(ClassGroup=(Pragmata), meta=(BlueprintSpawnableComponent))
class PRAGMATA_2P_API UDianaVoiceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDianaVoiceComponent();

	// authority에서만 호출. 행동별 cooldown과 현재 재생 중 guard를 통과한 경우에만 음성을 재생
	void PlayVoiceAction(EDianaVoiceAction Action);

private:
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayVoiceLine(FSoftObjectPath SoundPath);

	int32 SelectLineIndex(EDianaVoiceAction Action, const FDianaVoiceActionPool& Pool);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Pragmata|Diana Voice", meta=(AllowPrivateAccess="true"))
	TSoftObjectPtr<UDianaVoiceLineSet> VoiceLineSet;

	TMap<EDianaVoiceAction, int32> LastLineIndices;
	TMap<EDianaVoiceAction, float> NextAllowedVoiceTimes;
	float ActiveVoiceUntilServerTime = 0.f;
};
