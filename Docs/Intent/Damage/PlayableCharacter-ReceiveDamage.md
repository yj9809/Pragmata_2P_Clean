# APlayableCharacter::ReceiveDamage_Implementation 의도 카드

## 대상

`APlayableCharacter::ReceiveDamage_Implementation`

## 종류

Playable character damage baseline

## 정의 위치

- `Source/Pragmata_2P/Characters/PlayableCharacter.h`
- `Source/Pragmata_2P/Characters/PlayableCharacter.cpp`

## 이 코드가 존재하는 이유

Hugh와 Diana 계열 playable actor가 공유하는 damage 수신 기본 처리를 제공하기 위해 존재한다.

## 사용해야 하는 상황

- playable character subclass가 공통 health component damage를 쓰는 경우
- subclass가 추가 반응만 덧붙여야 하는 경우

## 사용하면 안 되는 상황

- enemy 전용 death animation이나 hack damage 특수 처리
- weapon source에서 직접 호출하는 우회 경로

## 기본 흐름

`CanReceiveDamage` 확인 -> `HealthComponent->ReceiveDamage(DamageSpec)`

## 이 코드를 호출하는 쪽

- playable subclass의 `ReceiveDamage_Implementation`
- `IPragmataDamageable::Execute_ReceiveDamage`

## 이 코드가 호출하는 쪽

- `CanReceiveDamage`
- `UPragmataHealthComponent::ReceiveDamage`

## 서버/클라이언트 조건

server damage 처리에서 기준으로 사용.

## Blueprint 연결 여부

미확인. C++ interface override 기준.

## 헷갈리기 쉬운 코드

- `AHughCharacter::ReceiveDamage_Implementation`: Hugh 전용 반응 추가
- `AMonsterBase::ReceiveDamage_Implementation`: enemy 전용 반응 추가

## 관련 문서

- `Docs/Systems/Damage/damage-flow.md`
- `Docs/KNOW_HOW.md`

## 관련 커밋

- `dcbb10af refactor: ReceiveDamage 피해 경로 단일화`

## 확실한 근거

- `PlayableCharacter.cpp`에서 `HealthComponent && HealthComponent->ReceiveDamage(DamageSpec)` 반환

## 추정한 의도

playable actor 간 damage 처리 중복을 줄이고 subclass는 반응만 확장하게 하려는 의도.

## 확신도

강한 추정

## 아직 모르는 점

Diana pawn이 이 baseline을 직접 공유하는지 여부.

## 검증 방법

```powershell
rg -n "APlayableCharacter::ReceiveDamage_Implementation|CanReceiveDamage_Implementation" Source Docs
```

## 다음 수정 시 주의점

subclass 전용 정책을 baseline에 넣으면 Hugh와 다른 playable actor 모두에 영향이 간다.
