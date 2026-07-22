#include "Enemy/MonsterBase.h"

#include "Components/PragmataFactionComponent.h"
#include "Components/PragmataHealthComponent.h"
#include "Components/PragmataPlayerStatsComponent.h"
#include "Characters/HughCharacter.h"
#include "Puzzle/EnemyHackingPuzzleComponent.h"
#include "AI/MonsterAIController.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInterface.h"
#include "Components/WidgetComponent.h"
#include "UI/EnemyHPWidgetComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Game/PragmataGameState.h"
#include "Pawns/DianaOperatorPawn.h"
#include "Components/DianaVoiceComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Net/UnrealNetwork.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AIPerceptionSystem.h"
#include "Perception/AISense_Damage.h"
#include "Perception/AISenseConfig_Damage.h"
#include "Perception/AISenseConfig_Sight.h"
#include "DamageText.h"
#include "Enemy/PragmataMonsterMovementComponent.h"
#include "TimerManager.h"
#include "AI/MonsterBBKey.h"
#include "BehaviorTree/ValueOrBBKey.h"
#include "Weapon/WeaponBase.h"

namespace
{
	// 대상 mesh만 BeginPlay 기본 오염 강제를 적용
	bool IsCleanseTaintMesh(const USkeletalMesh* SkeletalMesh)
	{
		if (!SkeletalMesh)
		{
			return false;
		}

		const FName MeshName = SkeletalMesh->GetFName();
		return MeshName == FName(TEXT("SM_Sentinel"))
			|| MeshName == FName(TEXT("SM_Walker_Tainted"))
			|| MeshName == FName(TEXT("SM_Executor_Tainted"));
	}

	constexpr float HalfHealthHitReactRatio = 0.5f;

	struct FResolvedDeathAnimation
	{
		UAnimSequenceBase* Animation = nullptr;
		float RagdollDelay = -1.f;
	};

	// 방향 variant와 그 ragdoll 시간을 같은 항목으로 선택하고 비었으면 단일 설정으로 fallback
	FResolvedDeathAnimation ResolveDirectionalDeathAnimation(
		bool bUseDirectionVariants,
		UAnimSequenceBase* Animation,
		float RagdollDelay,
		UAnimSequenceBase* FrontAnimation,
		float FrontRagdollDelay,
		UAnimSequenceBase* BackAnimation,
		float BackRagdollDelay,
		EPragmataHitDirection HitDirection)
	{
		if (!bUseDirectionVariants)
		{
			return {Animation, RagdollDelay};
		}

		const FResolvedDeathAnimation DirectionalResult = HitDirection == EPragmataHitDirection::Back
			? FResolvedDeathAnimation{BackAnimation, BackRagdollDelay}
			: FResolvedDeathAnimation{FrontAnimation, FrontRagdollDelay};
		return DirectionalResult.Animation
			? DirectionalResult
			: FResolvedDeathAnimation{Animation, RagdollDelay};
	}

	// weapon actor가 아니라 실제 플레이어 pawn을 AI target 후보로 복원
	AActor* ResolveWeaponDamageInstigatorActor(const FPragmataDamageSpec& DamageSpec)
	{
		if (AController* InstigatorController = DamageSpec.InstigatorController.Get())
		{
			if (APawn* InstigatorPawn = InstigatorController->GetPawn())
			{
				return InstigatorPawn;
			}
		}

		if (IsValid(DamageSpec.SourceActor.Get()))
		{
			return DamageSpec.SourceActor.Get();
		}

		AActor* DamageCauser = DamageSpec.DamageCauser.Get();
		if (!IsValid(DamageCauser))
		{
			return nullptr;
		}

		return IsValid(DamageCauser->GetOwner())
			? DamageCauser->GetOwner()
			: DamageCauser;
	}

	// direct hit weapon과 weapon-owned field actor를 모두 weapon override 기준으로 복원
	const AWeaponBase* ResolveDamageWeaponActor(const FPragmataDamageSpec& DamageSpec)
	{
		if (const AWeaponBase* DamageWeapon = Cast<AWeaponBase>(DamageSpec.DamageCauser.Get()))
		{
			return DamageWeapon;
		}

		const AActor* DamageCauser = DamageSpec.DamageCauser.Get();
		if (DamageCauser)
		{
			if (const AWeaponBase* OwnerWeapon = Cast<AWeaponBase>(DamageCauser->GetOwner()))
			{
				return OwnerWeapon;
			}
		}

		return Cast<AWeaponBase>(DamageSpec.SourceActor.Get());
	}

	FVector ResolveDamageSenseHitLocation(const AActor* DamagedActor, const FPragmataDamageSpec& DamageSpec)
	{
		if (DamageSpec.bHasImpact && DamageSpec.ImpactHit.bBlockingHit)
		{
			return DamageSpec.ImpactHit.ImpactPoint;
		}

		return DamagedActor ? DamagedActor->GetActorLocation() : FVector::ZeroVector;
	}
}

AMonsterBase::AMonsterBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UPragmataMonsterMovementComponent>(
		ACharacter::CharacterMovementComponentName))
{
	/* 서버 설정 */
	// 액터 리플리케이션 활성화
	bReplicates = true;
	// Movement 리플리케이션 활성화
	SetReplicateMovement(true);

	/* 기본 설정 */
	// Movement 설정
	GetCharacterMovement()->bUseControllerDesiredRotation = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// AIController 클래스 정보 설정
	// 맵에서 로드 또는 런타임에 스폰(생성)되는 모든 경우 미리 지정한 AIController에 빙의되도록 설정.
	AIControllerClass = AMonsterAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	// 스텟 컴포넌트
	HealthComponent = CreateDefaultSubobject<UPragmataHealthComponent>(TEXT("HealthComponent"));

	// 진영 설정
	FactionComponent = CreateDefaultSubobject<UPragmataFactionComponent>(TEXT("FactionComponent"));

	// 적 해킹 퍼즐은 서버가 시뮬하고 양쪽 HUD가 복제 상태를 표시
	EnemyHackingPuzzleComponent = CreateDefaultSubobject<UEnemyHackingPuzzleComponent>(TEXT("EnemyHackingPuzzleComponent"));

	//ai perception
	AIPerceptionComponent = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("AIPerceptionComponent"));
	if (!AIPerceptionComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("AIPerceptionComponent is nullptr"));
	}
	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
	if (!SightConfig)
	{
		UE_LOG(LogTemp, Warning, TEXT("SightConfig is nullptr"));
	}

	if (SightConfig != nullptr)
	{
		SightConfig->SightRadius = 1000.f;
		SightConfig->LoseSightRadius = 1500.f;
		SightConfig->PeripheralVisionAngleDegrees = 60.f;

		SightConfig->DetectionByAffiliation.bDetectEnemies = true;
		SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
		SightConfig->DetectionByAffiliation.bDetectNeutrals = true;

		AIPerceptionComponent->ConfigureSense(*SightConfig);
		AIPerceptionComponent->SetDominantSense(SightConfig->GetSenseImplementation());
	}

	DamageConfig = CreateDefaultSubobject<UAISenseConfig_Damage>(TEXT("DamageConfig"));
	if (!DamageConfig)
	{
		UE_LOG(LogTemp, Warning, TEXT("DamageConfig is nullptr"));
	}

	if (DamageConfig != nullptr && AIPerceptionComponent)
	{
		AIPerceptionComponent->ConfigureSense(*DamageConfig);
	}

	//damagetext
	static ConstructorHelpers::FClassFinder<AActor> DamageTextBlueprintFinder(TEXT("/Game/Blueprints/Enemy/Common/BP_DamageText.BP_DamageText_C"));

	if (DamageTextBlueprintFinder.Succeeded())
	{
		DamageTextClass = DamageTextBlueprintFinder.Class;
	}
}

void AMonsterBase::BeginPlay()
{
	Super::BeginPlay();

	// target 복제 전에도 월드 HP bar가 기본 노출되지 않게 초기 상태 적용
	ApplyTargetHPBarVisibility();

	// BP body collision은 이동에만 남기고 총알 trace는 정확한 Physics Asset body가 단독 수신
	UCapsuleComponent* RootCapsule = GetCapsuleComponent();
	USkeletalMeshComponent* MeshComponent = GetMesh();
	// mesh의 오염 variant가 server gameplay state와 같은 초기 기준을 사용
	const bool bShouldStartTainted = IsCleanseTaintMesh(MeshComponent ? MeshComponent->GetSkeletalMeshAsset() : nullptr);
	if (HasAuthority() && bIsTainted != bShouldStartTainted)
	{
		bIsTainted = bShouldStartTainted;
		HandleTaintStateChanged();
		ForceNetUpdate();
	}

	TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents(this);
	for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		if (!PrimitiveComponent)
		{
			continue;
		}

		if (PrimitiveComponent != RootCapsule)
		{
			PrimitiveComponent->SetCanEverAffectNavigation(false);
		}

		PrimitiveComponent->SetCollisionResponseToChannel(
			ECC_GameTraceChannel1,
			PrimitiveComponent == MeshComponent ? ECR_Block : ECR_Ignore);
	}

	if (FactionComponent)
	{
		FactionComponent->SetFaction(EPragmataFaction::Monster);
	}

	if (HealthComponent)
	{
		HealthComponent->OnDeath.AddUniqueDynamic(this, &ThisClass::HandleDeath);
	}
}

void AMonsterBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// actor 종료 후 delayed callback이 남지 않게 timer 해제
	GetWorldTimerManager().ClearTimer(DeathRagdollTimerHandle);
	for (TPair<TWeakObjectPtr<AActor>, FTimerHandle>& StunTimerPair : StasisStunSourceTimers)
	{
		GetWorldTimerManager().ClearTimer(StunTimerPair.Value);
	}
	StasisStunSourceTimers.Reset();
	ClearHackDurationTimer();

	if (APragmataGameState* GameState = GetWorld() ? GetWorld()->GetGameState<APragmataGameState>() : nullptr)
	{
		GameState->UnregisterEnemyAggro(this);
	}

	Super::EndPlay(EndPlayReason);
}

void AMonsterBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	CalculateAngle();
	UpdateFallingSpawnAppearance();
	UpdateProximityControlledAttackLoop();
}

void AMonsterBase::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	AMonsterAIController* MonsterController = Cast<AMonsterAIController>(NewController);
	if (!MonsterController)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AMonsterBase::PossessedBy] AIController is nullptr"));
		return;
	}

	if (AIPerceptionComponent)
	{
		// Damage Sense는 damaged Pawn의 AIController에서 perception component를 찾음
		if (MonsterController->GetPerceptionComponent() != AIPerceptionComponent)
		{
			MonsterController->SetPerceptionComponent(*AIPerceptionComponent);
		}

		AIPerceptionComponent->OnTargetPerceptionUpdated.AddUniqueDynamic(this, &AMonsterBase::OnTargetDetected);
	}
}

void AMonsterBase::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);

	// 하강 appearance는 실제 착지 순간에도 loop 탈출을 보장
	RequestFallingSpawnAppearanceEnd();
}

void AMonsterBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AMonsterBase, bHacked);
	DOREPLIFETIME(AMonsterBase, AppliedHackDuration);
	DOREPLIFETIME(AMonsterBase, HackedEndServerTime);
	DOREPLIFETIME(AMonsterBase, bIsTainted);
	DOREPLIFETIME(AMonsterBase, bIsAttacking);
	DOREPLIFETIME(AMonsterBase, bSpawnAppearancePlaying);
	DOREPLIFETIME(AMonsterBase, ActiveSpawnAppearanceMontage);
	DOREPLIFETIME(AMonsterBase, bSpawnAppearanceIgnoresRootMotion);
	DOREPLIFETIME(AMonsterBase, bStasisStunned);
	DOREPLIFETIME(AMonsterBase, CurrentDeathAnimation);
	DOREPLIFETIME(AMonsterBase, CurrentDeathRagdollDelay);
	DOREPLIFETIME(AMonsterBase, fAngle);
	DOREPLIFETIME(AMonsterBase, TargetActor);
}

// 서버 target 변경을 받은 각 peer에서 월드 HP bar 표시를 즉시 동기화
void AMonsterBase::OnRep_TargetActor()
{
	ApplyTargetHPBarVisibility();
}

// Blackboard는 서버 전용이므로 복제된 TargetActor를 월드 UI의 공통 표시 기준으로 사용
void AMonsterBase::ApplyTargetHPBarVisibility()
{
	const bool bShouldShowHPBar = IsValid(TargetActor) && !IsDead();
	TInlineComponentArray<UEnemyHPWidgetComponent*> HPWidgetComponents(this);
	for (UEnemyHPWidgetComponent* HPWidgetComponent : HPWidgetComponents)
	{
		if (HPWidgetComponent)
		{
			HPWidgetComponent->SetOwnerTargetedVisible(bShouldShowHPBar);
		}
	}
}

