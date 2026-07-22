#include "Game/PragmataLobbyPlayerController.h"

#include "Blueprint/UserWidget.h"
#include "Camera/CameraActor.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/InputComponent.h"
#include "Data/LobbyCharacterSkinSet.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/PragmataLobbyGameMode.h"
#include "Game/PragmataLobbyGameState.h"
#include "Game/PragmataPlayerState.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "Lobby/LobbyPresentationDirector.h"

// lobby 전용 widget/input asset을 기본값으로 지정해 BP 설정 누락에도 선택 화면이 동작
APragmataLobbyPlayerController::APragmataLobbyPlayerController()
{
	PrimaryActorTick.bCanEverTick = true;
	LobbyWidgetClass = TSoftClassPtr<UUserWidget>(FSoftClassPath(TEXT("/Game/UI/Widget/WBP_RoleLobby.WBP_RoleLobby_C")));
	LobbyInputMappingContext = TSoftObjectPtr<UInputMappingContext>(FSoftObjectPath(TEXT("/Game/Blueprints/Player/Input/Common/IMC_Lobby.IMC_Lobby")));
	LobbySelectAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath(TEXT("/Game/Blueprints/Player/Input/Common/IA_LobbySelect.IA_LobbySelect")));
	LobbySkinSelectAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath(TEXT("/Game/Blueprints/Player/Input/Common/IA_LobbySkinSelect.IA_LobbySkinSelect")));
	LobbyConfirmAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath(TEXT("/Game/Blueprints/Player/Input/Common/IA_LobbyConfirm.IA_LobbyConfirm")));
	LobbySkinSet = TSoftObjectPtr<ULobbyCharacterSkinSet>(FSoftObjectPath(TEXT("/Game/Data/DA_LobbyCharacterSkinSet.DA_LobbyCharacterSkinSet")));
	bShowMouseCursor = true;
}

// lobby key 배치는 IMC_Lobby가 소유하고 C++은 action만 처리
void APragmataLobbyPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (!InputComponent)
	{
		return;
	}

	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent))
	{
		if (UInputAction* SelectAction = LoadLobbyInputAction(LobbySelectAction))
		{
			EnhancedInputComponent->BindAction(SelectAction, ETriggerEvent::Triggered, this, &ThisClass::HandleLobbySelectAction);
		}

		if (UInputAction* SkinSelectAction = LoadLobbyInputAction(LobbySkinSelectAction))
		{
			EnhancedInputComponent->BindAction(SkinSelectAction, ETriggerEvent::Triggered, this, &ThisClass::HandleLobbySkinSelectAction);
		}

		if (UInputAction* ConfirmAction = LoadLobbyInputAction(LobbyConfirmAction))
		{
			EnhancedInputComponent->BindAction(ConfirmAction, ETriggerEvent::Started, this, &ThisClass::HandleConfirmPressed);
		}
	}
}

// PlayerState가 늦게 복제되는 클라이언트에서 lobby 예측 정정 delegate를 다시 연결
void APragmataLobbyPlayerController::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	BindLobbyPlayerState();
}

// 선택 입력은 소유 클라이언트에 즉시 표시하고 서버에는 확정 요청만 보냄
void APragmataLobbyPlayerController::RequestLobbySelectionStep(int32 Direction)
{
	if (Direction == 0)
	{
		return;
	}

	if (!IsCharacterSelectionInputAllowed())
	{
		return;
	}

	if (IsLocalController())
	{
		AdvanceLocalDisplayedRole(Direction);
	}

	ServerRequestSelectionStep(Direction);
}

// skin 입력도 local prediction 후 서버 검증 요청으로만 확정
void APragmataLobbyPlayerController::RequestLobbySkinSelectionStep(int32 Direction)
{
	if (Direction == 0)
	{
		return;
	}

	if (!IsCharacterSelectionInputAllowed())
	{
		return;
	}

	if (IsLocalController())
	{
		AdvanceLocalDisplayedSkin(Direction);
	}

	ServerRequestSkinSelectionStep(Direction);
}

// ready/start 입력은 현재 lobby phase와 host 여부를 서버 권한 함수로 위임
void APragmataLobbyPlayerController::RequestLobbyConfirm()
{
	const APragmataLobbyGameState* LobbyGameState = GetWorld() ? GetWorld()->GetGameState<APragmataLobbyGameState>() : nullptr;
	if (!LobbyGameState)
	{
		return;
	}

	const EPragmataLobbyPhase LobbyPhase = LobbyGameState->GetLobbyPhase();
	if (LobbyPhase == EPragmataLobbyPhase::ReadyToStart && IsHostLobbyPlayer())
	{
		ServerRequestStartGame();
		return;
	}

	if (LobbyPhase == EPragmataLobbyPhase::CharacterSelect || LobbyPhase == EPragmataLobbyPhase::ReadyToStart)
	{
		ServerRequestReady();
	}
}

