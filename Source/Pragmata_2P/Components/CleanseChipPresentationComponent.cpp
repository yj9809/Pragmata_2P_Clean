#include "Components/CleanseChipPresentationComponent.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"

void UCleanseChipPresentationComponent::Configure(USkeletalMeshComponent* InDianaMesh, const TSoftObjectPtr<USkeletalMesh>& InChipMesh)
{
	DianaMesh = InDianaMesh;
	ChipMesh = InChipMesh;
}

// montage NotifyState가 시작한 구간에서만 chip을 Diana 엄지 끝에 표시
void UCleanseChipPresentationComponent::BeginChipHold()
{
	if (EnsureChipMeshComponent())
	{
		ChipMeshComponent->SetHiddenInGame(false);
	}
}

// NotifyState 종료와 cinematic abort가 같은 정리 경로를 사용
void UCleanseChipPresentationComponent::EndChipHold()
{
	if (ChipMeshComponent)
	{
		ChipMeshComponent->SetHiddenInGame(true);
	}
}

void UCleanseChipPresentationComponent::ResetPresentation()
{
	EndChipHold();
	if (ChipMeshComponent)
	{
		ChipMeshComponent->DestroyComponent();
		ChipMeshComponent = nullptr;
	}
}

bool UCleanseChipPresentationComponent::EnsureChipMeshComponent()
{
	USkeletalMeshComponent* TargetMesh = DianaMesh.Get();
	USkeletalMesh* LoadedChipMesh = ChipMesh.LoadSynchronous();
	if (!TargetMesh || !LoadedChipMesh)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CleanseChipPresentationComponent::EnsureChipMeshComponent] Diana mesh or chip mesh is missing"));
		return false;
	}

	if (!ChipMeshComponent)
	{
		ChipMeshComponent = NewObject<USkeletalMeshComponent>(GetOwner(), TEXT("CleanseChipPresentationMesh"));
		ChipMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ChipMeshComponent->SetGenerateOverlapEvents(false);
		ChipMeshComponent->SetCastShadow(false);
		ChipMeshComponent->SetSkeletalMesh(LoadedChipMesh);
		ChipMeshComponent->RegisterComponent();
		ChipMeshComponent->AttachToComponent(TargetMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, TEXT("R_Thumb3"));
	}

	return true;
}
