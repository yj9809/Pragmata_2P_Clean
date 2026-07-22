#include "Animation/AnimNotifyState_CleanseChipAttachment.h"

#include "Components/CleanseChipPresentationComponent.h"
#include "Components/SkeletalMeshComponent.h"

// montage frame 구간이 시작될 때만 준비된 chip presentation을 보이게 함
void UAnimNotifyState_CleanseChipAttachment::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	if (AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr)
	{
		if (UCleanseChipPresentationComponent* Presentation = Owner->FindComponentByClass<UCleanseChipPresentationComponent>())
		{
			Presentation->BeginChipHold();
		}
	}
}

// montage 중단도 NotifyState 종료로 chip을 숨기게 함
void UAnimNotifyState_CleanseChipAttachment::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);
	if (AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr)
	{
		if (UCleanseChipPresentationComponent* Presentation = Owner->FindComponentByClass<UCleanseChipPresentationComponent>())
		{
			Presentation->EndChipHold();
		}
	}
}
