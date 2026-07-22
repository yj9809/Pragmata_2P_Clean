# DealDamageToActor 의도 카드

## 대상

`UPragmataDamageLibrary::DealDamageToActor`

## 종류

Damage delivery gate

## 정의 위치

- `Source/Pragmata_2P/Combat/PragmataDamageLibrary.h`
- `Source/Pragmata_2P/Combat/PragmataDamageLibrary.cpp`

## 이 코드가 존재하는 이유

damage source가 만든 `FPragmataDamageSpec`을 target actor의 `ReceiveDamage`로 전달하기 전에 공통 gate를 적용하기 위해 존재한다.

## 사용해야 하는 상황

- weapon trace, melee sweep, overlap damage처럼 target actor가 정해진 경우
- source와 target faction 확인이 필요한 경우

## 사용하면 안 되는 상황

- target actor가 없는 광역 preview
- actor 내부에서 이미 자기 자신에게 확정 damage를 적용하는 특수 처리

## 기본 흐름

target 유효성 확인 -> faction gate -> `IPragmataDamageable::ReceiveDamage`

## 이 코드를 호출하는 쪽

- `AWeaponBase::DealDamageToHitActor`
- `UAnimNotifyState_AttackHitWindow`
- `UAnimNotify_AttackHitOverlap`
- `AWatcherMonster`

## 이 코드가 호출하는 쪽

- `IPragmataDamageable::ReceiveDamage`

## 서버/클라이언트 조건

server에서 호출되어야 한다.

## Blueprint 연결 여부

미확인. 현재 근거는 C++ 사용처 기준.

## 헷갈리기 쉬운 코드

- `ReceiveDamage`: 실제 receiver 진입점
- `UPragmataHealthComponent::ReceiveDamage`: 최종 health 변경

## 관련 문서

- `Docs/Systems/Damage/damage-flow.md`
- `Docs/Architecture/architecture.md`

## 관련 커밋

미확인. `git log -SDealDamageToActor -- Source/Pragmata_2P`로 추적.

## 확실한 근거

`Docs/Systems/Damage/damage-flow.md`와 `Docs/KNOW_HOW.md`에 공통 진입점으로 기록.

## 추정한 의도

damage 전달 전에 faction과 damageable 여부를 중앙에서 통제하려는 의도.

## 확신도

강한 추정

## 아직 모르는 점

Blueprint에서 library function을 직접 호출하는지 여부.

## 검증 방법

```powershell
rg -n "DealDamageToActor" Source Docs
```

## 다음 수정 시 주의점

source별 규칙을 이 함수에 과도하게 넣지 않는다. 공통 gate만 유지.
