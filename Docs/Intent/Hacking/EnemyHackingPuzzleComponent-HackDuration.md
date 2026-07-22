# UEnemyHackingPuzzleComponent::GetResolvedHackDurationSeconds 의도 카드

## 대상

`UEnemyHackingPuzzleComponent::GetResolvedHackDurationSeconds`

## 종류

Enemy hack duration resolver

## 정의 위치

- `Source/Pragmata_2P/Puzzle/EnemyHackingPuzzleComponent.h`
- `Source/Pragmata_2P/Puzzle/EnemyHackingPuzzleComponent.cpp`

## 이 코드가 존재하는 이유

적 종류와 완료 경로의 파란 노드 수를 같은 puzzle model에서 읽어 hacked 유지 시간을 하나로 결정한다. damage 계산과 달리 duration은 적 상태 timer에만 전달한다.

## 사용해야 하는 상황

- `UDianaHackComponent::HandleEnemyHackCompleted`가 서버 gameplay effect를 호출하기 직전
- 적 종류별 duration balance profile을 조정할 때

## 사용하면 안 되는 상황

- puzzle 진행 중 UI 남은 시간 표시
- 이미 hacked인 repeat completion의 timer 연장
- client가 duration을 권위 있게 결정하는 처리

## 기본 흐름

`EnemyType` + `NormalNodesVisited` -> profile tier 조회 -> 마지막 tier clamp -> `GetEarnedHackDuration` UI 예상값 / `ApplySuccessfulHack` 완료 snapshot -> 최초 hacked 진입 timer

profile이 없거나 값이 유효하지 않으면 `0` 이하 값을 반환한다. `AMonsterBase`가 기존 `HackDurationSeconds` fallback을 선택한다. Sentinel은 profile을 보유하므로 이 fallback을 사용하지 않는다.

## 서버/클라이언트 조건

파란 노드 수는 server가 puzzle 입력으로 갱신한다. duration 계산과 timer 시작은 server authority다. client는 `bHacked` 복제로 결과만 표시한다.

## Blueprint 연결 여부

profile은 class default tuning용 `EditDefaultsOnly` data다. resolver 결과는 C++ completion 경로에서 gameplay timer로 전달한다.

## 헷갈리기 쉬운 코드

- `GetProjectedDamage`: 같은 node count를 쓰지만 damage 배수를 계산
- `GetEarnedHackDuration`: UI 게이지용 wrapper. hardcoded 표를 두지 않고 `GetResolvedHackDurationSeconds` 결과를 그대로 사용
- `AMonsterBase::HackDurationSeconds`: profile 부재나 invalid profile의 fallback
- `StartHackDurationTimer`: 계산하지 않고 전달된 duration으로 timer만 시작

## 관련 문서

- `Docs/Systems/Hacking/enemy-hacking-integration.md`
- `Docs/Intent/Hacking/MonsterBase-HackState.md`
- `Docs/Intent/Hacking/EnemyHackingPuzzleComponent-CompleteHack.md`

## 검증 방법

```powershell
rg -n "GetResolvedHackDurationSeconds|DurationSecondsByVisitedNodeCount|ApplySuccessfulHack" Source Docs
```

## 다음 수정 시 주의점

새 적 profile을 넣을 때는 0-node tier를 포함하고, 지원하지 않는 큰 node count는 마지막 tier에 고정되는지 automation test로 확인한다.
