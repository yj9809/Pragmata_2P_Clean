#pragma once

#include "CoreMinimal.h"
#include "Data/PragmataTypes.h"
#include "Engine/DataAsset.h"
#include "LobbyCharacterSkinSet.generated.h"

class USkeletalMesh;

namespace PragmataLobbySkinIds
{
	PRAGMATA_2P_API extern const FName DelphiSuit;
	PRAGMATA_2P_API extern const FName ScribbleSuit;
	PRAGMATA_2P_API extern const FName DianaDefault;
}

USTRUCT(BlueprintType)
struct PRAGMATA_2P_API FLobbyCharacterSkinDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pragmata|Lobby|Skin")
	EPragmataPlayerRole PlayerRole = EPragmataPlayerRole::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pragmata|Lobby|Skin")
	FName SkinId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pragmata|Lobby|Skin")
	FString CodeLabel;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pragmata|Lobby|Skin")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pragmata|Lobby|Skin")
	bool bDefaultForRole = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pragmata|Lobby|Skin")
	TSoftObjectPtr<USkeletalMesh> Mesh;
};

UCLASS(BlueprintType)
class PRAGMATA_2P_API ULobbyCharacterSkinSet : public UDataAsset
{
	GENERATED_BODY()

public:
	static TArray<FLobbyCharacterSkinDefinition> MakeFallbackSkinDefinitions();

	UFUNCTION(BlueprintPure, Category="Pragmata|Lobby|Skin")
	FName ResolveDefaultSkinId(EPragmataPlayerRole PlayerRole) const;

	UFUNCTION(BlueprintPure, Category="Pragmata|Lobby|Skin")
	FName GetAdjacentSkinId(EPragmataPlayerRole PlayerRole, FName CurrentSkinId, int32 Direction) const;

	UFUNCTION(BlueprintPure, Category="Pragmata|Lobby|Skin")
	bool IsValidSkin(EPragmataPlayerRole PlayerRole, FName SkinId) const;

	const FLobbyCharacterSkinDefinition* FindSkin(EPragmataPlayerRole PlayerRole, FName SkinId) const;
	const FLobbyCharacterSkinDefinition* FindDefaultSkin(EPragmataPlayerRole PlayerRole) const;
	USkeletalMesh* LoadSkinMesh(EPragmataPlayerRole PlayerRole, FName SkinId) const;
	void LoadAllMeshes() const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pragmata|Lobby|Skin")
	TArray<FLobbyCharacterSkinDefinition> Skins;

private:
	const TArray<FLobbyCharacterSkinDefinition>& GetEffectiveSkins() const;
};
