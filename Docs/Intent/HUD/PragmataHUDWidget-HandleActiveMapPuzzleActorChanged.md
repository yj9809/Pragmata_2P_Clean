# UPragmataHUDWidget::HandleActiveMapPuzzleActorChanged 의도 카드

## 대상

`UPragmataHUDWidget::HandleActiveMapPuzzleActorChanged`

## 종류

Root HUD map hack bridge

## 정의 위치

- `Source/Pragmata_2P/UI/PragmataHUDWidget.h`
- `Source/Pragmata_2P/UI/PragmataHUDWidget.cpp`

## 이 코드가 존재하는 이유

GameState의 replicated map puzzle actor 변경을 root HUD와 Blueprint event/ViewModel 경로에 전달하기 위해 존재한다.

## 사용해야 하는 상황

- PlayerController가 `ActiveMapPuzzleActor` 변경 delegate를 받은 경우

## 사용하면 안 되는 상황

- enemy hack target 변경
- node focus 후보 표시

## 기본 흐름

puzzle actor 수신 -> map hacking widget 또는 Blueprint event에 전달 -> map hacking ViewModel bind/unbind

## 이 코드를 호출하는 쪽

- `APragmataPlayerController::HandleActiveMapPuzzleActorChanged`

## 이 코드가 호출하는 쪽

- `OnActiveMapPuzzleActorChanged`
- map hacking widget/ViewModel binding 경로

## 서버/클라이언트 조건

client UI 처리. source state는 server replicated GameState.

## Blueprint 연결 여부

`BlueprintCallable`과 `BlueprintImplementableEvent` 경계가 있음. Blueprint event graph 확인 필요.

## 헷갈리기 쉬운 코드

- `HandleActiveEnemyHackTargetChanged`: enemy hack HUD bridge
- `APragmataGameState::ActiveMapPuzzleActor`: replicated source state

## 관련 문서

- `Docs/Systems/Hacking/map-hacking-integration.md`
- `Docs/KNOW_HOW.md`

## 관련 커밋

- `5fe4d726 feat: 해킹 UI 서버 권위 통합`

## 확실한 근거

- `Docs/KNOW_HOW.md`에 `ActiveMapPuzzleActor`가 root HUD Blueprint event에서 `UHackingPuzzleViewModel::BindToComponent()`로 연결된다고 기록

## 추정한 의도

map hack UI를 GameState replicated actor 변경에 맞춰 열고 닫게 하려는 의도.

## 확신도

강한 추정

## 아직 모르는 점

Blueprint event graph에서 map widget을 찾는 정확한 방식.

## 검증 방법

```powershell
rg -n "HandleActiveMapPuzzleActorChanged|OnActiveMapPuzzleActorChanged|BindToComponent" Source Docs
```

## 다음 수정 시 주의점

map puzzle actor null 처리와 component reset/cancel 처리를 함께 검증.
