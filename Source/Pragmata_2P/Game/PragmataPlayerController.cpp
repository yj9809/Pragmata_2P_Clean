#include "Game/PragmataPlayerController.h"

#include "EnhancedInputSubsystems.h"
#include "Blueprint/UserWidget.h"
#include "Camera/PlayerCameraManager.h"
#include "Characters/HughCharacter.h"
#include "Components/WidgetComponent.h"
#include "Components/PragmataPlayerStatsComponent.h"
#include "Data/PragmataGameFlowConfig.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameInstance.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Game/PragmataGameState.h"
#include "Game/PragmataGameMode.h"
#include "Game/PragmataMultiplayerSessionSubsystem.h"
#include "Game/PragmataPlayerState.h"
#include "Game/TutorialManagerSubsystem.h"
#include "Game/TutorialBlueprintLibrary.h"
#include "InputCoreTypes.h"
#include "InputKeyEventArgs.h"
#include "InputMappingContext.h"
#include "Interfaces/PragmataDamageable.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Pawns/DianaOperatorPawn.h"
#include "Props/CleanseSkillAcquisitionProp.h"
#include "UI/ScanIconWidget.h"
#include "UI/PragmataHUDWidget.h"
#include "UI/PragmataGameClearWidget.h"
#include "UI/PragmataUpgradeMenuWidget.h"

namespace
{
constexpr float GamepadAxisDeviceSwitchThreshold = 0.2f;
constexpr float PointerAxisDeviceSwitchThreshold = 0.01f;

bool IsMeaningfulInputDeviceEvent(const FInputKeyEventArgs& Params)
{
	if (Params.Event == IE_Released || Params.Event == IE_MAX)
	{
		return false;
	}

	const FKey Key = Params.Key;
	if (!Key.IsAnalog())
	{
		return true;
	}

	const float Threshold = Key.IsGamepadKey() ? GamepadAxisDeviceSwitchThreshold : PointerAxisDeviceSwitchThreshold;
	return FMath::Abs(Params.AmountDepressed) >= Threshold;
}

// 9키 테스트에서 키보드 방향 입력을 피격 방향으로 변환
bool TryGetDebugDamageDirection(const FKey& Key, EPragmataHitDirection& OutHitDirection)
{
	if (Key == EKeys::W || Key == EKeys::Up)
	{
		OutHitDirection = EPragmataHitDirection::Front;
		return true;
	}
	if (Key == EKeys::S || Key == EKeys::Down)
	{
		OutHitDirection = EPragmataHitDirection::Back;
		return true;
	}
	if (Key == EKeys::A || Key == EKeys::Left)
	{
		OutHitDirection = EPragmataHitDirection::Left;
		return true;
	}
	if (Key == EKeys::D || Key == EKeys::Right)
	{
		OutHitDirection = EPragmataHitDirection::Right;
		return true;
	}

	return false;
}
}

APragmataPlayerController::APragmataPlayerController()
{
	HughInputMappingContext = TSoftObjectPtr<UInputMappingContext>(FSoftObjectPath(TEXT("/Game/Blueprints/Player/Input/Hugh/IMC_Hugh.IMC_Hugh")));
	DianaInputMappingContext = TSoftObjectPtr<UInputMappingContext>(FSoftObjectPath(TEXT("/Game/Blueprints/Player/Input/Diana/IMC_Diana.IMC_Diana")));
	HughCrosshairWidgetClass = TSoftClassPtr<UUserWidget>(FSoftClassPath(TEXT("/Game/UI/Widget/WBP_Crosshair.WBP_Crosshair_C")));
	UpgradeMenuWidgetClass = TSoftClassPtr<UPragmataUpgradeMenuWidget>(FSoftClassPath(TEXT("/Game/UI/Widget/Upgrade/WBP_UpgradeMenuWidget.WBP_UpgradeMenuWidget_C")));
}

// 서버가 지정한 owning client에서만 역할 전용 튜토리얼을 표시
void APragmataPlayerController::Client_ShowTutorialStep_Implementation(UTutorialStepData* StepData)
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UTutorialManagerSubsystem* TutorialManager = GameInstance->GetSubsystem<UTutorialManagerSubsystem>())
		{
			TutorialManager->ShowTutorialStep(StepData);
		}
	}
}

// PlayerState가 이미 있거나 나중에 복제되는 두 경로 모두 role input을 적용
void APragmataPlayerController::BeginPlay()
{
	Super::BeginPlay();

	BindPlayerStateRole();
	ApplyGameplayInputMode();
	ApplyRoleInputAndCamera();
	EnsureGameplayHUD();
	EnsureHughCrosshair();
}

void APragmataPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// controller 종료 시 pending reveal과 object icon을 함께 정리해 다음 possession에 남기지 않는다.
	for (TPair<TObjectPtr<AActor>, FTimerHandle>& Pair : ScanIconRevealTimers)
	{
		GetWorldTimerManager().ClearTimer(Pair.Value);
	}
	ScanIconRevealTimers.Reset();
	HideActiveScanIcons();
	StopScanPulse();
	GetWorldTimerManager().ClearTimer(GameClearPresentationTimerHandle);
	GetWorldTimerManager().ClearTimer(GameClearReturnTimerHandle);

	Super::EndPlay(EndPlayReason);
}

// 서버 possess 직후 role별 mapping과 camera clamp를 다시 적용
void APragmataPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	ApplyGameplayInputMode();
	ApplyRoleInputAndCamera();
	EnsureGameplayHUD();
}

// owning client가 possession을 승인한 뒤 local input state를 보정
void APragmataPlayerController::AcknowledgePossession(APawn* P)
{
	Super::AcknowledgePossession(P);

	ApplyGameplayInputMode();
	ApplyRoleInputAndCamera();
	EnsureGameplayHUD();
	EnsureHughCrosshair();
}

// PlayerState 복제 도착 후 role delegate와 mapping을 다시 연결
void APragmataPlayerController::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	BindPlayerStateRole();
	ApplyGameplayInputMode();
	ApplyRoleInputAndCamera();
	EnsureGameplayHUD();
	EnsureHughCrosshair();
}

void APragmataPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	UpdateScanPulse();
	UpdateCleanseAcquisitionVicinityIndicators();
}

