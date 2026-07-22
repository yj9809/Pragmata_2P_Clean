# UPragmataHealthComponent::ReceiveDamage 의도 카드

## 대상

`UPragmataHealthComponent::ReceiveDamage`

## 종류

Health mutation endpoint

## 정의 위치

- `Source/Pragmata_2P/Components/PragmataHealthComponent.h`
- `Source/Pragmata_2P/Components/PragmataHealthComponent.cpp`

## 이 코드가 존재하는 이유

actor별 damage 판단이 끝난 뒤 실제 health 값을 변경하는 최종 지점을 하나로 모으기 위해 존재한다.

## 사용해야 하는 상황

- `IPragmataDamageable::ReceiveDamage` 구현에서 damage 적용이 확정된 경우
- health 변경 결과를 기준으로 death, hit react, UI 갱신을 이어야 하는 경우

## 사용하면 안 되는 상황

- faction, invincible, hack 특수 규칙처럼 actor context가 먼저 필요한 판단
- UI preview나 projected damage 계산

## 기본 흐름

`FPragmataDamageSpec` 수신 -> current health 감소 -> 변경 delegate 또는 상태 갱신

## 이 코드를 호출하는 쪽

- `APlayableCharacter::ReceiveDamage_Implementation`
- `AHughCharacter::ReceiveDamage_Implementation`
- `AMonsterBase::ReceiveDamage_Implementation`

## 이 코드가 호출하는 쪽

health 변경 delegate와 내부 health 상태 갱신.

## 서버/클라이언트 조건

server에서 실제 health mutation을 수행한다.

## Blueprint 연결 여부

미확인. component가 Blueprint에 붙어 있을 수 있으나 damage mutation 근거는 C++ 경로.

## 헷갈리기 쉬운 코드

- `IPragmataDamageable::ReceiveDamage`: actor-level 규칙 진입점
- `TakeDamage`: 현재 주 damage flow가 아님

## 관련 문서

- `Docs/Systems/Damage/damage-flow.md`
- `Docs/KNOW_HOW.md`

## 관련 커밋

- `dcbb10af refactor: ReceiveDamage 피해 경로 단일화`

## 확실한 근거

- `Docs/KNOW_HOW.md`에 최종 체력 변경 지점으로 기록
- `PlayableCharacter.cpp`에서 `HealthComponent->ReceiveDamage(DamageSpec)` 호출

## 추정한 의도

health mutation을 component에 고정해 actor subclass가 같은 체력 모델을 공유하게 하려는 의도.

## 확신도

강한 추정

## 아직 모르는 점

Blueprint에서 health component delegate를 어디까지 직접 구독하는지.

## 검증 방법

```powershell
rg -n "HealthComponent->ReceiveDamage|UPragmataHealthComponent::ReceiveDamage" Source Docs
```

## 다음 수정 시 주의점

actor별 damage 거부 조건을 health component로 밀어 넣지 않는다.
