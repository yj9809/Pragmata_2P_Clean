#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Interfaces/PragmataDamageable.h"
#include "PlayableCharacter.generated.h"

class UPragmataHealthComponent;
class UPragmataFactionComponent;

UCLASS(Abstract)
class PRAGMATA_2P_API APlayableCharacter : public ACharacter, public IPragmataDamageable
{
	GENERATED_BODY()

public:
	// 하위 캐릭터가 기본 movement component class를 교체할 수 있게 ObjectInitializer를 노출
	explicit APlayableCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// 공통 피해 수신 가능 여부 확인
	virtual bool CanReceiveDamage_Implementation(const FPragmataDamageSpec& DamageSpec) const override;

	// 공통 피해 수신 처리
	virtual bool ReceiveDamage_Implementation(const FPragmataDamageSpec& DamageSpec) override;

	// 체력 컴포넌트 조회
	UFUNCTION(BlueprintPure, Category="Pragmata|Character")
	UPragmataHealthComponent* GetHealthComponent() const { return HealthComponent; }

	// 진영 컴포넌트 조회
	UFUNCTION(BlueprintPure, Category="Pragmata|Character")
	UPragmataFactionComponent* GetFactionComponent() const { return FactionComponent; }

protected:
	// 공통 체력 상태
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Pragmata|Character")
	TObjectPtr<UPragmataHealthComponent> HealthComponent = nullptr;

	// 공통 진영 상태
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Pragmata|Character")
	TObjectPtr<UPragmataFactionComponent> FactionComponent = nullptr;
};
