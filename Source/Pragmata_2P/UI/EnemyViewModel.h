// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "EnemyViewModel.generated.h"

class UPragmataHealthComponent;

/**
 * 적 1기의 HP / 그로기 게이지 UI용 ViewModel.
 * - HP: 타겟 적의 UPragmataHealthComponent.OnHealthChanged 에 바인딩해 자동 갱신.
 * - 그로기: 백킹 시스템이 아직 없어 외부가 UpdateGroggy()로 푸시 (그로기 컴포넌트 생기면 HP처럼 바인딩 전환).
 * HPBar / GroggyBar WBP는 각각 HealthRatio / GroggyRatio(0~1)를 머티리얼 Percent에 바인딩.
 */
UCLASS()
class PRAGMATA_2P_API UEnemyViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	// --- FieldNotify Getters (HP) ---

	// 현재 체력 (텍스트 표시용)
	UFUNCTION(BlueprintPure, Category = "Pragmata|EnemyVM")
	float GetCurrentHealth() const { return CurrentHealth; }

	// 최대 체력
	UFUNCTION(BlueprintPure, Category = "Pragmata|EnemyVM")
	float GetMaxHealth() const { return MaxHealth; }

	// HP 바 채움 비율 (0~1) — 머티리얼 Percent 바인딩용
	UFUNCTION(BlueprintPure, Category = "Pragmata|EnemyVM")
	float GetHealthRatio() const { return HealthRatio; }

	UFUNCTION(BlueprintPure, Category = "Pragmata|EnemyVM")
	bool GetIsDead() const { return bIsDead; }

	// --- FieldNotify Getters (그로기) ---

	// 현재 그로기 수치
	UFUNCTION(BlueprintPure, Category = "Pragmata|EnemyVM")
	float GetGroggyValue() const { return GroggyValue; }

	// 최대 그로기 수치
	UFUNCTION(BlueprintPure, Category = "Pragmata|EnemyVM")
	float GetMaxGroggy() const { return MaxGroggy; }

	// 그로기 바 채움 비율 (0~1) — 머티리얼 Percent 바인딩용
	UFUNCTION(BlueprintPure, Category = "Pragmata|EnemyVM")
	float GetGroggyRatio() const { return GroggyRatio; }

	// --- 생명주기 (HP) ---

	// 타겟 적을 받아 그 적의 HealthComponent에 바인딩 (null이면 해제) — 통합 진입점.
	UFUNCTION(BlueprintCallable, Category = "Pragmata|EnemyVM")
	void SetEnemyTarget(AActor* TargetActor);

	UFUNCTION(BlueprintCallable, Category = "Pragmata|EnemyVM")
	void BindToHealth(UPragmataHealthComponent* HealthComp);

	UFUNCTION(BlueprintCallable, Category = "Pragmata|EnemyVM")
	void UnbindFromHealth();

	// --- 그로기 (외부 푸시) ---

	// 그로기 시스템이 현재/최대 그로기 수치를 밀어넣음 (그로기 컴포넌트 생기면 델리게이트 바인딩으로 교체 예정).
	UFUNCTION(BlueprintCallable, Category = "Pragmata|EnemyVM")
	void UpdateGroggy(float InCurrent, float InMax);

private:
	UFUNCTION()
	void HandleHealthChanged(float InCurrentHealth, float InMaxHealth);

	UFUNCTION()
	void HandleDeath();

	void SetCurrentHealth(float NewValue);
	void SetMaxHealth(float NewValue);
	void SetHealthRatio(float NewValue);
	void SetIsDead(bool bNewValue);
	void SetGroggyValue(float NewValue);
	void SetMaxGroggy(float NewValue);
	void SetGroggyRatio(float NewValue);

	void UpdateHealthRatio();
	void UpdateGroggyRatio();

	void SyncFromHealth();

private:
	// --- HP ---
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Pragmata|EnemyVM", meta = (AllowPrivateAccess = "true"))
	float CurrentHealth = 0.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Pragmata|EnemyVM", meta = (AllowPrivateAccess = "true"))
	float MaxHealth = 0.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Pragmata|EnemyVM", meta = (AllowPrivateAccess = "true"))
	float HealthRatio = 0.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Pragmata|EnemyVM", meta = (AllowPrivateAccess = "true"))
	bool bIsDead = false;

	// --- 그로기 ---
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Pragmata|EnemyVM", meta = (AllowPrivateAccess = "true"))
	float GroggyValue = 0.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Pragmata|EnemyVM", meta = (AllowPrivateAccess = "true"))
	float MaxGroggy = 0.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Pragmata|EnemyVM", meta = (AllowPrivateAccess = "true"))
	float GroggyRatio = 0.f;

	TWeakObjectPtr<UPragmataHealthComponent> BoundHealthComp;
};
