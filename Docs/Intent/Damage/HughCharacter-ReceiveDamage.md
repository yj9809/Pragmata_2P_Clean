# AHughCharacter::ReceiveDamage_Implementation 의도 카드

## 대상

`AHughCharacter::ReceiveDamage_Implementation`

## 종류

Hugh damage reaction policy

## 정의 위치

- `Source/Pragmata_2P/Characters/HughCharacter.h`
- `Source/Pragmata_2P/Characters/HughCharacter.cpp`

## 이 코드가 존재하는 이유

Hugh가 실제로 잃은 health를 기준으로 hit react 강도와 후속 반응을 계산하기 위해 존재한다.

## 사용해야 하는 상황

- Hugh actor가 `FPragmataDamageSpec`을 수신한 경우
- health 감소량과 character-specific reaction이 함께 필요한 경우

## 사용하면 안 되는 상황

- monster death animation 선택
- weapon이나 hack source에서 직접 health를 깎는 처리

## 기본 흐름

damage 전 health 확인 -> `Super::ReceiveDamage_Implementation` -> damage 후 health 비교 -> Hugh 반응 처리

## 이 코드를 호출하는 쪽

- `IPragmataDamageable::Execute_ReceiveDamage`

## 이 코드가 호출하는 쪽

- `APlayableCharacter::ReceiveDamage_Implementation`
- Hugh hit react 관련 처리

## 서버/클라이언트 조건

server damage 처리에서 기준 동작. client presentation은 복제 상태나 animation 경로로 따라온다.

## Blueprint 연결 여부

미확인. Hugh Blueprint가 animation presentation을 담당할 가능성은 있으나 damage 진입은 C++ interface.

## 헷갈리기 쉬운 코드

- `APlayableCharacter::ReceiveDamage_Implementation`: 공통 playable damage 처리
- `UPragmataHealthComponent::ReceiveDamage`: 최종 health 변경

## 관련 문서

- `Docs/Systems/Damage/damage-flow.md`
- `Docs/KNOW_HOW.md`

## 관련 커밋

- `b37dda22 feat: Hugh 공유 stats 추가`
- `dcbb10af refactor: ReceiveDamage 피해 경로 단일화`

## 확실한 근거

- `Docs/KNOW_HOW.md`에 Hugh가 `Super` 전후 health 비교로 HitReact 강도 계산한다고 기록
- `Source/Pragmata_2P/Characters/HughCharacter.cpp`에 override 존재

## 추정한 의도

raw damage amount가 아니라 실제 적용된 damage로 Hugh 반응을 맞추려는 의도.

## 확신도

강한 추정

## 아직 모르는 점

HitReact animation 선택 세부 기준 전체.

## 검증 방법

```powershell
rg -n "AHughCharacter::ReceiveDamage_Implementation|HitReact|ReceiveDamage" Source Docs
```

## 다음 수정 시 주의점

`Super` 호출 전후 health 비교 순서를 바꾸면 hit react 강도 계산 의미가 달라진다.
