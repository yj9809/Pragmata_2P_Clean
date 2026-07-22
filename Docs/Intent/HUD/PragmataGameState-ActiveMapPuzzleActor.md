# APragmataGameState::ActiveMapPuzzleActor 의도 카드

## 대상

`APragmataGameState::ActiveMapPuzzleActor`

## 종류

Replicated map hack HUD signal

## 정의 위치

- `Source/Pragmata_2P/Game/PragmataGameState.h`
- `Source/Pragmata_2P/Game/PragmataGameState.cpp`

## 이 코드가 존재하는 이유

현재 map hack actor를 양쪽 클라이언트 HUD에 같은 신호로 전달하기 위해 존재한다.

## 사용해야 하는 상황

- map hack 시작 시 root HUD가 map puzzle widget을 bind해야 하는 경우
- map hack 완료, cancel, reset 후 HUD를 닫아야 하는 경우

## 사용하면 안 되는 상황

- enemy hack target 표시
- node focus 후보 표시

## 기본 흐름

server `SetActiveMapPuzzleActor` -> replicated property 변경 -> `OnRep_ActiveMapPuzzleActor` -> PlayerController/HUD bind

## 이 코드를 호출하는 쪽

- `UDianaNodeInteractionComponent`

## 이 코드가 호출하는 쪽

- `OnActiveMapPuzzleActorChanged` delegate

## 서버/클라이언트 조건

server가 값을 설정하고 client는 OnRep/delegate로 표시한다.

## Blueprint 연결 여부

`BlueprintReadOnly` replicated property. root HUD Blueprint event와 map hacking ViewModel bind로 이어짐.

## 헷갈리기 쉬운 코드

- `ActiveEnemyHackTarget`: enemy hack HUD 신호
- `UDianaNodeInteractionComponent::ActiveMapPuzzleActor`: component 내부 session state

## 관련 문서

- `Docs/Systems/Hacking/map-hacking-integration.md`
- `Docs/Systems/Server/server.md`
- `Docs/KNOW_HOW.md`

## 관련 커밋

- `5fe4d726 feat: 해킹 UI 서버 권위 통합`

## 확실한 근거

- `Docs/KNOW_HOW.md`에 `ActiveMapPuzzleActor`가 root HUD Blueprint event에서 map ViewModel bind로 연결된다고 기록

## 추정한 의도

map hack HUD 표시를 server-approved active puzzle actor 하나로 통일하려는 의도.

## 확신도

강한 추정

## 아직 모르는 점

Blueprint root HUD event graph의 정확한 node 연결.

## 검증 방법

```powershell
rg -n "ActiveMapPuzzleActor|OnActiveMapPuzzleActorChanged" Source Docs
```

## 다음 수정 시 주의점

component 내부 active actor와 GameState replicated actor가 불일치하지 않게 처리.
