# UDianaHackComponent::RequestHackDirection 의도 카드

## 대상

`UDianaHackComponent::RequestHackDirection`

## 종류

Enemy hack input relay

## 정의 위치

- `Source/Pragmata_2P/Components/DianaHackComponent.h`
- `Source/Pragmata_2P/Components/DianaHackComponent.cpp`

## 이 코드가 존재하는 이유

Diana의 방향 입력을 현재 enemy hack puzzle의 서버 처리 함수로 전달하기 위해 존재한다.

## 사용해야 하는 상황

- enemy hack UI가 active인 상태에서 방향 입력을 받은 경우
- 입력 source가 keyboard, gamepad, UI여도 같은 server path를 써야 하는 경우

## 사용하면 안 되는 상황

- map hack 방향 입력
- hack이 active가 아닌 preview 입력

## 기본 흐름

direction 수신 -> `Server_ProcessEnemyHackDirection(Direction)` -> active puzzle `ProcessDirectionInput`

## 이 코드를 호출하는 쪽

- Diana input 처리 경로

## 이 코드가 호출하는 쪽

- `Server_ProcessEnemyHackDirection`

## 서버/클라이언트 조건

client 입력을 server RPC로 보내고, puzzle 이동은 server에서 처리한다.

## Blueprint 연결 여부

public C++ 함수지만 `BlueprintCallable`은 아님. Blueprint 연결은 scan에서 간접 입력 경로 확인 필요.

## 헷갈리기 쉬운 코드

- `UDianaNodeInteractionComponent::RequestMapHackDirection`
- `UEnemyHackingPuzzleComponent::ProcessDirectionInput`

## 관련 문서

- `Docs/Systems/Hacking/enemy-hacking-integration.md`
- `Docs/KNOW_HOW.md`

## 관련 커밋

- `6dbb1c83 fix: 해킹 퍼즐 서버 흐름 보강`

## 확실한 근거

- `Docs/KNOW_HOW.md`에 enemy hack active 중 Diana 방향 입력이 서버 `ProcessDirectionInput()`으로만 진행된다고 기록

## 추정한 의도

입력 장치와 puzzle simulation을 분리하고 서버 권위를 유지하려는 의도.

## 확신도

강한 추정

## 아직 모르는 점

모든 입력 binding이 이 함수만 거치는지 여부.

## 검증 방법

```powershell
rg -n "RequestHackDirection|Server_ProcessEnemyHackDirection|ProcessDirectionInput" Source Docs
```

## 다음 수정 시 주의점

client에서 puzzle component를 직접 이동시키는 경로를 만들지 않는다.
