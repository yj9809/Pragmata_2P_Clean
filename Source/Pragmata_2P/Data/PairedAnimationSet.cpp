#include "Data/PairedAnimationSet.h"

namespace
{
bool MatchesCueStrengthAndDirection(const FPairedAnimationEntry& Entry, const FPairedAnimationCue& Cue)
{
	return Entry.CueType == Cue.CueType
		&& Entry.Strength == Cue.Strength
		&& Entry.Direction == Cue.Direction;
}

bool MatchesCueAndStrength(const FPairedAnimationEntry& Entry, const FPairedAnimationCue& Cue)
{
	return Entry.CueType == Cue.CueType
		&& Entry.Strength == Cue.Strength;
}

bool MatchesCueOnly(const FPairedAnimationEntry& Entry, const FPairedAnimationCue& Cue)
{
	return Entry.CueType == Cue.CueType;
}
}

const FPairedAnimationEntry* UPairedAnimationSet::FindEntry(const FPairedAnimationCue& Cue) const
{
	for (const FPairedAnimationEntry& Entry : Entries)
	{
		// 가장 구체적인 hit direction montage가 있으면 우선 사용
		if (MatchesCueStrengthAndDirection(Entry, Cue))
		{
			return &Entry;
		}
	}

	for (const FPairedAnimationEntry& Entry : Entries)
	{
		// 방향별 montage가 없으면 같은 강도의 공통 montage로 fallback
		if (MatchesCueAndStrength(Entry, Cue))
		{
			return &Entry;
		}
	}

	for (const FPairedAnimationEntry& Entry : Entries)
	{
		// 강도별 entry도 없을 때만 cue type 기본 montage를 사용
		if (MatchesCueOnly(Entry, Cue))
		{
			return &Entry;
		}
	}

	// 기존 Diana death entry의 cue/strength 값이 달라도 방향 montage는 복구
	if (Cue.CueType == EPairedAnimationCueType::Death || Cue.Strength == EPragmataHitReactStrength::Death)
	{
		for (const FPairedAnimationEntry& Entry : Entries)
		{
			if (Entry.Direction == Cue.Direction && Entry.DianaMontage)
			{
				return &Entry;
			}
		}
	}

	return nullptr;
}