// pickup은 scan 결과에 넣지 않고, 모든 local client가 공유 Hugh 위치에서 object-owned icon만 갱신
void APragmataPlayerController::UpdateCleanseAcquisitionVicinityIndicators()
{
	if (!IsLocalController() || !GetWorld())
	{
		return;
	}

	const AHughCharacter* HughCharacter = ResolveSharedHughCharacter();
	if (!HughCharacter)
	{
		return;
	}

	constexpr float CleanseAcquisitionVicinityDistance = 2000.f;
	for (TActorIterator<ACleanseSkillAcquisitionProp> It(GetWorld()); It; ++It)
	{
		ACleanseSkillAcquisitionProp* AcquisitionProp = *It;
		if (!AcquisitionProp)
		{
			continue;
		}

		const float Distance = FVector::Distance(HughCharacter->GetActorLocation(), AcquisitionProp->GetActorLocation());
		AcquisitionProp->SetVicinityIndicatorVisible(Distance <= CleanseAcquisitionVicinityDistance, Distance);
	}
}

// local UI가 마지막 물리 입력 장치에 맞춰 key prompt를 고를 수 있게 유지
bool APragmataPlayerController::InputKey(const FInputKeyEventArgs& Params)
{
	UpdateInputDeviceFromKeyEvent(Params);
	if (bGameClearPresentationActive && Params.Event == IE_Pressed)
	{
		RequestReturnToTitleFromGameClear();
		return true;
	}
#if !UE_BUILD_SHIPPING
	// UI 연출 검증용 임시 재화 입력
	if (Params.Event == IE_Pressed && Params.Key == EKeys::Zero)
	{
		DebugAddLunafilamentForUITest(50);
		return true;
	}
	// 9키 hold 동안만 방향 입력을 사망 검증용 피해로 소비
	if (Params.Key == EKeys::Nine)
	{
		bDebugDamageDirectionModifierHeld = Params.Event != IE_Released;
		return true;
	}
	if (bDebugDamageDirectionModifierHeld && Params.Event == IE_Pressed)
	{
		EPragmataHitDirection HitDirection = EPragmataHitDirection::Front;
		if (TryGetDebugDamageDirection(Params.Key, HitDirection))
		{
			DebugDealPlayerDamageForDeathTest(1000.f, HitDirection);
			return true;
		}
	}
#endif
	return Super::InputKey(Params);
}

// PlayerState 복제 전에는 cached role을 사용해 spawn 직후 입력 적용을 유지
EPragmataPlayerRole APragmataPlayerController::GetAssignedRole() const
{
	if (const APragmataPlayerState* PragmataPlayerState = GetPlayerState<APragmataPlayerState>())
	{
		return PragmataPlayerState->GetAssignedRole();
	}

	return CachedAssignedRole;
}

// swap 준비용 조작 role을 우선 읽고 아직 없으면 담당 role로 fallback
EPragmataPlayerRole APragmataPlayerController::GetActiveControlRole() const
{
	if (const APragmataPlayerState* PragmataPlayerState = GetPlayerState<APragmataPlayerState>())
	{
		const EPragmataPlayerRole ActiveControlRole = PragmataPlayerState->GetActiveControlRole();
		if (ActiveControlRole == EPragmataPlayerRole::Hugh || ActiveControlRole == EPragmataPlayerRole::Diana)
		{
			return ActiveControlRole;
		}
	}

	if (CachedActiveControlRole == EPragmataPlayerRole::Hugh || CachedActiveControlRole == EPragmataPlayerRole::Diana)
	{
		return CachedActiveControlRole;
	}

	return GetAssignedRole();
}

// 서버가 role을 확정하면 ActiveControlRole 기본값도 같은 값으로 맞춤
void APragmataPlayerController::SetAssignedRole(EPragmataPlayerRole NewRole)
{
	CachedAssignedRole = NewRole;
	if (CachedActiveControlRole == EPragmataPlayerRole::None)
	{
		CachedActiveControlRole = NewRole;
	}

	if (HasAuthority())
	{
		if (APragmataPlayerState* PragmataPlayerState = GetPlayerState<APragmataPlayerState>())
		{
			PragmataPlayerState->SetAssignedRole(NewRole);
		}
	}

	ApplyRoleInputAndCamera();
	EnsureGameplayHUD();
}

// local HUD 소유 controller가 crosshair 표시 상태를 한 곳에서 결정
void APragmataPlayerController::SetCrosshairVisibility(bool bIsVisible)
{
	if (!IsLocalController())
	{
		return;
	}

	EnsureHughCrosshair();
	if (!HughCrosshairWidget)
	{
		return;
	}

	HughCrosshairWidget->SetVisibility(bIsVisible ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
}

void APragmataPlayerController::ShowObjectiveAlert(const FText& ObjectiveText, float DisplaySeconds)
{
	// 목표 상태 변경 판단은 상위 게임 로직이 담당하고, controller는 owning local HUD 표시만 보장
	if (!IsLocalController())
	{
		return;
	}

	EnsureGameplayHUD();
	if (GameplayHUD)
	{
		GameplayHUD->ShowObjectiveAlert(ObjectiveText, DisplaySeconds);
	}
}

void APragmataPlayerController::Client_ShowObjectiveAlert_Implementation(const FText& ObjectiveText, float DisplaySeconds)
{
	ShowObjectiveAlert(ObjectiveText, DisplaySeconds);
}

void APragmataPlayerController::PlayDialogueLine(const FDialogueLine& Line, float DisplaySeconds)
{
	// 대사 진행 판정은 상위 게임 로직이 담당하고, controller는 owning local HUD 표시만 보장
	if (!IsLocalController())
	{
		return;
	}

	EnsureGameplayHUD();
	if (GameplayHUD)
	{
		GameplayHUD->PlayDialogueLine(Line, DisplaySeconds);
	}
}

void APragmataPlayerController::HideDialogue()
{
	if (!IsLocalController())
	{
		return;
	}

	if (GameplayHUD)
	{
		GameplayHUD->HideDialogue();
	}
}

// GameState clear 복제를 받은 local peer만 조작을 멈추고 공통 ending widget을 표시
void APragmataPlayerController::ShowGameClearPresentation()
{
	if (!IsLocalController() || bGameClearPresentationActive)
	{
		return;
	}

	bGameClearPresentationActive = true;
	bGameClearInputReady = false;
	SetIgnoreMoveInput(true);
	SetIgnoreLookInput(true);
	SetCrosshairVisibility(false);
	CreateGameClearWidget();

	if (GameClearWidget)
	{
		FInputModeGameAndUI InputMode;
		InputMode.SetWidgetToFocus(GameClearWidget->TakeWidget());
		InputMode.SetHideCursorDuringCapture(false);
		SetInputMode(InputMode);
		bShowMouseCursor = true;
	}

	constexpr float FadeSeconds = 0.6f;
	if (PlayerCameraManager)
	{
		PlayerCameraManager->StartCameraFade(0.f, 1.f, FadeSeconds, FLinearColor::Black, false, true);
	}
	GetWorldTimerManager().SetTimer(
		GameClearPresentationTimerHandle,
		this,
		&ThisClass::FinishGameClearPresentationFade,
		FadeSeconds,
		false);
}

// black fade가 화면을 덮은 뒤 illustration을 드러내고 그 다음 입력만 Title 복귀 요청으로 허용
void APragmataPlayerController::FinishGameClearPresentationFade()
{
	if (!bGameClearPresentationActive || bReturningToTitleFromGameClear)
	{
		return;
	}

	constexpr float FadeSeconds = 0.8f;
	if (GameClearWidget)
	{
		GameClearWidget->BeginGameClearPresentation(FadeSeconds);
	}
	if (PlayerCameraManager)
	{
		PlayerCameraManager->StartCameraFade(1.f, 0.f, FadeSeconds, FLinearColor::Black, false, true);
	}

	GetWorldTimerManager().SetTimer(
		GameClearPresentationTimerHandle,
		FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			bGameClearInputReady = true;
		}),
		FadeSeconds,
		false);
}

