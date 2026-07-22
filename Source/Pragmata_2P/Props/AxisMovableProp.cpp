#include "Props/AxisMovableProp.h"

#include "Components/ChildActorComponent.h"
#include "Components/SceneComponent.h"
#include "Net/UnrealNetwork.h"
#include "Props/PropInteractionBase.h"

AAxisMovableProp::AAxisMovableProp()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bReplicates = true;
	SetReplicateMovement(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	InteractionActorComponent = CreateDefaultSubobject<UChildActorComponent>(TEXT("InteractionActor"));
	InteractionActorComponent->SetupAttachment(SceneRoot);
}

void AAxisMovableProp::BeginPlay()
{
	Super::BeginPlay();

	// 배치 위치를 축 이동의 기준점으로 고정
	CacheInitialLocationIfNeeded();
	if (HasAuthority())
	{
		bIsAtTarget = bStartMoved;
	}
	if (bStartMoved)
	{
		SetActorLocation(GetLocationForState(true));
	}

	if (APropInteractionBase* ResolvedInteractionActor = ResolveInteractionActor())
	{
		ResolvedInteractionActor->OnInteractionActivated.AddUniqueDynamic(this, &AAxisMovableProp::HandleInteractionActivated);
	}
}

void AAxisMovableProp::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// 각 machine에서 같은 시작점과 목표점으로 이동 연출만 재생
	if (!bIsMoving)
	{
		SetActorTickEnabled(false);
		return;
	}

	MoveElapsedTime += DeltaSeconds;
	const float Alpha = MoveDuration <= KINDA_SMALL_NUMBER ? 1.f : FMath::Clamp(MoveElapsedTime / MoveDuration, 0.f, 1.f);
	SetActorLocation(FMath::Lerp(MoveStartLocation, MoveTargetLocation, Alpha));

	if (Alpha >= 1.f)
	{
		FinishMovement();
	}
}

void AAxisMovableProp::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 목표 상태와 이동 설정은 서버 값을 기준으로 복제
	DOREPLIFETIME(AAxisMovableProp, MoveDirection);
	DOREPLIFETIME(AAxisMovableProp, MoveDistance);
	DOREPLIFETIME(AAxisMovableProp, MoveDuration);
	DOREPLIFETIME(AAxisMovableProp, bIsAtTarget);
}

void AAxisMovableProp::ToggleMoved()
{
	if (!HasAuthority())
	{
		return;
	}

	SetMoved(!bIsAtTarget);
}

void AAxisMovableProp::HandleInteractionActivated(AActor* Interactor)
{
	// 연결된 상호작용 액터의 발동 신호만 받아 서버 권위 이동으로 변환
	(void)Interactor;
	// 전투 봉쇄 중인 자식 prop은 이동 상호작용을 거부
	if (!IsMovementInteractionEnabled())
	{
		return;
	}

	ToggleMoved();
}

void AAxisMovableProp::SetMoved(bool bNewMoved)
{
	// server에서 목표 상태를 갱신하고 client는 OnRep 경로로 따라감
	if (!HasAuthority() || bIsAtTarget == bNewMoved)
	{
		return;
	}

	bIsAtTarget = bNewMoved;
	StartMoveToState(bIsAtTarget);
}

void AAxisMovableProp::OnRep_IsAtTarget()
{
	// client는 복제된 목표 상태를 받아 로컬에서 같은 축 이동을 재생
	StartMoveToState(bIsAtTarget);
}

void AAxisMovableProp::CacheInitialLocationIfNeeded()
{
	if (bHasInitialLocation)
	{
		return;
	}

	InitialLocation = GetActorLocation();
	bHasInitialLocation = true;
}

FVector AAxisMovableProp::GetMoveOffset() const
{
	const FVector SafeDirection = MoveDirection.IsNearlyZero() ? FVector::UpVector : MoveDirection.GetSafeNormal();
	return SafeDirection * MoveDistance;
}

FVector AAxisMovableProp::GetLocationForState(bool bMoved) const
{
	return bMoved ? InitialLocation + GetMoveOffset() : InitialLocation;
}

void AAxisMovableProp::StartMoveToState(bool bMoved)
{
	// OnRep와 BeginPlay 순서 차이를 막기 위해 목표 계산 전 기준 위치 보장
	CacheInitialLocationIfNeeded();
	MoveStartLocation = GetActorLocation();
	MoveTargetLocation = GetLocationForState(bMoved);
	MoveElapsedTime = 0.f;

	if (MoveDuration <= KINDA_SMALL_NUMBER || MoveStartLocation.Equals(MoveTargetLocation))
	{
		SetActorLocation(MoveTargetLocation);
		FinishMovement();
		return;
	}

	bIsMoving = true;
	SetActorTickEnabled(true);
}

void AAxisMovableProp::FinishMovement()
{
	bIsMoving = false;
	SetActorLocation(MoveTargetLocation);
	SetActorTickEnabled(false);
}

APropInteractionBase* AAxisMovableProp::ResolveInteractionActor() const
{
	// 뷰포트에 붙인 ChildActor를 상호작용 actor로 사용
	if (!InteractionActorComponent)
	{
		return nullptr;
	}

	AActor* ChildActor = InteractionActorComponent->GetChildActor();
	APropInteractionBase* InteractionActor = Cast<APropInteractionBase>(ChildActor);
	if (!InteractionActor && ChildActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AAxisMovableProp::ResolveInteractionActor] Child actor is not APropInteractionBase: %s"), *GetNameSafe(ChildActor));
	}

	return InteractionActor;
}
