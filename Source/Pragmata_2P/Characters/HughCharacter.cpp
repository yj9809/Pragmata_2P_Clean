#include "Characters/HughCharacter.h"

#include "EnhancedInputComponent.h"
#include "KismetAnimationLibrary.h"
#include "Animation/AnimInstance.h"
#include "Animation/HughAnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PairedAnimationComponent.h"
#include "Animation/DianaAnimInstance.h"
#include "Components/PragmataFactionComponent.h"
#include "Components/PragmataHealthComponent.h"
#include "Components/PragmataPlayerStatsComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Data/LobbyCharacterSkinSet.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Characters/PragmataHughMovementComponent.h"
#include "Engine/Engine.h"
#include "Game/PragmataPlayerController.h"
#include "Game/PragmataGameMode.h"
#include "Game/PragmataGameState.h"
#include "InputAction.h"
#include "Components/AudioComponent.h"
#include "Game/TutorialBlueprintLibrary.h"
#include "Pawns/DianaOperatorPawn.h"
#include "Components/DianaVoiceComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "UI/PragmataHUDWidget.h"
#include "UObject/SoftObjectPath.h"
#include "Weapon/DecoyGun.h"
#include "Weapon/GripGun.h"
#include "Weapon/ShotGun.h"
#include "Weapon/WeaponBase.h"
#include "WorldPartition/ContentBundle/ContentBundleLog.h"

namespace
{
	FName GetHitReactSectionName(EPragmataHitDirection Direction)
	{
		switch (Direction)
		{
		case EPragmataHitDirection::Right:
			return TEXT("Right");
		case EPragmataHitDirection::Back:
			return TEXT("Back");
		case EPragmataHitDirection::Left:
			return TEXT("Left");
		case EPragmataHitDirection::Front:
		default:
			return TEXT("Front");
		}
	}
}

// Hugh 전용 movement prediction 컴포넌트로 기본 이동 컴포넌트 교체
AHughCharacter::AHughCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UPragmataHughMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	// Diana의 별도 SpringArm이 Hugh를 장애물로 판정해 조준 카메라가 캐릭터 내부로 당겨지는 현상을 방지
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(GetRootComponent());
	CameraBoom->TargetArmLength = DefaultArmLength;
	CameraBoom->SocketOffset = FVector(0.f, 80.f, 70.f);
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->CameraLagSpeed = 12.f;

	CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraComponent"));
	CameraComponent->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	CameraComponent->bUsePawnControlRotation = false;

	DianaAvatarMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("DianaAvatarMesh"));
	DianaAvatarMesh->SetupAttachment(GetMesh(), DianaMountSocketName);
	DianaAvatarMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DianaAvatarMesh->SetGenerateOverlapEvents(false);

	HeadVisorSpotlight = CreateDefaultSubobject<USpotLightComponent>(TEXT("HeadVisorSpotlight"));
	HeadVisorSpotlight->SetupAttachment(GetMesh(), HeadVisorLightSocketName);
	HeadVisorSpotlight->SetRelativeLocation(FVector::ZeroVector);
	HeadVisorSpotlight->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	HeadVisorSpotlight->SetIntensity(5000.f);
	HeadVisorSpotlight->SetAttenuationRadius(1400.f);
	HeadVisorSpotlight->SetInnerConeAngle(14.f);
	HeadVisorSpotlight->SetOuterConeAngle(34.f);
	HeadVisorSpotlight->SetCastShadows(true);
	HeadVisorSpotlight->SetVisibility(bHeadVisorSpotlightEnabled);
	HeadVisorSpotlight->SetHiddenInGame(!bHeadVisorSpotlightEnabled);

	//기존 waistarm 처리방식
	// WaistArmMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WaistArmMesh"));
	// WaistArmMesh->SetupAttachment(GetMesh(), WaistArmAttachSocketName);
	// WaistArmMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// WaistArmMesh->SetGenerateOverlapEvents(false);

	//신규 waistarm 처리방식(component화)
	WaistArmComponent = CreateDefaultSubobject<UWaistArmComponent>(TEXT("WaistArmComponent"));
	WaistArmComponent->SetupAttachment(GetMesh(), WaistArmAttachSocketName);

	PairedAnimationComponent = CreateDefaultSubobject<UPairedAnimationComponent>(TEXT("PairedAnimationComponent"));
	PlayerStatsComponent = CreateDefaultSubobject<UPragmataPlayerStatsComponent>(TEXT("PlayerStatsComponent"));
	if (HealthComponent)
	{
		HealthComponent->SetDefaultHealth(DefaultMaxHP);
	}

	//charactermovement
	if (GetCharacterMovement())
	{
		GetCharacterMovement()->RotationRate = FRotator(0.f, 540.f, 0.f);
		GetCharacterMovement()->JumpZVelocity = 800.f;
		GetCharacterMovement()->bUseControllerDesiredRotation = false;
		GetCharacterMovement()->bOrientRotationToMovement = true;
		GetCharacterMovement()->MaxFlySpeed = 100.f;
	}
	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	//스태미나 초기화
	CurStamina = MaxStamina;
	GameplayHUDClass = TSoftClassPtr<UPragmataHUDWidget>(FSoftClassPath(TEXT("/Game/UI/Widget/WBP_HughHUD.WBP_HughHUD_C")));
	LobbySkinSet = TSoftObjectPtr<ULobbyCharacterSkinSet>(FSoftObjectPath(TEXT("/Game/Data/DA_LobbyCharacterSkinSet.DA_LobbyCharacterSkinSet")));
	RefreshReplicatedAnimState();

	//weapon array
	WeaponInventory.SetNumZeroed(static_cast<int>(EWeaponType::Max));
	CurrentWeaponIndex = 0;
}

// BP 인스턴스 생성 시 로봇팔 child mesh가 CDO component에 붙는 template mismatch를 보정
void AHughCharacter::PreRegisterAllComponents()
{
	if (WaistArmComponent)
	{
		WaistArmComponent->EnsureWaistArmMeshAttachment();
	}

	Super::PreRegisterAllComponents();
}

// JunHyun branch 기준으로 각 Hugh 인스턴스가 직접 무기를 만들고 로봇팔에 부착
void AHughCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	if (HasAuthority() && GetWorld() && DefaultWeaponClass)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.Instigator = this;

		CurrentWeapon = GetWorld()->SpawnActor<AWeaponBase>(DefaultWeaponClass, SpawnParams);
		if (CurrentWeapon && WaistArmComponent)
		{
			CurrentWeapon->SetOwner(this);

			//게임 시작 시 총은 로봇팔에 부착
			CurrentWeapon->AttachToComponent(WaistArmComponent->GetWaistArmMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, WaistArmWeaponAttachSocketName);

			//무기 장착
			EquipWeapon(CurrentWeapon);
			//무기 인벤토리에 등록
			WeaponInventory[static_cast<int>(CurrentWeapon->GetWeaponType())] = CurrentWeapon;
		}
	}

	if (HasAuthority() && GetWorld() && AttackWeaponClass)
	{
		// BP_HughCharacter가 지정한 Attack weapon만 시작 inventory에 지급
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.Instigator = this;

		AWeaponBase* AttackWeapon = GetWorld()->SpawnActor<AWeaponBase>(AttackWeaponClass, SpawnParams);
		const int32 AttackWeaponIndex = AttackWeapon ? static_cast<int32>(AttackWeapon->GetWeaponType()) : INDEX_NONE;
		if (AttackWeapon && WeaponInventory.IsValidIndex(AttackWeaponIndex) && !WeaponInventory[AttackWeaponIndex])
		{
			AttackWeapon->StoreWeapon(this, LeftBackWeaponSocketName);
			WeaponInventory[AttackWeaponIndex] = AttackWeapon;
		}
		else if (AttackWeapon)
		{
			AttackWeapon->Destroy();
		}
	}

	if (HasAuthority() && GetWorld() && TacticalWeaponClass)
	{
		// BP_HughCharacter가 지정한 Tactical weapon만 시작 inventory에 지급
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.Instigator = this;

		AWeaponBase* TacticalWeapon = GetWorld()->SpawnActor<AWeaponBase>(TacticalWeaponClass, SpawnParams);
		const int32 TacticalWeaponIndex = TacticalWeapon ? static_cast<int32>(TacticalWeapon->GetWeaponType()) : INDEX_NONE;
		if (TacticalWeapon && WeaponInventory.IsValidIndex(TacticalWeaponIndex) && !WeaponInventory[TacticalWeaponIndex])
		{
			TacticalWeapon->StoreWeapon(this, LeftBackWeaponSocketName);
			WeaponInventory[TacticalWeaponIndex] = TacticalWeapon;
		}
		else if (TacticalWeapon)
		{
			TacticalWeapon->Destroy();
		}
	}

	if (HasAuthority())
	{
		RefreshStoredWeaponPresentation();
		OnWeaponInventoryChanged.Broadcast();
	}
}

// 시작 시 Diana avatar와 로봇팔 component 상태를 로컬 component에 반영
void AHughCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (FactionComponent)
	{
		FactionComponent->SetFaction(EPragmataFaction::Player);
	}
	if (HasAuthority() && HealthComponent)
	{
		// BP component default가 오래된 HP 값을 저장해도 Hugh 기본값을 서버 source에 고정
		HealthComponent->SetDefaultHealth(DefaultMaxHP);
		HealthComponent->OnDeath.AddDynamic(this, &AHughCharacter::HandleDeath);
		HealthComponent->OnHealthChanged.AddDynamic(this, &AHughCharacter::OnHealSourceHealthChanged);
		if (PlayerStatsComponent)
		{
			PlayerStatsComponent->OnRepairCartridgeChanged.AddDynamic(this, &AHughCharacter::OnHealSourceCartridgeChanged);
			PlayerStatsComponent->RefreshUpgradeEffects();
		}
	}

	AttachDianaAvatar();
	AttachWaistArm();
	EnsureHeadVisorSpotlightAttachment();

	if (PairedAnimationComponent)
	{
		PairedAnimationComponent->InitializeMeshes(GetMesh(), DianaAvatarMesh);
	}

	if (GetCharacterMovement())
	{
		DefaultGravityScale = GetCharacterMovement()->GravityScale;
	}

	if (HasAuthority())
	{
		bHeadVisorSpotlightEnabled = bHeadVisorSpotlightStartsEnabled;
		UpdateHealAvailability();
	}
	ApplyHeadVisorSpotlightEnabled(bHeadVisorSpotlightEnabled);
}

// 매 프레임 예측 intent와 애니메이션 상태를 동기화
void AHughCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (IsDead && DianaAvatarMesh && !DianaAvatarMesh->GetAttachParent() && GetMesh())
	{
		FVector DianaLocation = DianaAvatarMesh->GetComponentLocation();
		DianaLocation.Z = GetMesh()->GetSocketLocation(TEXT("root")).Z;
		DianaAvatarMesh->SetWorldLocation(DianaLocation);
	}

	SyncMovementIntentFromMovementComponent();
	UpdateMovementState();

	//캐릭터 회전
	//조준 중일때 카메라 시야 제한
	if (HughAnimState.bIsAiming && Controller && CameraComponent)
	{
		FVector CameraForward = CameraComponent->GetForwardVector();
		FRotator TargetRotation = CameraForward.Rotation();
		// TargetRotation.Pitch = 0.f;
		TargetRotation.Roll = 0.f;

		//현재 캐릭터의 rotation에서 타겟(카메라의 회전값)으로 보간하여 움직이게 함
		FRotator CurrentRotation = GetActorRotation();
		FRotator InterpolatedRotation = FMath::RInterpTo(CurrentRotation, TargetRotation, DeltaSeconds, RotateSpeed);
		SetActorRotation(InterpolatedRotation);
	}

	//조준 시 카메라 설정값
	if (CameraComponent && CameraBoom)
	{
		float TargetFOV = HughAnimState.bIsAiming ? AimFOV : DefaultFOV;
		float TargetArmLength = HughAnimState.bIsAiming ? AimArmLength : DefaultArmLength;

		float CurrentFOV = CameraComponent->FieldOfView;
		CameraComponent->FieldOfView = FMath::FInterpTo(CurrentFOV, TargetFOV, DeltaSeconds, ZoomSpeed);

		float CurrentArmLength = CameraBoom->TargetArmLength;
		CameraBoom->TargetArmLength = FMath::FInterpTo(CurrentArmLength, TargetArmLength, DeltaSeconds, ZoomSpeed);
	}

	//무기 반동 제어
	WeaponRecoil(DeltaSeconds);

	//스태미나 회복
	if (HasAuthority() || IsLocallyControlled())
	{
		RecoverStamina(DeltaSeconds);
	}

	//HUD(부스트 게이지)로 현재 스태미나/잠금 상태 전달
	if (HasAuthority() || IsLocallyControlled())
	{
		BroadcastStaminaChanged();
	}

	if (HasAuthority())
	{
		UpdateHealAvailability();
	}

	if (IsLocallyControlled())
	{
		UpdateHealHold(DeltaSeconds);
	}

	//회전값 계산하기
	CalculateRotation(DeltaSeconds);

	//각도 구하기
	CalculateAngle();

	// PrintDebugMsg();
}

// 원격 클라이언트가 Hugh 애니메이션 상태를 재구성할 snapshot 복제
void AHughCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	//서버가 클라들에게 데이터를 강제로 복사해주는 통로:
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(AHughCharacter, HughAnimState, COND_SkipOwner);
	DOREPLIFETIME(AHughCharacter, bHeadVisorSpotlightEnabled);
	DOREPLIFETIME(AHughCharacter, bCanUseHeal);
	DOREPLIFETIME(AHughCharacter, MaxStamina);
	DOREPLIFETIME(AHughCharacter, CurStamina);
	DOREPLIFETIME(AHughCharacter, IsStaminaConsumable);
	DOREPLIFETIME(AHughCharacter, CurrentWeapon);
	DOREPLIFETIME(AHughCharacter, WeaponInventory);
	DOREPLIFETIME(AHughCharacter, LastLeftBackWeaponType);
	DOREPLIFETIME(AHughCharacter, ReplicatedHughSkinId);
	DOREPLIFETIME(AHughCharacter, ReplicatedDianaSkinId);
}

