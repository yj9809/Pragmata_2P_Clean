#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "PragmataTitlePlayerController.generated.h"

class UInputAction;
class UInputMappingContext;
class UPragmataTitleScreenWidget;

UCLASS()
class PRAGMATA_2P_API APragmataTitlePlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	APragmataTitlePlayerController();

	virtual void SetupInputComponent() override;

	UFUNCTION(BlueprintCallable, Category="Pragmata|Title")
	void RequestStartFromTitle();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void SetupTitleCameraView();
	void ShowTitleWidget();
	void ApplyTitleInputMappingContext();
	void RemoveTitleInputMappingContext();
	UInputMappingContext* LoadTitleInputMappingContext() const;
	UInputAction* LoadTitleInputAction() const;
	bool IsTitleMapWorld() const;
	void HandleStartPressed();
	void TravelToLobby();

	UPROPERTY(EditDefaultsOnly, Category="Pragmata|Title")
	TSoftClassPtr<UPragmataTitleScreenWidget> TitleWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category="Pragmata|Title")
	float FadeToBlackSeconds = 0.8f;

	UPROPERTY(EditDefaultsOnly, Category="Pragmata|Title")
	FName TitleCameraTag = TEXT("TitleCamera");

	UPROPERTY(EditDefaultsOnly, Category="Pragmata|Title")
	FTransform FallbackCameraTransform = FTransform(FRotator(-8.0, 0.0, 0.0), FVector(-520.0, 0.0, 170.0), FVector::OneVector);

	UPROPERTY(EditDefaultsOnly, Category="Pragmata|Title")
	TSoftObjectPtr<UInputMappingContext> TitleInputMappingContext;

	UPROPERTY(EditDefaultsOnly, Category="Pragmata|Title")
	TSoftObjectPtr<UInputAction> TitleStartAction;

	UPROPERTY(Transient)
	TObjectPtr<UPragmataTitleScreenWidget> TitleWidget = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> AppliedTitleInputMappingContext = nullptr;

	FTimerHandle TravelTimerHandle;
	bool bTransitionStarted = false;
};
