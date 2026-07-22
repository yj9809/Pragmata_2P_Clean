#pragma once

#include "CoreMinimal.h"
#include "WaistArmComponent.h"
#include "Characters/PlayableCharacter.h"
#include "Data/PairedAnimationSet.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "UObject/SoftObjectPtr.h"
#include "Weapon/WeaponBase.h"
#include "HughCharacter.generated.h"

class UWaistArmComponent;
class AWeaponBase;
class UInputAction;
class UPragmataHughMovementComponent;
struct FInputActionValue;
class UPairedAnimationComponent;
class UCameraComponent;
class USpotLightComponent;
class USkeletalMeshComponent;
class USpringArmComponent;
class UPragmataHUDWidget;
class UPragmataPlayerStatsComponent;
class AGripGun;
class ULobbyCharacterSkinSet;

// 스태미나(부스트 게이지) 변경 알림. (CurrentStamina, MaxStamina, bStaminaLocked=빨강 표시 여부)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FStaminaChangedSignature, float, CurrentStamina, float, MaxStamina, bool, bStaminaLocked);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCurrentWeaponChangedSignature, AWeaponBase*, CurrentWeapon);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FWeaponInventoryChangedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FHughHealAvailabilityChangedSignature, bool, bNewCanUseHeal, int32, RemainingHealCartridges);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FHughHealHoldChangedSignature, float, HoldProgress, bool, bHolding);

UCLASS()
class PRAGMATA_2P_API AHughCharacter : public APlayableCharacter
{
	GENERATED_BODY()

public:
	// Hugh movement component를 교체해 생성
	explicit AHughCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// BP component template attachment 보정
	virtual void PreRegisterAllComponents() override;

	// 기본 무기 spawn과 부착
	virtual void PostInitializeComponents() override;

	// Diana avatar와 runtime component 초기화
	virtual void BeginPlay() override;

	// movement, camera, stamina, animation 상태 갱신
	virtual void Tick(float DeltaSeconds) override;

	// Hugh animation/light 복제값 등록
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;



	// 피해 적용 후 Hugh hit react와 필요한 paired cue만 처리
	virtual bool ReceiveDamage_Implementation(const FPragmataDamageSpec& DamageSpec) override;

	// Hugh input action binding
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	// 실제 감소량을 MaxHealth 비율로 바꿔 Hugh 피격 강도를 결정
	EPragmataHitReactStrength ResolveHitReactStrength(float ActualDamage) const;

	// damage source 위치를 기준으로 Hugh 피격 방향을 결정
	EPragmataHitDirection ResolveHitReactDirection(const FPragmataDamageSpec& DamageSpec) const;

	// Hugh mesh에서 hit react montage와 방향 section을 직접 재생
	bool PlayHitReactMontage(EPragmataHitReactStrength Strength, EPragmataHitDirection Direction);

	// Diana를 같이 잠글 정도의 큰 피격 cue인지 확인
	bool ShouldSendPairedHitReactCue(EPragmataHitReactStrength Strength) const;
public:
	// 실제 2D 이동 속도 조회
	FORCEINLINE float GetMovementSpeed() const {return GetVelocity().Size2D();}

	// lean 값 조회
	FORCEINLINE float GetLeanAmount() const {return LeanAmount;}

	// 조준 이동 각도 조회
	FORCEINLINE float GetAngle() const {return Angle;}

	// 조준 상태 조회
	FORCEINLINE bool GetIsAiming() const {return HughAnimState.bIsAiming;}

	// 달리기 상태 조회
	FORCEINLINE bool GetIsRunning() const {return HughAnimState.bIsRunning;}

	// hover 상태 조회
	FORCEINLINE bool GetIsHovering() const {return HughAnimState.bIsHovering;}

	// 점프 입력 유지 상태 조회
	FORCEINLINE bool GetIsJumpInput() const {return HughAnimState.bIsJumpInput;}

	// 현재 weapon 조회
	FORCEINLINE AWeaponBase* GetCurrentWeapon() const {return CurrentWeapon;}

	// HUD가 weapon wheel 전체 slot을 채울 때 읽는 inventory snapshot
	FORCEINLINE const TArray<AWeaponBase*>& GetWeaponInventory() const { return WeaponInventory; }

	// Primary는 항상, 나머지는 weapon과 ammo가 있을 때만 선택 가능
	bool IsWeaponSlotSelectable(int32 TargetWeaponIndex) const;

	// GripGun reload presentation 시작 요청
	bool TryStartGripGunReload(AGripGun* RequestingGripGun);
	float GetGripGunReloadDuration() const;
	void ApplyGripGunReloadPresentation(bool bReloading);

