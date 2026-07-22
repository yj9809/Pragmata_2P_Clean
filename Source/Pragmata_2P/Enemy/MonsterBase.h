#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimEnums.h"
#include "Data/PragmataTypes.h"
#include "GameFramework/Character.h"
#include "Interfaces/PragmataDamageSource.h"
#include "Interfaces/PragmataDamageable.h"
#include "Interfaces/PragmataHackable.h"
#include "Interfaces/PragmataScannable.h"
#include "Perception/AIPerceptionTypes.h"
#include "MonsterBase.generated.h"

class UAISenseConfig_Sight;
class UAISenseConfig_Damage;
class UAISense_Sight;
class UAIPerceptionComponent;
class AMonsterAIController;
class UPragmataFactionComponent;
class UPragmataHealthComponent;
class UEnemyHackingPuzzleComponent;
class UBehaviorTree;
class UAnimMontage;
class UAnimSequenceBase;
class AWeaponBase;
class AMonsterBase;

// 공격 종료 델리게이트 선언.
DECLARE_DELEGATE(FMonsterAttackFinished);
DECLARE_MULTICAST_DELEGATE_OneParam(FMonsterSpawnAppearanceFinished, AMonsterBase*);

// Monster Combat Data Struct
USTRUCT(BlueprintType)
struct PRAGMATA_2P_API FMonsterCombatData
{
	GENERATED_BODY()

public:
	// 기본 melee 공격 damage
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat")
	float AttackDamage = 10.f;

	// 공격 사이 cooldown
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta=(ClampMin="0.0", UIMin="0.0"))
	float CommonAttackCooldown = 5.f;

	// 공통 공격 cooldown을 pattern 재선택마다 범위 안에서 새로 뽑을지 여부
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat")
	bool bUseRandomCommonAttackCooldown = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta=(EditCondition="bUseRandomCommonAttackCooldown", EditConditionHides, ClampMin="0.0", UIMin="0.0"))
	float CommonAttackCooldownMin = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta=(EditCondition="bUseRandomCommonAttackCooldown", EditConditionHides, ClampMin="0.0", UIMin="0.0"))
	float CommonAttackCooldownMax = 5.f;
};

// monster BP에서 특정 weapon class만 별도 피해 배율로 보정
USTRUCT(BlueprintType)
struct PRAGMATA_2P_API FMonsterWeaponDamageTakenMultiplier
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Damage")
	TSubclassOf<AWeaponBase> WeaponClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Damage", meta=(ClampMin="0.0", UIMin="0.0"))
	float DamageTakenMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Damage")
	bool bUseHackedStateDamageTakenMultipliers = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Damage", meta=(ClampMin="0.0", UIMin="0.0", EditCondition="bUseHackedStateDamageTakenMultipliers", EditConditionHides))
	float DefaultDamageTakenMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Damage", meta=(ClampMin="0.0", UIMin="0.0", EditCondition="bUseHackedStateDamageTakenMultipliers", EditConditionHides))
	float HackedDamageTakenMultiplier = 1.f;
};