// 1P lobby index를 host 권한 UI와 start game 조건으로 사용
bool APragmataLobbyPlayerController::IsHostLobbyPlayer() const
{
	const APragmataPlayerState* PragmataPlayerState = GetPlayerState<APragmataPlayerState>();
	return PragmataPlayerState && PragmataPlayerState->GetLobbyPlayerIndex() == 1;
}

// local UI는 예측 선택값을 먼저 읽고, 원격 플레이어는 PlayerState 확정값만 읽음
EPragmataPlayerRole APragmataLobbyPlayerController::GetDisplayedLobbySelectedRole() const
{
	if (bHasPredictedLobbySelection)
	{
		return PredictedLobbySelectedRole;
	}

	const APragmataPlayerState* PragmataPlayerState = GetPlayerState<APragmataPlayerState>();
	if (PragmataPlayerState && PragmataPlayerState->GetLobbySelectedRole() != EPragmataPlayerRole::None)
	{
		return PragmataPlayerState->GetLobbySelectedRole();
	}

	return EPragmataPlayerRole::None;
}

// local UI는 skin prediction을 먼저 읽고, 확정 전에는 role 기본값으로 복구
FName APragmataLobbyPlayerController::GetDisplayedLobbySelectedSkinId() const
{
	if (bHasPredictedLobbySkinSelection)
	{
		return PredictedLobbySelectedSkinId;
	}

	const APragmataPlayerState* PragmataPlayerState = GetPlayerState<APragmataPlayerState>();
	if (PragmataPlayerState && PragmataPlayerState->GetLobbySelectedSkinId() != NAME_None)
	{
		return PragmataPlayerState->GetLobbySelectedSkinId();
	}

	return ResolveDefaultSkinForRole(GetDisplayedLobbySelectedRole());
}

bool APragmataLobbyPlayerController::CanHandleCharacterSelectionInput() const
{
	return IsCharacterSelectionInputAllowed();
}

// 로컬 lobby 화면에 필요한 input, camera, widget, PlayerState binding을 구성
void APragmataLobbyPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalController() || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	ApplyLobbyInputMappingContext();
	BindLobbyPlayerState();
	SetupLobbyCameraView();
	ShowLobbyWidget();

	if (PlayerCameraManager)
	{
		PlayerCameraManager->StartCameraFade(1.0f, 0.0f, 0.65f, FLinearColor::Black, false, true);
	}
}

void APragmataLobbyPlayerController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	RefreshLobbyPresentationAvailability();
}

// lobby를 나갈 때 input mapping과 PlayerState delegate를 누수 없이 제거
void APragmataLobbyPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (BoundLobbyPlayerState)
	{
		BoundLobbyPlayerState->OnLobbyStateChanged.RemoveAll(this);
		BoundLobbyPlayerState = nullptr;
	}
	for (const TWeakObjectPtr<APragmataPlayerState>& ObservedPlayerState : ObservedLobbyPlayerStates)
	{
		if (APragmataPlayerState* PragmataPlayerState = ObservedPlayerState.Get())
		{
			PragmataPlayerState->OnLobbyStateChanged.RemoveAll(this);
		}
	}
	ObservedLobbyPlayerStates.Reset();

	RemoveLobbyInputMappingContext();

	Super::EndPlay(EndPlayReason);
}

// lobby widget은 로컬 컨트롤러에만 생성해 split/listen server 입력을 분리
void APragmataLobbyPlayerController::ShowLobbyWidget()
{
	if (LobbyWidget)
	{
		return;
	}

	const TSubclassOf<UUserWidget> WidgetClass = LobbyWidgetClass.LoadSynchronous();
	if (!WidgetClass)
	{
		UE_LOG(LogTemp, Error, TEXT("[PragmataLobbyPlayerController::ShowLobbyWidget] Missing lobby widget class"));
		return;
	}

	// 로컬 진입 맵에서만 초대 코드 UI 표시
	LobbyWidget = CreateWidget<UUserWidget>(this, WidgetClass);
	if (!LobbyWidget)
	{
		UE_LOG(LogTemp, Error, TEXT("[PragmataLobbyPlayerController::ShowLobbyWidget] Failed to create lobby widget"));
		return;
	}

	LobbyWidget->AddToViewport();

	FInputModeGameAndUI InputMode;
	InputMode.SetWidgetToFocus(LobbyWidget->TakeWidget());
	InputMode.SetHideCursorDuringCapture(false);
	SetInputMode(InputMode);
	bShowMouseCursor = true;
}