	FVector GetAuthoritativeAimOrigin() const;
	FVector GetAuthoritativeAimDirection() const;

	// lobby 선택 skin을 gameplay mesh와 Diana avatar mesh에 반영
	void SetGameplaySkinIds(FName HughSkinId, FName DianaSkinId);

	// 이 role의 local gameplay HUD class 조회
	const TSoftClassPtr<UPragmataHUDWidget>& GetGameplayHUDClass() const { return GameplayHUDClass; }

	UFUNCTION(BlueprintCallable, Category="Pragmata|Objective")
	void ShowObjectiveAlert(float DisplaySeconds = 4.f);

	UFUNCTION(BlueprintCallable, Category="Pragmata|Objective")
	void ShowObjectiveAlertText(const FText& ObjectiveText, float DisplaySeconds = 4.f);

	// shared Hugh gameplay stat source 조회
	UFUNCTION(BlueprintPure, Category="Pragmata|Hugh")
	UPragmataPlayerStatsComponent* GetPlayerStatsComponent() const { return PlayerStatsComponent; }
	UPragmataHealthComponent* GetHealthComponent() const { return HealthComponent; }

	// checkpoint respawn timer가 끝난 뒤 death/action presentation을 정상 locomotion 상태로 되돌린다.
	void ResetForRespawn();

	// checkpoint interaction과 respawn이 같은 shared resource 보급 경계를 사용
	UFUNCTION(BlueprintCallable, Category="Pragmata|Hugh")
	void ReplenishCheckpointResources();

	UFUNCTION(BlueprintPure, Category="Pragmata|Hugh|Heal")
	int32 GetRemainingHealCartridges() const;

	UFUNCTION(BlueprintPure, Category="Pragmata|Hugh|Heal")
	bool CanUseHeal() const;

	UFUNCTION(BlueprintPure, Category="Pragmata|Hugh|Heal")
	float GetHealHoldProgress() const;

	// 상체 action 상태 조회
	FORCEINLINE EActionState GetActionState() const {return ActionState;}

	// 이동 상태 조회
	FORCEINLINE EMovementState GetMovementState() const {return MovementState;}
public:
	// dash 상태 직접 지정
	void SetIsDashing(bool bNewDashing);

	// 현재 stamina 조회
	FORCEINLINE float GetCurrentStamina() const {return CurStamina;}

	// 최대 stamina 조회
	FORCEINLINE float GetMaxStamina() const {return MaxStamina;}

	// stamina 소모 가능 여부 조회
	FORCEINLINE bool GetIsStaminaConsumable() const {return IsStaminaConsumable;}
public:
	FORCEINLINE void SetOverlappingWeapon(AWeaponBase* Weapon) { OverlappingWeapon = Weapon;}

	FORCEINLINE FName GetRightHandSocketName() const {return RightHandSocketName;}
	
	FORCEINLINE void SetIsWeaponTriggerBlocked(bool flag){ IsWeaponTriggerBlocked = flag; }
public:
	void AttachWeaponToSocket(FName SocketName);

public:
	// HUD ViewModel이 구독하는 스태미나 변경 델리게이트.
	UPROPERTY(BlueprintAssignable, Category="Pragmata|Stamina")
	FStaminaChangedSignature OnStaminaChanged;

	// HUD weapon wheel이 현재 무기 ammo source를 다시 바인딩하는 delegate
	UPROPERTY(BlueprintAssignable, Category="Pragmata|Weapon")
	FCurrentWeaponChangedSignature OnCurrentWeaponChanged;

	// HUD weapon wheel이 전체 slot 보유 상태를 다시 읽는 delegate
	UPROPERTY(BlueprintAssignable, Category="Pragmata|Weapon")
	FWeaponInventoryChangedSignature OnWeaponInventoryChanged;

	UPROPERTY(BlueprintAssignable, Category="Pragmata|Hugh|Heal")
	FHughHealAvailabilityChangedSignature OnHealAvailabilityChanged;

	UPROPERTY(BlueprintAssignable, Category="Pragmata|Hugh|Heal")
	FHughHealHoldChangedSignature OnHealHoldChanged;

protected://input bind functions
	// 비어 있는 input action을 기본 asset로 복구
	void ResolveInputActions();

	// 이동 입력 시작 처리
	void OnStartMove(const FInputActionValue& Value);

	// 이동 입력 지속 처리
	void OnTriggerMove(const FInputActionValue& Value);