// 공격 외 각 상태에서 단일 또는 Front/Back으로 선택할 사망 animation
USTRUCT(BlueprintType)
struct PRAGMATA_2P_API FMonsterDeathAnimationSet
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Death")
	bool bUseDefaultDirectionVariants = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Death", meta=(EditCondition="!bUseDefaultDirectionVariants", EditConditionHides))
	TObjectPtr<UAnimSequenceBase> DefaultAnimation = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Death", meta=(EditCondition="!bUseDefaultDirectionVariants", EditConditionHides, ClampMin="-1.0", Units="s", ToolTip="-1이면 Monster의 Death Ragdoll Delay 사용"))
	float DefaultRagdollDelay = -1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Death", meta=(EditCondition="bUseDefaultDirectionVariants", EditConditionHides))
	TObjectPtr<UAnimSequenceBase> DefaultFrontAnimation = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Death", meta=(EditCondition="bUseDefaultDirectionVariants", EditConditionHides, ClampMin="-1.0", Units="s", ToolTip="-1이면 Monster의 Death Ragdoll Delay 사용"))
	float DefaultFrontRagdollDelay = -1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Death", meta=(EditCondition="bUseDefaultDirectionVariants", EditConditionHides))
	TObjectPtr<UAnimSequenceBase> DefaultBackAnimation = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Death", meta=(EditCondition="bUseDefaultDirectionVariants", EditConditionHides, ClampMin="-1.0", Units="s", ToolTip="-1이면 Monster의 Death Ragdoll Delay 사용"))
	float DefaultBackRagdollDelay = -1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Death")
	bool bUseLocomotionDirectionVariants = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Death", meta=(EditCondition="!bUseLocomotionDirectionVariants", EditConditionHides))
	TObjectPtr<UAnimSequenceBase> LocomotionAnimation = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Death", meta=(EditCondition="!bUseLocomotionDirectionVariants", EditConditionHides, ClampMin="-1.0", Units="s", ToolTip="-1이면 Monster의 Death Ragdoll Delay 사용"))
	float LocomotionRagdollDelay = -1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Death", meta=(EditCondition="bUseLocomotionDirectionVariants", EditConditionHides))
	TObjectPtr<UAnimSequenceBase> LocomotionFrontAnimation = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Death", meta=(EditCondition="bUseLocomotionDirectionVariants", EditConditionHides, ClampMin="-1.0", Units="s", ToolTip="-1이면 Monster의 Death Ragdoll Delay 사용"))
	float LocomotionFrontRagdollDelay = -1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Death", meta=(EditCondition="bUseLocomotionDirectionVariants", EditConditionHides))
	TObjectPtr<UAnimSequenceBase> LocomotionBackAnimation = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Death", meta=(EditCondition="bUseLocomotionDirectionVariants", EditConditionHides, ClampMin="-1.0", Units="s", ToolTip="-1이면 Monster의 Death Ragdoll Delay 사용"))
	float LocomotionBackRagdollDelay = -1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Death")
	bool bUseHackDirectionVariants = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Death", meta=(EditCondition="!bUseHackDirectionVariants", EditConditionHides))
	TObjectPtr<UAnimSequenceBase> HackAnimation = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Death", meta=(EditCondition="!bUseHackDirectionVariants", EditConditionHides, ClampMin="-1.0", Units="s", ToolTip="-1이면 Monster의 Death Ragdoll Delay 사용"))
	float HackRagdollDelay = -1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Death", meta=(EditCondition="bUseHackDirectionVariants", EditConditionHides))
	TObjectPtr<UAnimSequenceBase> HackFrontAnimation = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Death", meta=(EditCondition="bUseHackDirectionVariants", EditConditionHides, ClampMin="-1.0", Units="s", ToolTip="-1이면 Monster의 Death Ragdoll Delay 사용"))
	float HackFrontRagdollDelay = -1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Death", meta=(EditCondition="bUseHackDirectionVariants", EditConditionHides))
	TObjectPtr<UAnimSequenceBase> HackBackAnimation = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Death", meta=(EditCondition="bUseHackDirectionVariants", EditConditionHides, ClampMin="-1.0", Units="s", ToolTip="-1이면 Monster의 Death Ragdoll Delay 사용"))
	float HackBackRagdollDelay = -1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Death")
	bool bUseCriticalDirectionVariants = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Death", meta=(EditCondition="!bUseCriticalDirectionVariants", EditConditionHides))
	TObjectPtr<UAnimSequenceBase> CriticalAnimation = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Death", meta=(EditCondition="!bUseCriticalDirectionVariants", EditConditionHides, ClampMin="-1.0", Units="s", ToolTip="-1이면 Monster의 Death Ragdoll Delay 사용"))
	float CriticalRagdollDelay = -1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Death", meta=(EditCondition="bUseCriticalDirectionVariants", EditConditionHides))
	TObjectPtr<UAnimSequenceBase> CriticalFrontAnimation = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Death", meta=(EditCondition="bUseCriticalDirectionVariants", EditConditionHides, ClampMin="-1.0", Units="s", ToolTip="-1이면 Monster의 Death Ragdoll Delay 사용"))
	float CriticalFrontRagdollDelay = -1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Death", meta=(EditCondition="bUseCriticalDirectionVariants", EditConditionHides))
	TObjectPtr<UAnimSequenceBase> CriticalBackAnimation = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Death", meta=(EditCondition="bUseCriticalDirectionVariants", EditConditionHides, ClampMin="-1.0", Units="s", ToolTip="-1이면 Monster의 Death Ragdoll Delay 사용"))
	float CriticalBackRagdollDelay = -1.f;
};

// 공격 pattern 외 반피 피격을 일반 또는 해킹 상태 section으로 매핑
USTRUCT(BlueprintType)
struct PRAGMATA_2P_API FMonsterHalfHealthHitReactSectionSet
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hit React")
	FName NormalFrontSectionName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hit React")
	FName NormalBackSectionName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hit React")
	FName HackedFrontSectionName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hit React")
	FName HackedBackSectionName = NAME_None;
};

// Physics Asset body가 연결된 bone별 해킹 약점 대미지 설정
USTRUCT(BlueprintType)
struct PRAGMATA_2P_API FMonsterHackedWeakPoint
{
	GENERATED_BODY()

public:
	// Physics Asset Body의 Bone Name 또는 총알 HitResult에 찍히는 BoneName
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weak Point")
	FName BoneName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weak Point", meta=(ClampMin="1.0", UIMin="1.0"))
	float DamageMultiplier = 2.f;
};

