# Monster Territory Combat Lock

## 대상

- `AMonsterTerritoryActor::LockCombatTargets`
- `AMonsterTerritoryActor::CheckTerritoryCombatClear`
- `ITerritoryCombatLockable::SetTerritoryCombatLocked`

## 목적

Territory 전투가 시작되면 등록한 map gimmick을 잠그고, Territory가 관리하는 배치·runtime spawn monster를 모두 처치했을 때만 해제한다.

## 흐름

```text
첫 player 진입 확정
-> CombatLockTargets true 호출
-> sequence/appearance/runtime spawn 준비
-> 등록 monster의 OnDeath 또는 OnDestroyed
-> 살아 있는 ActiveMonsters 없음
-> CombatLockTargets false 호출
```

## 경계

- Territory만 monster 처치 완료를 판정한다. `ABossRouteElevator.RequiredCombatActors`는 새 전투 진행 판정에 사용하지 않는다.
- Door, Elevator, BossRouteElevator는 잠금 상태를 수행할 뿐 다른 Territory의 monster를 직접 감시하지 않는다.
- 메시, 머티리얼, 차단 콜리전 연출은 각 Blueprint의 `OnTerritoryCombatLockStateChanged`에서 구현한다.
