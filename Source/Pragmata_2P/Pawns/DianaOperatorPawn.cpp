#include "Pawns/DianaOperatorPawn.h"

#include "Camera/CameraComponent.h"
#include "Characters/HughCharacter.h"
#include "Components/DianaHackComponent.h"
#include "Components/DianaNodeInteractionComponent.h"
#include "Components/DianaCleanseComponent.h"
#include "Components/DianaScanComponent.h"
#include "Components/DianaVoiceComponent.h"
#include "Components/SceneComponent.h"
#include "Components/OverDriveComponent.h"
#include "EnhancedInputComponent.h"
#include "Game/PragmataGameState.h"
#include "Game/PragmataPlayerController.h"
#include "Game/PragmataPlayerState.h"
#include "Game/PragmataTraceUtils.h"
#include "GameFramework/SpringArmComponent.h"
#include "Interfaces/PragmataHackable.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"
#include "Net/UnrealNetwork.h"
#include "UI/PragmataHUDWidget.h"
#include "UObject/SoftObjectPath.h"

// Diana player는 몸체 없이 camera/scan/hack intent만 보내는 operator pawn 사용
ADianaOperatorPawn::ADianaOperatorPawn()
{
	bReplicates = true;
	SetReplicateMovement(false);
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(SceneRoot);
	CameraBoom->TargetArmLength = DefaultArmLength;
	CameraBoom->SocketOffset = FVector(0.f, 80.f, 70.f);
	CameraBoom->bUsePawnControlRotation = false;
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->CameraLagSpeed = 12.f;

	CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraComponent"));
	CameraComponent->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);

	HackComponent = CreateDefaultSubobject<UDianaHackComponent>(TEXT("HackComponent"));
	ScanComponent = CreateDefaultSubobject<UDianaScanComponent>(TEXT("ScanComponent"));
	NodeInteractionComponent = CreateDefaultSubobject<UDianaNodeInteractionComponent>(TEXT("NodeInteractionComponent"));
	CleanseComponent = CreateDefaultSubobject<UDianaCleanseComponent>(TEXT("CleanseComponent"));
	OverDriveComponent = CreateDefaultSubobject<UOverDriveComponent>(TEXT("OverDriveComponent"));
	VoiceComponent = CreateDefaultSubobject<UDianaVoiceComponent>(TEXT("VoiceComponent"));
	GameplayHUDClass = TSoftClassPtr<UPragmataHUDWidget>(FSoftClassPath(TEXT("/Game/UI/Widget/WBP_DianaHUD.WBP_DianaHUD_C")));
}

// Diana camera anchor는 Hugh 위치를 따라가되 movement replication은 쓰지 않음
void ADianaOperatorPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	FollowHughAnchor();
	UpdateAimCamera(DeltaSeconds);
	UpdateEnemyHackAim(DeltaSeconds);
}

// operator mode와 서버가 인정한 view rotation만 복제
void ADianaOperatorPawn::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADianaOperatorPawn, OperatorMode);
	DOREPLIFETIME(ADianaOperatorPawn, bIsAiming);
	DOREPLIFETIME(ADianaOperatorPawn, EnemyHackSuccessSerial);
	DOREPLIFETIME(ADianaOperatorPawn, ServerViewRotation);
}

