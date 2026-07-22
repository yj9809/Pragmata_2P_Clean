// Fill out your copyright notice in the Description page of Project Settings.


#include "Enemy/ExecutorMonster.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInterface.h"
#include "Puzzle/EnemyHackingPuzzleComponent.h"

namespace
{
	constexpr int32 ExecutorHackMaterialSlots[] = {8, 9, 16};

	const TCHAR* ExecutorHackedMaterialPaths[] =
	{
		TEXT("/Game/Enemy/Executor/Materials/ch6200_00_BlueEmissive_01_NoStamp_Hacked.ch6200_00_BlueEmissive_01_NoStamp_Hacked"),
		TEXT("/Game/Enemy/Executor/Materials/ch6200_00_Inside_01_NoStamp_Hacked.ch6200_00_Inside_01_NoStamp_Hacked"),
		TEXT("/Game/Enemy/Executor/Materials/ch6200_00_Inside_02_NoStamp_Hacked.ch6200_00_Inside_02_NoStamp_Hacked")
	};
}

AExecutorMonster::AExecutorMonster()
{
	MonsterId = TEXT("Executor");
	ScanDisplayText = NSLOCTEXT("Pragmata", "ExecutorScanDisplayText", "Executor");
	if (EnemyHackingPuzzleComponent)
	{
		EnemyHackingPuzzleComponent->EnemyType = EEnemyHackType::Executor;
	}

	// 매시 초기화
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> MeshRef(
		TEXT("/Game/Enemy/Executor/SM_Executor.SM_Executor")
	);

	if (MeshRef.Succeeded())
	{
		GetMesh()->SetSkeletalMesh(MeshRef.Object);
	}

	// Mesh Transform Setting
	GetMesh()->SetRelativeLocationAndRotation(FVector(0.f,0.f,-88.f),FRotator(0.f, -90.f, 0.f));

	// ABP 초기화
	static ConstructorHelpers::FClassFinder<UAnimInstance> AnimBPRef(
		TEXT("/Game/Blueprints/Enemy/Executor/ABP_ExecutorMonster.ABP_ExecutorMonster_C")
	);

	if (AnimBPRef.Succeeded())
	{
		GetMesh()->SetAnimInstanceClass(AnimBPRef.Class);
	}
}

void AExecutorMonster::BeginPlay()
{
	Super::BeginPlay();

	// 복제 초기값이나 server 즉시 상태를 현재 material에 반영
	ApplyHackMaterialsForCurrentState();
}

void AExecutorMonster::HandleHackStateChanged()
{
	Super::HandleHackStateChanged();

	// bHacked 복제 경로와 server 적용 경로가 같은 material 전환을 사용
	ApplyHackMaterialsForCurrentState();
}

void AExecutorMonster::CacheNormalHackMaterialsIfNeeded()
{
	if (bCachedNormalHackMaterials)
	{
		return;
	}

	USkeletalMeshComponent* MeshComponent = GetMesh();
	if (!MeshComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AExecutorMonster::CacheNormalHackMaterialsIfNeeded] MeshComponent is nullptr"));
		return;
	}

	NormalHackSlotMaterials.SetNum(UE_ARRAY_COUNT(ExecutorHackMaterialSlots));
	for (int32 MaterialIndex = 0; MaterialIndex < UE_ARRAY_COUNT(ExecutorHackMaterialSlots); ++MaterialIndex)
	{
		const int32 SlotIndex = ExecutorHackMaterialSlots[MaterialIndex];
		if (SlotIndex >= MeshComponent->GetNumMaterials())
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("[AExecutorMonster::CacheNormalHackMaterialsIfNeeded] Missing material slot %d"),
				SlotIndex);
			continue;
		}

		NormalHackSlotMaterials[MaterialIndex] = MeshComponent->GetMaterial(SlotIndex);
	}

	bCachedNormalHackMaterials = true;
}

void AExecutorMonster::ApplyHackMaterialsForCurrentState()
{
	CacheNormalHackMaterialsIfNeeded();

	USkeletalMeshComponent* MeshComponent = GetMesh();
	if (!MeshComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AExecutorMonster::ApplyHackMaterialsForCurrentState] MeshComponent is nullptr"));
		return;
	}

	static TSoftObjectPtr<UMaterialInterface> HackedMaterials[] =
	{
		TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(ExecutorHackedMaterialPaths[0])),
		TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(ExecutorHackedMaterialPaths[1])),
		TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(ExecutorHackedMaterialPaths[2]))
	};

	for (int32 MaterialIndex = 0; MaterialIndex < UE_ARRAY_COUNT(ExecutorHackMaterialSlots); ++MaterialIndex)
	{
		const int32 SlotIndex = ExecutorHackMaterialSlots[MaterialIndex];
		if (SlotIndex >= MeshComponent->GetNumMaterials())
		{
			continue;
		}

		if (!IsHacked())
		{
			UMaterialInterface* NormalMaterial = NormalHackSlotMaterials.IsValidIndex(MaterialIndex)
				? NormalHackSlotMaterials[MaterialIndex].Get()
				: nullptr;
			MeshComponent->SetMaterial(SlotIndex, NormalMaterial);
			continue;
		}

		UMaterialInterface* HackedMaterial = HackedMaterials[MaterialIndex].LoadSynchronous();
		if (!HackedMaterial)
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("[AExecutorMonster::ApplyHackMaterialsForCurrentState] Missing hacked material %s"),
				ExecutorHackedMaterialPaths[MaterialIndex]);
			continue;
		}

		MeshComponent->SetMaterial(SlotIndex, HackedMaterial);
	}

	// hack material 갱신이 정화 후 숨겨야 할 slot을 덮어쓰지 않게 재적용
	ApplyTaintMaterialVisibility();
}