	// 이동 입력 종료 처리
	void OnCompleteMove(const FInputActionValue& Value);

	// 시점 입력 처리
	void OnTriggerLook(const FInputActionValue& Value);

	// 공격 입력 시작 처리
	void OnStartAttack(const FInputActionValue& Value);

	// dash 입력 시작 처리
	void OnStartDash(const FInputActionValue& Value);

	// dodge 입력 시작 처리
	void OnStartDodge(const FInputActionValue& Value);

	// jump hold 입력 처리
	void OnTriggerJump(const FInputActionValue& Value);

	// jump 입력 시작 처리
	void OnStartJump(const FInputActionValue& Value);

	// jump 입력 종료 처리
	void OnCompleteJump(const FInputActionValue& Value);

	// aim 입력 시작 처리
	void OnStartAim(const FInputActionValue& Value);

	// aim 입력 종료 처리
	void OnCompleteAim(const FInputActionValue& Value);

	// boost 입력 지속 처리
	void OnTriggerBoost(const FInputActionValue& Value);

	// boost 입력 종료 처리
	void OnCompleteBoost(const FInputActionValue& Value);

	// run 입력 지속 처리
	void OnTriggerRun(const FInputActionValue& Value);

	// run 입력 종료 처리
	void OnCompleteRun(const FInputActionValue& Value);

	// reload 입력 시작 처리
	void OnStartReload(const FInputActionValue& Value);

	// HeadVisor light toggle 입력 처리
	void OnToggleHeadVisorSpotlight(const FInputActionValue& Value);

	//change weapon
	void OnStartWeaponChange(const FInputActionValue& Value);

	// mouse wheel weapon cycle 입력 처리
	void OnCycleWeapon(const FInputActionValue& Value);

	//interact(pickup weapon)
	void OnStartInteract(const FInputActionValue& Value);

	// heal hold 시작
	void OnStartHeal(const FInputActionValue& Value);

	// heal hold 종료
	void OnCompleteHeal(const FInputActionValue& Value);

	// heal 가능 조건에 영향을 주는 HP 변화를 반영
	UFUNCTION()
	void OnHealSourceHealthChanged(float NewCurrentHealth, float NewMaxHealth);

	// heal 가능 조건에 영향을 주는 cartridge 변화를 반영
	UFUNCTION()
	void OnHealSourceCartridgeChanged(int32 NewRepairCartridge, int32 NewMaxRepairCartridge);
protected:
	// HeadVisor light socket attachment 보정
	void EnsureHeadVisorSpotlightAttachment();

	// HeadVisor light 표시 상태 적용
	void ApplyHeadVisorSpotlightEnabled(bool bNewEnabled);

	// HeadVisor light 서버/local 상태 설정
	void SetHeadVisorSpotlightEnabled(bool bNewEnabled);

	// movement component intent를 Hugh 상태로 동기화
	void SyncMovementIntentFromMovementComponent();

	// 이동 상태 계산
	void UpdateMovementState();

	// movement speed 반영
	void UpdateMovementSpeed();

	// camera recoil 처리
	void WeaponRecoil(float DeltaSeconds);

	// stamina 회복 처리
	void RecoverStamina(float DeltaSeconds);

	// stamina 소모 처리
	void ConsumeStamina(float Stamina, bool bRequestServerSpend = true);

	// stamina UI delegate 호출
	void BroadcastStaminaChanged();

	// heal 가능 상태 계산
	bool CalculateHealAvailability() const;

	// 서버 heal 가능 상태 갱신
	void UpdateHealAvailability();

	// HUD용 heal 가능 상태 전달
	void BroadcastHealAvailabilityChanged();

	// local hold progress와 반복 heal 요청 갱신
	void UpdateHealHold(float DeltaSeconds);

	// local hold UI 상태 정리
	void CancelHealHold();

	// 서버 heal 요청
	void RequestHealUse();

	// 회전 lean 계산
	void CalculateRotation(float DeltaSeconds);

	// 조준 이동 각도 계산
	void CalculateAngle();

	// 임시 debug message 출력
	void PrintDebugMsg();

	// 조준 상태 설정
	void SetAimingState(bool bNewAiming);

	// 달리기 상태 설정
	void SetRunningState(bool bNewRunning);

	// jump 입력 유지 상태 설정
	void SetJumpInputState(bool bNewJumpInput);

	// hover 상태 설정
	void SetHoveringState(bool bNewHovering);

	// replication용 animation state 갱신
	void RefreshReplicatedAnimState();

