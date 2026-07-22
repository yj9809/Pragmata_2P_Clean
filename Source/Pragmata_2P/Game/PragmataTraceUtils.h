#pragma once

#include "CoreMinimal.h"
#include "Engine/HitResult.h"
#include "GameFramework/Actor.h"

namespace PragmataTraceUtils
{
	inline bool IsHitActorWithinCandidate(const FHitResult& Hit, const AActor* Candidate)
	{
		for (const AActor* HitActor = Hit.GetActor(); HitActor; HitActor = HitActor->GetAttachParentActor())
		{
			if (HitActor == Candidate)
			{
				return true;
			}
		}

		return false;
	}
}