// 피해 결과를 Hugh 직접 hit react와 필요한 Diana paired cue로 분리
bool AHughCharacter::ReceiveDamage_Implementation(const FPragmataDamageSpec& DamageSpec)
{
	const float PreviousHealth = HealthComponent ? HealthComponent->GetCurrentHealth() : 0.f;
	const bool bApplied = Super::ReceiveDamage_Implementation(DamageSpec);
	if (!bApplied)
	{
		return bApplied;
	}

	IsDead = HealthComponent && HealthComponent->IsDead();
	const float CurrentHealth = HealthComponent ? HealthComponent->GetCurrentHealth() : PreviousHealth;
	const float ActualDamage = FMath::Max(PreviousHealth - CurrentHealth, 0.f);
	if (HasAuthority() && ActualDamage > 0.f && !IsDead)
	{
		if (APragmataGameState* GameState = GetWorld() ? GetWorld()->GetGameState<APragmataGameState>() : nullptr)
		{
			if (ADianaOperatorPawn* DianaPawn = GameState->GetDianaOperatorPawn())
			{
				DianaPawn->GetVoiceComponent()->PlayVoiceAction(EDianaVoiceAction::Caution);
			}
		}
	}

	const EPragmataHitReactStrength Strength = IsDead
		? EPragmataHitReactStrength::Death
		: DamageSpec.HitReactStrength == EPragmataHitReactStrength::ForcedPair
			? EPragmataHitReactStrength::ForcedPair
			: DamageSpec.HitReactStrength;

	const EPragmataHitDirection Direction = ResolveHitReactDirection(DamageSpec);

	//
	if (UHughAnimInstance* AnimInstance = Cast<UHughAnimInstance>(GetMesh()->GetAnimInstance()))
	{
		AnimInstance->SetHitDirection(Direction);
	}

	PlayHitReactMontage(Strength, Direction);

	if (PairedAnimationComponent && ShouldSendPairedHitReactCue(Strength))
	{
		const EPairedAnimationCueType CueType = IsDead
			? EPairedAnimationCueType::Death
			: EPairedAnimationCueType::HitReact;
		PairedAnimationComponent->TriggerPairedCue(CueType, Strength, Direction);
	}

	return true;
}

// Hugh 입력 action을 로컬 예측 상태 변경 함수에 연결
void AHughCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	ResolveInputActions();

	if (UEnhancedInputComponent* EnhancedInputComponent = CastChecked<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (MoveAction)
		{
			EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Started, this, &AHughCharacter::OnStartMove);
			EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AHughCharacter::OnTriggerMove);
			EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Completed, this, &AHughCharacter::OnCompleteMove);
		}
		if (LookAction)
		{
			EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AHughCharacter::OnTriggerLook);
		}
		if (AttackAction)
		{
			EnhancedInputComponent->BindAction(AttackAction, ETriggerEvent::Started, this, &AHughCharacter::OnStartAttack);
		}
		if (DodgeAction)
		{
			EnhancedInputComponent->BindAction(DodgeAction, ETriggerEvent::Started, this, &AHughCharacter::OnStartDash);
		}
		if (JumpAction)
		{
			EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &AHughCharacter::OnStartJump);
			EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Triggered, this, &AHughCharacter::OnTriggerJump);
			EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &AHughCharacter::OnCompleteJump);
		}
		if (AimAction)
		{
			EnhancedInputComponent->BindAction(AimAction, ETriggerEvent::Started, this, &AHughCharacter::OnStartAim);
			EnhancedInputComponent->BindAction(AimAction, ETriggerEvent::Completed, this, &AHughCharacter::OnCompleteAim);
		}
		if (BoostAction)
		{
			EnhancedInputComponent->BindAction(BoostAction, ETriggerEvent::Triggered, this, &AHughCharacter::OnTriggerRun);
			EnhancedInputComponent->BindAction(BoostAction, ETriggerEvent::Completed, this, &AHughCharacter::OnCompleteRun);
		}
		if (HeadVisorLightToggleAction)
		{
			EnhancedInputComponent->BindAction(HeadVisorLightToggleAction, ETriggerEvent::Started, this, &AHughCharacter::OnToggleHeadVisorSpotlight);
		}
		if (WeaponChangeAction)
		{
			EnhancedInputComponent->BindAction(WeaponChangeAction, ETriggerEvent::Started, this, &AHughCharacter::OnStartWeaponChange);
		}
		if (CycleWeaponAction)
		{
			EnhancedInputComponent->BindAction(CycleWeaponAction, ETriggerEvent::Triggered, this, &AHughCharacter::OnCycleWeapon);
		}
		if (InteractAction)
		{
			EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Started, this, &AHughCharacter::OnStartInteract);
		}
		if (HealAction)
		{
			EnhancedInputComponent->BindAction(HealAction, ETriggerEvent::Started, this, &AHughCharacter::OnStartHeal);
			EnhancedInputComponent->BindAction(HealAction, ETriggerEvent::Completed, this, &AHughCharacter::OnCompleteHeal);
			EnhancedInputComponent->BindAction(HealAction, ETriggerEvent::Canceled, this, &AHughCharacter::OnCompleteHeal);
		}
	}
}

int32 AHughCharacter::GetRemainingHealCartridges() const
{
	return PlayerStatsComponent ? PlayerStatsComponent->GetRemainingHealCartridges() : 0;
}

bool AHughCharacter::CanUseHeal() const
{
	if (HasAuthority() || IsLocallyControlled())
	{
		return CalculateHealAvailability();
	}

	return bCanUseHeal;
}

float AHughCharacter::GetHealHoldProgress() const
{
	const float ThresholdSeconds = FMath::Max(UE_KINDA_SMALL_NUMBER, HealHoldThresholdSeconds);
	return bHealInputHeld ? FMath::Clamp(HealHeldSeconds / ThresholdSeconds, 0.f, 1.f) : 0.f;
}

FVector AHughCharacter::GetAuthoritativeAimOrigin() const
{
	// Diana 빠른 정화는 Hugh가 실제로 보고 있는 camera 위치를 서버 기준점으로 사용
	return CameraComponent ? CameraComponent->GetComponentLocation() : GetActorLocation();
}

FVector AHughCharacter::GetAuthoritativeAimDirection() const
{
	// controller 조준값이 있으면 weapon/camera presentation보다 우선
	if (const AController* HughController = GetController())
	{
		const FVector ControlDirection = HughController->GetControlRotation().Vector().GetSafeNormal();
		if (!ControlDirection.IsNearlyZero())
		{
			return ControlDirection;
		}
	}

	if (CameraComponent)
	{
		const FVector CameraDirection = CameraComponent->GetForwardVector().GetSafeNormal();
		if (!CameraDirection.IsNearlyZero())
		{
			return CameraDirection;
		}
	}

	return GetActorForwardVector().GetSafeNormal();
}

void AHughCharacter::ShowObjectiveAlert(float DisplaySeconds)
{
	ShowObjectiveAlertText(ObjectiveAlertText, DisplaySeconds);
}

void AHughCharacter::ShowObjectiveAlertText(const FText& InObjectiveText, float DisplaySeconds)
{
	// 캐릭터는 목표 문구 보관과 owning HUD 표시 진입점만 제공하고, 목표 생성 판정은 상위 로직이 담당
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

// GameMode가 확정한 lobby skin 선택을 Hugh pawn 복제 상태로 전파
void AHughCharacter::SetGameplaySkinIds(FName HughSkinId, FName DianaSkinId)
{
	if (!HasAuthority())
	{
		return;
	}

	ReplicatedHughSkinId = HughSkinId;
	ReplicatedDianaSkinId = DianaSkinId;
	ApplyGameplaySkinIds();
}

// skin id 복제 도착 후 main mesh와 Diana avatar mesh를 같은 경로로 갱신
void AHughCharacter::OnRep_GameplaySkinIds()
{
	ApplyGameplaySkinIds();
}

// mesh 로드 실패 시 기존 mesh를 유지해 패키지에서 교체 순간 빈 표시를 막음
void AHughCharacter::ApplyGameplaySkinIds()
{
	ULobbyCharacterSkinSet* SkinSetAsset = LoadLobbySkinSet();
	if (!SkinSetAsset)
	{
		return;
	}

	const FName HughSkinId = ReplicatedHughSkinId != NAME_None
		? ReplicatedHughSkinId
		: SkinSetAsset->ResolveDefaultSkinId(EPragmataPlayerRole::Hugh);
	if (USkeletalMesh* HughSkinMesh = SkinSetAsset->LoadSkinMesh(EPragmataPlayerRole::Hugh, HughSkinId))
	{
		if (GetMesh() && GetMesh()->GetSkeletalMeshAsset() != HughSkinMesh)
		{
			GetMesh()->SetSkeletalMesh(HughSkinMesh);
		}
	}

	const FName DianaSkinId = ReplicatedDianaSkinId != NAME_None
		? ReplicatedDianaSkinId
		: SkinSetAsset->ResolveDefaultSkinId(EPragmataPlayerRole::Diana);
	if (USkeletalMesh* DianaSkinMesh = SkinSetAsset->LoadSkinMesh(EPragmataPlayerRole::Diana, DianaSkinId))
	{
		if (DianaAvatarMesh && DianaAvatarMesh->GetSkeletalMeshAsset() != DianaSkinMesh)
		{
			DianaAvatarMesh->SetSkeletalMesh(DianaSkinMesh);
		}
	}
}

ULobbyCharacterSkinSet* AHughCharacter::LoadLobbySkinSet()
{
	if (LoadedLobbySkinSet)
	{
		return LoadedLobbySkinSet;
	}

	if (!LobbySkinSet.IsNull())
	{
		LoadedLobbySkinSet = LobbySkinSet.LoadSynchronous();
	}

	if (!LoadedLobbySkinSet)
	{
		LoadedLobbySkinSet = NewObject<ULobbyCharacterSkinSet>(this, TEXT("RuntimeGameplaySkinSet"));
	}

	return LoadedLobbySkinSet;
}

EPragmataHitDirection AHughCharacter::ResolveHitReactDirection(const FPragmataDamageSpec& DamageSpec) const
{
	const AActor* DirectionSource = DamageSpec.DamageCauser ? DamageSpec.DamageCauser.Get() : DamageSpec.SourceActor.Get();
	if (!DirectionSource)
	{
		return DamageSpec.HitDirection;
	}

	FVector DirectionToSource = DirectionSource->GetActorLocation() - GetActorLocation();
	DirectionToSource.Z = 0.f;
	if (!DirectionToSource.Normalize())
	{
		return DamageSpec.HitDirection;
	}

	FVector ForwardVector = GetActorForwardVector();
	ForwardVector.Z = 0.f;
	ForwardVector.Normalize();

	FVector RightVector = GetActorRightVector();
	RightVector.Z = 0.f;
	RightVector.Normalize();

	const float ForwardDot = FVector::DotProduct(ForwardVector, DirectionToSource);
	const float RightDot = FVector::DotProduct(RightVector, DirectionToSource);

	// Hugh 기준 4방향 section 경계를 45도 대각선으로 나눈다.
	if (FMath::Abs(ForwardDot) > FMath::Abs(RightDot))
	{
		return ForwardDot > 0.f ? EPragmataHitDirection::Front : EPragmataHitDirection::Back;
	}

	return RightDot > 0.f ? EPragmataHitDirection::Right : EPragmataHitDirection::Left;
}

bool AHughCharacter::PlayHitReactMontage(EPragmataHitReactStrength Strength, EPragmataHitDirection Direction)
{
	UAnimMontage* Montage = nullptr;
	switch (Strength)
	{
	case EPragmataHitReactStrength::Small:
		Montage = HitReactSmallMontage;
		break;
	case EPragmataHitReactStrength::Medium:
		Montage = HitReactMediumMontage;
		break;
	case EPragmataHitReactStrength::Large:
	case EPragmataHitReactStrength::ForcedPair:
		Montage = HitReactLargeMontage;
		break;
	default:
		return false;
	}

	//check if is in the air
	if (!IsDead && (!GetCharacterMovement() && GetCharacterMovement()->IsFalling()) || HughAnimState.bIsHovering)
	{
		Montage = HitReactMidairMontage;
	}

	if (!Montage || !GetMesh())
	{
		return false;
	}

	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (!AnimInstance)
	{
		return false;
	}

	AnimInstance->Montage_Play(Montage, 1.f);
	AnimInstance->Montage_JumpToSection(GetHitReactSectionName(Direction), Montage);
	return true;
}

bool AHughCharacter::ShouldSendPairedHitReactCue(EPragmataHitReactStrength Strength) const
{
	return Strength == EPragmataHitReactStrength::Large
		|| Strength == EPragmataHitReactStrength::Death
		|| Strength == EPragmataHitReactStrength::ForcedPair;
}

// 이동 시작 시 dash montage 잔류와 braking 값을 정리
void AHughCharacter::OnStartMove(const FInputActionValue& Value)
{
	if (IsDead) return;
	IsBraking = false;
	if (GetCharacterMovement())
	{
		if (!HughAnimState.bIsAiming)
			GetCharacterMovement()->bOrientRotationToMovement = true;

		//움직이기 시작하면 deceleration값 원상복구 시키기
		GetCharacterMovement()->BrakingDecelerationWalking = DefaultBreakingDeceleration;
	}

	//dash 모션이 재생중일 경우 종료 시킴
	if (!IsDashing)
	{
		UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
		if (!AnimInstance) return;
		//dash변수가 false인데 아직 dash 애니메이션 재생중일 경우 바로 해당 모션 종료
		if (DashMontage && AnimInstance->Montage_IsPlaying(DashMontage))
		{
			StopAnimMontage(DashMontage);
		}
		//finish aimdash montage if is playing
		if (AimDashMontage && AnimInstance->Montage_IsPlaying(AimDashMontage))
		{
			StopAnimMontage(AimDashMontage);
		}
		//finish midairdash montage if is playing
		if (MidairDashMontage && AnimInstance->Montage_IsPlaying(MidairDashMontage))
		{
			StopAnimMontage(MidairDashMontage);
		}
	}
}

void AHughCharacter::AttachWeaponToSocket(FName SocketName)
{
	//animnotify에서 지정한 소켓 이름에 무기 부착시키기
	if (CurrentWeapon)
	{
		CurrentWeapon->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, SocketName);
	}
}

