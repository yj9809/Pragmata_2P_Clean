#include "Animation/AnimNotify_StopAttackRotation.h"

#include "Components/SkeletalMeshComponent.h"
#include "Enemy/MonsterBase.h"

void UAnimNotify_StopAttackRotation::Notify(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference
)
{
	Super::Notify(MeshComp, Animation, EventReference);

	AMonsterBase* Monster = MeshComp ? Cast<AMonsterBase>(MeshComp->GetOwner()) : nullptr;
	if (Monster)
	{
		Monster->StopAttackRotationTracking();
	}
}
