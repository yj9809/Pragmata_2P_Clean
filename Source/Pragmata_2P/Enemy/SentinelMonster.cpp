#include "Enemy/SentinelMonster.h"

#include "Components/PragmataHealthComponent.h"
#include "Game/PragmataGameState.h"
#include "GameFramework/GameState.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "Puzzle/EnemyHackingPuzzleComponent.h"


ASentinelMonster::ASentinelMonster()
{
	MonsterId = TEXT("Sentinel");
	ScanDisplayText = NSLOCTEXT("Pragmata", "SentinelScanDisplayText", "Sentinel");

	if (EnemyHackingPuzzleComponent)
	{
		EnemyHackingPuzzleComponent->EnemyType = EEnemyHackType::Sentinel;
	}
}

void ASentinelMonster::ClearTargetIfSame(AActor* Actor)
{
	// Territory가 target을 지울 때 현재 Sentinel HUD도 같은 actor 기준으로 해제
	const bool bClearingCurrentTarget = GetTargetActor() == Actor;
	Super::ClearTargetIfSame(Actor);

	if (bClearingCurrentTarget)
	{
		ClearBossHealthView();
	}
}

bool ASentinelMonster::TrySetTargetIfPerceived(AActor* Actor)
{
	if (!Super::TrySetTargetIfPerceived(Actor))
	{
		return false;
	}

	// perception으로 전투가 시작된 Sentinel만 HUD의 active boss로 등록
	ActiveBossHealthView();
	return true;
}

bool ASentinelMonster::SetTerritoryTarget(AActor* Actor)
{
	if (!Super::SetTerritoryTarget(Actor))
	{
		return false;
	}

	// Territory가 확정한 player target도 boss HUD 표시 기준으로 사용
	ActiveBossHealthView();
	return true;
}

// 사망 전 phase sequence와 GameState boss target을 정리해 연출과 HUD 잔류를 막음
void ASentinelMonster::SetDead()
{
	if (bPhaseTransitionSequencePlaying)
	{
		bPhaseTransitionSequencePlaying = false;
		MulticastStopPhaseTransitionSequence();
	}

	ClearBossHealthView();
	Super::SetDead();
}

// Sentinel 체력 기준으로 2phase 공격 선택 여부를 결정
bool ASentinelMonster::IsPhaseTwoActive() const
{
	return GetCurrentHealthRatio() <= PhaseTwoHealthRatio;
}

// Sentinel 전용 phase 조건으로 공통 공격 패턴 가중치를 최종 필터링
float ASentinelMonster::GetEffectiveSelectionWeight(int32 PatternIndex) const
{
	const float BaseWeight = Super::GetEffectiveSelectionWeight(PatternIndex);
	if (BaseWeight <= 0.f || !AttackPatterns.IsValidIndex(PatternIndex))
	{
		return 0.f;
	}

	const FMonsterAttackPattern& Pattern = AttackPatterns[PatternIndex];
	return IsAttackPatternAllowedForCurrentPhase(Pattern) ? BaseWeight : 0.f;
}

float ASentinelMonster::GetCurrentHealthRatio() const
{
	if (!HealthComponent || HealthComponent->GetMaxHealth() <= 0.f)
	{
		return 1.f;
	}

	return HealthComponent->GetCurrentHealth() / HealthComponent->GetMaxHealth();
}

bool ASentinelMonster::IsAttackPatternAllowedForCurrentPhase(const FMonsterAttackPattern& Pattern) const
{
	const TArray<FName>& CurrentPhasePatternNames = IsPhaseTwoActive()
		? PhaseTwoAttackPatternNames
		: PhaseOneAttackPatternNames;

	if (CurrentPhasePatternNames.IsEmpty())
	{
		return true;
	}

	return Pattern.PatternName != NAME_None && CurrentPhasePatternNames.Contains(Pattern.PatternName);
}

// 반피 HitReact가 정상 종료된 뒤에만 boss phase sequence를 이어 재생하고 공격 잠금을 유지
void ASentinelMonster::HandleHalfHealthHitReactMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (!HasAuthority() || Montage != HalfHealthHitReactMontage)
	{
		return;
	}

	if (!bInterrupted && !IsDead() && StartPhaseTransitionSequence())
	{
		return;
	}

	Super::HandleHalfHealthHitReactMontageEnded(Montage, bInterrupted);
}

