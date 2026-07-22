#include "PoweredResponBox.h"
#include "Game/PragmataGameState.h" // 실제 프로젝트의 게임스테이트 헤더 포함
#include "Characters/PlayableCharacter.h"
#include "Enemy/MonsterTerritoryActor.h"
#include "TimerManager.h"
#include "Game/TutorialBlueprintLibrary.h"

void APoweredResponBox::BeginPlay()
{
	// 1. 부모 로직 실행
	Super::BeginPlay();

	// 2. 메시 및 상태 강제 초기화 (닫힘 상태 보장)
	if (DoorMesh)
	{
		DoorMesh->Stop();
		DoorMesh->SetPosition(0.0f);
	}
	bIsOpen = false;
	bIsAnimating = false;
	bIsPlayerNearby = false; // 초기엔 근처에 없음

	// 3. 델리게이트 구독 (중요: 여기서 바로 UpdateDoorState를 호출하지 마십시오!)
	if (APragmataGameState* GS = GetWorld()->GetGameState<APragmataGameState>())
	{
		GS->OnPowerStateChanged.AddDynamic(this, &APoweredResponBox::HandlePowerStateChanged);

		// 전원 상태만 먼저 동기화 (잠금/해제 여부만 결정)
		bIsLocked = !GS->GetGlobalPowerState();
	}
}

void APoweredResponBox::HandlePowerStateChanged(bool bIsPowerOn)
{
	// 전원이 변경되면 잠금 상태만 갱신합니다.
	bIsLocked = !bIsPowerOn;

	// 만약 전원이 들어왔는데 이미 플레이어가 근처에 서 있다면? 그때만 문을 엽니다.
	if (bIsPowerOn && bIsPlayerNearby)
	{
		UpdateDoorState();
		
		//hack enemy tutorial
		if (HackingEnemyTutorialData)
			UTutorialBlueprintLibrary::ShowTutorialOnce(this, TEXT("HackingEnemy"), HackingEnemyTutorialData);
	}
	// 전원이 꺼지면 닫아야 함
	else if (!bIsPowerOn)
	{
		// 부모의 닫기 로직 호출 (필요시)
		CloseDoor();
	}
}

void APoweredResponBox::UpdateDoorState()
{
	// 정석적인 Gatekeeper 패턴
	// 1. 이미 열려있거나, 2. 잠겨있거나, 3. 플레이어가 근처에 없으면 실행 안 함
	if (bIsOpen || bIsLocked || !bIsPlayerNearby)
	{
		return;
	}

	const bool bWasOpen = bIsOpen;
	Super::UpdateDoorState();

	if (HasAuthority() && !bWasOpen && bIsPlayerNearby && !bIsLocked)
	{
		ActivateLinkedTerritoryTriggers();
	}
}

void APoweredResponBox::OpenDoor()
{
	const bool bWasOpen = bIsOpen;

	Super::OpenDoor();

	if (!bWasOpen && bIsOpen)
	{
		ActivateLinkedTerritoryTriggers();
	}
}

void APoweredResponBox::OnAnimationFinished()
{
	Super::OnAnimationFinished();

	// 열림 애니메이션을 끝까지 재생한 server만 actor 삭제를 결정한다.
	if (!HasAuthority() || !bRemoveAfterOpening || !bIsOpen || bHasScheduledRemovalAfterOpen)
	{
		return;
	}

	bHasScheduledRemovalAfterOpen = true;
	GetWorldTimerManager().SetTimer(RemoveAfterOpenTimerHandle, this, &ThisClass::RemoveAfterOpenDelay, RemovalDelayAfterOpening, false);
}

void APoweredResponBox::RemoveAfterOpenDelay()
{
	Destroy();
}

// 문이 실제로 열린 서버 시점에 연결된 Territory 전투 시작을 승인
void APoweredResponBox::ActivateLinkedTerritoryTriggers()
{
	if (!HasAuthority() || bHasActivatedLinkedTerritories)
	{
		return;
	}

	bool bActivatedAnyTerritory = false;
	for (AMonsterTerritoryActor* Territory : TerritoriesToActivateOnOpen)
	{
		if (IsValid(Territory))
		{
			Territory->ActivateTerritoryTrigger();
			bActivatedAnyTerritory = true;
		}
	}

	if (bActivatedAnyTerritory)
	{
		bHasActivatedLinkedTerritories = true;
	}
	else if (TerritoriesToActivateOnOpen.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[APoweredResponBox::ActivateLinkedTerritoryTriggers] No territories assigned"));
	}
}

void APoweredResponBox::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// 1. 유효성 검사 및 자기 자신과의 충돌 방지
	if (OtherActor && OtherActor != this)
	{
		// 2. 정확히 플레이어 캐릭터인지 확인 (다른 NPC나 발사체 무시)
		if (APlayableCharacter* PlayerChar = Cast<APlayableCharacter>(OtherActor))
		{
			bIsPlayerNearby = true;

			// 플레이어가 진입했으므로 문 개방 조건을 재평가합니다.
			// 전원이 켜져 있다면 UpdateDoorState 내부 조건이 충족되어 문이 열립니다.
			UpdateDoorState();
		}
	}
}

void APoweredResponBox::OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (OtherActor && OtherActor != this)
	{
		if (APlayableCharacter* PlayerChar = Cast<APlayableCharacter>(OtherActor))
		{
			bIsPlayerNearby = false;

		}
	}
}
