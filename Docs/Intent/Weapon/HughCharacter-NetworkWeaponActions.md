# AHughCharacter network weapon action 의도 카드

## 대상

`Server_RequestFire`, `Server_RequestPickup`, `Server_RequestDash`, weapon presentation `OnRep`

## 종류

Hugh client intent와 server gameplay 확정 경계

## 정의 위치

- `Source/Pragmata_2P/Characters/HughCharacter.h`
- `Source/Pragmata_2P/Characters/HughCharacter.cpp`
- `Source/Pragmata_2P/Weapon/WeaponBase.h`
- `Source/Pragmata_2P/Weapon/WeaponBase.cpp`

## 이 코드가 존재하는 이유

remote Hugh 입력이 client inventory, ammo, cooldown, stamina를 단독 변경하지 못하게 하고 listen server가 현재 상태를 다시 검증하기 위해 존재한다.

## 기본 흐름

- Fire: local input -> `Server_RequestFire` -> owner/current slot/aim/ammo/cooldown 검증 -> server weapon `PullTrigger`
- Weapon depletion: server weapon `OnAmmoDepletion` -> `HandleAmmoDepletion` serial 확정 -> owner/simulated proxy montage 재생 -> `ANS_DropWeapon` server inventory 변경
- Pickup: local overlap candidate -> `Server_RequestPickup` -> server overlap identity/distance/owner/slot 검증 -> inventory mutation
- Dash: local input -> `Server_RequestDash` -> server stamina/death/중복 검증 -> server montage/state commit -> owner Client RPC + remote `HughAnimState` serial
- Presentation: `CurrentWeapon`, `WeaponInventory`, `HughAnimState`, GripGun reload state의 `OnRep`에서 최종 socket/montage 복구

## 사용하면 안 되는 상황

- weapon Actor와 Hugh Actor에 같은 fire intent Server RPC를 각각 보내는 경우
- client overlap candidate를 거리 재검증 없이 pickup하는 경우
- client AnimNotify에서 inventory, ammo, reload 완료를 확정하는 경우
- client hit result를 server damage 근거로 사용하는 경우

## 서버/클라이언트 조건

Client는 intent와 camera recoil 같은 owner presentation만 담당한다. Ammo, cooldown, damage, inventory, reload, dash stamina는 server만 변경한다. weapon depletion처럼 모든 peer가 같은 단발 montage를 봐야 하는 경우 server action serial을 복제하고, client AnimNotify는 state를 바꾸지 않는다. 반복 가능한 sound/VFX는 server 승인 뒤 multicast한다.

## 검증 방법

- Hugh host와 Hugh client 배치에서 fire/pickup/reload/dash 최종값 비교
- `Net PktLag=100`, `Net PktLag=200`에서 중복 mutation과 stale presentation 확인
- `rg -n "Server_RequestFire|Server_RequestPickup|Server_RequestDash|OnRep_IsReloading" Source/Pragmata_2P`

## 다음 수정 시 주의점

Owner prediction을 추가해도 gameplay mutation을 먼저 실행하지 않는다. 단발 presentation은 action serial 또는 승인 RPC로 중복을 막고, 최종 socket은 durable replicated state에서 복구한다.