// Clear widget class는 GameFlow config가 소유해 UI asset 교체와 cook ownership을 한 곳에서 관리
void APragmataPlayerController::CreateGameClearWidget()
{
	if (GameClearWidget)
	{
		return;
	}

	const UPragmataGameFlowConfig* GameFlowConfig = UPragmataGameFlowConfig::Get(this);
	const TSoftClassPtr<UPragmataGameClearWidget> WidgetClass = GameFlowConfig
		? GameFlowConfig->GetGameClearWidgetClass()
		: TSoftClassPtr<UPragmataGameClearWidget>();
	if (WidgetClass.IsNull())
	{
		UE_LOG(LogTemp, Warning, TEXT("[PragmataPlayerController::CreateGameClearWidget] Missing game clear widget class"));
		return;
	}

	const TSubclassOf<UPragmataGameClearWidget> LoadedWidgetClass = WidgetClass.LoadSynchronous();
	if (!LoadedWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PragmataPlayerController::CreateGameClearWidget] Failed to load game clear widget class: %s"), *WidgetClass.ToSoftObjectPath().ToString());
		return;
	}

	GameClearWidget = CreateWidget<UPragmataGameClearWidget>(this, LoadedWidgetClass);
	if (!GameClearWidget)
	{
		UE_LOG(LogTemp, Error, TEXT("[PragmataPlayerController::CreateGameClearWidget] Failed to create game clear widget"));
		return;
	}

	GameClearWidget->AddToViewport(2000);
}

// 최초 clear 화면이 완전히 열린 뒤의 아무 입력이나 server에 공통 Title 복귀를 요청
void APragmataPlayerController::RequestReturnToTitleFromGameClear()
{
	if (!IsLocalController() || !bGameClearPresentationActive || !bGameClearInputReady || bReturningToTitleFromGameClear)
	{
		return;
	}

	bReturningToTitleFromGameClear = true;
	constexpr float FadeSeconds = 0.3f;
	if (GameClearWidget)
	{
		GameClearWidget->BeginGameClearExit(FadeSeconds);
	}
	if (PlayerCameraManager)
	{
		PlayerCameraManager->StartCameraFade(0.f, 1.f, FadeSeconds, FLinearColor::Black, false, true);
	}

	GetWorldTimerManager().SetTimer(
		GameClearReturnTimerHandle,
		this,
		&ThisClass::Server_RequestReturnToTitleFromGameClear,
		FadeSeconds,
		false);
}

// server clear 상태가 남아 있는 경우에만 첫 요청을 모든 peer의 로컬 Title 복귀로 fan-out
void APragmataPlayerController::Server_RequestReturnToTitleFromGameClear_Implementation()
{
	APragmataGameState* PragmataGameState = GetWorld() ? GetWorld()->GetGameState<APragmataGameState>() : nullptr;
	if (PragmataGameState && PragmataGameState->IsGameClearPresentationActive())
	{
		PragmataGameState->Multicast_ReturnPlayersToTitle();
	}
}

// 각 peer는 자신이 소유한 LAN session을 정리한 뒤 독립적으로 Title map을 열어 다음 game 시작을 초기 상태로 만듦
void APragmataPlayerController::ReturnToTitleFromGameClear()
{
	if (!IsLocalController())
	{
		return;
	}

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UPragmataMultiplayerSessionSubsystem* SessionSubsystem = GameInstance->GetSubsystem<UPragmataMultiplayerSessionSubsystem>())
		{
			SessionSubsystem->DestroyPragmataSession();
		}
	}

	const UPragmataGameFlowConfig* GameFlowConfig = UPragmataGameFlowConfig::Get(this);
	const FString TitleMapPath = GameFlowConfig ? GameFlowConfig->GetTitleMapPath() : FString();
	if (TitleMapPath.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[PragmataPlayerController::ReturnToTitleFromGameClear] Title map is empty"));
		return;
	}

	UGameplayStatics::OpenLevel(this, FName(*TitleMapPath));
}

void APragmataPlayerController::Client_PlayDialogueLine_Implementation(const FDialogueLine& Line, float DisplaySeconds)
{
	PlayDialogueLine(Line, DisplaySeconds);
}

void APragmataPlayerController::Client_HideDialogue_Implementation()
{
	HideDialogue();
}

void APragmataPlayerController::Client_ShowPOIScanResults_Implementation(const TArray<FPragmataScanResult>& ScanResults, float ScanDuration)
{
	// server snapshot은 actor-owned icon을 거리 순서대로 reveal
	if (!IsLocalController())
	{
		return;
	}

	if (ScanResults.Num() == 0)
	{
		UKismetSystemLibrary::PrintString(this, TEXT("No scan POI found"), true, true, FLinearColor(0.28f, 0.95f, 1.f), 2.f);
	}
	HideActiveScanIcons();
	ActiveScanIconDisplayDuration = FMath::Max(ScanDuration, 0.f);
	for (TPair<TObjectPtr<AActor>, FTimerHandle>& Pair : ScanIconRevealTimers)
	{
		GetWorldTimerManager().ClearTimer(Pair.Value);
	}
	ScanIconRevealTimers.Reset();

	for (const FPragmataScanResult& ScanResult : ScanResults)
	{
		if (!ScanResult.TargetActor)
		{
			continue;
		}

		FTimerHandle& RevealTimer = ScanIconRevealTimers.FindOrAdd(ScanResult.TargetActor);
		GetWorldTimerManager().SetTimer(RevealTimer, FTimerDelegate::CreateWeakLambda(this, [this, ScanResult]()
		{
			ScanIconRevealTimers.Remove(ScanResult.TargetActor);
			ShowScanIconForResult(ScanResult);
		}), FMath::Max(0.f, ScanResult.RevealDelay), false);
	}
}