	// remote proxy animation state 적용
	void ApplyReplicatedAnimState(const FHughReplicatedAnimState& OldHughAnimState);

	//무기 장착(변경) 시 호출
	void EquipWeapon(AWeaponBase* Weapon);

	// weapon index 선택을 현재 장착 상태와 등 표시 상태에 반영
	bool ChangeWeaponByIndex(int32 TargetWeaponIndex, bool bPlayAimDrawMontage);

	// mouse wheel 방향 기준 다음 선택 가능한 weapon index 탐색
	int32 FindNextSelectableWeaponIndex(int32 Direction) const;

	// selected non-primary weapon만 등 소켓에 보이도록 갱신
	void RefreshStoredWeaponPresentation();

	// 최근 왼쪽 등 선택 weapon이 비었으면 남은 Attack/Tactical weapon으로 보정
	void NormalizeLastLeftBackWeaponType();

	// 특정 weapon actor를 type별 수납 socket에 부착
	void AttachWeaponToStoredSocket(AWeaponBase* Weapon);

	// 왼쪽 등 공유 socket을 쓰는 weapon type인지 확인
	bool IsLeftBackStoredWeaponType(EWeaponType WeaponType) const;

	// weapon type별 비조준 수납 socket 선택
	FName GetStoredWeaponSocketName(EWeaponType WeaponType) const;

	// weapon type별 조준 시작 몽타주 선택
	UAnimMontage* GetDrawStoredWeaponMontage(EWeaponType WeaponType) const;

	// weapon type별 조준 종료 수납 몽타주 선택
	UAnimMontage* GetStoreWeaponMontage(EWeaponType WeaponType) const;

public:
	//무기 버리기
	void DropWeapon();

	//무기 획득
	bool PickUpWeapon(AWeaponBase* PickupWeapon);
protected:
	// 착지 후 hover/dash 상태 정리
	virtual void Landed(const FHitResult& Hit) override;
public:

	// Diana avatar mesh 조회
	UFUNCTION(BlueprintPure, Category="Pragmata|Hugh")
	USkeletalMeshComponent* GetDianaAvatarMesh() const { return DianaAvatarMesh; }

protected:

	// WaistArm mesh 조회
	UFUNCTION(BlueprintPure, Category="Pragmata|Hugh")
	USkeletalMeshComponent* GetWaistArmMesh() const { return WaistArmComponent->GetWaistArmMesh(); }

	// paired animation component 조회
	UFUNCTION(BlueprintPure, Category="Pragmata|Hugh")
	UPairedAnimationComponent* GetPairedAnimationComponent() const { return PairedAnimationComponent; }
protected://delegates
	// reload montage 종료 처리
	UFUNCTION()
	void OnReloadMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted);

	// HeadVisor light 복제값 반영
	UFUNCTION()
	void OnRep_HeadVisorSpotlightEnabled();

	// 복제된 stamina snapshot을 HUD에 전달
	UFUNCTION()
	void OnRep_StaminaSnapshot();

	// 복제된 heal 가능 상태를 HUD에 전달
	UFUNCTION()
	void OnRep_CanUseHeal();

	// 서버가 spawn한 weapon pointer를 클라이언트 HUD/부착 상태에 연결
	UFUNCTION()
	void OnRep_CurrentWeapon();

	// 서버가 지급한 weapon inventory pointer를 클라이언트 표시 상태에 연결
	UFUNCTION()
	void OnRep_WeaponInventory();

	UFUNCTION()
	void OnRep_StoredWeaponPresentationState();
public:
	// Diana avatar를 Hugh mesh에 부착
	UFUNCTION(BlueprintCallable, Category="Pragmata|Hugh")
	void AttachDianaAvatar();

	// death/respawn lifecycle에서 Diana avatar mount socket을 같은 경계로 전환
	UFUNCTION(BlueprintCallable, Category="Pragmata|Hugh")
	void SetDianaAvatarDeathAttachment(bool bUseDeathSocket);

