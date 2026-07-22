#include "Game/PragmataTitlePlayerController.h"

#include "Blueprint/UserWidget.h"
#include "Camera/CameraActor.h"
#include "Camera/PlayerCameraManager.h"
#include "Data/PragmataGameFlowConfig.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "UI/PragmataTitleScreenWidget.h"

// title 전용 입력과 UI asset 기본값 지정
APragmataTitlePlayerController::APragmataTitlePlayerController()
{
	TitleWidgetClass = TSoftClassPtr<UPragmataTitleScreenWidget>(FSoftClassPath(TEXT("/Game/UI/Widget/WBP_TitleScreen.WBP_TitleScreen_C")));
	TitleInputMappingContext = TSoftObjectPtr<UInputMappingContext>(FSoftObjectPath(TEXT("/Game/Blueprints/Player/Input/Common/IMC_Title.IMC_Title")));
	TitleStartAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath(TEXT("/Game/Blueprints/Player/Input/Common/IA_TitleStart.IA_TitleStart")));
	bShowMouseCursor = true;
}

// title key 배치는 IMC_Title이 소유하고 C++은 start action만 처리
void APragmataTitlePlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (!InputComponent)
	{
		return;
	}

	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent))
	{
		if (UInputAction* StartAction = LoadTitleInputAction())
		{
			EnhancedInputComponent->BindAction(StartAction, ETriggerEvent::Started, this, &ThisClass::HandleStartPressed);
		}
	}
}

// title map 진입 시 UI, input, camera fade-in 상태 구성
void APragmataTitlePlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalController() || GetNetMode() == NM_DedicatedServer || !IsTitleMapWorld())
	{
		return;
	}

	ApplyTitleInputMappingContext();
	SetupTitleCameraView();
	ShowTitleWidget();

	if (PlayerCameraManager)
	{
		PlayerCameraManager->StartCameraFade(1.0f, 0.0f, 0.65f, FLinearColor::Black, false, true);
	}
}

// title map을 벗어날 때 지연 travel과 input mapping을 정리
void APragmataTitlePlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TravelTimerHandle);
	}

	RemoveTitleInputMappingContext();
	Super::EndPlay(EndPlayReason);
}

// 첫 start 입력만 받아 fade-out과 lobby travel timer를 시작
void APragmataTitlePlayerController::RequestStartFromTitle()
{
	if (bTransitionStarted || !IsTitleMapWorld())
	{
		return;
	}

	bTransitionStarted = true;
	if (TitleWidget)
	{
		TitleWidget->BeginTitleExit(FadeToBlackSeconds);
	}

	if (PlayerCameraManager)
	{
		PlayerCameraManager->StartCameraFade(0.0f, 1.0f, FadeToBlackSeconds, FLinearColor::Black, false, true);
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(TravelTimerHandle, this, &ThisClass::TravelToLobby, FadeToBlackSeconds, false);
	}
}

// TitleCamera tag/name/label을 우선 사용하고 없으면 fallback camera 생성
void APragmataTitlePlayerController::SetupTitleCameraView()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (TActorIterator<ACameraActor> CameraIt(World); CameraIt; ++CameraIt)
	{
		ACameraActor* CameraActor = *CameraIt;
		if (!CameraActor)
		{
			continue;
		}

		const bool bMatchesTagOrName = CameraActor->ActorHasTag(TitleCameraTag)
			|| CameraActor->GetFName() == TitleCameraTag
			|| CameraActor->GetName().Contains(TitleCameraTag.ToString());
#if WITH_EDITOR
		const bool bMatchesEditorLabel = CameraActor->GetActorLabel() == TitleCameraTag.ToString();
#else
		const bool bMatchesEditorLabel = false;
#endif
		if (bMatchesTagOrName || bMatchesEditorLabel)
		{
			SetViewTarget(CameraActor);
			return;
		}
	}

	ACameraActor* CameraActor = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FallbackCameraTransform);
	if (CameraActor)
	{
		SetViewTarget(CameraActor);
	}
}