// scan 대상의 전용 widget만 점등하고 마지막 reveal 기준 공통 수명주기를 시작
void APragmataPlayerController::ShowScanIconForResult(const FPragmataScanResult& ScanResult)
{
	AActor* TargetActor = ScanResult.TargetActor;
	if (!TargetActor)
	{
		return;
	}

	TInlineComponentArray<UWidgetComponent*> WidgetComponents(TargetActor);
	for (UWidgetComponent* WidgetComponent : WidgetComponents)
	{
		if (!WidgetComponent)
		{
			continue;
		}

		WidgetComponent->InitWidget();
		UScanIconWidget* ScanIconWidget = Cast<UScanIconWidget>(WidgetComponent->GetUserWidgetObject());
		if (!ScanIconWidget)
		{
			continue;
		}

		ScanIconWidget->SetDistanceText(FText::FromString(FString::Printf(TEXT("%.0fm"), ScanResult.Distance / 100.f)));
		ScanIconWidget->SetScanIconVisible(true);
		ActiveScanIconActors.AddUnique(TargetActor);
		break;
	}

	if (UWorld* World = GetWorld())
	{
		// 전체 icon lifecycle은 마지막 wave reveal 뒤 ScanDuration(기본 5초)로 정리한다.
		World->GetTimerManager().SetTimer(ScanIconHideTimer, this, &ThisClass::HideActiveScanIcons, ActiveScanIconDisplayDuration, false);
	}
}

// 현재 scan lifecycle이 점등한 object icon만 함께 소등
void APragmataPlayerController::HideActiveScanIcons()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ScanIconHideTimer);
	}

	for (const TWeakObjectPtr<AActor>& ActorPtr : ActiveScanIconActors)
	{
		TInlineComponentArray<UWidgetComponent*> WidgetComponents(ActorPtr.Get());
		for (UWidgetComponent* WidgetComponent : WidgetComponents)
		{
			if (UScanIconWidget* ScanIconWidget = WidgetComponent ? Cast<UScanIconWidget>(WidgetComponent->GetUserWidgetObject()) : nullptr)
			{
				ScanIconWidget->SetScanIconVisible(false);
			}
		}
	}
	ActiveScanIconActors.Reset();
}

void APragmataPlayerController::Client_PlayDianaScanPulse_Implementation(FVector ScanOrigin, float PulseRadius, float PulseDuration)
{
	// scan 판정과 분리된 local-only fullscreen pulse
	if (!IsLocalController())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World || PulseDuration <= 0.f)
	{
		return;
	}

	EnsureScanPulseVolume();
	if (!ScanPulseVolume)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PragmataPlayerController::Client_PlayDianaScanPulse] Failed to create scan pulse post-process volume"));
		return;
	}

	ScanPulseVolume->SetActorLocation(ScanOrigin);
	ScanPulseVolume->BlendRadius = PulseRadius;
	ScanPulseVolume->Settings.WeightedBlendables.Array.Reset();
	ScanPulseVolume->BlendWeight = 0.f;
	ScanPulseVolume->bEnabled = true;

	ScanPulseStartTime = World->GetTimeSeconds();
	ScanPulseDuration = PulseDuration;
	bScanPulseActive = true;
	UpdateScanPulse();
}

void APragmataPlayerController::Client_SetCheckpointUpgradeMenuOpen_Implementation(bool bOpen, bool bInteractive)
{
	SetCheckpointUpgradeMenuOpen(bOpen, bInteractive);
}

void APragmataPlayerController::SetCheckpointUpgradeMenuOpen(bool bOpen, bool bInteractive)
{
	if (!IsLocalController())
	{
		return;
	}

	if (!bOpen)
	{
		if (CheckpointUpgradeMenu)
		{
			CheckpointUpgradeMenu->RemoveFromParent();
			CheckpointUpgradeMenu = nullptr;
		}
		ApplyGameplayInputMode();
		return;
	}

	UClass* MenuClass = UpgradeMenuWidgetClass.LoadSynchronous();
	if (!MenuClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PragmataPlayerController::SetCheckpointUpgradeMenuOpen] Missing upgrade menu class"));
		return;
	}

	if (!CheckpointUpgradeMenu)
	{
		CheckpointUpgradeMenu = CreateWidget<UPragmataUpgradeMenuWidget>(this, MenuClass);
	}
	if (!CheckpointUpgradeMenu)
	{
		return;
	}

	CheckpointUpgradeMenu->SetReadOnly(!bInteractive);
	CheckpointUpgradeMenu->AddToViewport();
	if (bInteractive)
	{
		CheckpointUpgradeMenu->OpenForTest();
	}
}

// UI 입력은 owning controller RPC로 보내고 서버가 shared Hugh stat source만 수정
void APragmataPlayerController::RequestUpgrade(EPragmataUpgradeType UpgradeType)
{
	if (HasAuthority())
	{
		ApplyUpgradeRequest(UpgradeType, false);
		return;
	}

	Server_RequestUpgrade(UpgradeType);
}

void APragmataPlayerController::RequestCloseCheckpointUpgradeMenu()
{
	Server_RequestCloseCheckpointUpgradeMenu();
}

void APragmataPlayerController::Server_RequestCloseCheckpointUpgradeMenu_Implementation()
{
	const APragmataPlayerState* PragmataPlayerState = GetPlayerState<APragmataPlayerState>();
	if (!PragmataPlayerState || PragmataPlayerState->GetAssignedRole() != EPragmataPlayerRole::Hugh)
	{
		return;
	}

	if (APragmataGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<APragmataGameMode>() : nullptr)
	{
		GameMode->CloseCheckpointUpgradeSession();
	}
}

// 환급도 강화와 같은 서버 검증 경로를 사용
void APragmataPlayerController::RequestRefundUpgrade(EPragmataUpgradeType UpgradeType)
{
	if (HasAuthority())
	{
		ApplyUpgradeRequest(UpgradeType, true);
		return;
	}

	Server_RequestRefundUpgrade(UpgradeType);
}

void APragmataPlayerController::RequestCheckpointUpgradeSelection(EPragmataUpgradeType UpgradeType)
{
	if (HasAuthority())
	{
		Server_RequestCheckpointUpgradeSelection_Implementation(UpgradeType);
		return;
	}
	Server_RequestCheckpointUpgradeSelection(UpgradeType);
}

