#include "Data/LobbyPreviewAnimationSet.h"

bool ULobbyPreviewAnimationSet::FindSequence(EPragmataPlayerRole PreviewRole, EPragmataPlayerRole FocusedRole, FLobbyPreviewAnimationSequence& OutSequence) const
{
	for (const FLobbyPreviewAnimationSequence& Sequence : Sequences)
	{
		if (Sequence.PreviewRole == PreviewRole && Sequence.FocusedRole == FocusedRole)
		{
			OutSequence = Sequence;
			return true;
		}
	}

	return false;
}