// Diana 전용 input action은 pawn이 직접 binding해 scan/hack component로 전달
void ADianaOperatorPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	ResolveInputActions();

	if (UEnhancedInputComponent* EnhancedInputComponent = CastChecked<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (LookAction)
		{
			EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ADianaOperatorPawn::OnTriggerLook);
		}
		if (ScanAction)
		{
			EnhancedInputComponent->BindAction(ScanAction, ETriggerEvent::Started, this, &ADianaOperatorPawn::OnStartScan);
		}
		if (InteractAction)
		{
			EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Started, this, &ADianaOperatorPawn::OnStartInteract);
		}
		if (AimAction)
		{
			EnhancedInputComponent->BindAction(AimAction, ETriggerEvent::Started, this, &ADianaOperatorPawn::OnStartAim);
			EnhancedInputComponent->BindAction(AimAction, ETriggerEvent::Completed, this, &ADianaOperatorPawn::OnCompleteAim);
		}
		if (CleanseAction)
		{
			EnhancedInputComponent->BindAction(CleanseAction, ETriggerEvent::Started, this, &ADianaOperatorPawn::OnStartCleanse);
			EnhancedInputComponent->BindAction(CleanseAction, ETriggerEvent::Completed, this, &ADianaOperatorPawn::OnCompleteCleanse);
			EnhancedInputComponent->BindAction(CleanseAction, ETriggerEvent::Canceled, this, &ADianaOperatorPawn::OnCompleteCleanse);
		}
	}

	PlayerInputComponent->BindKey(EKeys::W, IE_Pressed, this, &ADianaOperatorPawn::OnHackDirectionUp);
	PlayerInputComponent->BindKey(EKeys::S, IE_Pressed, this, &ADianaOperatorPawn::OnHackDirectionDown);
	PlayerInputComponent->BindKey(EKeys::A, IE_Pressed, this, &ADianaOperatorPawn::OnHackDirectionLeft);
	PlayerInputComponent->BindKey(EKeys::D, IE_Pressed, this, &ADianaOperatorPawn::OnHackDirectionRight);
	PlayerInputComponent->BindKey(EKeys::Gamepad_FaceButton_Top, IE_Pressed, this, &ADianaOperatorPawn::OnHackDirectionUp);
	PlayerInputComponent->BindKey(EKeys::Gamepad_FaceButton_Bottom, IE_Pressed, this, &ADianaOperatorPawn::OnHackDirectionDown);
	PlayerInputComponent->BindKey(EKeys::Gamepad_FaceButton_Left, IE_Pressed, this, &ADianaOperatorPawn::OnHackDirectionLeft);
	PlayerInputComponent->BindKey(EKeys::Gamepad_FaceButton_Right, IE_Pressed, this, &ADianaOperatorPawn::OnHackDirectionRight);
	PlayerInputComponent->BindKey(EKeys::X, IE_Pressed, this, &ADianaOperatorPawn::RequestOverDrive);
	PlayerInputComponent->BindKey(EKeys::Gamepad_LeftThumbstick, IE_Pressed, this, &ADianaOperatorPawn::OnOverDriveLeftStickPressed);
	PlayerInputComponent->BindKey(EKeys::Gamepad_RightThumbstick, IE_Pressed, this, &ADianaOperatorPawn::OnOverDriveRightStickPressed);
}

// client camera 입력은 즉시 적용하되 pitch는 player 기대 방향에 맞게 반전
void ADianaOperatorPawn::AddCameraInput(FVector2D LookInput)
{
	if (!IsLocallyControlled() || !CanUseDianaActionIntent() || LookInput.IsNearlyZero())
	{
		return;
	}

	FRotator NewRotation = GetActorRotation();
	NewRotation.Yaw += LookInput.X * CameraYawSpeed;
	NewRotation.Pitch -= LookInput.Y * CameraPitchSpeed;

	const FRotator ClampedRotation = ClampViewRotation(NewRotation);
	ApplyViewRotation(ClampedRotation);
	Server_SetViewRotation(ClampedRotation);
}

// scan 요청은 operator mode가 허용할 때만 component 경로로 전달
void ADianaOperatorPawn::RequestScan()
{
	if (CanUseDianaActionIntent() && ScanComponent)
	{
		ScanComponent->RequestScan();
	}
}

void ADianaOperatorPawn::RequestOverDrive()
{
	if (CanUseDianaActionIntent() && OverDriveComponent)
	{
		OverDriveComponent->RequestOverDrive();
	}
}

bool ADianaOperatorPawn::IsHackActionActive() const
{
	// OverDrive 시작 중 기존 enemy/map puzzle 입력 상태를 중첩하지 않음
	return (HackComponent && HackComponent->IsEnemyHackActive())
		|| (NodeInteractionComponent && NodeInteractionComponent->IsMapHackActive());
}

void ADianaOperatorPawn::OnOverDriveLeftStickPressed()
{
	// 좌측 stick press는 반대쪽 입력 window와 조합해 OverDrive 요청
	TryRequestOverDriveFromStick(true);
}

void ADianaOperatorPawn::OnOverDriveRightStickPressed()
{
	// 우측 stick press는 반대쪽 입력 window와 조합해 OverDrive 요청
	TryRequestOverDriveFromStick(false);
}