bool AMonsterBase::IsDead() const
{
	return HealthComponent && HealthComponent->IsDead();
}

float AMonsterBase::GetCurrentAttackDamage_Implementation() const
{
	// 공격 notify는 owner에게 현재 damage만 묻기 때문에 선택된 pattern override를 여기서 확정
	if (const FMonsterAttackPattern* Pattern = GetSelectedAttackPattern())
	{
		if (Pattern->bUsePatternAttackDamage)
		{
			return FMath::Max(0.f, Pattern->PatternAttackDamage);
		}
	}

	return FMath::Max(0.f, CombatData.AttackDamage);
}

bool AMonsterBase::CanReceiveDamage_Implementation(const FPragmataDamageSpec& DamageSpec) const
{
	return HealthComponent && !HealthComponent->IsDead() && DamageSpec.DamageAmount > 0.f;
}

// health 차감 전에 사망과 반피 피격에 사용할 현재 공격·이동 상태를 확정
bool AMonsterBase::ReceiveDamage_Implementation(const FPragmataDamageSpec& DamageSpec)
{
	if (!IPragmataDamageable::Execute_CanReceiveDamage(this, DamageSpec))
	{
		return false;
	}

	FPragmataDamageSpec FinalDamageSpec = DamageSpec;
	ResolveDamageDirection(FinalDamageSpec);
	FinalDamageSpec.DamageAmount *= ResolveUnhackedDamageTakenMultiplier(FinalDamageSpec);
	FinalDamageSpec.DamageAmount *= ResolveHackedWeakPointDamageMultiplier(FinalDamageSpec);
	FinalDamageSpec.DamageAmount *= ResolveWeaponDamageTakenMultiplier(FinalDamageSpec);

	const float PreviousHealth = HealthComponent->GetCurrentHealth();
	const float HalfHealth = HealthComponent->GetMaxHealth() * HalfHealthHitReactRatio;
	const float PredictedHealth = FMath::Max(0.f, PreviousHealth - FinalDamageSpec.DamageAmount);
	// Stasis 정지 중에는 dot damage가 반피 montage로 정지 상태를 깨지 않게 함
	const bool bShouldPlayHalfHealthHitReact = !bHalfHealthHitReactPlayed
		&& !bStasisStunned
		&& PreviousHealth > HalfHealth
		&& PredictedHealth <= HalfHealth
		&& PredictedHealth > 0.f;
	const FName HalfHealthHitReactSection = bShouldPlayHalfHealthHitReact
		? ResolveHalfHealthHitReactSection(FinalDamageSpec)
		: NAME_None;

	// OnDeath가 ReceiveDamage 내부에서 즉시 호출되므로 치명타 상황을 먼저 고정
	if (FinalDamageSpec.DamageAmount >= PreviousHealth)
	{
		CurrentDeathAnimation = ResolveDeathAnimation(FinalDamageSpec, CurrentDeathRagdollDelay);
	}

	const bool bApplied = HealthComponent->ReceiveDamage(FinalDamageSpec);
	if (FinalDamageSpec.DamageKind == EPragmataDamageKind::Weapon)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[DamageAggro][ReceiveDamage] Monster=%s Applied=%d Dead=%d Amount=%.2f Source=%s Causer=%s InstigatorController=%s TargetActor=%s"),
			*GetNameSafe(this),
			bApplied ? 1 : 0,
			IsDead() ? 1 : 0,
			FinalDamageSpec.DamageAmount,
			*GetNameSafe(FinalDamageSpec.SourceActor.Get()),
			*GetNameSafe(FinalDamageSpec.DamageCauser.Get()),
			*GetNameSafe(FinalDamageSpec.InstigatorController.Get()),
			*GetNameSafe(TargetActor.Get()));
	}

	if (bApplied && IsDead())
	{
		// 사망 animation과 HealthComponent의 bIsDead를 같은 net update에 전달
		ForceNetUpdate();
	}
	else if (bApplied && HasAuthority() && !bDianaImminentVictoryAnnounced)
	{
		if (APragmataGameState* GameState = GetWorld() ? GetWorld()->GetGameState<APragmataGameState>() : nullptr)
		{
			const float MaxHealth = HealthComponent ? HealthComponent->GetMaxHealth() : 0.f;
			const float HealthRatio = MaxHealth > UE_KINDA_SMALL_NUMBER
				? HealthComponent->GetCurrentHealth() / MaxHealth
				: 1.f;
			if (GameState->GetActiveBossTarget() == this && HealthRatio <= DianaImminentVictoryHealthRatio)
			{
				bDianaImminentVictoryAnnounced = true;
				if (ADianaOperatorPawn* DianaPawn = GameState->GetDianaOperatorPawn())
				{
					DianaPawn->GetVoiceComponent()->PlayVoiceAction(EDianaVoiceAction::ImminentVictory);
				}
			}
		}
	}
	else if (bApplied && bShouldPlayHalfHealthHitReact)
	{
		// 회복 후 다시 반피를 통과해도 반복하지 않도록 최초 crossing을 소비
		bHalfHealthHitReactPlayed = true;
		PlayHalfHealthHitReact(HalfHealthHitReactSection);
	}

	if (bApplied && !IsDead() && FinalDamageSpec.DamageAmount > 0.f)
	{
		ReportWeaponDamageSense(FinalDamageSpec);
	}

	if (bApplied && FinalDamageSpec.DamageAmount > 0.f)
	{
		// overkill로 health가 0에 clamp돼도 이번 공격의 최종 계산 대미지를 그대로 표시
		SpawnDamageText(FinalDamageSpec.DamageAmount, FinalDamageSpec);
	}

	return bApplied;
}

// repeat hack puzzle은 허용하고 사망/비활성 대상만 차단
bool AMonsterBase::CanBeHacked_Implementation(AActor* HackInstigator) const
{
	return bHackable && HealthComponent && !HealthComponent->IsDead();
}

// 성공한 hack damage와 hacked 상태 진입을 분리해 repeat hack 보상은 유지
bool AMonsterBase::ApplySuccessfulHack_Implementation(AActor* HackInstigator, float HackDamage, float ResolvedHackDurationSeconds)
{
	if (!HasAuthority())
	{
		return false;
	}

	if (!IPragmataHackable::Execute_CanBeHacked(this, HackInstigator))
	{
		return false;
	}

	FPragmataDamageSpec DamageSpec;
	DamageSpec.DamageAmount = HackDamage;
	DamageSpec.SourceActor = HackInstigator;
	DamageSpec.DamageCauser = HackInstigator;
	DamageSpec.DamageKind = EPragmataDamageKind::Hack;
	DamageSpec.HitReactStrength = EPragmataHitReactStrength::Small;

	const bool bWasHacked = bHacked;
	const bool bApplied = IPragmataDamageable::Execute_ReceiveDamage(this, DamageSpec);
	if (bApplied && !bWasHacked)
	{
		SetHackedState(true);
		StartHackDurationTimer(ResolvedHackDurationSeconds);
		OnHackApplied(HackInstigator, HackDamage);
	}

	return bApplied;
}

bool AMonsterBase::CleanseTaint(AActor* CleanseInstigator)
{
	// 오염은 정화 스킬만이 해제 가능하며 hacked timer와 별도 상태로 유지
	(void)CleanseInstigator;

	if (!HasAuthority() || !bIsTainted || !HealthComponent || HealthComponent->IsDead())
	{
		return false;
	}

	// 정화된 상태는 timer로 되돌리지 않고 replicated visual state로 고정
	bIsTainted = false;
	HandleTaintStateChanged();
	ForceNetUpdate();
	return true;
}

void AMonsterBase::OnRep_Hacked()
{
	// client는 복제된 hack 상태로 local presentation을 갱신
	HandleHackStateChanged();
}

void AMonsterBase::OnRep_Tainted()
{
	// client는 복제된 오염 상태로 visual presentation을 갱신
	HandleTaintStateChanged();
}

void AMonsterBase::HandleHackStateChanged()
{
}

void AMonsterBase::HandleTaintStateChanged()
{
	// 정화 뒤 지정한 오염 material slot을 모든 peer에서 숨김
	ApplyTaintMaterialVisibility();
	ReceiveTaintStateChanged();
}

// 정화가 확정된 mesh의 taint 전용 slot만 숨겨 다른 material은 보존
void AMonsterBase::ApplyTaintMaterialVisibility()
{
	if (bIsTainted)
	{
		return;
	}

	USkeletalMeshComponent* MeshComponent = GetMesh();
	USkeletalMesh* SkeletalMesh = MeshComponent ? MeshComponent->GetSkeletalMeshAsset() : nullptr;
	if (!SkeletalMesh)
	{
		return;
	}

	const TCHAR* const SentinelTaintMaterialSlots[] =
	{
		TEXT("ch6500_00_Emissive1"),
		TEXT("ch6500_60_DF"),
		TEXT("ch6500_00_DF2")
	};
	const TCHAR* const WalkerTaintMaterialSlots[] =
	{
		TEXT("ch5100_00_Head_001"),
		TEXT("DF")
	};
	const TCHAR* const ExecutorTaintMaterialSlots[] =
	{
		TEXT("ch6200_60_DF1"),
		TEXT("DF2"),
		TEXT("DF3"),
		TEXT("ch6200_60_DF_Broken"),
		TEXT("ch6200_60_Head"),
		TEXT("ch6200_60_Armor")
	};

	const TCHAR* const* TaintMaterialSlots = nullptr;
	int32 TaintMaterialSlotCount = 0;
	const FName MeshName = SkeletalMesh->GetFName();
	if (MeshName == FName(TEXT("SM_Sentinel")))
	{
		TaintMaterialSlots = SentinelTaintMaterialSlots;
		TaintMaterialSlotCount = UE_ARRAY_COUNT(SentinelTaintMaterialSlots);
	}
	else if (MeshName == FName(TEXT("SM_Walker_Tainted")))
	{
		TaintMaterialSlots = WalkerTaintMaterialSlots;
		TaintMaterialSlotCount = UE_ARRAY_COUNT(WalkerTaintMaterialSlots);
	}
	else if (MeshName == FName(TEXT("SM_Executor_Tainted")))
	{
		TaintMaterialSlots = ExecutorTaintMaterialSlots;
		TaintMaterialSlotCount = UE_ARRAY_COUNT(ExecutorTaintMaterialSlots);
	}
	else
	{
		return;
	}

	static TSoftObjectPtr<UMaterialInterface> InvisibleMaterial{
		FSoftObjectPath(TEXT("/Game/VFX/M_Invisible.M_Invisible"))};
	UMaterialInterface* Material = InvisibleMaterial.LoadSynchronous();
	if (!Material)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AMonsterBase::ApplyTaintMaterialVisibility] Missing M_Invisible"));
		return;
	}

	for (int32 SlotListIndex = 0; SlotListIndex < TaintMaterialSlotCount; ++SlotListIndex)
	{
		const FName SlotName(TaintMaterialSlots[SlotListIndex]);
		const int32 SlotIndex = MeshComponent->GetMaterialIndex(SlotName);
		if (SlotIndex == INDEX_NONE)
		{
			UE_LOG(LogTemp, Warning, TEXT("[AMonsterBase::ApplyTaintMaterialVisibility] Missing material slot %s on %s"), *SlotName.ToString(), *MeshName.ToString());
			continue;
		}

		MeshComponent->SetMaterial(SlotIndex, Material);
	}
}

// replicated hack 상태 변경과 server/client presentation 갱신을 한 경로로 통일
void AMonsterBase::SetHackedState(bool bNewHacked)
{
	if (bHacked == bNewHacked)
	{
		return;
	}

	bHacked = bNewHacked;
	if (!bHacked)
	{
		ClearHackDurationTimer();
		// 만료 시 게이지가 비워지도록 복제 타이밍도 초기화
		AppliedHackDuration = 0.f;
		HackedEndServerTime = 0.f;
	}

	HandleHackStateChanged();
	ForceNetUpdate();
}

// 최초 hacked 진입만 전달된 duration을 적용하고 profile 누락은 기존 기본값으로 복구
void AMonsterBase::StartHackDurationTimer(float ResolvedHackDurationSeconds)
{
	if (!HasAuthority() || !bHacked)
	{
		return;
	}

	// 완료 snapshot의 profile duration을 사용해 UI 표시와 실제 timer를 같은 값으로 고정
	const float Duration = FMath::Max(
		0.1f,
		ResolvedHackDurationSeconds > 0.f ? ResolvedHackDurationSeconds : HackDurationSeconds);

	// 게이지가 클라에서도 정확하도록 만료 서버 시각을 복제. 서버 월드 시각 기준.
	const UWorld* MonsterWorld = GetWorld();
	const AGameStateBase* GameStateBase = MonsterWorld ? MonsterWorld->GetGameState() : nullptr;
	const float ServerNow = GameStateBase
		? GameStateBase->GetServerWorldTimeSeconds()
		: (MonsterWorld ? MonsterWorld->GetTimeSeconds() : 0.f);

	AppliedHackDuration = Duration;
	HackedEndServerTime = ServerNow + Duration;
	ClearHackDurationTimer();
	GetWorldTimerManager().SetTimer(
		HackDurationTimerHandle,
		this,
		&ThisClass::ExpireHackDuration,
		Duration,
		false
	);
	ForceNetUpdate();
}

