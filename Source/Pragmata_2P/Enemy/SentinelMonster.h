#pragma once

#include "CoreMinimal.h"
#include "Enemy/MonsterBase.h"
#include "SentinelMonster.generated.h"

class ULevelSequence;
class ULevelSequencePlayer;

UCLASS()
class PRAGMATA_2P_API ASentinelMonster : public AMonsterBase
{
	GENERATED_BODY()

public:
	ASentinelMonster();

	virtual void ClearTargetIfSame(AActor* Actor) override;
	virtual bool TrySetTargetIfPerceived(AActor* Actor) override;
	virtual bool SetTerritoryTarget(AActor* Actor) override;

	UFUNCTION(BlueprintPure, Category="Pragmata|Boss|Phase")
	bool IsPhaseTwoActive() const;
	
	UFUNCTION(BlueprintCallable, Category="Pragmata|Boss|Phase")
	void ActiveBossHealthView();

protected:
	virtual void SetDead() override;
	virtual float GetEffectiveSelectionWeight(int32 PatternIndex) const override;
	virtual void HandleHalfHealthHitReactMontageEnded(UAnimMontage* Montage, bool bInterrupted) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pragmata|Boss|Phase", meta=(ClampMin="0.0", ClampMax="1.0"))
	float PhaseTwoHealthRatio = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pragmata|Boss|Phase")
	TArray<FName> PhaseOneAttackPatternNames;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pragmata|Boss|Phase")
	TArray<FName> PhaseTwoAttackPatternNames;

	// 반피 HitReact 종료 뒤 boss 전용 sequence 재생 여부와 asset을 BP 기본값에서 선택
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Pragmata|Boss|Phase|Sequence")
	bool bPlayPhaseTransitionSequence = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Pragmata|Boss|Phase|Sequence", meta=(EditCondition="bPlayPhaseTransitionSequence", EditConditionHides))
	TObjectPtr<ULevelSequence> PhaseTransitionSequence = nullptr;

	// sequence에서 현재 Sentinel actor로 바꿔 연결할 binding tag
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Pragmata|Boss|Phase|Sequence", meta=(EditCondition="bPlayPhaseTransitionSequence", EditConditionHides))
	FName PhaseTransitionSequenceBossBindingTag = TEXT("Boss");

private:
	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlayPhaseTransitionSequence(ULevelSequence* Sequence);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastStopPhaseTransitionSequence();

	UFUNCTION()
	void HandlePhaseTransitionSequenceFinished();

	void ClearBossHealthView();
	float GetCurrentHealthRatio() const;
	bool IsAttackPatternAllowedForCurrentPhase(const FMonsterAttackPattern& Pattern) const;
	bool StartPhaseTransitionSequence();
	void PlayPhaseTransitionSequenceLocally(ULevelSequence* Sequence);

	bool bPhaseTransitionSequencePlaying = false;

	UPROPERTY(Transient)
	TObjectPtr<ULevelSequencePlayer> PhaseTransitionSequencePlayer = nullptr;
};
