# AGripGun::PullTrigger 의도 카드

## 대상

`AGripGun::PullTrigger`, `CommitServerShot`, `BeginServerReload`, `FinishServerReload`

## 종류

Server-authoritative weapon fire boundary

## 정의 위치

- `Source/Pragmata_2P/Weapon/GripGun.h`
- `Source/Pragmata_2P/Weapon/GripGun.cpp`
- `Source/Pragmata_2P/Weapon/WeaponBase.h`
- `Source/Pragmata_2P/Weapon/WeaponBase.cpp`

## 이 코드가 존재하는 이유

ammo 소모, reload 전환, line trace, damage, impact effect를 server-authoritative 흐름으로 묶어 client prediction/입력과 gameplay mutation을 분리하기 위해 존재한다.

## 사용해야 하는 상황

- Hugh server fire request가 현재 weapon을 발사할 때
- GripGun ammo와 reload state를 확정할 때
- hit trace 결과로 damage를 적용하고 impact effect를 multicast할 때

## 사용하면 안 되는 상황

- client에서 ammo/reload/damage를 직접 확정하려는 경우
- montage callback에서 `bIsReloading`과 ammo를 직접 변경하는 경우
- non-weapon damage source에서 gun fire helper를 재사용하려는 경우

## 기본 흐름

`AHughCharacter::Server_RequestFire` -> `CurrentWeapon->PullTrigger()` -> authority/reload/controller 검증 -> `Super::PullTrigger()` fire gate -> `CommitServerShot` -> ammo/reload/trace/damage/impact effect

0발 reload는 `BeginServerReload`가 replicated `bIsReloading`을 열고 server timer를 시작한다. `OnRep_IsReloading`은 각 peer에서 Hugh montage만 재생한다. Timer 만료 뒤 `FinishServerReload`가 ammo 1발과 reload 종료를 확정한다. Montage callback은 socket presentation만 정리한다.

## 이 코드를 호출하는 쪽

- `AHughCharacter::Server_RequestFire_Implementation`
- server-side Hugh attack/fire path

## 이 코드가 호출하는 쪽

- `AWeaponBase::PullTrigger`
- `AWeaponBase::DealDamageToHitActor`
- `AHughCharacter::TryStartGripGunReload`
- `BeginServerReload`
- `FinishServerReload`
- `StartAutoReplenish`
- `MulticastSpawnImpactEffect`

## 서버/클라이언트 조건

`AGripGun::PullTrigger`는 `HasAuthority()`가 아니면 즉시 실패한다. client는 fire intent만 보내고, server가 ammo/reload/trace/damage를 확정한다.

## Blueprint 연결 여부

weapon actor/asset defaults는 Blueprint에서 설정될 수 있지만 이 fire mutation 경계는 C++ 함수다.

## 헷갈리기 쉬운 코드

- `AWeaponBase::PullTrigger`: 공통 weapon fire 가능 여부
- `AWeaponBase::DealDamageToHitActor`: hit actor damage spec 생성과 적용
- `AHughCharacter::OnStartAttack`: local input/cooldown entry

## 관련 문서

- `Docs/Systems/Damage/damage-flow.md`
- `Docs/KNOW_HOW.md`
- `Docs/Intent/Damage/WeaponBase-DealDamageToHitActor.md`

## 관련 커밋

- `a95ce668 feat: 권총 추가`
- P1 authority refactor는 session context `gripgun-authority-20260703T095239Z`에서 복원

## 확실한 근거

- `GripGun.cpp`가 `!HasAuthority()` 또는 `IsReloading`이면 `PullTrigger`를 실패 처리
- `CommitServerShot` 안에서 ammo 감소, reload/replenish, line trace, damage, impact multicast를 실행
- reload 완료는 server timer의 `FinishServerReload`로만 수렴
- `.omx/context/gripgun-authority-20260703T095239Z.md`가 P1 structural ambiguity를 server-authoritative boundary로 해결하라고 기록

## 추정한 의도

listen-server 테스트에서 client가 ammo나 damage를 먼저 확정해 서버 상태와 어긋나는 문제를 막으려는 의도.

## 확신도

강한 추정

## 아직 모르는 점

impact VFX replication 범위가 dedicated server/remote client에서 충분한지 별도 PIE 검증 필요.

## 검증 방법

```powershell
rg -n "PullTrigger|CommitServerShot|BeginServerReload|DealDamageToHitActor" Source Docs
```

## 다음 수정 시 주의점

client fire responsiveness를 추가하더라도 ammo, reload, damage 확정은 server helper 밖으로 빼지 않는다. Reload montage callback을 gameplay 완료 신호로 다시 사용하지 않는다.