// 특정 weapon actor를 type별 수납 socket에 부착
void AHughCharacter::AttachWeaponToStoredSocket(AWeaponBase* Weapon)
{
	if (!Weapon)
	{
		return;
	}

	if (Weapon->GetWeaponType() == EWeaponType::Primary)
	{
		if (WaistArmComponent)
		{
			Weapon->AttachToComponent(WaistArmComponent->GetWaistArmMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, WaistArmWeaponAttachSocketName);
		}
		return;
	}

	const FName StoreSocketName = GetStoredWeaponSocketName(Weapon->GetWeaponType());
	if (StoreSocketName != NAME_None)
	{
		Weapon->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, StoreSocketName);
	}
}

// 비조준 weapon type 선택은 같은 등 socket에 selected weapon 하나만 보이게 함
void AHughCharacter::RefreshStoredWeaponPresentation()
{
	NormalizeLastLeftBackWeaponType();

	for (AWeaponBase* Weapon : WeaponInventory)
	{
		if (!IsValid(Weapon))
		{
			continue;
		}

		const bool bIsCurrentWeapon = Weapon == CurrentWeapon;
		if (Weapon->GetWeaponType() == EWeaponType::Primary)
		{
			Weapon->SetActorHiddenInGame(false);
			if (!HughAnimState.bIsAiming)
			{
				AttachWeaponToStoredSocket(Weapon);
			}
			continue;
		}

		const bool bShouldShowStoredWeapon = IsLeftBackStoredWeaponType(Weapon->GetWeaponType())
			? Weapon->GetWeaponType() == LastLeftBackWeaponType
			: bIsCurrentWeapon;
		Weapon->SetActorHiddenInGame(!bShouldShowStoredWeapon);
		if (bShouldShowStoredWeapon && !HughAnimState.bIsAiming)
		{
			AttachWeaponToStoredSocket(Weapon);
		}
	}
}

// 최근 왼쪽 등 선택 weapon이 제거됐으면 남아 있는 왼쪽 등 weapon으로 fallback
void AHughCharacter::NormalizeLastLeftBackWeaponType()
{
	const int32 LastLeftBackIndex = static_cast<int32>(LastLeftBackWeaponType);
	if (WeaponInventory.IsValidIndex(LastLeftBackIndex) && IsValid(WeaponInventory[LastLeftBackIndex]))
	{
		return;
	}

	const int32 TacticalIndex = static_cast<int32>(EWeaponType::Tactical);
	if (WeaponInventory.IsValidIndex(TacticalIndex) && IsValid(WeaponInventory[TacticalIndex]))
	{
		LastLeftBackWeaponType = EWeaponType::Tactical;
		return;
	}

	const int32 AttackIndex = static_cast<int32>(EWeaponType::Attack);
	if (WeaponInventory.IsValidIndex(AttackIndex) && IsValid(WeaponInventory[AttackIndex]))
	{
		LastLeftBackWeaponType = EWeaponType::Attack;
	}
}

// Attack/Tactical은 하나의 왼쪽 등 socket을 공유
bool AHughCharacter::IsLeftBackStoredWeaponType(EWeaponType WeaponType) const
{
	return WeaponType == EWeaponType::Attack || WeaponType == EWeaponType::Tactical;
}

// weapon slot별 비조준 수납 socket 선택
FName AHughCharacter::GetStoredWeaponSocketName(EWeaponType WeaponType) const
{
	switch (WeaponType)
	{
	case EWeaponType::Primary:
		return WaistArmWeaponAttachSocketName;
	case EWeaponType::Attack:
	case EWeaponType::Tactical:
		return LeftBackWeaponSocketName;
	case EWeaponType::Defense:
		return RightBackWeaponSocketName;
	default:
		return NAME_None;
	}
}

// weapon slot별 조준 시작 몽타주 선택. 오른쪽 등 몽타주가 비어 있으면 기존 왼쪽 등 몽타주로 fallback
UAnimMontage* AHughCharacter::GetDrawStoredWeaponMontage(EWeaponType WeaponType) const
{
	switch (WeaponType)
	{
	case EWeaponType::Primary:
		// return DrawWeaponFromWaistMontage.Get();
		return nullptr;
	case EWeaponType::Attack:
	case EWeaponType::Tactical:
		return DrawWeaponFromLeftBackMontage.Get();
	case EWeaponType::Defense:
		// return DrawWeaponFromRightBackMontage ? DrawWeaponFromRightBackMontage.Get() : DrawWeaponFromLeftBackMontage.Get();
		return nullptr;
	default:
		return nullptr;
	}
}

// weapon slot별 조준 종료 수납 몽타주 선택. 오른쪽 등 몽타주가 비어 있으면 기존 왼쪽 등 몽타주로 fallback
UAnimMontage* AHughCharacter::GetStoreWeaponMontage(EWeaponType WeaponType) const
{
	switch (WeaponType)
	{
	case EWeaponType::Attack:
	case EWeaponType::Tactical:
		return StoreWeaponToLeftBackMontage.Get();
	case EWeaponType::Defense:
		return StoreWeaponToRightBackMontage ? StoreWeaponToRightBackMontage.Get() : StoreWeaponToLeftBackMontage.Get();
	default:
		return nullptr;
	}
}

// Blueprint 입력 asset이 비어 있어도 C++ 기본 경로로 Hugh 조작을 유지
void AHughCharacter::ResolveInputActions()
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
			UE_LOG(LogTemp, Warning, TEXT("[HughCharacter::ResolveInputActions] Missing input action: %s"), Path);
		}
	};

	LoadActionIfMissing(MoveAction, TEXT("/Game/Blueprints/Player/Input/Hugh/IA_Move.IA_Move"));
	LoadActionIfMissing(LookAction, TEXT("/Game/Blueprints/Player/Input/Common/IA_Look.IA_Look"));
	LoadActionIfMissing(AttackAction, TEXT("/Game/Blueprints/Player/Input/Hugh/IA_Attack.IA_Attack"));
	LoadActionIfMissing(DodgeAction, TEXT("/Game/Blueprints/Player/Input/Hugh/IA_Dodge.IA_Dodge"));
	LoadActionIfMissing(JumpAction, TEXT("/Game/Blueprints/Player/Input/Hugh/IA_Jump.IA_Jump"));
	LoadActionIfMissing(AimAction, TEXT("/Game/Blueprints/Player/Input/Common/IA_Aim.IA_Aim"));
	LoadActionIfMissing(BoostAction, TEXT("/Game/Blueprints/Player/Input/Hugh/IA_Boost.IA_Boost"));
	LoadActionIfMissing(HeadVisorLightToggleAction, TEXT("/Game/Blueprints/Player/Input/Hugh/IA_HeadVisorLightToggle.IA_HeadVisorLightToggle"));
	LoadActionIfMissing(WeaponChangeAction, TEXT("/Game/Blueprints/Player/Input/Hugh/IA_ChangeWeapon.IA_ChangeWeapon"));
	LoadActionIfMissing(CycleWeaponAction, TEXT("/Game/Blueprints/Player/Input/Hugh/IA_CycleWeapon.IA_CycleWeapon"));
	LoadActionIfMissing(HealAction, TEXT("/Game/Blueprints/Player/Input/Hugh/IA_Heal.IA_Heal"));
}

// visor light 입력은 소유자에게 즉시 보이고 서버 복제로 다른 화면에 동기화
void AHughCharacter::OnToggleHeadVisorSpotlight(const FInputActionValue& /*Value*/)
{
	if (IsDead) return;
	const bool bNewEnabled = !bHeadVisorSpotlightEnabled;
	SetHeadVisorSpotlightEnabled(bNewEnabled);

	if (!HasAuthority())
	{
		Server_SetHeadVisorSpotlightEnabled(bNewEnabled);
	}
}

// non-primary weapon은 실제 보유 weapon과 남은 ammo가 있어야 선택 가능
bool AHughCharacter::IsWeaponSlotSelectable(int32 TargetWeaponIndex) const
{
	if (!WeaponInventory.IsValidIndex(TargetWeaponIndex))
	{
		return false;
	}

	if (TargetWeaponIndex == static_cast<int32>(EWeaponType::Primary))
	{
		return IsValid(WeaponInventory[TargetWeaponIndex]);
	}

	const AWeaponBase* TargetWeapon = WeaponInventory[TargetWeaponIndex];
	return IsValid(TargetWeapon) && TargetWeapon->GetCurAmmo() > 0;
}

// wheel 방향별 고정 순서에서 비어 있거나 ammo가 없는 slot은 건너뜀
int32 AHughCharacter::FindNextSelectableWeaponIndex(int32 Direction) const
{
	if (Direction == 0)
	{
		return INDEX_NONE;
	}

	static constexpr EWeaponType WeaponCycleOrder[] =
	{
		EWeaponType::Primary,
		EWeaponType::Tactical,
		EWeaponType::Defense,
		EWeaponType::Attack
	};
	static constexpr int32 WeaponCycleCount = UE_ARRAY_COUNT(WeaponCycleOrder);

	int32 CurrentOrderIndex = 0;
	for (int32 OrderIndex = 0; OrderIndex < WeaponCycleCount; ++OrderIndex)
	{
		if (static_cast<int32>(WeaponCycleOrder[OrderIndex]) == CurrentWeaponIndex)
		{
			CurrentOrderIndex = OrderIndex;
			break;
		}
	}

	const int32 Step = Direction > 0 ? 1 : -1;
	for (int32 Offset = 1; Offset < WeaponCycleCount; ++Offset)
	{
		const int32 NextOrderIndex = (CurrentOrderIndex + Step * Offset + WeaponCycleCount) % WeaponCycleCount;
		const int32 TargetWeaponIndex = static_cast<int32>(WeaponCycleOrder[NextOrderIndex]);
		if (IsWeaponSlotSelectable(TargetWeaponIndex))
		{
			return TargetWeaponIndex;
		}
	}

	return INDEX_NONE;
}

// weapon slot 선택을 current weapon과 visible stored weapon에 함께 반영
bool AHughCharacter::ChangeWeaponByIndex(int32 TargetWeaponIndex, bool bPlayAimDrawMontage)
{
	if (!WeaponInventory.IsValidIndex(TargetWeaponIndex))
	{
		return false;
	}

	if (!IsWeaponSlotSelectable(TargetWeaponIndex))
	{
		OnCurrentWeaponChanged.Broadcast(CurrentWeapon);
		return false;
	}

	AWeaponBase* TargetWeapon = WeaponInventory[TargetWeaponIndex];
	if (CurrentWeaponIndex == TargetWeaponIndex)
	{
		if (IsLeftBackStoredWeaponType(TargetWeapon->GetWeaponType()))
		{
			LastLeftBackWeaponType = TargetWeapon->GetWeaponType();
		}
		RefreshStoredWeaponPresentation();
		OnCurrentWeaponChanged.Broadcast(CurrentWeapon);
		return true;
	}

	if (IsLeftBackStoredWeaponType(TargetWeapon->GetWeaponType()))
	{
		LastLeftBackWeaponType = TargetWeapon->GetWeaponType();
	}

	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (bPlayAimDrawMontage && AnimInstance && HughAnimState.bIsAiming)
	{
		// 기존에 들고 있던 무기는 바로 수납공간에 부착
		AttachWeaponToStoredSocket(CurrentWeapon);

		if (UAnimMontage* DrawMontage = GetDrawStoredWeaponMontage(TargetWeapon->GetWeaponType()))
		{
			AnimInstance->Montage_Play(DrawMontage, 1.f);
		}
		else
		{
			//gripgun, decoygun은 바로 손에 부착
			if (TargetWeaponIndex == 0 || TargetWeaponIndex == 3)
			{
				//부착은 animnotify로 진행
				AnimInstance->Montage_Play(DrawWeaponFromWaistMontage, 1.f);
			}
		}
	}

	EquipWeapon(TargetWeapon);
	RefreshStoredWeaponPresentation();
	return true;
}

void AHughCharacter::OnStartWeaponChange(const FInputActionValue& Value)
{
	if (IsDead) return;
	const float InputMagnitude = Value.Get<float>();

	if (InputMagnitude <= 0.f) return;

	//1~4 -> 0~3
	int32 TargetWeaponIndex = FMath::RoundToInt(InputMagnitude) - 1;

	if (!HasAuthority())
	{
		Server_RequestWeaponChange(TargetWeaponIndex);
		return;
	}

	AWeaponBase* PreviousWeapon = CurrentWeapon;
	if (ChangeWeaponByIndex(TargetWeaponIndex, false))
	{
		CommitServerWeaponChangePresentation(PreviousWeapon, CurrentWeapon);
	}
}

// wheel 입력은 slot 번호가 아니라 방향만 보내므로 현재 보유 weapon 기준으로 다음 slot을 찾음
void AHughCharacter::OnCycleWeapon(const FInputActionValue& Value)
{
	if (IsDead) return;
	const float InputMagnitude = Value.Get<float>();
	if (FMath::IsNearlyZero(InputMagnitude))
	{
		return;
	}

	const int32 Direction = InputMagnitude > 0.f ? 1 : -1;
	const int32 TargetWeaponIndex = FindNextSelectableWeaponIndex(Direction);
	if (TargetWeaponIndex == INDEX_NONE)
	{
		return;
	}

	if (!HasAuthority())
	{
		Server_RequestWeaponChange(TargetWeaponIndex);
		return;
	}

	AWeaponBase* PreviousWeapon = CurrentWeapon;
	if (ChangeWeaponByIndex(TargetWeaponIndex, false))
	{
		CommitServerWeaponChangePresentation(PreviousWeapon, CurrentWeapon);
	}
}