// lobby 전용 mapping context를 높은 priority로 올려 character input과 충돌 방지
void APragmataLobbyPlayerController::ApplyLobbyInputMappingContext()
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

	RemoveLobbyInputMappingContext();

	UInputMappingContext* MappingContext = LoadLobbyInputMappingContext();
	if (!MappingContext)
	{
		return;
	}

	Subsystem->AddMappingContext(MappingContext, 10);
	AppliedLobbyInputMappingContext = MappingContext;
}

// travel 또는 controller 종료 시 lobby mapping이 gameplay 입력에 남지 않게 정리
void APragmataLobbyPlayerController::RemoveLobbyInputMappingContext()
{
	if (!AppliedLobbyInputMappingContext || !IsLocalController())
	{
		AppliedLobbyInputMappingContext = nullptr;
		return;
	}

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		Subsystem->RemoveMappingContext(AppliedLobbyInputMappingContext);
	}

	AppliedLobbyInputMappingContext = nullptr;
}

// asset 누락을 로그로 남기고 lobby 입력 적용을 중단
UInputMappingContext* APragmataLobbyPlayerController::LoadLobbyInputMappingContext() const
{
	if (LobbyInputMappingContext.IsNull())
	{
		return nullptr;
	}

	UInputMappingContext* MappingContext = LobbyInputMappingContext.LoadSynchronous();
	if (!MappingContext)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PragmataLobbyPlayerController::LoadLobbyInputMappingContext] Missing input mapping context: %s"), *LobbyInputMappingContext.ToSoftObjectPath().ToString());
	}

	return MappingContext;
}

// Enhanced Input action soft reference를 runtime에만 확인
UInputAction* APragmataLobbyPlayerController::LoadLobbyInputAction(const TSoftObjectPtr<UInputAction>& InputAction) const
{
	if (InputAction.IsNull())
	{
		return nullptr;
	}

	UInputAction* LoadedAction = InputAction.LoadSynchronous();
	if (!LoadedAction)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PragmataLobbyPlayerController::LoadLobbyInputAction] Missing input action: %s"), *InputAction.ToSoftObjectPath().ToString());
	}

	return LoadedAction;
}

// dedicated lobby asset이 없을 때도 preview를 볼 수 있는 fallback camera 생성
void APragmataLobbyPlayerController::SetupLobbyCameraView()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	LobbyPresentationDirector = FindLobbyPresentationDirector();
	if (!LobbyPresentationDirector)
	{
		LobbyPresentationDirector = World->SpawnActor<ALobbyPresentationDirector>(ALobbyPresentationDirector::StaticClass(), FTransform::Identity);
	}
	if (LobbyPresentationDirector)
	{
		SetViewTarget(LobbyPresentationDirector);
		RefreshLobbyPresentationAvailability();
		return;
	}

	ACameraActor* CameraActor = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FallbackCameraTransform);
	if (CameraActor)
	{
		SetViewTarget(CameraActor);
	}
}

ALobbyPresentationDirector* APragmataLobbyPlayerController::FindLobbyPresentationDirector() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	for (TActorIterator<ALobbyPresentationDirector> It(World); It; ++It)
	{
		return *It;
	}

	return nullptr;
}

// local prediction과 서버 확정 선택을 같은 presentation 경로로 전달
void APragmataLobbyPlayerController::UpdateLobbyPresentationFocus(int32 Direction)
{
	if (!IsLocalController())
	{
		return;
	}

	if (!LobbyPresentationDirector)
	{
		LobbyPresentationDirector = FindLobbyPresentationDirector();
	}
	if (!LobbyPresentationDirector)
	{
		return;
	}

	if (!IsCharacterSelectionInputAllowed())
	{
		LobbyPresentationDirector->SetPreviewPresentationEnabled(false);
		return;
	}

	LobbyPresentationDirector->SetPreviewPresentationEnabled(true);
	RefreshLobbyPresentationSkins();
	const EPragmataPlayerRole DisplayedRole = GetDisplayedLobbySelectedRole();
	if (DisplayedRole == EPragmataPlayerRole::Hugh || DisplayedRole == EPragmataPlayerRole::Diana)
	{
		LobbyPresentationDirector->SetPreviewSkin(DisplayedRole, GetDisplayedLobbySelectedSkinId());
		LobbyPresentationDirector->FocusRole(DisplayedRole, Direction);
	}
}