USTRUCT(BlueprintType)
struct PRAGMATA_2P_API FMonsterAttackPattern
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attack")
	FName PatternName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attack")
	TObjectPtr<UAnimMontage> AttackMontage = nullptr;

	// 체크한 pattern만 공통 AttackDamage 대신 개별 damage 사용
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attack|Damage")
	bool bUsePatternAttackDamage = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attack|Damage", meta=(EditCondition="bUsePatternAttackDamage", EditConditionHides, ClampMin="0.0", UIMin="0.0"))
	float PatternAttackDamage = 10.f;

	// 이 공격 도중 사망했을 때 재생할 animation
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attack|Death")
	TObjectPtr<UAnimSequenceBase> DeathAnimation = nullptr;

	// -1이면 Monster의 공통 DeathRagdollDelay 사용
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attack|Death", meta=(ClampMin="-1.0", Units="s"))
	float DeathRagdollDelay = -1.f;

	// 이 공격 도중 반피를 처음 통과하고 앞에서 맞았을 때 재생할 section
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attack|Hit React")
	FName HalfHealthHitReactFrontSectionName = NAME_None;

	// 이 공격 도중 반피를 처음 통과하고 뒤에서 맞았을 때 재생할 section
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attack|Hit React")
	FName HalfHealthHitReactBackSectionName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attack", meta=(ClampMin="0.0"))
	float SelectionWeight = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attack")
	bool bAllowConsecutiveUse = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attack", meta=(ClampMin="0.0"))
	float AttackCooldown = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attack", meta=(ClampMin="0.0"))
	float MinimumAttackRange = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attack", meta=(ClampMin="0.0"))
	float AttackRange = 200.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attack", meta=(ClampMin="0.0"))
	float AttackRadius = 50.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack", meta = (ClampMin = "0.0"))
	float TurnSpeed = 10.f;

	// target이 몬스터 뒤쪽 cone 안에 있을 때만 이 pattern을 선택 후보로 사용
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attack|Condition")
	bool bRequireTargetBehind = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attack|Condition", meta=(EditCondition="bRequireTargetBehind", EditConditionHides, ClampMin="0.0", ClampMax="360.0", UIMin="0.0", UIMax="180.0", Units="deg"))
	float TargetBehindAngleDegrees = 120.f;

	// target 근접 시 loop section에서 end section으로 전환
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attack|Montage Loop")
	bool bUseProximityControlledMontageLoop = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attack|Montage Loop", meta=(EditCondition="bUseProximityControlledMontageLoop", EditConditionHides))
	FName EndSectionName = NAME_None;

	// loop에서 end로 즉시 전환할 target 중심 거리
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attack|Montage Loop", meta=(EditCondition="bUseProximityControlledMontageLoop", EditConditionHides, ClampMin="0.0"))
	float EndTransitionDistance = 150.f;

	// target 유실이나 추격 실패 시 loop를 끝내는 최대 시간
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attack|Montage Loop", meta=(EditCondition="bUseProximityControlledMontageLoop", EditConditionHides, ClampMin="0.0"))
	float MaxLoopDuration = 5.f;

	// 공격 montage의 root motion을 공중 이동으로 처리할 pattern만 Flying 이동 모드 사용
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attack|Movement")
	bool bUseFlyingMovementDuringAttackMontage = false;

	// Flying loop 동안 CharacterMovement가 유지할 전진 속도. 최대 이동 거리는 속도와 MaxLoopDuration의 곱으로 결정
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attack|Movement", meta=(EditCondition="bUseFlyingMovementDuringAttackMontage && bUseProximityControlledMontageLoop", EditConditionHides, ClampMin="0.0", UIMin="0.0", Units="cm/s"))
	float FlyingLoopMoveSpeed = 1200.f;
};

UCLASS(Abstract)
class PRAGMATA_2P_API AMonsterBase : public ACharacter, public IPragmataDamageSource, public IPragmataDamageable, public IPragmataHackable, public IPragmataScannable
{
	GENERATED_BODY()

public:
	// 몬스터 기본값 생성
	AMonsterBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// 시작 시 진영 상태 설정
	virtual void BeginPlay() override;

	// 사라지는 monster의 aggro combat 등록 해제
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// target actor 기준 회전 보조값 갱신
  // 이동 방향과 공격 montage loop 종료 조건 갱신
	virtual void Tick(float DeltaSeconds) override;

	// AIController possession 이후 perception binding
	virtual void PossessedBy(AController* NewController) override;

	// 하강 appearance가 실제 바닥에 닿으면 loop section을 종료 section으로 전환
	virtual void Landed(const FHitResult& Hit) override;

	// monster replicated state 등록
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 현재 공격 pattern override 또는 monster combat data의 melee damage 제공
	virtual float GetCurrentAttackDamage_Implementation() const override;

	// 공통 피해 수신 가능 여부 확인
	virtual bool CanReceiveDamage_Implementation(const FPragmataDamageSpec& DamageSpec) const override;

