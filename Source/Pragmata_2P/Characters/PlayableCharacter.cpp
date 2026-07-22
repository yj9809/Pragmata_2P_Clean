#include "Characters/PlayableCharacter.h"

#include "Components/PragmataFactionComponent.h"
#include "Components/PragmataHealthComponent.h"

// 하위 character가 ObjectInitializer로 movement component를 교체할 수 있게 유지
APlayableCharacter::APlayableCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bReplicates = true;

	HealthComponent = CreateDefaultSubobject<UPragmataHealthComponent>(TEXT("HealthComponent"));
	FactionComponent = CreateDefaultSubobject<UPragmataFactionComponent>(TEXT("FactionComponent"));
}

// 죽었거나 무효 피해인 경우 공통 피해 적용 경로를 차단
bool APlayableCharacter::CanReceiveDamage_Implementation(const FPragmataDamageSpec& DamageSpec) const
{
	return HealthComponent && !HealthComponent->IsDead() && DamageSpec.DamageAmount > 0.f;
}

// 하위 character가 animation cue를 덧붙일 수 있도록 기본 health 처리만 담당
bool APlayableCharacter::ReceiveDamage_Implementation(const FPragmataDamageSpec& DamageSpec)
{
	if (!IPragmataDamageable::Execute_CanReceiveDamage(this, DamageSpec))
	{
		return false;
	}

	return HealthComponent && HealthComponent->ReceiveDamage(DamageSpec);
}
