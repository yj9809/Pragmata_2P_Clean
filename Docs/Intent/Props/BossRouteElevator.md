# ABossRouteElevator

## 의도

- 보스전 이동 구간의 `퍼즐 해결 -> 엘리베이터 상승 -> 전투 -> 다음 퍼즐` 흐름을 한 actor에서 서버 권한으로 관리
- `APuzzleInteractable::TargetActors`가 호출하는 `IPuzzleTargetInterface::OnPuzzleTriggered()`를 받아 다음 상승 stage 시작
- elevator 아래에 붙은 child puzzle actor는 `OnInteractionActivated` delegate를 자동 구독해 같은 상승 stage 시작
- `MonsterTerritoryActor`가 마지막 monster 처치 뒤 `SetTerritoryCombatLocked(false)`를 호출하면 `NotifyCombatCleared()`로 다음 stage 진행
- 다음 stage 진입 전 `ControlledPuzzleActors`를 `ResetPuzzleForNextAttempt()`로 되돌려 다시 해킹 가능한 잠금 상태를 만듦
- 월드 인스턴스의 `Allow Repeated Puzzle Hacking`, `Max Puzzle Hack Count`를 child puzzle actor에 전달

## 경계

- `MonsterAIController`, `MonsterBase`, `MonsterTerritoryActor`를 직접 수정하지 않음
- 몬스터 스폰 박스 열기, 퍼즐 잠금 표시, UI 갱신은 Blueprint event에서 처리
- 실제 플랫폼 mesh는 `ElevatorMesh` 또는 BP에서 붙인 child component로 구성

## 사용 흐름

1. `BP_BossRouteElevator` 생성
2. `Stages`에 이동 거리, 이동 시간, 전투 필요 여부 설정
3. 중앙 puzzle node를 `BP_BossRouteElevator` child actor로 배치
4. world에 따로 둔 puzzle node를 쓸 경우 `ControlledPuzzleActors`에 중앙 puzzle node 등록
5. 전투 Territory의 `Combat Lock Targets`에 이 elevator를 등록
6. `OnCombatStarted(StageIndex)`에서 스폰 박스 열기 등 시각 연출 처리
7. 월드 Details에서 반복 해킹 필요 시 `Allow Repeated Puzzle Hacking`과 `Max Puzzle Hack Count` 설정
8. 전투 클리어 후 puzzle 완료 애니메이션 역재생과 함께 다음 해킹 대기

## 주의

- 마지막 stage는 `bRequiresCombatClear=false`로 두면 도착 즉시 `Completed`
- 반복 단계의 puzzle 상호작용 가능 여부는 `SetPuzzleInteractionEnabled()`로 서버에서 제어
- 반복 단계의 puzzle 잠금 표시는 `OnPuzzleAvailabilityChanged()` 또는 `OnPuzzleInteractionEnabledChanged()`에서 BP가 처리
- child puzzle actor는 `TargetActors`에 elevator를 직접 넣지 않아도 progression 신호를 전달
- `RequiredCombatActors`에 살아 있는 `AMonsterBase`가 없으면 도착 직후 전투 완료로 처리
- 반복 해킹은 elevator의 월드 인스턴스 설정만 사용하며, child actor template의 값은 BeginPlay에 덮어씀
- `LunarMap_T`의 `BP_BossRouteElevator`는 `ElevatorMesh`와 child actor `BP_PuzzleNode_Elevator0`를 같은 `SceneRoot` 아래에 둔다. floor와 PuzzleMesh가 같은 XY에서 겹치므로 Diana interaction multi-trace는 parent `ElevatorMesh` hit만 건너뛰고 다음 blocker를 판정한다.
- floor 뒤의 child `PuzzleMesh`를 실제로 맞힌 경우만 candidate를 허용한다. floor 외 다른 blocker를 맞히면 뒤 puzzle interaction이 시작되지 않는다.
