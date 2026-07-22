#include "Enemy/WalkerMonster.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Materials/MaterialInterface.h"

namespace
{
	struct FWalkerHackMaterialSlot
	{
		const TCHAR* SlotName;
		const TCHAR* HackedMaterialPath;
	};

	const FWalkerHackMaterialSlot WalkerHackMaterialSlots[] =
	{
		{
			TEXT("ch5100_00_Inside"),
			TEXT("/Game/Enemy/Walker/Materials/M_Walker_Inside_Hacked_Emissive.M_Walker_Inside_Hacked_Emissive")
		},
		{
			TEXT("ch5100_90_Parts_Parts_Mat"),
			TEXT("/Game/Enemy/Walker/Materials/M_Walker_Parts_Parts_Mat_Hacked_Emissive.M_Walker_Parts_Parts_Mat_Hacked_Emissive")
		},
		{
			TEXT("ch5100_90_Frame_Body"),
			TEXT("/Game/Enemy/Walker/Materials/M_Walker_Frame_Body_Hacked_Emissive.M_Walker_Frame_Body_Hacked_Emissive")
		},
		{
			TEXT("ch5100_90_Frame_Head"),
			TEXT("/Game/Enemy/Walker/Materials/M_Walker_Frame_Head_Hacked_Emissive.M_Walker_Frame_Head_Hacked_Emissive")
		}
	};

	const FName WalkerAttackWeaponMaterialSlotName = TEXT("ch5100_00_Weapon_NoStamp");
	const TCHAR* WalkerAttackWeaponMaterialPath =
		TEXT("/Game/Enemy/Walker/Materials/ch5100_00_Weapon_NoStamp.ch5100_00_Weapon_NoStamp");
	const TCHAR* WalkerInvisibleMaterialPath = TEXT("/Game/VFX/M_Invisible.M_Invisible");
}

AWalkerMonster::AWalkerMonster()
{
	MonsterId = TEXT("Walker");
	ScanDisplayText = NSLOCTEXT("Pragmata", "WalkerScanDisplayText", "Walker");
}

void AWalkerMonster::BeginPlay()
{
	Super::BeginPlay();

	// 복제 초기값이나 server 즉시 상태를 현재 material에 반영
	ApplyHackMaterialsForCurrentState();

	// 비공격 상태는 모든 peer에서 weapon을 숨긴 상태로 시작
	ResetAttackWeaponVisibility();
	if (UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
	{
		AnimInstance->OnMontageStarted.AddUniqueDynamic(this, &ThisClass::HandleLocalMontageStarted);
		AnimInstance->OnMontageEnded.AddUniqueDynamic(this, &ThisClass::HandleLocalMontageEnded);
	}
}

void AWalkerMonster::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// actor 종료 시 local montage callback과 weapon override 정리
	if (UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
	{
		AnimInstance->OnMontageStarted.RemoveDynamic(this, &ThisClass::HandleLocalMontageStarted);
		AnimInstance->OnMontageEnded.RemoveDynamic(this, &ThisClass::HandleLocalMontageEnded);
	}
	ResetAttackWeaponVisibility();

	Super::EndPlay(EndPlayReason);
}

void AWalkerMonster::HandleHackStateChanged()
{
	Super::HandleHackStateChanged();

	// bHacked 복제 경로와 server 적용 경로가 같은 material 전환을 사용
	ApplyHackMaterialsForCurrentState();
}

void AWalkerMonster::HandleLocalMontageStarted(UAnimMontage* Montage)
{
	if (!Montage)
	{
		return;
	}

	const bool bIsAttackMontage = AttackPatterns.ContainsByPredicate(
		[Montage](const FMonsterAttackPattern& Pattern)
		{
			return Pattern.AttackMontage.Get() == Montage;
		});
	if (!bIsAttackMontage)
	{
		return;
	}

	// 공격 montage의 wind-up부터 recovery까지 weapon을 유지
	const bool bWasInactive = ActiveAttackWeaponMontages.IsEmpty();
	int32& ActiveMontageCount = ActiveAttackWeaponMontages.FindOrAdd(TObjectKey<UAnimMontage>(Montage));
	++ActiveMontageCount;
	if (bWasInactive)
	{
		ApplyAttackWeaponMaterial(true);
	}
}

void AWalkerMonster::HandleLocalMontageEnded(UAnimMontage* Montage, bool /*bInterrupted*/)
{
	if (!Montage)
	{
		return;
	}

	int32* ActiveMontageCount = ActiveAttackWeaponMontages.Find(TObjectKey<UAnimMontage>(Montage));
	if (!ActiveMontageCount)
	{
		return;
	}

	--(*ActiveMontageCount);
	if (*ActiveMontageCount <= 0)
	{
		ActiveAttackWeaponMontages.Remove(TObjectKey<UAnimMontage>(Montage));
	}

	// blend-out 중인 이전 instance가 새 공격 weapon을 조기 숨기지 않게 마지막 종료만 처리
	if (ActiveAttackWeaponMontages.IsEmpty())
	{
		ApplyAttackWeaponMaterial(false);
	}
}

void AWalkerMonster::ResetAttackWeaponVisibility()
{
	ActiveAttackWeaponMontages.Reset();
	ApplyAttackWeaponMaterial(false);
}

void AWalkerMonster::ApplyAttackWeaponMaterial(bool bVisible)
{
	USkeletalMeshComponent* MeshComponent = GetMesh();
	if (!MeshComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AWalkerMonster::ApplyAttackWeaponMaterial] MeshComponent is nullptr"));
		return;
	}

	if (!bResolvedAttackWeaponMaterialSlot)
	{
		ResolvedAttackWeaponMaterialSlot = MeshComponent->GetMaterialIndex(WalkerAttackWeaponMaterialSlotName);
		bResolvedAttackWeaponMaterialSlot = true;
	}

	if (ResolvedAttackWeaponMaterialSlot == INDEX_NONE
		|| ResolvedAttackWeaponMaterialSlot >= MeshComponent->GetNumMaterials())
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[AWalkerMonster::ApplyAttackWeaponMaterial] Missing material slot %s"),
			*WalkerAttackWeaponMaterialSlotName.ToString());
		return;
	}

	TSoftObjectPtr<UMaterialInterface> VisibleWeaponMaterial{
		FSoftObjectPath(WalkerAttackWeaponMaterialPath)};
	TSoftObjectPtr<UMaterialInterface> InvisibleWeaponMaterial{
		FSoftObjectPath(WalkerInvisibleMaterialPath)};
	UMaterialInterface* Material = bVisible
		? VisibleWeaponMaterial.LoadSynchronous()
		: InvisibleWeaponMaterial.LoadSynchronous();
	if (!Material)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[AWalkerMonster::ApplyAttackWeaponMaterial] Missing material %s"),
			bVisible ? WalkerAttackWeaponMaterialPath : WalkerInvisibleMaterialPath);
		return;
	}

	MeshComponent->SetMaterial(ResolvedAttackWeaponMaterialSlot, Material);
}