protected:

	// WaistArm component를 Hugh mesh에 부착
	UFUNCTION(BlueprintCallable, Category="Pragmata|Hugh")
	void AttachWaistArm();

	// 서버 복제 Hugh animation snapshot이 도착한 시점에 owner와 remote presentation 갱신
	UFUNCTION()
	void OnRep_HughAnimState(FHughReplicatedAnimState OldHughAnimState);

	// client 사격 요청을 서버에서 검증하고 weapon fire를 실행
	UFUNCTION(Server, Reliable)
	void Server_RequestFire();

	// server 승인 fire의 owner camera recoil만 재생
	UFUNCTION(Client, Unreliable)
	void Client_ConfirmFireFeedback();

	// client weapon slot 선택을 서버 current weapon으로 확정
	UFUNCTION(Server, Reliable)
	void Server_RequestWeaponChange(int32 TargetWeaponIndex);

	// client pickup candidate를 server overlap 상태로 재검증
	UFUNCTION(Server, Reliable)
	void Server_RequestPickup(AWeaponBase* PickupCandidate);

	// client dash intent를 server stamina와 movement state로 검증
	UFUNCTION(Server, Reliable)
	void Server_RequestDash();

	// owner에게 server 승인 dash montage를 재생
	UFUNCTION(Client, Reliable)
	void Client_PlayDashPresentation(uint8 DashMode, FName SectionName, int32 ActionSerial);

	// owner에게 server 승인 weapon change montage를 재생
	UFUNCTION(Client, Reliable)
	void Client_PlayWeaponChangePresentation(AWeaponBase* PreviousWeapon, AWeaponBase* TargetWeapon, int32 ActionSerial);

	// server 승인 뒤 owner에게 pickup montage만 재생
	UFUNCTION(Client, Reliable)
	void Client_PlayPickupPresentation();

	// visor light 상태는 서버에서 확정해 remote 화면에도 같은 조명을 표시
	UFUNCTION(Server, Reliable)
	void Server_SetHeadVisorSpotlightEnabled(bool bNewEnabled);

	// owner 예측 stamina 소모를 서버 snapshot으로 확정
	UFUNCTION(Server, Reliable)
	void Server_RequestConsumeStamina(float Stamina);

	// client heal intent를 서버에서 검증
	UFUNCTION(Server, Reliable)
	void Server_RequestHeal();

	// server 탄약 고갈을 replicated presentation serial로 전환
	UFUNCTION()
	void HandleAmmoDepletion();
	void PlayWeaponDepletionPresentation(EWeaponType WeaponType, int32 ActionSerial);

	UFUNCTION()
	void HandleDeath();

	UFUNCTION()
	void OnRep_GameplaySkinIds();

	void ApplyGameplaySkinIds();
	bool TryCommitServerDash();
	void PlayDashPresentation(uint8 DashMode, FName SectionName, int32 ActionSerial);
	void FinishServerDash();
	void ClearServerWeaponTriggerBlock();
	bool StartServerWeaponTriggerBlock(AWeaponBase* TargetWeapon);
	void CommitServerWeaponChangePresentation(AWeaponBase* PreviousWeapon, AWeaponBase* TargetWeapon);
	void PlayWeaponChangePresentation(AWeaponBase* PreviousWeapon, AWeaponBase* TargetWeapon, int32 ActionSerial);
	ULobbyCharacterSkinSet* LoadLobbySkinSet();
