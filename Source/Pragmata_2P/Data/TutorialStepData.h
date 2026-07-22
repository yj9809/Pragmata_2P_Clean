#pragma once

#include "CoreMinimal.h"
#include "Data/PragmataTypes.h"
#include "Engine/DataAsset.h"
#include "TutorialStepData.generated.h"

class UTexture2D;

UCLASS(BlueprintType)
class PRAGMATA_2P_API UTutorialStepData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	UFUNCTION(BlueprintPure, Category="Pragmata|Tutorial")
	FText GetStepTitle() const { return StepTitle; }

	UFUNCTION(BlueprintPure, Category="Pragmata|Tutorial")
	TSoftObjectPtr<UTexture2D> GetPreviewImage() const { return PreviewImage; }

	UFUNCTION(BlueprintPure, Category="Pragmata|Tutorial")
	FText GetDescriptionText() const { return DescriptionText; }

	UFUNCTION(BlueprintPure, Category="Pragmata|Tutorial")
	const TArray<FTutorialKeyBinding>& GetKeyBindings() const { return KeyBindings; }

	UFUNCTION(BlueprintPure, Category="Pragmata|Tutorial")
	EPragmataPlayerRole GetTargetRole() const { return TargetRole; }

	UFUNCTION(BlueprintPure, Category="Pragmata|Tutorial")
	float GetDisplayDurationSeconds() const { return DisplayDurationSeconds; }

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pragmata|Tutorial", meta=(AllowPrivateAccess="true"))
	FText StepTitle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pragmata|Tutorial", meta=(AllowPrivateAccess="true"))
	TSoftObjectPtr<UTexture2D> PreviewImage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pragmata|Tutorial", meta=(AllowPrivateAccess="true", MultiLine="true"))
	FText DescriptionText;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pragmata|Tutorial", meta=(AllowPrivateAccess="true"))
	TArray<FTutorialKeyBinding> KeyBindings;

	// 이 스텝을 보여줄 대상. None이면 Hugh/Diana 둘 다에게 표시
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pragmata|Tutorial", meta=(AllowPrivateAccess="true"))
	EPragmataPlayerRole TargetRole = EPragmataPlayerRole::None;

	// 모든 성공 표시가 같은 안전한 local serial 기준으로 닫히도록 DataAsset이 표시 시간을 소유
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pragmata|Tutorial", meta=(AllowPrivateAccess="true", ClampMin="0.1", Units="s"))
	float DisplayDurationSeconds = 5.f;
};