void APragmataPlayerController::RequestCheckpointUpgradeHold(float Progress, bool bRefund)
{
	Server_RequestCheckpointUpgradeHold(FMath::Clamp(Progress, 0.f, 1.f), bRefund);
}

// PlayerState delegate 중복 binding을 막고 role 변경을 local cache와 연결
void APragmataPlayerController::BindPlayerStateRole()
{
	APragmataPlayerState* PragmataPlayerState = GetPlayerState<APragmataPlayerState>();
	if (BoundPragmataPlayerState == PragmataPlayerState)
	{
		return;
	}

	if (BoundPragmataPlayerState)
	{
		BoundPragmataPlayerState->OnAssignedRoleChanged.RemoveAll(this);
		BoundPragmataPlayerState->OnActiveControlRoleChanged.RemoveAll(this);
	}

	BoundPragmataPlayerState = PragmataPlayerState;
	if (BoundPragmataPlayerState)
	{
		BoundPragmataPlayerState->OnAssignedRoleChanged.AddUObject(this, &ThisClass::HandleAssignedRoleChanged);
		BoundPragmataPlayerState->OnActiveControlRoleChanged.AddUObject(this, &ThisClass::HandleActiveControlRoleChanged);
		CachedAssignedRole = BoundPragmataPlayerState->GetAssignedRole();
		CachedActiveControlRole = BoundPragmataPlayerState->GetActiveControlRole();
	}
}

// 담당 role 변경은 swap이 없을 때 조작 role의 기본값도 함께 갱신
void APragmataPlayerController::HandleAssignedRoleChanged(EPragmataPlayerRole NewRole)
{
	CachedAssignedRole = NewRole;
	if (CachedActiveControlRole == EPragmataPlayerRole::None)
	{
		CachedActiveControlRole = NewRole;
	}
	ApplyRoleInputAndCamera();
	EnsureGameplayHUD();
	BindGameplayHackState();
}

// GameInput/XInput 등 Unreal gamepad key로 들어온 입력은 공통 device state로 취급
void APragmataPlayerController::UpdateInputDeviceFromKeyEvent(const FInputKeyEventArgs& Params)
{
	if (!IsLocalController() || Params.bIsTouchEvent || Params.IsSimulatedInput())
	{
		return;
	}

	if (!IsMeaningfulInputDeviceEvent(Params))
	{
		return;
	}

	bIsGamepadInput = Params.IsGamepad();
}

// local gameplay 화면마다 AssignedRole에 맞는 HUD를 한 번만 생성
void APragmataPlayerController::EnsureGameplayHUD()
{
	if (!IsLocalController())
	{
		return;
	}

	if (!GameplayHUD)
	{
		TSoftClassPtr<UPragmataHUDWidget> SelectedHUDClass;
		if (!TryGetGameplayHUDClassForAssignedRole(SelectedHUDClass))
		{
			return;
		}

		if (SelectedHUDClass.IsNull())
		{
			UE_LOG(LogTemp, Warning, TEXT("[PragmataPlayerController::EnsureGameplayHUD] Missing gameplay HUD class reference"));
			return;
		}

		TSubclassOf<UPragmataHUDWidget> LoadedHUDClass = SelectedHUDClass.LoadSynchronous();
		if (!LoadedHUDClass)
		{
			UE_LOG(LogTemp, Warning, TEXT("[PragmataPlayerController::EnsureGameplayHUD] Failed to load gameplay HUD class: %s"), *SelectedHUDClass.ToSoftObjectPath().ToString());
			return;
		}

		GameplayHUD = CreateWidget<UPragmataHUDWidget>(this, LoadedHUDClass);
		if (!GameplayHUD)
		{
			UE_LOG(LogTemp, Error, TEXT("[PragmataPlayerController::EnsureGameplayHUD] Failed to create gameplay HUD widget: %s"), *SelectedHUDClass.ToSoftObjectPath().ToString());
			return;
		}

		GameplayHUD->AddToViewport();
	}

	BindGameplayHackState();
}

void APragmataPlayerController::BindGameplayHackState()
{
	// local HUD가 GameState의 복제 대상과 공유 stat source를 구독
	if (!IsLocalController() || !GameplayHUD)
	{
		return;
	}

	APragmataGameState* PragmataGameState = GetWorld() ? GetWorld()->GetGameState<APragmataGameState>() : nullptr;
	if (BoundPragmataGameState == PragmataGameState)
	{
		RebindSharedHUDStats();
		return;
	}

	if (BoundPragmataGameState)
	{
		BoundPragmataGameState->OnActiveEnemyHackTargetChanged.RemoveAll(this);
		BoundPragmataGameState->OnActiveMapPuzzleActorChanged.RemoveAll(this);
		BoundPragmataGameState->OnActiveBossTargetChanged.RemoveAll(this);
		BoundPragmataGameState->OnEnemyCombatActiveChanged.RemoveAll(this);
		BoundPragmataGameState->OnHughCharacterChanged.RemoveAll(this);
		BoundPragmataGameState->OnDianaOperatorPawnChanged.RemoveAll(this);
	}

	BoundPragmataGameState = PragmataGameState;
	if (BoundPragmataGameState)
	{
		BoundPragmataGameState->OnActiveEnemyHackTargetChanged.AddUObject(this, &ThisClass::HandleActiveEnemyHackTargetChanged);
		BoundPragmataGameState->OnActiveMapPuzzleActorChanged.AddUObject(this, &ThisClass::HandleActiveMapPuzzleActorChanged);
		BoundPragmataGameState->OnActiveBossTargetChanged.AddUObject(this, &ThisClass::HandleActiveBossTargetChanged);
		BoundPragmataGameState->OnEnemyCombatActiveChanged.AddUObject(this, &ThisClass::HandleEnemyCombatActiveChanged);
		BoundPragmataGameState->OnHughCharacterChanged.AddUObject(this, &ThisClass::HandleHughCharacterChanged);
		BoundPragmataGameState->OnDianaOperatorPawnChanged.AddUObject(this, &ThisClass::HandleDianaOperatorPawnChanged);
		HandleActiveEnemyHackTargetChanged(BoundPragmataGameState->GetActiveEnemyHackTarget());
		HandleActiveMapPuzzleActorChanged(BoundPragmataGameState->GetActiveMapPuzzleActor());
		HandleActiveBossTargetChanged(BoundPragmataGameState->GetActiveBossTarget());
		HandleEnemyCombatActiveChanged(BoundPragmataGameState->IsEnemyCombatActive());
	}

	RebindSharedHUDStats();
}

