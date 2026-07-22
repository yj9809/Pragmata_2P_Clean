// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RecoveryItemContainerWidget.generated.h"

class URecoveryItemIcon;
class UPanelWidget;

/**
 *
 */
enum class ERecoveryItemIconState : uint8
{
	Hidden,
	Available,
	Used
};

UCLASS()
class PRAGMATA_2P_API URecoveryItemContainerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Pragmata|Recovery")
	void SetRecoveryItemCount(int32 NewCurrentCount, int32 NewMaxCount);

	UFUNCTION(BlueprintCallable, Category="Pragmata|Recovery")
	void SetCurrentRecoveryItemCount(int32 NewCurrentCount);

	UFUNCTION(BlueprintCallable, Category="Pragmata|Recovery")
	void SetMaxRecoveryItemCount(int32 NewMaxCount);

protected:
	virtual void NativeConstruct() override;

private:
	void RebuildRecoveryItemIcons();

	void ApplyRecoveryItemLayout(bool bForce);

	void ApplyIconState(URecoveryItemIcon* Icon, ERecoveryItemIconState NewState);

	ERecoveryItemIconState GetIconStateForIndex(int32 Index) const;

private:
	static constexpr int32 MaxRecoveryItemSlot = 7;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UPanelWidget> RecoveryItemPanel = nullptr;

	UPROPERTY(EditAnywhere, Category="Pragmata|Recovery")
	FMargin IconPadding = FMargin(2.f, 0.f);

	UPROPERTY(EditDefaultsOnly, Category="Pragmata|Recovery")
	TSubclassOf<URecoveryItemIcon> RecoveryItemIconClass;

	TArray<TObjectPtr<URecoveryItemIcon>> RecoveryItemIcons;
	TArray<ERecoveryItemIconState> CachedIconState;

	int32 CurrentRecoveryItemCount = MaxRecoveryItemSlot;
	int32 MaxRecoveryItemCount = MaxRecoveryItemSlot;
};