	// 공통 피해 수신 처리
	virtual bool ReceiveDamage_Implementation(const FPragmataDamageSpec& DamageSpec) override;

	// enemy hack puzzle 시작 가능 여부 확인
	virtual bool CanBeHacked_Implementation(AActor* HackInstigator) const override;

	// hack 성공 처리와 hack damage 및 유지 시간 전달
	virtual bool ApplySuccessfulHack_Implementation(AActor* HackInstigator, float HackDamage, float ResolvedHackDurationSeconds) override;

	// AnimBP가 hack 성공 상태를 읽기 위한 조회
	UFUNCTION(BlueprintPure, Category="Pragmata|Monster|Hack")
	bool IsHacked() const { return bHacked; }

	// 완료 시점 파란 노드 수로 산출된 hacked 지속시간(초) — 게이지 분모. 서버가 채우고 복제.
	UFUNCTION(BlueprintPure, Category="Pragmata|Monster|Hack")
	float GetAppliedHackDuration() const { return AppliedHackDuration; }

	// 현재 hacked 상태의 남은 지속시간(초) — 복제된 만료 서버 시각 기준으로 클라에서도 정확.
	// UI가 닫혔다 열려도 이 값이 진실원이라 게이지가 항상 실제 잔량을 표시.
	UFUNCTION(BlueprintPure, Category="Pragmata|Monster|Hack")
	float GetHackedRemainingSeconds() const;

	// OverDrive처럼 puzzle damage/reward 없이 hacked 상태와 duration만 강제할 때 사용
	UFUNCTION(BlueprintCallable, Category="Pragmata|Monster|Hack")
	bool ApplyTimedHackState(float DurationSeconds);

	// AnimBP/Blueprint가 오염된 visual 상태를 읽기 위한 조회
	UFUNCTION(BlueprintPure, Category="Pragmata|Monster|Taint")
	bool IsTainted() const { return bIsTainted; }

	bool CleanseTaint(AActor* CleanseInstigator);

	// AnimBP가 StasisNet 정지 상태를 읽기 위한 조회
	UFUNCTION(BlueprintPure, Category="Pragmata|Monster|Stasis")
	bool IsStasisStunned() const { return bStasisStunned; }

	// scan UI에 보여줄 monster 정보 생성
	virtual FPragmataScanResult GetPragmataScanResult_Implementation(AActor* ScanRequester) const override;

	// Territory Actor
	// 같은 target이 territory에서 사라졌을 때 target 해제
	virtual void ClearTargetIfSame(AActor* Actor);

	// perception으로 확인된 actor를 AI target으로 설정
	virtual bool TrySetTargetIfPerceived(AActor* Actor);
	void ForgetPerceivedActor(AActor* Actor);

	// Territory spawn처럼 perception stimulus 없이 확정된 player를 AI target으로 설정
	virtual bool SetTerritoryTarget(AActor* Actor);
public:
	// 체력 컴포넌트 조회
	UFUNCTION(BlueprintPure, Category = "Pragmata|Character")
	UPragmataHealthComponent* GetHealthComponent() const { return HealthComponent; }

	// 사망 여부 조회
	UFUNCTION(BlueprintPure, Category="Pragmata|Monster|Death")
	bool IsDead() const;

	// AnimBP가 Death 상태에서 재생할 최종 animation
	UFUNCTION(BlueprintPure, Category="Pragmata|Monster|Death")
	UAnimSequenceBase* GetDeathAnimation() const { return CurrentDeathAnimation; }

	// 진영 컴포넌트 조회
	UFUNCTION(BlueprintPure, Category = "Pragmata|Character")
	UPragmataFactionComponent* GetFactionComponent() const { return FactionComponent; }

	// for Behavior Tree
	// Get BT Asset
	FORCEINLINE UBehaviorTree* GetBTAsset() const { return BTAsset; }

	FORCEINLINE bool GetIsAttacking() const { return bIsAttacking; }
	FORCEINLINE float GetAngle() const { return fAngle; }
	FORCEINLINE bool CanTrackTargetDuringAttack() const { return bTrackTargetDuringAttack; }
	bool ShouldKeepCurrentFacingForSelectedAttackPattern() const;
	FORCEINLINE FName GetSelectedAttackPatternName() const
	{
		const FMonsterAttackPattern* Pattern = GetSelectedAttackPattern();
		return Pattern ? Pattern->PatternName : NAME_None;
	}

	// Getter 플레이어 액터
	UFUNCTION(BlueprintPure)
	AActor* GetTargetActor() const { return TargetActor; }

	float GetAttackRange() const;
	float GetAttackRadius() const;
	float GetMinimumAttackRange() const;
	float GetAIAttackDetectRange() const;
	float GetTurnSpeed() const;
	bool IsSelectedAttackPatternInRange(const AActor* Target) const;

