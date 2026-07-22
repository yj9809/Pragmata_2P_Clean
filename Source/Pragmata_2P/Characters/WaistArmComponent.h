// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Data/PragmataTypes.h"
#include "GameFramework/SpringArmComponent.h"
#include "WaistArmComponent.generated.h"


enum class EWaistArmState : uint8;

UCLASS(BlueprintType, Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class PRAGMATA_2P_API UWaistArmComponent : public USceneComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UWaistArmComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
public:
	UFUNCTION(BlueprintCallable, Category="Pragmata|Animation")
	void SetArmState(EWaistArmState State);
	
	UFUNCTION(BlueprintCallable, Category="Pragmata|Animation")
	float PlayArm_AimEndMontage(float InPlayRate = 1.f) const;

	UFUNCTION(BlueprintPure, Category="Pragmata|Mesh")
	USkeletalMeshComponent* GetWaistArmMesh() const { return WaistArmMesh; }

	void EnsureWaistArmMeshAttachment();
	
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Pragmata|Components")
	TObjectPtr<USkeletalMeshComponent> WaistArmMesh;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Pragmata|State")
	EWaistArmState CurrentState = EWaistArmState::Idle;
	
	UPROPERTY(EditDefaultsOnly, Category="Pragmata|Settings")
	FName WaistArmAttachSocketName = TEXT("HipCarrySocket");
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Pragmata|Animation")
	TObjectPtr<UAnimMontage> AimEndMontage;
};
