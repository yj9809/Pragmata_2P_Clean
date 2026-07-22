#include "Weapon/StasisNetField.h"

#include "Combat/PragmataDamageLibrary.h"
#include "Components/SphereComponent.h"
#include "DrawDebugHelpers.h"
#include "Enemy/MonsterBase.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

AStasisNetField::AStasisNetField()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(false);

	FieldCollision = CreateDefaultSubobject<USphereComponent>(TEXT("FieldCollision"));
	RootComponent = FieldCollision;
	if (FieldCollision)
	{
		FieldCollision->InitSphereRadius(FieldRadius);
		FieldCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		FieldCollision->SetCollisionObjectType(ECC_WorldDynamic);
		FieldCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
		FieldCollision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
		FieldCollision->SetGenerateOverlapEvents(true);
	}
}

// 서버 field 시작 시 현재 overlap과 dot timer를 gameplay 기준으로 확정
void AStasisNetField::BeginPlay()
{
	Super::BeginPlay();

	if (FieldCollision)
	{
		FieldCollision->SetSphereRadius(FieldRadius, true);
		FieldCollision->OnComponentBeginOverlap.AddUniqueDynamic(this, &ThisClass::HandleFieldBeginOverlap);
		FieldCollision->OnComponentEndOverlap.AddUniqueDynamic(this, &ThisClass::HandleFieldEndOverlap);
	}

	if (!HasAuthority())
	{
		return;
	}

	SetLifeSpan(FieldDuration);

	TArray<AActor*> OverlappingActors;
	if (FieldCollision)
	{
		FieldCollision->GetOverlappingActors(OverlappingActors, AMonsterBase::StaticClass());
	}

	for (AActor* Actor : OverlappingActors)
	{
		AddMonster(Cast<AMonsterBase>(Actor));
	}

	const float ClampedInterval = FMath::Max(KINDA_SMALL_NUMBER, DamageInterval);
	GetWorldTimerManager().SetTimer(
		DotDamageTimerHandle,
		this,
		&AStasisNetField::ApplyDotDamage,
		ClampedInterval,
		true,
		ClampedInterval);
}

// field actor 자체가 VFX 대신 모든 peer에서 영역을 debug로 표시
void AStasisNetField::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	DrawFieldDebug();
}

// field 종료가 남은 stun timer보다 우선하도록 overlap monster를 모두 해제
void AStasisNetField::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority())
	{
		GetWorldTimerManager().ClearTimer(DotDamageTimerHandle);
		RemoveAllMonsters();
	}

	Super::EndPlay(EndPlayReason);
}

// client debug 표시가 server field radius와 맞도록 반경만 복제
void AStasisNetField::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AStasisNetField, FieldRadius);
}

// deferred spawn 중 weapon에서 확정한 field/damage 값을 BeginPlay 전에 주입
void AStasisNetField::InitializeField(
	AActor* InSourceActor,
	AController* InInstigatorController,
	float InBaseDamage,
	float InHackedDamage,
	float InFieldRadius,
	float InFieldDuration,
	float InStunDuration,
	float InDamageInterval)
{
	SourceActor = InSourceActor;
	InstigatorController = InInstigatorController;
	BaseDotDamage = FMath::Max(0.f, InBaseDamage);
	HackedDotDamage = FMath::Max(0.f, InHackedDamage);
	FieldRadius = FMath::Max(0.f, InFieldRadius);
	FieldDuration = FMath::Max(0.f, InFieldDuration);
	StunDuration = FMath::Max(0.f, InStunDuration);
	DamageInterval = FMath::Max(KINDA_SMALL_NUMBER, InDamageInterval);
	if (FieldCollision)
	{
		FieldCollision->SetSphereRadius(FieldRadius, true);
	}
}

// 서버 overlap 진입만 dot damage와 stun 대상 등록으로 인정
void AStasisNetField::HandleFieldBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	(void)OverlappedComponent;
	(void)OtherComp;
	(void)OtherBodyIndex;
	(void)bFromSweep;
	(void)SweepResult;

	if (!HasAuthority())
	{
		return;
	}

	AddMonster(Cast<AMonsterBase>(OtherActor));
}