float AMonsterBase::GetHackedRemainingSeconds() const
{
	if (!bHacked)
	{
		return 0.f;
	}

	const UWorld* MonsterWorld = GetWorld();
	const AGameStateBase* GameStateBase = MonsterWorld ? MonsterWorld->GetGameState() : nullptr;
	if (!GameStateBase)
	{
		return 0.f;
	}

	return FMath::Max(0.f, HackedEndServerTime - GameStateBase->GetServerWorldTimeSeconds());
}

// OverDrive는 일반 hack 완료와 달리 damage, reward, gauge를 만들지 않고 상태 timer만 갱신
bool AMonsterBase::ApplyTimedHackState(float DurationSeconds)
{
	if (!HasAuthority() || !IPragmataHackable::Execute_CanBeHacked(this, nullptr))
	{
		return false;
	}

	if (!bHacked)
	{
		SetHackedState(true);
	}

	StartHackDurationTimer(FMath::Max(0.1f, DurationSeconds));
	return true;
}

// server timer 만료 시 replicated hack 상태를 해제
void AMonsterBase::ExpireHackDuration()
{
	if (!HasAuthority())
	{
		return;
	}

	SetHackedState(false);
}

void AMonsterBase::ClearHackDurationTimer()
{
	GetWorldTimerManager().ClearTimer(HackDurationTimerHandle);
}

FPragmataScanResult AMonsterBase::GetPragmataScanResult_Implementation(AActor* ScanRequester) const
{
	FPragmataScanResult Result;
	Result.bIsValid = true;
	Result.TargetActor = const_cast<AMonsterBase*>(this);
	Result.TargetWorldLocation = GetActorLocation();
	Result.TargetId = MonsterId;
	Result.TargetKind = EPragmataScanTargetKind::Enemy;
	Result.DisplayText = ScanDisplayText.IsEmpty() ? FText::FromName(MonsterId) : ScanDisplayText;
	// UDianaScanComponent이 camera 기준 최종 거리를 덮어쓰며, 이 값은 다른 requester용 fallback
	Result.Distance = ScanRequester ? FVector::Dist(ScanRequester->GetActorLocation(), GetActorLocation()) : 0.f;
	return Result;
}

float AMonsterBase::GetAttackRange() const
{
	const FMonsterAttackPattern* Pattern = GetSelectedAttackPattern();
	return Pattern ? Pattern->AttackRange : 0.f;
}

float AMonsterBase::GetAttackRadius() const
{
	const FMonsterAttackPattern* Pattern = GetSelectedAttackPattern();
	return Pattern ? Pattern->AttackRadius : 0.f;
}

float AMonsterBase::GetMinimumAttackRange() const
{
	const FMonsterAttackPattern* Pattern = GetSelectedAttackPattern();
	return Pattern ? Pattern->MinimumAttackRange : 0.f;
}

float AMonsterBase::GetAIAttackDetectRange() const
{
	return GetAttackRange() + GetAttackRadius() * 2.f;
}

float AMonsterBase::GetTurnSpeed() const
{
	const FMonsterAttackPattern* Pattern = GetSelectedAttackPattern();
	return Pattern ? Pattern->TurnSpeed : 0.f;
}

bool AMonsterBase::IsSelectedAttackPatternInRange(const AActor* Target) const
{
	return IsAttackPatternInRange(SelectedAttackPatternIndex, Target);
}

bool AMonsterBase::HasSelectedAttackPattern() const
{
	return GetSelectedAttackPattern() != nullptr;
}

// BT 회전 task와 공격 실행부가 선택된 pattern의 방향 유지 규칙을 공유
bool AMonsterBase::ShouldKeepCurrentFacingForSelectedAttackPattern() const
{
	const FMonsterAttackPattern* Pattern = GetSelectedAttackPattern();
	return Pattern && Pattern->bRequireTargetBehind;
}

// 패턴 선택하기.
// 쿨타임 준비 확인/가중치 우선순위 확인(이전 패턴은 우선순위 낮아짐 여부도 확인).
bool AMonsterBase::EnsureSelectedAttackPattern()
{
	if (!HasAuthority() || bIsAttacking)
	{
		return HasSelectedAttackPattern();
	}

	const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	const bool bHasAvailablePattern = HasAvailableAttackPattern(CurrentTime);
	const bool bHasAvailablePatternInRange = HasAvailableAttackPatternInRange(CurrentTime, TargetActor);
	if (HasSelectedAttackPattern()
		&& (!bHasAvailablePattern || IsAttackPatternAvailable(SelectedAttackPatternIndex, CurrentTime))
		&& IsAttackPatternAboveMinimumRange(SelectedAttackPatternIndex, TargetActor)
		&& IsAttackPatternPositionRequirementMet(SelectedAttackPatternIndex, TargetActor)
		&& (!bHasAvailablePatternInRange || IsAttackPatternInRange(SelectedAttackPatternIndex, TargetActor)))
	{
		return true;
	}

	SelectedAttackPatternIndex = INDEX_NONE;
	AttackPatternNextAllowedTimes.SetNum(AttackPatterns.Num());

	float TotalWeight = 0.f;
	for (int32 PatternIndex = 0; PatternIndex < AttackPatterns.Num(); ++PatternIndex)
	{
		const FMonsterAttackPattern& Pattern = AttackPatterns[PatternIndex];
		const float EffectiveWeight = GetEffectiveSelectionWeight(PatternIndex);
		const bool bValidPattern = Pattern.AttackMontage && EffectiveWeight > 0.f;
		const bool bAvailable = !bHasAvailablePattern || IsAttackPatternAvailable(PatternIndex, CurrentTime);
		const bool bAboveMinimumRange = IsAttackPatternAboveMinimumRange(PatternIndex, TargetActor);
		const bool bMatchesPositionRequirement = IsAttackPatternPositionRequirementMet(PatternIndex, TargetActor);
		const bool bMatchesCurrentRange = !bHasAvailablePatternInRange
			|| IsAttackPatternInRange(PatternIndex, TargetActor);
		if (bValidPattern && bAvailable && bAboveMinimumRange && bMatchesPositionRequirement && bMatchesCurrentRange)
		{
			TotalWeight += EffectiveWeight;
		}
	}

	if (TotalWeight <= 0.f)
	{
		return false;
	}

	float RandomWeight = FMath::FRandRange(0.f, TotalWeight);
	for (int32 PatternIndex = 0; PatternIndex < AttackPatterns.Num(); ++PatternIndex)
	{
		const FMonsterAttackPattern& Pattern = AttackPatterns[PatternIndex];
		const float EffectiveWeight = GetEffectiveSelectionWeight(PatternIndex);
		const bool bValidPattern = Pattern.AttackMontage && EffectiveWeight > 0.f;
		const bool bUnavailable = bHasAvailablePattern && !IsAttackPatternAvailable(PatternIndex, CurrentTime);
		const bool bBelowMinimumRange = !IsAttackPatternAboveMinimumRange(PatternIndex, TargetActor);
		const bool bPositionRequirementFailed = !IsAttackPatternPositionRequirementMet(PatternIndex, TargetActor);
		const bool bOutsidePreferredRange = bHasAvailablePatternInRange
			&& !IsAttackPatternInRange(PatternIndex, TargetActor);
		if (!bValidPattern || bUnavailable || bBelowMinimumRange || bPositionRequirementFailed || bOutsidePreferredRange)
		{
			continue;
		}

		RandomWeight -= EffectiveWeight;
		if (RandomWeight <= 0.f)
		{
			SelectedAttackPatternIndex = PatternIndex;
			return true;
		}
	}

	return false;
}

void AMonsterBase::ClearSelectedAttackPattern()
{
	SelectedAttackPatternIndex = INDEX_NONE;
}

const FMonsterAttackPattern* AMonsterBase::GetSelectedAttackPattern() const
{
	return AttackPatterns.IsValidIndex(SelectedAttackPatternIndex)
		? &AttackPatterns[SelectedAttackPatternIndex]
		: nullptr;
}

// 공격 패턴 조건 검사
bool AMonsterBase::IsAttackPatternAvailable(int32 PatternIndex, float CurrentTime) const
{
	if (!AttackPatterns.IsValidIndex(PatternIndex))
	{
		return false;
	}

	const FMonsterAttackPattern& Pattern = AttackPatterns[PatternIndex];
	if (!Pattern.AttackMontage || GetEffectiveSelectionWeight(PatternIndex) <= 0.f)
	{
		return false;
	}

	const float NextAllowedTime = AttackPatternNextAllowedTimes.IsValidIndex(PatternIndex)
		? AttackPatternNextAllowedTimes[PatternIndex]
		: 0.f;
	return CurrentTime >= NextAllowedTime;
}

bool AMonsterBase::IsAttackPatternInRange(int32 PatternIndex, const AActor* Target) const
{
	if (!AttackPatterns.IsValidIndex(PatternIndex) || !Target)
	{
		return false;
	}

	const FMonsterAttackPattern& Pattern = AttackPatterns[PatternIndex];
	const float DistanceToTarget = FVector::Dist2D(GetActorLocation(), Target->GetActorLocation());
	const float MinimumRange = FMath::Max(0.f, Pattern.MinimumAttackRange);
	const float MaximumRange = FMath::Max(MinimumRange, Pattern.AttackRange + Pattern.AttackRadius * 2.f);
	return DistanceToTarget >= MinimumRange && DistanceToTarget <= MaximumRange;
}

bool AMonsterBase::IsAttackPatternAboveMinimumRange(int32 PatternIndex, const AActor* Target) const
{
	if (!AttackPatterns.IsValidIndex(PatternIndex) || !Target)
	{
		return false;
	}

	const float DistanceToTarget = FVector::Dist2D(GetActorLocation(), Target->GetActorLocation());
	return DistanceToTarget >= FMath::Max(0.f, AttackPatterns[PatternIndex].MinimumAttackRange);
}

// 위치 조건이 켜진 pattern만 target 방향을 추가 필터로 검사
bool AMonsterBase::IsAttackPatternPositionRequirementMet(int32 PatternIndex, const AActor* Target) const
{
	if (!AttackPatterns.IsValidIndex(PatternIndex))
	{
		return false;
	}

	const FMonsterAttackPattern& Pattern = AttackPatterns[PatternIndex];
	if (!Pattern.bRequireTargetBehind)
	{
		return true;
	}

	if (!Target)
	{
		return false;
	}

	FVector DirectionToTarget = Target->GetActorLocation() - GetActorLocation();
	DirectionToTarget.Z = 0.f;
	if (!DirectionToTarget.Normalize())
	{
		return false;
	}

	const float HalfAngle = FMath::Clamp(Pattern.TargetBehindAngleDegrees, 0.f, 360.f) * 0.5f;
	const float RequiredDot = FMath::Cos(FMath::DegreesToRadians(HalfAngle));
	const float BehindDot = FVector::DotProduct(-GetActorForwardVector(), DirectionToTarget);
	return BehindDot >= RequiredDot;
}

float AMonsterBase::GetEffectiveSelectionWeight(int32 PatternIndex) const
{
	if (!AttackPatterns.IsValidIndex(PatternIndex))
	{
		return 0.f;
	}

	const FMonsterAttackPattern& Pattern = AttackPatterns[PatternIndex];
	if (PatternIndex == LastUsedAttackPatternIndex && !Pattern.bAllowConsecutiveUse)
	{
		return 0.f;
	}

	return FMath::Max(0.f, Pattern.SelectionWeight);
}

bool AMonsterBase::HasAvailableAttackPattern(float CurrentTime) const
{
	for (int32 PatternIndex = 0; PatternIndex < AttackPatterns.Num(); ++PatternIndex)
	{
		if (IsAttackPatternAvailable(PatternIndex, CurrentTime))
		{
			return true;
		}
	}

	return false;
}

bool AMonsterBase::HasAvailableAttackPatternInRange(float CurrentTime, const AActor* Target) const
{
	for (int32 PatternIndex = 0; PatternIndex < AttackPatterns.Num(); ++PatternIndex)
	{
		if (IsAttackPatternAvailable(PatternIndex, CurrentTime)
			&& IsAttackPatternInRange(PatternIndex, Target)
			&& IsAttackPatternPositionRequirementMet(PatternIndex, Target))
		{
			return true;
		}
	}

	return false;
}