// 각 client가 복제된 PlayerState와 자기 prediction을 합쳐 role별 preview skin을 맞춤
void APragmataLobbyPlayerController::RefreshLobbyPresentationSkins()
{
	if (!LobbyPresentationDirector)
	{
		return;
	}

	const APragmataLobbyGameState* LobbyGameState = GetWorld() ? GetWorld()->GetGameState<APragmataLobbyGameState>() : nullptr;
	if (!LobbyGameState)
	{
		return;
	}

	for (APlayerState* ObservedBasePlayerState : LobbyGameState->PlayerArray)
	{
		const APragmataPlayerState* PragmataPlayerState = Cast<APragmataPlayerState>(ObservedBasePlayerState);
		if (!PragmataPlayerState)
		{
			continue;
		}

		const bool bIsOwningPlayerState = PragmataPlayerState == GetPlayerState<APragmataPlayerState>();
		const EPragmataPlayerRole PlayerRole = bIsOwningPlayerState ? GetDisplayedLobbySelectedRole() : PragmataPlayerState->GetLobbySelectedRole();
		const FName SkinId = bIsOwningPlayerState ? GetDisplayedLobbySelectedSkinId() : PragmataPlayerState->GetLobbySelectedSkinId();
		if ((PlayerRole == EPragmataPlayerRole::Hugh || PlayerRole == EPragmataPlayerRole::Diana) && SkinId != NAME_None)
		{
			LobbyPresentationDirector->SetPreviewSkin(PlayerRole, SkinId);
		}
	}
}

void APragmataLobbyPlayerController::RefreshLobbyPresentationAvailability()
{
	if (!IsLocalController())
	{
		return;
	}

	RebindObservedLobbyPlayerStates();
	UpdateLobbyPresentationFocus(0);
}

// confirm 입력을 현재 lobby phase에 맞는 ready/start 요청으로 변환
void APragmataLobbyPlayerController::HandleConfirmPressed()
{
	RequestLobbyConfirm();
}

// Enhanced Input axis 값을 analog stick 선택 처리로 전달
void APragmataLobbyPlayerController::HandleLobbySelectAction(const FInputActionValue& Value)
{
	HandleLeftStickHorizontal(Value.Get<float>());
}

void APragmataLobbyPlayerController::HandleLobbySkinSelectAction(const FInputActionValue& Value)
{
	HandleLeftStickVertical(Value.Get<float>());
}

// analog stick은 반복 입력 간격을 제한해 서버 선택 RPC가 과도하게 쌓이지 않게 함
void APragmataLobbyPlayerController::HandleLeftStickHorizontal(float Value)
{
	if (FMath::Abs(Value) < 0.65f)
	{
		return;
	}

	UWorld* World = GetWorld();
	const float CurrentTime = World ? World->GetTimeSeconds() : 0.0f;
	if (CurrentTime < NextLeftStickStepTime)
	{
		return;
	}

	NextLeftStickStepTime = CurrentTime + 0.25f;
	RequestLobbySelectionStep(Value < 0.0f ? -1 : 1);
}

// gamepad left stick Y는 skin 순환만 처리하며 role 입력과 같은 반복 간격을 사용
void APragmataLobbyPlayerController::HandleLeftStickVertical(float Value)
{
	if (FMath::Abs(Value) < 0.65f)
	{
		return;
	}

	UWorld* World = GetWorld();
	const float CurrentTime = World ? World->GetTimeSeconds() : 0.0f;
	if (CurrentTime < NextLeftStickSkinStepTime)
	{
		return;
	}

	NextLeftStickSkinStepTime = CurrentTime + 0.25f;
	RequestLobbySkinSelectionStep(Value < 0.0f ? -1 : 1);
}

// 선택과 ready phase에서만 role 변경 입력을 받아 서버 상태와 UI를 일치
bool APragmataLobbyPlayerController::IsCharacterSelectionInputAllowed() const
{
	const APragmataLobbyGameState* LobbyGameState = GetWorld() ? GetWorld()->GetGameState<APragmataLobbyGameState>() : nullptr;
	if (!LobbyGameState)
	{
		return false;
	}

	const EPragmataLobbyPhase LobbyPhase = LobbyGameState->GetLobbyPhase();
	return LobbyGameState->HasEnoughPlayersForSelection()
		&& (LobbyPhase == EPragmataLobbyPhase::CharacterSelect || LobbyPhase == EPragmataLobbyPhase::ReadyToStart);
}

