#pragma once

#include "CoreMinimal.h"
#include "Enemy/MonsterBase.h"
#include "WalkerMonster.generated.h"

class UMaterialInterface;
class UAnimMontage;

UCLASS()
class PRAGMATA_2P_API AWalkerMonster : public AMonsterBase
{
	GENERATED_BODY()

public:
	AWalkerMonster();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void HandleHackStateChanged() override;

private:
	void CacheNormalHackMaterialsIfNeeded();
	void ApplyHackMaterialsForCurrentState();
	void ApplyAttackWeaponMaterial(bool bVisible);
	void ResetAttackWeaponVisibility();

	// AttackPatterns에 등록된 local montage 시작에서 weapon을 표시
	UFUNCTION()
	void HandleLocalMontageStarted(UAnimMontage* Montage);

	// 공격 montage 종료와 중단에서 weapon을 숨김
	UFUNCTION()
	void HandleLocalMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInterface>> NormalHackSlotMaterials;

	TArray<int32> ResolvedHackMaterialSlots;

	bool bCachedNormalHackMaterials = false;

	TMap<TObjectKey<UAnimMontage>, int32> ActiveAttackWeaponMontages;

	int32 ResolvedAttackWeaponMaterialSlot = INDEX_NONE;
	bool bResolvedAttackWeaponMaterialSlot = false;
};