void AHughCharacter::OnStartInteract(const FInputActionValue& Value)
{
	if (IsDead) return;
	AWeaponBase* PickupCandidate = OverlappingWeapon.Get();
	if (!IsValid(PickupCandidate)) return;

	if (HasAuthority())
	{
		if (PickUpWeapon(PickupCandidate))
		{
			Client_PlayPickupPresentation();
		}
		return;
	}

	Server_RequestPickup(PickupCandidate);
}

// heal hold는 local UI 진행률만 예측하고 실제 회복은 서버 RPC로 확정
void AHughCharacter::OnStartHeal(const FInputActionValue& /*Value*/)
{
	if (IsDead) return;
	if (!IsLocallyControlled())
	{
		return;
	}

	if (!CanUseHeal())
	{
		CancelHealHold();
		return;
	}

	bHealInputHeld = true;
	HealHeldSeconds = 0.f;
	NextHealUseSeconds = FMath::Max(UE_KINDA_SMALL_NUMBER, HealHoldThresholdSeconds);
	OnHealHoldChanged.Broadcast(GetHealHoldProgress(), bHealInputHeld);
}

// heal 입력 해제는 circular progress와 반복 실행 예약을 함께 정리
void AHughCharacter::OnCompleteHeal(const FInputActionValue& /*Value*/)
{
	CancelHealHold();
}

void AHughCharacter::OnHealSourceHealthChanged(float /*NewCurrentHealth*/, float /*NewMaxHealth*/)
{
	if (HasAuthority())
	{
		// 체력 보충은 death timer가 소유한 respawn lifecycle을 앞당기지 않음
		UpdateHealAvailability();
	}
}

void AHughCharacter::OnHealSourceCartridgeChanged(int32 /*NewRepairCartridge*/, int32 /*NewMaxRepairCartridge*/)
{
	if (HasAuthority())
	{
		UpdateHealAvailability();
	}
}

// 카메라 기준 이동 입력을 CharacterMovement에 넣어 prediction 경로로 전달
void AHughCharacter::OnTriggerMove(const FInputActionValue& Value)
{
	if (IsDead) return;
	//dash 모션이 재생중일 경우 종료 시킴
	if (!IsDashing)
	{
		UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
		if (!AnimInstance) return;
		//dash변수가 false인데 아직 dash 애니메이션 재생중일 경우 바로 해당 모션 종료
		if (DashMontage && AnimInstance->Montage_IsPlaying(DashMontage))
		{
			StopAnimMontage(DashMontage);
		}

		if (AimDashMontage && AnimInstance->Montage_IsPlaying(AimDashMontage))
		{
			StopAnimMontage(AimDashMontage);
		}

		//finish midairdash montage if is playing
		if (MidairDashMontage && AnimInstance->Montage_IsPlaying(MidairDashMontage))
		{
			StopAnimMontage(MidairDashMontage);
		}
	}

	const FVector2d MovementVector = Value.Get<FVector2D>();

	float InputX = MovementVector.X;
	float InputY = MovementVector.Y;
	float InputAngle = atan2(InputX, InputY);

	if (Controller == nullptr) return;

	const FRotator Rotation = Controller->GetControlRotation();
	const FRotator YawRotation(0.f, Rotation.Yaw, 0.f);
	const FVector ForwardDir = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDir = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	AddMovementInput(ForwardDir, InputY, false);
	AddMovementInput(RightDir, InputX, false);

}

// 이동 입력 종료 후 조준 여부에 맞춰 action state 복원
void AHughCharacter::OnCompleteMove(const FInputActionValue& Value)
{
	IsBraking = true;

	//초기 상태로 리셋
	//IntendedMovementState = EMovementState::Jog;
	//MovementState = EMovementState::Idle;
	//조준 중일땐 combat, 아닐땐 locomotion으로 전환
	ActionState = HughAnimState.bIsAiming ? EActionState::Combat : EActionState::Locomotion;
	RefreshReplicatedAnimState();

	if (GetCharacterMovement() && !HughAnimState.bIsAiming)
	{
		GetCharacterMovement()->bOrientRotationToMovement = false;
	}
}

// look 입력은 소유 클라이언트 camera/control rotation에만 즉시 반영
void AHughCharacter::OnTriggerLook(const FInputActionValue& Value)
{
	AddControllerYawInput(Value.Get<FVector2D>().X * (HughAnimState.bIsAiming ? AimSpeed : LookSpeed));
	AddControllerPitchInput(Value.Get<FVector2D>().Y * (HughAnimState.bIsAiming ? AimSpeed : LookSpeed));
}

// 사격 입력은 무기 발사와 로컬 recoil 상태를 갱신
void AHughCharacter::OnStartAttack(const FInputActionValue& Value)
{
	if (IsDead) return;
	if (!HughAnimState.bIsAiming || !CurrentWeapon || IsWeaponTriggerBlocked) return;

	//cannot shoot when cooling down
	if (CurrentWeapon->GetIsCoolingDown()) return;

	//return if no ammo
	if (CurrentWeapon->GetCurAmmo() <= 0) return;

	// 실제 사격과 damage는 server에서 확정
	if (HasAuthority())
	{
		CurrentWeapon->PullTrigger();
	}
	else
	{
		// client는 intent만 보내고 cooldown, ammo, damage를 server transaction에서 확정
		Server_RequestFire();
	}
}

// dash 입력은 조준 여부에 따라 montage section과 stamina 소모를 분기
void AHughCharacter::OnStartDash(const FInputActionValue& Value)
{
	if (IsDead) return;
	if (IsDashing || !GetCharacterMovement()) return;

	if (HasAuthority())
	{
		TryCommitServerDash();
		return;
	}

	Server_RequestDash();
}

// dodge montage hook은 이후 서버 검증형 회피 동작이 들어갈 위치
void AHughCharacter::OnStartDodge(const FInputActionValue& Value)
{
	//playanimmontage(dodgemontage)
}

// jump hold 동안 자동 hover 진입과 stamina 소모를 같은 상태값으로 유지
void AHughCharacter::OnTriggerJump(const FInputActionValue& Value)
{
	if (IsDead) return;
	if (!GetCharacterMovement()) return;

	//스태미나 바닥난 경우 바로 추락 처리
	if (CurStamina <= 0.f)
	{
		//호버링 중일 경우 호버링 해제 및 설정값들 초기화
		if (HughAnimState.bIsHovering)
		{
			SetHoveringState(false);
			SetJumpInputState(false);
		}
		return;
	}

	//이미 호버링 상태 & 점프키 계속 누르고 있을 경우 스태미나 감소
	//자동 호버링: 점프 후 최고 높이 도달 시 자동으로 호버링으로 전환
	if (!HughAnimState.bIsHovering && GetCharacterMovement()->IsFalling() && HughAnimState.bIsJumpInput)
	{
		if (GetCharacterMovement()->Velocity.Z <= 0.f)//정점 도달
		{
			SetHoveringState(true);
		}
	}

	//호버링이 유지되는 동안 스태미나 감소
	if (HughAnimState.bIsHovering)
	{
		float DeltaSeconds = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.016f;
		ConsumeStamina(StaminaConsumeSpeed * DeltaSeconds, false);
	}
}

// 점프 시작 입력을 예측 상태와 hover 진입 조건에 반영
void AHughCharacter::OnStartJump(const FInputActionValue& Value)
{
	if (IsDead) return;
	if (!GetCharacterMovement() ) return;

	SetJumpInputState(true);
	//점프 후 착지 시 미끄러지지않도록 deceleration값 증가시키기
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;


	//hover (이미 공중에 떠있는 상태에서 다시 점프키 눌렀을 때)
	if (GetCharacterMovement()->IsFalling())
	{
		//호버링 상태가 아니고 스태미나가 충분할 때
		if (!HughAnimState.bIsHovering && CurStamina > 0.f)
		{
			SetHoveringState(true);
		}
	}
	else if (!GetCharacterMovement()->IsFalling())//jump (그냥 점프)
	{
		Jump();
		ConsumeStamina(JumpStaminaConsumeAmount);
	}
}

// 점프 입력 해제 시 hover와 jump intent를 함께 정리
void AHughCharacter::OnCompleteJump(const FInputActionValue& Value)
{
	SetJumpInputState(false);

	//호버링 중일 경우 호버링 종료 및 중력값 원상복구
	if (HughAnimState.bIsHovering)
	{
		SetHoveringState(false);
	}
}

// 조준 시작은 달리기와 crosshair를 combat 상태에 맞춰 전환
void AHughCharacter::OnStartAim(const FInputActionValue& Value)
{
	if (IsDead) return;
	if (HughAnimState.bIsAiming || HughAnimState.bIsHovering || GetCharacterMovement()->IsFalling()) return;

	if (GetCharacterMovement() && GetCharacterMovement()->IsFalling())
		return;

	// weapon 복제 전에는 조준 장착 montage를 확정하지 않음
	const AWeaponBase* AimWeapon = CurrentWeapon.Get();
	if (!AimWeapon) return;

	//조준 시작하면 deceleration값 증가시키기
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;

	if (HughAnimState.bIsRunning)
		SetRunningState(false);


	SetAimingState(true);
	if (APragmataPlayerController* PragmataPlayerController = Cast<APragmataPlayerController>(GetController()))
	{
		PragmataPlayerController->SetCrosshairVisibility(true);
	}
	ActionState = EActionState::Combat;

	//aim 상태 돌입 시 캐릭터도 카메라가 바라보는 방향으로 회전해야 함
	//조준 중에는 이동방향으로 몸을 돌리는 옵션 끄기
	if (GetCharacterMovement())
	{
		//orient rotation(회전을 맞추기) to movement(움직이는 방향으로)
		//캐릭터가 이동하는 방향(키보드 누르는 방향)으로 몸을 자동으로 돌리게 할 것

		//작동방식: 캐릭터의 몸이 움직임과 독립이 됨. A키를 눌러도 캐릭터의 몸은 여전히 카메라가 보는 방향을 조준한 채로 몸만 왼쪽으로 이동(lockontarget할때 처럼)
		GetCharacterMovement()->bOrientRotationToMovement = false;
	}

	//무기 손에 부착
	if (WaistArmComponent && AimWeapon->GetWeaponType() == EWeaponType::Primary)
	{
		WaistArmComponent->SetArmState(EWaistArmState::Aim_Start);
	}
	//휴 손에 무기 부착시키기
	// if (CurrentWeapon)
	// {
	// 	CurrentWeapon->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, RightHandSocketName);
	// }

	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (!AnimInstance) return;
	//조준 시 각 무기별로 부착 혹은 애니메이션 재생
	switch(AimWeapon->GetWeaponType())
	{
	case EWeaponType::Primary://gripgun
		//AnimInstance->Montage_Play(DrawWeaponFromWaistMontage, 1.f);
		//gripgun은 조준 시 바로 손에 부착
		AttachWeaponToSocket(RightHandSocketName);
		break;
	case EWeaponType::Defense://decoygun
		//무기 줍는 모션 재생중일 경우 애니메이션 취소, animnotifystate 끝내고 notifyend호출, 오른손에 부착
		if (AnimInstance->Montage_IsPlaying(PickUpWeaponMontage))
		{
			AnimInstance->Montage_Stop(0.1f, PickUpWeaponMontage);
		}
		AttachWeaponToSocket(RightHandSocketName);
		break;
	case EWeaponType::Attack://shotgun
		if (AnimInstance->Montage_IsPlaying(PickUpWeaponMontage))
		{
			UE_LOG(LogTemp, Warning, TEXT("Shotgun pickup Montage Stoppped"));
			AnimInstance->Montage_Stop(0.1f, PickUpWeaponMontage);
		}
		AttachWeaponToSocket(RightHandSocketName);
		break;
	case EWeaponType::Tactical:
		break;
	}
	if (UAnimMontage* DrawMontage = GetDrawStoredWeaponMontage(AimWeapon->GetWeaponType()))
	{
		AnimInstance->Montage_Play(DrawMontage, 1.f);
	}
	
	if (IsLocallyControlled() && OnStartAimSFX)
	{
		UGameplayStatics::PlaySound2D(this, OnStartAimSFX);
	}
}

// 조준 종료는 crosshair와 waist-arm attach를 locomotion 상태로 복귀
void AHughCharacter::OnCompleteAim(const FInputActionValue& Value)
{
	//이미 조준 해제된 경우
	if (!HughAnimState.bIsAiming) return;

	SetAimingState(false);
	if (APragmataPlayerController* PragmataPlayerController = Cast<APragmataPlayerController>(GetController()))
	{
		PragmataPlayerController->SetCrosshairVisibility(false);
	}
	ActionState = EActionState::Locomotion;

	//조준 완료 시 다시 원상복구
	if (GetCharacterMovement())
	{
		//작동방식: 카메라는 가만히 있어도 A를 누르면 캐릭터가 왼쪽을 보고 이동함.
		GetCharacterMovement()->bOrientRotationToMovement = true;

		//aim 끝나면 deceleration값 원상복구 시키기
		GetCharacterMovement()->BrakingDecelerationWalking = DefaultBreakingDeceleration;
	}

	//로봇팔 애니메이션 재생
	const AGripGun* GripGun = WeaponInventory.IsValidIndex(0) ? Cast<AGripGun>(WeaponInventory[0]) : nullptr;
	if (WaistArmComponent && (!GripGun || !GripGun->GetIsReloading()))
	{
		WaistArmComponent->SetArmState(EWaistArmState::Aim_End);
		WaistArmComponent->PlayArm_AimEndMontage(1.f);
	}

	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	//로봇팔에 무기 부착시키기(gripgun) 및 무기 수납 애니메이션(shotgun 등) 재생
	if (CurrentWeapon && AnimInstance)
	{
		switch (CurrentWeapon->GetWeaponType())
		{
		case EWeaponType::Primary:
			//gripgun은 바로 waistarm에 부착
			//재장전 중일 경우 blendout에서 로봇팔에 부착하기 진행
			if (!AnimInstance->Montage_IsPlaying(ReloadGripgunMontage))
				CurrentWeapon->AttachToComponent(WaistArmComponent->GetWaistArmMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, WaistArmWeaponAttachSocketName);
			break;
		case EWeaponType::Attack:
		case EWeaponType::Tactical:
		case EWeaponType::Defense:
			//샷건 무기 부착은 animnotify에서 진행
			//에임 풀었을때 만약 탄약을 다 썼다면 무기 버리기
			if (CurrentWeapon->GetCurAmmo() <= 0 && ThrowAwayWeaponMontage)
				AnimInstance->Montage_Play(ThrowAwayWeaponMontage);
			else if (UAnimMontage* StoreMontage = GetStoreWeaponMontage(CurrentWeapon->GetWeaponType()))
				AnimInstance->Montage_Play(StoreMontage);
			break;
		}
	}
}

