#include "Props/TutorialTriggerActor.h"

#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "Game/PragmataGameState.h"
#include "Game/PragmataPlayerController.h"
#include "Game/TutorialBlueprintLibrary.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"

ATutorialTriggerActor::ATutorialTriggerActor()
{
	PrimaryActorTick.bCanEverTick = false;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	SetRootComponent(TriggerBox);
	TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerBox->SetCollisionObjectType(ECC_WorldDynamic);
	TriggerBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	TriggerBox->SetGenerateOverlapEvents(true);
	TriggerBox->ShapeColor = FColor::Yellow;
}

// runtime begin/end overlap 경로를 하나로 유지하도록 native trigger delegate를 한 번만 연결
void ATutorialTriggerActor::BeginPlay()
{
	Super::BeginPlay();

	if (TriggerBox)
	{
		TriggerBox->OnComponentBeginOverlap.AddUniqueDynamic(this, &ATutorialTriggerActor::HandleTriggerBeginOverlap);
		TriggerBox->OnComponentEndOverlap.AddUniqueDynamic(this, &ATutorialTriggerActor::HandleTriggerEndOverlap);
	}
}

void ATutorialTriggerActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// streaming level unload 뒤 timer가 사라진 actor를 다시 호출하지 않게 정리
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DismissTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

// overlap 표시 요청은 server에서만 발행하고 각 peer의 영구 이력으로 중복을 차단
void ATutorialTriggerActor::HandleTriggerBeginOverlap(
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

	TriggerConfiguredTutorial(OtherActor);
}

// 서버가 WeaponUse 같은 map-local objective를 최초 종료 overlap 한 번으로 양쪽 HUD에 알림
void ATutorialTriggerActor::HandleTriggerEndOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex)
{
	(void)OverlappedComponent;
	(void)OtherComp;
	(void)OtherBodyIndex;

	if (!HasAuthority() || !bEnabled || !bShowObjectiveAlertOnExit || bExitObjectiveAlertShown)
	{
		return;
	}

	APragmataPlayerController* TriggeringController = GetValidTriggeringController(OtherActor);
	if (!TriggeringController)
	{
		return;
	}

	if (ActivationRole != EPragmataPlayerRole::None && TriggeringController->GetAssignedRole() != ActivationRole)
	{
		return;
	}

	if (ExitObjectiveAlertText.IsEmptyOrWhitespace())
	{
		UE_LOG(LogTemp, Warning, TEXT("[ATutorialTriggerActor::HandleTriggerEndOverlap] ExitObjectiveAlertText가 설정되지 않음: %s"), *GetName());
		return;
	}

	UWorld* World = GetWorld();
	APragmataGameState* PragmataGameState = World ? World->GetGameState<APragmataGameState>() : nullptr;
	if (!PragmataGameState)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ATutorialTriggerActor::HandleTriggerEndOverlap] PragmataGameState를 찾지 못함: %s"), *GetName());
		return;
	}

	bExitObjectiveAlertShown = true;
	PragmataGameState->Multicast_ShowObjectiveAlert(ExitObjectiveAlertText, FMath::Max(ExitObjectiveAlertDisplaySeconds, 0.1f));
}

// actor/controller/role 검증 뒤 stable TutorialId를 공통 영구 1회 경로로 전달
void ATutorialTriggerActor::TriggerConfiguredTutorial(AActor* TriggeringActor)
{
	if (!HasAuthority() || !bEnabled)
	{
		return;
	}

	APragmataPlayerController* TriggeringController = GetValidTriggeringController(TriggeringActor);
	if (!TriggeringController)
	{
		return;
	}

	if (ActivationRole != EPragmataPlayerRole::None && TriggeringController->GetAssignedRole() != ActivationRole)
	{
		return;
	}

	if (!TutorialStep)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ATutorialTriggerActor::TriggerConfiguredTutorial] TutorialStep이 설정되지 않음: %s"), *GetName());
		return;
	}

	if (TutorialId == NAME_None)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ATutorialTriggerActor::TriggerConfiguredTutorial] TutorialId가 설정되지 않음: %s"), *GetName());
		return;
	}

	uint32 PresentationSerial = 0;
	if (!UTutorialBlueprintLibrary::ShowTutorialOnceWithPresentationSerial(this, TutorialId, TutorialStep, PresentationSerial))
	{
		return;
	}

	// 재사용 trigger의 이전 timer가 새 presentation serial을 닫지 않게 먼저 해제
	GetWorld()->GetTimerManager().ClearTimer(DismissTimerHandle);
	LatestPresentationSerial = PresentationSerial;

	if (DismissPolicy == ETutorialTriggerDismissPolicy::Timed)
	{
		GetWorld()->GetTimerManager().SetTimer(
			DismissTimerHandle,
			this,
			&ATutorialTriggerActor::HandleDismissTimerElapsed,
			FMath::Max(DisplayDurationSeconds, 0.1f),
			false);
	}
}

// 명시 hide도 이 actor의 최신 serial만 요청해 다른 trigger popup을 보존
void ATutorialTriggerActor::HideConfiguredTutorial()
{
	if (!HasAuthority() || LatestPresentationSerial == 0)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DismissTimerHandle);
		if (APragmataGameState* PragmataGameState = World->GetGameState<APragmataGameState>())
		{
			PragmataGameState->Multicast_HideTutorialPresentation(LatestPresentationSerial);
			LatestPresentationSerial = 0;
		}
	}
}

// timer 만료 시 callback 시점의 최신 serial만 닫음
void ATutorialTriggerActor::HandleDismissTimerElapsed()
{
	HideConfiguredTutorial();
}

// player-controlled Pawn과 프로젝트 controller를 모두 확인해 AI/환경 overlap을 제외
APragmataPlayerController* ATutorialTriggerActor::GetValidTriggeringController(AActor* TriggeringActor) const
{
	APawn* Pawn = Cast<APawn>(TriggeringActor);
	if (!Pawn || !Pawn->IsPlayerControlled())
	{
		return nullptr;
	}

	return Cast<APragmataPlayerController>(Pawn->GetController());
}
