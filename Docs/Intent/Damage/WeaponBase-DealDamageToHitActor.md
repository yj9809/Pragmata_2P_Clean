# AWeaponBase::DealDamageToHitActor 의도 카드

## 대상

`AWeaponBase::DealDamageToHitActor`

## 종류

Weapon hit damage adapter

## 정의 위치

- `Source/Pragmata_2P/Weapon/WeaponBase.h`
- `Source/Pragmata_2P/Weapon/WeaponBase.cpp`

## 이 코드가 존재하는 이유

weapon trace의 `FHitResult`를 project damage 규격인 `FPragmataDamageSpec`으로 바꿔 공통 damage gate에 전달하기 위해 존재한다.

## 사용해야 하는 상황

- weapon이 이미 hit actor와 hit result를 확보한 경우
- weapon별 damage amount를 project 공통 damage flow로 보내야 하는 경우

## 사용하면 안 되는 상황

- hit result가 없는 area damage
- hack, melee notify처럼 weapon hit가 아닌 source

## 기본 흐름

`FHitResult` 확인 -> `FPragmataDamageSpec` 생성 -> `UPragmataDamageLibrary::DealDamageToActor`

## 이 코드를 호출하는 쪽

- `AGripGun::Fire`

## 이 코드가 호출하는 쪽

- `UPragmataDamageLibrary::DealDamageToActor`

## 서버/클라이언트 조건

damage 적용은 server authority 기준이어야 한다.

## Blueprint 연결 여부

Blueprint asset scan 기준 직접 확인 필요. C++ 사용처는 `GripGun.cpp`에서 확인.

## 헷갈리기 쉬운 코드

- `UPragmataDamageLibrary::DealDamageToActor`: 공통 gate
- `IPragmataDamageable::ReceiveDamage`: receiver 진입점

## 관련 문서

- `Docs/Systems/Damage/damage-flow.md`
- `Docs/Architecture/architecture.md`
- `Docs/KNOW_HOW.md`

## 관련 커밋

- `4aada14a docs: GripGun 권한 경계 변경 문서 추가`

## 확실한 근거

- `Source/Pragmata_2P/Weapon/GripGun.cpp`에서 `DealDamageToHitActor` 호출
- `Docs/KNOW_HOW.md` damage flow에 weapon damage 경로 기록

## 추정한 의도

weapon별 hit detection과 project 공통 damage 정책을 분리하려는 의도.

## 확신도

강한 추정

## 아직 모르는 점

Blueprint weapon subclass가 이 함수를 override하거나 직접 호출하는지 여부.

## 검증 방법

```powershell
rg -n "DealDamageToHitActor|DealDamageToActor" Source Docs
```

## 다음 수정 시 주의점

weapon 전용 예외를 공통 library에 넣기 전에 이 adapter에서 처리 가능한지 먼저 확인.