// boost 유지 입력을 달리기 intent로 예측 적용
void AHughCharacter::OnTriggerBoost(const FInputActionValue& Value)
{
	if (HughAnimState.bIsAiming) return;

	SetRunningState(true);
}

// boost 해제 시 달리기 intent를 서버 상태와 맞춤
void AHughCharacter::OnCompleteBoost(const FInputActionValue& Value)
{
	if (HughAnimState.bIsAiming) return;

	SetRunningState(false);
}

// run action은 boost와 같은 달리기 intent 경로를 공유
void AHughCharacter::OnTriggerRun(const FInputActionValue& Value)
{
	if (IsDead) return;
	if (HughAnimState.bIsAiming) return;

	SetRunningState(true);
}

// run 해제 시 prediction flag와 replicated anim state를 함께 내림
void AHughCharacter::OnCompleteRun(const FInputActionValue& Value)
{
	if (HughAnimState.bIsAiming) return;

	SetRunningState(false);
}

// custom movement flag로 도착한 intent를 서버 authoritative 상태로 확정
void AHughCharacter::SyncMovementIntentFromMovementComponent()
{
	//클라에서 서버로 이동을 예측
	//서버의 허락을 기다리지 않고 플레이어가 입력한 input값대로 aim 및 run을 하고 싶을 때
	//내 컴퓨터가 먼저 예측해서 캐릭터를 움직이게 함.
	UPragmataHughMovementComponent* HughMovement = Cast<UPragmataHughMovementComponent>(GetCharacterMovement());
	if (!HughMovement || (!HasAuthority() && !IsLocallyControlled()))
	{
		return;
	}

	if (HughMovement->WantsToAim() != HughAnimState.bIsAiming)
	{
		SetAimingState(HughMovement->WantsToAim());
	}

	if (HughMovement->WantsToRun() != HughAnimState.bIsRunning)
	{
		SetRunningState(HughMovement->WantsToRun());
	}

	if (HughMovement->IsJumpInputHeld() != HughAnimState.bIsJumpInput)
	{
		SetJumpInputState(HughMovement->IsJumpInputHeld());
	}

	if (HughMovement->WantsToHover() != HughAnimState.bIsHovering)
	{
		if (!HughMovement->WantsToHover() || HughMovement->IsFalling() || HughMovement->MovementMode == MOVE_Flying)
		{
			SetHoveringState(HughMovement->WantsToHover());
		}
	}

	if (HasAuthority() && !IsLocallyControlled() && HughAnimState.bIsHovering && HughAnimState.bIsJumpInput)
	{
		const float DeltaSeconds = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.016f;
		ConsumeStamina(StaminaConsumeSpeed * DeltaSeconds, false);
	}
}

// 소유자와 서버는 이동 상태를 계산하고 원격 프록시는 복제값만 사용
void AHughCharacter::UpdateMovementState()
{
	if (!HasAuthority() && !IsLocallyControlled())
	{
		ActionState = HughAnimState.ActionState;
		MovementState = HughAnimState.MovementState;
		if (UHughAnimInstance* AnimInstance = Cast<UHughAnimInstance>(GetMesh()->GetAnimInstance()))
		{
			AnimInstance->SetActionState(HughAnimState.ActionState);
		}
		return;
	}

	//캐릭터의 가속도가 0이 아닐 경우 움직이려 하는 것
	bIsTryingToMove = GetCharacterMovement() && !GetCharacterMovement()->GetCurrentAcceleration().IsNearlyZero();

	if (bIsTryingToMove)
	{
		if (HughAnimState.bIsRunning)
			MovementState = EMovementState::Run;
		else if (HughAnimState.bIsAiming)
			MovementState = EMovementState::Walk;
		else
			MovementState = EMovementState::Jog;

		UpdateMovementSpeed();
	}
	else
	{
		MovementState = EMovementState::Idle;
	}

	UpdateMovementSpeed();
	RefreshReplicatedAnimState();
}

// 애니메이션 이동 상태와 실제 CharacterMovement 속도를 같은 값으로 유지
void AHughCharacter::UpdateMovementSpeed()
{
	switch (MovementState)
	{
	case EMovementState::Idle:
		// GetCharacterMovement()->MaxWalkSpeed = 0.f;
		break;
	case EMovementState::Walk:
		GetCharacterMovement()->MaxWalkSpeed = 200.f;
		break;
	case EMovementState::Jog:
		GetCharacterMovement()->MaxWalkSpeed = 400.f;
		break;
	case EMovementState::Run:
		GetCharacterMovement()->MaxWalkSpeed = 600.f;
		break;
	}
}

void AHughCharacter::WeaponRecoil(float DeltaSeconds)
{
	if (!CurrentWeapon) return;

	float RecoilData = CurrentWeapon->GetRecoilData(DeltaSeconds);

	// local camera feedback은 현재 possession controller 타입과 무관하게 적용
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (PC && PC->IsLocalController())
	{
		FRotator CurrentRotation = PC->GetControlRotation();
		FRotator RecoilRotator(RecoilData, 0.f, 0.f);
		PC->SetControlRotation(CurrentRotation + RecoilRotator);
	}
}

// 조준 상태 변경을 movement prediction, 로봇팔, 무기 attach에 동시에 반영
void AHughCharacter::SetAimingState(bool bNewAiming)
{
	if (HughAnimState.bIsAiming == bNewAiming)
	{
		return;
	}

	HughAnimState.bIsAiming = bNewAiming;
	ActionState = HughAnimState.bIsAiming ? EActionState::Combat : EActionState::Locomotion;
	if (UPragmataHughMovementComponent* HughMovement = Cast<UPragmataHughMovementComponent>(GetCharacterMovement()))
	{
		// 이동 예측에 aim intent 저장
		HughMovement->SetWantsToAim(HughAnimState.bIsAiming);
		if (HughAnimState.bIsAiming)
		{
			HughMovement->SetWantsToRun(false);
		}
	}

	if (GetCharacterMovement())
	{
		GetCharacterMovement()->bOrientRotationToMovement = !HughAnimState.bIsAiming;
	}

	// weapon pointer가 늦게 도착한 client state sync에서도 arm 처리 crash 방지
	// if (WaistArmComponent && CurrentWeapon && CurrentWeapon->GetWeaponType() == EWeaponType::Primary)s
	// {
	// 	//로봇팔 애니메이션 재생
	// 	WaistArmComponent->SetArmState(HughAnimState.bIsAiming ? EWaistArmState::Aim_Start : EWaistArmState::Aim_End);
	// 	WaistArmComponent->PlayArm_AimEndMontage(1.f);
	// }
	RefreshReplicatedAnimState();
}

// 달리기 intent를 조준 상태와 충돌하지 않게 저장
void AHughCharacter::SetRunningState(bool bNewRunning)
{
	const bool bCanRun = bNewRunning && !HughAnimState.bIsAiming;
	if (HughAnimState.bIsRunning == bCanRun)
	{
		return;
	}

	HughAnimState.bIsRunning = bCanRun;
	if (UPragmataHughMovementComponent* HughMovement = Cast<UPragmataHughMovementComponent>(GetCharacterMovement()))
	{
		HughMovement->SetWantsToRun(HughAnimState.bIsRunning);
	}

	RefreshReplicatedAnimState();
}

// jump held 상태를 hover 판정과 movement prediction에 공유
void AHughCharacter::SetJumpInputState(bool bNewJumpInput)
{
	if (HughAnimState.bIsJumpInput == bNewJumpInput)
	{
		return;
	}

	HughAnimState.bIsJumpInput = bNewJumpInput;
	if (UPragmataHughMovementComponent* HughMovement = Cast<UPragmataHughMovementComponent>(GetCharacterMovement()))
	{
		HughMovement->SetJumpInputHeld(HughAnimState.bIsJumpInput);
	}

	RefreshReplicatedAnimState();
}

// hover 진입과 이탈을 이동 모드와 복제 애니메이션 상태에 반영
void AHughCharacter::SetHoveringState(bool bNewHovering)
{
	if (HughAnimState.bIsHovering == bNewHovering)
	{
		return;
	}

	HughAnimState.bIsHovering = bNewHovering;
	if (UPragmataHughMovementComponent* HughMovement = Cast<UPragmataHughMovementComponent>(GetCharacterMovement()))
	{
		HughMovement->SetWantsToHover(HughAnimState.bIsHovering);
	}

	if (GetCharacterMovement())
	{
		if (HughAnimState.bIsHovering)
		{
			//호버링 상태 진입
			//호버링을 시작한 순간의 캐릭터 높이값(Z값) 저장
			HoverTargetZ = GetActorLocation().Z;
			//movementmode 비행모드로 변경
			GetCharacterMovement()->SetMovementMode(MOVE_Flying);
			// GetCharacterMovement()->GravityScale = HoverGravityScale;
			//중력값 변경 대신 상하 속도를 0으로 초기화
			GetCharacterMovement()->Velocity.Z = 0.f;
		}
		else
		{
			GetCharacterMovement()->GravityScale = DefaultGravityScale;
			if (GetCharacterMovement()->MovementMode == MOVE_Flying)
			{
				GetCharacterMovement()->SetMovementMode(MOVE_Falling);
			}
		}
	}

	RefreshReplicatedAnimState();
	
	if (IsLocallyControlled() && HoveringSFX)
	{
		if (bNewHovering)
		{
			if (HoveringAudioComponent &&  HoveringAudioComponent->IsPlaying())
			{
				return;
			}
			HoveringAudioComponent = UGameplayStatics::SpawnSound2D(this, HoveringSFX);
		}
		else
		{
			if (HoveringAudioComponent && HoveringAudioComponent->IsPlaying())
			{
				HoveringAudioComponent->FadeOut(0.5f, 0.f);
			}
		}
	}
}

// 서버와 소유 클라이언트의 계산값을 animation snapshot에 갱신
void AHughCharacter::RefreshReplicatedAnimState()
{
	HughAnimState.ActionState = ActionState;
	HughAnimState.MovementState = MovementState;
}

// 원격 프록시가 복제 snapshot으로 로컬 표시 부작용을 재구성
void AHughCharacter::ApplyReplicatedAnimState(const FHughReplicatedAnimState& OldHughAnimState)
{
	ActionState = HughAnimState.ActionState;
	MovementState = HughAnimState.MovementState;
	IsDashing = HughAnimState.bIsDashing;
	if (HughAnimState.bIsDashing && HughAnimState.DashActionSerial != OldHughAnimState.DashActionSerial)
	{
		PlayDashPresentation(
			HughAnimState.DashPresentationMode,
			HughAnimState.DashSectionName,
			HughAnimState.DashActionSerial);
	}
	if (HughAnimState.bWeaponChangeActive
		&& HughAnimState.WeaponChangeActionSerial != OldHughAnimState.WeaponChangeActionSerial)
	{
		const int32 TargetIndex = static_cast<int32>(HughAnimState.WeaponChangeTargetType);
		const int32 PreviousIndex = static_cast<int32>(HughAnimState.WeaponChangePreviousType);
		AWeaponBase* TargetWeapon = WeaponInventory.IsValidIndex(TargetIndex) ? WeaponInventory[TargetIndex] : nullptr;
		AWeaponBase* PreviousWeapon = WeaponInventory.IsValidIndex(PreviousIndex) ? WeaponInventory[PreviousIndex] : nullptr;
		PlayWeaponChangePresentation(PreviousWeapon, TargetWeapon, HughAnimState.WeaponChangeActionSerial);
	}
	if (HughAnimState.WeaponDepletionActionSerial != OldHughAnimState.WeaponDepletionActionSerial)
	{
		PlayWeaponDepletionPresentation(HughAnimState.WeaponDepletionType, HughAnimState.WeaponDepletionActionSerial);
	}

	if (GetCharacterMovement())
	{
		GetCharacterMovement()->bOrientRotationToMovement = !HughAnimState.bIsAiming;
	}

	// remote proxy는 replicated aim state에서 최종 arm/socket 상태를 재구성
	if (WaistArmComponent)
	{
		WaistArmComponent->SetArmState(HughAnimState.bIsAiming ? EWaistArmState::Aim_Start : EWaistArmState::Aim_End);
		if (OldHughAnimState.bIsAiming && !HughAnimState.bIsAiming)
		{
			WaistArmComponent->PlayArm_AimEndMontage(1.f);
		}
	}

	if (HughAnimState.bIsAiming)
	{
		AttachWeaponToSocket(RightHandSocketName);
	}
	else
	{
		RefreshStoredWeaponPresentation();
	}
}

void AHughCharacter::EquipWeapon(AWeaponBase* Weapon)
{
	if (!Weapon) return;

	//현재 장착한 무기와 다른 무기일 경우
	if (CurrentWeapon && CurrentWeapon != Weapon && CurrentWeapon->GetWeaponType() != EWeaponType::Primary)
	{
		CurrentWeapon->OnAmmoDepletion.RemoveDynamic(this, &AHughCharacter::HandleAmmoDepletion);
	}

	CurrentWeapon = Weapon;
	CurrentWeaponIndex = static_cast<int>(CurrentWeapon->GetWeaponType());
	
	if (CurrentWeapon->GetWeaponType() != EWeaponType::Primary)
	{
		CurrentWeapon->OnAmmoDepletion.AddUniqueDynamic(this, &AHughCharacter::HandleAmmoDepletion);
	}

	OnCurrentWeaponChanged.Broadcast(CurrentWeapon);
}

