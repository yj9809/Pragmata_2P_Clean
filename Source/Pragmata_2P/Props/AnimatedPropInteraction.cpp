#include "Props/AnimatedPropInteraction.h"

#include "Animation/AnimationAsset.h"
#include "Components/SkeletalMeshComponent.h"
#include "Net/UnrealNetwork.h"

AAnimatedPropInteraction::AAnimatedPropInteraction()
{
	AnimatedMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("AnimatedMesh"));
	RootComponent = AnimatedMesh;

	bReplicates = true;
}

void AAnimatedPropInteraction::BeginPlay()
{
	Super::BeginPlay();

}

void AAnimatedPropInteraction::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AAnimatedPropInteraction, InteractionAnimationSerial);
}

void AAnimatedPropInteraction::Interact_Implementation(AActor* Interactor)
{
	// Interaction results and animation serial are server-authoritative
	if (!HasAuthority())
	{
		return;
	}

	PlayPresentationInteractionAnimation();

	Super::Interact_Implementation(Interactor);
}

// reward 없이 prop의 닫힘/열림 presentation만 복제
void AAnimatedPropInteraction::PlayPresentationInteractionAnimation()
{
	if (!HasAuthority())
	{
		return;
	}

	PlayInteractionAnimation();
	++InteractionAnimationSerial;
	ForceNetUpdate();
}

void AAnimatedPropInteraction::OnRep_InteractionAnimationSerial()
{
	// Clients replay the interaction animation when the server serial changes
	if (InteractionAnimationSerial > 0)
	{
		PlayInteractionAnimation();
	}
}

void AAnimatedPropInteraction::PlayInteractionAnimation()
{
	if (!AnimatedMesh || !InteractionAnimation)
	{
		return;
	}

	AnimatedMesh->PlayAnimation(InteractionAnimation, false);
	// Apply play rate after PlayAnimation switches to single-node playback
	AnimatedMesh->SetPlayRate(InteractionAnimationPlayRate);
}