void AMonsterBase::SpawnDamageText(float DamageAmount, const FPragmataDamageSpec& DamageSpec)
{
	FVector DamageNumberLocation = GetActorLocation() + FVector(0.f, 0.f, 100.f);
	if (DamageSpec.bHasImpact)
	{
		DamageNumberLocation = DamageSpec.ImpactHit.Location;
		DamageNumberLocation.Z += 20.f;
		DamageNumberLocation.X += FMath::FRandRange(-10.f, 10.f);
	}

	MulticastSpawnDamageText(DamageAmount, DamageNumberLocation);
}

void AMonsterBase::MulticastSpawnDamageText_Implementation(float DamageAmount, FVector DamageNumberLocation)
{
	if (!DamageTextClass || !GetWorld() || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	ADamageText* SpawnedDamageText = Cast<ADamageText>(GetWorld()->SpawnActor<AActor>(DamageTextClass, DamageNumberLocation, FRotator::ZeroRotator));
	if (SpawnedDamageText)
	{
		SpawnedDamageText->Initialize(DamageAmount);
	}
}

void AMonsterBase::SetAIMoveSpeedMode(EMonsterMoveSpeedMode MoveSpeedMode)
{
	// 서버 예외처리
	if (!HasAuthority())
	{
		return;
	}

	float MovementSpeed = 300.f;

	if (MoveSpeedMode == EMonsterMoveSpeedMode::Default)
	{
		MovementSpeed = 300.f;
	}
	else if (MoveSpeedMode == EMonsterMoveSpeedMode::Patrol)
	{
		MovementSpeed = 150.f;
	}
	else if (MoveSpeedMode == EMonsterMoveSpeedMode::Chase)
	{
		MovementSpeed = 450.f;
	}
	else if (MoveSpeedMode == EMonsterMoveSpeedMode::Slow)
	{
		MovementSpeed = 100.f;
	}
	else if (MoveSpeedMode == EMonsterMoveSpeedMode::Fast)
	{
		MovementSpeed = 450.f;
	}
	else
	{
		MovementSpeed = 300.f;
	}

	// 최종 MovementSpeed를 CharacterMovement에 반영
	GetCharacterMovement()->MaxWalkSpeed = MovementSpeed;
}

// 공격 여부 델리게이트 저장
void AMonsterBase::SetAIAttackDelegate(const FMonsterAttackFinished& InOnAttackFinished)
{
	OnAttackFinished = InOnAttackFinished;
}

void AMonsterBase::ClearAIAttackDelegate()
{
	OnAttackFinished.Unbind();
}

void AMonsterBase::StopAttackRotationTracking()
{
	if (HasAuthority())
	{
		bTrackTargetDuringAttack = false;
	}
}

bool AMonsterBase::AttackByAI()
{
	if (!HasAuthority()
		|| bIsAttacking
		|| !CanAttack()
		|| !EnsureSelectedAttackPattern()
		|| !IsSelectedAttackPatternInRange(TargetActor))
	{
		return false;
	}

	const FMonsterAttackPattern* Pattern = GetSelectedAttackPattern();
	if (!Pattern)
	{
		return false;
	}

	ResetProximityControlledAttackLoop();

	UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (!AnimInstance)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AMonsterBase::AttackByAI] AnimInstance is nullptr"));
		return false;
	}

	const bool bKeepCurrentFacingForAttack = ShouldKeepCurrentFacingForSelectedAttackPattern();

	// 뒤쪽 조건 공격은 현재 방향을 유지해야 animation의 후방 이동이 target 쪽으로 향함
	if (!bKeepCurrentFacingForAttack && TargetActor)
	{
		FVector LookDirection = TargetActor->GetActorLocation() - GetActorLocation();
		LookDirection.Z = 0.f;
		if (LookDirection.Normalize())
		{
			SetActorRotation(FRotator(0.f, LookDirection.Rotation().Yaw, 0.f));
		}
	}

	const float MontageDuration = AnimInstance->Montage_Play(Pattern->AttackMontage);
	if (MontageDuration <= 0.f)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AMonsterBase::AttackByAI] Failed to play AttackMontage"));
		return false;
	}

	if (Pattern->bUseProximityControlledMontageLoop)
	{
		if (Pattern->EndSectionName.IsNone()
			|| !Pattern->AttackMontage->IsValidSectionName(Pattern->EndSectionName))
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("[AMonsterBase::AttackByAI] Invalid end section for '%s'"),
				*GetNameSafe(Pattern->AttackMontage));
			AnimInstance->Montage_Stop(0.f, Pattern->AttackMontage);
			return false;
		}

		bProximityControlledAttackLoopActive = true;
	}

	if (Pattern->bUseFlyingMovementDuringAttackMontage)
	{
		// pattern별 공중 공격 loop가 CharacterMovement의 Flying 속도로 진행되게 전환
		BeginAttackFlyingMovement(*Pattern);
	}

	bIsAttacking = true;
	bTrackTargetDuringAttack = !bKeepCurrentFacingForAttack;
	SetBlackboardCanAttack(false);

	if (AMonsterAIController* MonsterController = Cast<AMonsterAIController>(GetController()))
	{
		MonsterController->ResetOrbitDirection();
	}

	FOnMontageEnded MontageEndedDelegate;
	MontageEndedDelegate.BindUObject(this, &AMonsterBase::HandleAttackMontageEnded);
	AnimInstance->Montage_SetEndDelegate(MontageEndedDelegate, Pattern->AttackMontage);
	MulticastPlayAttackMontage(Pattern->AttackMontage);

	return true;
}

void AMonsterBase::MulticastPlayAttackMontage_Implementation(UAnimMontage* Montage)
{
	if (HasAuthority() || !Montage)
	{
		return;
	}

	if (UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
	{
		AnimInstance->Montage_Play(Montage);
	}
}

void AMonsterBase::MulticastJumpAttackMontageToEnd_Implementation(UAnimMontage* Montage, FName EndSectionName)
{
	if (HasAuthority() || !Montage || EndSectionName.IsNone())
	{
		return;
	}

	UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (AnimInstance && AnimInstance->Montage_IsPlaying(Montage) && Montage->IsValidSectionName(EndSectionName))
	{
		AnimInstance->Montage_JumpToSection(EndSectionName, Montage);
	}
}

void AMonsterBase::MulticastSetAttackLoopRootMotionIgnored_Implementation(bool bIgnoreRootMotion)
{
	SetAttackLoopRootMotionIgnored(bIgnoreRootMotion);
}

// server가 확정한 appearance section 전환을 client presentation에 반영
void AMonsterBase::MulticastJumpSpawnAppearanceToSection_Implementation(UAnimMontage* Montage, FName SectionName)
{
	if (HasAuthority())
	{
		return;
	}

	JumpSpawnAppearanceToSectionLocally(Montage, SectionName);
}

// server가 고른 section을 client에서도 재생하고 진행 중인 공격 montage를 중단
void AMonsterBase::MulticastPlayHalfHealthHitReactMontage_Implementation(
	UAnimMontage* Montage,
	FName SectionName)
{
	if (HasAuthority() || !Montage || SectionName.IsNone())
	{
		return;
	}

	UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (!AnimInstance || !Montage->IsValidSectionName(SectionName))
	{
		return;
	}

	const float MontageDuration = AnimInstance->Montage_Play(
		Montage,
		1.f,
		EMontagePlayReturnType::MontageLength,
		0.f,
		true);
	if (MontageDuration > 0.f)
	{
		AnimInstance->Montage_JumpToSection(SectionName, Montage);
	}
}

void AMonsterBase::UpdateProximityControlledAttackLoop()
{
	if (!HasAuthority() || !bIsAttacking || !bProximityControlledAttackLoopActive || bAttackLoopEndRequested)
	{
		return;
	}

	const FMonsterAttackPattern* Pattern = GetSelectedAttackPattern();
	UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	UWorld* World = GetWorld();
	if (!Pattern || !AnimInstance || !World || !Pattern->AttackMontage)
	{
		return;
	}

	const FName CurrentSectionName = AnimInstance->Montage_GetCurrentSection(Pattern->AttackMontage);
	const int32 CurrentSectionIndex = Pattern->AttackMontage->GetSectionIndex(CurrentSectionName);
	if (CurrentSectionIndex == INDEX_NONE)
	{
		return;
	}

	const int32 NextSectionIndex = AnimInstance->Montage_GetNextSectionID(
		Pattern->AttackMontage,
		CurrentSectionIndex
	);
	if (NextSectionIndex != CurrentSectionIndex)
	{
		return;
	}

	if (AttackLoopStartTime < 0.f)
	{
		AttackLoopStartTime = World->GetTimeSeconds();
	}

	const bool bTargetLost = !IsValid(TargetActor);
	const bool bReachedTarget = !bTargetLost
		&& FVector::Dist2D(GetActorLocation(), TargetActor->GetActorLocation()) <= Pattern->EndTransitionDistance;
	const bool bLoopTimedOut = Pattern->MaxLoopDuration > 0.f
		&& World->GetTimeSeconds() - AttackLoopStartTime >= Pattern->MaxLoopDuration;
	if (bTargetLost || bReachedTarget || bLoopTimedOut)
	{
		// End section의 root motion은 기존 montage 설정으로 처리하도록 loop override를 먼저 복구
		if (bAttackLoopRootMotionModeSaved)
		{
			MulticastSetAttackLoopRootMotionIgnored(false);
		}

		// loop 중 근접·target 유실·시간 초과가 발생하면 end section으로 즉시 전환
		bAttackLoopEndRequested = true;
		AnimInstance->Montage_JumpToSection(Pattern->EndSectionName, Pattern->AttackMontage);
		MulticastJumpAttackMontageToEnd(Pattern->AttackMontage, Pattern->EndSectionName);
		return;
	}

	if (Pattern->bUseFlyingMovementDuringAttackMontage
		&& Pattern->FlyingLoopMoveSpeed > 0.f
		&& bAttackFlyingMovementActive)
	{
		// animation root motion 대신 CharacterMovement Flying 속도로 loop section에서만 실제 전진
		if (!bAttackLoopRootMotionModeSaved)
		{
			MulticastSetAttackLoopRootMotionIgnored(true);
		}

		AddMovementInput(GetActorForwardVector(), 1.f, true);
	}
}

void AMonsterBase::ResetProximityControlledAttackLoop()
{
	bProximityControlledAttackLoopActive = false;
	bAttackLoopEndRequested = false;
	AttackLoopStartTime = -1.f;
}

// 공중 공격은 montage 재생 동안만 Flying을 적용하고 loop 전진 속도를 기존 값과 분리해 보관
void AMonsterBase::BeginAttackFlyingMovement(const FMonsterAttackPattern& Pattern)
{
	if (!HasAuthority() || bAttackFlyingMovementActive)
	{
		return;
	}

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		SavedAttackMovementMode = Movement->MovementMode;
		SavedAttackCustomMovementMode = Movement->CustomMovementMode;
		SavedAttackMaxFlySpeed = Movement->MaxFlySpeed;
		if (Pattern.FlyingLoopMoveSpeed > 0.f)
		{
			Movement->MaxFlySpeed = Pattern.FlyingLoopMoveSpeed;
		}

		bAttackFlyingMovementActive = true;
		Movement->SetMovementMode(MOVE_Flying);
	}
}

// montage loop의 code-driven 이동 중에만 root motion translation을 무시하고 종료 시 AnimBP 설정을 복구
void AMonsterBase::SetAttackLoopRootMotionIgnored(bool bIgnoreRootMotion)
{
	UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (!AnimInstance)
	{
		return;
	}

	if (bIgnoreRootMotion)
	{
		if (bAttackLoopRootMotionModeSaved)
		{
			return;
		}

		SavedAttackLoopRootMotionMode = AnimInstance->RootMotionMode;
		bAttackLoopRootMotionModeSaved = true;
		AnimInstance->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
		return;
	}

	if (bAttackLoopRootMotionModeSaved)
	{
		AnimInstance->SetRootMotionMode(SavedAttackLoopRootMotionMode);
		bAttackLoopRootMotionModeSaved = false;
	}
}

// montage 종료·피격 취소·사망 정리에서 Flying 상태가 다음 AI 이동으로 남지 않게 복구
void AMonsterBase::RestoreAttackMovementMode()
{
	if (!HasAuthority() || !bAttackFlyingMovementActive)
	{
		return;
	}

	bAttackFlyingMovementActive = false;
	if (bAttackLoopRootMotionModeSaved)
	{
		MulticastSetAttackLoopRootMotionIgnored(false);
	}

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->MaxFlySpeed = SavedAttackMaxFlySpeed;
		if (!IsDead())
		{
			Movement->SetMovementMode(SavedAttackMovementMode, SavedAttackCustomMovementMode);
		}
	}

	SavedAttackMovementMode = MOVE_Walking;
	SavedAttackCustomMovementMode = 0;
	SavedAttackMaxFlySpeed = 0.f;
}

