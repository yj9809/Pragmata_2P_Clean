// Fill out your copyright notice in the Description page of Project Settings.


#include "Enemy/WatcherMonster.h"

#include "Combat/PragmataDamageLibrary.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Puzzle/EnemyHackingPuzzleComponent.h"

AWatcherMonster::AWatcherMonster()
{
	MonsterId = TEXT("Watcher");
	ScanDisplayText = NSLOCTEXT("Pragmata", "WatcherScanDisplayText", "Watcher");
	if (EnemyHackingPuzzleComponent)
	{
		EnemyHackingPuzzleComponent->EnemyType = EEnemyHackType::Watcher;
	}
	
	if (GetCharacterMovement())
	{
		GetCharacterMovement()->SetMovementMode(MOVE_None);
		GetCharacterMovement()->GravityScale = 0.f;
	}
}

// Watcher 원거리 탄환도 MonsterBase 공통 combat damage를 사용
void AWatcherMonster::FireBullet(const FHitResult& HitResult)
{
	if (!HitResult.GetActor() || !GetController()) return;
	
	if (HasAuthority())
	{
		FPragmataDamageSpec DamageSpec;
		DamageSpec.DamageAmount = IPragmataDamageSource::Execute_GetCurrentAttackDamage(this);
		DamageSpec.SourceActor = GetOwner();
		DamageSpec.DamageCauser = this;
		DamageSpec.InstigatorController = GetController();
		DamageSpec.DamageKind = EPragmataDamageKind::Generic;
		DamageSpec.HitReactStrength = EPragmataHitReactStrength::Small;
		DamageSpec.bHasImpact = true;
		DamageSpec.ImpactHit = HitResult;
		DamageSpec.ImpactDirection = (HitResult.ImpactPoint - HitResult.TraceStart).GetSafeNormal();
	
		UPragmataDamageLibrary::DealDamageToActor(HitResult.GetActor(), DamageSpec);
		
		//request all clients to play effects
		MulticastPlayFireEffects(HitResult);
	}
}

void AWatcherMonster::MulticastPlayFireEffects_Implementation(const FHitResult& HitResult)
{
	if (GEngine)
	{
		const FString Message = FString::Printf(TEXT("Watcher Fired!"));
		GEngine->ClearOnScreenDebugMessages();
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, Message);
	}
}
