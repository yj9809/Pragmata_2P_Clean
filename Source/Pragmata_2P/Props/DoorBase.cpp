#include "Props/DoorBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Animation/AnimationAsset.h"
#include "TimerManager.h"
#include "Characters/PlayableCharacter.h" // 반드시 헤더 추가
#include "Net/UnrealNetwork.h"

namespace
{
	void ConfigureDoorProximityTriggerCollision(UBoxComponent* ProximityTrigger)
	{
		if (!ProximityTrigger)
		{
			return;
		}

		// 문 근접 trigger는 player overlap만 필요하므로 weapon trace와 AI trace를 막지 않음
		ProximityTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		ProximityTrigger->SetCollisionObjectType(ECC_WorldDynamic);
		ProximityTrigger->SetCollisionResponseToAllChannels(ECR_Ignore);
		ProximityTrigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
		ProximityTrigger->SetGenerateOverlapEvents(true);
	}
}

ADoorBase::ADoorBase()
{
	PrimaryActorTick.bCanEverTick = false;

	DoorMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("DoorMesh"));
	RootComponent = DoorMesh;

	ProximityTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("ProximityTrigger"));
	ProximityTrigger->SetupAttachment(RootComponent);
	ConfigureDoorProximityTriggerCollision(ProximityTrigger);

	bIsLocked = false;
	bIsOpen = false;
	bIsAnimating = false;
	bReplicates = true;
	bReplicatedShouldBeOpen = false;
}

void ADoorBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADoorBase, bReplicatedShouldBeOpen);
	DOREPLIFETIME(ADoorBase, bTerritoryCombatLocked);
}

void ADoorBase::SetTerritoryCombatLocked_Implementation(bool bLocked)
{
	// Territory 전투 봉쇄는 기존 puzzle·power 잠금과 분리해 server에서만 결정
	if (!HasAuthority() || bTerritoryCombatLocked == bLocked)
	{
		return;
	}

	bTerritoryCombatLocked = bLocked;
	OnTerritoryCombatLockStateChanged(bTerritoryCombatLocked);

	if (bTerritoryCombatLocked)
	{
		SetDoorOpenState(false);
	}
	else
	{
		UpdateDoorState();
	}

	ForceNetUpdate();
}

void ADoorBase::BeginPlay()
{
	Super::BeginPlay();

	ConfigureDoorProximityTriggerCollision(ProximityTrigger);
	ProximityTrigger->OnComponentBeginOverlap.AddDynamic(this, &ADoorBase::OnOverlapBegin);
	ProximityTrigger->OnComponentEndOverlap.AddDynamic(this, &ADoorBase::OnOverlapEnd);
}

void ADoorBase::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (OtherActor && OtherActor != this && Cast<APlayableCharacter>(OtherActor))
	{
		UpdateDoorState();
	}
}

void ADoorBase::OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (OtherActor && OtherActor != this && OtherActor->IsA<APawn>())
	{
		UpdateDoorState();
	}
}

void ADoorBase::UpdateDoorState()
{
	if (!HasAuthority() || bIsAnimating) return;

	bool bShouldBeOpen = false;

	if (!bIsLocked && !bTerritoryCombatLocked)
	{
		TArray<AActor*> OverlappingActors;
		ProximityTrigger->GetOverlappingActors(OverlappingActors);

		for (AActor* Actor : OverlappingActors)
		{
			if (Actor && Actor->IsA<APlayableCharacter>())
			{
				bShouldBeOpen = true;
				break;
			}
		}
	}

	SetDoorOpenState(bShouldBeOpen);
}

void ADoorBase::OnRep_ShouldBeOpen()
{
	ApplyDoorOpenState(bReplicatedShouldBeOpen);
}

void ADoorBase::OnRep_TerritoryCombatLocked()
{
	// client도 같은 BP 잠금 연출을 재생하도록 복제 상태에서 event 전달
	OnTerritoryCombatLockStateChanged(bTerritoryCombatLocked);
}

void ADoorBase::SetDoorOpenState(bool bNewShouldBeOpen)
{
	if (!HasAuthority())
	{
		return;
	}

	if (bReplicatedShouldBeOpen == bNewShouldBeOpen && bIsOpen == bNewShouldBeOpen)
	{
		return;
	}

	bReplicatedShouldBeOpen = bNewShouldBeOpen;
	ApplyDoorOpenState(bReplicatedShouldBeOpen);
	ForceNetUpdate();
}

void ADoorBase::ApplyDoorOpenState(bool bNewShouldBeOpen)
{
	if (bNewShouldBeOpen && !bIsOpen)
	{
		OpenDoor();
	}
	else if (!bNewShouldBeOpen && bIsOpen)
	{
		CloseDoor();
	}
}

void ADoorBase::OpenDoor()
{
	if (!OpenAnimation) return;

	bIsOpen = true;
	bIsAnimating = true;
	DoorMesh->SetPlayRate(1.0f);
	DoorMesh->PlayAnimation(OpenAnimation, false);

	GetWorldTimerManager().SetTimer(AnimationTimerHandle, this, &ADoorBase::OnAnimationFinished, OpenAnimation->GetPlayLength(), false);
}

void ADoorBase::CloseDoor()
{
	if (!OpenAnimation) return;

	bIsOpen = false;
	bIsAnimating = true;
	DoorMesh->PlayAnimation(OpenAnimation, false);
	DoorMesh->SetPosition(OpenAnimation->GetPlayLength());
	DoorMesh->SetPlayRate(-1.0f);

	GetWorldTimerManager().SetTimer(AnimationTimerHandle, this, &ADoorBase::OnAnimationFinished, OpenAnimation->GetPlayLength(), false);
}

void ADoorBase::OnAnimationFinished()
{
	bIsAnimating = false;
	UpdateDoorState();
}
