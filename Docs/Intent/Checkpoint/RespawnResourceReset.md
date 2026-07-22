# Checkpoint respawn resource reset 의도 카드

## 대상

`UPragmataHealthComponent::RestoreFullHealth`, `AWeaponBase::RefillAmmoToMax`

## 이유

checkpoint interaction과 death respawn이 HP, 획득한 weapon ammo, heal cartridge를 동일한 server-authoritative 보급 규칙으로 복구하게 한다.

## 규칙

- HP restore는 server에서만 실행하되 death lifecycle을 해제하지 않음
- ammo restore는 max ammo와 HUD ammo delegate를 함께 갱신
- Lunafilament, upgrade level, OverDrive, cleanse unlock은 보급 대상이 아님

## 호출 경계

checkpoint activation/active interaction과 10초 respawn callback만 호출한다.

사망 상태 해제와 pawn transform 복구는 10초 timer 뒤 `APragmataGameMode::ExecuteTeamRespawn`만 소유한다. 체력 delegate나 사망 대기 중 checkpoint 보급이 `ResetForRespawn`을 호출하면 안 된다.

`AHughCharacter::ReplenishCheckpointResources`가 HP, 보유 weapon ammo, repair cartridge 호출을 묶는다.
