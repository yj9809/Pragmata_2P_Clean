#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/PragmataTypes.h"
#include "PragmataHealthComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPragmataHealthChangedSignature, float, CurrentHealth, float, MaxHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPragmataDeathSignature);

UCLASS(ClassGroup=(Pragmata), meta=(BlueprintSpawnableComponent))
class PRAGMATA_2P_API UPragmataHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// 기본 체력 컴포넌트 생성
	UPragmataHealthComponent();

	// 체력 상태 replication 등록
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 서버 권한에서 최종 체력 차감
	UFUNCTION(BlueprintCallable, Category="Pragmata|Health")
	bool ReceiveDamage(const FPragmataDamageSpec& DamageSpec);

	// 서버 권한에서 체력 회복
	UFUNCTION(BlueprintCallable, Category="Pragmata|Health")
	void Heal(float HealAmount);

	// 서버 권한에서 int 기반 HP 회복
	UFUNCTION(BlueprintCallable, Category="Pragmata|Health")
	bool RecoverHP(int32 RecoveryAmount);

	// 현재 체력 조회
	UFUNCTION(BlueprintPure, Category="Pragmata|Health")
	float GetCurrentHealth() const { return CurrentHealth; }

	// 현재 HP 정수 표시값 조회
	UFUNCTION(BlueprintPure, Category="Pragmata|Health")
	int32 GetCurrentHP() const { return FMath::RoundToInt(CurrentHealth); }

	// 최대 체력 조회
	UFUNCTION(BlueprintPure, Category="Pragmata|Health")
	float GetMaxHealth() const { return MaxHealth; }

	// 최대 HP 정수 표시값 조회
	UFUNCTION(BlueprintPure, Category="Pragmata|Health")
	int32 GetMaxHP() const { return FMath::RoundToInt(MaxHealth); }

	// 사망 상태 조회
	UFUNCTION(BlueprintPure, Category="Pragmata|Health")
	bool IsDead() const { return bIsDead; }

	// 특정 actor class의 기본 체력값을 constructor에서 지정
	void SetDefaultHealth(int32 NewMaxHP);

	// 강화 레벨에서 계산한 최대 HP를 서버 체력 source에 반영
	UFUNCTION(BlueprintCallable, Category="Pragmata|Health")
	bool SetMaxHPFromUpgrade(int32 NewMaxHP);

	// checkpoint/respawn server reset에서 death state와 HP를 최대치로 복구
	UFUNCTION(BlueprintCallable, Category="Pragmata|Health")
	bool RestoreFullHealth();

	UFUNCTION(BlueprintCallable, Category="Pragmata|Health")
	void SetDamageInvulnerable(bool bNewInvulnerable);

	// 체력 변경 알림
	UPROPERTY(BlueprintAssignable, Category="Pragmata|Health")
	FPragmataHealthChangedSignature OnHealthChanged;

	// 사망 알림
	UPROPERTY(BlueprintAssignable, Category="Pragmata|Health")
	FPragmataDeathSignature OnDeath;

protected:
	// 시작 시 서버 체력값 보정
	virtual void BeginPlay() override;

private:
	// 복제 체력 변경을 UI에 전달
	UFUNCTION()
	void OnRep_HealthSnapshot();

	// 복제 사망 상태를 local event로 전달
	UFUNCTION()
	void OnRep_IsDead();

	// 서버 권한 체력 회복 공통 경로
	bool ApplyRecovery(float RecoveryAmount);

	// 최대 체력
	UPROPERTY(EditDefaultsOnly, ReplicatedUsing=OnRep_HealthSnapshot, Category="Pragmata|Health", meta=(ClampMin="0"))
	float MaxHealth = 100.f;

	// 현재 체력
	UPROPERTY(ReplicatedUsing=OnRep_HealthSnapshot, VisibleInstanceOnly, Category="Pragmata|Health")
	float CurrentHealth = 100.f;

	// 사망 여부
	UPROPERTY(ReplicatedUsing=OnRep_IsDead, VisibleInstanceOnly, Category="Pragmata|Health")
	bool bIsDead = false;

	UPROPERTY(Replicated, VisibleInstanceOnly, Category="Pragmata|Health")
	bool bDamageInvulnerable = false;
};