void ADianaOperatorPawn::TryRequestOverDriveFromStick(bool bLeftStick)
{
	// 동시 입력 강제 대신 짧은 window를 허용해 gamepad 조작 실패를 줄임
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float Now = World->GetTimeSeconds();
	float& ThisPressTime = bLeftStick ? LastOverDriveLeftStickPressTime : LastOverDriveRightStickPressTime;
	const float OtherPressTime = bLeftStick ? LastOverDriveRightStickPressTime : LastOverDriveLeftStickPressTime;
	ThisPressTime = Now;
	if (Now - OtherPressTime <= OverDriveStickPressWindow)
	{
		LastOverDriveLeftStickPressTime = -BIG_NUMBER;
		LastOverDriveRightStickPressTime = -BIG_NUMBER;
		RequestOverDrive();
	}
}

// hack 요청은 client target intent를 component의 서버 검증 경로로 전달
void ADianaOperatorPawn::RequestHack(AActor* TargetActor)
{
	if (CanUseDianaActionIntent() && HackComponent)
	{
		HackComponent->RequestHack(TargetActor);
	}
}

// node interaction의 Roaming/aim/role 검증은 component의 client/server 공통 gate가 담당
void ADianaOperatorPawn::RequestNodeInteraction()
{
	if (CanUseDianaActionIntent() && NodeInteractionComponent)
	{
		NodeInteractionComponent->RequestNodeInteraction();
	}
}

void ADianaOperatorPawn::RequestCleanse()
{
	// 기존 직접 호출은 quick execute와 같은 단순 입력으로 유지
	RequestQuickCleanse();
}

void ADianaOperatorPawn::BeginPrecisionCleanse()
{
	// Aim+Cleanse hold가 시작될 때만 Diana 카메라 기준 preview를 요청
	if (CanUseDianaActionIntent() && CleanseComponent)
	{
		CleanseComponent->BeginPrecisionCleanse();
	}
}

void ADianaOperatorPawn::CommitPrecisionCleanse()
{
	// Cleanse release가 정밀 조준 실행 intent가 됨
	if (CanUseDianaActionIntent() && CleanseComponent)
	{
		CleanseComponent->CommitPrecisionCleanse();
	}
}

void ADianaOperatorPawn::CancelPrecisionCleanse()
{
	// Aim hold가 먼저 끊기면 정밀 조준 preview만 종료
	if (CleanseComponent)
	{
		CleanseComponent->CancelPrecisionCleanse();
	}
}

void ADianaOperatorPawn::RequestQuickCleanse()
{
	// 빠른 실행은 Diana camera 대신 Hugh 조준 방향을 component가 서버에서 해석
	if (CanUseDianaActionIntent() && CleanseComponent)
	{
		CleanseComponent->RequestQuickCleanse();
	}
}

// aim 상태는 즉시 local camera에 반영하고 서버 복제 상태로 확정
void ADianaOperatorPawn::SetAimingState(bool bNewAiming)
{
	if (bIsAiming == bNewAiming)
	{
		return;
	}

	bIsAiming = bNewAiming;
	if (!HasAuthority())
	{
		Server_SetAimingState(bNewAiming);
	}
}

// operator mode는 서버만 바꿔 animation/input 잠금 경계를 유지
void ADianaOperatorPawn::SetOperatorMode(EDianaOperatorMode NewMode)
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[DianaOperatorPawn::SetOperatorMode] Client tried to set operator mode"));
		return;
	}

	OperatorMode = NewMode;
}

void ADianaOperatorPawn::NotifyEnemyHackSucceeded()
{
	// enemy hack 성공 one-shot 신호를 AnimBP가 놓치지 않게 serial로 복제
	if (!HasAuthority())
	{
		return;
	}

	++EnemyHackSuccessSerial;
}

void ADianaOperatorPawn::ShowObjectiveAlert(float DisplaySeconds)
{
	ShowObjectiveAlertText(ObjectiveAlertText, DisplaySeconds);
}

