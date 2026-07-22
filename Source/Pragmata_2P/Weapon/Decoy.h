// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/SphereComponent.h"
#include "GameFramework/Actor.h"
#include "Decoy.generated.h"

class ADecoyProjectile;
class UPragmataFactionComponent;
class AMonsterBase;
class USphereComponent;

UCLASS()
class PRAGMATA_2P_API ADecoy : public AActor
{
	GENERATED_BODY()
public:	
	ADecoy();
	
public:
	FORCEINLINE void SetDecoyProjectile(AActor* Projectile) { DecoyProjectile = Projectile; };
protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
protected:
	void OnTimerEnd(AMonsterBase* Monster);
	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, 
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat")
	TObjectPtr<USphereComponent> SphereComp;//범위를 나타낼 구체
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Pragmata|Character")
	TObjectPtr<UPragmataFactionComponent> FactionComponent = nullptr;
	// UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Pragmata|Character")
	// TObjectPtr<AActor> HughCharacter = nullptr;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Pragmata|Character")
	TObjectPtr<AActor> DecoyProjectile;
protected:
	FTimerHandle ActiveTimerHandle;
};
