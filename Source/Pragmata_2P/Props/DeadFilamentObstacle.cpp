#include "Props/DeadFilamentObstacle.h"

#include "Components/DianaCleanseComponent.h"
#include "Components/StaticMeshComponent.h"

ADeadFilamentObstacle::ADeadFilamentObstacle()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	DeadFilamentMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DeadFilamentMesh"));
	SetRootComponent(DeadFilamentMesh);
}

void ADeadFilamentObstacle::Interact_Implementation(AActor* Interactor)
{
	// DeadFilament 제거는 server 정화 스킬 실행 중인 Diana component만 허용
	if (!HasAuthority())
	{
		return;
	}

	// 정화 스킬 판정 루프 밖에서 직접 Interact를 호출한 경우는 제거 권한이 아님
	if (!CanBeCleansedBy(Interactor))
	{
		return;
	}

	Destroy();
}

bool ADeadFilamentObstacle::CanBeCleansedBy(AActor* Interactor) const
{
	// 일반 special interaction 호출이 장애물을 지우지 못하게 정화 스킬 실행 guard 확인
	const UDianaCleanseComponent* CleanseComponent = Interactor ? Interactor->FindComponentByClass<UDianaCleanseComponent>() : nullptr;
	// bServerCleanseExecuting은 서버가 이미 cone/LOS/cooldown 검증을 끝낸 짧은 적용 구간
	return CleanseComponent && CleanseComponent->IsServerCleanseExecuting();
}