// 내 화면의 선택 preview만 즉시 바꾸고 서버 확정은 PlayerState 복제로 정정
void APragmataLobbyPlayerController::AdvanceLocalDisplayedRole(int32 Direction)
{
	const EPragmataPlayerRole CurrentDisplayedRole = GetDisplayedLobbySelectedRole();
	if (CurrentDisplayedRole == EPragmataPlayerRole::Hugh || CurrentDisplayedRole == EPragmataPlayerRole::Diana)
	{
		PredictedLobbySelectedRole = CurrentDisplayedRole == EPragmataPlayerRole::Hugh ? EPragmataPlayerRole::Diana : EPragmataPlayerRole::Hugh;
	}
	else
	{
		PredictedLobbySelectedRole = Direction < 0 ? EPragmataPlayerRole::Diana : EPragmataPlayerRole::Hugh;
	}

	bHasPredictedLobbySelection = true;
	bHasPredictedLobbySkinSelection = true;
	PredictedLobbySelectedSkinId = ResolveDefaultSkinForRole(PredictedLobbySelectedRole);
	UpdateLobbyPresentationFocus(Direction);
}

// 현재 role 안에서만 skin preview를 바꾸고 Diana 단일 skin은 같은 id로 유지
void APragmataLobbyPlayerController::AdvanceLocalDisplayedSkin(int32 Direction)
{
	const EPragmataPlayerRole DisplayedRole = GetDisplayedLobbySelectedRole();
	PredictedLobbySelectedSkinId = ResolveAdjacentSkinForRole(DisplayedRole, GetDisplayedLobbySelectedSkinId(), Direction);
	bHasPredictedLobbySkinSelection = PredictedLobbySelectedSkinId != NAME_None;
	UpdateLobbyPresentationFocus(0);
}

FName APragmataLobbyPlayerController::ResolveDefaultSkinForRole(EPragmataPlayerRole PlayerRole) const
{
	if (ULobbyCharacterSkinSet* SkinSet = LoadLobbySkinSet())
	{
		return SkinSet->ResolveDefaultSkinId(PlayerRole);
	}

	return NAME_None;
}

FName APragmataLobbyPlayerController::ResolveAdjacentSkinForRole(EPragmataPlayerRole PlayerRole, FName CurrentSkinId, int32 Direction) const
{
	if (ULobbyCharacterSkinSet* SkinSet = LoadLobbySkinSet())
	{
		return SkinSet->GetAdjacentSkinId(PlayerRole, CurrentSkinId, Direction);
	}

	return NAME_None;
}

ULobbyCharacterSkinSet* APragmataLobbyPlayerController::LoadLobbySkinSet() const
{
	if (LoadedLobbySkinSet)
	{
		return LoadedLobbySkinSet;
	}

	APragmataLobbyPlayerController* MutableThis = const_cast<APragmataLobbyPlayerController*>(this);
	if (!LobbySkinSet.IsNull())
	{
		MutableThis->LoadedLobbySkinSet = LobbySkinSet.LoadSynchronous();
	}

	if (!LoadedLobbySkinSet)
	{
		MutableThis->LoadedLobbySkinSet = NewObject<ULobbyCharacterSkinSet>(GetTransientPackage(), TEXT("ClientRuntimeLobbyCharacterSkinSet"));
	}

	return LoadedLobbySkinSet;
}

// PlayerState OnRep를 구독해 서버 확정 lobby state로 예측 표시를 정정
void APragmataLobbyPlayerController::BindLobbyPlayerState()
{
	APragmataPlayerState* PragmataPlayerState = GetPlayerState<APragmataPlayerState>();
	if (BoundLobbyPlayerState == PragmataPlayerState)
	{
		return;
	}

	if (BoundLobbyPlayerState)
	{
		BoundLobbyPlayerState->OnLobbyStateChanged.RemoveAll(this);
	}

	BoundLobbyPlayerState = PragmataPlayerState;
	if (BoundLobbyPlayerState)
	{
		BoundLobbyPlayerState->OnLobbyStateChanged.AddUObject(this, &ThisClass::HandleLobbyStateChanged);
		HandleLobbyStateChanged();
		UpdateLobbyPresentationFocus(0);
	}
}