void AWalkerMonster::CacheNormalHackMaterialsIfNeeded()
{
	if (bCachedNormalHackMaterials)
	{
		return;
	}

	USkeletalMeshComponent* MeshComponent = GetMesh();
	if (!MeshComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AWalkerMonster::CacheNormalHackMaterialsIfNeeded] MeshComponent is nullptr"));
		return;
	}

	ResolvedHackMaterialSlots.SetNum(UE_ARRAY_COUNT(WalkerHackMaterialSlots));
	NormalHackSlotMaterials.SetNum(UE_ARRAY_COUNT(WalkerHackMaterialSlots));
	for (int32 MaterialIndex = 0; MaterialIndex < UE_ARRAY_COUNT(WalkerHackMaterialSlots); ++MaterialIndex)
	{
		// SM_Walker_Tainted는 DF slot이 중간에 삽입되어 있으므로 slot name으로 실제 index를 찾는다.
		const int32 SlotIndex = MeshComponent->GetMaterialIndex(FName(WalkerHackMaterialSlots[MaterialIndex].SlotName));
		ResolvedHackMaterialSlots[MaterialIndex] = SlotIndex;
		if (SlotIndex == INDEX_NONE || SlotIndex >= MeshComponent->GetNumMaterials())
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("[AWalkerMonster::CacheNormalHackMaterialsIfNeeded] Missing material slot %s"),
				WalkerHackMaterialSlots[MaterialIndex].SlotName);
			continue;
		}

		NormalHackSlotMaterials[MaterialIndex] = MeshComponent->GetMaterial(SlotIndex);
	}

	bCachedNormalHackMaterials = true;
}

void AWalkerMonster::ApplyHackMaterialsForCurrentState()
{
	CacheNormalHackMaterialsIfNeeded();

	USkeletalMeshComponent* MeshComponent = GetMesh();
	if (!MeshComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AWalkerMonster::ApplyHackMaterialsForCurrentState] MeshComponent is nullptr"));
		return;
	}

	static TSoftObjectPtr<UMaterialInterface> HackedMaterials[] =
	{
		TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(WalkerHackMaterialSlots[0].HackedMaterialPath)),
		TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(WalkerHackMaterialSlots[1].HackedMaterialPath)),
		TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(WalkerHackMaterialSlots[2].HackedMaterialPath)),
		TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(WalkerHackMaterialSlots[3].HackedMaterialPath))
	};

	for (int32 MaterialIndex = 0; MaterialIndex < UE_ARRAY_COUNT(WalkerHackMaterialSlots); ++MaterialIndex)
	{
		const int32 SlotIndex = ResolvedHackMaterialSlots.IsValidIndex(MaterialIndex)
			? ResolvedHackMaterialSlots[MaterialIndex]
			: INDEX_NONE;
		if (SlotIndex == INDEX_NONE || SlotIndex >= MeshComponent->GetNumMaterials())
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
				TEXT("[AWalkerMonster::ApplyHackMaterialsForCurrentState] Missing hacked material %s"),
				WalkerHackMaterialSlots[MaterialIndex].HackedMaterialPath);
			continue;
		}

		MeshComponent->SetMaterial(SlotIndex, HackedMaterial);
	}

	// hack material 갱신이 정화 후 숨겨야 할 slot을 덮어쓰지 않게 재적용
	ApplyTaintMaterialVisibility();
}