void AMonsterBase::FinishAttackByAI()
{
	if (!HasAuthority() || !bIsAttacking)
	{
		return;
	}

	bIsAttacking = false;
	bTrackTargetDuringAttack = false;
	ResetProximityControlledAttackLoop();
	// attack montage 종료 뒤 다음 BT 이동에 Flying 상태가 남지 않게 복구
	RestoreAttackMovementMode();
	StartSelectedAttackPatternCooldown();
	LastUsedAttackPatternIndex = SelectedAttackPatternIndex;
	ClearSelectedAttackPattern();
	StartCommonAttackCooldown();

	FMonsterAttackFinished AttackFinishedDelegate = OnAttackFinished;
	OnAttackFinished.Unbind();
	AttackFinishedDelegate.ExecuteIfBound();
}

void AMonsterBase::HandleAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	const FMonsterAttackPattern* Pattern = GetSelectedAttackPattern();
	if (Pattern && Montage == Pattern->AttackMontage)
	{
		FinishAttackByAI();
	}
}

// for Attack cooldown check
bool AMonsterBase::CanAttack() const
{
	const UWorld* World = GetWorld();
	if (!World || bIsAttacking || bHalfHealthHitReactPlaying || bSpawnAppearancePlaying || bStasisStunned)
	{
		return false;
	}

	const float CurrentTime = World->GetTimeSeconds();
	return CurrentTime >= NextAttackAllowedTime && HasAvailableAttackPattern(CurrentTime);
}

// Territory appearance를 server에서 시작하고 이동·공격을 montage 종료까지 잠금
void AMonsterBase::PlaySpawnAppearance()
{
	TryPlaySpawnAppearance();
}

bool AMonsterBase::HasSpawnAppearanceMontage(UAnimMontage* AppearanceMontageOverride) const
{
	return AppearanceMontageOverride || SpawnAppearanceMontage;
}

// Territory는 반환값으로 appearance 후 BT 시작 대기 여부를 결정
bool AMonsterBase::TryPlaySpawnAppearance(
	UAnimMontage* AppearanceMontageOverride,
	bool bUseFallingAppearance,
	FName FallingEndSectionName,
	float FallingEndTransitionDistance)
{
	UAnimMontage* MontageToPlay = AppearanceMontageOverride ? AppearanceMontageOverride : SpawnAppearanceMontage.Get();
	if (!HasAuthority() || !MontageToPlay || bSpawnAppearancePlaying || IsDead())
	{
		return false;
	}

	ActiveSpawnAppearanceMontage = MontageToPlay;
	bSpawnAppearancePlaying = true;
	bSpawnAppearanceUsesFalling = bUseFallingAppearance;
	bSpawnAppearanceIgnoresRootMotion = bUseFallingAppearance;
	bSpawnAppearanceEndRequested = false;
	SpawnAppearanceFallingEndSectionName = FallingEndSectionName;
	SpawnAppearanceFallingEndTransitionDistance = FMath::Max(0.f, FallingEndTransitionDistance);
	SetBlackboardCanAttack(false);
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		if (bSpawnAppearanceUsesFalling)
		{
			Movement->SetMovementMode(MOVE_Falling);
		}
		else
		{
			Movement->DisableMovement();
		}
	}

	if (!PlaySpawnAppearanceLocally(true))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[AMonsterBase::TryPlaySpawnAppearance] Local play failed Monster=%s Montage=%s AnimInstance=%s Mesh=%s"),
			*GetNameSafe(this),
			*GetNameSafe(MontageToPlay),
			*GetNameSafe(GetMesh() ? GetMesh()->GetAnimInstance() : nullptr),
			*GetNameSafe(GetMesh()));
		bSpawnAppearancePlaying = false;
		ActiveSpawnAppearanceMontage = nullptr;
		bSpawnAppearanceUsesFalling = false;
		bSpawnAppearanceIgnoresRootMotion = false;
		bSpawnAppearanceEndRequested = false;
		SpawnAppearanceFallingEndSectionName = NAME_None;
		SpawnAppearanceFallingEndTransitionDistance = 0.f;
		RestoreSpawnAppearanceRootMotionMode();
		if (UCharacterMovementComponent* Movement = GetCharacterMovement())
		{
			Movement->SetMovementMode(bUseFallingAppearance ? MOVE_Falling : MOVE_Walking);
		}
		RefreshAttackAvailability();
		return false;
	}
	ForceNetUpdate();
	return true;
}

// Territory fallback timer가 appearance 대기 시간을 계산할 때 사용
float AMonsterBase::GetSpawnAppearanceMontagePlayLength(UAnimMontage* AppearanceMontageOverride) const
{
	const UAnimMontage* MontageToPlay = AppearanceMontageOverride ? AppearanceMontageOverride : SpawnAppearanceMontage.Get();
	return MontageToPlay ? MontageToPlay->GetPlayLength() : 0.f;
}

// appearance 종료 delegate가 누락돼도 Territory 전투 시작이 멈추지 않게 상태를 정리
bool AMonsterBase::ForceFinishSpawnAppearance()
{
	if (!HasAuthority() || !bSpawnAppearancePlaying || !ActiveSpawnAppearanceMontage)
	{
		return false;
	}

	UAnimMontage* MontageToFinish = ActiveSpawnAppearanceMontage;
	if (UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
	{
		if (AnimInstance->Montage_IsPlaying(MontageToFinish))
		{
			AnimInstance->Montage_Stop(0.1f, MontageToFinish);
		}
	}

	HandleSpawnAppearanceMontageEnded(MontageToFinish, true);
	return true;
}

// Territory 연출 대기 monster는 possession 직후 BT 자동 시작을 막음
void AMonsterBase::SetAutoRunAIOnPossess(bool bNewAutoRunAIOnPossess)
{
	bAutoRunAIOnPossess = bNewAutoRunAIOnPossess;
}

void AMonsterBase::OnRep_SpawnAppearancePlaying()
{
	UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (bSpawnAppearancePlaying)
	{
		PlaySpawnAppearanceLocally(false);
	}
	else if (AnimInstance && ActiveSpawnAppearanceMontage && AnimInstance->Montage_IsPlaying(ActiveSpawnAppearanceMontage))
	{
		AnimInstance->Montage_Stop(0.1f, ActiveSpawnAppearanceMontage);
		RestoreSpawnAppearanceRootMotionMode();
	}
	else
	{
		RestoreSpawnAppearanceRootMotionMode();
	}
}

// client는 복제된 stun 상태로 montage 표시만 맞춤
void AMonsterBase::OnRep_StasisStunned()
{
	if (bStasisStunned)
	{
		PlayStasisStunLocally();
	}
	else
	{
		StopStasisStunLocally();
	}
}

bool AMonsterBase::PlaySpawnAppearanceLocally(bool bBindEndDelegate)
{
	UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	UAnimMontage* MontageToPlay = ActiveSpawnAppearanceMontage ? ActiveSpawnAppearanceMontage.Get() : SpawnAppearanceMontage.Get();
	if (!AnimInstance || !MontageToPlay)
	{
		return false;
	}

	if (AnimInstance->Montage_IsPlaying(MontageToPlay))
	{
		if (bSpawnAppearanceIgnoresRootMotion)
		{
			ApplySpawnAppearanceRootMotionMode();
		}
		if (bBindEndDelegate)
		{
			FOnMontageEnded MontageEndedDelegate;
			MontageEndedDelegate.BindUObject(this, &ThisClass::HandleSpawnAppearanceMontageEnded);
			AnimInstance->Montage_SetEndDelegate(MontageEndedDelegate, MontageToPlay);
		}
		return true;
	}

	if (bSpawnAppearanceIgnoresRootMotion)
	{
		ApplySpawnAppearanceRootMotionMode();
	}

	const float MontageDuration = AnimInstance->Montage_Play(MontageToPlay);
	if (MontageDuration <= 0.f)
	{
		RestoreSpawnAppearanceRootMotionMode();
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[AMonsterBase::PlaySpawnAppearanceLocally] Failed to play SpawnAppearanceMontage Monster=%s AnimInstance=%s Montage=%s SlotCount=%d"),
			*GetNameSafe(this),
			*GetNameSafe(AnimInstance),
			*GetNameSafe(MontageToPlay),
			MontageToPlay->SlotAnimTracks.Num());
		return false;
	}

	if (bBindEndDelegate)
	{
		FOnMontageEnded MontageEndedDelegate;
		MontageEndedDelegate.BindUObject(this, &ThisClass::HandleSpawnAppearanceMontageEnded);
		AnimInstance->Montage_SetEndDelegate(MontageEndedDelegate, MontageToPlay);
	}
	return true;
}

// server appearance 종료 후 AI 이동과 공격 가능 상태 복구
void AMonsterBase::HandleSpawnAppearanceMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (!HasAuthority() || Montage != ActiveSpawnAppearanceMontage || !bSpawnAppearancePlaying)
	{
		return;
	}

	const bool bWasFallingAppearance = bSpawnAppearanceUsesFalling;
	bSpawnAppearancePlaying = false;
	bSpawnAppearanceUsesFalling = false;
	bSpawnAppearanceIgnoresRootMotion = false;
	bSpawnAppearanceEndRequested = false;
	SpawnAppearanceFallingEndSectionName = NAME_None;
	SpawnAppearanceFallingEndTransitionDistance = 0.f;
	RestoreSpawnAppearanceRootMotionMode();
	if (!IsDead())
	{
		if (UCharacterMovementComponent* Movement = GetCharacterMovement())
		{
			Movement->SetMovementMode(
				bWasFallingAppearance && !Movement->IsMovingOnGround()
					? MOVE_Falling
					: MOVE_Walking);
		}
		RefreshAttackAvailability();
	}
	ForceNetUpdate();
	OnSpawnAppearanceFinished.Broadcast(this);
}

// field source별 timer를 분리해 여러 StasisNet field가 겹쳐도 마지막 source까지 정지 유지
bool AMonsterBase::ApplyStasisStun(AActor* StunSource, float Duration)
{
	if (!HasAuthority() || !IsValid(StunSource) || Duration <= 0.f || IsDead())
	{
		return false;
	}

	const TWeakObjectPtr<AActor> StunSourceKey(StunSource);
	FTimerHandle& StunTimerHandle = StasisStunSourceTimers.FindOrAdd(StunSourceKey);
	GetWorldTimerManager().ClearTimer(StunTimerHandle);

	FTimerDelegate StunExpiredDelegate;
	StunExpiredDelegate.BindUObject(this, &ThisClass::HandleStasisStunExpired, StunSourceKey);
	GetWorldTimerManager().SetTimer(StunTimerHandle, StunExpiredDelegate, Duration, false);

	if (!bStasisStunned)
	{
		bStasisStunned = true;
		BeginStasisStun();
	}

	ForceNetUpdate();
	return true;
}

// field 이탈이나 소멸 시 해당 source가 부여한 stun만 제거
void AMonsterBase::RemoveStasisStun(AActor* StunSource)
{
	if (!HasAuthority() || !StunSource)
	{
		return;
	}

	const TWeakObjectPtr<AActor> StunSourceKey(StunSource);
	if (FTimerHandle* StunTimerHandle = StasisStunSourceTimers.Find(StunSourceKey))
	{
		GetWorldTimerManager().ClearTimer(*StunTimerHandle);
		StasisStunSourceTimers.Remove(StunSourceKey);
	}

	if (StasisStunSourceTimers.Num() == 0 && bStasisStunned)
	{
		bStasisStunned = false;
		EndStasisStun();
		ForceNetUpdate();
	}
}

// 최대 stun 시간 만료 시 source 목록에서 제거하고 필요하면 이동을 복구
void AMonsterBase::HandleStasisStunExpired(TWeakObjectPtr<AActor> StunSource)
{
	if (!HasAuthority())
	{
		return;
	}

	StasisStunSourceTimers.Remove(StunSource);
	if (StasisStunSourceTimers.Num() == 0 && bStasisStunned)
	{
		bStasisStunned = false;
		EndStasisStun();
		ForceNetUpdate();
	}
}

// stun 진입은 현재 공격과 AI 이동을 끊고 field/timer가 해제할 때까지 잠금
void AMonsterBase::BeginStasisStun()
{
	if (!HasAuthority())
	{
		return;
	}

	// StasisNet은 공격 montage와 AI 이동을 끊고 source별 timer로 복구 시점을 관리
	if (bIsAttacking)
	{
		const FMonsterAttackPattern* Pattern = GetSelectedAttackPattern();
		UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
		if (Pattern && Pattern->AttackMontage && AnimInstance && AnimInstance->Montage_IsPlaying(Pattern->AttackMontage))
		{
			AnimInstance->Montage_Stop(0.1f, Pattern->AttackMontage);
		}
	}
	AbortAttackForDeath();
	GetWorldTimerManager().ClearTimer(AttackCooldownTimerHandle);
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->DisableMovement();
	}
	PlayStasisStunLocally();
}

