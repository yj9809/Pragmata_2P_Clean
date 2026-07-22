#include "Game/PragmataGameState.h"

#include "Components/DianaNodeInteractionComponent.h"
#include "Pawns/DianaOperatorPawn.h"
#include "Characters/HughCharacter.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Game/PragmataPlayerController.h"
#include "Game/TutorialManagerSubsystem.h"
#include "Net/UnrealNetwork.h"

// 전역 전원 상태 변경을 local delegate로 알려 환경 반응을 분리
void APragmataGameState::SetGlobalPowerState(bool bNewPowerState)
{
	if (!HasAuthority())
	{
		return;
	}

	if (bGlobalPowerOn != bNewPowerState)
	{
		bGlobalPowerOn = bNewPowerState;
		OnPowerStateChanged.Broadcast(bGlobalPowerOn);
	}
}

// 전원 상태 복제 수신 시 client 로컬 조명/장치가 같은 델리게이트 경로로 갱신
void APragmataGameState::OnRep_GlobalPowerOn()
{
	OnPowerStateChanged.Broadcast(bGlobalPowerOn);
}

// gameplay role pointer와 connection flag를 모든 client의 조회 기준으로 복제
void APragmataGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(APragmataGameState, HughCharacter);
	DOREPLIFETIME(APragmataGameState, DianaOperatorPawn);
	DOREPLIFETIME(APragmataGameState, CheckpointUpgradeSelection);
	DOREPLIFETIME(APragmataGameState, CheckpointUpgradeHoldProgress);
	DOREPLIFETIME(APragmataGameState, bCheckpointUpgradeRefundHold);
	DOREPLIFETIME(APragmataGameState, RoomCode);
	DOREPLIFETIME(APragmataGameState, bHughConnected);
	DOREPLIFETIME(APragmataGameState, bDianaConnected);
	DOREPLIFETIME(APragmataGameState, bDianaActionsLocked);
	DOREPLIFETIME(APragmataGameState, bEnemyCombatActive);
	DOREPLIFETIME(APragmataGameState, bGameClearPresentationActive);
	DOREPLIFETIME(APragmataGameState, ActiveEnemyHackTarget);
	DOREPLIFETIME(APragmataGameState, ActiveEnemyHackSerial);
	DOREPLIFETIME(APragmataGameState, ActiveMapPuzzleActor);
	DOREPLIFETIME(APragmataGameState, ActiveBossTarget);
	DOREPLIFETIME(APragmataGameState, ActiveBossTargetSerial);
	DOREPLIFETIME(APragmataGameState, bGlobalPowerOn);
}

// 서버가 possess 완료된 Hugh pawn을 Diana 쪽 조회 기준으로 공개
void APragmataGameState::SetHughCharacter(AHughCharacter* NewHughCharacter)
{
	if (HasAuthority() && HughCharacter != NewHughCharacter)
	{
		HughCharacter = NewHughCharacter;
		OnHughCharacterChanged.Broadcast(HughCharacter);
	}
}

// 서버가 possess 완료된 Diana operator pawn을 Hugh/AnimBP 조회 기준으로 공개
void APragmataGameState::SetDianaOperatorPawn(ADianaOperatorPawn* NewDianaOperatorPawn)
{
	if (HasAuthority() && DianaOperatorPawn != NewDianaOperatorPawn)
	{
		DianaOperatorPawn = NewDianaOperatorPawn;
		OnDianaOperatorPawnChanged.Broadcast(DianaOperatorPawn);
	}
}

// lobby에서 넘어온 room code를 gameplay UI와 debug 표시 기준으로 유지
void APragmataGameState::SetRoomCode(const FString& NewRoomCode)
{
	if (HasAuthority())
	{
		RoomCode = NewRoomCode;
	}
}

// Hugh/Diana 접속 상태를 role별로 복제해 matchmaking 종료와 UI 표시를 분리
void APragmataGameState::SetRoleConnected(EPragmataPlayerRole PlayerRole, bool bConnected)
{
	if (!HasAuthority())
	{
		return;
	}

	if (PlayerRole == EPragmataPlayerRole::Hugh)
	{
		bHughConnected = bConnected;
	}
	else if (PlayerRole == EPragmataPlayerRole::Diana)
	{
		bDianaConnected = bConnected;
	}
}

// Territory가 마지막 보스 사망을 확정하면 server가 한 번만 clear 상태를 복제하고 listen host UI도 즉시 표시
void APragmataGameState::BeginGameClearPresentation()
{
	if (!HasAuthority() || bGameClearPresentationActive)
	{
		return;
	}

	bGameClearPresentationActive = true;
	ShowGameClearPresentationOnLocalControllers();
}

