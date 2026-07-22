// Fill out your copyright notice in the Description page of Project Settings.


#include "Weapon/WeaponBase.h"

#include <filesystem>

#include "CS_WeaponRecoil.h"
#include "DrawDebugHelpers.h"
#include "NiagaraFunctionLibrary.h"
#include "TimerManager.h"
#include "Characters/HughCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/PragmataPlayerStatsComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Combat/PragmataDamageLibrary.h"
#include "Components/SphereComponent.h"
#include "Net/UnrealNetwork.h"

// Sets default values
AWeaponBase::AWeaponBase()
{
	// 서버 spawn 무기 복제
	bReplicates = true;
	SetReplicateMovement(true);

 	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh"));
	if (Mesh)
	{
		RootComponent = Mesh;
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetGenerateOverlapEvents(false);
	}

	CameraShakeClass = UCS_WeaponRecoil::StaticClass();

	//interaction collider
	InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComponent"));
	InteractionSphere->SetSphereRadius(150.f);
	InteractionSphere->SetupAttachment(RootComponent);
	InteractionSphere->SetCollisionProfileName(TEXT("Interaction"));
}

// Called when the game starts or when spawned
void AWeaponBase::BeginPlay()
{
	Super::BeginPlay();

	if (InteractionSphere)
	{
		InteractionSphere->OnComponentBeginOverlap.AddDynamic(this, &AWeaponBase::OnSphereOverlap);
		InteractionSphere->OnComponentEndOverlap.AddDynamic(this, &AWeaponBase::OnSphereEndOverlap);
	}
}

// Called every frame
void AWeaponBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);


}

// ammo는 서버 확정 snapshot만 복제해 HUD 숫자와 실제 발사 판정을 맞춤
void AWeaponBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AWeaponBase, CurAmmunition);
	DOREPLIFETIME(AWeaponBase, MaxAmmunition);
	DOREPLIFETIME(AWeaponBase, IsCoolingDown);
}

// 서버 발사 확정은 공통 ammo/cooldown gate를 통과한 호출만 진행
bool AWeaponBase::PullTrigger()
{
	// 탄약과 cooldown은 소유 캐릭터의 Server RPC를 통과한 authority만 변경
	if (!HasAuthority())
	{
		return false;
	}

	if (IsCoolingDown || CurAmmunition <= 0)
	{
		return false;
	}

	ApplyLocalFireFeedback();

	StartFireCooldown();
	MulticastPlayFireSound();

	return true;
}

void AWeaponBase::ApplyLocalFireFeedback()
{
	TargetRecoilPitch = TargetRecoilPitchAmount;
}

void AWeaponBase::MulticastPlayFireSound_Implementation()
{
	if (GetNetMode() == NM_DedicatedServer || !GunshotSFX)
	{
		return;
	}

	UGameplayStatics::PlaySoundAtLocation(
		this,
		GunshotSFX,
		GetActorLocation(),
		GetActorRotation(),
		1.f,
		1.f,
		0.f,
		GunshotAttenuation);
}

bool AWeaponBase::DealDamageToHitActor(const FHitResult& Hit, AController* InstigatorController)
{
	AActor* HitActor = Hit.GetActor();
	if (!HitActor || !InstigatorController)
	{
		return false;
	}

	// weapon hit 정보를 프로젝트 공통 damage spec으로 전달
	// Hugh shared stat 배율을 weapon damage에만 좁게 적용
	const UPragmataPlayerStatsComponent* PlayerStatsComponent = GetOwner()
		? GetOwner()->FindComponentByClass<UPragmataPlayerStatsComponent>()
		: nullptr;
	const float DamageMultiplier = PlayerStatsComponent ? PlayerStatsComponent->GetPlayerDamageMultiplier() : 1.f;

	FPragmataDamageSpec DamageSpec;
	DamageSpec.DamageAmount = BaseDamage * FMath::Max(0.f, DamageMultiplier);
	DamageSpec.SourceActor = GetOwner();
	DamageSpec.DamageCauser = this;
	DamageSpec.InstigatorController = InstigatorController;
	DamageSpec.DamageKind = EPragmataDamageKind::Weapon;
	DamageSpec.HitReactStrength = EPragmataHitReactStrength::Medium;
	DamageSpec.bHasImpact = true;
	DamageSpec.ImpactHit = Hit;
	DamageSpec.ImpactDirection = (Hit.ImpactPoint - Hit.TraceStart).GetSafeNormal();

	return UPragmataDamageLibrary::DealDamageToActor(HitActor, DamageSpec);
}

