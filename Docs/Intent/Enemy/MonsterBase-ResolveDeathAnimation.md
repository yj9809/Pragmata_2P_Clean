# AMonsterBase::ResolveDeathAnimation 의도 카드

## 대상

`AMonsterBase::ResolveDeathAnimation`, `CurrentDeathAnimation`, `HandleDeath`

## 종류

Monster death animation selection boundary

## 정의 위치

- `Source/Pragmata_2P/Enemy/MonsterBase.h`
- `Source/Pragmata_2P/Enemy/MonsterBase.cpp`
- `Source/Pragmata_2P/Animation/MonsterAnimInstance.cpp`

## 이 코드가 존재하는 이유

치명타 직전의 `FPragmataDamageSpec`과 monster 상태로 death animation을 먼저 확정해, health mutation과 `OnDeath` 이후에도 모든 peer가 같은 death animation을 재생하게 하기 위해 존재한다.

## 사용해야 하는 상황

- monster가 lethal damage를 받을 때
- hack, critical, attack pattern, locomotion 방향에 따라 death animation을 다르게 골라야 할 때
- active attack montage를 death pose로 자연스럽게 정리해야 할 때

## 사용하면 안 되는 상황

- damage 적용 후 이미 `OnDeath`가 실행된 다음에 사망 원인을 다시 추론하려는 경우
- 단순 hit react 또는 non-lethal damage 처리
- client-only animation override로 death state를 임의 결정하는 경우

## 기본 흐름

`ReceiveDamage` -> lethal 여부 확인 -> `ResolveDeathAnimation(DamageSpec)` -> `CurrentDeathAnimation` replication -> `HealthComponent::ReceiveDamage` -> `OnDeath` -> `HandleDeath` -> `SetDead` -> AnimBP death state

## 이 코드를 호출하는 쪽

- `AMonsterBase::ReceiveDamage_Implementation`

## 이 코드가 호출하는 쪽

- `ResolveDirectionalDeathAnimation`
- `AbortAttackForDeath`
- `UCharacterMovementComponent::StopMovementImmediately`
- `UAnimInstance::Montage_Stop`

## 서버/클라이언트 조건

server가 lethal damage 처리 중 `CurrentDeathAnimation`을 확정하고 replication한다. client는 `MonsterAnimInstance`에서 replicated 값을 읽어 death state를 표시한다.

## Blueprint 연결 여부

`CurrentDeathAnimation`은 `BlueprintReadOnly`, `GetDeathAnimation`은 `BlueprintPure`라 AnimBP가 Death 상태에서 읽을 수 있다.

## 헷갈리기 쉬운 코드

- `UPragmataHealthComponent::ReceiveDamage`: 실제 death flag와 `OnDeath`를 확정
- `AMonsterBase::SetDead`: AI/movement/collision/montage 정리
- `AMonsterBase::AbortAttackForDeath`: 정상 공격 종료와 달리 cooldown 없이 공격 상태 정리

## 관련 문서

- `Docs/Systems/Damage/damage-flow.md`
- `Docs/KNOW_HOW.md`
- `Docs/Intent/Damage/ReceiveDamage.md`

## 관련 커밋

- `36ba8232 feat: 몬스터 상황별 죽음 애니메이션 처리 구현`
- `dcbb10af refactor: ReceiveDamage 피해 경로 단일화`

## 확실한 근거

- `MonsterBase.cpp`가 lethal damage일 때 `HealthComponent::ReceiveDamage` 호출 전에 `CurrentDeathAnimation`을 설정
- `CurrentDeathAnimation`은 replicated property
- `MonsterAnimInstance.cpp`가 monster의 death 상태와 death animation을 읽음

## 추정한 의도

공격 중 공중에 뜬 mesh, hack death, critical death처럼 상황별 death pose가 health event 이후에 사라지는 문제를 막으려는 의도.

## 확신도

강한 추정

## 아직 모르는 점

각 monster Blueprint가 모든 `FMonsterDeathAnimationSet` variant를 채웠는지 여부.

## 검증 방법

```powershell
rg -n "ResolveDeathAnimation|CurrentDeathAnimation|GetDeathAnimation|HandleDeath" Source Docs
```

## 다음 수정 시 주의점

`OnDeath` 이후에 death animation을 고르도록 순서를 바꾸면 damage kind와 attack pattern context가 사라질 수 있다.