	bool HasSelectedAttackPattern() const;
	bool EnsureSelectedAttackPattern();
	void ClearSelectedAttackPattern();

	// for BTTask_ChangeMoveSpeed
	void SetAIMoveSpeedMode(EMonsterMoveSpeedMode MoveSpeedMode);
	// 캐릭터에서 델리게이트를 넘길 때 사용할 함수.
	void SetAIAttackDelegate(const FMonsterAttackFinished& InOnAttackFinished);
	void ClearAIAttackDelegate();
	bool AttackByAI();
	void StopAttackRotationTracking();

	UFUNCTION(BlueprintCallable, Category="Pragmata|Monster|Attack")
	void FinishAttackByAI();

	// Territory runtime spawn 직후 monster별 appearance montage 재생
	UFUNCTION(BlueprintCallable, Category="Pragmata|Monster|Appearance")
	void PlaySpawnAppearance();

	bool HasSpawnAppearanceMontage() const { return SpawnAppearanceMontage != nullptr; }
	bool HasSpawnAppearanceMontage(UAnimMontage* AppearanceMontageOverride) const;
	bool TryPlaySpawnAppearance(
		UAnimMontage* AppearanceMontageOverride = nullptr,
		bool bUseFallingAppearance = false,
		FName FallingEndSectionName = NAME_None,
		float FallingEndTransitionDistance = 0.f);
	float GetSpawnAppearanceMontagePlayLength(UAnimMontage* AppearanceMontageOverride = nullptr) const;
	bool ForceFinishSpawnAppearance();
	void SetAutoRunAIOnPossess(bool bNewAutoRunAIOnPossess);
	bool ShouldAutoRunAIOnPossess() const { return bAutoRunAIOnPossess; }

	FMonsterSpawnAppearanceFinished OnSpawnAppearanceFinished;

	bool ApplyStasisStun(AActor* StunSource, float Duration);
	void RemoveStasisStun(AActor* StunSource);

	// for cooldown
	bool CanAttack() const;
	void StartCommonAttackCooldown();

protected:
	// server montage 재생을 client에 전달
	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlayAttackMontage(UAnimMontage* Montage);

	// server의 즉시 end section 전환을 client montage에도 적용
	UFUNCTION(NetMulticast, Reliable)
	void MulticastJumpAttackMontageToEnd(UAnimMontage* Montage, FName EndSectionName);

	// Flying loop의 code-driven 이동이 montage root motion에 막히지 않도록 모든 peer의 적용 모드를 맞춤
	UFUNCTION(NetMulticast, Reliable)
	void MulticastSetAttackLoopRootMotionIgnored(bool bIgnoreRootMotion);

	// 하강 appearance의 착지 section 전환을 모든 peer의 montage에 적용
	UFUNCTION(NetMulticast, Reliable)
	void MulticastJumpSpawnAppearanceToSection(UAnimMontage* Montage, FName SectionName);

	// server가 확정한 반피 피격 section을 모든 peer에서 같은 montage로 재생
	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlayHalfHealthHitReactMontage(UAnimMontage* Montage, FName SectionName);

	UFUNCTION(BlueprintImplementableEvent, Category="Pragmata|Monster")
	void OnHackApplied(AActor* Hacker, float HackDamage);

	UFUNCTION()
	void OnRep_Hacked();

	UFUNCTION()
	void OnRep_Tainted();

	UFUNCTION()
	void OnRep_SpawnAppearancePlaying();

	UFUNCTION()
	void OnRep_StasisStunned();

	UFUNCTION()
	void OnRep_TargetActor();

	virtual void HandleHackStateChanged();
	virtual void HandleTaintStateChanged();
	void ApplyTaintMaterialVisibility();
	void SetHackedState(bool bNewHacked);
	void StartHackDurationTimer(float ResolvedHackDurationSeconds);
	void ExpireHackDuration();
	void ClearHackDurationTimer();

	UFUNCTION()
	void OnTargetDetected(AActor* Actor, FAIStimulus Stimulus);