void AHughCharacter::DropWeapon()
{
	if (!HasAuthority()) return;

	if (CurrentWeapon)
	{
		CurrentWeapon->DropWeapon();
		if (WeaponInventory.IsValidIndex(CurrentWeaponIndex))
		{
			WeaponInventory[CurrentWeaponIndex] = nullptr;//기존에 들고 있던 무기(drop한 무기) 인벤토리에서 제거하기
		}

		const int32 PrimaryWeaponIndex = static_cast<int32>(EWeaponType::Primary);
		if (WeaponInventory.IsValidIndex(PrimaryWeaponIndex) && IsValid(WeaponInventory[PrimaryWeaponIndex]))
		{
			EquipWeapon(WeaponInventory[PrimaryWeaponIndex]);//default로 gripgun 장착
		}
		if (HughAnimState.bIsAiming)
			AttachWeaponToSocket(RightHandSocketName);
		RefreshStoredWeaponPresentation();
		OnWeaponInventoryChanged.Broadcast();
	}
}

bool AHughCharacter::PickUpWeapon(AWeaponBase* PickupWeapon)
{
	if (!HasAuthority() || !IsValid(PickupWeapon) || OverlappingWeapon != PickupWeapon)
	{
		return false;
	}

	constexpr float PickupValidationDistance = 200.f;
	if (FVector::DistSquared(GetActorLocation(), PickupWeapon->GetActorLocation())
		> FMath::Square(PickupValidationDistance))
	{
		return false;
	}

	if (PickupWeapon->GetOwner() && PickupWeapon->GetOwner() != this)
	{
		return false;
	}

	const int32 PickupWeaponIndex = static_cast<int32>(PickupWeapon->GetWeaponType());
	if (!WeaponInventory.IsValidIndex(PickupWeaponIndex))
	{
		return false;
	}

	//예외처리: 이미 갖고 있는 무기면 탄약만 증가시키고 해당 무기액터 삭제
	if (AWeaponBase* CorrespondingWeapon = WeaponInventory[PickupWeaponIndex])
	{
		if (CorrespondingWeapon->AddAmmo(CorrespondingWeapon->GetIncreaseAmount()))
		{
			PickupWeapon->Destroy();
			OverlappingWeapon = nullptr;
			ForceNetUpdate();
			return true;
		}
		return false;
	}
	
	EquipWeapon(OverlappingWeapon);
	
	FName AttachSocketName = LeftBackWeaponSocketName;
	//무기 부착 처리, set owner, disable collision
	if (CurrentWeaponIndex == 3)
		AttachSocketName = WaistArmWeaponAttachSocketName;
	
	if (CurrentWeapon == nullptr) return false;
	
	CurrentWeapon->StoreWeapon(this, AttachSocketName);
	
	if (CurrentWeapon->GetWeaponType() == EWeaponType::Primary)
	{
		if (WaistArmComponent)
		{
			CurrentWeapon->AttachToComponent(WaistArmComponent->GetWaistArmMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, WaistArmWeaponAttachSocketName);
		}
	}
	else if (const FName StoreSocketName = GetStoredWeaponSocketName(CurrentWeapon->GetWeaponType()); StoreSocketName != NAME_None)
	{
		AttachWeaponToSocket(StoreSocketName);
	}

	int iWeaponIndex = static_cast<int>(CurrentWeapon->GetWeaponType());
	if (WeaponInventory.IsValidIndex(iWeaponIndex) && WeaponInventory[iWeaponIndex] == nullptr)
	{
		WeaponInventory[iWeaponIndex] = CurrentWeapon;
	}

	if (IsLeftBackStoredWeaponType(CurrentWeapon->GetWeaponType()))
	{
		LastLeftBackWeaponType = CurrentWeapon->GetWeaponType();
	}

	RefreshStoredWeaponPresentation();
	OnWeaponInventoryChanged.Broadcast();
	OverlappingWeapon = nullptr;
	ForceNetUpdate();
	
	//tutorial
	FName TutorialID = NAME_None;
	switch (iWeaponIndex)
	{
	case 1://shotgun
		TutorialID = TEXT("Shotgun.FirstAcquired");
		break;
	case 2://stasis
		TutorialID = TEXT("Stasis.FirstAcquired");
		break;
	case 3://decoy
		TutorialID = TEXT("Decoy.FirstAcquired");
		break;
	}
	const bool bFirstAcquired = CurrentWeapon->GetTutorialStepData()
		&& TutorialID != NAME_None
		&& UTutorialBlueprintLibrary::ShowTutorialOnce(this, TutorialID, CurrentWeapon->GetTutorialStepData());
	if (bFirstAcquired)
	{
		if (APragmataGameState* GameState = GetWorld() ? GetWorld()->GetGameState<APragmataGameState>() : nullptr)
		{
			if (ADianaOperatorPawn* DianaPawn = GameState->GetDianaOperatorPawn())
			{
				switch (CurrentWeapon->GetWeaponType())
				{
				case EWeaponType::Attack:
					DianaPawn->GetVoiceComponent()->PlayVoiceAction(EDianaVoiceAction::ShockwaveGunObtain);
					break;
				case EWeaponType::Tactical:
					DianaPawn->GetVoiceComponent()->PlayVoiceAction(EDianaVoiceAction::StasisNetObtain);
					break;
				case EWeaponType::Defense:
					DianaPawn->GetVoiceComponent()->PlayVoiceAction(EDianaVoiceAction::DecoyGeneratorObtain);
					break;
				default:
					break;
				}
			}
		}
	}
	
	return true;
}

// hover가 아닐 때만 stamina 회복을 지연시켜 점프/hover 루프의 리듬을 유지
void AHughCharacter::RecoverStamina(float DeltaSeconds)
{
	if (!GetWorld()) return;

	//호버링 중이거나 스태미나가 이미 최대치면 회복 X
	if (HughAnimState.bIsHovering || CurStamina >= MaxStamina) return;

	const float CurTime = GetWorld()->GetTimeSeconds();
	const float TimeSinceLastConsume = CurTime - LastStaminaConsumedTime;

	if (TimeSinceLastConsume >= StaminaRecoveryDelay)
	{
		CurStamina += StaminaRecoverySpeed * DeltaSeconds;
		if (CurStamina > MaxStamina)
			CurStamina = MaxStamina;
	}
}

// stamina 소모 시점을 기록해 회복 delay가 클라이언트 입력에 흔들리지 않게 함
void AHughCharacter::ConsumeStamina(float Stamina, bool bRequestServerSpend)
{
	if (Stamina <= 0.f) return;

	CurStamina -= Stamina;
	if (CurStamina < 0.f)
	{
		CurStamina = 0.f;
	}

	if (GetWorld())
	{
		LastStaminaConsumedTime = GetWorld()->GetTimeSeconds();
	}

	if (bRequestServerSpend && !HasAuthority() && IsLocallyControlled())
	{
		Server_RequestConsumeStamina(Stamina);
	}
}

// stamina HUD delegate는 현재 소모 가능 여부까지 같이 전달
void AHughCharacter::BroadcastStaminaChanged()
{
	// 잠금(빨강) = 소모 불가. IsStaminaConsumable이 false일 때 빨갛게.
	OnStaminaChanged.Broadcast(CurStamina, MaxStamina, !IsStaminaConsumable);
}

// 회복 입력은 수량, HP, 이동 blocker가 모두 통과할 때만 허용
bool AHughCharacter::CalculateHealAvailability() const
{
	const UPragmataHealthComponent* HughHealthComponent = GetHealthComponent();
	const UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
	const AGripGun* GripGun = WeaponInventory.IsValidIndex(0) ? Cast<AGripGun>(WeaponInventory[0]) : nullptr;
	return PlayerStatsComponent
		&& HughHealthComponent
		&& PlayerStatsComponent->GetRemainingHealCartridges() > 0
		&& HughHealthComponent->GetCurrentHealth() < HughHealthComponent->GetMaxHealth()
		&& !HughHealthComponent->IsDead()
		&& !IsDead
		&& (!GripGun || !GripGun->GetIsReloading())
		&& !IsDashing
		&& !HughAnimState.bIsRunning
		&& !HughAnimState.bIsHovering
		&& (!MovementComponent || !MovementComponent->IsFalling());
}

// 서버 authoritative 가능 상태를 복제해 양쪽 HUD가 같은 blocker를 보게 함
void AHughCharacter::UpdateHealAvailability()
{
	const bool bNewCanUseHeal = CalculateHealAvailability();
	if (bCanUseHeal == bNewCanUseHeal)
	{
		return;
	}

	bCanUseHeal = bNewCanUseHeal;
	BroadcastHealAvailabilityChanged();
}

// 복제 상태와 현재 cartridge 수량을 HUD ViewModel으로 전달
void AHughCharacter::BroadcastHealAvailabilityChanged()
{
	OnHealAvailabilityChanged.Broadcast(bCanUseHeal, GetRemainingHealCartridges());
	if (IsLocallyControlled() && !bCanUseHeal)
	{
		CancelHealHold();
	}
}

// hold threshold 이후에는 같은 입력 유지 중 1초 간격으로 서버에 사용 요청
void AHughCharacter::UpdateHealHold(float DeltaSeconds)
{
	if (!bHealInputHeld)
	{
		return;
	}

	if (!CanUseHeal())
	{
		CancelHealHold();
		return;
	}

	HealHeldSeconds += FMath::Max(0.f, DeltaSeconds);
	OnHealHoldChanged.Broadcast(GetHealHoldProgress(), bHealInputHeld);

	if (HealHeldSeconds + UE_KINDA_SMALL_NUMBER >= NextHealUseSeconds)
	{
		RequestHealUse();
		NextHealUseSeconds += FMath::Max(UE_KINDA_SMALL_NUMBER, HealRepeatIntervalSeconds);
		if (!CanUseHeal())
		{
			CancelHealHold();
		}
	}
}

// blocker 발생 또는 입력 해제 시 circular progress를 즉시 비움
void AHughCharacter::CancelHealHold()
{
	const bool bWasHolding = bHealInputHeld;
	const float PreviousProgress = GetHealHoldProgress();
	bHealInputHeld = false;
	HealHeldSeconds = 0.f;
	NextHealUseSeconds = 0.f;
	if (bWasHolding || PreviousProgress > 0.f)
	{
		OnHealHoldChanged.Broadcast(0.f, false);
	}
}

void AHughCharacter::RequestHealUse()
{
	// listen server와 client 모두 같은 서버 검증 경로로 회복을 요청
	if (HasAuthority())
	{
		if (CalculateHealAvailability() && PlayerStatsComponent)
		{
			PlayerStatsComponent->ConsumeRepairCartridge();
			UpdateHealAvailability();
		}
		return;
	}

	Server_RequestHeal();
}

// remote HUD는 replicated snapshot 도착 시점에 stamina 표시를 갱신
void AHughCharacter::OnRep_StaminaSnapshot()
{
	BroadcastStaminaChanged();
}

void AHughCharacter::OnRep_CanUseHeal()
{
	BroadcastHealAvailabilityChanged();
}

// replicated weapon pointer가 늦게 도착해도 HUD ammo source를 다시 연결
void AHughCharacter::OnRep_CurrentWeapon()
{
	if (!CurrentWeapon)
	{
		OnCurrentWeaponChanged.Broadcast(nullptr);
		return;
	}

	EquipWeapon(CurrentWeapon);
	if (HughAnimState.bIsAiming)
	{
		AttachWeaponToSocket(RightHandSocketName);
	}
	else
	{
		RefreshStoredWeaponPresentation();
	}
}

// inventory pointer 복제 후 현재 선택된 weapon만 수납 소켓에 표시
void AHughCharacter::OnRep_WeaponInventory()
{
	RefreshStoredWeaponPresentation();
	OnWeaponInventoryChanged.Broadcast();
	if (const AGripGun* GripGun = WeaponInventory.IsValidIndex(0) ? Cast<AGripGun>(WeaponInventory[0]) : nullptr;
		GripGun && GripGun->GetIsReloading())
	{
		ApplyGripGunReloadPresentation(true);
	}
	if (HughAnimState.bWeaponChangeActive
		&& HughAnimState.WeaponChangeActionSerial != LastPlayedWeaponChangeActionSerial)
	{
		const int32 TargetIndex = static_cast<int32>(HughAnimState.WeaponChangeTargetType);
		const int32 PreviousIndex = static_cast<int32>(HughAnimState.WeaponChangePreviousType);
		PlayWeaponChangePresentation(
			WeaponInventory.IsValidIndex(PreviousIndex) ? WeaponInventory[PreviousIndex] : nullptr,
			WeaponInventory.IsValidIndex(TargetIndex) ? WeaponInventory[TargetIndex] : nullptr,
			HughAnimState.WeaponChangeActionSerial);
	}
}

void AHughCharacter::OnRep_StoredWeaponPresentationState()
{
	RefreshStoredWeaponPresentation();
}

// 회전 변화량을 lean 값으로 변환해 AnimBP banking에 전달
void AHughCharacter::CalculateRotation(float DeltaSeconds)
{
	//마우스 회전값에 따른 캐릭터 회전값 계산
	//마우스 왼쪽으로 틀때 캐릭터가 몸을 왼쪽으로 기울수 있도록 기우는 정도를 계산

	//속도를 구하려면 이전 프레임과 현재 프레임의 각도 차를 구해야 함
	FRotator CurrentRotation = GetActorRotation();
	if (LastRotation.IsZero()) LastRotation = CurrentRotation;

	//각도의 차이: 마우스를 왼쪽으로 돌려 캐릭터가 1도 움직였다면 deltarotation.yaw의 값은 1.0이 됨
	//getnormalized를 통해 -180에서 180사이로 값을 보정
	FRotator DeltaRotation = (CurrentRotation - LastRotation).GetNormalized();

	//-1과 1사이 값으로 스케일을 축소
	float TargetLean = 0.f;
	if (GetMovementSpeed() > WalkThreshold)
	{
		TargetLean = FMath::Clamp((DeltaRotation.Yaw / DeltaSeconds) * 0.1f, -1.f, 1.f);
	}
	//보간
	LeanAmount = FMath::FInterpTo(LeanAmount, TargetLean, DeltaSeconds, RotateSpeed);
	//값 update
	LastRotation = CurrentRotation;
}