// stun 종료는 사망과 spawn appearance를 침범하지 않는 범위에서 이동과 공격 가능성을 복구
void AMonsterBase::EndStasisStun()
{
	StopStasisStunLocally();

	if (!HasAuthority() || IsDead())
	{
		return;
	}

	if (!bSpawnAppearancePlaying)
	{
		if (UCharacterMovementComponent* Movement = GetCharacterMovement())
		{
			Movement->SetMovementMode(MOVE_Walking);
		}
		RefreshAttackAvailability();
	}
	else
	{
		SetBlackboardCanAttack(false);
	}
}

// stun montage는 선택 사항이라 asset이 없으면 gameplay 정지만 유지
void AMonsterBase::PlayStasisStunLocally()
{
	UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (!AnimInstance || !StasisStunMontage || IsDead())
	{
		return;
	}

	if (!AnimInstance->Montage_IsPlaying(StasisStunMontage))
	{
		AnimInstance->Montage_Play(StasisStunMontage);
	}
}

// stun 해제와 사망 정리에서 선택 montage 잔류를 제거
void AMonsterBase::StopStasisStunLocally()
{
	UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (AnimInstance && StasisStunMontage && AnimInstance->Montage_IsPlaying(StasisStunMontage))
	{
		AnimInstance->Montage_Stop(0.1f, StasisStunMontage);
	}
}

void AMonsterBase::StartCommonAttackCooldown()
{
	if (!HasAuthority())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	float Cooldown = FMath::Max(0.f, CombatData.CommonAttackCooldown);
	if (CombatData.bUseRandomCommonAttackCooldown)
	{
		const float CooldownMin = FMath::Max(0.f, CombatData.CommonAttackCooldownMin);
		const float CooldownMax = FMath::Max(CooldownMin, CombatData.CommonAttackCooldownMax);
		Cooldown = FMath::FRandRange(CooldownMin, CooldownMax);
	}

	// server가 공격 종료마다 한 번만 뽑아 NoAttack 재진입 시간을 권한 기준으로 확정
	NextAttackAllowedTime = World->GetTimeSeconds() + Cooldown;
	RefreshAttackAvailability();
}

// 개별 패턴 쿨타임 저
void AMonsterBase::StartSelectedAttackPatternCooldown()
{
	if (!HasAuthority() || !AttackPatterns.IsValidIndex(SelectedAttackPatternIndex))
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	AttackPatternNextAllowedTimes.SetNum(AttackPatterns.Num());
	const float PatternCooldown = FMath::Max(0.f, AttackPatterns[SelectedAttackPatternIndex].AttackCooldown);
	AttackPatternNextAllowedTimes[SelectedAttackPatternIndex] = World->GetTimeSeconds() + PatternCooldown;
}

// 공격 가능 여부 판단.
void AMonsterBase::RefreshAttackAvailability()
{
	if (!HasAuthority())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FTimerManager& TimerManager = World->GetTimerManager();
	TimerManager.ClearTimer(AttackCooldownTimerHandle);
	// 반피 피격이나 spawn appearance 종료 전에는 cooldown timer가 공격을 다시 열지 않게 차단
	if (bHalfHealthHitReactPlaying || bSpawnAppearancePlaying || bStasisStunned)
	{
		SetBlackboardCanAttack(false);
		return;
	}

	if (CanAttack())
	{
		SetBlackboardCanAttack(true);
		return;
	}

	SetBlackboardCanAttack(false);

	const float CurrentTime = World->GetTimeSeconds();
	float NextAvailableTime = TNumericLimits<float>::Max();
	for (int32 PatternIndex = 0; PatternIndex < AttackPatterns.Num(); ++PatternIndex)
	{
		const FMonsterAttackPattern& Pattern = AttackPatterns[PatternIndex];
		if (!Pattern.AttackMontage || GetEffectiveSelectionWeight(PatternIndex) <= 0.f)
		{
			continue;
		}

		const float PatternReadyTime = AttackPatternNextAllowedTimes.IsValidIndex(PatternIndex)
			? AttackPatternNextAllowedTimes[PatternIndex]
			: 0.f;
		NextAvailableTime = FMath::Min(NextAvailableTime, FMath::Max(NextAttackAllowedTime, PatternReadyTime));
	}

	if (NextAvailableTime == TNumericLimits<float>::Max())
	{
		return;
	}

	const float Delay = FMath::Max(KINDA_SMALL_NUMBER, NextAvailableTime - CurrentTime);
	TimerManager.SetTimer(
		AttackCooldownTimerHandle,
		this,
		&AMonsterBase::RefreshAttackAvailability,
		Delay,
		false
	);
}

void AMonsterBase::SetBlackboardCanAttack(bool bCanAttack) const
{
	if (AMonsterAIController* MonsterController = Cast<AMonsterAIController>(GetController()))
	{
		MonsterController->SetCanAttack(bCanAttack);
	}
}

// AI Perception의 시야·피격 stimulus가 들어오면 공통 target 획득 경로로 전달
void AMonsterBase::OnTargetDetected(AActor* Actor, FAIStimulus Stimulus)
{
	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[DamageAggro][OnTargetDetected] Monster=%s Actor=%s Success=%d Strength=%.2f StimulusLocation=%s ReceiverLocation=%s Tag=%s"),
		*GetNameSafe(this),
		*GetNameSafe(Actor),
		Stimulus.WasSuccessfullySensed() ? 1 : 0,
		Stimulus.Strength,
		*Stimulus.StimulusLocation.ToCompactString(),
		*Stimulus.ReceiverLocation.ToCompactString(),
		*Stimulus.Tag.ToString());

	//initial setup
	if (Stimulus.WasSuccessfullySensed())
	{
		// 감지 성공
		TrySetTargetIfPerceived(Actor);
	}
	else
	{
		// 감지 상실
		// TargetActor = nullptr;은 TerritoryActor에서 설정함.
	}
}

void AMonsterBase::HandleDeath()
{
	if (HasAuthority())
	{
		if (APragmataGameState* GameState = GetWorld() ? GetWorld()->GetGameState<APragmataGameState>() : nullptr)
		{
			if (GameState->GetActiveBossTarget() == this)
			{
				if (ADianaOperatorPawn* DianaPawn = GameState->GetDianaOperatorPawn())
				{
					DianaPawn->GetVoiceComponent()->PlayVoiceAction(EDianaVoiceAction::Victory);
				}
			}
		}
	}

	SetDead();
}

// 사망 event를 server gameplay 정리와 각 peer의 animation 정리에 한 번만 반영
void AMonsterBase::SetDead()
{
	if (bDeathHandled)
	{
		return;
	}
	bDeathHandled = true;
	// 사망 직후 target이 남아 있어도 HP bar를 먼저 숨김
	ApplyTargetHPBarVisibility();

	// BP에서 붙인 HP bar 같은 monster presentation widget은 사망 pose·ragdoll에 끌려가지 않게 즉시 숨김
	TInlineComponentArray<UWidgetComponent*> WidgetComponents(this);
	for (UWidgetComponent* WidgetComponent : WidgetComponents)
	{
		if (WidgetComponent)
		{
			WidgetComponent->SetVisibility(false, true);
			WidgetComponent->SetHiddenInGame(true, true);
		}
	}

	if (HasAuthority())
	{
		// killer/source와 무관하게 authoritative death 확정마다 shared currency를 한 번만 지급
		if (LunafilamentReward > 0)
		{
			if (APragmataGameState* GameState = GetWorld() ? GetWorld()->GetGameState<APragmataGameState>() : nullptr)
			{
				if (AHughCharacter* HughCharacter = GameState->GetHughCharacter())
				{
					if (UPragmataPlayerStatsComponent* PlayerStats = HughCharacter->GetPlayerStatsComponent())
					{
						PlayerStats->AddLunafilament(LunafilamentReward);
					}
				}
			}
		}

		for (TPair<TWeakObjectPtr<AActor>, FTimerHandle>& StunTimerPair : StasisStunSourceTimers)
		{
			GetWorldTimerManager().ClearTimer(StunTimerPair.Value);
		}
		StasisStunSourceTimers.Reset();
		bStasisStunned = false;
		// 사망 중단에서도 appearance root motion override가 다음 montage에 남지 않게 정리
		bSpawnAppearancePlaying = false;
		bSpawnAppearanceUsesFalling = false;
		bSpawnAppearanceIgnoresRootMotion = false;
		bSpawnAppearanceEndRequested = false;
		RestoreSpawnAppearanceRootMotionMode();

		// BT abort가 공격 완료 callback을 실행하지 않도록 먼저 해제
		ClearAIAttackDelegate();
		if (AMonsterAIController* MonsterController = Cast<AMonsterAIController>(GetController()))
		{
			MonsterController->StopAI();
		}

		AbortAttackForDeath();
		GetWorldTimerManager().ClearTimer(AttackCooldownTimerHandle);

		if (UCharacterMovementComponent* Movement = GetCharacterMovement())
		{
			Movement->StopMovementImmediately();
			Movement->SetMovementMode(EMovementMode::MOVE_None);
		}

		// AIController가 없더라도 combat 등록은 반드시 정리
		if (APragmataGameState* GameState = GetWorld() ? GetWorld()->GetGameState<APragmataGameState>() : nullptr)
		{
			GameState->UnregisterEnemyAggro(this);
		}

		if (DeadEventDelayTime > 0.f)
		{
			SetLifeSpan(DeadEventDelayTime);
		}
	}

	// replicated death event를 받은 각 peer가 multicast 공격 montage를 직접 중단
	if (USkeletalMeshComponent* MeshComponent = GetMesh())
	{
		if (UAnimInstance* AnimInstance = MeshComponent->GetAnimInstance())
		{
			AnimInstance->Montage_Stop(DeathMontageBlendOutTime);
		}

		MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// 바닥 collision은 유지하고 Pawn과의 충돌만 제거
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	}

	if (!bEnableDeathRagdoll)
	{
		return;
	}

	const float ResolvedRagdollDelay = FMath::Max(0.f, CurrentDeathRagdollDelay);
	if (ResolvedRagdollDelay <= 0.f)
	{
		EnableDeathRagdoll();
	}
	else
	{
		GetWorldTimerManager().SetTimer(
			DeathRagdollTimerHandle,
			this,
			&ThisClass::EnableDeathRagdoll,
			ResolvedRagdollDelay,
			false);
	}
}

// 사망 animation이 끊기는 지점부터 mesh physics가 낙하를 이어서 처리
void AMonsterBase::EnableDeathRagdoll()
{
	USkeletalMeshComponent* MeshComponent = GetMesh();
	if (!MeshComponent || !MeshComponent->GetPhysicsAsset())
	{
		UE_LOG(LogTemp, Warning, TEXT("[AMonsterBase::EnableDeathRagdoll] Physics Asset is missing"));
		return;
	}

	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	MeshComponent->SetCollisionProfileName(TEXT("Ragdoll"));
	MeshComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	MeshComponent->SetEnableGravity(true);
	MeshComponent->SetAllBodiesSimulatePhysics(true);
	MeshComponent->SetSimulatePhysics(true);
	MeshComponent->WakeAllRigidBodies();
	MeshComponent->bBlendPhysics = true;
}

// 사망 중단은 정상 공격 종료의 cooldown과 완료 delegate를 실행하지 않음
void AMonsterBase::AbortAttackForDeath()
{
	if (!HasAuthority())
	{
		return;
	}

	bIsAttacking = false;
	bTrackTargetDuringAttack = false;
	ResetProximityControlledAttackLoop();
	// 사망 또는 stun 취소 뒤 이동 정리 전에 공격 전 이동 모드로 복구
	RestoreAttackMovementMode();
	ClearSelectedAttackPattern();
	ClearAIAttackDelegate();
	SetBlackboardCanAttack(false);
}