	UFUNCTION()
	void HandleDeath();
protected:
	// server gameplay와 각 peer의 montage를 한 번만 정리
	virtual void SetDead();
	// 사망 animation의 마지막 포즈를 physics로 이어 바닥에 안착
	void EnableDeathRagdoll();
	// 정상 공격 완료와 분리해 cooldown 없이 공격 상태 정리
	void AbortAttackForDeath();
	// 치명타 직전 공격·이동·피격 상황에 맞는 animation과 ragdoll 시간을 함께 선택
	UAnimSequenceBase* ResolveDeathAnimation(const FPragmataDamageSpec& DamageSpec, float& OutRagdollDelay) const;
	// 반피를 처음 통과한 순간의 공격 pattern·해킹 상태·피격 방향을 montage section으로 변환
	FName ResolveHalfHealthHitReactSection(const FPragmataDamageSpec& DamageSpec) const;
	// 해킹 전 방어 배율을 공통 기본값 또는 monster BP override로 반환
	float ResolveUnhackedDamageTakenMultiplier(const FPragmataDamageSpec& DamageSpec) const;
	// 해킹 유지 상태의 weapon impact bone을 BP 약점 목록과 비교해 대미지 배율 반환
	float ResolveHackedWeakPointDamageMultiplier(const FPragmataDamageSpec& DamageSpec) const;
	// 몬스터별 weapon 피해 취약/저항 배율 반환
	float ResolveWeaponDamageTakenMultiplier(const FPragmataDamageSpec& DamageSpec) const;
	// 공격을 취소하고 확정된 반피 피격 section을 server에서 재생
	bool PlayHalfHealthHitReact(FName SectionName);
	virtual void HandleHalfHealthHitReactMontageEnded(UAnimMontage* Montage, bool bInterrupted);
	// damage source 위치를 monster 기준 Front/Back 방향으로 변환
	void ResolveDamageDirection(FPragmataDamageSpec& DamageSpec) const;
	// Territory 내부 player weapon 피격만 Damage Sense로 보고
	bool ReportWeaponDamageSense(const FPragmataDamageSpec& DamageSpec);
	void HandleStasisStunExpired(TWeakObjectPtr<AActor> StunSource);
	void BeginStasisStun();
	void EndStasisStun();
	void PlayStasisStunLocally();
	void StopStasisStunLocally();

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastSpawnDamageText(float DamageAmount, FVector DamageNumberLocation);

	UFUNCTION(BlueprintImplementableEvent, Category="Pragmata|Monster|Taint")
	void ReceiveTaintStateChanged();

	// server가 damage text 위치를 계산하고 각 peer에 표시 요청
	void SpawnDamageText(float DamageAmount, const FPragmataDamageSpec& DamageSpec);

	// 이동 방향과 target 방향 사이 각도 계산
	void CalculateAngle();
	void UpdateFallingSpawnAppearance();
	void RequestFallingSpawnAppearanceEnd();
	void JumpSpawnAppearanceToSectionLocally(UAnimMontage* Montage, FName SectionName);
	void ApplySpawnAppearanceRootMotionMode();
	void RestoreSpawnAppearanceRootMotionMode();
	// server가 loop 중 target 거리와 최대 지속 시간을 검사
	void UpdateProximityControlledAttackLoop();
	// 공격 시작과 종료에서 loop runtime 상태 초기화
	void ResetProximityControlledAttackLoop();
	// root motion 공중 공격 동안만 Flying과 loop 전진 속도를 적용하고 기존 이동 모드를 복구
	void BeginAttackFlyingMovement(const FMonsterAttackPattern& Pattern);
	void RestoreAttackMovementMode();
	void SetAttackLoopRootMotionIgnored(bool bIgnoreRootMotion);
	void HandleAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted);
	bool PlaySpawnAppearanceLocally(bool bBindEndDelegate);
	void HandleSpawnAppearanceMontageEnded(UAnimMontage* Montage, bool bInterrupted);
	const FMonsterAttackPattern* GetSelectedAttackPattern() const;
	bool IsAttackPatternAvailable(int32 PatternIndex, float CurrentTime) const;
	bool IsAttackPatternAboveMinimumRange(int32 PatternIndex, const AActor* Target) const;
	bool IsAttackPatternInRange(int32 PatternIndex, const AActor* Target) const;
	bool IsAttackPatternPositionRequirementMet(int32 PatternIndex, const AActor* Target) const;
	virtual float GetEffectiveSelectionWeight(int32 PatternIndex) const;
	bool HasAvailableAttackPattern(float CurrentTime) const;
	bool HasAvailableAttackPatternInRange(float CurrentTime, const AActor* Target) const;
	void StartSelectedAttackPatternCooldown();
	void RefreshAttackAvailability();
	void SetBlackboardCanAttack(bool bCanAttack) const;
	void ApplyTargetHPBarVisibility();