void ADianaOperatorPawn::ShowObjectiveAlertText(const FText& InObjectiveText, float DisplaySeconds)
{
	// operator pawn은 목표 문구 보관과 owning HUD 표시 진입점만 제공하고, 목표 생성 판정은 상위 로직이 담당
	if (InObjectiveText.IsEmptyOrWhitespace())
	{
		return;
	}

	APragmataPlayerController* PragmataController = Cast<APragmataPlayerController>(GetController());
	if (!PragmataController)
	{
		return;
	}

	if (PragmataController->IsLocalController())
	{
		PragmataController->ShowObjectiveAlert(InObjectiveText, DisplaySeconds);
		return;
	}

	if (HasAuthority())
	{
		PragmataController->Client_ShowObjectiveAlert(InObjectiveText, DisplaySeconds);
	}
}

// 서버는 Diana 소유자만 view rotation intent를 갱신할 수 있게 함
void ADianaOperatorPawn::Server_SetViewRotation_Implementation(FRotator NewViewRotation)
{
	if (!IsOwnedByDianaPlayer())
	{
		return;
	}

	// 서버가 Diana 시점 intent를 재계산
	ApplyViewRotation(ClampViewRotation(NewViewRotation));
}

void ADianaOperatorPawn::Server_SetAimingState_Implementation(bool bNewAiming)
{
	// Diana 소유자만 ABP가 읽을 aim 상태를 서버에 갱신할 수 있음
	if (!IsOwnedByDianaPlayer())
	{
		return;
	}

	bIsAiming = bNewAiming;
}

// remote proxy는 서버가 인정한 Diana view를 actor rotation으로 표시
void ADianaOperatorPawn::OnRep_ServerViewRotation()
{
	if (!IsLocallyControlled())
	{
		SetActorRotation(ServerViewRotation);
	}
}

// Blueprint input asset 누락 시에도 soft path fallback으로 기본 조작 유지
void ADianaOperatorPawn::ResolveInputActions()
{
	auto LoadActionIfMissing = [](TObjectPtr<UInputAction>& Action, const TCHAR* Path)
	{
		if (Action)
		{
			return;
		}

		Action = Cast<UInputAction>(FSoftObjectPath(Path).TryLoad());
		if (!Action)
		{
			UE_LOG(LogTemp, Warning, TEXT("[DianaOperatorPawn::ResolveInputActions] Missing input action: %s"), Path);
		}
	};

	LoadActionIfMissing(LookAction, TEXT("/Game/Blueprints/Player/Input/Common/IA_Look.IA_Look"));
	LoadActionIfMissing(ScanAction, TEXT("/Game/Blueprints/Player/Input/Diana/IA_Scan.IA_Scan"));
	LoadActionIfMissing(InteractAction, TEXT("/Game/Blueprints/Player/Input/Diana/IA_Interact.IA_Interact"));
	LoadActionIfMissing(AimAction, TEXT("/Game/Blueprints/Player/Input/Common/IA_Aim.IA_Aim"));
	LoadActionIfMissing(CleanseAction, TEXT("/Game/Blueprints/Player/Input/Diana/IA_Cleanse.IA_Cleanse"));
}

// look action 값을 Diana camera 회전 intent로 변환
void ADianaOperatorPawn::OnTriggerLook(const FInputActionValue& Value)
{
	if (!CanUseDianaActionIntent())
	{
		return;
	}
	AddCameraInput(Value.Get<FVector2D>());
}

// Diana aim 입력은 local camera 상태와 crosshair 표시를 함께 켬
void ADianaOperatorPawn::OnStartAim(const FInputActionValue& Value)
{
	if (!CanUseDianaActionIntent())
	{
		return;
	}
	SetAimingState(true);
	if (APragmataPlayerController* PragmataPlayerController = Cast<APragmataPlayerController>(GetController()))
	{
		PragmataPlayerController->SetCrosshairVisibility(true);
	}
}

// Diana aim 해제는 local camera 상태와 crosshair 표시를 함께 끔
void ADianaOperatorPawn::OnCompleteAim(const FInputActionValue& Value)
{
	CancelPrecisionCleanse();
	RequestCancelHack();
	SetAimingState(false);
	if (APragmataPlayerController* PragmataPlayerController = Cast<APragmataPlayerController>(GetController()))
	{
		PragmataPlayerController->SetCrosshairVisibility(false);
	}
}

