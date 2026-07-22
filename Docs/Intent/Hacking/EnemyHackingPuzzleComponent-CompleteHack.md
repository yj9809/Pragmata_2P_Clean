# UEnemyHackingPuzzleComponent::CompleteHack 의도 카드

## 대상

`UEnemyHackingPuzzleComponent::CompleteHack`

## 종류

Enemy hack completion trigger

## 정의 위치

- `Source/Pragmata_2P/Puzzle/EnemyHackingPuzzleComponent.cpp`

## 이 코드가 존재하는 이유

enemy hack 성공 시 puzzle 완료 이벤트를 내고, Diana hack component가 damage, 파란 노드별 duration, OverDrive, HUD hold 처리를 이어가게 하기 위해 존재한다.

## 사용해야 하는 상황

- enemy hack cursor가 target 완료 조건에 도달한 경우

## 사용하면 안 되는 상황

- 실패 reset
- map hack 완료

## 기본 흐름

state completed 전환 -> completion delegate broadcast -> owner component 후속 처리

## 이 코드를 호출하는 쪽

- `UEnemyHackingPuzzleComponent::ProcessDirectionInput`

## 이 코드가 호출하는 쪽

- `OnHackCompleted` delegate

## 서버/클라이언트 조건

server completion이 기준. client HUD는 replicated state와 delegate로 반응.

## Blueprint 연결 여부

private C++ 함수. Blueprint는 delegate나 replicated state를 통해 결과를 받는다.

## 헷갈리기 쉬운 코드

- `UDianaHackComponent::HandleEnemyHackCompleted`: damage와 HUD target clear 지연 처리
- `UMapHackingPuzzleComponent::CompleteHack`: map puzzle 완료

## 관련 문서

- `Docs/Systems/Hacking/enemy-hacking-integration.md`
- `Docs/KNOW_HOW.md`

## 관련 커밋

- `6dbb1c83 fix: 해킹 퍼즐 서버 흐름 보강`

## 확실한 근거

- `Docs/KNOW_HOW.md`에 `UEnemyHackingPuzzleComponent::CompleteHack()`가 `OnHackCompleted` broadcast한다고 기록

## 추정한 의도

puzzle 성공 판정과 damage/duration gameplay effect 적용을 delegate 경계로 분리하려는 의도.

## 확신도

강한 추정

## 아직 모르는 점

completion 후 HUD hold 시간의 최종 tuning 기준.

## 검증 방법

```powershell
rg -n "CompleteHack|OnHackCompleted|HandleEnemyHackCompleted" Source Docs
```

## 다음 수정 시 주의점

completion 함수 안에 직접 damage나 timer 적용을 넣으면 puzzle과 gameplay effect 경계가 흐려진다. duration 계산은 puzzle, timer 시작은 target enemy가 담당한다.
