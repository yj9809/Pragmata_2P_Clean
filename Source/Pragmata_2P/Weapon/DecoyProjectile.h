// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DecoyProjectile.generated.h"

class ADecoy;
class USphereComponent;
class UProjectileMovementComponent;

UCLASS()
class PRAGMATA_2P_API ADecoyProjectile : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ADecoyProjectile();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
public:
	void InitVelocity(const FVector& LaunchVelocity);
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement")
	UProjectileMovementComponent* ProjectileComp;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Collision")
	USphereComponent* SphereComp;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Decoy")
	TSubclassOf<ADecoy> DecoyClass;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Decoy")
	TObjectPtr<USkeletalMeshComponent> Mesh;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	class UAnimSequence* LoopAnimation;

	UPROPERTY(ReplicatedUsing=OnRep_HasLanded)
	bool bHasLanded = false;
protected:
	UFUNCTION()
	void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	UFUNCTION()
	void OnRep_HasLanded();

	void ApplyLandedState();
	
};
