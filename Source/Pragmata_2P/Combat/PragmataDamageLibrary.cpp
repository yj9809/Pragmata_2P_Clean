#include "Combat/PragmataDamageLibrary.h"

#include "Characters/HughCharacter.h"
#include "Components/PragmataFactionComponent.h"
#include "Components/PragmataPlayerStatsComponent.h"
#include "Interfaces/PragmataDamageable.h"

namespace
{
// faction 판정 기준 actor를 source 우선, causer fallback으로 고정
const AActor* ResolveDamageFactionActor(const FPragmataDamageSpec& DamageSpec)
{
	return IsValid(DamageSpec.SourceActor.Get())
		? DamageSpec.SourceActor.Get()
		: DamageSpec.DamageCauser.Get();
}
}

bool UPragmataDamageLibrary::DealDamageToActor(AActor* TargetActor, const FPragmataDamageSpec& DamageSpec)
{
	// caller는 대상만 찾고 실제 수신 가능 여부는 target contract가 결정
	if (!TargetActor || !TargetActor->GetClass()->ImplementsInterface(UPragmataDamageable::StaticClass()))
	{
		return false;
	}

	// 공통 damage 진입점에서 target receiver 부작용까지 서버 권한으로 고정
	if (!TargetActor->HasAuthority())
	{
		return false;
	}

	const AActor* SourceActor = ResolveDamageFactionActor(DamageSpec);
	const UPragmataFactionComponent* SourceFaction = SourceActor ? SourceActor->FindComponentByClass<UPragmataFactionComponent>() : nullptr;
	const UPragmataFactionComponent* TargetFaction = TargetActor->FindComponentByClass<UPragmataFactionComponent>();
	if (SourceFaction && TargetFaction && !SourceFaction->IsEnemyFaction(TargetFaction->GetFaction()))
	{
		return false;
	}

	FPragmataDamageSpec ResolvedDamageSpec = DamageSpec;
	if (DamageSpec.DamageKind == EPragmataDamageKind::Weapon)
	{
		// Hugh만 가진 stat component의 Decode 배율을 공통 weapon damage 경계에서 한 번 적용
		if (const AHughCharacter* HughCharacter = Cast<AHughCharacter>(SourceActor))
		{
			if (const UPragmataPlayerStatsComponent* PlayerStatsComponent = HughCharacter->GetPlayerStatsComponent())
			{
				ResolvedDamageSpec.DamageAmount *= FMath::Max(0.f, PlayerStatsComponent->GetDecodeWeaponDamageMultiplier());
			}
		}
	}

	return IPragmataDamageable::Execute_ReceiveDamage(TargetActor, ResolvedDamageSpec);
}