// field 이탈 시 이 field source가 부여한 stun만 해제
void AStasisNetField::HandleFieldEndOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex)
{
	(void)OverlappedComponent;
	(void)OtherComp;
	(void)OtherBodyIndex;

	if (!HasAuthority())
	{
		return;
	}

	RemoveMonster(Cast<AMonsterBase>(OtherActor));
}

// 서버 overlap 목록에 들어온 monster만 stun과 dot damage 대상으로 관리
void AStasisNetField::AddMonster(AMonsterBase* Monster)
{
	if (!HasAuthority() || !Monster || ActiveMonsters.Contains(Monster) || Monster->IsDead())
	{
		return;
	}

	ActiveMonsters.Add(Monster);
	Monster->ApplyStasisStun(this, StunDuration);
}

// field를 벗어난 monster는 source stun을 즉시 해제
void AStasisNetField::RemoveMonster(AMonsterBase* Monster)
{
	if (!HasAuthority() || !Monster)
	{
		return;
	}

	if (ActiveMonsters.Remove(Monster) > 0)
	{
		Monster->RemoveStasisStun(this);
	}
}

// field 소멸 시 남은 stun을 모두 강제로 닫음
void AStasisNetField::RemoveAllMonsters()
{
	TArray<TObjectPtr<AMonsterBase>> MonstersToRelease = ActiveMonsters.Array();
	ActiveMonsters.Reset();

	for (AMonsterBase* Monster : MonstersToRelease)
	{
		if (IsValid(Monster))
		{
			Monster->RemoveStasisStun(this);
		}
	}
}

// field 내부 monster에게 hack 상태 기준 dot damage를 1초 간격으로 적용
void AStasisNetField::ApplyDotDamage()
{
	if (!HasAuthority())
	{
		return;
	}

	TArray<TObjectPtr<AMonsterBase>> MonstersToRemove;
	for (AMonsterBase* Monster : ActiveMonsters)
	{
		if (!IsValid(Monster) || Monster->IsDead())
		{
			MonstersToRemove.Add(Monster);
			continue;
		}

		const float DamageAmount = Monster->IsHacked() ? HackedDotDamage : BaseDotDamage;
		if (DamageAmount <= 0.f)
		{
			continue;
		}

		FPragmataDamageSpec DamageSpec;
		DamageSpec.DamageAmount = DamageAmount;
		DamageSpec.SourceActor = SourceActor;
		DamageSpec.DamageCauser = this;
		DamageSpec.InstigatorController = InstigatorController;
		DamageSpec.DamageKind = EPragmataDamageKind::Weapon;
		DamageSpec.HitReactStrength = EPragmataHitReactStrength::None;
		DamageSpec.bHasImpact = false;
		DamageSpec.ImpactDirection = (Monster->GetActorLocation() - GetActorLocation()).GetSafeNormal();

		const bool bApplied = UPragmataDamageLibrary::DealDamageToActor(Monster, DamageSpec);
		UE_LOG(
			LogTemp,
			Log,
			TEXT("[StasisNetField::ApplyDotDamage] Target=%s Hacked=%d Damage=%.2f Applied=%d"),
			*GetNameSafe(Monster),
			Monster->IsHacked() ? 1 : 0,
			DamageAmount,
			bApplied ? 1 : 0);
	}

	for (AMonsterBase* Monster : MonstersToRemove)
	{
		RemoveMonster(Monster);
	}
}

// runtime VFX가 붙기 전까지 field 범위를 월드 debug sphere로 확인
void AStasisNetField::DrawFieldDebug() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	DrawDebugSphere(
		World,
		GetActorLocation(),
		FieldRadius,
		32,
		FColor::Yellow,
		false,
		DebugDrawLifetime,
		0,
		DebugLineThickness);
}
