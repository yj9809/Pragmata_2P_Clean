#include "Data/LobbyCharacterSkinSet.h"

#include "Engine/SkeletalMesh.h"

namespace PragmataLobbySkinIds
{
	const FName DelphiSuit(TEXT("DelphiSuit"));
	const FName ScribbleSuit(TEXT("ScribbleSuit"));
	const FName DianaDefault(TEXT("DianaDefault"));
}

namespace
{
FLobbyCharacterSkinDefinition MakeSkinDefinition(EPragmataPlayerRole PlayerRole, FName SkinId, const TCHAR* CodeLabel, const TCHAR* DisplayName, bool bDefaultForRole, const TCHAR* MeshPath)
{
	FLobbyCharacterSkinDefinition Definition;
	Definition.PlayerRole = PlayerRole;
	Definition.SkinId = SkinId;
	Definition.CodeLabel = CodeLabel;
	Definition.DisplayName = FText::FromString(DisplayName);
	Definition.bDefaultForRole = bDefaultForRole;
	Definition.Mesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(MeshPath));
	return Definition;
}

const TArray<FLobbyCharacterSkinDefinition>& GetFallbackSkinDefinitions()
{
	static const TArray<FLobbyCharacterSkinDefinition> FallbackDefinitions = ULobbyCharacterSkinSet::MakeFallbackSkinDefinitions();
	return FallbackDefinitions;
}
}

TArray<FLobbyCharacterSkinDefinition> ULobbyCharacterSkinSet::MakeFallbackSkinDefinitions()
{
	TArray<FLobbyCharacterSkinDefinition> Definitions;
	Definitions.Reserve(3);
	Definitions.Add(MakeSkinDefinition(
		EPragmataPlayerRole::Hugh,
		PragmataLobbySkinIds::DelphiSuit,
		TEXT("Delphi Suit"),
		TEXT("기본 · 델피 우주복"),
		true,
		TEXT("/Game/Character/Hugh/SM_Hugh.SM_Hugh")));
	Definitions.Add(MakeSkinDefinition(
		EPragmataPlayerRole::Hugh,
		PragmataLobbySkinIds::ScribbleSuit,
		TEXT("Scribble Suit"),
		TEXT("낙서 우주복"),
		false,
		TEXT("/Game/Character/Hugh_Scribble/SM_Hugh_Scribble.SM_Hugh_Scribble")));
	Definitions.Add(MakeSkinDefinition(
		EPragmataPlayerRole::Diana,
		PragmataLobbySkinIds::DianaDefault,
		TEXT("Diana Default"),
		TEXT("기본 · Diana"),
		true,
		TEXT("/Game/Character/Diana/SM_Diana.SM_Diana")));
	return Definitions;
}

// DataAsset이 아직 생성되지 않은 빌드에서도 lobby skin 기능이 동작하도록 동일한 fallback을 사용
const TArray<FLobbyCharacterSkinDefinition>& ULobbyCharacterSkinSet::GetEffectiveSkins() const
{
	return Skins.IsEmpty() ? GetFallbackSkinDefinitions() : Skins;
}

// role별 기본 skin은 명시 기본값을 우선하고 없으면 첫 유효 항목으로 복구
FName ULobbyCharacterSkinSet::ResolveDefaultSkinId(EPragmataPlayerRole PlayerRole) const
{
	if (const FLobbyCharacterSkinDefinition* DefaultSkin = FindDefaultSkin(PlayerRole))
	{
		return DefaultSkin->SkinId;
	}

	for (const FLobbyCharacterSkinDefinition& Definition : GetEffectiveSkins())
	{
		if (Definition.PlayerRole == PlayerRole && Definition.SkinId != NAME_None)
		{
			return Definition.SkinId;
		}
	}

	return NAME_None;
}

// 상하 입력은 현재 role 안에서만 순환하고 단일 skin이면 같은 id를 유지
FName ULobbyCharacterSkinSet::GetAdjacentSkinId(EPragmataPlayerRole PlayerRole, FName CurrentSkinId, int32 Direction) const
{
	TArray<FLobbyCharacterSkinDefinition> RoleSkins;
	for (const FLobbyCharacterSkinDefinition& Definition : GetEffectiveSkins())
	{
		if (Definition.PlayerRole == PlayerRole && Definition.SkinId != NAME_None)
		{
			RoleSkins.Add(Definition);
		}
	}

	if (RoleSkins.IsEmpty())
	{
		return NAME_None;
	}

	if (RoleSkins.Num() == 1 || Direction == 0)
	{
		return IsValidSkin(PlayerRole, CurrentSkinId) ? CurrentSkinId : RoleSkins[0].SkinId;
	}

	int32 CurrentIndex = 0;
	for (int32 Index = 0; Index < RoleSkins.Num(); ++Index)
	{
		if (RoleSkins[Index].SkinId == CurrentSkinId)
		{
			CurrentIndex = Index;
			break;
		}
	}

	const int32 Step = Direction < 0 ? -1 : 1;
	const int32 NextIndex = (CurrentIndex + Step + RoleSkins.Num()) % RoleSkins.Num();
	return RoleSkins[NextIndex].SkinId;
}

bool ULobbyCharacterSkinSet::IsValidSkin(EPragmataPlayerRole PlayerRole, FName SkinId) const
{
	return FindSkin(PlayerRole, SkinId) != nullptr;
}

const FLobbyCharacterSkinDefinition* ULobbyCharacterSkinSet::FindSkin(EPragmataPlayerRole PlayerRole, FName SkinId) const
{
	for (const FLobbyCharacterSkinDefinition& Definition : GetEffectiveSkins())
	{
		if (Definition.PlayerRole == PlayerRole && Definition.SkinId == SkinId)
		{
			return &Definition;
		}
	}

	return nullptr;
}

const FLobbyCharacterSkinDefinition* ULobbyCharacterSkinSet::FindDefaultSkin(EPragmataPlayerRole PlayerRole) const
{
	for (const FLobbyCharacterSkinDefinition& Definition : GetEffectiveSkins())
	{
		if (Definition.PlayerRole == PlayerRole && Definition.bDefaultForRole)
		{
			return &Definition;
		}
	}

	return nullptr;
}

// packaged lobby에서 교체 순간 로딩 지연을 줄이기 위해 사용 지점에서 동기 load 가능하게 제공
USkeletalMesh* ULobbyCharacterSkinSet::LoadSkinMesh(EPragmataPlayerRole PlayerRole, FName SkinId) const
{
	const FLobbyCharacterSkinDefinition* Definition = FindSkin(PlayerRole, SkinId);
	if (!Definition || Definition->Mesh.IsNull())
	{
		return nullptr;
	}

	return Definition->Mesh.LoadSynchronous();
}

// preview/gameplay 진입 전에 명시 skin mesh를 미리 올려 순간적인 빈 mesh 표시를 피함
void ULobbyCharacterSkinSet::LoadAllMeshes() const
{
	for (const FLobbyCharacterSkinDefinition& Definition : GetEffectiveSkins())
	{
		if (!Definition.Mesh.IsNull())
		{
			Definition.Mesh.LoadSynchronous();
		}
	}
}