void APragmataPlayerController::HandleActiveEnemyHackTargetChanged(AActor* TargetActor)
{
	if (GameplayHUD)
	{
		GameplayHUD->HandleActiveEnemyHackTargetChanged(TargetActor);
	}
}

void APragmataPlayerController::HandleActiveMapPuzzleActorChanged(AActor* PuzzleActor)
{
	if (GameplayHUD)
	{
		GameplayHUD->HandleActiveMapPuzzleActorChanged(PuzzleActor);
	}
}

void APragmataPlayerController::HandleActiveBossTargetChanged(AActor* BossTarget)
{
	if (GameplayHUD)
	{
		GameplayHUD->HandleActiveBossTargetChanged(BossTarget);
	}
}

void APragmataPlayerController::HandleEnemyCombatActiveChanged(bool bNewActive)
{
	if (GameplayHUD)
	{
		GameplayHUD->OnEnemyCombatActiveChanged(bNewActive);
	}
}

void APragmataPlayerController::HandleHughCharacterChanged(AHughCharacter* /*HughCharacter*/)
{
	RebindSharedHUDStats();
}

void APragmataPlayerController::HandleDianaOperatorPawnChanged(ADianaOperatorPawn* /*DianaPawn*/)
{
	RebindSharedHUDStats();
}

void APragmataPlayerController::RebindSharedHUDStats()
{
	// role pawn pointer가 늦게 복제되어도 양쪽 HUD가 같은 gameplay source를 다시 구독
	if (!IsLocalController() || !GameplayHUD)
	{
		return;
	}

	AHughCharacter* HughCharacter = BoundPragmataGameState ? BoundPragmataGameState->GetHughCharacter() : nullptr;
	ADianaOperatorPawn* DianaPawn = BoundPragmataGameState ? BoundPragmataGameState->GetDianaOperatorPawn() : nullptr;
	GameplayHUD->BindSharedHUDStats(HughCharacter, DianaPawn);
}

bool APragmataPlayerController::ApplyUpgradeRequest(EPragmataUpgradeType UpgradeType, bool bRefund)
{
	// checkpoint upgrade session의 Hugh controller만 shared stat을 바꿀 수 있음
	if (!HasAuthority())
	{
		return false;
	}
	const APragmataPlayerState* PragmataPlayerState = GetPlayerState<APragmataPlayerState>();
	const APragmataGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<APragmataGameMode>() : nullptr;
	if (!PragmataPlayerState || PragmataPlayerState->GetAssignedRole() != EPragmataPlayerRole::Hugh
		|| !GameMode || !GameMode->IsCheckpointUpgradeSessionActive())
	{
		return false;
	}

	AHughCharacter* HughCharacter = ResolveSharedHughCharacter();
	UPragmataPlayerStatsComponent* PlayerStatsComponent = HughCharacter ? HughCharacter->GetPlayerStatsComponent() : nullptr;
	if (!PlayerStatsComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PragmataPlayerController::ApplyUpgradeRequest] Missing Hugh player stats component"));
		return false;
	}

	return bRefund
		? PlayerStatsComponent->TryRefundUpgrade(UpgradeType)
		: PlayerStatsComponent->TryUpgrade(UpgradeType);
}

AHughCharacter* APragmataPlayerController::ResolveSharedHughCharacter() const
{
	if (const APragmataGameState* PragmataGameState = GetWorld() ? GetWorld()->GetGameState<APragmataGameState>() : nullptr)
	{
		if (AHughCharacter* HughCharacter = PragmataGameState->GetHughCharacter())
		{
			return HughCharacter;
		}
	}

	return Cast<AHughCharacter>(GetPawn());
}

void APragmataPlayerController::EnsureScanPulseVolume()
{
	if (ScanPulseVolume || !IsLocalController())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	SpawnParameters.ObjectFlags |= RF_Transient;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ScanPulseVolume = World->SpawnActor<APostProcessVolume>(APostProcessVolume::StaticClass(), FTransform::Identity, SpawnParameters);
	if (!ScanPulseVolume)
	{
		return;
	}

	ScanPulseVolume->bUnbound = true;
	ScanPulseVolume->bEnabled = false;
	ScanPulseVolume->BlendWeight = 0.f;
	ScanPulseVolume->Priority = 500.f;
	ScanPulseVolume->SetActorHiddenInGame(true);
}

void APragmataPlayerController::UpdateScanPulse()
{
	if (!bScanPulseActive || !ScanPulseVolume)
	{
		return;
	}

	const UWorld* World = GetWorld();
	if (!World || ScanPulseDuration <= 0.f)
	{
		StopScanPulse();
		return;
	}

	const float Progress = FMath::Clamp((World->GetTimeSeconds() - ScanPulseStartTime) / ScanPulseDuration, 0.f, 1.f);
	if (Progress >= 1.f)
	{
		StopScanPulse();
		return;
	}

	const float PulseWeight = FMath::Sin(Progress * UE_PI);
	FPostProcessSettings& PulseSettings = ScanPulseVolume->Settings;
	PulseSettings.bOverride_ColorSaturation = true;
	PulseSettings.bOverride_ColorContrast = true;
	PulseSettings.bOverride_ColorGamma = true;
	PulseSettings.bOverride_ColorGain = true;
	PulseSettings.bOverride_ColorOffset = true;
	PulseSettings.bOverride_BlueCorrection = true;
	PulseSettings.bOverride_ExpandGamut = true;
	PulseSettings.bOverride_ToneCurveAmount = true;
	PulseSettings.bOverride_BloomIntensity = true;
	PulseSettings.bOverride_VignetteIntensity = true;
	PulseSettings.bOverride_SceneFringeIntensity = true;

	PulseSettings.ColorSaturation = FVector4(
		FMath::Lerp(0.2f, 0.02f, PulseWeight),
		FMath::Lerp(0.45f, 0.08f, PulseWeight),
		FMath::Lerp(1.4f, 2.6f, PulseWeight),
		1.f
	);
	PulseSettings.ColorContrast = FVector4(0.85f, 0.95f, 1.35f + 0.55f * PulseWeight, 1.f);
	PulseSettings.ColorGamma = FVector4(1.65f + 0.4f * PulseWeight, 1.3f + 0.2f * PulseWeight, 0.55f - 0.15f * PulseWeight, 1.f);
	PulseSettings.ColorGain = FVector4(0.04f, 0.16f, 2.4f + 1.6f * PulseWeight, 1.f);
	PulseSettings.ColorOffset = FVector4(-0.18f * PulseWeight, -0.08f * PulseWeight, 0.62f * PulseWeight, 0.f);
	PulseSettings.BlueCorrection = 1.f;
	PulseSettings.ExpandGamut = 1.f;
	PulseSettings.ToneCurveAmount = 0.35f;
	PulseSettings.BloomIntensity = 0.25f + 2.4f * PulseWeight;
	PulseSettings.VignetteIntensity = 0.18f * PulseWeight;
	PulseSettings.SceneFringeIntensity = 0.04f * PulseWeight;
	ScanPulseVolume->BlendWeight = PulseWeight * 0.5f;
}

