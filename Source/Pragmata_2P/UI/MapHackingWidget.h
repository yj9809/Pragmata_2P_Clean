// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MapHackingWidget.generated.h"

class UTextBlock;
class UImage;
class UWidget;
class AActor;

UCLASS()
class PRAGMATA_2P_API UMapHackingWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Hugh HUD에서 요약 표시 모드로 전환할 때 호출
	UFUNCTION(BlueprintImplementableEvent, Category = "Pragmata|HackUI")
	void SetCompactMode(bool bCompact);

	UFUNCTION(BlueprintCallable, Category = "Pragmata|HackUI")
	void SetMapHackingVisible(bool bVisible);

	UFUNCTION(BlueprintCallable, Category = "Pragmata|HackUI")
	void SetMapPuzzleActor(AActor* PuzzleActor);

protected:
	virtual void NativeConstruct() override;

private:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UWidget> Root;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UWidget> MapHackingRoot;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UImage> DirectionIcon;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> DepthText;
};
