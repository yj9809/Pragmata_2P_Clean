// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LightBase.generated.h"

UCLASS()
class PRAGMATA_2P_API ALightBase : public AActor
{
	GENERATED_BODY()

public:
	ALightBase();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// �������Ʈ���� ������Ʈ���� ������ �� ������
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class USceneComponent* RootSceneComponent;

	// �׻� �����ִ� �������� ���� (������ ����)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Power System")
	bool bIsAlwaysOn = false;

	// ��������Ʈ�� ���ε��� �ݹ� �Լ�
	UFUNCTION()
	void HandlePowerStateChanged(bool bIsPowerOn);

};
