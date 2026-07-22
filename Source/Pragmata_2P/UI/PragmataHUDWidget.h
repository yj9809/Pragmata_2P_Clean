// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Data/PragmataTypes.h"
#include "PragmataHUDWidget.generated.h"

class UWeaponWheelWidget;
class UBossHPWidget;
class UWidget;
class AActor;
class UEnemyHackingWidget;
class UMapHackingWidget;
class UObjectiveAlertWidget;
class UDialogueWidget;
class AHughCharacter;
class ADianaOperatorPawn;
class UPragmataHUDViewModel;

/**
 *
 */
UCLASS()
class PRAGMATA_2P_API UPragmataHUDWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

public:
	explicit UPragmataHUDWidget(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category = "Pragmata|HUD Motion")
	void AddHUDShake(FVector2D ShakeAmount);

	UFUNCTION(BlueprintCallable, Category="Pragmata|Weapon Wheel")
	void SetWeaponWheelVisible(bool bNewVisible);

	UFUNCTION(BlueprintCallable, Category="Pragmata|Hack")
	void HandleActiveEnemyHackTargetChanged(AActor* TargetActor);

	UFUNCTION(BlueprintCallable, Category="Pragmata|HUD")
	void BindSharedHUDStats(AHughCharacter* HughCharacter, ADianaOperatorPawn* DianaPawn);

	UFUNCTION(BlueprintCallable, Category="Pragmata|Objective")
	void ShowObjectiveAlert(const FText& ObjectiveText, float DisplaySeconds = 4.f);

	// 대사 표시 판정/타이밍은 게임 로직이 소유하고 HUD는 로컬 화면 표시만 담당
	UFUNCTION(BlueprintCallable, Category="Pragmata|Dialogue")
	void PlayDialogueLine(const FDialogueLine& Line, float DisplaySeconds = -1.f);

	UFUNCTION(BlueprintCallable, Category="Pragmata|Dialogue")
	void HideDialogue();

	UFUNCTION(BlueprintCallable, Category="Pragmata|Hack")
	void HandleActiveMapPuzzleActorChanged(AActor* PuzzleActor);

	UFUNCTION(BlueprintCallable, Category="Pragmata|Boss")
	void HandleActiveBossTargetChanged(AActor* BossTarget);

	UFUNCTION(BlueprintImplementableEvent, Category="Pragmata|Hack")
	void OnActiveEnemyHackTargetChanged(AActor* TargetActor);

	UFUNCTION(BlueprintImplementableEvent, Category="Pragmata|Hack")
	void OnActiveMapPuzzleActorChanged(AActor* PuzzleActor);

	UFUNCTION(BlueprintImplementableEvent, Category="Pragmata|Boss")
	void OnActiveBossTargetChanged(AActor* BossTarget);

	UFUNCTION(BlueprintImplementableEvent, Category="Pragmata|Combat")
	void OnEnemyCombatActiveChanged(bool bNewActive);

private:
	FVector2D CalculateLookSway(float DeltaTime);
	FVector2D CalculateMovementSway(float DeltaTime);
	void UpdateShake(float DeltaTime);
	void UpdateIdleHide(float DeltaTime);
	bool DetectPlayerActivity();
	UWeaponWheelWidget* ResolveWeaponWheelRoot();
	UBossHPWidget* ResolveBossHPWidget();
	UObjectiveAlertWidget* ResolveObjectiveAlertWidget();
	UDialogueWidget* ResolveDialogueWidget();
	UPragmataHUDViewModel* ResolveHUDViewModel() const;

private:
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UWidget> HUDMotionRoot = nullptr;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UWeaponWheelWidget> WeaponWheelRoot = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UEnemyHackingWidget> EnemyHackingWidget = nullptr;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UBossHPWidget> BossHPWidget = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UPragmataHUDViewModel> BoundHUDViewModel = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UMapHackingWidget> MapHackingWidget = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UObjectiveAlertWidget> ObjectiveAlertWidget = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UDialogueWidget> DialogueWidget = nullptr;

	UPROPERTY(EditAnywhere, Category = "Pragmata|HUD Motion")
	bool bEnableHUDMotion = true;

	UPROPERTY(EditAnywhere, Category = "Pragmata|HUD Motion|Look")
	float LookSwayAmount = 6.f;

	UPROPERTY(EditAnywhere, Category = "Pragmata|HUD Motion|Look")
	float LookSwayInterpSpeed = 10.f;

	UPROPERTY(EditAnywhere, Category = "Pragmata|HUD Motion|Look")
	float MaxLookSwayOffset = 14.f;

	UPROPERTY(EditAnywhere, Category = "Pragmata|HUD Motion|Movement")
	float MovementSwayAmount = 2.f;

	UPROPERTY(EditAnywhere, Category = "Pragmata|HUD Motion|Movement")
	float MovementSwayInterpSpeed = 8.f;

	UPROPERTY(EditAnywhere, Category = "Pragmata|HUD Motion|Movement")
	float MaxMovementSwayOffset = 6.f;

	UPROPERTY(EditAnywhere, Category = "Pragmata|HUD Motion|Shake")
	float ShakeRecoverySpeed = 16.f;

	// 일정 시간 조작이 없으면 HUD를 페이드 아웃했다가, 다시 조작하면 페이드 인
	UPROPERTY(EditAnywhere, Category = "Pragmata|HUD Idle Hide")
	bool bEnableIdleHide = true;

	// 이 시간(초) 동안 조작이 없으면 HUD를 숨김
	UPROPERTY(EditAnywhere, Category = "Pragmata|HUD Idle Hide")
	float IdleHideDelay = 5.f;

	// 페이드 인/아웃 보간 속도 (클수록 빠름)
	UPROPERTY(EditAnywhere, Category = "Pragmata|HUD Idle Hide")
	float IdleFadeSpeed = 5.f;

	// 이 각도(도)보다 큰 시야 회전이 있으면 조작 중으로 판정
	UPROPERTY(EditAnywhere, Category = "Pragmata|HUD Idle Hide")
	float LookActivityThreshold = 0.1f;

	// 이 속도(cm/s)보다 빠르게 이동하면 조작 중으로 판정
	UPROPERTY(EditAnywhere, Category = "Pragmata|HUD Idle Hide")
	float MoveActivityThreshold = 1.f;

	FVector2D CurrentMotionOffset = FVector2D::ZeroVector;
	FVector2D LookSwayOffset = FVector2D::ZeroVector;
	FVector2D MovementSwayOffset = FVector2D::ZeroVector;
	FVector2D ShakeOffset = FVector2D::ZeroVector;

	FRotator PreviousControlRotation = FRotator::ZeroRotator;
	bool bHasPreviousControlRotation = false;

	// Idle hide 상태 (HUD Motion과 독립적으로 동작)
	float IdleTimer = 0.f;
	float CurrentHUDOpacity = 1.f;
	FRotator PreviousIdleControlRotation = FRotator::ZeroRotator;
	FRotator PreviousIdlePawnRotation = FRotator::ZeroRotator;
	bool bHasPreviousIdleRotation = false;
	bool bLoggedMissingWeaponWheelRoot = false;
	bool bLoggedMissingObjectiveAlertWidget = false;
	bool bLoggedMissingDialogueWidget = false;
};
