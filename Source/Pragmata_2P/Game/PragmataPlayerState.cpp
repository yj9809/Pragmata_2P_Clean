#include "Game/PragmataPlayerState.h"

#include "Net/UnrealNetwork.h"

// role과 lobby 상태를 PlayerState에 복제해 travel 이후에도 같은 선택을 유지
void APragmataPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(APragmataPlayerState, AssignedRole);
	DOREPLIFETIME(APragmataPlayerState, ActiveControlRole);
	DOREPLIFETIME(APragmataPlayerState, LobbyPlayerIndex);
	DOREPLIFETIME(APragmataPlayerState, LobbySelectedRole);
	DOREPLIFETIME(APragmataPlayerState, LobbySelectedSkinId);
	DOREPLIFETIME(APragmataPlayerState, bLobbyReady);
	DOREPLIFETIME(APragmataPlayerState, bCleanseUnlocked);
}

// seamless travel handoff 중 기존 PlayerState의 role/lobby 선택을 새 PlayerState로 넘김
void APragmataPlayerState::CopyProperties(APlayerState* PlayerState)
{
	Super::CopyProperties(PlayerState);

	if (APragmataPlayerState* PragmataPlayerState = Cast<APragmataPlayerState>(PlayerState))
	{
		PragmataPlayerState->AssignedRole = AssignedRole;
		PragmataPlayerState->ActiveControlRole = ActiveControlRole;
		PragmataPlayerState->LobbyPlayerIndex = LobbyPlayerIndex;
		PragmataPlayerState->LobbySelectedRole = LobbySelectedRole;
		PragmataPlayerState->LobbySelectedSkinId = LobbySelectedSkinId;
		PragmataPlayerState->bLobbyReady = bLobbyReady;
		PragmataPlayerState->bCleanseUnlocked = bCleanseUnlocked;
	}
}

// inactive PlayerState 복구 시 lobby UI와 gameplay role delegate를 다시 울림
void APragmataPlayerState::OverrideWith(APlayerState* PlayerState)
{
	Super::OverrideWith(PlayerState);

	if (const APragmataPlayerState* PragmataPlayerState = Cast<APragmataPlayerState>(PlayerState))
	{
		AssignedRole = PragmataPlayerState->AssignedRole;
		ActiveControlRole = PragmataPlayerState->ActiveControlRole;
		LobbyPlayerIndex = PragmataPlayerState->LobbyPlayerIndex;
		LobbySelectedRole = PragmataPlayerState->LobbySelectedRole;
		LobbySelectedSkinId = PragmataPlayerState->LobbySelectedSkinId;
		bLobbyReady = PragmataPlayerState->bLobbyReady;
		bCleanseUnlocked = PragmataPlayerState->bCleanseUnlocked;

		OnRep_AssignedRole();
		OnRep_ActiveControlRole();
		OnRep_LobbyState();
		OnRep_CleanseUnlocked();
	}
}

void APragmataPlayerState::UnlockCleanse()
{
	if (HasAuthority())
	{
		bCleanseUnlocked = true;
		OnRep_CleanseUnlocked();
	}
}

void APragmataPlayerState::OnRep_CleanseUnlocked()
{
	OnCleanseUnlockedChanged.Broadcast(bCleanseUnlocked);
}

// 서버가 gameplay spawn 전에 확정한 role만 클라이언트에 알림
void APragmataPlayerState::SetAssignedRole(EPragmataPlayerRole NewRole)
{
	if (!HasAuthority())
	{
		return;
	}

	if (AssignedRole == NewRole)
	{
		if (ActiveControlRole == EPragmataPlayerRole::None)
		{
			SetActiveControlRole(NewRole);
		}
		return;
	}

	const bool bShouldFollowAssignedRole = ActiveControlRole == EPragmataPlayerRole::None || ActiveControlRole == AssignedRole;
	AssignedRole = NewRole;
	OnRep_AssignedRole();

	if (bShouldFollowAssignedRole)
	{
		SetActiveControlRole(NewRole);
	}
}

// swap이 없을 때는 AssignedRole을 그대로 따르고, swap 중에는 input/camera 기준만 바꿈
void APragmataPlayerState::SetActiveControlRole(EPragmataPlayerRole NewRole)
{
	if (!HasAuthority() || ActiveControlRole == NewRole)
	{
		return;
	}

	ActiveControlRole = NewRole;
	OnRep_ActiveControlRole();
}

// lobby slot 번호는 handoff fallback과 host 판정에 같이 쓰임
void APragmataPlayerState::SetLobbyPlayerIndex(int32 NewIndex)
{
	if (!HasAuthority() || LobbyPlayerIndex == NewIndex)
	{
		return;
	}

	LobbyPlayerIndex = NewIndex;
	OnRep_LobbyState();
}

// 서버 확정 선택값을 복제해 owning client prediction을 정정
void APragmataPlayerState::SetLobbySelectedRole(EPragmataPlayerRole NewRole)
{
	if (!HasAuthority() || LobbySelectedRole == NewRole)
	{
		return;
	}

	LobbySelectedRole = NewRole;
	OnRep_LobbyState();
}

// skin 선택도 role 선택과 같은 lobby state delegate로 prediction을 정정
void APragmataPlayerState::SetLobbySelectedSkinId(FName NewSkinId)
{
	if (!HasAuthority() || LobbySelectedSkinId == NewSkinId)
	{
		return;
	}

	LobbySelectedSkinId = NewSkinId;
	OnRep_LobbyState();
}

// ready 상태는 선택 충돌 정정과 달리 서버 확정값만 UI에 노출
void APragmataPlayerState::SetLobbyReady(bool bNewReady)
{
	if (!HasAuthority() || bLobbyReady == bNewReady)
	{
		return;
	}

	bLobbyReady = bNewReady;
	OnRep_LobbyState();
}

// gameplay controller와 UI가 AssignedRole 변경 시점에 binding을 갱신
void APragmataPlayerState::OnRep_AssignedRole()
{
	OnAssignedRoleChanged.Broadcast(AssignedRole);
}

// gameplay pawn 소유권을 건드리지 않고 조작 권한 표시만 갱신
void APragmataPlayerState::OnRep_ActiveControlRole()
{
	OnActiveControlRoleChanged.Broadcast(ActiveControlRole);
}

// lobby 선택/ready 복제 도착을 owning client prediction 정리 경로에 전달
void APragmataPlayerState::OnRep_LobbyState()
{
	OnLobbyStateChanged.Broadcast();
}