void APragmataPlayerController::StopScanPulse()
{
	bScanPulseActive = false;
	ScanPulseDuration = 0.f;
	if (ScanPulseVolume)
	{
		ScanPulseVolume->BlendWeight = 0.f;
		ScanPulseVolume->bEnabled = false;
		ScanPulseVolume->Settings = FPostProcessSettings();
	}
}

// UI 연출 검증용 임시 입력만 서버 재화 source로 전달
void APragmataPlayerController::DebugAddLunafilamentForUITest(int32 Amount)
{
#if !UE_BUILD_SHIPPING
	if (!IsLocalController() || Amount <= 0)
	{
		return;
	}

	if (!HasAuthority())
	{
		Server_DebugAddLunafilamentForUITest(Amount);
		return;
	}

	ApplyDebugAddLunafilamentForUITest(Amount);
#endif
}

// 임시 재화 입력도 server RPC에서 같은 권위 경로로 처리
void APragmataPlayerController::Server_DebugAddLunafilamentForUITest_Implementation(int32 Amount)
{
#if !UE_BUILD_SHIPPING
	ApplyDebugAddLunafilamentForUITest(Amount);
#endif
}

// 실제 검증 값은 Hugh PlayerStatsComponent만 변경
void APragmataPlayerController::ApplyDebugAddLunafilamentForUITest(int32 Amount)
{
#if !UE_BUILD_SHIPPING
	if (!HasAuthority() || Amount <= 0)
	{
		return;
	}

	APragmataGameState* PragmataGameState = BoundPragmataGameState;
	if (!PragmataGameState)
	{
		PragmataGameState = GetWorld() ? GetWorld()->GetGameState<APragmataGameState>() : nullptr;
	}

	AHughCharacter* HughCharacter = PragmataGameState ? PragmataGameState->GetHughCharacter() : nullptr;
	UPragmataPlayerStatsComponent* PlayerStatsComponent = HughCharacter ? HughCharacter->GetPlayerStatsComponent() : nullptr;
	if (!PlayerStatsComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PragmataPlayerController::ApplyDebugAddLunafilamentForUITest] Missing Hugh PlayerStatsComponent"));
		return;
	}

	if (PlayerStatsComponent->AddLunafilament(Amount))
	{
		UE_LOG(LogTemp, Log, TEXT("[PragmataPlayerController::ApplyDebugAddLunafilamentForUITest] Added Lunafilament: %d"), Amount);
	}
#endif
}

// player death 검증용 임시 입력만 서버 damage 경로로 전달
void APragmataPlayerController::DebugDealPlayerDamageForDeathTest(float DamageAmount, EPragmataHitDirection HitDirection)
{
#if !UE_BUILD_SHIPPING
	if (!IsLocalController() || DamageAmount <= 0.f)
	{
		return;
	}

	if (!HasAuthority())
	{
		Server_DebugDealPlayerDamageForDeathTest(DamageAmount, HitDirection);
		return;
	}

	ApplyDebugDealPlayerDamageForDeathTest(DamageAmount, HitDirection);
#endif
}

// 임시 피해 입력도 server RPC에서 shared health로 전달
void APragmataPlayerController::Server_DebugDealPlayerDamageForDeathTest_Implementation(float DamageAmount, EPragmataHitDirection HitDirection)
{
#if !UE_BUILD_SHIPPING
	ApplyDebugDealPlayerDamageForDeathTest(DamageAmount, HitDirection);
#endif
}

// Hugh shared health에 기존 ReceiveDamage/OnDeath 경로를 그대로 태움
void APragmataPlayerController::ApplyDebugDealPlayerDamageForDeathTest(float DamageAmount, EPragmataHitDirection HitDirection)
{
#if !UE_BUILD_SHIPPING
	if (!HasAuthority() || DamageAmount <= 0.f)
	{
		return;
	}

	AHughCharacter* HughCharacter = ResolveSharedHughCharacter();
	if (!HughCharacter)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PragmataPlayerController::ApplyDebugDealPlayerDamageForDeathTest] Missing HughCharacter"));
		return;
	}

	FPragmataDamageSpec DamageSpec;
	DamageSpec.DamageAmount = DamageAmount;
	DamageSpec.SourceActor = GetPawn();
	DamageSpec.DamageCauser = GetPawn();
	DamageSpec.InstigatorController = this;
	DamageSpec.DamageKind = EPragmataDamageKind::Generic;
	DamageSpec.HitReactStrength = EPragmataHitReactStrength::Large;
	DamageSpec.HitDirection = HitDirection;
	DamageSpec.bHasImpact = false;

	if (!IPragmataDamageable::Execute_ReceiveDamage(HughCharacter, DamageSpec))
	{
		UE_LOG(LogTemp, Warning, TEXT("[PragmataPlayerController::ApplyDebugDealPlayerDamageForDeathTest] Damage failed Amount=%.2f Hugh=%s"), DamageAmount, *GetNameSafe(HughCharacter));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[PragmataPlayerController::ApplyDebugDealPlayerDamageForDeathTest] Applied debug player damage: %.2f"), DamageAmount);
#endif
}

// role pawn BP defaults에서 HUD class를 읽어 UI 교체를 code 변경 없이 처리
bool APragmataPlayerController::TryGetGameplayHUDClassForAssignedRole(TSoftClassPtr<UPragmataHUDWidget>& OutHUDClass) const
{
	OutHUDClass.Reset();
	const APawn* ControlledPawn = GetPawn();
	const EPragmataPlayerRole AssignedRole = GetAssignedRole();
	if (AssignedRole == EPragmataPlayerRole::Hugh)
	{
		const AHughCharacter* HughCharacter = Cast<AHughCharacter>(ControlledPawn);
		if (!HughCharacter)
		{
			return false;
		}

		OutHUDClass = HughCharacter->GetGameplayHUDClass();
		return true;
	}
	if (AssignedRole == EPragmataPlayerRole::Diana)
	{
		const ADianaOperatorPawn* DianaPawn = Cast<ADianaOperatorPawn>(ControlledPawn);
		if (!DianaPawn)
		{
			return false;
		}

		OutHUDClass = DianaPawn->GetGameplayHUDClass();
		return true;
	}

	return false;
}