void ADianaOperatorPawn::OnHackDirectionUp()
{
	RequestHackDirection(EHackDirection::Up);
}

void ADianaOperatorPawn::OnHackDirectionDown()
{
	RequestHackDirection(EHackDirection::Down);
}

void ADianaOperatorPawn::OnHackDirectionLeft()
{
	RequestHackDirection(EHackDirection::Left);
}

void ADianaOperatorPawn::OnHackDirectionRight()
{
	RequestHackDirection(EHackDirection::Right);
}

// scan button은 target 없이 현재 camera trace 경로를 사용
void ADianaOperatorPawn::OnStartScan(const FInputActionValue& Value)
{
	UE_LOG(LogTemp, Log, TEXT("[DianaOperatorPawn::OnStartScan] IA_Scan input reached Diana pawn"));
	RequestScan();
}

// POI interaction은 scan/aim focus와 분리해 Diana 전용 입력에서만 실행
void ADianaOperatorPawn::OnStartInteract(const FInputActionValue& Value)
{
	RequestNodeInteraction();
}

// 정화 스킬 입력은 scan/interact와 분리해 별도 cooldown skill로 전달
void ADianaOperatorPawn::OnStartCleanse(const FInputActionValue& Value)
{
	if (GetIsAiming())
	{
		BeginPrecisionCleanse();
		return;
	}

	RequestQuickCleanse();
}

void ADianaOperatorPawn::OnCompleteCleanse(const FInputActionValue& Value)
{
	CommitPrecisionCleanse();
}

AActor* ADianaOperatorPawn::FindHackTargetAtReticle() const
{
	// enemy collision이 Visibility를 막지 않아도 reticle 주변 hackable actor를 잡음
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	const FVector TraceStart = GetCameraOrigin();
	const FVector TraceDirection = GetCameraDirection().GetSafeNormal();
	const FVector TraceEnd = TraceStart + TraceDirection * ReticleTraceDistance;

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DianaHackReticle), false, this);
	if (World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, Params))
	{
		AActor* HitActor = ResolveHackableActor(Hit.GetActor());
		if (HitActor)
		{
			return HitActor;
		}
	}

	if (EnemyHackAimSweepRadius <= 0.f)
	{
		return nullptr;
	}

	FCollisionQueryParams SweepParams(SCENE_QUERY_STAT(DianaHackReticleSweep), false, this);
	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
	ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);

	TArray<FHitResult> Hits;
	World->SweepMultiByObjectType(Hits, TraceStart, TraceEnd, FQuat::Identity, ObjectParams, FCollisionShape::MakeSphere(EnemyHackAimSweepRadius), SweepParams);

	AActor* BestActor = nullptr;
	float BestScore = TNumericLimits<float>::Max();
	for (const FHitResult& SweepHit : Hits)
	{
		AActor* HitActor = ResolveHackableActor(SweepHit.GetActor());
		if (!HitActor || !HasLineOfSightToHackTarget(HitActor, TraceStart))
		{
			continue;
		}

		const FVector ToActor = HitActor->GetActorLocation() - TraceStart;
		const float ForwardDistance = FVector::DotProduct(ToActor, TraceDirection);
		if (ForwardDistance < 0.f || ForwardDistance > ReticleTraceDistance)
		{
			continue;
		}

		const FVector ClosestPoint = TraceStart + TraceDirection * ForwardDistance;
		const float LateralDistance = FVector::Dist(ClosestPoint, HitActor->GetActorLocation());
		const float Score = LateralDistance * 10.f + ForwardDistance * 0.01f;
		if (Score < BestScore)
		{
			BestScore = Score;
			BestActor = HitActor;
		}
	}

	return BestActor;
}

AActor* ADianaOperatorPawn::ResolveHackableActor(AActor* Candidate) const
{
	// component/child actor hit도 상위 hackable actor로 승격해 target 판정 흔들림을 줄임
	for (AActor* Current = Candidate; Current; Current = Current->GetAttachParentActor())
	{
		if (Current->GetClass()->ImplementsInterface(UPragmataHackable::StaticClass()))
		{
			return Current;
		}
	}

	return nullptr;
}

