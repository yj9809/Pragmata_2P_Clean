# UDianaNodeInteractionComponent::RequestMapHackDirection 의도 카드

## 대상

`UDianaNodeInteractionComponent::RequestMapHackDirection`

## 종류

Map hack input relay

## 정의 위치

- `Source/Pragmata_2P/Components/DianaNodeInteractionComponent.h`
- `Source/Pragmata_2P/Components/DianaNodeInteractionComponent.cpp`

## 이 코드가 존재하는 이유

active map hack의 방향 입력을 서버 `UMapHackingPuzzleComponent`로 전달하기 위해 존재한다.

## 사용해야 하는 상황

- map hack UI가 active인 상태에서 Diana 방향 입력이 들어온 경우

## 사용하면 안 되는 상황

- enemy hack grid 입력
- map hack이 active가 아닌 일반 node interaction

## 기본 흐름

direction 수신 -> `Server_ProcessMapHackDirection(Direction)` -> active map puzzle `ProcessDirectionInput`

## 이 코드를 호출하는 쪽

- Diana input 처리 경로

## 이 코드가 호출하는 쪽

- `Server_ProcessMapHackDirection`

## 서버/클라이언트 조건

client 입력은 server RPC를 통해서만 puzzle state를 바꾼다.

## Blueprint 연결 여부

public C++ 함수. Blueprint 직접 호출 여부는 asset scan 확인 대상.

## 헷갈리기 쉬운 코드

- `UDianaHackComponent::RequestHackDirection`
- `UMapHackingPuzzleComponent::ProcessDirectionInput`

## 관련 문서

- `Docs/Systems/Hacking/map-hacking-integration.md`
- `Docs/KNOW_HOW.md`

## 관련 커밋

- `6dbb1c83 fix: 해킹 퍼즐 서버 흐름 보강`

## 확실한 근거

- `Docs/KNOW_HOW.md`에 map hack active 중 Diana 방향 입력이 서버 `ProcessDirectionInput()`으로만 진행된다고 기록

## 추정한 의도

enemy hack과 동일하게 map puzzle도 서버 state machine 하나를 기준으로 동작시키려는 의도.

## 확신도

강한 추정

## 아직 모르는 점

input binding이 enemy hack과 map hack active 상태를 어떤 우선순위로 분기하는지.

## 검증 방법

```powershell
rg -n "RequestMapHackDirection|Server_ProcessMapHackDirection|UMapHackingPuzzleComponent::ProcessDirectionInput" Source Docs
```

## 다음 수정 시 주의점

enemy hack active 상태와 map hack active 상태가 동시에 true가 되지 않는지 함께 점검.
