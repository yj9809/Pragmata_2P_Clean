# UPragmataHUDWidget::HandleActiveEnemyHackTargetChanged 의도 카드

## 대상

`UPragmataHUDWidget::HandleActiveEnemyHackTargetChanged`

## 종류

Root HUD enemy hack bridge

## 정의 위치

- `Source/Pragmata_2P/UI/PragmataHUDWidget.h`
- `Source/Pragmata_2P/UI/PragmataHUDWidget.cpp`

## 이 코드가 존재하는 이유

GameState의 replicated enemy hack target 변경을 root HUD와 Blueprint event/ViewModel 경로에 전달하기 위해 존재한다.

## 사용해야 하는 상황

- PlayerController가 `ActiveEnemyHackTarget` 변경 delegate를 받은 경우

## 사용하면 안 되는 상황

- map puzzle actor 변경
- enemy puzzle 내부 cursor/grid state 직접 표시

## 기본 흐름

target actor 수신 -> enemy hacking widget 또는 Blueprint event에 전달 -> enemy hacking ViewModel bind/unbind

## 이 코드를 호출하는 쪽

- `APragmataPlayerController::HandleActiveEnemyHackTargetChanged`

## 이 코드가 호출하는 쪽

- `OnActiveEnemyHackTargetChanged`
- enemy hacking widget/ViewModel binding 경로

## 서버/클라이언트 조건

client UI 처리. source state는 server replicated GameState.

## Blueprint 연결 여부

`BlueprintCallable`과 `BlueprintImplementableEvent` 경계가 있음. Blueprint event graph 확인 필요.

## 헷갈리기 쉬운 코드

- `HandleActiveMapPuzzleActorChanged`: map hack HUD bridge
- `APragmataGameState::ActiveEnemyHackTarget`: replicated source state

## 관련 문서

- `Docs/Systems/Hacking/enemy-hacking-integration.md`
- `Docs/KNOW_HOW.md`

## 관련 커밋

- `5fe4d726 feat: 해킹 UI 서버 권위 통합`
- `f830bae4 fix: enemy hack UI 타깃 갱신 보강`

## 확실한 근거

- `Docs/KNOW_HOW.md`에 root HUD C++ 기본 경로가 `UEnemyHackingPuzzleViewModel::SetHackTarget()`으로 연결된다고 기록

## 추정한 의도

GameState 변경을 HUD Blueprint 내부 구조와 분리하는 C++ bridge로 두려는 의도.

## 확신도

강한 추정

## 아직 모르는 점

Blueprint event graph에서 embedded widget을 찾는 정확한 방식.

## 검증 방법

```powershell
rg -n "HandleActiveEnemyHackTargetChanged|OnActiveEnemyHackTargetChanged|SetHackTarget" Source Docs
```

## 다음 수정 시 주의점

target null 처리 시 success animation hold 요구사항을 함께 확인.
