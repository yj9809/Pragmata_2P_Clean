// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Data/PragmataTypes.h"
#include "GameFramework/Actor.h"
#include "WeaponBase.generated.h"

class UTutorialStepData;
class AHughCharacter;
class UNiagaraSystem;
class AController;
class UAnimMontage;
class UCameraShakeBase;
class USkeletalMeshComponent;
class UWeaponDataAsset;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAmmoDepletion);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FWeaponAmmoChangedSignature, int32, CurAmmo, int32, MaxAmmo);

UCLASS()
class PRAGMATA_2P_API AWeaponBase : public AActor
{
	GENERATED_BODY()

public:
	// 기본 weapon actor 생성
	AWeaponBase();
	// weapon 초기화 hook
	virtual void BeginPlay() override;
	// weapon tick hook
	virtual void Tick(float DeltaTime) override;
	// weapon ammo 복제값 등록
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	// 서버 발사 확정에 필요한 공통 ammo/cooldown gate 통과 시 true 반환
	virtual bool PullTrigger();
	// hit actor에게 weapon damage 전달
	virtual bool DealDamageToHitActor(const FHitResult& Hit, AController* InstigatorController);
public:
	// 발사 cooldown timer 시작
	void StartFireCooldown();
	// 사격 cooldown 해제
	void ResetShootingCooldown();
	float GetRecoilData(float DeltaSeconds);
	// server 승인 사격의 owner camera recoil target만 로컬 적용
	void ApplyLocalFireFeedback();
	void StoreWeapon(AHughCharacter* HughCharacter, const FName& SocketName);
	void DropWeapon();
	
	
public:
	//reload animation delegate
	UPROPERTY(BlueprintAssignable, Category="Weapon Data")
	FOnAmmoDepletion OnAmmoDepletion;
public:
	// 현재 cooldown 여부 조회
	FORCEINLINE bool GetIsCoolingDown() const {return IsCoolingDown;}
	FORCEINLINE int GetCurAmmo() const {return CurAmmunition;}
	FORCEINLINE int GetMaxAmmo() const {return MaxAmmunition;}
	FORCEINLINE int GetIncreaseAmount() const {return IncreaseAmount;}
	FORCEINLINE EWeaponType GetWeaponType() const {return WeaponType;}
	FORCEINLINE UWeaponDataAsset* GetWeaponData() const { return WeaponData.Get(); }
	FORCEINLINE UTutorialStepData* GetTutorialStepData() const { return WeaponTutorialData; }
	// server pickup 보급량을 max ammo 범위에서 반영
	bool AddAmmo(int32 AmmoAmount);
	// checkpoint/respawn 보급은 max ammo와 HUD delegate를 같은 경계에서 갱신
	void RefillAmmoToMax();
	// HUD가 서버 확정 ammo snapshot을 구독하는 delegate
	UPROPERTY(BlueprintAssignable, Category="Weapon Data")
	FWeaponAmmoChangedSignature OnAmmoChange;
protected://Componenets
	// weapon mesh root
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Weapon", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USkeletalMeshComponent> Mesh = nullptr;
protected://무기 공용 변수들
	// 사격 cooldown 중 여부
	UPROPERTY(Replicated)
	bool IsCoolingDown = false;

	// 발사 간격
	UPROPERTY(EditAnywhere, Category="Weapon Data")
	float CoolDown = 1.f;

	// 현재 탄약 수
	UPROPERTY(VisibleAnywhere, ReplicatedUsing=OnRep_AmmoSnapshot, Category="Weapon Data")
	int CurAmmunition = 0;

	// 최대 탄약 수
	UPROPERTY(EditAnywhere, ReplicatedUsing=OnRep_AmmoSnapshot, Category="Weapon Data")
	int MaxAmmunition = 0;
	
	//탄약 증가개수
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Weapon Data")
	int IncreaseAmount = 2;

	// 기본 weapon damage
	UPROPERTY(EditAnywhere, Category="Weapon Data")
	float BaseDamage = 0.f;

	// 사격 trace 최대 거리
	UPROPERTY(EditAnywhere, Category="Weapon Data")
	float MaxRange = 5000.f;
protected://recoil: 무기마다 값을 다르게 설정
	UPROPERTY()
	float CurrentRecoilPitch = 0.f;
	UPROPERTY()
	float TargetRecoilPitch = 0.f;
	UPROPERTY()
	float TargetRecoilPitchAmount = 0.f;
	UPROPERTY()
	float LastRecoilPitch = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil")
	float CameraUpSpeed = 30.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recoil")
	float CameraDownSpeed = 10.f;
protected:

	// cooldown timer handle
	FTimerHandle CooldownTimerHandle;

	// 위로 튀는 반동의 세기 (도)
	// UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Stat|Recoil")
	// float UpwardRecoil = 1.5f;
	// // 좌우로 흔들리는 반동의 범위 (도)
	// UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Stat|Recoil")
	// float SideRecoilRange = 0.5f;
protected:
	// 재장전 montage
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animation")
	TObjectPtr<UAnimMontage> ReloadWeaponAnimation;
protected:
	// weapon data asset
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon Data")
	TObjectPtr<UWeaponDataAsset> WeaponData = nullptr;

	// weapon slot type
	UPROPERTY(EditAnywhere, Category="Weapon Data")
	EWeaponType WeaponType = EWeaponType::Primary;

	// impact VFX
	UPROPERTY(EditAnywhere, Category="Weapon Data")
	UNiagaraSystem* ImpactEffect;
protected:
	// 에디터에서 선택할 카메라 셰이크 에셋
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Stat|Effects")
	TSubclassOf<UCameraShakeBase> CameraShakeClass;

	// 복제된 ammo snapshot을 HUD delegate로 전달
	UFUNCTION()
	void OnRep_AmmoSnapshot();
	
	// 서버/클라 동일한 경로로 ammo 변경 알림 전달
	void BroadcastAmmoChanged();

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayFireSound();
	
protected:
	UFUNCTION()
	void OnSphereOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	UFUNCTION()
	void OnSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);
protected:
	TObjectPtr<class USphereComponent> InteractionSphere;
	
protected://SFX
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	USoundBase* GunshotSFX;
    
	UPROPERTY(EditAnywhere,  BlueprintReadWrite, Category = "Audio")
	USoundAttenuation* GunshotAttenuation;
	
protected:
	UPROPERTY(EditDefaultsOnly, Category="Pragmata|Tutorial")
	TObjectPtr<UTutorialStepData> WeaponTutorialData;
};