// paired animation 중 Diana 입력 잠금을 모든 client animation state에 전달
void APragmataGameState::SetDianaActionsLocked(bool bNewLocked)
{
	if (HasAuthority())
	{
		bDianaActionsLocked = bNewLocked;
	}
}

void APragmataGameState::SetCheckpointUpgradeSelection(EPragmataUpgradeType NewSelection)
{
	// Hugh 선택을 복제해 Diana mirror가 같은 row를 표시
	if (!HasAuthority() || CheckpointUpgradeSelection == NewSelection)
	{
		return;
	}
	CheckpointUpgradeSelection = NewSelection;
	OnCheckpointUpgradeSelectionChanged.Broadcast(CheckpointUpgradeSelection);
}

void APragmataGameState::OnRep_CheckpointUpgradeSelection()
{
	OnCheckpointUpgradeSelectionChanged.Broadcast(CheckpointUpgradeSelection);
}

void APragmataGameState::SetCheckpointUpgradeHold(float NewProgress, bool bNewRefund)
{
	// Hugh hold gauge를 Diana mirror까지 같은 session state로 전달
	if (!HasAuthority())
	{
		return;
	}
	CheckpointUpgradeHoldProgress = FMath::Clamp(NewProgress, 0.f, 1.f);
	bCheckpointUpgradeRefundHold = bNewRefund;
	OnCheckpointUpgradeHoldChanged.Broadcast(CheckpointUpgradeHoldProgress, bCheckpointUpgradeRefundHold);
}

void APragmataGameState::OnRep_CheckpointUpgradeHold()
{
	OnCheckpointUpgradeHoldChanged.Broadcast(CheckpointUpgradeHoldProgress, bCheckpointUpgradeRefundHold);
}

void APragmataGameState::RegisterEnemyAggro(AActor* EnemyActor, AActor* TargetActor)
{
	// enemy AI target 보유 여부를 양쪽 플레이어 combat 기준으로 집계
	if (!HasAuthority() || !EnemyActor || !IsCombatTargetActor(TargetActor))
	{
		return;
	}

	CleanupInvalidAggroEnemies();
	ActiveAggroEnemies.Add(EnemyActor);
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CombatExitGraceTimerHandle);
	}
	SetEnemyCombatActive(true);
}

void APragmataGameState::UnregisterEnemyAggro(AActor* EnemyActor)
{
	if (!HasAuthority() || !EnemyActor)
	{
		return;
	}

	ActiveAggroEnemies.Remove(EnemyActor);
	CleanupInvalidAggroEnemies();
	if (ActiveAggroEnemies.Num() > 0)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			CombatExitGraceTimerHandle,
			this,
			&ThisClass::HandleCombatExitGraceElapsed,
			CombatExitGraceSeconds,
			false
		);
	}
	else
	{
		SetEnemyCombatActive(false);
	}
}

void APragmataGameState::SetActiveEnemyHackTarget(AActor* NewTarget)
{
	// 같은 target 재시작도 client HUD가 다시 열리도록 serial을 같이 복제
	if (HasAuthority())
	{
		ActiveEnemyHackTarget = NewTarget;
		++ActiveEnemyHackSerial;
		OnActiveEnemyHackTargetChanged.Broadcast(ActiveEnemyHackTarget);
	}
}

void APragmataGameState::SetActiveMapPuzzleActor(AActor* NewPuzzleActor)
{
	// listen server HUD도 OnRep 없이 같은 경로로 갱신
	if (HasAuthority() && ActiveMapPuzzleActor != NewPuzzleActor)
	{
		ActiveMapPuzzleActor = NewPuzzleActor;
		OnActiveMapPuzzleActorChanged.Broadcast(ActiveMapPuzzleActor);
	}
}

void APragmataGameState::SetActiveBossTarget(AActor* NewBossTarget)
{
	// boss HP target은 GameState 복제를 통해 Hugh/Diana 로컬 HUD 양쪽에 같은 actor를 전달
	if (HasAuthority())
	{
		ActiveBossTarget = NewBossTarget;
		++ActiveBossTargetSerial;
		OnActiveBossTargetChanged.Broadcast(ActiveBossTarget);
	}
}

void APragmataGameState::RegisterScannablePOI(AActor* POIActor)
{
	// registry는 각 world의 local 후보 cache이며 서버 RPC에서 다시 권위 판정
	if (POIActor)
	{
		RegisteredScannablePOIs.Add(POIActor);
	}
}

void APragmataGameState::UnregisterScannablePOI(AActor* POIActor)
{
	if (POIActor)
	{
		RegisteredScannablePOIs.Remove(POIActor);
	}
}

void APragmataGameState::GetRegisteredScannablePOIs(TArray<AActor*>& OutPOIActors) const
{
	OutPOIActors.Reset();

	for (const TObjectPtr<AActor>& POIActor : RegisteredScannablePOIs)
	{
		if (IsValid(POIActor.Get()))
		{
			OutPOIActors.Add(POIActor.Get());
		}
	}
}

