# UMapHackingPuzzleComponent::StartHack 의도 카드

## 대상

`UMapHackingPuzzleComponent::StartHack`

## 종류

Map hack sequence opener

## 정의 위치

- `Source/Pragmata_2P/Puzzle/MapHackingPuzzleComponent.h`
- `Source/Pragmata_2P/Puzzle/MapHackingPuzzleComponent.cpp`

## 이 코드가 존재하는 이유

map hack의 방향 sequence와 depth 상태를 서버에서 생성하고 replicated HUD 표시를 시작하기 위해 존재한다.

## 사용해야 하는 상황

- 서버가 map puzzle actor interaction을 승인한 경우

## 사용하면 안 되는 상황

- enemy grid hack
- client에서 직접 puzzle을 여는 처리

## 기본 흐름

authority 확인 -> sequence 생성 -> current depth 초기화 -> active state 전환 -> start/sequence delegate

## 이 코드를 호출하는 쪽

- `UDianaNodeInteractionComponent::StartMapHack`

## 이 코드가 호출하는 쪽

- `GenerateSequence`
- `BroadcastStarted`
- `OnSequenceReady`

## 서버/클라이언트 조건

server에서만 의미 있는 시작 함수.

## Blueprint 연결 여부

`BlueprintCallable`. 운영 흐름은 server interaction component 경유.

## 헷갈리기 쉬운 코드

- `UEnemyHackingPuzzleComponent::StartHack`: enemy grid puzzle
- `UDianaNodeInteractionComponent::StartMapHack`: actor/session bind

## 관련 문서

- `Docs/Systems/Hacking/map-hacking-integration.md`
- `Docs/KNOW_HOW.md`

## 관련 커밋

- `6dbb1c83 fix: 해킹 퍼즐 서버 흐름 보강`

## 확실한 근거

- `Docs/Systems/Hacking/map-hacking-integration.md`에 `StartHack()`는 서버에서만 의미 있다고 기록

## 추정한 의도

map puzzle의 random sequence와 HUD 표시를 서버 복제 상태로 고정하려는 의도.

## 확신도

강한 추정

## 아직 모르는 점

sequence 생성 난이도 조정 기준.

## 검증 방법

```powershell
rg -n "UMapHackingPuzzleComponent::StartHack|GenerateSequence|OnSequenceReady" Source Docs
```

## 다음 수정 시 주의점

sequence를 client별로 생성하면 양쪽 HUD가 다른 답을 볼 수 있다.