bool ADianaOperatorPawn::HasLineOfSightToHackTarget(AActor* Candidate, const FVector& TraceStart) const
{
	// sweep 후보도 visibility blocker를 통과해야 서버 거절 RPC 반복을 줄임
	UWorld* World = GetWorld();
	if (!World || !Candidate)
	{
		return false;
	}

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DianaHackReticleLineOfSight), false, this);
	const bool bHit = World->LineTraceSingleByChannel(Hit, TraceStart, Candidate->GetActorLocation(), ECC_Visibility, Params);

	return !bHit || PragmataTraceUtils::IsHitActorWithinCandidate(Hit, Candidate);
}

void ADianaOperatorPawn::UpdateEnemyHackAim(float DeltaSeconds)
{
	// 조준 중에는 mode와 무관하게 enemy reticle target을 서버 검증 경로로 보냄
	if (!IsLocallyControlled() || !bIsAiming || !HackComponent || !CanUseDianaActionIntent() || (CleanseComponent && CleanseComponent->IsCleansePreviewActive()))
	{
		EnemyHackAimRetryTimer = 0.f;
		EnemyHackAimLostTimer = 0.f;
		LastRequestedEnemyHackTarget = nullptr;
		PendingEnemyHackCancelTarget = nullptr;
		LastObservedActiveEnemyHackTarget = nullptr;
		return;
	}

	AActor* ReticleTarget = FindHackTargetAtReticle();
	if (ReticleTarget != LastRequestedEnemyHackTarget)
	{
		EnemyHackAimRetryTimer = 0.f;
	}

	if (AActor* ActiveHackTarget = HackComponent->GetActiveEnemyHackTarget())
	{
		if (ActiveHackTarget != LastObservedActiveEnemyHackTarget)
		{
			LastObservedActiveEnemyHackTarget = ActiveHackTarget;
			EnemyHackAimLostTimer = 0.f;
			PendingEnemyHackCancelTarget = nullptr;
		}

		EnemyHackAimRetryTimer = 0.f;
		LastRequestedEnemyHackTarget = nullptr;
		if (ReticleTarget != ActiveHackTarget)
		{
			EnemyHackAimLostTimer += DeltaSeconds;
			if (EnemyHackAimLostTimer >= EnemyHackAimLostCancelDelay
				&& PendingEnemyHackCancelTarget != ActiveHackTarget)
			{
				PendingEnemyHackCancelTarget = ActiveHackTarget;
				RequestCancelHack(ActiveHackTarget);
			}
		}
		else
		{
			EnemyHackAimLostTimer = 0.f;
			PendingEnemyHackCancelTarget = nullptr;
		}
		return;
	}

	LastObservedActiveEnemyHackTarget = nullptr;
	EnemyHackAimLostTimer = 0.f;
	PendingEnemyHackCancelTarget = nullptr;

	EnemyHackAimRetryTimer -= DeltaSeconds;
	if (EnemyHackAimRetryTimer > 0.f)
	{
		return;
	}

	EnemyHackAimRetryTimer = EnemyHackAimRetryInterval;
	if (ReticleTarget)
	{
		LastRequestedEnemyHackTarget = ReticleTarget;
		RequestHack(ReticleTarget);
	}
	else
	{
		LastRequestedEnemyHackTarget = nullptr;
	}
}

void ADianaOperatorPawn::RequestHackDirection(EHackDirection Direction)
{
	if (!CanUseDianaActionIntent())
	{
		return;
	}

	// 해킹 UI가 열려 있을 때만 방향 키를 서버 puzzle 입력으로 전달
	if (HackComponent && HackComponent->IsEnemyHackActive())
	{
		HackComponent->RequestHackDirection(Direction);
		return;
	}

	if (NodeInteractionComponent && NodeInteractionComponent->IsMapHackActive())
	{
		NodeInteractionComponent->RequestMapHackDirection(Direction);
	}
}

void ADianaOperatorPawn::RequestCancelHack(AActor* ExpectedEnemyHackTarget)
{
	// enemy reticle 이탈 cancel은 expected target으로 stale RPC를 거르고 aim 해제는 전체 취소
	if (HackComponent)
	{
		HackComponent->RequestCancelHack(ExpectedEnemyHackTarget);
	}
	if (!ExpectedEnemyHackTarget && NodeInteractionComponent)
	{
		NodeInteractionComponent->RequestCancelMapHack();
	}
}