void APragmataGameState::OnRep_ActiveEnemyHackTarget()
{
	OnActiveEnemyHackTargetChanged.Broadcast(ActiveEnemyHackTarget);
}

void APragmataGameState::OnRep_ActiveEnemyHackSerial()
{
	// target pointer 값이 같아도 새 hack session이면 HUD를 다시 bind
	OnActiveEnemyHackTargetChanged.Broadcast(ActiveEnemyHackTarget);
}

void APragmataGameState::OnRep_ActiveMapPuzzleActor()
{
	OnActiveMapPuzzleActorChanged.Broadcast(ActiveMapPuzzleActor);
}

void APragmataGameState::OnRep_ActiveBossTarget()
{
	OnActiveBossTargetChanged.Broadcast(ActiveBossTarget);
}

void APragmataGameState::OnRep_ActiveBossTargetSerial()
{
	// 같은 boss actor를 다시 표시하는 경우에도 HUD bind 경로를 다시 실행
	OnActiveBossTargetChanged.Broadcast(ActiveBossTarget);
}

void APragmataGameState::OnRep_EnemyCombatActive()
{
	OnEnemyCombatActiveChanged.Broadcast(bEnemyCombatActive);
}

// 복제된 clear 상태를 받은 peer마다 자신의 local controller에만 UI 생성을 요청
void APragmataGameState::OnRep_GameClearPresentationActive()
{
	if (bGameClearPresentationActive)
	{
		ShowGameClearPresentationOnLocalControllers();
	}
}

// client HUD가 늦게 도착한 Hugh stat source를 다시 구독
void APragmataGameState::OnRep_HughCharacter()
{
	OnHughCharacterChanged.Broadcast(HughCharacter);
}

// client HUD가 늦게 도착한 Diana stat source를 다시 구독
void APragmataGameState::OnRep_DianaOperatorPawn()
{
	OnDianaOperatorPawnChanged.Broadcast(DianaOperatorPawn);
}

void APragmataGameState::SetEnemyCombatActive(bool bNewActive)
{
	if (HasAuthority() && bEnemyCombatActive != bNewActive)
	{
		bEnemyCombatActive = bNewActive;
		if (DianaOperatorPawn)
		{
			const EDianaOperatorMode CurrentMode = DianaOperatorPawn->GetOperatorMode();
			if (bEnemyCombatActive && CurrentMode == EDianaOperatorMode::Roaming)
			{
				DianaOperatorPawn->SetOperatorMode(EDianaOperatorMode::Combat);
				if (UDianaNodeInteractionComponent* NodeInteractionComponent = DianaOperatorPawn->FindComponentByClass<UDianaNodeInteractionComponent>())
				{
					NodeInteractionComponent->CancelActiveMapHackFromServer();
				}
			}
			else if (!bEnemyCombatActive && CurrentMode == EDianaOperatorMode::Combat)
			{
				DianaOperatorPawn->SetOperatorMode(EDianaOperatorMode::Roaming);
			}
		}
		OnEnemyCombatActiveChanged.Broadcast(bEnemyCombatActive);
	}
}

void APragmataGameState::HandleCombatExitGraceElapsed()
{
	if (!HasAuthority())
	{
		return;
	}

	CleanupInvalidAggroEnemies();
	if (ActiveAggroEnemies.Num() == 0)
	{
		SetEnemyCombatActive(false);
	}
}

// 서버 호출 한 번으로 Hugh/Diana 양쪽 클라이언트가 각자 로컬 Subsystem에서 표시 (role 필터는 Subsystem 내부에서 처리)
void APragmataGameState::Multicast_ShowTutorialStep_Implementation(UTutorialStepData* StepData)
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UTutorialManagerSubsystem* TutorialManager = GameInstance->GetSubsystem<UTutorialManagerSubsystem>())
		{
			TutorialManager->ShowTutorialStep(StepData);
		}
	}
}

void APragmataGameState::Multicast_HideTutorial_Implementation()
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UTutorialManagerSubsystem* TutorialManager = GameInstance->GetSubsystem<UTutorialManagerSubsystem>())
		{
			TutorialManager->HideTutorial();
		}
	}
}

// 서버가 표시 요청마다 0이 아닌 world 고유 serial을 발급해 timed hide 대상을 고정
uint32 APragmataGameState::BeginTutorialPresentation(UTutorialStepData* StepData)
{
	if (!HasAuthority() || !StepData)
	{
		return 0;
	}

	++TutorialPresentationSerial;
	if (TutorialPresentationSerial == 0)
	{
		++TutorialPresentationSerial;
	}

	Multicast_ShowTutorialPresentation(StepData, TutorialPresentationSerial);
	return TutorialPresentationSerial;
}

