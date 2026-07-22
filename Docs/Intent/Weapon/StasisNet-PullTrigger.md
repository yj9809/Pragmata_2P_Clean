# AStasisNet::PullTrigger 의도 카드

## 대상
`AStasisNet::PullTrigger`, `AStasisNet::ResolveAimTrace`, `AStasisNetField::ApplyDotDamage`

## 종류
Server-authoritative tactical field weapon

## 정의 위치

- `Source/Pragmata_2P/Weapon/StasisNet.h`
- `Source/Pragmata_2P/Weapon/StasisNet.cpp`
- `Source/Pragmata_2P/Weapon/StasisNetField.h`
- `Source/Pragmata_2P/Weapon/StasisNetField.cpp`
- `Source/Pragmata_2P/Characters/HughCharacter.h`
- `Source/Pragmata_2P/Characters/HughCharacter.cpp`

## 존재 이유

StasisNet은 projectile actor 없이 HUD 중앙 reticle 위치를 기준으로 총구 trace 결과에 지속 field를 만들기 위해 존재한다.
VFX/UI가 준비되기 전까지 조준선, 명중 구체, field 범위는 모두 `DrawDebug*`로 확인한다.

## 기본 흐름

`AHughCharacter::Server_RequestFire` -> `AStasisNet::PullTrigger` -> ammo/cooldown gate -> camera reticle trace -> muzzle socket/bone trace -> `AStasisNetField` spawn -> overlap monster stun + 1초 dot damage

`AHughCharacter::PostInitializeComponents` -> BP_HughCharacter `TacticalWeaponClass`가 있으면 Tactical inventory slot spawn -> `IA_ChangeWeapon` value `3` 또는 `IA_CycleWeapon` 순환 -> 기존 weapon inventory index `2` -> `CurrentWeapon = AStasisNet`

ShotGun은 Attack index `1` 슬롯이며 `IA_ChangeWeapon` value `2`로 교체된다. Attack/Tactical 무기는 왼쪽 등 수납 흐름을 공유하고, Defense는 오른쪽 등 수납 흐름으로 분리한다.
비조준 상태의 weapon type 선택은 `CurrentWeapon`을 바꾸고 selected stored weapon 표시를 갱신한다. Attack을 선택하면 왼쪽 등에 ShotGun, Tactical을 선택하면 왼쪽 등에 StasisNet이 보이고, Primary로 돌아가도 최근 Attack/Tactical weapon은 왼쪽 등에 남는다. 조준 시작 시에는 현재 선택된 weapon을 손으로 꺼낸다.
선택한 non-primary slot이 비어 있거나 ammo가 0이면 해당 slot은 선택하지 않고 현재 유효 weapon/UI 선택을 유지한다.

## 서버/클라이언트 조건

- ammo 소모, trace 확정, field spawn, dot damage, stun은 server authority에서만 처리
- weapon 교체는 기존 `AHughCharacter::OnStartWeaponChange`의 `WeaponInventory` index 규칙을 그대로 사용하고, mouse wheel 순환도 다음 index 계산 뒤 같은 `Server_RequestWeaponChange()` 경로로 확정
- 조준 preview는 local Hugh client가 Tactical/StasisNet을 현재 장착하고 aim 중일 때만 draw
- preview 기준 Hugh는 owner, instigator, attach parent 순서로 찾는다. weapon spawn/attach/replication 순서가 바뀌어도 preview가 조용히 막히지 않게 하기 위함이다.
- `MuzzleSocketName`은 socket 또는 skeletal mesh bone 이름으로 쓰며, socket 생성이 막혀도 기존 `muzzle` 계열 bone으로 총구 시작점을 잡을 수 있다.
- field actor는 replicate되고 각 peer가 debug sphere를 그림

## 구분할 코드

- `AGripGun::PullTrigger`: 즉시 hit damage와 impact 표시 성격
- `AWeaponBase::DealDamageToHitActor`: `FHitResult`가 있는 단발 hit damage adapter
- `AStasisNetField::ApplyDotDamage`: hit result 없이 field 안 monster에게 weapon dot damage 전달

## 수정 주의

- `WeaponBase` 구조 변경을 전제하지 않음
- 조준 preview는 `Tick()` 기반이므로 StasisNet은 `BeginPlay()`에서 tick을 다시 켜 BP default나 merge된 asset 설정의 tick 비활성화 영향을 줄인다.
- camera trace는 HUD 중앙 world point를 찾는 용도이고 실제 debug line, fire hit, field 위치는 `MuzzleSocketName`에서 시작한 trace 기준
- StasisNet dot damage는 weak point bone을 쓰지 않음
- hacked monster는 `HackedDamage`, non-hacked monster는 `BaseDamage`를 field가 선택
- 실제 VFX/UI/projectile 추가 전까지 debug draw를 제거하지 않음

## 검증

```powershell
rg -n "StasisNet|StasisNetField|ApplyDotDamage|ResolveAimTrace" Source Docs
```