void AWeaponBase::RefillAmmoToMax()
{
	if (!HasAuthority())
	{
		return;
	}

	CurAmmunition = MaxAmmunition;
	OnAmmoChange.Broadcast(CurAmmunition, MaxAmmunition);
}

bool AWeaponBase::AddAmmo(int32 AmmoAmount)
{
	if (!HasAuthority() || AmmoAmount <= 0 || CurAmmunition >= MaxAmmunition)
	{
		return false;
	}

	CurAmmunition = FMath::Min(CurAmmunition + AmmoAmount, MaxAmmunition);
	BroadcastAmmoChanged();
	ForceNetUpdate();
	return true;
}

void AWeaponBase::StartFireCooldown()
{
	// fire transaction 밖의 client 호출로 server cooldown을 선점하지 못하게 차단
	if (!HasAuthority())
	{
		return;
	}

	if (IsCoolingDown)
	{
		return;
	}

	IsCoolingDown = true;

	GetWorldTimerManager().SetTimer(
		CooldownTimerHandle,
		this,
		&AWeaponBase::ResetShootingCooldown,
		CoolDown,
		false);
}

void AWeaponBase::ResetShootingCooldown()
{
	IsCoolingDown = false;
}

float AWeaponBase::GetRecoilData(float DeltaSeconds)
{
	// 1. Determine if we are moving UP (interp to target) or DOWN (recovering to 0)
	float CurrentSpeed = (TargetRecoilPitch < 0.f) ? CameraUpSpeed : CameraDownSpeed;

	// 2. Smoothly interpolate position WITHOUT spring bouncing
	CurrentRecoilPitch = FMath::FInterpTo(CurrentRecoilPitch, TargetRecoilPitch, DeltaSeconds, CurrentSpeed);

	// 3. If we safely reached near the peak, change target to 0 so it starts smoothly sliding down
	if (FMath::IsNearlyEqual(CurrentRecoilPitch, TargetRecoilPitch, 0.1f))
	{
		TargetRecoilPitch = 0.f;
	}

	float DeltaPitch = CurrentRecoilPitch - LastRecoilPitch;
	LastRecoilPitch = CurrentRecoilPitch;

	return DeltaPitch;
}

// client가 서버 ammo 복제값을 받는 순간 UI delegate로 전달
void AWeaponBase::OnRep_AmmoSnapshot()
{
	BroadcastAmmoChanged();
}

void AWeaponBase::BroadcastAmmoChanged()
{
	OnAmmoChange.Broadcast(CurAmmunition, MaxAmmunition);
}

void AWeaponBase::StoreWeapon(AHughCharacter* HughCharacter, const FName& SocketName)
{
	if (!HughCharacter)	return;

	SetOwner(HughCharacter);

	InteractionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	if (Mesh)
	{
		Mesh->SetSimulatePhysics(false);
		Mesh->SetCollisionProfileName(TEXT("NoCollision"));
	}

	FAttachmentTransformRules AttachmentRules(EAttachmentRule::SnapToTarget, true);
	AttachToComponent(HughCharacter->GetMesh(), AttachmentRules, SocketName);
}

void AWeaponBase::DropWeapon()
{
	if (!HasAuthority()) return;
	if (WeaponType == EWeaponType::Primary) return;

	SetOwner(nullptr);
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

	if (Mesh)
	{
		Mesh->SetSimulatePhysics(true);
		Mesh->SetEnableGravity(true);
		Mesh->SetCollisionProfileName(TEXT("PhysicsActor"));
		Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		InteractionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void AWeaponBase::OnSphereOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
                                  UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (AHughCharacter* Character = Cast<AHughCharacter>(OtherActor))
	{
		Character->SetOverlappingWeapon(this);
	}
}

void AWeaponBase::OnSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (AHughCharacter* Character = Cast<AHughCharacter>(OtherActor))
	{
		Character->SetOverlappingWeapon(nullptr);
	}
}