// 각 local peer가 저장된 TutorialId를 독립 검사하도록 ID와 새 serial을 함께 multicast
uint32 APragmataGameState::BeginTutorialPresentationOnce(FName TutorialId, UTutorialStepData* StepData)
{
	if (!HasAuthority() || TutorialId == NAME_None || !StepData)
	{
		return 0;
	}

	++TutorialPresentationSerial;
	if (TutorialPresentationSerial == 0)
	{
		++TutorialPresentationSerial;
	}

	Multicast_ShowTutorialPresentationOnce(TutorialId, StepData, TutorialPresentationSerial);
	return TutorialPresentationSerial;
}

// 각 peer가 local role과 UI 상태를 검증한 뒤 성공한 presentation만 활성화
void APragmataGameState::Multicast_ShowTutorialPresentation_Implementation(UTutorialStepData* StepData, uint32 PresentationSerial)
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UTutorialManagerSubsystem* TutorialManager = GameInstance->GetSubsystem<UTutorialManagerSubsystem>())
		{
			TutorialManager->ShowTutorialPresentation(StepData, PresentationSerial);
		}
	}
}

// 역할과 local SaveGame 이력 검사는 각 peer의 TutorialManagerSubsystem에서 수행
void APragmataGameState::Multicast_ShowTutorialPresentationOnce_Implementation(FName TutorialId, UTutorialStepData* StepData, uint32 PresentationSerial)
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UTutorialManagerSubsystem* TutorialManager = GameInstance->GetSubsystem<UTutorialManagerSubsystem>())
		{
			TutorialManager->ShowTutorialOnce(TutorialId, StepData, PresentationSerial);
		}
	}
}

// expected serial을 로컬 manager에 전달해 최신 popup 오종료를 차단
void APragmataGameState::Multicast_HideTutorialPresentation_Implementation(uint32 ExpectedPresentationSerial)
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UTutorialManagerSubsystem* TutorialManager = GameInstance->GetSubsystem<UTutorialManagerSubsystem>())
		{
			TutorialManager->HideTutorialPresentation(ExpectedPresentationSerial);
		}
	}
}

void APragmataGameState::Multicast_ShowObjectiveAlert_Implementation(const FText& ObjectiveText, float DisplaySeconds)
{
	// 서버의 목표 생성 이벤트를 저장 상태로 복제하지 않고, 현재 각 로컬 HUD 표시 요청으로만 전달
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APragmataPlayerController* PragmataController = Cast<APragmataPlayerController>(It->Get());
		if (PragmataController && PragmataController->IsLocalController())
		{
			PragmataController->ShowObjectiveAlert(ObjectiveText, DisplaySeconds);
		}
	}
}

void APragmataGameState::Multicast_PlayDialogueLine_Implementation(const FDialogueLine& Line, float DisplaySeconds)
{
	// 대사 표시 판정은 서버가 소유하고, 상태 복제 없이 각 로컬 HUD 표시 요청으로만 전달
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APragmataPlayerController* PragmataController = Cast<APragmataPlayerController>(It->Get());
		if (PragmataController && PragmataController->IsLocalController())
		{
			PragmataController->PlayDialogueLine(Line, DisplaySeconds);
		}
	}
}

void APragmataGameState::Multicast_HideDialogue_Implementation()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APragmataPlayerController* PragmataController = Cast<APragmataPlayerController>(It->Get());
		if (PragmataController && PragmataController->IsLocalController())
		{
			PragmataController->HideDialogue();
		}
	}
}

// clear 확인 입력은 server가 검증한 뒤 모든 peer가 각자 세션을 정리하고 Title map으로 돌아감
void APragmataGameState::Multicast_ReturnPlayersToTitle_Implementation()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APragmataPlayerController* PragmataController = Cast<APragmataPlayerController>(It->Get());
		if (PragmataController && PragmataController->IsLocalController())
		{
			PragmataController->ReturnToTitleFromGameClear();
		}
	}
}

// listen host는 OnRep를 받지 않으므로 server에서 local controller UI를 직접 시작
void APragmataGameState::ShowGameClearPresentationOnLocalControllers()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APragmataPlayerController* PragmataController = Cast<APragmataPlayerController>(It->Get());
		if (PragmataController && PragmataController->IsLocalController())
		{
			PragmataController->ShowGameClearPresentation();
		}
	}
}

bool APragmataGameState::IsCombatTargetActor(const AActor* TargetActor) const
{
	return TargetActor && (TargetActor == HughCharacter || TargetActor == DianaOperatorPawn);
}

void APragmataGameState::CleanupInvalidAggroEnemies()
{
	for (auto It = ActiveAggroEnemies.CreateIterator(); It; ++It)
	{
		if (!IsValid(*It))
		{
			It.RemoveCurrent();
		}
	}
}
