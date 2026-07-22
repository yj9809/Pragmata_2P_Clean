#pragma once

#include "CoreMinimal.h"
#include "Data/PragmataTypes.h"
#include "GameFramework/PlayerState.h"
#include "PragmataPlayerState.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FPragmataPlayerRoleChangedSignature, EPragmataPlayerRole);
DECLARE_MULTICAST_DELEGATE(FPragmataLobbyStateChangedSignature);
DECLARE_MULTICAST_DELEGATE_OneParam(FPragmataCleanseUnlockedChangedSignature, bool);

UCLASS()
class PRAGMATA_2P_API APragmataPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void CopyProperties(APlayerState* PlayerState) override;
	virtual void OverrideWith(APlayerState* PlayerState) override;

	UFUNCTION(BlueprintPure, Category="Pragmata|Player")
	EPragmataPlayerRole GetAssignedRole() const { return AssignedRole; }

	void SetAssignedRole(EPragmataPlayerRole NewRole);

	UFUNCTION(BlueprintPure, Category="Pragmata|Player")
	EPragmataPlayerRole GetActiveControlRole() const { return ActiveControlRole; }

	void SetActiveControlRole(EPragmataPlayerRole NewRole);

	UFUNCTION(BlueprintPure, Category="Pragmata|Lobby")
	int32 GetLobbyPlayerIndex() const { return LobbyPlayerIndex; }

	void SetLobbyPlayerIndex(int32 NewIndex);

	UFUNCTION(BlueprintPure, Category="Pragmata|Lobby")
	EPragmataPlayerRole GetLobbySelectedRole() const { return LobbySelectedRole; }

	void SetLobbySelectedRole(EPragmataPlayerRole NewRole);

	UFUNCTION(BlueprintPure, Category="Pragmata|Lobby")
	FName GetLobbySelectedSkinId() const { return LobbySelectedSkinId; }

	void SetLobbySelectedSkinId(FName NewSkinId);

	UFUNCTION(BlueprintPure, Category="Pragmata|Lobby")
	bool IsLobbyReady() const { return bLobbyReady; }

	void SetLobbyReady(bool bNewReady);

	UFUNCTION(BlueprintPure, Category="Pragmata|Player")
	bool IsCleanseUnlocked() const { return bCleanseUnlocked; }

	void UnlockCleanse();

	FPragmataPlayerRoleChangedSignature OnAssignedRoleChanged;
	// swap 중 input/camera binding만 바꿀 수 있게 분리된 조작 role 알림
	FPragmataPlayerRoleChangedSignature OnActiveControlRoleChanged;
	// lobby UI 예측 정정 알림
	FPragmataLobbyStateChangedSignature OnLobbyStateChanged;
	FPragmataCleanseUnlockedChangedSignature OnCleanseUnlockedChanged;

private:
	UFUNCTION()
	void OnRep_AssignedRole();

	UFUNCTION()
	void OnRep_ActiveControlRole();

	UFUNCTION()
	void OnRep_LobbyState();

	UFUNCTION()
	void OnRep_CleanseUnlocked();

	// 서버가 확정한 플레이어 역할
	UPROPERTY(ReplicatedUsing=OnRep_AssignedRole, BlueprintReadOnly, Category="Pragmata|Player", meta=(AllowPrivateAccess="true"))
	EPragmataPlayerRole AssignedRole = EPragmataPlayerRole::None;

	// 실제 담당 role과 분리된 현재 조작 role
	UPROPERTY(ReplicatedUsing=OnRep_ActiveControlRole, BlueprintReadOnly, Category="Pragmata|Player", meta=(AllowPrivateAccess="true"))
	EPragmataPlayerRole ActiveControlRole = EPragmataPlayerRole::None;

	// lobby handoff와 host 판정에 쓰는 안정적인 slot 번호
	UPROPERTY(ReplicatedUsing=OnRep_LobbyState, BlueprintReadOnly, Category="Pragmata|Lobby", meta=(AllowPrivateAccess="true"))
	int32 LobbyPlayerIndex = 0;

	// lobby UI의 서버 확정 선택값
	UPROPERTY(ReplicatedUsing=OnRep_LobbyState, BlueprintReadOnly, Category="Pragmata|Lobby", meta=(AllowPrivateAccess="true"))
	EPragmataPlayerRole LobbySelectedRole = EPragmataPlayerRole::None;

	// lobby UI와 gameplay mesh가 공유하는 서버 확정 skin 선택값
	UPROPERTY(ReplicatedUsing=OnRep_LobbyState, BlueprintReadOnly, Category="Pragmata|Lobby", meta=(AllowPrivateAccess="true"))
	FName LobbySelectedSkinId = NAME_None;

	// match start 조건은 서버 확정 ready 값만 신뢰
	UPROPERTY(ReplicatedUsing=OnRep_LobbyState, BlueprintReadOnly, Category="Pragmata|Lobby", meta=(AllowPrivateAccess="true"))
	bool bLobbyReady = false;

	UPROPERTY(ReplicatedUsing=OnRep_CleanseUnlocked, BlueprintReadOnly, Category="Pragmata|Player", meta=(AllowPrivateAccess="true"))
	bool bCleanseUnlocked = false;
};
