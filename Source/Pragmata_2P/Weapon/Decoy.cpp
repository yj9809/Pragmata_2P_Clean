// Fill out your copyright notice in the Description page of Project Settings.


#include "Weapon/Decoy.h"

#include "TimerManager.h"
#include "Components/PragmataFactionComponent.h"
#include "Enemy/MonsterBase.h"
#include "Engine/World.h"

// Sets default values
ADecoy::ADecoy()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	
	SphereComp = CreateDefaultSubobject<USphereComponent>("SphereComp");
	if (SphereComp)
	{
		SphereComp->InitSphereRadius(500.f);
		SphereComp->SetCollisionProfileName(TEXT("Projectile"));
		SphereComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		SphereComp->SetSimulatePhysics(false);
		SphereComp->SetNotifyRigidBodyCollision(true);
		RootComponent = SphereComp;
	}
	
	FactionComponent = CreateDefaultSubobject<UPragmataFactionComponent>(TEXT("FactionComponent"));
	if (FactionComponent)
	{
		FactionComponent->SetFaction(EPragmataFaction::Player);
	}
}

void ADecoy::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority() && SphereComp)
	{
		SphereComp->OnComponentBeginOverlap.AddDynamic(this, &ADecoy::OnOverlapBegin);
	}
	
}

void ADecoy::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void ADecoy::OnTimerEnd(AMonsterBase* Monster)
{
	if (Monster)
	{
		Monster->SetIsTaunted(false);
	}
	GetWorldTimerManager().ClearTimer(ActiveTimerHandle);
	if (DecoyProjectile)
	{
		DecoyProjectile->Destroy();
	}
	Destroy();
}

void ADecoy::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
                            int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority()) return;
	
	//원형 범위 내에 적이 들어오면 해당 적에게 타겟 변경 시도
	AMonsterBase* Monster = Cast<AMonsterBase>(OtherActor);
	if (Monster && !Monster->GetIsTaunted())
	{
		//해당 몬스터의 타겟이 이미 decoy일 경우
		if (Monster->GetTargetActor() == this) return;
	
		if (Monster->TrySetTargetIfPerceived(this))
		{
			Monster->SetIsTaunted(true);
			
			UE_LOG(LogTemp, Warning, TEXT("OVERLAP SUCCESSFUL: %s Targetting Decoy"), *Monster->GetName());
		}
	}
	FTimerDelegate TimerEndDelegate;
	TimerEndDelegate.BindUObject(this, &ADecoy::OnTimerEnd, Monster);
	GetWorldTimerManager().SetTimer(ActiveTimerHandle, TimerEndDelegate, 10.f, false);
}
