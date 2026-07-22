# UEnemyHackingPuzzleComponent::StartHack 의도 카드

## 대상

`UEnemyHackingPuzzleComponent::StartHack`

## 종류

Enemy hack session opener

## 정의 위치

- `Source/Pragmata_2P/Puzzle/EnemyHackingPuzzleComponent.h`
- `Source/Pragmata_2P/Puzzle/EnemyHackingPuzzleComponent.cpp`

## 이 코드가 존재하는 이유

enemy hack grid와 replicated puzzle state를 서버에서 초기화하고, HUD가 구독할 시작 이벤트를 발행하기 위해 존재한다.

## 사용해야 하는 상황

- 서버가 enemy hack target 검증을 끝낸 뒤 puzzle session을 열 때

## 사용하면 안 되는 상황

- Diana client가 직접 puzzle을 시작하려는 경우
- map hack puzzle 시작

## 기본 흐름

authority 확인 -> grid 초기화 -> state active 전환 -> start/grid delegate broadcast

## 이 코드를 호출하는 쪽

- `UDianaHackComponent::Server_RequestHack_Implementation`
- 일부 ViewModel 테스트 경로

## 이 코드가 호출하는 쪽

- grid 초기화 함수
- `BroadcastStarted`
- `OnGridReady`

## 서버/클라이언트 조건

서버에서만 의미 있음. client 직접 호출은 no-op 또는 테스트 전용으로 취급.

## Blueprint 연결 여부

`BlueprintCallable`. Blueprint에서 호출 가능하나 운영 흐름은 서버 RPC 경유가 기준.

## 헷갈리기 쉬운 코드

- `UMapHackingPuzzleComponent::StartHack`: map sequence puzzle 시작
- `UDianaHackComponent::Server_RequestHack_Implementation`: target 검증과 GameState bind 담당

## 관련 문서

- `Docs/Systems/Hacking/enemy-hacking-integration.md`
- `Docs/KNOW_HOW.md`

## 관련 커밋

- `6dbb1c83 fix: 해킹 퍼즐 서버 흐름 보강`

## 확실한 근거

- source comment에 조작 함수는 서버 권위라고 명시
- `Docs/Systems/Hacking/enemy-hacking-integration.md`에 `StartHack`는 서버 RPC 경유라고 기록

## 추정한 의도

puzzle state와 HUD 표시를 replicated server state 하나로 맞추려는 의도.

## 확신도

강한 추정

## 아직 모르는 점

ViewModel 테스트 경로를 장기 유지할지 여부.

## 검증 방법

```powershell
rg -n "UEnemyHackingPuzzleComponent::StartHack|StartEnemyHacking|Server_RequestHack" Source Docs
```

## 다음 수정 시 주의점

client에서 시작이 안 되는 문제를 `StartHack` 권한 조건 완화로 해결하지 않는다.
