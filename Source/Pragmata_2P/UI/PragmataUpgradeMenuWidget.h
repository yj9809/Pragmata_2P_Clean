#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Data/PragmataTypes.h"
#include "PragmataUpgradeMenuWidget.generated.h"

class APragmataGameState;
class AHughCharacter;
class UPragmataPlayerStatsComponent;
class UUpgradeItemRowWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPragmataUpgradeMenuChangedSignature);

// F(강화)/C(환급) 홀드 진입점이 어느 쪽인지 구분
UENUM(BlueprintType)
enum class EPragmataUpgradeHoldSide : uint8
{
	Upgrade,
	Refund
};

UCLASS()
class PRAGMATA_2P_API UPragmataUpgradeMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Pragmata|Upgrade")
	void BindPlayerStatsComponent(UPragmataPlayerStatsComponent* PlayerStatsComponent);

	UFUNCTION(BlueprintCallable, Category="Pragmata|Upgrade")
	void UnbindPlayerStatsComponent();

	UFUNCTION(BlueprintCallable, Category="Pragmata|Upgrade")
	void RefreshSharedUpgradeStats();

	UFUNCTION(BlueprintCallable, Category="Pragmata|Upgrade")
	void SetSelectedUpgradeType(EPragmataUpgradeType UpgradeType);

	UFUNCTION(BlueprintCallable, Category="Pragmata|Upgrade")
	void SelectNextUpgrade();

	UFUNCTION(BlueprintCallable, Category="Pragmata|Upgrade")
	void SelectPreviousUpgrade();

	UFUNCTION(BlueprintCallable, Category="Pragmata|Upgrade")
	void RequestSelectedUpgrade();

	UFUNCTION(BlueprintCallable, Category="Pragmata|Upgrade")
	void RequestSelectedRefund();

	UFUNCTION(BlueprintPure, Category="Pragmata|Upgrade")
	EPragmataUpgradeType GetSelectedUpgradeType() const { return SelectedUpgradeType; }

	UFUNCTION(BlueprintPure, Category="Pragmata|Upgrade")
	int32 GetLunafilament() const;

	UFUNCTION(BlueprintPure, Category="Pragmata|Upgrade")
	int32 GetUpgradeLevel(EPragmataUpgradeType UpgradeType) const;

	UFUNCTION(BlueprintPure, Category="Pragmata|Upgrade")
	int32 GetMaxUpgradeLevel(EPragmataUpgradeType UpgradeType) const;

	UFUNCTION(BlueprintPure, Category="Pragmata|Upgrade")
	int32 GetUpgradeCost(EPragmataUpgradeType UpgradeType) const;

	UFUNCTION(BlueprintPure, Category="Pragmata|Upgrade")
	bool CanUpgrade(EPragmataUpgradeType UpgradeType) const;

	UFUNCTION(BlueprintPure, Category="Pragmata|Upgrade")
	bool CanRefundUpgrade(EPragmataUpgradeType UpgradeType) const;

	UFUNCTION(BlueprintPure, Category="Pragmata|Upgrade")
	bool IsUpgradeCostAffordable(EPragmataUpgradeType UpgradeType) const;

	UFUNCTION(BlueprintPure, Category="Pragmata|Upgrade")
	float GetUpgradeCurrentValue(EPragmataUpgradeType UpgradeType) const;

	UFUNCTION(BlueprintPure, Category="Pragmata|Upgrade")
	float GetUpgradeNextValue(EPragmataUpgradeType UpgradeType) const;

	UFUNCTION(BlueprintPure, Category="Pragmata|Upgrade")
	int32 GetUpgradeValueFractionalDigits(EPragmataUpgradeType UpgradeType) const;

	UPROPERTY(BlueprintAssignable, Category="Pragmata|Upgrade")
	FPragmataUpgradeMenuChangedSignature OnUpgradeMenuChanged;

	// 홀드 코어 진입점 — 입력 어댑터(UMG 키/Enhanced Input 등)가 호출. 입력 방식과 무관
	UFUNCTION(BlueprintCallable, Category="Pragmata|Upgrade|Hold")
	void BeginHold(EPragmataUpgradeHoldSide Side);

	UFUNCTION(BlueprintCallable, Category="Pragmata|Upgrade|Hold")
	void EndHold(EPragmataUpgradeHoldSide Side);

	// 오픈 트리거 확정 전, F/C 홀드를 눌러보기 위한 임시 진입점 (포커스/입력모드 세팅)
	UFUNCTION(BlueprintCallable, Category="Pragmata|Upgrade|Hold")
	void OpenForTest();

	UFUNCTION(BlueprintCallable, Category="Pragmata|Upgrade")
	void SetReadOnly(bool bNewReadOnly);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnKeyUp(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	UFUNCTION()
	void HandleLunafilamentChanged(int32 NewLunafilament);

	UFUNCTION()
	void HandleDamageMultiplierChanged(float NewMultiplier);

	UFUNCTION()
	void HandleUpgradeLevelChanged(EPragmataUpgradeType UpgradeType, int32 UpgradeLevel);

	// 강화 상태 변경 시 두 로우의 홀드 버튼(F/C) 표시 규칙을 갱신
	UFUNCTION()
	void HandleMenuChangedForButtons();

	void ApplyHoldButtonStates(EPragmataUpgradeType UpgradeType, UUpgradeItemRowWidget* Row);

	void HandleHughCharacterChanged(AHughCharacter* HughCharacter);
	void HandleCheckpointUpgradeSelectionChanged(EPragmataUpgradeType UpgradeType);
	void HandleCheckpointUpgradeHoldChanged(float Progress, bool bRefund);
	void BindGameState(APragmataGameState* GameState);
	void UnbindGameState();
	void ApplySelectedUpgradeType(EPragmataUpgradeType UpgradeType);
	AHughCharacter* ResolveSharedHughCharacter() const;
	float GetUpgradeValueAtLevel(EPragmataUpgradeType UpgradeType, int32 UpgradeLevel) const;
	EPragmataUpgradeType GetAdjacentSelectableUpgrade(int32 Direction) const;
	void BroadcastMenuChanged();

	// 현재 선택된 타입에 해당하는 로우 반환 (SuitRow/WeaponRow)
	UUpgradeItemRowWidget* GetActiveRow() const;

	UPROPERTY(BlueprintReadOnly, Category="Pragmata|Upgrade", meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UUpgradeItemRowWidget> SuitRow;

	UPROPERTY(BlueprintReadOnly, Category="Pragmata|Upgrade", meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UUpgradeItemRowWidget> WeaponRow;

	// 홀드 완료까지 걸리는 시간(초). 에디터에서 조정 가능
	UPROPERTY(EditAnywhere, Category="Pragmata|Upgrade|Hold")
	float HoldDuration = 0.8f;

	TWeakObjectPtr<APragmataGameState> BoundGameState;
	TWeakObjectPtr<UPragmataPlayerStatsComponent> BoundPlayerStatsComponent;
	EPragmataUpgradeType SelectedUpgradeType = EPragmataUpgradeType::Suit;

	float HoldProgress = 0.f;
	bool bHolding = false;
	bool bFired = false;
	bool bReadOnly = false;
	EPragmataUpgradeHoldSide HoldSide = EPragmataUpgradeHoldSide::Upgrade;
};
