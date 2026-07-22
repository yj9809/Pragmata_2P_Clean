# AMonsterBase::SetTerritoryTarget 의도 카드

## 대상
`AMonsterBase::SetTerritoryTarget`, `AMonsterBase::TrySetTargetIfPerceived`, `AMonsterBase::ForgetPerceivedActor`

## 종류

Monster target acquisition boundary

## 정의 위치

- `Source/Pragmata_2P/Enemy/MonsterBase.h`
- `Source/Pragmata_2P/Enemy/MonsterBase.cpp`
- `Source/Pragmata_2P/Enemy/MonsterTerritoryActor.cpp`

## 이 코드가 존재하는 이유

Territory runtime spawn, 외부 이벤트 spawn처럼 player가 이미 Territory 안에 있다는 조건이 server에서 확정된 경우 AI Perception stimulus를 기다리지 않고 monster target을 지정하기 위해 존재.

기존 `TrySetTargetIfPerceived`는 시야·피격 stimulus가 성공 상태로 남아 있을 때만 target을 지정한다. spawn 직후 monster는 아직 perception 기록이 없을 수 있으므로 별도 경로 필요.

Territory를 나갔다가 다시 들어온 기존 배치 monster는 BeginOverlap에서 기존 successful stimulus를 재사용하지 않는다. `ForgetPerceivedActor`로 해당 actor perception cache를 지운 뒤 새 Sight 또는 Damage stimulus가 들어온 경우에만 `TrySetTargetIfPerceived` 경로로 target을 다시 잡는다.

## 사용해야 하는 상황

- `AMonsterTerritoryActor`가 server에서 spawn한 monster에 현재 Territory player를 즉시 물릴 때
- sequence 완료 뒤 생성된 reinforcement를 바로 전투 상태로 진입시킬 때

## 사용하면 안 되는 상황

- 일반 시야 감지, 피격 감지, 순찰 중 target 획득
- 기존 active monster가 Territory 재진입만으로 target을 복구하는 경우
- Territory 내부 여부를 검증하지 않은 actor를 target으로 지정하려는 경우
- client-only 연출이나 UI 상태에서 target을 임의 지정하는 경우

## 기본 흐름

`SpawnMonster` 또는 활성화형 배치 monster -> `BeginMonsterEncounter` -> optional `TryPlaySpawnAppearance` -> `StartMonsterAIWithCurrentTarget` -> `AssignCurrentTargetToMonster` -> `SetTerritoryTarget` -> `MonsterAIController::SetTarget` -> Blackboard `TargetActor`

## 헷갈리기 쉬운 코드

- `TrySetTargetIfPerceived`: AI Perception 성공 stimulus 필요
- `ForgetPerceivedActor`: Territory 재진입 시 기존 perception cache 삭제
- `SetTerritoryTarget`: Territory가 이미 검증한 player를 perception 없이 지정
- `MonsterAIController::SetTarget`: 최종 Blackboard 설정, assigned Territory 내부 여부 검증

## 서버/클라이언트 조건

server authority에서만 처리. `MonsterAIController::SetTarget`이 실패하면 `TargetActor`도 설정하지 않음.

## 확실한 근거

- `SetTerritoryTarget`이 `HasAuthority()`, target null, 기존 `TargetActor`가 다른 actor인지 확인
- 같은 actor를 다시 지정하는 경우 BT 재시작 뒤 Blackboard target 복구를 허용
- 최종 target 지정은 `MonsterAIController::SetTarget` 성공 뒤에만 `TargetActor`에 반영
- Territory 밖 target은 `MonsterAIController::SetTarget`의 `IsActorInsideTerritory` 검증에서 차단

## 관련 문서

- `Docs/KNOW_HOW.md`
- `Docs/Intent/Enemy/MonsterTerritoryActor-Activation.md`

## 검증 방법

```powershell
rg -n "SetTerritoryTarget|TrySetTargetIfPerceived|AssignCurrentTargetToMonster" Source Docs
```

PIE:

- runtime spawn monster가 perception 감지 전에도 Target을 갖는지 확인
- 기본 배치 monster는 perception 감지 전까지 Target이 지정되지 않는지 확인
- 활성화형 배치 monster는 `ActivateTerritoryTrigger` 이후 Target이 지정되는지 확인
- 배치 monster는 player 재진입 직후 기존 perception cache만으로 Target이 복구되지 않는지 확인
- player가 Territory 밖으로 나가면 새 spawn target 지정이 실패하는지 확인