protected://components
	// third-person camera boom
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Pragmata|Camera", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USpringArmComponent> CameraBoom = nullptr;

	// local camera component
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Pragmata|Camera", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UCameraComponent> CameraComponent = nullptr;

	// Hugh 등에 표시되는 Diana avatar mesh
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Pragmata|Hugh", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USkeletalMeshComponent> DianaAvatarMesh = nullptr;

	// HeadVisor spotlight component
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Pragmata|Hugh|Light", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USpotLightComponent> HeadVisorSpotlight = nullptr;

	// Hugh/Diana paired animation cue component
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Pragmata|Hugh", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UPairedAnimationComponent> PairedAnimationComponent = nullptr;

	// Hugh 중심 shared player stat source
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Pragmata|Hugh", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UPragmataPlayerStatsComponent> PlayerStatsComponent = nullptr;

	// Hugh 전용 기본 HP. HealthComponent 전역 기본값은 enemy 영향 때문에 유지
	UPROPERTY(EditDefaultsOnly, Category="Pragmata|Hugh|Stats", meta=(ClampMin="0"))
	int32 DefaultMaxHP = 1500;

	// Diana avatar mount socket
	UPROPERTY(EditDefaultsOnly, Category="Pragmata|Hugh")
	FName DianaMountSocketName = TEXT("DianaMountSocket_root");

	// Hugh death presentation 동안 사용하는 Diana avatar mount socket
	UPROPERTY(EditDefaultsOnly, Category="Pragmata|Hugh")
	FName DianaDeathMountSocketName = TEXT("DianaMountSocket_root_death");

	// HeadVisor light socket
	UPROPERTY(EditDefaultsOnly, Category="Pragmata|Hugh|Light")
	FName HeadVisorLightSocketName = TEXT("HeadVisor_LightSocket");

	// 시작 시 HeadVisor light 켜짐 여부
	UPROPERTY(EditDefaultsOnly, Category="Pragmata|Hugh|Light")
	bool bHeadVisorSpotlightStartsEnabled = true;

	UPROPERTY(EditDefaultsOnly, Category="Pragmata|Lobby|Skin")
	TSoftObjectPtr<ULobbyCharacterSkinSet> LobbySkinSet;

	UPROPERTY(Transient)
	TObjectPtr<ULobbyCharacterSkinSet> LoadedLobbySkinSet = nullptr;

	UPROPERTY(ReplicatedUsing=OnRep_GameplaySkinIds)
	FName ReplicatedHughSkinId = NAME_None;

	UPROPERTY(ReplicatedUsing=OnRep_GameplaySkinIds)
	FName ReplicatedDianaSkinId = NAME_None;

	// spawn할 기본 weapon class
	UPROPERTY(EditDefaultsOnly, Category="Combat")
	TSubclassOf<class AWeaponBase> DefaultWeaponClass;

	// Attack 슬롯에 기본 지급할 ShotGun class
	UPROPERTY(EditDefaultsOnly, Category="Combat")
	TSubclassOf<class AWeaponBase> AttackWeaponClass;

	// Tactical 슬롯에 기본 지급할 StasisNet class
	UPROPERTY(EditDefaultsOnly, Category="Combat")
	TSubclassOf<class AWeaponBase> TacticalWeaponClass;

	// 현재 장착 weapon
	UPROPERTY(ReplicatedUsing=OnRep_CurrentWeapon, BlueprintReadOnly, Category="Combat")
	TObjectPtr<class AWeaponBase> CurrentWeapon = nullptr;

	// 현재 장착 weapon
	// 각 world의 overlap callback이 채우는 local candidate, gameplay 판정은 server 값만 사용
	UPROPERTY(Transient, BlueprintReadOnly, Category="Combat")
	TObjectPtr<class AWeaponBase> OverlappingWeapon = nullptr;

	// 오른손 weapon socket
	UPROPERTY(EditDefaultsOnly, Category="Combat")
	FName RightHandSocketName = TEXT("WeaponSocket");

	// back weapon socket (shotgun)
	UPROPERTY(EditDefaultsOnly, Category="Combat")
	FName LeftBackWeaponSocketName = TEXT("LeftWeaponSocket");

	// back weapon socket (defense)
	UPROPERTY(EditDefaultsOnly, Category="Combat")
	FName RightBackWeaponSocketName = TEXT("RightWeaponSocket");

	// UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Pragmata|Hugh", meta=(AllowPrivateAccess="true"))
	// TObjectPtr<USkeletalMeshComponent> WaistArmMesh = nullptr;

	// WaistArm attachment socket
	UPROPERTY(EditDefaultsOnly, Category="Pragmata|Hugh")
	FName WaistArmAttachSocketName = TEXT("HipCarrySocket");

	// WaistArm weapon attachment socket (gripgun)
	UPROPERTY(EditDefaultsOnly, Category="Pragmata|Hugh")
	FName WaistArmWeaponAttachSocketName = TEXT("WeaponArmSocket");

	// BP_HughCharacter defaults에서 UI 디자이너가 교체할 Hugh HUD widget
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Pragmata|UI", meta=(AllowPrivateAccess="true"))
	TSoftClassPtr<UPragmataHUDWidget> GameplayHUDClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pragmata|Objective", meta=(AllowPrivateAccess="true", MultiLine="true"))
	FText ObjectiveAlertText;

	//waistarm component
	// WaistArm mesh와 상태를 소유하는 component
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Pragmata|Hugh", meta = (AllowPrivateAccess="true"))
	TObjectPtr<UWaistArmComponent> WaistArmComponent;

