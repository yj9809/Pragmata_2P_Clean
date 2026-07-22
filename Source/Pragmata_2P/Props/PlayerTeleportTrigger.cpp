#include "Props/PlayerTeleportTrigger.h"

#include "Components/BoxComponent.h"
#include "GameFramework/Pawn.h"

APlayerTeleportTrigger::APlayerTeleportTrigger()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	TeleportTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("TeleportTrigger"));
	SetRootComponent(TeleportTrigger);
	TeleportTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TeleportTrigger->SetCollisionObjectType(ECC_WorldDynamic);
	TeleportTrigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	TeleportTrigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	TeleportTrigger->SetGenerateOverlapEvents(true);
	TeleportTrigger->ShapeColor = FColor::Cyan;
}

void APlayerTeleportTrigger::BeginPlay()
{
	Super::BeginPlay();

	if (TeleportTrigger)
	{
		TeleportTrigger->OnComponentBeginOverlap.AddDynamic(this, &APlayerTeleportTrigger::HandleTriggerBeginOverlap);
	}
}

void APlayerTeleportTrigger::HandleTriggerBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult
)
{
	(void)OverlappedComponent;
	(void)OtherComp;
	(void)OtherBodyIndex;
	(void)bFromSweep;
	(void)SweepResult;

	// 플레이어 위치 변경은 서버 권위에서만 확정
	if (!HasAuthority() || !CanTeleportActor(OtherActor))
	{
		return;
	}

	TeleportActorToTarget(OtherActor);
}

bool APlayerTeleportTrigger::CanTeleportActor(AActor* OtherActor) const
{
	if (!OtherActor || OtherActor == this)
	{
		return false;
	}

	const APawn* Pawn = Cast<APawn>(OtherActor);
	return Pawn && Pawn->IsPlayerControlled();
}

void APlayerTeleportTrigger::TeleportActorToTarget(AActor* TargetActor) const
{
	if (!TargetActor)
	{
		return;
	}

	if (!TargetActor->TeleportTo(TargetLocation, TargetActor->GetActorRotation(), false, true))
	{
		UE_LOG(LogTemp, Warning, TEXT("[APlayerTeleportTrigger::TeleportActorToTarget] Failed to teleport actor: %s"), *GetNameSafe(TargetActor));
	}
}
