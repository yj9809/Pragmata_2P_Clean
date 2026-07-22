// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "EnemyHackingWidget.generated.h"

class UWidget;
class UEnemyHackingPuzzleViewModel;

UCLASS()
class PRAGMATA_2P_API UEnemyHackingWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Pragmata|EnemyHackUI")
	void SetEnemyHackingVisible(bool bVisible);

	UFUNCTION(BlueprintCallable, Category = "Pragmata|EnemyHackUI")
	void SetHackTarget(AActor* TargetActor);

	UFUNCTION(BlueprintCallable, Category = "Pragmata|EnemyHackUI")
	void SetHackCountdownDurationSeconds(float NewDurationSeconds);

	// 완료 판정 시 C++가 호출해 BP 성공 애니메이션 시작
	UFUNCTION(BlueprintImplementableEvent, Category = "Pragmata|EnemyHackUI")
	void OnHackCompleted();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	UFUNCTION()
	void HandleHackCompleted(float FinalDamage);

	void BindToViewModel(UEnemyHackingPuzzleViewModel* ViewModel);
	void UnbindFromViewModel();
	// 게이지 잔량을 ViewModel로 밀어넣음(0으로 리셋할 때 사용)
	void StopHackCountdown(bool bResetViewModel);
	// 매 틱 몬스터의 복제된 hacked 지속시간을 읽어 게이지에 반영 — UI를 닫았다 열어도 정확
	void RefreshHackCountdown();
	void PushHackCountdown(float RemainingSeconds, float DurationSeconds, float FullGaugeRatio);

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UWidget> Root;

	// (deprecated) 지속시간은 이제 몬스터 복제 상태가 소유. BP 호환을 위해 setter는 유지되나 미사용.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pragmata|EnemyHackUI", meta = (AllowPrivateAccess = "true", ClampMin = "0.1", Units = "s"))
	float HackCountdownDurationSeconds = 10.f;

	UPROPERTY(Transient)
	TObjectPtr<UEnemyHackingPuzzleViewModel> BoundViewModel = nullptr;
};