protected://Animation Montages
	// jog start montage
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pragmata|Animation", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UAnimMontage> JogStartMontage;
	// UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pragmata|Animation", meta=(AllowPrivateAccess="true"))
	// TObjectPtr<UAnimMontage> WalkEndMontage;
	// dash montage
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pragmata|Animation", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UAnimMontage> DashMontage;

	// aim dash montage
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pragmata|Animation", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UAnimMontage> AimDashMontage;

	// midair dash montage
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pragmata|Animation", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UAnimMontage> MidairDashMontage;

	// reload montage
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pragmata|Animation", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UAnimMontage> ReloadGripgunMontage;

	// small hit react montage
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pragmata|Animation", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UAnimMontage> HitReactSmallMontage;

	// medium hit react montage
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pragmata|Animation", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UAnimMontage> HitReactMediumMontage;

	// large hit react montage
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pragmata|Animation", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UAnimMontage> HitReactLargeMontage;

	// midair hit react montage
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pragmata|Animation", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UAnimMontage> HitReactMidairMontage;

	// death montage
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pragmata|Animation", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UAnimMontage> DeathMontage;

	// draw shotgun montage
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pragmata|Animation", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UAnimMontage> DrawWeaponFromLeftBackMontage;

	// store shotgun montage
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pragmata|Animation", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UAnimMontage> StoreWeaponToLeftBackMontage;

	// draw defense weapon montage
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pragmata|Animation", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UAnimMontage> DrawWeaponFromRightBackMontage;

	// store defense weapon montage
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pragmata|Animation", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UAnimMontage> StoreWeaponToRightBackMontage;


	// throw away weapon montage
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pragmata|Animation", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UAnimMontage> ThrowAwayWeaponMontage;

	// pickup weapon montage
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pragmata|Animation", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UAnimMontage> PickUpWeaponMontage;
	
	// draw weapon from waistarm montage
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pragmata|Animation", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UAnimMontage> DrawWeaponFromWaistMontage;
protected://inputs
	//IA_Move
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;

	//IA_Look
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> LookAction;

	//IA_Attack
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> AttackAction;

	//IA_Dodge
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> DodgeAction;

	//IA_Jump
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> JumpAction;

	//IA_Aim
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> AimAction;

	//IA_Boost
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> BoostAction;

	//IA_HeadVisorLightToggle
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> HeadVisorLightToggleAction;

	//IA_WeaponChange
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> WeaponChangeAction;

	//IA_CycleWeapon
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> CycleWeaponAction;

	//IA_Interact
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> InteractAction;

	//IA_Heal
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> HealAction;
protected://Player States
	// 현재 상체 action state
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	EActionState ActionState = EActionState::Locomotion;

	//현재 플레이어의 실제 movement state
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	EMovementState MovementState = EMovementState::Idle;

	//IA_Move 입력이 들어왔을 때 유저의 의도
	// UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	// EMovementState IntendedMovementState = EMovementState::Jog;
protected://recoil
	//누적된 반동값
	UPROPERTY()
	float CurrentRecoilPitch = 0.f;

	// 목표 recoil pitch
	UPROPERTY()
	float TargetRecoilPitch = 0.f;

	// 직전 recoil pitch
	UPROPERTY()
	float LastRecoilPitch = 0.f;

	// recoil 상승 보간 속도
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil")
	float CameraUpSpeed = 30.f;

	// recoil 회복 보간 속도
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recoil")
	float CameraDownSpeed = 10.f;
protected://조준 관련 변수들
	// 기본 camera FOV
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pragmata|Combat")
	float DefaultFOV  = 90.f;

	// 조준 camera FOV
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pragmata|Combat")
	float AimFOV = 60.f;

	// 기본 camera boom 길이
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pragmata|Combat")
	float DefaultArmLength = 270.f;

	// 조준 camera boom 길이
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pragmata|Combat")
	float AimArmLength = 240.f;
protected://움직임 관련 변수들
	// 직전 actor rotation
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Pragmata|Movement")
	FRotator LastRotation = FRotator::ZeroRotator;

	// AnimBP lean 값
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Pragmata|Movement")
	float LeanAmount = 0.f;

	// 조준 이동 방향 각도
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Pragmata|Movement")
	float Angle = 0.f;

	// 회전 보간 속도
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pragmata|Movement")
	float RotateSpeed = 15.f;

	// walk 판정 속도 임계값
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pragmata|Movement")
	float WalkThreshold = 10.f;

	// walk end montage 임계값
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pragmata|Movement")
	float WalkEndThreshold = 10.f;

	// 이동 입력 intent 상태
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Pragmata|Movement")
	bool bIsTryingToMove = false;

	// camera zoom 보간 속도
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pragmata|Movement")
	float ZoomSpeed = 10.f;

	// hover 중력 scale
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pragmata|Movement")
	float HoverGravityScale = 0.f;

	// 기본 중력 scale
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Pragmata|Movement")
	float DefaultGravityScale = 1.75f;

	// braking 상태
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Pragmata|Movement")
	bool IsBraking = false;

	// dash 상태
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Pragmata|Movement")
	bool IsDashing = false;
	int32 LastPlayedDashActionSerial = 0;
	FTimerHandle DashStateTimerHandle;
	FTimerHandle WeaponChangeBlockTimerHandle;
	int32 LastPlayedWeaponChangeActionSerial = 0;
	int32 LastPlayedWeaponDepletionActionSerial = 0;

	// 기본 braking deceleration
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Pragmata|Movement")
	float DefaultBreakingDeceleration = 500.f;

	// 일반 look 감도
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pragmata|Movement")
	float LookSpeed = 5.f;

	// 조준 look 감도
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pragmata|Movement")
	float AimSpeed = 2.5f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Pragmata|Movement")
	bool IsDead = false;