// 조준 이동 방향 angle을 AnimBP가 읽는 단일 값으로 계산
void AHughCharacter::CalculateAngle()
{
	//조준 시 필요한 움직임 각도 계산(왼쪽 -90도, 오른쪽 90도 등등)
	Angle = UKismetAnimationLibrary::CalculateDirection(GetVelocity() ,GetActorRotation());
}

void AHughCharacter::PrintDebugMsg()
{
	//__DEBUG
	if (GEngine)
	{
		// 2. 소수점 둘째 자리까지 깔끔하게 포맷팅합니다.
		const FString SpeedMessage = FString::Printf(TEXT("Stamina: %.2f"), CurStamina);
		GEngine->ClearOnScreenDebugMessages();
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan, SpeedMessage);

		// const FString FallingMessage = FString::Printf(TEXT("IsFalling: %d"), GetCharacterMovement()->IsFalling());
		// GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan, FallingMessage);

		const FString LeanMsg = FString::Printf(TEXT("LeanAMount: %.2f"), LeanAmount);
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan, LeanMsg);

		const FString SpeedMsg = FString::Printf(TEXT("Speed: %.2f"), GetVelocity().Size2D());
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan, SpeedMsg);

		const FString DashMsg = FString::Printf(TEXT("Dash: %d"), IsDashing);
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan, DashMsg);

		const AGripGun* DebugGripGun = WeaponInventory.IsValidIndex(0) ? Cast<AGripGun>(WeaponInventory[0]) : nullptr;
		const FString ReloadMsg = FString::Printf(TEXT("Reload: %d"), DebugGripGun && DebugGripGun->GetIsReloading());
        GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan, ReloadMsg);

		// weapon pointer 복제가 늦은 client tick에서도 debug 출력 crash 방지
		const AWeaponBase* DebugWeapon = IsValid(CurrentWeapon) ? CurrentWeapon.Get() : nullptr;
		const FString AmmoMsg = DebugWeapon
			? FString::Printf(TEXT("Ammo: %d"), DebugWeapon->GetCurAmmo())
			: FString(TEXT("Ammo: None"));
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan, AmmoMsg);

		const FString CooldownMsg = DebugWeapon
			? FString::Printf(TEXT("IsCoolingDown: %d"), DebugWeapon->GetIsCoolingDown())
			: FString(TEXT("IsCoolingDown: None"));
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan, CooldownMsg);
	}
}

// 착지 시 hover 이동 모드와 스태미나 회복 상태를 정리
void AHughCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);
	//바닥 착지 시 호버상태를 해제
	SetHoveringState(false);
	//중력 원상복구
	if (GetCharacterMovement())
	{
		GetCharacterMovement()->GravityScale = DefaultGravityScale;
	}

	//공중 대시 모션 재생중일 경우 바로 끝내기
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance)
	{
		if (MidairDashMontage && AnimInstance->Montage_IsPlaying(MidairDashMontage))
		{
			StopAnimMontage(MidairDashMontage);
			if (HasAuthority())
			{
				GetWorldTimerManager().ClearTimer(DashStateTimerHandle);
			}
			SetIsDashing(false);
		}
		if (HitReactMidairMontage && AnimInstance->Montage_IsPlaying(HitReactMidairMontage))
		{
			StopAnimMontage(HitReactMidairMontage);
		}
	}
	//ACharacter 점프중단 내부함수 호출
	StopJumping();

	//스태미나 회복 시작
	IsRecoveringStamina = true;
	
	//land SFX
	if (IsLocallyControlled() && OnLandSFX)
	{
		UGameplayStatics::PlaySound2D(this, OnLandSFX);
	}
}

// reload를 요청한 GripGun을 기준으로 완료해 weapon switch 중에도 lock을 남기지 않음
void AHughCharacter::OnReloadMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted)
{
	if (Montage == ReloadGripgunMontage)
	{
		if (bInterrupted)
		{
			return;
		}

		//재장전 모션 끝났을때, 조준해제 상태일 경우 여기서 로봇팔에 무기 붙이기
		if (!HughAnimState.bIsAiming)
		{
			if (WaistArmComponent)
			{
				WaistArmComponent->SetArmState(EWaistArmState::Aim_End);
				WaistArmComponent->PlayArm_AimEndMontage(1.f);
				//로봇팔에 무기 부착시키기
				if (CurrentWeapon)
				{
					CurrentWeapon->AttachToComponent(WaistArmComponent->GetWaistArmMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, WaistArmWeaponAttachSocketName);
				}
			}
		}
	}
}

// GripGun reload gameplay는 weapon이 소유하고 Hugh는 presentation 시작만 보장
bool AHughCharacter::TryStartGripGunReload(AGripGun* RequestingGripGun)
{
	if (!RequestingGripGun)
	{
		return false;
	}

	UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (!AnimInstance || !ReloadGripgunMontage)
	{
		return false;
	}

	FOnMontageBlendingOutStarted BlendingOutDelegate;
	//when blendingoutdelegate is called, execute onreloadmontageblendingout
	BlendingOutDelegate.BindUObject(this, &AHughCharacter::OnReloadMontageBlendingOut);
	if (!BlendingOutDelegate.IsBound())
	{
		return false;
	}

	
	//hand over delegate to animation system
	AnimInstance->Montage_Play(ReloadGripgunMontage);
	AnimInstance->Montage_SetBlendingOutDelegate(BlendingOutDelegate, ReloadGripgunMontage);
	if (!HughAnimState.bIsAiming && WaistArmComponent)
	{
		WaistArmComponent->SetArmState(EWaistArmState::Aim_Start);
		AttachWeaponToSocket(RightHandSocketName);
	}

	return true;
}

float AHughCharacter::GetGripGunReloadDuration() const
{
	return ReloadGripgunMontage ? ReloadGripgunMontage->GetPlayLength() : 1.f;
}

void AHughCharacter::ApplyGripGunReloadPresentation(bool bReloading)
{
	if (bReloading)
	{
		TryStartGripGunReload(WeaponInventory.IsValidIndex(0) ? Cast<AGripGun>(WeaponInventory[0]) : nullptr);
		return;
	}

	if (UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
		AnimInstance && ReloadGripgunMontage && AnimInstance->Montage_IsPlaying(ReloadGripgunMontage))
	{
		AnimInstance->Montage_Stop(0.1f, ReloadGripgunMontage);
	}
}

// Hugh 애니메이션 복제값으로 owner와 remote의 단발 presentation을 같은 serial로 재생
void AHughCharacter::OnRep_HughAnimState(FHughReplicatedAnimState OldHughAnimState)
{
	// respawn 시 montage notify가 적용했던 death attachment를 기본 socket으로 복구
	if (OldHughAnimState.ActionState == EActionState::Death && HughAnimState.ActionState != EActionState::Death)
	{
		SetDianaAvatarDeathAttachment(false);
		if (UHughAnimInstance* HughAnimInstance = Cast<UHughAnimInstance>(GetMesh()->GetAnimInstance()))
		{
			HughAnimInstance->Montage_Stop(0.f);
			HughAnimInstance->SetActionState(HughAnimState.ActionState);
			HughAnimInstance->SetIsDead(false);
		}
		if (UDianaAnimInstance* DianaAnimInstance = DianaAvatarMesh ? Cast<UDianaAnimInstance>(DianaAvatarMesh->GetAnimInstance()) : nullptr)
		{
			DianaAnimInstance->ResetDeathPresentation();
		}
	}

	if (IsLocallyControlled()
		&& HughAnimState.WeaponDepletionActionSerial != OldHughAnimState.WeaponDepletionActionSerial)
	{
		PlayWeaponDepletionPresentation(HughAnimState.WeaponDepletionType, HughAnimState.WeaponDepletionActionSerial);
	}

	if (!IsLocallyControlled())
	{
		ApplyReplicatedAnimState(OldHughAnimState);
	}
}

// server가 확정한 탄약 고갈만 replication serial과 montage로 전환
void AHughCharacter::HandleAmmoDepletion()
{
	if (!HasAuthority()
		|| !IsValid(CurrentWeapon)
		|| CurrentWeapon->GetWeaponType() == EWeaponType::Primary
		|| CurrentWeapon->GetCurAmmo() > 0)
	{
		return;
	}

	HughAnimState.WeaponDepletionType = CurrentWeapon->GetWeaponType();
	++HughAnimState.WeaponDepletionActionSerial;
	ForceNetUpdate();
	PlayWeaponDepletionPresentation(HughAnimState.WeaponDepletionType, HughAnimState.WeaponDepletionActionSerial);
}

// replicated depletion serial은 montage만 재생하고 AnimNotify의 inventory 변경은 server guard에 맡김
void AHughCharacter::PlayWeaponDepletionPresentation(EWeaponType WeaponType, int32 ActionSerial)
{
	if (WeaponType == EWeaponType::Primary
		|| (ActionSerial != 0 && LastPlayedWeaponDepletionActionSerial == ActionSerial))
	{
		return;
	}

	UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (!AnimInstance || !ThrowAwayWeaponMontage)
	{
		return;
	}

	LastPlayedWeaponDepletionActionSerial = ActionSerial;
	AnimInstance->Montage_Play(ThrowAwayWeaponMontage, 1.f);
}

void AHughCharacter::ReplenishCheckpointResources()
{
	if (!HasAuthority())
	{
		return;
	}

	if (AGripGun* GripGun = WeaponInventory.IsValidIndex(0) ? Cast<AGripGun>(WeaponInventory[0]) : nullptr)
	{
		GripGun->CancelServerReload();
	}

	if (HealthComponent)
	{
		HealthComponent->RestoreFullHealth();
	}

	for (AWeaponBase* Weapon : WeaponInventory)
	{
		if (IsValid(Weapon))
		{
			Weapon->RefillAmmoToMax();
		}
	}

	if (PlayerStatsComponent)
	{
		PlayerStatsComponent->SetRepairCartridge(PlayerStatsComponent->GetMaxRepairCartridge());
	}
}

void AHughCharacter::ResetForRespawn()
{
	if (!HasAuthority())
	{
		return;
	}

	IsDead = false;
	HughAnimState.ActionState = EActionState::Locomotion;
	HughAnimState.MovementState = EMovementState::Idle;
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->SetComponentTickEnabled(true);
		Movement->SetMovementMode(MOVE_Walking);
	}
	if (UHughAnimInstance* AnimInstance = Cast<UHughAnimInstance>(GetMesh()->GetAnimInstance()))
	{
		AnimInstance->Montage_Stop(0.f);
		AnimInstance->SetActionState(HughAnimState.ActionState);
		AnimInstance->SetIsDead(false);
	}
	if (UDianaAnimInstance* DianaAnimInstance = DianaAvatarMesh ? Cast<UDianaAnimInstance>(DianaAvatarMesh->GetAnimInstance()) : nullptr)
	{
		DianaAnimInstance->ResetDeathPresentation();
	}
	RefreshReplicatedAnimState();
}

void AHughCharacter::HandleDeath()
{
	if (HasAuthority())
	{
		if (APragmataGameState* GameState = GetWorld() ? GetWorld()->GetGameState<APragmataGameState>() : nullptr)
		{
			if (ADianaOperatorPawn* DianaPawn = GameState->GetDianaOperatorPawn())
			{
				DianaPawn->GetVoiceComponent()->PlayVoiceAction(EDianaVoiceAction::OnDeath);
			}
		}
	}

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->StopMovementImmediately();
		MoveComp->DisableMovement();
		MoveComp->SetComponentTickEnabled(false);
	}

	if (HasAuthority())
	{
		GetWorldTimerManager().ClearTimer(WeaponChangeBlockTimerHandle);
		IsWeaponTriggerBlocked = false;
		HughAnimState.bWeaponChangeActive = false;
		ForceNetUpdate();
		if (AGripGun* GripGun = WeaponInventory.IsValidIndex(0) ? Cast<AGripGun>(WeaponInventory[0]) : nullptr)
		{
			GripGun->CancelServerReload();
		}

		if (APragmataGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<APragmataGameMode>() : nullptr)
		{
			GameMode->ScheduleTeamRespawn();
		}

		if (AController* PC = GetController())
		{
			// PC->UnPossess();
		}
	}

	HughAnimState.ActionState = EActionState::Death;
	if (UHughAnimInstance* AnimInstance = Cast<UHughAnimInstance>(GetMesh()->GetAnimInstance()))
	{
		AnimInstance->SetActionState(HughAnimState.ActionState);
		AnimInstance->SetIsDead();
	}
}

// 서버가 combat 상태와 무기를 검증한 뒤 사격을 실행
void AHughCharacter::Server_RequestFire_Implementation()
{
	if (!IsValid(CurrentWeapon)
		|| CurrentWeapon->GetOwner() != this
		|| !WeaponInventory.IsValidIndex(CurrentWeaponIndex)
		|| WeaponInventory[CurrentWeaponIndex] != CurrentWeapon
		|| CurrentWeapon->GetIsCoolingDown()
		|| CurrentWeapon->GetCurAmmo() <= 0
		|| !HughAnimState.bIsAiming
		|| HughAnimState.ActionState != EActionState::Combat
		|| IsWeaponTriggerBlocked
		|| IsDead)
	{
		return;
	}

	if (CurrentWeapon->PullTrigger() && !IsLocallyControlled())
	{
		Client_ConfirmFireFeedback();
	}
}

void AHughCharacter::Client_ConfirmFireFeedback_Implementation()
{
	if (CurrentWeapon)
	{
		CurrentWeapon->ApplyLocalFireFeedback();
	}
}