// remote PlayerState 변경까지 preview에 바로 반영하도록 lobby PlayerState를 모두 관찰
void APragmataLobbyPlayerController::RebindObservedLobbyPlayerStates()
{
	const APragmataLobbyGameState* LobbyGameState = GetWorld() ? GetWorld()->GetGameState<APragmataLobbyGameState>() : nullptr;
	if (!LobbyGameState)
	{
		return;
	}

	TArray<TWeakObjectPtr<APragmataPlayerState>> NextObservedPlayerStates;
	for (APlayerState* ObservedBasePlayerState : LobbyGameState->PlayerArray)
	{
		if (APragmataPlayerState* PragmataPlayerState = Cast<APragmataPlayerState>(ObservedBasePlayerState))
		{
			NextObservedPlayerStates.Add(PragmataPlayerState);
		}
	}

	if (NextObservedPlayerStates.Num() == ObservedLobbyPlayerStates.Num())
	{
		bool bSameObservedSet = true;
		for (int32 Index = 0; Index < NextObservedPlayerStates.Num(); ++Index)
		{
			bSameObservedSet &= NextObservedPlayerStates[Index] == ObservedLobbyPlayerStates[Index];
		}
		if (bSameObservedSet)
		{
			return;
		}
	}

	for (const TWeakObjectPtr<APragmataPlayerState>& ObservedPlayerState : ObservedLobbyPlayerStates)
	{
		if (APragmataPlayerState* PragmataPlayerState = ObservedPlayerState.Get())
		{
			PragmataPlayerState->OnLobbyStateChanged.RemoveAll(this);
		}
	}

	ObservedLobbyPlayerStates = MoveTemp(NextObservedPlayerStates);
	for (const TWeakObjectPtr<APragmataPlayerState>& ObservedPlayerState : ObservedLobbyPlayerStates)
	{
		if (APragmataPlayerState* PragmataPlayerState = ObservedPlayerState.Get())
		{
			PragmataPlayerState->OnLobbyStateChanged.AddUObject(this, &ThisClass::HandleLobbyStateChanged);
		}
	}
}

// 서버가 선택값을 확정하면 local prediction overlay를 제거하고 preview skin을 재동기화
void APragmataLobbyPlayerController::HandleLobbyStateChanged()
{
	if (!BoundLobbyPlayerState)
	{
		return;
	}

	const EPragmataPlayerRole ConfirmedRole = BoundLobbyPlayerState->GetLobbySelectedRole();
	if ((bHasPredictedLobbySelection || bHasPredictedLobbySkinSelection)
		&& (ConfirmedRole == EPragmataPlayerRole::Hugh || ConfirmedRole == EPragmataPlayerRole::Diana))
	{
		// 서버 확정값 도착 후 예측값 정리
		bHasPredictedLobbySelection = false;
		PredictedLobbySelectedRole = EPragmataPlayerRole::None;
		bHasPredictedLobbySkinSelection = false;
		PredictedLobbySelectedSkinId = NAME_None;
	}
	UpdateLobbyPresentationFocus(0);
}

// 선택 충돌 처리는 서버 lobby game mode의 단일 권한 경로에 맡김
void APragmataLobbyPlayerController::ServerRequestSelectionStep_Implementation(int32 Direction)
{
	if (APragmataLobbyGameMode* LobbyGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<APragmataLobbyGameMode>() : nullptr)
	{
		LobbyGameMode->RequestSelectionStep(this, Direction);
	}
}

// skin 선택 충돌과 유효성 검사는 서버 lobby game mode의 단일 권한 경로에 맡김
void APragmataLobbyPlayerController::ServerRequestSkinSelectionStep_Implementation(int32 Direction)
{
	if (APragmataLobbyGameMode* LobbyGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<APragmataLobbyGameMode>() : nullptr)
	{
		LobbyGameMode->RequestSkinSelectionStep(this, Direction);
	}
}

// ready 상태는 PlayerState 복제값으로 모든 lobby UI에 전파
void APragmataLobbyPlayerController::ServerRequestReady_Implementation()
{
	if (APragmataLobbyGameMode* LobbyGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<APragmataLobbyGameMode>() : nullptr)
	{
		LobbyGameMode->RequestReady(this);
	}
}

// host start 요청은 서버가 handoff role을 저장한 뒤 gameplay map으로 이동
void APragmataLobbyPlayerController::ServerRequestStartGame_Implementation()
{
	if (APragmataLobbyGameMode* LobbyGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<APragmataLobbyGameMode>() : nullptr)
	{
		LobbyGameMode->RequestStartGame(this);
	}
}
