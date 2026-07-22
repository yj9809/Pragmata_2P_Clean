// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/PragmataScannable.h"
#include "GameFramework/Actor.h"
#include "PropInteractableInterface.h"

#include "PropInteractionBase.generated.h"

class UPropInteractionComponent;
class UWidgetComponent;
class UScanIconWidget;
class UHoldKeyPromptWidget;
class UTexture2D;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPropInteractionActivatedSignature, AActor*, Interactor);

UCLASS()
class PRAGMATA_2P_API APropInteractionBase : public AActor, public IPropInteractableInterface, public IPragmataScannable
{
	GENERATED_BODY()

public:
	APropInteractionBase();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FPragmataScanResult GetPragmataScanResult_Implementation(AActor* ScanRequester) const override;
	float GetDianaInteractionDistance() const { return DianaInteractionDistance; }

	UFUNCTION(BlueprintCallable, Category="Interaction|Widget")
	void ShowInteractionPromptWidget();

	UFUNCTION(BlueprintCallable, Category="Interaction|Widget")
	void HideInteractionPromptWidget();

	virtual bool ShouldShowInteractionPromptOnDianaFocus() const;

	UPROPERTY(BlueprintAssignable, Category="Interaction")
	FPropInteractionActivatedSignature OnInteractionActivated;

protected:
	UFUNCTION(BlueprintCallable, Category="Interaction", meta=(BlueprintProtected="true"))
	void BroadcastInteractionActivated(AActor* Interactor);

	virtual bool ShouldCreateInteractionPromptWidget() const;

	UFUNCTION(BlueprintCallable, Category="Interaction|Widget")
	UHoldKeyPromptWidget* EnsureHoldKeyPromptWidget();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Interaction|Widget")
	TSubclassOf<UHoldKeyPromptWidget> HoldKeyPromptWidgetClass = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Interaction|Widget")
	TObjectPtr<UTexture2D> InteractionPromptKeyTexture = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category="Interaction|Widget")
	TObjectPtr<UHoldKeyPromptWidget> HoldKeyPromptWidget = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category="Interaction|Widget")
	TObjectPtr<UWidgetComponent> InteractionPromptWidgetComponent = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Interaction")
	UPropInteractionComponent* InteractionComponent = nullptr;

	// Scan 대상 prop이 공통 screen-space icon을 소유해 HUD marker를 만들지 않는다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Pragmata|Scan")
	UWidgetComponent* ScanIconComponent = nullptr;

	UPROPERTY(EditDefaultsOnly, Category="Pragmata|Scan")
	TSoftClassPtr<UScanIconWidget> ScanIconWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Pragmata|Scan")
	bool bCanBeScanned = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Pragmata|Scan")
	EPragmataScanTargetKind ScanTargetKind = EPragmataScanTargetKind::Unknown;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Pragmata|Scan")
	FName ScanTargetId = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Pragmata|Scan")
	FText ScanDisplayText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Pragmata|Interaction", meta=(ClampMin="0"))
	float DianaInteractionDistance = 0.f;
};
