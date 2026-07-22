# UDianaNodeInteractionComponent::StartMapHack 의도 카드

## 대상

`UDianaNodeInteractionComponent::StartMapHack`

## 종류

Map hack server session binder

## 정의 위치

- `Source/Pragmata_2P/Components/DianaNodeInteractionComponent.cpp`

## 이 코드가 존재하는 이유

map puzzle actor와 `UMapHackingPuzzleComponent`를 active session으로 묶고 GameState 복제 신호를 설정하기 위해 존재한다.

## 사용해야 하는 상황

- 서버가 `APuzzleInteractable`과 map puzzle component를 확인한 뒤 map hack을 시작할 때

## 사용하면 안 되는 상황

- enemy hack 시작
- client-only UI preview

## 기본 흐름

`PuzzleComponent->StartHack()` 성공 확인 -> active actor/component 저장 -> completion/reset delegate bind -> `SetActiveMapPuzzleActor`

## 이 코드를 호출하는 쪽

- `TryStartMapHackFromInteractable`
- `Server_RequestNodeInteraction` 흐름

## 이 코드가 호출하는 쪽

- `UMapHackingPuzzleComponent::StartHack`
- `APragmataGameState::SetActiveMapPuzzleActor`

## 서버/클라이언트 조건

server에서만 active session을 연다.

## Blueprint 연결 여부

private C++ 함수. 결과는 `ActiveMapPuzzleActor` 복제와 HUD Blueprint event로 이어진다.

## 헷갈리기 쉬운 코드

- `UMapHackingPuzzleComponent::StartHack`: puzzle 내부 sequence 시작
- `UDianaHackComponent::Server_RequestHack_Implementation`: enemy hack 시작

## 관련 문서

- `Docs/Systems/Hacking/map-hacking-integration.md`
- `Docs/Systems/Server/server.md`
- `Docs/KNOW_HOW.md`

## 관련 커밋

- `6dbb1c83 fix: 해킹 퍼즐 서버 흐름 보강`

## 확실한 근거

- `Docs/Systems/Hacking/map-hacking-integration.md`에 `PuzzleComp->StartHack()` 후 `GameState.ActiveMapPuzzleActor` 복제 흐름 기록

## 추정한 의도

map puzzle 시작 성공 여부와 HUD bind 신호를 같은 서버 결과에 묶으려는 의도.

## 확신도

강한 추정

## 아직 모르는 점

map node 실패/완료 상태를 표시하는 UI 세부 경로.

## 검증 방법

```powershell
rg -n "StartMapHack|SetActiveMapPuzzleActor|ActiveMapPuzzleActor" Source Docs
```

## 다음 수정 시 주의점

`StartHack()` 실패 후 active actor를 남기면 HUD가 잘못 열린다.
