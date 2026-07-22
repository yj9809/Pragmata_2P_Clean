#include "Props/OverDrivePulseRingActor.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

AOverDrivePulseRingActor::AOverDrivePulseRingActor()
{
	PrimaryActorTick.bCanEverTick = true;
	SetReplicates(true);
	SetReplicateMovement(false);

	RingSegments = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("RingSegments"));
	SetRootComponent(RingSegments);
	RingSegments->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RingSegments->SetCastShadow(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> RingMaterial(TEXT("/Game/Materials/M_OverDriveRing.M_OverDriveRing"));
	if (CubeMesh.Succeeded())
	{
		RingSegments->SetStaticMesh(CubeMesh.Object);
	}
	if (RingMaterial.Succeeded())
	{
		RingSegments->SetMaterial(0, RingMaterial.Object);
	}
}

void AOverDrivePulseRingActor::BeginPlay()
{
	// client별 spawn 지연과 무관하게 local time으로 ring 확장을 시작
	Super::BeginPlay();
	StartTimeSeconds = GetWorld()->GetTimeSeconds();
	SetLifeSpan(ExpansionDuration + 0.25f);
	UpdateRing(0.f);
}

void AOverDrivePulseRingActor::Tick(float DeltaSeconds)
{
	// floor decal 대신 world-space mesh segment를 50m까지 확장
	Super::Tick(DeltaSeconds);

	const float ElapsedSeconds = GetWorld()->GetTimeSeconds() - StartTimeSeconds;
	const float Alpha = FMath::Clamp(ElapsedSeconds / ExpansionDuration, 0.f, 1.f);
	UpdateRing(MaximumRadius * Alpha);
	if (Alpha >= 1.f)
	{
		SetActorTickEnabled(false);
	}
}

void AOverDrivePulseRingActor::UpdateRing(float Radius)
{
	// cube segment를 접선 방향으로 배치해 non-zero Z ring silhouette 유지
	const int32 SafeSegmentCount = FMath::Max(SegmentCount, 8);
	const float SegmentLength = (2.f * PI * FMath::Max(Radius, 1.f)) / SafeSegmentCount;
	const FVector SegmentScale(SegmentLength / 100.f, 0.25f, 0.08f);

	while (RingSegments->GetInstanceCount() < SafeSegmentCount)
	{
		RingSegments->AddInstance(FTransform::Identity);
	}

	for (int32 Index = 0; Index < SafeSegmentCount; ++Index)
	{
		const float Angle = (2.f * PI * Index) / SafeSegmentCount;
		const FVector Location(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0.f);
		const FRotator Rotation(0.f, FMath::RadiansToDegrees(Angle) + 90.f, 0.f);
		RingSegments->UpdateInstanceTransform(Index, FTransform(Rotation, Location, SegmentScale), false, true, true);
	}
}