// 치명타 직전 animation과 ragdoll 시간을 고정해 공격 상태 정리 이후에도 같은 설정 유지
UAnimSequenceBase* AMonsterBase::ResolveDeathAnimation(
	const FPragmataDamageSpec& DamageSpec,
	float& OutRagdollDelay) const
{
	FResolvedDeathAnimation ResolvedDeath;

	if (DamageSpec.DamageKind == EPragmataDamageKind::Hack)
	{
		ResolvedDeath = ResolveDirectionalDeathAnimation(
			DeathAnimations.bUseHackDirectionVariants,
			DeathAnimations.HackAnimation.Get(),
			DeathAnimations.HackRagdollDelay,
			DeathAnimations.HackFrontAnimation.Get(),
			DeathAnimations.HackFrontRagdollDelay,
			DeathAnimations.HackBackAnimation.Get(),
			DeathAnimations.HackBackRagdollDelay,
			DamageSpec.HitDirection);
	}
	else if (DamageSpec.DamageKind == EPragmataDamageKind::Melee)
	{
		ResolvedDeath = ResolveDirectionalDeathAnimation(
			DeathAnimations.bUseCriticalDirectionVariants,
			DeathAnimations.CriticalAnimation.Get(),
			DeathAnimations.CriticalRagdollDelay,
			DeathAnimations.CriticalFrontAnimation.Get(),
			DeathAnimations.CriticalFrontRagdollDelay,
			DeathAnimations.CriticalBackAnimation.Get(),
			DeathAnimations.CriticalBackRagdollDelay,
			DamageSpec.HitDirection);
	}
	else if (bIsAttacking)
	{
		const FMonsterAttackPattern* Pattern = GetSelectedAttackPattern();
		if (Pattern)
		{
			ResolvedDeath = FResolvedDeathAnimation{
				Pattern->DeathAnimation.Get(),
				Pattern->DeathRagdollDelay};
		}
	}
	else
	{
		ResolvedDeath = ResolveDirectionalDeathAnimation(
			DeathAnimations.bUseLocomotionDirectionVariants,
			DeathAnimations.LocomotionAnimation.Get(),
			DeathAnimations.LocomotionRagdollDelay,
			DeathAnimations.LocomotionFrontAnimation.Get(),
			DeathAnimations.LocomotionFrontRagdollDelay,
			DeathAnimations.LocomotionBackAnimation.Get(),
			DeathAnimations.LocomotionBackRagdollDelay,
			DamageSpec.HitDirection);
	}

	if (!ResolvedDeath.Animation)
	{
		ResolvedDeath = ResolveDirectionalDeathAnimation(
			DeathAnimations.bUseDefaultDirectionVariants,
			DeathAnimations.DefaultAnimation.Get(),
			DeathAnimations.DefaultRagdollDelay,
			DeathAnimations.DefaultFrontAnimation.Get(),
			DeathAnimations.DefaultFrontRagdollDelay,
			DeathAnimations.DefaultBackAnimation.Get(),
			DeathAnimations.DefaultBackRagdollDelay,
			DamageSpec.HitDirection);
	}

	OutRagdollDelay = ResolvedDeath.RagdollDelay >= 0.f
		? ResolvedDeath.RagdollDelay
		: DeathRagdollDelay;
	return ResolvedDeath.Animation;
}

// 공격 중에는 pattern 전용 section을 우선하고 없으면 일반/해킹 공통 section으로 fallback
FName AMonsterBase::ResolveHalfHealthHitReactSection(const FPragmataDamageSpec& DamageSpec) const
{
	const bool bHitFromBack = DamageSpec.HitDirection == EPragmataHitDirection::Back;
	if (bIsAttacking)
	{
		const FMonsterAttackPattern* Pattern = GetSelectedAttackPattern();
		if (Pattern)
		{
			const FName PatternSection = bHitFromBack
				? Pattern->HalfHealthHitReactBackSectionName
				: Pattern->HalfHealthHitReactFrontSectionName;
			if (!PatternSection.IsNone())
			{
				return PatternSection;
			}
		}
	}

	if (DamageSpec.DamageKind == EPragmataDamageKind::Hack)
	{
		const FName HackedSection = bHitFromBack
			? HalfHealthHitReactSections.HackedBackSectionName
			: HalfHealthHitReactSections.HackedFrontSectionName;
		if (!HackedSection.IsNone())
		{
			return HackedSection;
		}
	}

	return bHitFromBack
		? HalfHealthHitReactSections.NormalBackSectionName
		: HalfHealthHitReactSections.NormalFrontSectionName;
}

// hack damage는 방어 배율을 우회하고, 그 외 피해는 기본 0.2 또는 BP override를 적용
float AMonsterBase::ResolveUnhackedDamageTakenMultiplier(const FPragmataDamageSpec& DamageSpec) const
{
	if (bHacked || DamageSpec.DamageKind == EPragmataDamageKind::Hack)
	{
		return 1.f;
	}

	static constexpr float DefaultUnhackedDamageTakenMultiplier = 0.2f;
	return bOverrideUnhackedDamageTakenMultiplier
		? FMath::Max(0.f, UnhackedDamageTakenMultiplier)
		: DefaultUnhackedDamageTakenMultiplier;
}

// 총알 trace가 맞힌 Physics Asset body의 bone만 해킹 상태 약점으로 인정
float AMonsterBase::ResolveHackedWeakPointDamageMultiplier(const FPragmataDamageSpec& DamageSpec) const
{
	if (!bHacked
		|| DamageSpec.DamageKind != EPragmataDamageKind::Weapon
		|| !DamageSpec.bHasImpact)
	{
		return 1.f;
	}

	const FName HitBoneName = DamageSpec.ImpactHit.BoneName;
	if (HitBoneName.IsNone())
	{
		// 정확한 Physics Asset body가 확인되지 않은 hit는 위치가 가까워도 약점으로 추정하지 않음
		return 1.f;
	}

	const FMonsterHackedWeakPoint* WeakPoint = HackedWeakPoints.FindByPredicate(
		[HitBoneName](const FMonsterHackedWeakPoint& Candidate)
		{
			return !Candidate.BoneName.IsNone() && Candidate.BoneName == HitBoneName;
		});
	return WeakPoint ? FMath::Max(1.f, WeakPoint->DamageMultiplier) : 1.f;
}

// weapon damage는 class별 override가 있으면 그 값을, 없으면 monster 공통 배율을 적용
float AMonsterBase::ResolveWeaponDamageTakenMultiplier(const FPragmataDamageSpec& DamageSpec) const
{
	if (DamageSpec.DamageKind != EPragmataDamageKind::Weapon)
	{
		return 1.f;
	}

	const AWeaponBase* DamageWeapon = ResolveDamageWeaponActor(DamageSpec);
	if (DamageWeapon)
	{
		for (const FMonsterWeaponDamageTakenMultiplier& Override : PerWeaponDamageTakenMultipliers)
		{
			const UClass* WeaponClass = Override.WeaponClass.Get();
			if (WeaponClass && DamageWeapon->IsA(WeaponClass))
			{
				if (Override.bUseHackedStateDamageTakenMultipliers)
				{
					const float StateMultiplier = bHacked
						? Override.HackedDamageTakenMultiplier
						: Override.DefaultDamageTakenMultiplier;
					return FMath::Max(0.f, StateMultiplier);
				}

				return FMath::Max(0.f, Override.DamageTakenMultiplier);
			}
		}
	}

	return FMath::Max(0.f, WeaponDamageTakenMultiplier);
}

// 기존 공격 montage를 완전히 취소하고 반피 피격이 끝날 때까지 새 공격 선택을 잠금
bool AMonsterBase::PlayHalfHealthHitReact(FName SectionName)
{
	if (!HasAuthority())
	{
		return false;
	}
	if (!HalfHealthHitReactMontage || SectionName.IsNone())
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[AMonsterBase::PlayHalfHealthHitReact] Montage or section is not configured"));
		return false;
	}

	if (!HalfHealthHitReactMontage->IsValidSectionName(SectionName))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[AMonsterBase::PlayHalfHealthHitReact] Invalid section '%s' in '%s'"),
			*SectionName.ToString(),
			*GetNameSafe(HalfHealthHitReactMontage));
		return false;
	}

	UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (!AnimInstance)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AMonsterBase::PlayHalfHealthHitReact] AnimInstance is nullptr"));
		return false;
	}

	bHalfHealthHitReactPlaying = true;
	SetBlackboardCanAttack(false);
	if (bIsAttacking)
	{
		const FMonsterAttackPattern* Pattern = GetSelectedAttackPattern();
		if (Pattern && Pattern->AttackMontage && AnimInstance->Montage_IsPlaying(Pattern->AttackMontage))
		{
			AnimInstance->Montage_Stop(0.f, Pattern->AttackMontage);
		}

		// montage callback이 실행되지 않는 예외에도 공격 gameplay state를 반드시 종료
		if (bIsAttacking)
		{
			FinishAttackByAI();
		}
	}

	const float MontageDuration = AnimInstance->Montage_Play(
		HalfHealthHitReactMontage,
		1.f,
		EMontagePlayReturnType::MontageLength,
		0.f,
		true);
	if (MontageDuration <= 0.f)
	{
		bHalfHealthHitReactPlaying = false;
		RefreshAttackAvailability();
		UE_LOG(LogTemp, Warning, TEXT("[AMonsterBase::PlayHalfHealthHitReact] Failed to play montage"));
		return false;
	}

	AnimInstance->Montage_JumpToSection(SectionName, HalfHealthHitReactMontage);

	FOnMontageEnded MontageEndedDelegate;
	MontageEndedDelegate.BindUObject(this, &AMonsterBase::HandleHalfHealthHitReactMontageEnded);
	AnimInstance->Montage_SetEndDelegate(MontageEndedDelegate, HalfHealthHitReactMontage);
	MulticastPlayHalfHealthHitReactMontage(HalfHealthHitReactMontage, SectionName);
	return true;
}

// 피격 montage가 끝난 뒤 cooldown 상태를 다시 계산해 다음 공격 선택을 허용
void AMonsterBase::HandleHalfHealthHitReactMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (!HasAuthority() || Montage != HalfHealthHitReactMontage)
	{
		return;
	}

	bHalfHealthHitReactPlaying = false;
	if (!IsDead())
	{
		RefreshAttackAvailability();
	}
}

// 모든 damage 종류가 같은 기준으로 사망 animation 방향을 선택
void AMonsterBase::ResolveDamageDirection(FPragmataDamageSpec& DamageSpec) const
{
	AActor* SourceActor = IsValid(DamageSpec.SourceActor.Get())
		? DamageSpec.SourceActor.Get()
		: DamageSpec.DamageCauser.Get();
	if (!IsValid(SourceActor))
	{
		return;
	}

	FVector DirectionToSource = SourceActor->GetActorLocation() - GetActorLocation();
	DirectionToSource.Z = 0.f;
	if (!DirectionToSource.Normalize())
	{
		return;
	}

	const float ForwardDot = FVector::DotProduct(GetActorForwardVector(), DirectionToSource);
	DamageSpec.HitDirection = ForwardDot >= 0.f
		? EPragmataHitDirection::Front
		: EPragmataHitDirection::Back;
	DamageSpec.ImpactDirection = -DirectionToSource;
}

void AMonsterBase::ClearTargetIfSame(AActor* Actor)
{
	if (TargetActor == Actor)
	{
		TargetActor = nullptr;
		ApplyTargetHPBarVisibility();
	}
}

// TargetActor 세팅
bool AMonsterBase::TrySetTargetIfPerceived(AActor* Actor)
{
	//decoy 예외처리
	if (IsTaunted) return false;
	
	if (!Actor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DamageAggro][TrySetTargetIfPerceived] Blocked: Actor is null Monster=%s"), *GetNameSafe(this));
		return false;
	}

	// if (TargetActor)
	// {
	// 	UE_LOG(
	// 		LogTemp,
	// 		Warning,
	// 		TEXT("[DamageAggro][TrySetTargetIfPerceived] Blocked: Target already set Monster=%s CurrentTarget=%s NewActor=%s"),
	// 		*GetNameSafe(this),
	// 		*GetNameSafe(TargetActor.Get()),
	// 		*GetNameSafe(Actor));
	// 	return false;
	// }
	if (!AIPerceptionComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DamageAggro][TrySetTargetIfPerceived] Blocked: PerceptionComponent missing Monster=%s Actor=%s"), *GetNameSafe(this), *GetNameSafe(Actor));
		return false;
	}

	FActorPerceptionBlueprintInfo PerceptionInfo;
	AIPerceptionComponent->GetActorsPerception(Actor, PerceptionInfo);

	bool bCanCurrentlySenseActor = false;
	for (const FAIStimulus& SensedStimulus : PerceptionInfo.LastSensedStimuli)
	{
		if (SensedStimulus.WasSuccessfullySensed())
		{
			bCanCurrentlySenseActor = true;
			break;
		}
	}

	// if (!bCanCurrentlySenseActor)
	// {
	// 	UE_LOG(
	// 		LogTemp,
	// 		Warning,
	// 		TEXT("[DamageAggro][TrySetTargetIfPerceived] Blocked: No active successful stimulus Monster=%s Actor=%s Stimuli=%d"),
	// 		*GetNameSafe(this),
	// 		*GetNameSafe(Actor),
	// 		PerceptionInfo.LastSensedStimuli.Num());
	// 	return false;
	// }

	AMonsterAIController* MonsterController = Cast<AMonsterAIController>(GetController());
	if (!MonsterController)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DamageAggro][TrySetTargetIfPerceived] Blocked: MonsterController missing Monster=%s Actor=%s"), *GetNameSafe(this), *GetNameSafe(Actor));
		return false;
	}

	if (!MonsterController->SetTarget(Actor))
	{
		UE_LOG(LogTemp, Warning, TEXT("[DamageAggro][TrySetTargetIfPerceived] Blocked: SetTarget failed Monster=%s Actor=%s"), *GetNameSafe(this), *GetNameSafe(Actor));
		return false;
	}

	UE_LOG(LogTemp, Warning, TEXT("[DamageAggro][TrySetTargetIfPerceived] Success Monster=%s Target=%s"), *GetNameSafe(this), *GetNameSafe(Actor));
	TargetActor = Actor;
	ApplyTargetHPBarVisibility();
	return true;
}

