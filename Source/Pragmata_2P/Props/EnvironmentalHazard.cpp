#include "Props/EnvironmentalHazard.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Characters/PlayableCharacter.h"
#include "Combat/PragmataDamageLibrary.h"
#include "Engine/Engine.h"

AEnvironmentalHazard::AEnvironmentalHazard()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	RootScene = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));
	SetRootComponent(RootScene);

	HazardTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("HazardTrigger"));
	HazardTrigger->SetupAttachment(RootComponent);
	HazardTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	HazardTrigger->SetCollisionObjectType(ECC_WorldDynamic);
	HazardTrigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	HazardTrigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	HazardTrigger->SetGenerateOverlapEvents(true);
	HazardTrigger->ShapeColor = FColor::Orange;
}

void AEnvironmentalHazard::BeginPlay()
{
	Super::BeginPlay();

	if (HazardTrigger)
	{
		HazardTrigger->OnComponentBeginOverlap.AddDynamic(this, &AEnvironmentalHazard::HandleHazardBeginOverlap);
	}
}

bool AEnvironmentalHazard::CanAffectActor_Implementation(AActor* OtherActor) const
{
	return OtherActor && OtherActor != this && OtherActor->IsA<APlayableCharacter>();
}

// overlap 대상은 항상 OtherActor를 기준으로 하고 sweep 결과는 유효할 때만 impact 정보로 사용
void AEnvironmentalHazard::ApplyHazardToActor(AActor* TargetActor, const FHitResult* ImpactHit)
{
	if (!IsValid(TargetActor))
	{
		return;
	}
	
	FPragmataDamageSpec DamageSpec;
	DamageSpec.DamageAmount = 50.f;
	DamageSpec.SourceActor = this;
	DamageSpec.DamageCauser = this;
	DamageSpec.InstigatorController = GetInstigatorController();
	DamageSpec.DamageKind = EPragmataDamageKind::Generic;
	DamageSpec.HitReactStrength = EPragmataHitReactStrength::Small;
	DamageSpec.HitDirection = EPragmataHitDirection::Front;
	DamageSpec.bHasImpact = ImpactHit && ImpactHit->GetActor() == TargetActor;
	if (DamageSpec.bHasImpact)
	{
		DamageSpec.ImpactHit = *ImpactHit;
	}
	DamageSpec.ImpactDirection = (TargetActor->GetActorLocation() - GetActorLocation()).GetSafeNormal();

	UPragmataDamageLibrary::DealDamageToActor(TargetActor, DamageSpec);
}

void AEnvironmentalHazard::HandleHazardBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult
)
{
	const bool bHasAuthority = HasAuthority();
	const bool bCanAffect = CanAffectActor(OtherActor);
	// if (GEngine)
	// {
	// 	GEngine->AddOnScreenDebugMessage(
	// 		-1,
	// 		3.f,
	// 		bHasAuthority && bCanAffect ? FColor::Green : FColor::Yellow,
	// 		FString::Printf(
	// 			TEXT("[EnvironmentalHazard::Overlap] Hazard=%s Other=%s Comp=%s Authority=%d CanAffect=%d"),
	// 			*GetNameSafe(this),
	// 			*GetNameSafe(OtherActor),
	// 			*GetNameSafe(OtherComp),
	// 			bHasAuthority ? 1 : 0,
	// 			bCanAffect ? 1 : 0
	// 		)
	// 	);
	// }

	// 접촉형 위험물의 gameplay 영향은 서버 권한에서만 확정
	if (!bHasAuthority || !bCanAffect)
	{
		return;
	}

	const FHitResult* ImpactHit = bFromSweep && SweepResult.GetActor() == OtherActor ? &SweepResult : nullptr;
	ApplyHazardToActor(OtherActor, ImpactHit);
}
