# UMapHackingPuzzleComponent::CompleteHack 의도 카드

## 대상

`UMapHackingPuzzleComponent::CompleteHack`

## 종류

Map hack completion trigger

## 정의 위치

- `Source/Pragmata_2P/Puzzle/MapHackingPuzzleComponent.cpp`

## 이 코드가 존재하는 이유

map hack sequence 성공 시 puzzle completed state와 completion delegate를 발행하기 위해 존재한다.

## 사용해야 하는 상황

- 모든 map hack depth가 정답 처리된 경우

## 사용하면 안 되는 상황

- enemy hack 성공
- 단일 depth solved 중간 상태

## 기본 흐름

completed state 전환 -> completion delegate broadcast -> Diana node interaction component 후속 clear

## 이 코드를 호출하는 쪽

- `UMapHackingPuzzleComponent::ProcessDirectionInput`

## 이 코드가 호출하는 쪽

- `OnHackCompleted` delegate

## 서버/클라이언트 조건

server completion이 기준.

## Blueprint 연결 여부

private C++ 함수. Blueprint는 delegate, replicated state, GameState active actor 변화로 결과를 받는다.

## 헷갈리기 쉬운 코드

- `UEnemyHackingPuzzleComponent::CompleteHack`
- `UDianaNodeInteractionComponent::HandleMapHackCompleted`

## 관련 문서

- `Docs/Systems/Hacking/map-hacking-integration.md`
- `Docs/KNOW_HOW.md`

## 관련 커밋

- `6dbb1c83 fix: 해킹 퍼즐 서버 흐름 보강`

## 확실한 근거

- `Docs/Systems/Hacking/map-hacking-integration.md`에 완료 시 `GameState.ActiveMapPuzzleActor = nullptr` 흐름 기록

## 추정한 의도

puzzle 성공 판정과 map node/session cleanup을 delegate로 분리하려는 의도.

## 확신도

강한 추정

## 아직 모르는 점

완료된 node를 영구 완료로 저장하는 위치 전체.

## 검증 방법

```powershell
rg -n "UMapHackingPuzzleComponent::CompleteHack|HandleMapHackCompleted|ActiveMapPuzzleActor = nullptr" Source Docs
```

## 다음 수정 시 주의점

completion 후 active puzzle actor clear가 누락되면 양쪽 HUD가 계속 열린다.