// client slot 선택을 서버 current weapon으로 확정
void AHughCharacter::Server_RequestWeaponChange_Implementation(int32 TargetWeaponIndex)
{
	if (!WeaponInventory.IsValidIndex(TargetWeaponIndex) || !IsWeaponSlotSelectable(TargetWeaponIndex))
	{
		return;
	}

	AWeaponBase* PreviousWeapon = CurrentWeapon;
	AWeaponBase* TargetWeapon = WeaponInventory[TargetWeaponIndex];
	if (!ChangeWeaponByIndex(TargetWeaponIndex, false))
	{
		return;
	}

	CommitServerWeaponChangePresentation(PreviousWeapon, TargetWeapon);
}

void AHughCharacter::CommitServerWeaponChangePresentation(AWeaponBase* PreviousWeapon, AWeaponBase* TargetWeapon)
{
	if (!HasAuthority() || !TargetWeapon || PreviousWeapon == TargetWeapon)
	{
		return;
	}

	// 비조준 교체는 durable socket state만 갱신하고 transient montage event를 열지 않음
	if (!StartServerWeaponTriggerBlock(TargetWeapon))
	{
		HughAnimState.bWeaponChangeActive = false;
		ForceNetUpdate();
		return;
	}

	HughAnimState.WeaponChangePreviousType = PreviousWeapon ? PreviousWeapon->GetWeaponType() : EWeaponType::Primary;
	HughAnimState.WeaponChangeTargetType = TargetWeapon->GetWeaponType();
	HughAnimState.bWeaponChangeActive = true;
	++HughAnimState.WeaponChangeActionSerial;
	ForceNetUpdate();

	if (IsLocallyControlled())
	{
		PlayWeaponChangePresentation(PreviousWeapon, TargetWeapon, HughAnimState.WeaponChangeActionSerial);
	}
	else
	{
		Client_PlayWeaponChangePresentation(PreviousWeapon, TargetWeapon, HughAnimState.WeaponChangeActionSerial);
	}
}

void AHughCharacter::Client_PlayWeaponChangePresentation_Implementation(
	AWeaponBase* PreviousWeapon,
	AWeaponBase* TargetWeapon,
	int32 ActionSerial)
{
	PlayWeaponChangePresentation(PreviousWeapon, TargetWeapon, ActionSerial);
}

void AHughCharacter::PlayWeaponChangePresentation(AWeaponBase* PreviousWeapon, AWeaponBase* TargetWeapon, int32 ActionSerial)
{
	if (!TargetWeapon || (ActionSerial != 0 && LastPlayedWeaponChangeActionSerial == ActionSerial))
	{
		return;
	}

	UAnimMontage* ChangeMontage = GetDrawStoredWeaponMontage(TargetWeapon->GetWeaponType());
	if (!ChangeMontage && (TargetWeapon->GetWeaponType() == EWeaponType::Primary
		|| TargetWeapon->GetWeaponType() == EWeaponType::Defense))
	{
		ChangeMontage = DrawWeaponFromWaistMontage;
	}
	UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (!ChangeMontage || !AnimInstance)
	{
		return;
	}

	LastPlayedWeaponChangeActionSerial = ActionSerial;
	AttachWeaponToStoredSocket(PreviousWeapon);
	AnimInstance->Montage_Play(ChangeMontage, 1.f);
}

bool AHughCharacter::StartServerWeaponTriggerBlock(AWeaponBase* TargetWeapon)
{
	if (!HasAuthority() || !HughAnimState.bIsAiming || !TargetWeapon)
	{
		return false;
	}

	UAnimMontage* ChangeMontage = GetDrawStoredWeaponMontage(TargetWeapon->GetWeaponType());
	if (!ChangeMontage && (TargetWeapon->GetWeaponType() == EWeaponType::Primary
		|| TargetWeapon->GetWeaponType() == EWeaponType::Defense))
	{
		ChangeMontage = DrawWeaponFromWaistMontage;
	}
	if (!ChangeMontage)
	{
		return false;
	}

	IsWeaponTriggerBlocked = true;
	GetWorldTimerManager().SetTimer(
		WeaponChangeBlockTimerHandle,
		this,
		&AHughCharacter::ClearServerWeaponTriggerBlock,
		FMath::Max(ChangeMontage->GetPlayLength(), 0.1f),
		false);
	return true;
}

void AHughCharacter::ClearServerWeaponTriggerBlock()
{
	if (HasAuthority())
	{
		IsWeaponTriggerBlocked = false;
		HughAnimState.bWeaponChangeActive = false;
		ForceNetUpdate();
	}
}

void AHughCharacter::Server_RequestPickup_Implementation(AWeaponBase* PickupCandidate)
{
	if (!IsDead && PickUpWeapon(PickupCandidate))
	{
		Client_PlayPickupPresentation();
	}
}

void AHughCharacter::Server_RequestDash_Implementation()
{
	TryCommitServerDash();
}

bool AHughCharacter::TryCommitServerDash()
{
	if (!HasAuthority() || IsDead || IsDashing || CurStamina < DashStaminaConsumeAmount || !GetCharacterMovement())
	{
		return false;
	}

	uint8 DashMode = 0;
	FName SectionName = NAME_None;
	if (!HughAnimState.bIsAiming)
	{
		DashMode = (GetCharacterMovement()->IsFalling() || HughAnimState.bIsHovering) ? 2 : 0;
	}
	else
	{
		DashMode = 1;
		SectionName = TEXT("Forward");
		if (!GetCharacterMovement()->GetCurrentAcceleration().IsNearlyZero())
		{
			const float DashAngle = UKismetAnimationLibrary::CalculateDirection(GetVelocity(), GetActorRotation());
			if (DashAngle >= 45.f && DashAngle < 135.f) SectionName = TEXT("Right");
			else if (DashAngle >= -135.f && DashAngle < -45.f) SectionName = TEXT("Left");
			else if (DashAngle >= 135.f || DashAngle < -135.f) SectionName = TEXT("Backward");
		}
	}

	UAnimMontage* Montage = DashMode == 1 ? AimDashMontage : DashMode == 2 ? MidairDashMontage : DashMontage;
	if (!Montage)
	{
		return false;
	}

	ConsumeStamina(DashStaminaConsumeAmount, false);
	BroadcastStaminaChanged();
	IsDashing = true;
	HughAnimState.bIsDashing = true;
	HughAnimState.DashPresentationMode = DashMode;
	HughAnimState.DashSectionName = SectionName;
	++HughAnimState.DashActionSerial;
	ForceNetUpdate();

	PlayDashPresentation(DashMode, SectionName, HughAnimState.DashActionSerial);
	if (!IsLocallyControlled())
	{
		Client_PlayDashPresentation(DashMode, SectionName, HughAnimState.DashActionSerial);
	}

	float DashDuration = Montage->GetPlayLength();
	if (DashMode == 1 && !SectionName.IsNone())
	{
		const int32 SectionIndex = Montage->GetSectionIndex(SectionName);
		if (SectionIndex != INDEX_NONE)
		{
			DashDuration = Montage->GetSectionLength(SectionIndex);
		}
	}

	GetWorldTimerManager().SetTimer(
		DashStateTimerHandle,
		this,
		&AHughCharacter::FinishServerDash,
		FMath::Max(DashDuration, 0.1f),
		false);
	return true;
}

void AHughCharacter::Client_PlayDashPresentation_Implementation(uint8 DashMode, FName SectionName, int32 ActionSerial)
{
	PlayDashPresentation(DashMode, SectionName, ActionSerial);
}

void AHughCharacter::PlayDashPresentation(uint8 DashMode, FName SectionName, int32 ActionSerial)
{
	if (ActionSerial != 0 && LastPlayedDashActionSerial == ActionSerial)
	{
		return;
	}

	UAnimMontage* Montage = DashMode == 1 ? AimDashMontage : DashMode == 2 ? MidairDashMontage : DashMontage;
	if (!Montage)
	{
		return;
	}

	LastPlayedDashActionSerial = ActionSerial;
	IsDashing = true;
	PlayAnimMontage(Montage, 1.f, SectionName);
}

void AHughCharacter::FinishServerDash()
{
	SetIsDashing(false);
}

void AHughCharacter::SetIsDashing(bool bNewDashing)
{
	IsDashing = bNewDashing;
	if (HasAuthority())
	{
		HughAnimState.bIsDashing = bNewDashing;
		ForceNetUpdate();
	}
}

void AHughCharacter::Client_PlayPickupPresentation_Implementation()
{
	OnCompleteAim(FInputActionValue());
	if (UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
	{
		AnimInstance->Montage_Play(PickUpWeaponMontage);
	}
}

// HeadVisor socket이 없을 때도 component가 사라지지 않게 mesh root에 fallback attach
void AHughCharacter::EnsureHeadVisorSpotlightAttachment()
{
	if (!HeadVisorSpotlight || !GetMesh())
	{
		return;
	}

	if (!GetMesh()->DoesSocketExist(HeadVisorLightSocketName))
	{
		UE_LOG(LogTemp, Warning, TEXT("[HughCharacter::EnsureHeadVisorSpotlightAttachment] Missing socket: %s"), *HeadVisorLightSocketName.ToString());
		HeadVisorSpotlight->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		HeadVisorSpotlight->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
		return;
	}

	HeadVisorSpotlight->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, HeadVisorLightSocketName);
	HeadVisorSpotlight->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
}

// SpotLight component 표시 상태를 replicated bool과 동일하게 적용
void AHughCharacter::ApplyHeadVisorSpotlightEnabled(bool bNewEnabled)
{
	if (!HeadVisorSpotlight)
	{
		return;
	}

	HeadVisorSpotlight->SetVisibility(bNewEnabled, true);
	HeadVisorSpotlight->SetHiddenInGame(!bNewEnabled, true);
	if (bNewEnabled)
	{
		HeadVisorSpotlight->Activate(true);
	}
	else
	{
		HeadVisorSpotlight->Deactivate();
	}
}

// 서버 확정값과 local 예측값을 같은 함수로 갱신
void AHughCharacter::SetHeadVisorSpotlightEnabled(bool bNewEnabled)
{
	bHeadVisorSpotlightEnabled = bNewEnabled;
	ApplyHeadVisorSpotlightEnabled(bHeadVisorSpotlightEnabled);
}

// remote proxy가 서버 확정 visor light 상태를 반영
void AHughCharacter::OnRep_HeadVisorSpotlightEnabled()
{
	ApplyHeadVisorSpotlightEnabled(bHeadVisorSpotlightEnabled);
}

// client toggle 요청을 서버 상태로 확정
void AHughCharacter::Server_SetHeadVisorSpotlightEnabled_Implementation(bool bNewEnabled)
{
	SetHeadVisorSpotlightEnabled(bNewEnabled);
}

// client 예측 spend를 서버 stamina snapshot에 반영
void AHughCharacter::Server_RequestConsumeStamina_Implementation(float Stamina)
{
	const bool bKnownDiscreteSpend =
		FMath::IsNearlyEqual(Stamina, DashStaminaConsumeAmount)
		|| FMath::IsNearlyEqual(Stamina, JumpStaminaConsumeAmount);
	if (!bKnownDiscreteSpend)
	{
		return;
	}

	ConsumeStamina(Stamina, false);
	BroadcastStaminaChanged();
}

void AHughCharacter::Server_RequestHeal_Implementation()
{
	// client는 intent만 보내고 서버가 수량, HP, movement blocker를 다시 검증
	if (!CalculateHealAvailability() || !PlayerStatsComponent)
	{
		UpdateHealAvailability();
		return;
	}

	PlayerStatsComponent->ConsumeRepairCartridge();
	UpdateHealAvailability();
}

// Hugh 등에 붙는 Diana avatar mesh를 socket fallback까지 고려해 고정
void AHughCharacter::AttachDianaAvatar()
{
	SetDianaAvatarDeathAttachment(false);
}

// Diana avatar socket 전환은 death와 respawn 모두 같은 helper를 사용해 attachment 상태가 갈라지지 않게 함
void AHughCharacter::SetDianaAvatarDeathAttachment(bool bUseDeathSocket)
{
	if (!DianaAvatarMesh || !GetMesh())
	{
		return;
	}

	if (bUseDeathSocket)
	{
		// Diana death animation의 root 이동이 Hugh attachment에 고정되지 않도록 world transform을 유지해 분리
		DianaAvatarMesh->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		return;
	}

	const FName TargetSocketName = DianaMountSocketName;
	if (!GetMesh()->DoesSocketExist(TargetSocketName))
	{
		UE_LOG(LogTemp, Warning, TEXT("[HughCharacter::SetDianaAvatarDeathAttachment] Missing socket: %s"), *TargetSocketName.ToString());
		DianaAvatarMesh->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		return;
	}

	DianaAvatarMesh->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, TargetSocketName);
}

// 로봇팔 component attachment를 socket 또는 Hip fallback으로 복구
void AHughCharacter::AttachWaistArm()
{
	if (!WaistArmComponent || !GetMesh())
	{
		return;
	}

	if (!GetMesh()->DoesSocketExist(WaistArmAttachSocketName))
	{
		static const FName WaistArmFallbackBoneName(TEXT("Hip"));
		if (GetMesh()->GetBoneIndex(WaistArmFallbackBoneName) == INDEX_NONE && !GetMesh()->DoesSocketExist(WaistArmFallbackBoneName))
		{
			UE_LOG(LogTemp, Warning, TEXT("[HughCharacter::AttachWaistArm] Missing socket or bone: %s. Missing fallback bone: %s"),
				*WaistArmAttachSocketName.ToString(),
				*WaistArmFallbackBoneName.ToString());
			WaistArmComponent->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
			return;
		}

		UE_LOG(LogTemp, Warning, TEXT("[HughCharacter::AttachWaistArm] Missing socket or bone: %s. Fallback to %s"),
			*WaistArmAttachSocketName.ToString(),
			*WaistArmFallbackBoneName.ToString());
		WaistArmComponent->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, WaistArmFallbackBoneName);
		return;
	}

	WaistArmComponent->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, WaistArmAttachSocketName);
}
