# UMapHackingPuzzleComponent::ProcessDirectionInput 의도 카드

## 대상

`UMapHackingPuzzleComponent::ProcessDirectionInput`

## 종류

Map hack sequence input processor

## 정의 위치

- `Source/Pragmata_2P/Puzzle/MapHackingPuzzleComponent.h`
- `Source/Pragmata_2P/Puzzle/MapHackingPuzzleComponent.cpp`

## 이 코드가 존재하는 이유

map hack의 현재 depth에서 Diana 방향 입력이 정답인지 판정하고 진행, reset, complete를 결정하기 위해 존재한다.

## 사용해야 하는 상황

- map hack session이 active이고 server가 방향 입력을 받은 경우

## 사용하면 안 되는 상황

- enemy grid cursor 이동
- UI-only tutorial preview

## 기본 흐름

authority와 active state 확인 -> 현재 depth 방향 비교 -> 정답이면 depth 진행 -> 실패면 reset -> 마지막 depth면 complete

## 이 코드를 호출하는 쪽

- `UDianaNodeInteractionComponent::Server_ProcessMapHackDirection`

## 이 코드가 호출하는 쪽

- `NotifyDepthSolved`
- `NotifyDepthChanged`
- `ResetHack`
- `CompleteHack`

## 서버/클라이언트 조건

server에서만 puzzle state를 변경한다.

## Blueprint 연결 여부

`BlueprintCallable`. 운영 입력은 Diana component의 server RPC 경유.

## 헷갈리기 쉬운 코드

- `UEnemyHackingPuzzleComponent::ProcessDirectionInput`: grid cursor 처리
- `RequestMapHackDirection`: client input relay

## 관련 문서

- `Docs/Systems/Hacking/map-hacking-integration.md`
- `Docs/KNOW_HOW.md`

## 관련 커밋

- `6dbb1c83 fix: 해킹 퍼즐 서버 흐름 보강`

## 확실한 근거

- `Docs/Systems/Hacking/map-hacking-integration.md`에 방향 입력은 서버 RPC를 통해 `ProcessDirectionInput()` 호출한다고 기록

## 추정한 의도

map puzzle 진행을 단순 sequence state machine으로 유지해 HUD와 gameplay 완료 판정을 일치시키려는 의도.

## 확신도

강한 추정

## 아직 모르는 점

실패 reset UX가 최종 설계인지 여부.

## 검증 방법

```powershell
rg -n "UMapHackingPuzzleComponent::ProcessDirectionInput|ResetHack|CompleteHack" Source Docs
```

## 다음 수정 시 주의점

depth 변경 delegate와 replicated counter를 함께 점검해야 HUD가 같은 입력에도 갱신된다.
