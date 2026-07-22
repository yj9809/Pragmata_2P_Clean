// Fill out your copyright notice in the Description page of Project Settings.


#include "Weapon/DecoyProjectile.h"

#include "Decoy.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Net/UnrealNetwork.h"

// Sets default values
ADecoyProjectile::ADecoyProjectile()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(true);
	
	SphereComp = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComp"));
	if (SphereComp)
	{
		RootComponent = SphereComp;
		SphereComp->InitSphereRadius(15.f);
		SphereComp->SetCollisionProfileName(TEXT("Projectile"));
		SphereComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		SphereComp->SetSimulatePhysics(false);
		SphereComp->OnComponentHit.AddDynamic(this, &ADecoyProjectile::OnHit);
	}
	
	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>("SkMesh");
	if (Mesh)
	{
		Mesh->SetupAttachment(SphereComp);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	
	ProjectileComp = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileComp"));
	if (ProjectileComp)
	{
		ProjectileComp->UpdatedComponent = SphereComp;
		ProjectileComp->bRotationFollowsVelocity = false;
		ProjectileComp->ProjectileGravityScale = 1.f;
		ProjectileComp->InitialSpeed = 0.f;
		ProjectileComp->MaxSpeed = 0.f;
		ProjectileComp->bShouldBounce = false;
	}
}

// Called when the game starts or when spawned
void ADecoyProjectile::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ADecoyProjectile::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void ADecoyProjectile::InitVelocity(const FVector& LaunchVelocity)
{
	if(ProjectileComp)
	{
		ProjectileComp->Velocity = LaunchVelocity;
		float Speed = LaunchVelocity.Size();
		ProjectileComp->InitialSpeed = Speed;
		ProjectileComp->MaxSpeed = Speed;
		ProjectileComp->SetComponentTickEnabled(true);
		ProjectileComp->Activate(true);
		ProjectileComp->UpdateComponentVelocity();
	}
}

void ADecoyProjectile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADecoyProjectile, bHasLanded);
}

void ADecoyProjectile::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
                             FVector NormalImpulse, const FHitResult& Hit)
{
	// if (GEngine)
	// {
	// 	const FString Message = FString::Printf(TEXT("decoyprojectile hit on land."));
	// 	GEngine->ClearOnScreenDebugMessages();
	// 	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, Message);
	// }
	
	if (!HasAuthority() || bHasLanded) return;

	bHasLanded = true;
	ApplyLandedState();
	ForceNetUpdate();
	
	//spawn decoy
	if (GetWorld())
	{
		FTransform SpawnTransform(FRotator::ZeroRotator, GetActorLocation());
		ADecoy* HughDecoy = GetWorld()->SpawnActorDeferred<ADecoy>(
		DecoyClass,
		SpawnTransform,
		this,
		GetInstigator(),
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn
		);
		
		if (HughDecoy)
		{
			//춤 모션 재생 혹은 초기화 작업 여기서 진행
			HughDecoy->FinishSpawning(SpawnTransform);
			HughDecoy->SetDecoyProjectile(this);
			//땅에 착지 시 디코이 발판 애니메이션 재생, 홀로그램 사라지면 발판도 같이 사라짐 처리
			if (Mesh && LoopAnimation)
			{
				Mesh->PlayAnimation(LoopAnimation, true);
			}
		}
	}
}

void ADecoyProjectile::OnRep_HasLanded()
{
	if (bHasLanded)
	{
		ApplyLandedState();
	}
}

void ADecoyProjectile::ApplyLandedState()
{
	if (ProjectileComp)
	{
		ProjectileComp->StopMovementImmediately();
		ProjectileComp->Deactivate();
	}
	if (SphereComp)
	{
		SphereComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}