// title widget은 local viewport 전용이며 mouse/gamepad 입력 focus를 함께 받음
void APragmataTitlePlayerController::ShowTitleWidget()
{
	if (TitleWidget)
	{
		return;
	}

	const TSubclassOf<UPragmataTitleScreenWidget> WidgetClass = TitleWidgetClass.LoadSynchronous();
	if (!WidgetClass)
	{
		UE_LOG(LogTemp, Error, TEXT("[PragmataTitlePlayerController::ShowTitleWidget] Missing title widget class"));
		return;
	}

	TitleWidget = CreateWidget<UPragmataTitleScreenWidget>(this, WidgetClass);
	if (!TitleWidget)
	{
		UE_LOG(LogTemp, Error, TEXT("[PragmataTitlePlayerController::ShowTitleWidget] Failed to create title widget"));
		return;
	}

	TitleWidget->AddToViewport();

	FInputModeGameAndUI InputMode;
	InputMode.SetWidgetToFocus(TitleWidget->TakeWidget());
	InputMode.SetHideCursorDuringCapture(false);
	SetInputMode(InputMode);
	bShowMouseCursor = true;
}

// title mapping을 gameplay/lobby mapping보다 높은 priority로 적용
void APragmataTitlePlayerController::ApplyTitleInputMappingContext()
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

	RemoveTitleInputMappingContext();

	UInputMappingContext* MappingContext = LoadTitleInputMappingContext();
	if (!MappingContext)
	{
		return;
	}

	Subsystem->AddMappingContext(MappingContext, 20);
	AppliedTitleInputMappingContext = MappingContext;
}

// lobby로 travel하기 전 title 전용 mapping이 남지 않게 제거
void APragmataTitlePlayerController::RemoveTitleInputMappingContext()
{
	if (!AppliedTitleInputMappingContext || !IsLocalController())
	{
		AppliedTitleInputMappingContext = nullptr;
		return;
	}

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		Subsystem->RemoveMappingContext(AppliedTitleInputMappingContext);
	}

	AppliedTitleInputMappingContext = nullptr;
}

// title input asset 누락 시 start 입력 적용을 중단하고 로그로만 남김
UInputMappingContext* APragmataTitlePlayerController::LoadTitleInputMappingContext() const
{
	if (TitleInputMappingContext.IsNull())
	{
		return nullptr;
	}

	UInputMappingContext* MappingContext = TitleInputMappingContext.LoadSynchronous();
	if (!MappingContext)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PragmataTitlePlayerController::LoadTitleInputMappingContext] Missing input mapping context: %s"), *TitleInputMappingContext.ToSoftObjectPath().ToString());
	}

	return MappingContext;
}

// start action soft reference를 runtime에만 확인
UInputAction* APragmataTitlePlayerController::LoadTitleInputAction() const
{
	if (TitleStartAction.IsNull())
	{
		return nullptr;
	}

	UInputAction* InputAction = TitleStartAction.LoadSynchronous();
	if (!InputAction)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PragmataTitlePlayerController::LoadTitleInputAction] Missing input action: %s"), *TitleStartAction.ToSoftObjectPath().ToString());
	}

	return InputAction;
}

// Title.umap에서 시작한 경우에만 title UI와 lobby travel을 활성화
bool APragmataTitlePlayerController::IsTitleMapWorld() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	FString PackageName = World->GetOutermost() ? World->GetOutermost()->GetName() : World->GetMapName();
	PackageName = UWorld::RemovePIEPrefix(PackageName);
	return PackageName == TEXT("/Game/Map/Title");
}

// 모든 title start 입력을 중복 방지된 단일 transition으로 합침
void APragmataTitlePlayerController::HandleStartPressed()
{
	RequestStartFromTitle();
}

// fade-out이 끝난 뒤 GameFlow의 lobby entrypoint로 이동
void APragmataTitlePlayerController::TravelToLobby()
{
	RemoveTitleInputMappingContext();

	if (!IsTitleMapWorld())
	{
		return;
	}

	const UPragmataGameFlowConfig* GameFlowConfig = UPragmataGameFlowConfig::Get(this);
	if (!GameFlowConfig)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PragmataTitlePlayerController::TravelToLobby] Missing game flow config"));
		return;
	}

	const FString LobbyMapPath = GameFlowConfig->GetLobbyMapPath();
	if (LobbyMapPath.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[PragmataTitlePlayerController::TravelToLobby] Lobby map is empty"));
		return;
	}

	UGameplayStatics::OpenLevel(this, FName(*LobbyMapPath));
}