// server가 sequence player 생성 성공을 확인해야 HitReact 공격 잠금을 계속 유지
bool ASentinelMonster::StartPhaseTransitionSequence()
{
	if (!bPlayPhaseTransitionSequence || !PhaseTransitionSequence || bPhaseTransitionSequencePlaying)
	{
		return false;
	}

	bPhaseTransitionSequencePlaying = true;
	MulticastPlayPhaseTransitionSequence(PhaseTransitionSequence);
	if (PhaseTransitionSequencePlayer)
	{
		return true;
	}

	bPhaseTransitionSequencePlaying = false;
	return false;
}

// server가 확정한 phase sequence를 모든 peer의 현재 World에서 재생
void ASentinelMonster::MulticastPlayPhaseTransitionSequence_Implementation(ULevelSequence* Sequence)
{
	PlayPhaseTransitionSequenceLocally(Sequence);
}

// 사망 등으로 sequence를 중단할 때 peer별 runtime player 참조도 함께 해제
void ASentinelMonster::MulticastStopPhaseTransitionSequence_Implementation()
{
	if (PhaseTransitionSequencePlayer)
	{
		PhaseTransitionSequencePlayer->Stop();
		PhaseTransitionSequencePlayer = nullptr;
	}
}

// sequence binding tag를 현재 Sentinel로 교체해 BP asset을 어느 boss instance에서도 재사용
void ASentinelMonster::PlayPhaseTransitionSequenceLocally(ULevelSequence* Sequence)
{
	if (!Sequence || !GetWorld())
	{
		return;
	}

	ALevelSequenceActor* SequenceActor = nullptr;
	FMovieSceneSequencePlaybackSettings PlaybackSettings;
	PhaseTransitionSequencePlayer = ULevelSequencePlayer::CreateLevelSequencePlayer(
		this,
		Sequence,
		PlaybackSettings,
		SequenceActor);
	if (!PhaseTransitionSequencePlayer)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ASentinelMonster::PlayPhaseTransitionSequenceLocally] Failed to create sequence player"));
		return;
	}

	if (SequenceActor && !PhaseTransitionSequenceBossBindingTag.IsNone())
	{
		TArray<AActor*> BossActors;
		BossActors.Add(this);
		SequenceActor->SetBindingByTag(PhaseTransitionSequenceBossBindingTag, BossActors);
	}

	if (HasAuthority())
	{
		PhaseTransitionSequencePlayer->OnFinished.AddDynamic(this, &ThisClass::HandlePhaseTransitionSequenceFinished);
	}
	PhaseTransitionSequencePlayer->Play();
}

// sequence 완료 뒤 공통 HitReact 종료 처리를 호출해 공격 가능 상태를 다시 계산
void ASentinelMonster::HandlePhaseTransitionSequenceFinished()
{
	if (!HasAuthority() || !bPhaseTransitionSequencePlaying)
	{
		return;
	}

	bPhaseTransitionSequencePlaying = false;
	PhaseTransitionSequencePlayer = nullptr;
	Super::HandleHalfHealthHitReactMontageEnded(HalfHealthHitReactMontage, false);
}


void ASentinelMonster::ActiveBossHealthView()
{
	if (!HasAuthority() || !GetTargetActor() || IsDead())
	{
		return;
	}

	if (APragmataGameState* GS = GetWorld()->GetGameState<APragmataGameState>())
	{
		GS->SetActiveBossTarget(this);
	}
}

// 다른 boss가 표시 중인 경우를 보호하고 자신의 target 해제 때만 HUD를 닫음
void ASentinelMonster::ClearBossHealthView()
{
	if (!HasAuthority())
	{
		return;
	}

	if (APragmataGameState* GS = GetWorld()->GetGameState<APragmataGameState>())
	{
		if (GS->GetActiveBossTarget() == this)
		{
			GS->SetActiveBossTarget(nullptr);
		}
	}
}
