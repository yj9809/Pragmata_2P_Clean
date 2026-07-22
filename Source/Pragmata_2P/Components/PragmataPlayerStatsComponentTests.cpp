#include "Characters/HughCharacter.h"
#include "Components/PragmataHealthComponent.h"
#include "Components/PragmataPlayerStatsComponent.h"
#include "Data/PragmataTypes.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "UI/PragmataHUDViewModel.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPragmataPlayerStatsComponentTest,
	"Pragmata.Stats.HughSharedStats.PlayerStatsAndHUDViewModel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

// Hugh shared stats가 HP source와 회복 아이템을 같은 서버 actor에서 처리하는지 검증
bool FPragmataPlayerStatsComponentTest::RunTest(const FString& Parameters)
{
	UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false, TEXT("PragmataPlayerStatsComponentTest"));
	TestNotNull(TEXT("Test world"), TestWorld);
	if (!TestWorld)
	{
		return false;
	}

	AHughCharacter* HughCharacter = TestWorld->SpawnActor<AHughCharacter>();
	TestNotNull(TEXT("Hugh character"), HughCharacter);
	if (!HughCharacter)
	{
		TestWorld->DestroyWorld(false);
		return false;
	}

	UPragmataHealthComponent* HealthComponent = HughCharacter->GetHealthComponent();
	UPragmataPlayerStatsComponent* PlayerStatsComponent = HughCharacter->GetPlayerStatsComponent();
	TestNotNull(TEXT("Health component"), HealthComponent);
	TestNotNull(TEXT("Player stats component"), PlayerStatsComponent);
	if (!HealthComponent || !PlayerStatsComponent)
	{
		TestWorld->DestroyWorld(false);
		return false;
	}

	TestEqual(TEXT("Hugh default max HP"), HealthComponent->GetMaxHP(), 1500);
	TestEqual(TEXT("Hugh default current HP"), HealthComponent->GetCurrentHP(), 1500);
	TestEqual(TEXT("Initial Lunafilament"), PlayerStatsComponent->GetLunafilament(), 0);
	TestEqual(TEXT("Initial damage multiplier"), PlayerStatsComponent->GetPlayerDamageMultiplier(), 1.f);
	TestEqual(TEXT("Initial repair cartridge count"), PlayerStatsComponent->GetRepairCartridge(), 6);
	TestEqual(TEXT("Initial remaining heal cartridges"), PlayerStatsComponent->GetRemainingHealCartridges(), 6);
	TestEqual(TEXT("Max repair cartridge count"), PlayerStatsComponent->GetMaxRepairCartridge(), 7);

	UPragmataHUDViewModel* HUDViewModel = NewObject<UPragmataHUDViewModel>(GetTransientPackage());
	TestNotNull(TEXT("HUD ViewModel"), HUDViewModel);
	if (!HUDViewModel)
	{
		TestWorld->DestroyWorld(false);
		return false;
	}

	HUDViewModel->BindPlayerStatsComponent(PlayerStatsComponent);
	TestEqual(TEXT("ViewModel initial Lunafilament"), HUDViewModel->GetLunafilament(), 0);
	TestEqual(TEXT("Lunafilament gain merge window"), HUDViewModel->GetLunafilamentGainMergeWindowSeconds(), 1.f);
	TestEqual(TEXT("Initial Lunafilament gain sequence"), HUDViewModel->GetLunafilamentGainSequence(), 0);

	TestFalse(TEXT("Zero Lunafilament add is ignored"), PlayerStatsComponent->AddLunafilament(0));
	TestFalse(TEXT("Negative Lunafilament add is ignored"), PlayerStatsComponent->AddLunafilament(-10));
	TestEqual(TEXT("Invalid Lunafilament add does not change total"), PlayerStatsComponent->GetLunafilament(), 0);
	TestEqual(TEXT("Invalid Lunafilament add does not update ViewModel sequence"), HUDViewModel->GetLunafilamentGainSequence(), 0);
	TestTrue(TEXT("Lunafilament can increase"), PlayerStatsComponent->AddLunafilament(50));
	TestEqual(TEXT("Added Lunafilament total"), PlayerStatsComponent->GetLunafilament(), 50);
	TestEqual(TEXT("ViewModel Lunafilament follows authoritative total"), HUDViewModel->GetLunafilament(), 50);
	TestEqual(TEXT("ViewModel gain delta"), HUDViewModel->GetLastLunafilamentGainDelta(), 50);
	TestEqual(TEXT("ViewModel gain old total"), HUDViewModel->GetLastLunafilamentGainOldTotal(), 0);
	TestEqual(TEXT("ViewModel gain new total"), HUDViewModel->GetLastLunafilamentGainNewTotal(), 50);
	TestEqual(TEXT("ViewModel gain sequence increments"), HUDViewModel->GetLunafilamentGainSequence(), 1);
	TestTrue(TEXT("Lunafilament clamps to int max"), PlayerStatsComponent->AddLunafilament(MAX_int32));
	TestEqual(TEXT("Clamped Lunafilament total"), PlayerStatsComponent->GetLunafilament(), MAX_int32);
	TestEqual(TEXT("Clamped Lunafilament gain delta is actual increase"), HUDViewModel->GetLastLunafilamentGainDelta(), MAX_int32 - 50);
	TestEqual(TEXT("Clamped Lunafilament gain old total"), HUDViewModel->GetLastLunafilamentGainOldTotal(), 50);
	TestEqual(TEXT("Clamped Lunafilament gain new total"), HUDViewModel->GetLastLunafilamentGainNewTotal(), MAX_int32);
	TestEqual(TEXT("Clamped Lunafilament gain sequence increments"), HUDViewModel->GetLunafilamentGainSequence(), 2);
	TestFalse(TEXT("Clamped Lunafilament add does not broadcast success"), PlayerStatsComponent->AddLunafilament(1));
	TestEqual(TEXT("Lunafilament remains clamped"), PlayerStatsComponent->GetLunafilament(), MAX_int32);
	TestEqual(TEXT("Clamped Lunafilament add does not update gain sequence"), HUDViewModel->GetLunafilamentGainSequence(), 2);
	TestTrue(TEXT("Spend Lunafilament changes authoritative total"), PlayerStatsComponent->SpendLunafilament(25));
	TestEqual(TEXT("Spend Lunafilament updates ViewModel total"), HUDViewModel->GetLunafilament(), MAX_int32 - 25);
	TestEqual(TEXT("Spend Lunafilament does not update gain sequence"), HUDViewModel->GetLunafilamentGainSequence(), 2);
	HUDViewModel->UnbindPlayerStatsComponent();
	TestTrue(TEXT("Gain after ViewModel unbind succeeds"), PlayerStatsComponent->AddLunafilament(1));
	TestEqual(TEXT("Unbound ViewModel does not receive gain sequence"), HUDViewModel->GetLunafilamentGainSequence(), 2);
	TestTrue(TEXT("Saturated Lunafilament can reset for upgrade fixture"), PlayerStatsComponent->SpendLunafilament(PlayerStatsComponent->GetLunafilament()));
	TestEqual(TEXT("Upgrade fixture starts with zero Lunafilament"), PlayerStatsComponent->GetLunafilament(), 0);

	FPragmataDamageSpec DamageSpec;
	DamageSpec.DamageAmount = 600.f;
	TestTrue(TEXT("Damage can reduce Hugh HP"), HealthComponent->ReceiveDamage(DamageSpec));
	TestEqual(TEXT("Damaged HP"), HealthComponent->GetCurrentHP(), 900);

	TestFalse(TEXT("Zero recovery is ignored"), HealthComponent->RecoverHP(0));
	TestFalse(TEXT("Negative recovery is ignored"), HealthComponent->RecoverHP(-10));
	TestEqual(TEXT("Invalid recovery does not change HP"), HealthComponent->GetCurrentHP(), 900);

	TestTrue(TEXT("Repair cartridges clamp to max"), PlayerStatsComponent->AddRepairCartridge(10));
	TestEqual(TEXT("Clamped repair cartridge count"), PlayerStatsComponent->GetRepairCartridge(), 7);
	TestEqual(TEXT("Remaining heal cartridges follows repair cartridges"), PlayerStatsComponent->GetRemainingHealCartridges(), 7);
	TestTrue(TEXT("Damaged Hugh can use heal"), HughCharacter->CanUseHeal());

	TestTrue(TEXT("Repair cartridge consumes only after recovery"), PlayerStatsComponent->ConsumeRepairCartridge());
	TestEqual(TEXT("Repair cartridge recovers default amount"), HealthComponent->GetCurrentHP(), 1200);
	TestEqual(TEXT("Repair cartridge count decreases"), PlayerStatsComponent->GetRepairCartridge(), 6);
	TestEqual(TEXT("Remaining heal cartridges decreases"), HughCharacter->GetRemainingHealCartridges(), 6);

	TestTrue(TEXT("Direct recovery fills HP"), HealthComponent->RecoverHP(300));
	PlayerStatsComponent->SetRepairCartridge(1);
	TestFalse(TEXT("Full HP does not consume repair cartridge"), PlayerStatsComponent->ConsumeRepairCartridge());
	TestEqual(TEXT("Full HP preserves repair cartridge"), PlayerStatsComponent->GetRepairCartridge(), 1);
	TestFalse(TEXT("Full HP blocks heal use"), HughCharacter->CanUseHeal());

	PlayerStatsComponent->SetPlayerDamageMultiplier(2.f);
	TestEqual(TEXT("Damage multiplier can increase"), PlayerStatsComponent->GetPlayerDamageMultiplier(), 2.f);
	PlayerStatsComponent->SetPlayerDamageMultiplier(-1.f);
	TestEqual(TEXT("Damage multiplier clamps negative values"), PlayerStatsComponent->GetPlayerDamageMultiplier(), 0.f);
	PlayerStatsComponent->SetPlayerDamageMultiplier(1.f);
	TestFalse(TEXT("Invalid Decode bonus multiplier is ignored"), PlayerStatsComponent->ApplyDecodeWeaponDamageBonus(0.f, 10.f));
	TestFalse(TEXT("Invalid Decode bonus duration is ignored"), PlayerStatsComponent->ApplyDecodeWeaponDamageBonus(1.1f, 0.f));
	TestTrue(TEXT("Decode bonus applies"), PlayerStatsComponent->ApplyDecodeWeaponDamageBonus());
	TestTrue(TEXT("Decode bonus multiplier is 1.1"), FMath::IsNearlyEqual(PlayerStatsComponent->GetDecodeWeaponDamageMultiplier(), 1.1f));
	TestTrue(TEXT("Decode bonus refreshes without stacking"), PlayerStatsComponent->ApplyDecodeWeaponDamageBonus());
	TestTrue(TEXT("Refreshed Decode bonus stays 1.1"), FMath::IsNearlyEqual(PlayerStatsComponent->GetDecodeWeaponDamageMultiplier(), 1.1f));

	TestFalse(TEXT("Upgrade requires Lunafilament"), PlayerStatsComponent->TryUpgrade(EPragmataUpgradeType::Suit));
	TestTrue(TEXT("Lunafilament can be added for upgrade"), PlayerStatsComponent->AddLunafilament(200));
	TestEqual(TEXT("Upgrade currency after grant"), PlayerStatsComponent->GetLunafilament(), 200);

	TestTrue(TEXT("Suit upgrade consumes Lunafilament"), PlayerStatsComponent->TryUpgrade(EPragmataUpgradeType::Suit));
	TestEqual(TEXT("Suit upgrade level increases"), PlayerStatsComponent->GetUpgradeLevel(EPragmataUpgradeType::Suit), 1);
	TestEqual(TEXT("Suit upgrade max HP increases"), HealthComponent->GetMaxHP(), 1600);
	TestEqual(TEXT("Suit upgrade current HP increases by delta"), HealthComponent->GetCurrentHP(), 1600);
	TestEqual(TEXT("Suit upgrade cost spent"), PlayerStatsComponent->GetLunafilament(), 100);

	TestTrue(TEXT("Weapon upgrade consumes Lunafilament"), PlayerStatsComponent->TryUpgrade(EPragmataUpgradeType::Weapon));
	TestEqual(TEXT("Weapon upgrade level increases"), PlayerStatsComponent->GetUpgradeLevel(EPragmataUpgradeType::Weapon), 1);
	TestTrue(TEXT("Weapon upgrade multiplier applies"), FMath::IsNearlyEqual(PlayerStatsComponent->GetPlayerDamageMultiplier(), 1.1f));
	TestEqual(TEXT("Weapon upgrade cost spent"), PlayerStatsComponent->GetLunafilament(), 0);

	TestTrue(TEXT("Weapon refund returns Lunafilament"), PlayerStatsComponent->TryRefundUpgrade(EPragmataUpgradeType::Weapon));
	TestEqual(TEXT("Weapon upgrade level decreases"), PlayerStatsComponent->GetUpgradeLevel(EPragmataUpgradeType::Weapon), 0);
	TestEqual(TEXT("Weapon multiplier returns to base"), PlayerStatsComponent->GetPlayerDamageMultiplier(), 1.f);
	TestEqual(TEXT("Weapon refund currency returned"), PlayerStatsComponent->GetLunafilament(), 100);

	TestTrue(TEXT("Suit refund returns Lunafilament"), PlayerStatsComponent->TryRefundUpgrade(EPragmataUpgradeType::Suit));
	TestEqual(TEXT("Suit upgrade level decreases"), PlayerStatsComponent->GetUpgradeLevel(EPragmataUpgradeType::Suit), 0);
	TestEqual(TEXT("Suit refund max HP returns to base"), HealthComponent->GetMaxHP(), 1500);
	TestEqual(TEXT("Suit refund clamps current HP to max"), HealthComponent->GetCurrentHP(), 1500);
	TestEqual(TEXT("Suit refund currency returned"), PlayerStatsComponent->GetLunafilament(), 200);

	TestWorld->DestroyWorld(false);
	return true;
}

#endif