// Territory 재진입 시 기존 perception cache가 즉시 target 복구로 쓰이지 않게 actor 기록만 제거
void AMonsterBase::ForgetPerceivedActor(AActor* Actor)
{
	if (AIPerceptionComponent && Actor)
	{
		AIPerceptionComponent->ForgetActor(Actor);
	}
}

// Territory가 spawn/sequence 조건으로 이미 검증한 player를 perception 없이 target으로 확정
bool AMonsterBase::SetTerritoryTarget(AActor* Actor)
{
	if (!HasAuthority() || !Actor || (TargetActor && TargetActor != Actor))
	{
		return false;
	}

	AMonsterAIController* MonsterController = Cast<AMonsterAIController>(GetController());
	if (!MonsterController)
	{
		return false;
	}

	if (!MonsterController->SetTarget(Actor))
	{
		return false;
	}

	TargetActor = Actor;
	ApplyTargetHPBarVisibility();
	return true;
}

bool AMonsterBase::ReportWeaponDamageSense(const FPragmataDamageSpec& DamageSpec)
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[DamageAggro][ReportWeaponDamageSense] Blocked: no authority Monster=%s"), *GetNameSafe(this));
		return false;
	}
	if (TargetActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DamageAggro][ReportWeaponDamageSense] Blocked: Target already set Monster=%s Target=%s"), *GetNameSafe(this), *GetNameSafe(TargetActor.Get()));
		return false;
	}
	if (DamageSpec.DamageKind != EPragmataDamageKind::Weapon)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DamageAggro][ReportWeaponDamageSense] Blocked: DamageKind is not Weapon Monster=%s Kind=%d"), *GetNameSafe(this), static_cast<int32>(DamageSpec.DamageKind));
		return false;
	}

	AActor* InstigatorActor = ResolveWeaponDamageInstigatorActor(DamageSpec);
	const APawn* InstigatorPawn = Cast<APawn>(InstigatorActor);
	if (!IsValid(InstigatorActor))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[DamageAggro][ReportWeaponDamageSense] Blocked: InstigatorActor invalid Monster=%s Source=%s Causer=%s InstigatorController=%s"),
			*GetNameSafe(this),
			*GetNameSafe(DamageSpec.SourceActor.Get()),
			*GetNameSafe(DamageSpec.DamageCauser.Get()),
			*GetNameSafe(DamageSpec.InstigatorController.Get()));
		return false;
	}
	if (InstigatorActor == this)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DamageAggro][ReportWeaponDamageSense] Blocked: Instigator is self Monster=%s"), *GetNameSafe(this));
		return false;
	}
	if (!InstigatorPawn)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DamageAggro][ReportWeaponDamageSense] Blocked: Instigator is not Pawn Monster=%s Instigator=%s Class=%s"), *GetNameSafe(this), *GetNameSafe(InstigatorActor), *GetNameSafe(InstigatorActor->GetClass()));
		return false;
	}
	if (!InstigatorPawn->IsPlayerControlled())
	{
		UE_LOG(LogTemp, Warning, TEXT("[DamageAggro][ReportWeaponDamageSense] Blocked: Instigator pawn is not player controlled Monster=%s Instigator=%s Controller=%s"), *GetNameSafe(this), *GetNameSafe(InstigatorActor), *GetNameSafe(InstigatorPawn->GetController()));
		return false;
	}

	AMonsterAIController* MonsterController = Cast<AMonsterAIController>(GetController());
	if (!MonsterController)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DamageAggro][ReportWeaponDamageSense] Blocked: MonsterController missing Monster=%s Instigator=%s"), *GetNameSafe(this), *GetNameSafe(InstigatorActor));
		return false;
	}
	if (!MonsterController->IsActorInsideAssignedTerritory(InstigatorActor))
	{
		UE_LOG(LogTemp, Warning, TEXT("[DamageAggro][ReportWeaponDamageSense] Blocked: Instigator outside assigned territory or territory missing Monster=%s Instigator=%s Controller=%s"), *GetNameSafe(this), *GetNameSafe(InstigatorActor), *GetNameSafe(MonsterController));
		return false;
	}

	const FVector HitLocation = ResolveDamageSenseHitLocation(this, DamageSpec);
	if (!AIPerceptionComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DamageAggro][ReportWeaponDamageSense] Blocked: PerceptionComponent missing Monster=%s Instigator=%s"), *GetNameSafe(this), *GetNameSafe(InstigatorActor));
		return false;
	}

	const UAISense_Damage* DamageSense = GetDefault<UAISense_Damage>();
	if (!DamageSense)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DamageAggro][ReportWeaponDamageSense] Blocked: DamageSense CDO missing Monster=%s Instigator=%s"), *GetNameSafe(this), *GetNameSafe(InstigatorActor));
		return false;
	}

	UAIPerceptionSystem* PerceptionSystem = UAIPerceptionSystem::GetCurrent(this);
	if (!PerceptionSystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DamageAggro][ReportWeaponDamageSense] Blocked: PerceptionSystem missing Monster=%s Instigator=%s"), *GetNameSafe(this), *GetNameSafe(InstigatorActor));
		return false;
	}

	FAISenseID DamageSenseID = UAISense::GetSenseID<UAISense_Damage>();
	if (!PerceptionSystem->IsSenseInstantiated(DamageSenseID))
	{
		DamageSenseID = PerceptionSystem->RegisterSenseClass(UAISense_Damage::StaticClass());
	}
	if (!DamageSenseID.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[DamageAggro][ReportWeaponDamageSense] Blocked: DamageSenseID invalid Monster=%s Instigator=%s"), *GetNameSafe(this), *GetNameSafe(InstigatorActor));
		return false;
	}

	if (!DamageConfig)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DamageAggro][ReportWeaponDamageSense] Blocked: DamageConfig missing Monster=%s Instigator=%s"), *GetNameSafe(this), *GetNameSafe(InstigatorActor));
		return false;
	}
	AIPerceptionComponent->ConfigureSense(*DamageConfig);

	// damage를 받은 pawn owner 경로가 누락될 수 있어 실제 listener component에 직접 stimulus 등록
	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[DamageAggro][ReportWeaponDamageSense] Register DamageSense stimulus Monster=%s Instigator=%s Amount=%.2f EventLocation=%s HitLocation=%s"),
		*GetNameSafe(this),
		*GetNameSafe(InstigatorActor),
		DamageSpec.DamageAmount,
		*InstigatorActor->GetActorLocation().ToCompactString(),
		*HitLocation.ToCompactString());
	FAIStimulus DamageStimulus(
		*DamageSense,
		DamageSpec.DamageAmount,
		InstigatorActor->GetActorLocation(),
		HitLocation,
		FAIStimulus::SensingSucceeded
	);
	DamageStimulus.Type = DamageSenseID;
	AIPerceptionComponent->RegisterStimulus(
		InstigatorActor,
		DamageStimulus
	);
	AIPerceptionComponent->ProcessStimuli();
	return true;
}

void AMonsterBase::CalculateAngle()
{
	const FVector Velocity = GetCharacterMovement()->Velocity;
	if (Velocity.SizeSquared2D() > 4.f)
	{
		const FRotator MovementRotation = FRotationMatrix::MakeFromX(Velocity).Rotator();
		const FRotator DeltaRot = (MovementRotation - GetActorRotation()).GetNormalized();
		fAngle = -DeltaRot.Yaw;

		// if (GEngine)
		// {
		// 	// 2. 소수점 둘째 자리까지 깔끔하게 포맷팅합니다.
		// 	const FString SpeedMessage = FString::Printf(TEXT("Monster's Movement Angle: %.2f"), fAngle);
		// 	GEngine->ClearOnScreenDebugMessages();
		// 	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, SpeedMessage);
		// }
	}
	else
	{
		fAngle = 0.f;
	}
}

// 하강 appearance가 땅에 가까워지면 loop section에서 착지 section으로 넘김
void AMonsterBase::UpdateFallingSpawnAppearance()
{
	if (!HasAuthority()
		|| !bSpawnAppearancePlaying
		|| !bSpawnAppearanceUsesFalling
		|| bSpawnAppearanceEndRequested)
	{
		return;
	}

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (Movement && Movement->IsMovingOnGround())
	{
		RequestFallingSpawnAppearanceEnd();
		return;
	}

	if (SpawnAppearanceFallingEndTransitionDistance <= 0.f)
	{
		return;
	}

	UCapsuleComponent* Capsule = GetCapsuleComponent();
	UWorld* World = GetWorld();
	if (!Capsule || !World)
	{
		return;
	}

	const FVector Start = Capsule->GetComponentLocation();
	const FVector End = Start - FVector::UpVector * SpawnAppearanceFallingEndTransitionDistance;
	const FCollisionShape CapsuleShape = FCollisionShape::MakeCapsule(
		Capsule->GetScaledCapsuleRadius(),
		Capsule->GetScaledCapsuleHalfHeight());
	FCollisionQueryParams QueryParams(TEXT("FallingSpawnAppearanceGround"), false, this);
	FHitResult Hit;
	if (World->SweepSingleByChannel(
		Hit,
		Start,
		End,
		FQuat::Identity,
		Capsule->GetCollisionObjectType(),
		CapsuleShape,
		QueryParams)
		&& Hit.bBlockingHit)
	{
		RequestFallingSpawnAppearanceEnd();
	}
}

// server가 착지 전환을 확정하고 모든 peer의 appearance montage section을 맞춤
void AMonsterBase::RequestFallingSpawnAppearanceEnd()
{
	if (!HasAuthority()
		|| !bSpawnAppearancePlaying
		|| !bSpawnAppearanceUsesFalling
		|| bSpawnAppearanceEndRequested
		|| !ActiveSpawnAppearanceMontage)
	{
		return;
	}

	if (SpawnAppearanceFallingEndSectionName.IsNone()
		|| !ActiveSpawnAppearanceMontage->IsValidSectionName(SpawnAppearanceFallingEndSectionName))
	{
		bSpawnAppearanceEndRequested = true;
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[AMonsterBase::RequestFallingSpawnAppearanceEnd] Invalid falling end section Monster=%s Montage=%s Section=%s"),
			*GetNameSafe(this),
			*GetNameSafe(ActiveSpawnAppearanceMontage.Get()),
			*SpawnAppearanceFallingEndSectionName.ToString());
		if (UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
		{
			AnimInstance->Montage_Stop(0.1f, ActiveSpawnAppearanceMontage);
		}
		return;
	}

	bSpawnAppearanceEndRequested = true;
	JumpSpawnAppearanceToSectionLocally(ActiveSpawnAppearanceMontage, SpawnAppearanceFallingEndSectionName);
	MulticastJumpSpawnAppearanceToSection(ActiveSpawnAppearanceMontage, SpawnAppearanceFallingEndSectionName);
}

// montage loop를 끊는 section jump를 현재 peer의 AnimInstance에만 적용
void AMonsterBase::JumpSpawnAppearanceToSectionLocally(UAnimMontage* Montage, FName SectionName)
{
	if (!Montage || SectionName.IsNone())
	{
		return;
	}

	UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (AnimInstance && AnimInstance->Montage_IsPlaying(Montage) && Montage->IsValidSectionName(SectionName))
	{
		AnimInstance->Montage_JumpToSection(SectionName, Montage);
	}
}

// 하강 appearance의 root motion 이동을 무시해 실제 낙하는 CharacterMovement가 담당
void AMonsterBase::ApplySpawnAppearanceRootMotionMode()
{
	UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (!AnimInstance || bSpawnAppearanceRootMotionModeSaved)
	{
		return;
	}

	SavedSpawnAppearanceRootMotionMode = AnimInstance->RootMotionMode;
	bSpawnAppearanceRootMotionModeSaved = true;
	AnimInstance->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
}

// appearance 전 AnimBP root motion mode를 복구해 이후 montage 정책을 보존
void AMonsterBase::RestoreSpawnAppearanceRootMotionMode()
{
	UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (!AnimInstance || !bSpawnAppearanceRootMotionModeSaved)
	{
		return;
	}

	AnimInstance->SetRootMotionMode(SavedSpawnAppearanceRootMotionMode);
	bSpawnAppearanceRootMotionModeSaved = false;
}