// crosshair widget은 local viewport마다 한 번만 만들고 표시 상태는 aim callback이 결정
void APragmataPlayerController::EnsureHughCrosshair()
{
	if (!IsLocalController())
	{
		return;
	}

	if (HughCrosshairWidget)
	{
		return;
	}

	if (HughCrosshairWidgetClass.IsNull())
	{
		UE_LOG(LogTemp, Warning, TEXT("[PragmataPlayerController::EnsureHughCrosshair] Missing Hugh crosshair class reference"));
		return;
	}

	TSubclassOf<UUserWidget> LoadedCrosshairClass = HughCrosshairWidgetClass.LoadSynchronous();
	if (!LoadedCrosshairClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PragmataPlayerController::EnsureHughCrosshair] Failed to load Hugh crosshair class: %s"), *HughCrosshairWidgetClass.ToSoftObjectPath().ToString());
		return;
	}

	HughCrosshairWidget = CreateWidget<UUserWidget>(this, LoadedCrosshairClass);
	if (!HughCrosshairWidget)
	{
		UE_LOG(LogTemp, Error, TEXT("[PragmataPlayerController::EnsureHughCrosshair] Failed to create Hugh crosshair widget: %s"), *HughCrosshairWidgetClass.ToSoftObjectPath().ToString());
		return;
	}

	HughCrosshairWidget->AddToViewport();
	HughCrosshairWidget->SetVisibility(ESlateVisibility::Hidden);
}

// swap 중에는 possession 대신 mapping/camera 기준만 바꿀 수 있게 연결
void APragmataPlayerController::HandleActiveControlRoleChanged(EPragmataPlayerRole NewRole)
{
	CachedActiveControlRole = NewRole;
	ApplyRoleInputAndCamera();
}

// Title/Lobby의 UI input mode를 gameplay 진입 시 mouse look 전용 상태로 되돌림
void APragmataPlayerController::ApplyGameplayInputMode()
{
	if (!IsLocalController())
	{
		return;
	}

	FInputModeGameOnly InputMode;
	InputMode.SetConsumeCaptureMouseDown(false);
	SetInputMode(InputMode);

	bShowMouseCursor = false;
	SetIgnoreLookInput(false);
	SetIgnoreMoveInput(false);
}

// 현재 조작 role 기준으로 input mapping과 camera 제한을 적용
void APragmataPlayerController::ApplyRoleInputAndCamera()
{
	if (!IsLocalController())
	{
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	if (!Subsystem)
	{
		return;
	}

	if (AppliedMappingContext)
	{
		Subsystem->RemoveMappingContext(AppliedMappingContext);
		AppliedMappingContext = nullptr;
	}

	UInputMappingContext* MappingContext = nullptr;
	const EPragmataPlayerRole ActiveControlRole = GetActiveControlRole();
	if (ActiveControlRole == EPragmataPlayerRole::Hugh)
	{
		MappingContext = LoadInputMappingContext(HughInputMappingContext);

		if (PlayerCameraManager)
		{
			PlayerCameraManager->ViewPitchMin = -ViewPitchLimit;
			PlayerCameraManager->ViewPitchMax = ViewPitchLimit;
		}
	}
	else if (ActiveControlRole == EPragmataPlayerRole::Diana)
	{
		MappingContext = LoadInputMappingContext(DianaInputMappingContext);

		if (PlayerCameraManager)
		{
			PlayerCameraManager->ViewPitchMin = -89.f;
			PlayerCameraManager->ViewPitchMax = 89.f;
		}
	}

	if (MappingContext)
	{
		Subsystem->AddMappingContext(MappingContext, 0);
		AppliedMappingContext = MappingContext;
	}
}

// soft reference 누락 시 role 적용을 중단하고 로그로만 남김
UInputMappingContext* APragmataPlayerController::LoadInputMappingContext(const TSoftObjectPtr<UInputMappingContext>& MappingContext) const
{
	if (MappingContext.IsNull())
	{
		return nullptr;
	}

	UInputMappingContext* LoadedMappingContext = MappingContext.LoadSynchronous();
	if (!LoadedMappingContext)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PragmataPlayerController::LoadInputMappingContext] Missing input mapping context: %s"), *MappingContext.ToSoftObjectPath().ToString());
	}

	return LoadedMappingContext;
}

void APragmataPlayerController::Server_RequestUpgrade_Implementation(EPragmataUpgradeType UpgradeType)
{
	ApplyUpgradeRequest(UpgradeType, false);
}

void APragmataPlayerController::Server_RequestRefundUpgrade_Implementation(EPragmataUpgradeType UpgradeType)
{
	ApplyUpgradeRequest(UpgradeType, true);
}

void APragmataPlayerController::Server_RequestCheckpointUpgradeSelection_Implementation(EPragmataUpgradeType UpgradeType)
{
	// Hugh만 열린 checkpoint session의 mirror selection을 변경
	const APragmataPlayerState* PragmataPlayerState = GetPlayerState<APragmataPlayerState>();
	APragmataGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<APragmataGameMode>() : nullptr;
	APragmataGameState* GameState = GetWorld() ? GetWorld()->GetGameState<APragmataGameState>() : nullptr;
	if (!PragmataPlayerState || PragmataPlayerState->GetAssignedRole() != EPragmataPlayerRole::Hugh
		|| !GameMode || !GameMode->IsCheckpointUpgradeSessionActive() || !GameState)
	{
		return;
	}
	if (UpgradeType != EPragmataUpgradeType::Suit && UpgradeType != EPragmataUpgradeType::Weapon)
	{
		return;
	}
	GameState->SetCheckpointUpgradeSelection(UpgradeType);
}

void APragmataPlayerController::Server_RequestCheckpointUpgradeHold_Implementation(float Progress, bool bRefund)
{
	const APragmataPlayerState* PragmataPlayerState = GetPlayerState<APragmataPlayerState>();
	APragmataGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<APragmataGameMode>() : nullptr;
	if (!PragmataPlayerState || PragmataPlayerState->GetAssignedRole() != EPragmataPlayerRole::Hugh || !GameMode || !GameMode->IsCheckpointUpgradeSessionActive())
	{
		return;
	}
	if (APragmataGameState* GameState = GetWorld()->GetGameState<APragmataGameState>())
	{
		GameState->SetCheckpointUpgradeHold(Progress, bRefund);
	}
}