protected:
	// 몬스터 체력 상태
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Pragmata|Monster")
	TObjectPtr<UPragmataHealthComponent> HealthComponent = nullptr;

	// 몬스터 진영 상태
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Pragmata|Monster")
	TObjectPtr<UPragmataFactionComponent> FactionComponent = nullptr;

	// Diana enemy hacking puzzle model
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Pragmata|Monster|Hack")
	TObjectPtr<UEnemyHackingPuzzleComponent> EnemyHackingPuzzleComponent = nullptr;

	// scan 표시용 monster id
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Pragmata|Monster")
	FName MonsterId = TEXT("Monster");

	// scan 표시 text override
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Pragmata|Monster")
	FText ScanDisplayText;

	// Diana hack 가능 여부
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Pragmata|Monster")
	bool bHackable = true;

	// hack 성공 후 방어 감소 해제 상태
	UPROPERTY(ReplicatedUsing=OnRep_Hacked, BlueprintReadOnly, Category="Pragmata|Monster")
	bool bHacked = false;

	// 정화 스킬로만 해제되는 enemy 오염 visual 상태
	UPROPERTY(EditDefaultsOnly, ReplicatedUsing=OnRep_Tainted, BlueprintReadOnly, Category="Pragmata|Monster|Taint")
	bool bIsTainted = true;

	// hacked 상태 유지 시간(fallback). 퍼즐 컴포넌트가 있으면 노드 수 기반 지속시간이 우선한다.
	// 이미 hacked 상태에서 완료한 repeat hack은 이 timer를 reset하지 않음
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Pragmata|Monster|Hack", meta=(ClampMin="0.1", Units="s"))
	float HackDurationSeconds = 5.f;

	// 이번 hacked 세션의 실제 적용 지속시간(초) — 노드 수로 산출. 게이지 분모용, 서버가 채워 복제.
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Pragmata|Monster|Hack")
	float AppliedHackDuration = 0.f;

	// hacked 만료 서버 월드 시각(초). 클라가 GetServerWorldTimeSeconds와 비교해 잔량을 계산.
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Pragmata|Monster|Hack")
	float HackedEndServerTime = 0.f;

	// 체크하면 monster BP별 해킹 전 방어 배율을 사용
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pragmata|Monster|Damage")
	bool bOverrideUnhackedDamageTakenMultiplier = false;

	// 해킹 전 non-Hack 피해 배율. override가 꺼져 있으면 공통 0.2를 사용
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pragmata|Monster|Damage", meta=(ClampMin="0.0", UIMin="0.0", EditCondition="bOverrideUnhackedDamageTakenMultiplier", EditConditionHides))
	float UnhackedDamageTakenMultiplier = 0.2f;

	// monster BP별 총기 피해 취약/저항 배율. 0이면 weapon damage 면역, 1이면 기본값
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pragmata|Monster|Damage", meta=(ClampMin="0.0", UIMin="0.0"))
	float WeaponDamageTakenMultiplier = 1.f;

	// 특정 weapon class만 공통 총기 배율 대신 별도 배율로 보정
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pragmata|Monster|Damage")
	TArray<FMonsterWeaponDamageTakenMultiplier> PerWeaponDamageTakenMultipliers;

	// 해킹 유지 상태에서 weapon HitResult.BoneName과 일치할 때만 적용되는 약점 목록
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pragmata|Monster|Weak Point")
	TArray<FMonsterHackedWeakPoint> HackedWeakPoints;

	// 공격 animation/BT 표시 상태
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Pragmata|Monster")
	bool bIsAttacking = false;

	// Territory runtime spawn에서 선택적으로 재생할 monster별 appearance montage
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Pragmata|Monster|Appearance")
	TObjectPtr<UAnimMontage> SpawnAppearanceMontage = nullptr;

	// appearance 동안 server movement와 공격을 잠그고 client montage를 동기화
	UPROPERTY(ReplicatedUsing=OnRep_SpawnAppearancePlaying, BlueprintReadOnly, Category="Pragmata|Monster|Appearance")
	bool bSpawnAppearancePlaying = false;

	UPROPERTY(ReplicatedUsing=OnRep_SpawnAppearancePlaying, BlueprintReadOnly, Category="Pragmata|Monster|Appearance")
	TObjectPtr<UAnimMontage> ActiveSpawnAppearanceMontage = nullptr;

	UPROPERTY(ReplicatedUsing=OnRep_SpawnAppearancePlaying, BlueprintReadOnly, Category="Pragmata|Monster|Appearance")
	bool bSpawnAppearanceIgnoresRootMotion = false;

	UPROPERTY(ReplicatedUsing=OnRep_StasisStunned, BlueprintReadOnly, Category="Pragmata|Monster|Stasis")
	bool bStasisStunned = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pragmata|Monster|Stasis")
	TObjectPtr<UAnimMontage> StasisStunMontage = nullptr;

	// monster 종류별 공격 외 사망 animation
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pragmata|Monster|Death")
	FMonsterDeathAnimationSet DeathAnimations;

	// 반피를 처음 통과할 때 상황별 section을 재생할 공통 montage
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pragmata|Monster|Hit React")
	TObjectPtr<UAnimMontage> HalfHealthHitReactMontage = nullptr;

	// 공격 pattern 전용 section이 없을 때 일반/해킹 상태를 section으로 매핑
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pragmata|Monster|Hit React")
	FMonsterHalfHealthHitReactSectionSet HalfHealthHitReactSections;

	// 치명타 직전 확정해 모든 AnimBP가 같은 사망 animation을 재생
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Pragmata|Monster|Death")
	TObjectPtr<UAnimSequenceBase> CurrentDeathAnimation = nullptr;

	// 선택된 사망 animation과 함께 각 peer에 전달할 ragdoll 전환 시간
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Pragmata|Monster|Death")
	float CurrentDeathRagdollDelay = 1.f;

	// 사망 후 actor 제거까지 대기 시간, 0이면 자동 제거하지 않음
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Pragmata|Monster|Death", meta=(ClampMin="0.0", Units="s"))
	float DeadEventDelayTime = 5.f;

	// authoritative death 1회당 shared Hugh stats에 지급할 재화. BP별 override 가능
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Pragmata|Monster|Reward", meta=(ClampMin="0"))
	int32 LunafilamentReward = 50;

	// ActiveBossTarget가 이 monster일 때 한 번만 imminent victory 대사를 요청할 HP 비율
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Pragmata|Monster|Diana Voice", meta=(ClampMin="0.0", ClampMax="1.0"))
	float DianaImminentVictoryHealthRatio = 0.25f;

	// Mesh-only 공중 공격 포즈에서 Death animation으로 내려오는 blend 시간
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Pragmata|Monster|Death", meta=(ClampMin="0.0", Units="s"))
	float DeathMontageBlendOutTime = 0.35f;

	// 사망 animation 이후 mesh physics 전환 사용 여부
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Pragmata|Monster|Death")
	bool bEnableDeathRagdoll = true;

	// animation별 시간이 -1일 때 사용할 공통 ragdoll 전환 시간
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Pragmata|Monster|Death", meta=(EditCondition="bEnableDeathRagdoll", ClampMin="0.0", Units="s"))
	float DeathRagdollDelay = 1.f;

	bool bTrackTargetDuringAttack = false;
	bool bDeathHandled = false;
	bool bDianaImminentVictoryAnnounced = false;
	bool bHalfHealthHitReactPlayed = false;
	bool bHalfHealthHitReactPlaying = false;
	FTimerHandle DeathRagdollTimerHandle;
	TMap<TWeakObjectPtr<AActor>, FTimerHandle> StasisStunSourceTimers;
	FTimerHandle HackDurationTimerHandle;

	// target 기준 이동 각도
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Pragmata|Monster")
	float fAngle = 0.f;

	// BehaviorTree Asset
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pragmata|AI")
	TObjectPtr<UBehaviorTree> BTAsset = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pragmata|AI")
	TObjectPtr<UAIPerceptionComponent> AIPerceptionComponent = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pragmata|AI")
	TObjectPtr<UAISenseConfig_Sight> SightConfig = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pragmata|AI")
	TObjectPtr<UAISenseConfig_Damage> DamageConfig = nullptr;

	UPROPERTY(ReplicatedUsing=OnRep_TargetActor, BlueprintReadOnly, Category = "Pragmata|AI")
	TObjectPtr<AActor> TargetActor = nullptr;

	// Monster Data
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pragmata|Data")
	FMonsterCombatData CombatData;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pragmata|Attack")
	TArray<FMonsterAttackPattern> AttackPatterns;

	// Attack Cooldown
	float NextAttackAllowedTime = 0.f;
	TArray<float> AttackPatternNextAllowedTimes;
	int32 SelectedAttackPatternIndex = INDEX_NONE;
	int32 LastUsedAttackPatternIndex = INDEX_NONE;
	FTimerHandle AttackCooldownTimerHandle;
	bool bProximityControlledAttackLoopActive = false;
	bool bAttackLoopEndRequested = false;
	float AttackLoopStartTime = -1.f;
	bool bAttackFlyingMovementActive = false;
	EMovementMode SavedAttackMovementMode = MOVE_Walking;
	uint8 SavedAttackCustomMovementMode = 0;
	float SavedAttackMaxFlySpeed = 0.f;
	bool bAttackLoopRootMotionModeSaved = false;
	TEnumAsByte<ERootMotionMode::Type> SavedAttackLoopRootMotionMode = ERootMotionMode::RootMotionFromMontagesOnly;
	bool bAutoRunAIOnPossess = true;
	bool bSpawnAppearanceUsesFalling = false;
	bool bSpawnAppearanceEndRequested = false;
	FName SpawnAppearanceFallingEndSectionName = NAME_None;
	float SpawnAppearanceFallingEndTransitionDistance = 0.f;
	bool bSpawnAppearanceRootMotionModeSaved = false;
	TEnumAsByte<ERootMotionMode::Type> SavedSpawnAppearanceRootMotionMode = ERootMotionMode::RootMotionFromMontagesOnly;

	// SetAIAttackDelegate 함수로부터 전달받은 델리게이트를 저장할 변수.
	FMonsterAttackFinished OnAttackFinished;

	// floating damage text actor class
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat")
	TSubclassOf<AActor> DamageTextClass;
	
protected:
	bool IsTaunted = false;
public:
	FORCEINLINE void SetIsTaunted(bool flag) { IsTaunted = flag; }
	FORCEINLINE bool GetIsTaunted() const { return IsTaunted; }
};
