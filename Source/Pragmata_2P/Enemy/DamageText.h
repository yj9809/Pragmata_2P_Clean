// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DamageText.generated.h"

UCLASS()
class PRAGMATA_2P_API ADamageText : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ADamageText();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
public:
	// Call this immediately after spawning the actor to set the number
	void Initialize(float DamageAmount);
protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI")
	class UWidgetComponent* WidgetComponent;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI")
	float FloatSpeed = 80.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI")
	float LifeSpan = 1.2f;
	UPROPERTY()
	float ElapsedTime = 0.f;
};
