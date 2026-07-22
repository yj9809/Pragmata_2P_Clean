# AMonsterBase hack state 의도 카드

## 대상

`AMonsterBase::bHacked`, `AMonsterBase::ApplySuccessfulHack_Implementation`, `AMonsterBase::CanBeHacked_Implementation`

## 종류

Timed enemy hack gameplay state

## 정의 위치

- `Source/Pragmata_2P/Enemy/MonsterBase.h`
- `Source/Pragmata_2P/Enemy/MonsterBase.cpp`

## 이 코드가 존재하는 이유

enemy가 hacked 상태인지 gameplay와 presentation이 같은 replicated 값을 보게 하기 위해 존재한다. `bHacked`는 약점 damage, material swap, AnimBP overlay의 기준이며, Diana enemy hack puzzle 시작 가능 여부와는 분리한다.

`bIsTainted`는 별도 replicated 오염 visual state다. 정화 스킬로만 false가 되며, hack duration과 damage window를 표현하는 `bHacked`와 섞으면 안 된다.

## 사용해야 하는 상황

- enemy hack completion으로 hack damage를 적용할 때
- 최초 hacked 상태 진입과 timer 만료를 복제해야 할 때
- hacked 상태 약점 damage나 visual overlay를 판정할 때

## 사용하면 안 되는 상황

- 이미 hacked 상태라는 이유만으로 enemy hack puzzle 시작을 막는 용도
- repeat hack reward 지급 여부를 `bHacked` 하나로 결정하는 용도
- 오염된/정화된 enemy 외형을 영구 상태로 표현하는 용도. 그 경우 `bIsTainted`와 `CleanseTaint()`를 사용

## 기본 흐름

첫 completion: `ApplySuccessfulHack` -> hack damage 적용 -> puzzle이 계산한 duration으로 `bHacked=true` timer 시작 -> timer 만료 -> `bHacked=false`

Repeat completion during hacked: `ApplySuccessfulHack` -> hack damage 적용 -> Diana OverDrive/success signal 유지 -> 기존 timer 유지

## 이 코드를 호출하는 쪽

- `UDianaHackComponent::HandleEnemyHackCompleted`
- `UDianaHackComponent::ValidateHackTarget`
- Blueprint/AnimBP의 `IsHacked()` 조회

## 이 코드가 호출하는 쪽

- `IPragmataDamageable::ReceiveDamage`
- `HandleHackStateChanged`
- `OnHackApplied`

## 서버/클라이언트 조건

`bHacked` 변경과 timer는 server authority에서만 처리한다. client는 `OnRep_Hacked()`로 material/AnimBP presentation을 갱신한다. `AppliedHackDuration`과 `HackedEndServerTime`은 같은 server snapshot을 복제해 Widget이 정확한 duration bar와 number를 표시하게 한다.

## Blueprint 연결 여부

`bHacked`는 `BlueprintReadOnly` replicated state이고, `IsHacked()`는 AnimBP/Blueprint 조회용이다. `OnHackApplied()`는 최초 hacked 상태 진입 effect용 Blueprint event이며 repeat completion에서는 호출하지 않는다.

## 헷갈리기 쉬운 코드

- `CanBeHacked`: 이름은 hack 가능 여부지만 현재 의미는 enemy hack puzzle 시작 가능 여부. `bHacked` 상태를 이유로 false를 반환하면 안 됨.
- `ApplySuccessfulHack`: puzzle이 계산한 duration은 최초 hacked 진입에만 사용. 이 snapshot이 timer, `AppliedHackDuration`, `HackedEndServerTime`의 공통 진실원. repeat completion은 damage와 Diana 보상을 성공 처리하지만, timer와 복제된 end time, `OnHackApplied()`는 reset하지 않음.
- `UEnemyHackingPuzzleComponent::StartHack`: puzzle session 시작만 담당하고 enemy gameplay state를 직접 바꾸지 않음.

## 관련 문서

- `Docs/Systems/Hacking/enemy-hacking-integration.md`
- `Docs/KNOW_HOW.md`
- `Docs/Intent/Hacking/DianaHackComponent-ServerRequestHack.md`
- `Docs/Intent/Hacking/EnemyHackingPuzzleComponent-CompleteHack.md`
- `Docs/Intent/Hacking/MonsterBase-TaintState.md`

## 관련 커밋

미확인. `git log -SApplySuccessfulHack -- Source/Pragmata_2P/Enemy/MonsterBase.cpp`로 추적.

## 확실한 근거

- `UDianaHackComponent::HandleEnemyHackCompleted()`가 `ApplySuccessfulHack` 반환값으로 OverDrive와 success signal 처리를 이어감.
- `AExecutorMonster::HandleHackStateChanged()`가 `bHacked` 변경으로 hacked material을 적용/복구.
- `AWalkerMonster::HandleHackStateChanged()`가 `bHacked` 변경으로 Walker/Tainted Walker의 같은 slot name에만 hacked material을 적용/복구.
- `ResolveHackedWeakPointDamageMultiplier()`가 `bHacked` 기준으로 weapon weak point damage를 판정.

## 추정한 의도

repeat enemy hack은 퍼즐 플레이와 Diana 보상을 계속 허용하되, hacked visual/weak point window는 최초 진입 후 고정 duration으로만 관리하려는 의도.

## 확신도

강한 추정

## 아직 모르는 점

Sentinel boss duration이 실제 boss encounter 난이도와 맞는지 PIE에서 확인 필요.

## 검증 방법

```powershell
rg -n "GetResolvedHackDurationSeconds|GetEarnedHackDuration|AppliedHackDuration|HackedEndServerTime|HackDurationSeconds|ApplySuccessfulHack" Source Docs
```

## 다음 수정 시 주의점

repeat hack completion 정책을 바꿀 때는 enemy damage, Diana OverDrive/success signal, resolved duration 사용 시점, hacked timer reset 여부를 각각 따로 명시한다.