// Diana pawn 위치는 Hugh anchor 기준으로 유지해 camera cone 계산을 안정화
void ADianaOperatorPawn::FollowHughAnchor()
{
	const UWorld* World = GetWorld();
	const APragmataGameState* GameState = World ? World->GetGameState<APragmataGameState>() : nullptr;
	const AActor* HughActor = GameState ? GameState->GetHughCharacter() : nullptr;
	if (!HughActor)
	{
		return;
	}

	SetActorLocation(HughActor->GetActorLocation() + HughAnchorOffset);
}

void ADianaOperatorPawn::UpdateAimCamera(float DeltaSeconds)
{
	// Hugh aim처럼 FOV와 arm length를 보간해 Diana 조준감을 맞춤
	if (!CameraComponent || !CameraBoom)
	{
		return;
	}

	const float TargetFOV = bIsAiming ? AimFOV : DefaultFOV;
	const float TargetArmLength = bIsAiming ? AimArmLength : DefaultArmLength;
	CameraComponent->FieldOfView = FMath::FInterpTo(CameraComponent->FieldOfView, TargetFOV, DeltaSeconds, AimZoomSpeed);
	CameraBoom->TargetArmLength = FMath::FInterpTo(CameraBoom->TargetArmLength, TargetArmLength, DeltaSeconds, AimZoomSpeed);
}

// Diana camera는 pitch와 roll만 정리하고 yaw는 자유 회전을 유지
FRotator ADianaOperatorPawn::ClampViewRotation(const FRotator& DesiredRotation) const
{
	FRotator ClampedRotation = DesiredRotation;
	ClampedRotation.Pitch = FMath::ClampAngle(ClampedRotation.Pitch, MinPitch, MaxPitch);
	ClampedRotation.Roll = 0.f;
	return ClampedRotation;
}

// authority에서는 replicated view rotation도 함께 갱신
void ADianaOperatorPawn::ApplyViewRotation(const FRotator& NewViewRotation)
{
	SetActorRotation(NewViewRotation);
	if (HasAuthority())
	{
		ServerViewRotation = NewViewRotation;
	}
}

// AssignedRole이 Diana인 controller만 server view RPC를 인정
bool ADianaOperatorPawn::IsOwnedByDianaPlayer() const
{
	const APragmataPlayerController* PragmataPlayerController = Cast<APragmataPlayerController>(GetController());
	const APragmataPlayerState* PragmataPlayerState = PragmataPlayerController ? PragmataPlayerController->GetPlayerState<APragmataPlayerState>() : nullptr;
	return PragmataPlayerState && PragmataPlayerState->GetAssignedRole() == EPragmataPlayerRole::Diana;
}

FVector ADianaOperatorPawn::GetCameraOrigin() const
{
	return CameraComponent ? CameraComponent->GetComponentLocation() : GetActorLocation();
}

FVector ADianaOperatorPawn::GetCameraDirection() const
{
	return CameraComponent ? CameraComponent->GetForwardVector() : GetActorForwardVector();
}

// 서버 검증은 서버가 보유한 pawn transform 기준 camera origin을 사용
FVector ADianaOperatorPawn::GetAuthoritativeCameraOrigin() const
{
	return GetCameraOrigin();
}

// 서버 검증은 client가 보낸 trace direction 대신 저장된 view direction을 사용
FVector ADianaOperatorPawn::GetAuthoritativeCameraDirection() const
{
	return GetCameraDirection();
}

// operator가 자유 조작 가능한 mode일 때만 scan/hack component 호출 허용
bool ADianaOperatorPawn::CanUseScanOrHack() const
{
	return OperatorMode == EDianaOperatorMode::Roaming || OperatorMode == EDianaOperatorMode::Combat;
}

bool ADianaOperatorPawn::CanUseDianaActionIntent() const
{
	const UWorld* World = GetWorld();
	const APragmataGameState* GameState = World ? World->GetGameState<APragmataGameState>() : nullptr;
	return CanUseScanOrHack()
		&& (!GameState || !GameState->AreDianaActionsLocked());
}