protected://스태미나 관련 변수들
	// 최대 stamina
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing=OnRep_StaminaSnapshot, Category="Pragmata|Stamina")
	float MaxStamina = 100.f;

	// 현재 stamina
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing=OnRep_StaminaSnapshot, Category="Pragmata|Stamina")
	float CurStamina = 0.f;

	// stamina 초당 소모량
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pragmata|Stamina")
	float StaminaConsumeSpeed = 15.f;//스태미나 소모 속도

	// stamina 초당 회복량
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pragmata|Stamina")
	float StaminaRecoverySpeed = 15.f;//스태미나 회복 속도

	// stamina 회복 지연 시간
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pragmata|Stamina")
	float StaminaRecoveryDelay = 1.f;//스태미나 회복 시작 전 대기할 시간

	// jump stamina 소모량
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pragmata|Stamina")
	float JumpStaminaConsumeAmount = 10.f;

	// dash stamina 소모량
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pragmata|Stamina")
	float DashStaminaConsumeAmount = 30.f;

	// stamina 회복 중 여부
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Pragmata|Stamina")
	bool IsRecoveringStamina = false;//스태미나 회복 중일때

	// stamina 소모 가능 여부
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_StaminaSnapshot, Category="Pragmata|Stamina")
	bool IsStaminaConsumable = true;//스태미나 빨갛게 되었을 때

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Pragmata|Hugh|Heal", meta=(AllowPrivateAccess="true", ClampMin="0.01", Units="s"))
	float HealHoldThresholdSeconds = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Pragmata|Hugh|Heal", meta=(AllowPrivateAccess="true", ClampMin="0.01", Units="s"))
	float HealRepeatIntervalSeconds = 1.f;

	UPROPERTY(ReplicatedUsing=OnRep_CanUseHeal, BlueprintReadOnly, VisibleInstanceOnly, Category="Pragmata|Hugh|Heal", meta=(AllowPrivateAccess="true"))
	bool bCanUseHeal = false;

	UPROPERTY(Transient)
	bool bHealInputHeld = false;

	UPROPERTY(Transient)
	float HealHeldSeconds = 0.f;

	UPROPERTY(Transient)
	float NextHealUseSeconds = 0.f;
protected://hover 관련 변수들
	// hover 유지 목표 높이
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Pragmata|Hover")
	float HoverTargetZ = 0.f;

	// 마지막 stamina 소모 시각
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Pragmata|Hover")
	float LastStaminaConsumedTime = 0.f;
protected://weapon related vars
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_WeaponInventory, Category="Pragmata|Weapon")
	TArray<AWeaponBase*> WeaponInventory;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Pragmata|Weapon")
	int CurrentWeaponIndex = 0;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Pragmata|Weapon")
	bool IsWeaponTriggerBlocked = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_StoredWeaponPresentationState, Category="Pragmata|Weapon")
	EWeaponType LastLeftBackWeaponType = EWeaponType::Tactical;
protected://SFX
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pragmata|Audio")
	USoundBase* OnStartAimSFX;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pragmata|Audio")
	USoundBase* OnLandSFX;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pragmata|Audio")
	USoundBase* LackStaminaSFX;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pragmata|Audio")
	USoundBase* HoveringSFX;
	UPROPERTY()
	class UAudioComponent* HoveringAudioComponent;
private:
	// 소유자는 예측값, 원격 프록시는 서버 복제값을 읽는 애니메이션 원본
	UPROPERTY(ReplicatedUsing=OnRep_HughAnimState, BlueprintReadOnly, Category="Pragmata|Animation", meta=(AllowPrivateAccess="true"))
	FHughReplicatedAnimState HughAnimState;

	// HeadVisor light 서버 확정 표시 상태
	UPROPERTY(ReplicatedUsing=OnRep_HeadVisorSpotlightEnabled, BlueprintReadOnly, Category="Pragmata|Hugh|Light", meta=(AllowPrivateAccess="true"))
	bool bHeadVisorSpotlightEnabled = true;
};
